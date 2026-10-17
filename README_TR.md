# ETS2 Kablosuz ESP32-S3 Buton Paneli

Bu firmware, **ESP32-S3 DevKitC-1** kartını Bluetooth Low Energy (BLE) HID gamepad olarak kullanır. Mevcut kod yalnızca 13 dijital butonu destekler; analog joystick eksenleri bu sürümde yoktur.

## Donanım

- ESP32-S3 DevKitC-1
- 13 normalde açık buton (11 ana buton + 2 vites kulakçığı)
- Her buton GPIO ile GND arasına bağlanır

| HID butonu | GPIO |
|---:|---:|
| 1–11 | GPIO 6–16 (sırasıyla) |
| 12 | GPIO 5 |
| 13 | GPIO 21 |

Firmware `INPUT_PULLUP` kullanır: butona basılınca GPIO LOW okunur. GPIO pinlerine harici 5 V uygulama. Kalıcı kablolamadan önce kullandığın kart revizyonundaki GPIO etiketlerini doğrula.

## Derleme ve yükleme

1. Arduino IDE'de `ETS2_WirelessPanel.ino` dosyasını aç.
2. Kart olarak **ESP32S3 Dev Module** veya kart üreticisinin DevKitC-1 profiliyle uyumlu ESP32-S3 seçeneğini kullan.
3. GitHub Actions iş akışının kullandığı ESP32 Arduino core, NimBLE-Arduino ve ESP32-BLE-Gamepad sürümlerini kur.
4. Derle ve yükle.
5. Seri Monitör'ü **115200 baud** aç. Başlangıç ve BLE bağlantı mesajlarını kontrol et.

## Windows'ta test

1. Bluetooth ayarlarından **ETS2 Wireless Panel** cihazını eşleştir.
2. `Win + R` → `joy.cpl` aç.
3. Özellikler ekranında 13 butonun her birini fiziksel olarak test et.
4. Bir buton görünmüyorsa önce Seri Monitör'de ilgili GPIO'nun `BASILDI` ve `BIRAKILDI` mesajlarını kontrol et.

## Otomatik kontroller

GitHub Actions iki kontrol yapar:

- Python statik testleri: buton sayısı, GPIO listesi, yinelenen pinler, active-low bağlantı varsayımı ve HID press/release eşleşmesi.
- Arduino derlemesi: sketch'in ESP32-S3 hedefi için derlenip derlenmediği.

Bu kontroller fiziksel butonları, Bluetooth eşleşmesini veya ETS2 içindeki gerçek davranışı test edemez; bunlar kart üzerinde ayrıca doğrulanmalıdır.
