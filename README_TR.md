# ETS2 Kablosuz ESP32-S3 Paneli — SimHub RPM eklentisi dahil

Bu depo, SimHub'dan motor RPM bilgisini alıp ESP32-S3 panelinin LED'lerine göndermek için gereken yazılımı içerir.

**Veri akışı:** ETS2 → SimHub eklentisi → yerel UDP (127.0.0.1:29100) → Python BLE köprüsü → ESP32-S3 üzerindeki NuS servisi → 9 RPM LED'i.

İnternet çalışma sırasında gerekmez. USB, firmware yüklemek ve seri hata ayıklamak içindir; panel Windows'a Bluetooth Low Energy oyun kumandası olarak bağlanır.

## 1. SimHub eklentisini kur

1. SimHub'ı tamamen kapat.
2. Depoyu normal bir klasöre indir veya ZIP olarak çıkar.
3. **SimHubPlugin\Install-SimHubPlugin.bat** dosyasını çalıştır. Windows yönetici izni isterse ver.
4. Betik, SimHub SDK DLL'lerini bulup C# eklentisini yerel bilgisayarında derler ve DLL'yi SimHub klasörüne kopyalar.
5. SimHub'ı aç. Eklenti etkinleştirme isterse etkinleştir. Sol menüde **RPM UDP Bridge** sayfası ve gönderilen paket sayısı görünmelidir.

Derleme için Windows .NET Framework C# derleyicisi ve kurulu SimHub DLL'leri gerekir. GitHub Actions bu dosyalara erişemediğinden **C# eklentisinin gerçek derlemesini doğrulamıyor**; yalnızca kaynak ve derleme betiğinin varlığını kontrol ediyor. ESP32 ve Python kontrollerinin yeşil olması fiziksel SimHub kurulumunun test edildiği anlamına gelmez.

Eklentiyi kaldırmak için SimHub'ı kapatıp **SimHubPlugin\Uninstall-SimHubPlugin.ps1** betiğini çalıştır. Betiğin oluşturduğu yedek DLL dosyalarını silmez.

## 2. Python BLE köprüsünü çalıştır

PowerShell'de depo klasörüne girip şunları çalıştır:

~~~powershell
py -m pip install -r requirements.txt
py .\ble_udp_rpm_bridge.py
~~~

Köprü **127.0.0.1:29100** adresinde UDP paketi bekler. SimHub eklentisi yaklaşık her 50 ms'de bir mevcut RPM ile aracın maksimum RPM değerini gönderir. Python köprüsü bu ikiliyi 0–3500 aralığındaki LED bar ölçeğine dönüştürür; böylece farklı kırmızı çizgi devirlerine sahip ETS2 araçlarında bar üst devirde tam dolar. Eski tek değerli RPM paketleri de geriye dönük desteklenir. Python köprüsü paneli BLE reklamındaki NuS servis UUID'si veya tanınan cihaz adıyla bulur, RX karakteristiğinin desteklediği GATT yazma modunu kontrol eder ve bar değerini iletir.

RPM ayrıştırma testlerini çalıştırmak için:

~~~powershell
py -m unittest discover -s tests -v
~~~

## 3. ESP32-S3 firmware'i

**ETS2_WirelessPanel.ino**, ESP32-S3'ü Bluetooth oyun kumandası olarak çalıştırır ve aynı BLE sunucusuna NuS seri servisini ekler.

Firmware'in GitHub Actions derlemesinde doğrulanan bağımlılık sürümleri:

- ESP32 Arduino core: **3.3.12**
- ESP32-BLE-Gamepad: **0.8.0**
- NimBLE-Arduino: **2.5.1**
- NuS-NimBLE-Serial: **4.2.1**

Firmware, BLE üzerinden 14 dijital buton ve 4 analog joystick ekseni gönderir. NuS üzerinden aldığı geçerli RPM satırlarına göre 9 LED'i kademeli yakar. Bu işlevlerin fiziksel kartta ve oyunda çalıştığı henüz doğrulanmış değildir.

Mevcut pin eşlemesi:

- Butonlar: GPIO 6–18 ve GPIO 21 (toplam 14)
- Joystick eksenleri: GPIO 1, 2, 3, 4
- RPM LED'leri: GPIO 5, 38, 39, 40, 41, 42, 47, 48, 33

**Kablolamadan önce kartın gerçek pinlerini doğrula.** GPIO3 strapping pinidir; joystick devresinin reset sırasında uygunsuz seviyeye çekmediğinden emin ol. Resmî ESP32-S3-DevKitC-1 v1.0 kartında yerleşik RGB LED GPIO48'e, v1.1'de GPIO38'e bağlıdır; pin listemiz bu iki pini de ayrı LED çıkışı olarak kullandığından, kartında hangisi yerleşik RGB LED ise o çıkış çakışır ve pin eşlemesi kart revizyonuna göre değiştirilmelidir. Octal SPI flash/PSRAM içeren bazı modüllerde GPIO35–37 harici kullanım için uygun değildir. Her ayrı LED'e seri akım sınırlama direnci koy (genellikle 220–330 Ω, LED türüne göre doğrula). **12 V LED şeridini doğrudan ESP32 GPIO'suna bağlama.**

## 4. Joystick eksenlerini kalibre et

Varsayılan ADC merkezleri dört eksen için 2048'dir; bunlar henüz fiziksel joystick'lerinde ölçülmüş değerler değildir.

1. ETS2_WirelessPanel.ino dosyasında DEBUG_JOYSTICK_ADC değerini geçici olarak true yap ve firmware'i yükle.
2. Arduino IDE Seri Monitör'ü **115200 baud** ile aç. Çubuklar bırakılmış ve normal merkez konumundayken JOY_ADC:değer1,değer2,değer3,değer4 satırlarını izle.
3. Değerler sabitlenince dört merkez sayısını JOYSTICK_CENTER_ADC dizisine sırayla yaz. Sıra GPIO 1/X, GPIO 2/Y, GPIO 3/Z, GPIO 4/Rz'dir.
4. Her ekseni ayrı ayrı iki fiziksel sonuna götürüp JOY_ADC değerlerini kaydet. Gerekirse JOYSTICK_MIN_ADC ve JOYSTICK_MAX_ADC dizilerine ölçtüğün alt/üst değerleri gir; böylece çubuk tam hareket mesafesinde HID ekseninin tamamını kullanır. Varsayılan 0/4095 değerlerini kullanmak da mümkündür.
5. Yönlerden biri Windows oyun kumandası ekranında ters görünüyorsa aynı sıradaki JOYSTICK_INVERTED girdisini true yap.
6. DEBUG_JOYSTICK_ADC değerini false olarak geri alıp son firmware'i yükle.

Başlangıç deadzone değerleri menü eksenlerinde 100/70, kamera eksenlerinde 170/130 ADC sayımıdır (etkinleştirme/bırakma). Kamera joystick'i direksiyon titreşiminden etkilenirse önce gerçek merkez değerlerini ölç; rastgele pin değiştirme veya çok büyük deadzone belirleme.

## 5. Hızlı kontrol

- SimHub eklentisindeki paket sayısı artıyorsa SimHub → UDP gönderimi gerçekleşiyordur.
- Python penceresinde UDP dinleme adresi gösterilmelidir.
- Panel bulunduğunda Python penceresinde BLE cihaz adı ve bağlantı durumu görünmelidir.
- Paket sayısı artarken LED'ler yanmıyorsa önce NuS BLE bağlantısını, doğru karakteristiği ve ESP32 kablolamasını kontrol et.

## Test sınırı

GitHub Actions'ta son başarılı çalışmada Python testleri ve ESP32-S3 firmware derlemesi geçti. Bu otomatik kontroller donanım testinin yerine geçmez. SimHub eklentisinin C# derlemesi, Windows'taki yerel SimHub DLL'leriyle ayrıca doğrulanmalıdır.
