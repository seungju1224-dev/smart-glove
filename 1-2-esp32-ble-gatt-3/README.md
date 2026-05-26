# 스마트 장갑 BLE 음성 터미널

이 구성은 ESP32가 제품명이 아닌 **태그 고유번호(UID)만** BLE GATT notification으로 전송하고, 웹 페이지가 UID와 제품명의 연결, 한국어 음성 출력, 등록 여부에 따른 진동 판단을 담당하도록 분리합니다.

## 파일

- `smart-glove-terminal.html`: Windows/Android Chrome용 웹 단말
- `esp32_smart_glove_ble.ino`: ESP32 + RC522(SPI) BLE 송신/진동 스케치

## 동작 방식

1. ESP32가 `SmartGlove_BLE` 이름과 서비스 UUID를 광고합니다.
2. 웹 페이지가 BLE 알림 characteristic을 구독합니다.
3. RC522가 태그를 읽으면 ESP32는 `04B0B8C20F1E90` 같은 UID 문자열만 보냅니다.
4. 처음 들어온 UID이면 웹 페이지가 즉시 `인식되지 않은 고유번호입니다`라고 말하고 제품명 등록 창을 표시합니다.
5. 제품명을 한 번 등록한 UID가 다시 들어오면, 저장된 제품명을 표시하고 `제품명 입니다`라고 말합니다.
6. 웹 페이지는 등록된 UID이면 `KNOWN`, 미등록 UID이면 `UNKNOWN`을 ESP32에 되돌려 보내며, ESP32는 각각 짧은 진동 또는 긴 진동을 실행합니다.

등록 정보는 웹 브라우저의 `localStorage`에 저장됩니다. 따라서 같은 브라우저에서는 다시 열어도 남아 있지만 Windows 노트북과 Android 휴대폰 사이에 자동 동기화되지는 않습니다. 페이지의 `등록 정보 내보내기`와 `등록 정보 가져오기`를 사용하면 등록 목록을 옮길 수 있습니다. 가져오기는 이미 등록된 UID의 이름을 덮어쓰지 않습니다.

## BLE 데이터 규격

웹 페이지와 ESP32 코드에 다음 값이 동일하게 설정되어 있습니다.

| 항목 | 값 |
| --- | --- |
| 장치 이름 | `SmartGlove_BLE` |
| 서비스 UUID | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| 알림 characteristic UUID | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` |
| 명령 characteristic UUID | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` |
| ESP32 → 웹 payload | 콜론 없는 16진수 UID 하나만 전송, 예: `04B0B8C20F1E90` |
| 웹 → ESP32 payload | 등록 여부 명령 `KNOWN` 또는 `UNKNOWN` |
| UID 문자 | 대문자 16진수, 최대 20자 |

RC522가 읽을 수 있는 UID는 4, 7, 10바이트일 수 있습니다. 콜론 없이 전송하면 10바이트 UID도 20자로 기본 BLE notification 한 번에 들어갑니다. 웹 페이지는 화면에는 `04:B0:B8:C2:0F:1E:90`처럼 콜론을 넣어 표시하며, 예전 콜론 포함 등록 데이터도 같은 UID로 정규화하여 읽습니다.

## ESP32 업로드와 RC522 SPI 연결

1. Arduino IDE에 Espressif의 ESP32 보드 패키지를 설치하고 사용하는 ESP32 보드를 선택합니다.
2. Arduino IDE의 라이브러리 관리자에서 `MFRC522`를 설치합니다.
3. RC522를 SPI 방식으로 연결합니다. 스케치의 설정은 `SS_PIN = 5`, `RST_PIN = 22`이며, ESP32 기본 VSPI 핀을 사용합니다.
4. 진동모터는 ESP32 GPIO에 직접 연결하지 말고 트랜지스터 또는 모터 드라이버와 역기전력 보호 회로를 통해 `VIB_PIN = 25`로 제어합니다.
5. `esp32_smart_glove_ble.ino`를 열어 ESP32에 업로드합니다. Dabble 앱, Terminal 모듈, 제품명 배열은 사용하지 않습니다.
6. 업로드 후 시리얼 모니터를 `115200` baud로 열고 웹 페이지에서 `SmartGlove_BLE`에 연결합니다.
7. RC522에 태그를 가까이 대면 UID만 BLE로 전송됩니다. 같은 UID는 성공적으로 보낸 뒤 3초 안에는 반복 전송되지 않습니다.

제품명 배열은 ESP32 코드에 없습니다. ESP32는 UID 송신과 웹 결과에 따른 진동만 담당하며, 제품명 등록/검색/음성 출력과 등록 상태 판단은 웹 페이지가 처리합니다.

## Windows 노트북에서 실행

Web Bluetooth는 안전한 컨텍스트에서 동작합니다. Windows에서 파일을 직접 더블 클릭하는 대신 이 폴더에서 로컬 웹 서버를 실행하는 방법이 안정적입니다.

```powershell
py -m http.server 8000
```

그 다음 Windows의 Chrome 또는 Edge에서 아래 주소를 엽니다.

```text
http://localhost:8000/smart-glove-terminal.html
```

`localhost`는 로컬 개발용 신뢰 가능한 출처로 취급됩니다. 페이지에서 `블루투스 장갑 연결`을 누르고 `SmartGlove_BLE`를 선택합니다.

## Android에서 실행

Android에서는 Chrome으로 **HTTPS 주소에 배포된** `smart-glove-terminal.html`을 여는 방식을 권장합니다. 예를 들어 GitHub Pages 같은 정적 HTTPS 호스팅에 이 HTML 파일을 게시하면 별도 앱 설치 없이 BLE 연결, 제품명 등록, 저장, 음성 안내를 사용할 수 있습니다.

휴대폰에서 로컬 HTML 파일을 직접 여는 경우 브라우저와 파일 열기 방식에 따라 Web Bluetooth 권한이 제공되지 않을 수 있습니다. 연결 버튼에서 안전한 컨텍스트 오류가 나오면 HTTPS로 게시한 주소에서 다시 엽니다.

Android 사용 순서:

1. Bluetooth를 켜고 Android Chrome에서 HTTPS 페이지를 엽니다.
2. `블루투스 장갑 연결`을 눌러 `SmartGlove_BLE`를 선택합니다.
3. 새 UID를 태그하면 미등록 안내 음성이 나오고, 페이지에 제품명 등록 입력란이 나타납니다.
4. 제품명을 등록한 뒤 같은 태그를 다시 읽으면 등록한 이름으로 음성 안내됩니다.

한국어 음성이 들리지 않으면 Android 설정에서 한국어 텍스트 음성 변환(TTS) 음성 데이터가 활성화되어 있는지 확인합니다.

## 참고 자료

- Web Bluetooth 보안 및 API: [MDN Web Bluetooth API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Bluetooth_API)
- Chrome Web Bluetooth 안내: [developer.chrome.com - Communicating with Bluetooth devices over JavaScript](https://developer.chrome.com/docs/capabilities/bluetooth)
- ESP32 Arduino BLE 예제: [arduino-esp32 BLE examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/BLE/examples)
