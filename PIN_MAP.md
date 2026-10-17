# ESP32-S3 DevKitC-1 — Buton Pin Haritası

Bu tablo mevcut `ETS2_WirelessPanel.ino` koduyla eşleşir. Firmware 13 dijital butonu BLE HID gamepad olarak gönderir; bu sürümde analog joystick veya ADC ekseni yoktur.

| Fiziksel işlev | HID butonu | GPIO | Bağlantı |
|---|---:|---:|---|
| Ana buton 1 | 1 | GPIO 6 | GPIO ile GND arasına normalde açık buton |
| Ana buton 2 | 2 | GPIO 7 | GPIO ile GND arasına normalde açık buton |
| Ana buton 3 | 3 | GPIO 8 | GPIO ile GND arasına normalde açık buton |
| Ana buton 4 | 4 | GPIO 9 | GPIO ile GND arasına normalde açık buton |
| Ana buton 5 | 5 | GPIO 10 | GPIO ile GND arasına normalde açık buton |
| Ana buton 6 | 6 | GPIO 11 | GPIO ile GND arasına normalde açık buton |
| Ana buton 7 | 7 | GPIO 12 | GPIO ile GND arasına normalde açık buton |
| Ana buton 8 | 8 | GPIO 13 | GPIO ile GND arasına normalde açık buton |
| Ana buton 9 | 9 | GPIO 14 | GPIO ile GND arasına normalde açık buton |
| Ana buton 10 | 10 | GPIO 15 | GPIO ile GND arasına normalde açık buton |
| Ana buton 11 | 11 | GPIO 16 | GPIO ile GND arasına normalde açık buton |
| Sol vites kulakçığı | 12 | GPIO 5 | GPIO ile GND arasına normalde açık buton |
| Sağ vites kulakçığı | 13 | GPIO 21 | GPIO ile GND arasına normalde açık buton |

## Elektriksel davranış

- Tüm girişler `INPUT_PULLUP` olarak ayarlanır.
- Basılmayan buton HIGH, basılan buton LOW okunur.
- Her butonun yalnızca iki ucu vardır: biri ilgili GPIO'ya, diğeri GND'ye.
- GPIO'lara harici 5 V uygulama.
- GPIO 6–16 kullanılabilirliğini gerçek ESP32-S3 DevKitC-1 kart revizyonunun pin şemasıyla doğrula; farklı ESP32 kartlarıyla karıştırma.

## Test

1. Firmware yüklendikten sonra Windows'ta `Win + R` → `joy.cpl` aç.
2. BLE cihazını seçip Özellikler'e gir.
3. Butonları tek tek basıp HID numaralarını bu tabloyla karşılaştır.
4. Sorun varsa 115200 baud Seri Monitör'de GPIO başına `BASILDI` / `BIRAKILDI` mesajlarını incele.

Bu eşleme kod ve dokümantasyonla kontrol edilmiştir; fiziksel kart üzerinde henüz test edildiği anlamına gelmez.
