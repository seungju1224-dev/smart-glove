#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

// RC522 SPI pins. Change these only when the physical wiring changes.
constexpr uint8_t SS_PIN = 5;
constexpr uint8_t RST_PIN = 22;
MFRC522 rfid(SS_PIN, RST_PIN);

// Drive a vibration motor through a transistor or motor driver, not directly.
constexpr uint8_t VIB_PIN = 25;
constexpr unsigned long KNOWN_VIBRATION_MS = 300;
constexpr unsigned long UNKNOWN_VIBRATION_MS = 1000;

// These BLE values must match smart-glove-terminal.html.
constexpr char DEVICE_NAME[] = "SmartGlove_BLE";
constexpr char SERVICE_UUID[] = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
constexpr char COMMAND_CHAR_UUID[] = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
constexpr char NOTIFY_CHAR_UUID[] = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

// An RC522 UID can be 4, 7 or 10 bytes. Without separators, 10 bytes is 20 chars.
constexpr size_t MAX_UNIQUE_ID_CHARS = 20;
constexpr unsigned long SAME_TAG_INTERVAL_MS = 3000;

BLECharacteristic *notifyCharacteristic = nullptr;
bool deviceConnected = false;
bool wasConnected = false;

String lastUID = "";
unsigned long lastReadTime = 0;
bool vibrationActive = false;
unsigned long vibrationEndsAt = 0;

void startVibration(unsigned long durationMs) {
  digitalWrite(VIB_PIN, HIGH);
  vibrationActive = true;
  vibrationEndsAt = millis() + durationMs;
}

void updateVibration() {
  if (vibrationActive && static_cast<long>(millis() - vibrationEndsAt) >= 0) {
    digitalWrite(VIB_PIN, LOW);
    vibrationActive = false;
  }
}

class SmartGloveServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server) override {
    deviceConnected = true;
    Serial.println("BLE client connected.");
  }

  void onDisconnect(BLEServer *server) override {
    deviceConnected = false;
    digitalWrite(VIB_PIN, LOW);
    vibrationActive = false;
    Serial.println("BLE client disconnected.");
  }
};

// The website decides whether the UID is known, then writes the vibration result here.
class SmartGloveCommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    String command(characteristic->getValue().c_str());
    command.trim();
    command.toUpperCase();

    if (command == "KNOWN") {
      startVibration(KNOWN_VIBRATION_MS);
      Serial.println("Website recognized UID: short vibration.");
    } else if (command == "UNKNOWN") {
      startVibration(UNKNOWN_VIBRATION_MS);
      Serial.println("Website did not recognize UID: long vibration.");
    }
  }
};

String formatUniqueId(const MFRC522::Uid &uid) {
  const char hexDigits[] = "0123456789ABCDEF";
  String uniqueId = "";
  uniqueId.reserve(uid.size * 2);

  for (byte index = 0; index < uid.size; index++) {
    uniqueId += hexDigits[(uid.uidByte[index] >> 4) & 0x0F];
    uniqueId += hexDigits[uid.uidByte[index] & 0x0F];
  }

  return uniqueId;
}

bool notifyUniqueId(const String &uniqueId) {
  if (!deviceConnected) {
    Serial.println("UID not sent: web page is not connected by BLE.");
    return false;
  }

  if (uniqueId.length() == 0 || uniqueId.length() > MAX_UNIQUE_ID_CHARS) {
    Serial.println("UID not sent: UID is outside the supported format.");
    return false;
  }

  notifyCharacteristic->setValue(
      reinterpret_cast<uint8_t *>(const_cast<char *>(uniqueId.c_str())),
      uniqueId.length());
  notifyCharacteristic->notify();

  Serial.println("BLE UID sent: " + uniqueId);
  return true;
}

void setupBLE() {
  BLEDevice::init(DEVICE_NAME);
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new SmartGloveServerCallbacks());

  BLEService *service = server->createService(SERVICE_UUID);

  BLECharacteristic *commandCharacteristic = service->createCharacteristic(
      COMMAND_CHAR_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  commandCharacteristic->setCallbacks(new SmartGloveCommandCallbacks());

  notifyCharacteristic = service->createCharacteristic(
      NOTIFY_CHAR_UUID,
      BLECharacteristic::PROPERTY_NOTIFY);
  notifyCharacteristic->addDescriptor(new BLE2902());

  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("BLE ready: SmartGlove_BLE");
}

void setup() {
  Serial.begin(115200);
  Serial.println("System starting.");

  pinMode(VIB_PIN, OUTPUT);
  digitalWrite(VIB_PIN, LOW);

  setupBLE();

  SPI.begin();
  rfid.PCD_Init();
  delay(4);

  const byte version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  if (version == 0x00 || version == 0xFF) {
    Serial.println("RC522 was not found. Check SPI wiring and power.");
    while (true) {
      delay(10);
    }
  }

  Serial.print("RC522 connected. Version: 0x");
  Serial.println(version, HEX);
  Serial.println("Place a tag near the reader.");
}

void loop() {
  updateVibration();

  if (!deviceConnected && wasConnected) {
    BLEDevice::startAdvertising();
    Serial.println("BLE advertising restarted.");
    wasConnected = false;
  }

  if (deviceConnected && !wasConnected) {
    wasConnected = true;
  }

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    delay(5);
    return;
  }

  const String tagUID = formatUniqueId(rfid.uid);
  rfid.PICC_HaltA();

  if (tagUID == lastUID && millis() - lastReadTime < SAME_TAG_INTERVAL_MS) {
    return;
  }

  Serial.println("");
  Serial.println("Tag recognized.");
  Serial.println("UID: " + tagUID);

  // Only the UID is sent. The website responds with KNOWN or UNKNOWN for vibration.
  if (notifyUniqueId(tagUID)) {
    lastUID = tagUID;
    lastReadTime = millis();
  }
}
