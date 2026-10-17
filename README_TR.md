# ETS2 Kablosuz ESP32-S3 Paneli

Bu proje, ESP32-S3 tabanlı kablosuz bir oyun kumandasıdır. Aktif firmware **Bluetooth Low Energy (BLE) HID** üzerinden 15 dijital buton ve 4 analog joystick ekseni gönderir.

## Özellikler

- 15 dijital buton
- 4 analog joystick ekseni
- Butonlar için debounce
- Joystick ADC okumalarında ortalama filtreleme
- Eksen başına merkez ve uç nokta kalibrasyonu
- Merkez çevresinde histerezisli deadzone
- Eksen yönünü yazılımdan ters çevirme
- İsteğe bağlı ham ADC tanılama çıktısı

## 1. Firmware'i yükle

Arduino IDE'de `ETS2_WirelessPanel.ino` dosyasını aç. GitHub Actions derlemesinde kullanılan sürümler ESP32 Arduino core **3.3.12**, NimBLE-Arduino **2.5.1** ve ESP32-BLE-Gamepad **0.8.0** sürümleridir. Doğru ESP32-S3 kartını seçip firmware'i yükle.

Seri Monitör'ü **115200 baud** ile açarsan başlangıçta `ETS2_PANEL_READY` mesajını görmelisin.

## 2. Buton ve joystick bağlantıları

- **11 normal buton:** GPIO 6–16
- **Joystick basma butonları:** GPIO 17 ve 18
- **Vites kulakçıkları:** GPIO 5 ve 21
- **Dört analog joystick ekseni:** GPIO 1, 2, 3 ve 4

Butonları GPIO ile **GND** arasına bağla. Firmware `INPUT_PULLUP` kullandığı için buton basılınca giriş LOW olur. GPIO'lara harici 5 V uygulama.

Joystick modüllerini **3.3 V** ve GND ile besle; potansiyometrelerin orta uçlarını ilgili ADC GPIO'larına bağla. ESP32-S3 ADC girişlerine 5 V verme.

**Kablolamadan önce kartın gerçek pinlerini doğrula.** GPIO3 strapping pinidir; bağlı joystick'in reset sırasında pini uygunsuz seviyeye çekmediğini kontrol et. Kart revizyonuna ve flash/PSRAM varyantına göre kullanılabilir pinler değişebilir.

Ayrıntılı eşleme için [PIN_MAP.md](PIN_MAP.md) dosyasına bak.

## 3. Joystick merkezlerini kalibre et

Varsayılan ADC merkezleri dört eksen için 2048'dir; bunlar fiziksel joystick'lerinde ölçülmüş değerler değildir.

1. `ETS2_WirelessPanel.ino` dosyasında `DEBUG_JOYSTICK_ADC` değerini geçici olarak `true` yap ve firmware'i yükle.
2. Arduino IDE Seri Monitör'ü **115200 baud** ile aç.
3. Çubuklar bırakılmış ve normal merkez konumundayken `JOY_ADC:değer1,değer2,değer3,değer4` satırlarını izle.
4. Sabit merkez değerlerini `JOYSTICK_CENTER_ADC` dizisine yaz. Sıra GPIO 1/X, GPIO 2/Y, GPIO 3/Z, GPIO 4/Rz'dir.
5. Her eksenin iki fiziksel sonundaki değerleri ölç. Gerekirse `JOYSTICK_MIN_ADC` ve `JOYSTICK_MAX_ADC` dizilerini ayarla.
6. Bir eksenin yönü tersse aynı sıradaki `JOYSTICK_INVERTED` girdisini `true` yap.
7. `DEBUG_JOYSTICK_ADC` değerini tekrar `false` yapıp firmware'i yükle.

Başlangıç deadzone değerleri menü eksenlerinde 100/70, kamera eksenlerinde 170/130 ADC sayımıdır (etkinleştirme/bırakma). Bunlar başlangıç değerleridir; fiziksel merkez gürültüsüne göre ayarlanmalıdır.

## 4. Windows ve ETS2 kontrolü

1. Windows Bluetooth ayarlarından **ETS2 Custom Wheel** cihazını eşleştir.
2. Windows'un oyun kumandası test ekranında 15 butonun ve dört eksenin göründüğünü doğrula.
3. Menü joystick'inin X/Y, kamera joystick'inin Z/Rz eksenlerini kontrol et.
4. ETS2 kontrol ayarlarında butonları ve eksenleri ata; ters yönde hareket eden eksenleri kalibrasyon dizisiyle düzelt.

## Test sınırı

GitHub Actions firmware derlemesini otomatik kontrol eder. Bu, fiziksel joystick merkezlerinin, buton kablolamasının, Windows Bluetooth eşleşmesinin veya ETS2 içindeki eksen yönlerinin donanım üzerinde doğrulandığı anlamına gelmez.
