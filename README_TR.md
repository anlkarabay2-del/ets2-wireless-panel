# ETS2 Kablosuz ESP32-S3 Paneli — RPM telemetri seçenekleri

Bu depo, ETS2 motor devrini ESP32-S3 panelindeki 9 ayrı LED'e göndermek için gereken yazılımı içerir. RPM kaynağı olarak **mevcut SimHub eklentisini** veya **alternatif yerel ETS2 telemetri eklentisini** kullanabilirsin.

**Veri akışı (SimHub):** ETS2 → SimHub eklentisi → UDP 127.0.0.1:29100 → Python BLE köprüsü → ESP32-S3 NuS → 9 RPM LED'i.

**Veri akışı (yerel telemetri eklentisi):** ETS2 → SCS SDK telemetri eklentisi → JSON/UDP 127.0.0.1:49001 → Python BLE köprüsü → ESP32-S3 NuS → 9 RPM LED'i.

İki kaynağı aynı anda kullanman gerekmiyor. Python köprüsü her iki veri biçimini de kabul eder. İnternet çalışma sırasında gerekmez; USB firmware yüklemek ve hata ayıklamak içindir. Panel Windows'a Bluetooth Low Energy oyun kumandası olarak bağlanır.

## Seçenek A — Mevcut SimHub yöntemini kullan

1. SimHub'ı tamamen kapat.
2. Depoyu normal bir klasöre indir veya ZIP olarak çıkar.
3. \`SimHubPlugin\\Install-SimHubPlugin.bat\` dosyasını çalıştır. Windows yönetici izni isterse ver.
4. SimHub'ı aç. Eklenti etkinleştirme isterse etkinleştir. Sol menüde **RPM UDP Bridge** sayfası ve gönderilen paket sayısı görünmelidir.
5. Python köprüsünü varsayılan UDP portuyla çalıştır (aşağıdaki Python bölümüne bak).

Derleme için Windows .NET Framework C# derleyicisi ve kurulu SimHub DLL'leri gerekir. GitHub Actions, bu DLL'lere erişemediğinden C# eklentisinin gerçek derlemesini doğrulamaz. Fiziksel SimHub kurulumu ayrıca test edilmelidir.

Eklentiyi kaldırmak için SimHub'ı kapatıp \`SimHubPlugin\\Uninstall-SimHubPlugin.ps1\` betiğini çalıştır. Betiğin oluşturduğu yedek DLL dosyalarını silmez.

## Seçenek B — SimHub olmadan yerel ETS2 telemetri eklentisi

Bu alternatif, açık kaynaklı [MrHokss/ets2-telemetry-udp](https://github.com/MrHokss/ets2-telemetry-udp) projesini kullanır. Eklenti SCS Telemetry SDK üzerinden RPM'yi alıp JSON paketi olarak varsayılan \`127.0.0.1:49001\` adresine gönderir. Python köprüsü JSON içindeki \`rpm\` alanını okuyacak şekilde güncellendi.

### 1. Eklentiyi Windows için derle

1. [MinGW-w64](https://www.mingw-w64.org/) C++ derleyicisini kur; \`g++\` komutunun terminalde çalıştığını doğrula.
2. [Eklentinin GitHub deposunu](https://github.com/MrHokss/ets2-telemetry-udp) ZIP olarak indir ve çıkar. Depodaki \`include\` ve \`src\` klasörleri aynı ana klasörde kalmalı.
3. O klasörde terminal aç ve şu komutu çalıştır:

~~~powershell
g++ -shared -o ets2_telemetry_udp.dll src/telemetry.cpp -Iinclude -lws2_32
~~~

4. Derleme başarılı olursa \`ets2_telemetry_udp.dll\` dosyasını ETS2'nin \`bin\\win_x64\\plugins\` klasörüne kopyala. \`plugins\` klasörü yoksa oluştur.
5. ETS2'yi aç. Telemetri eklentisi izin penceresi gösterirse onayla.

**Not:** Bu alternatif yeni ve topluluk tarafından sağlanan bir eklentidir. Kaynak kodu ve derleme adımları incelenmiştir ancak bu projede Windows üzerinde gerçek oyun testi yapılmadı. Derleyici hatası, SDK uyumluluğu veya oyunun eklentiyi kabul etmemesi durumunda mevcut SimHub yöntemine geri dönebilirsin.

### 2. Python köprüsünü 49001 portunda çalıştır

PowerShell'de panel projesi klasörüne girip şunları çalıştır:

~~~powershell
py -m pip install -r requirements.txt
$env:ETS2_UDP_PORT = "49001"
py .\\ble_udp_rpm_bridge.py
~~~

Bu ayar yalnızca açık olan PowerShell oturumu için geçerlidir. SimHub yöntemine geri döneceksen yeni terminal açabilir veya \`$env:ETS2_UDP_PORT = "29100"\` ayarlayabilirsin.

## Python BLE köprüsü

Köprü, seçilen UDP portundan RPM alır ve Bluetooth üzerinden ESP32-S3'ün Nordic UART Service (NuS) RX karakteristiğine satır sonuyla birlikte gönderir. Kabul edilen UDP biçimleri:

- \`1420\`
- \`RPM:1420\`
- \`{"speed": 12.3, "rpm": 550.5, "gear": 1}\`

Köprü geçersiz RPM değerlerini yok sayar. Telemetri 1 saniyeden uzun süre kesilirse LED'lerin son devirde takılı kalmaması için sıfır RPM gönderir.

Varsayılan port **29100**'dür; mevcut SimHub eklentisi için değişiklik gerekmez.

## ESP32-S3 firmware'i

\`ETS2_WirelessPanel.ino\`, ESP32-S3'ü Bluetooth oyun kumandası olarak çalıştırır ve aynı BLE sunucusuna NuS seri servisini ekler.

Firmware'in GitHub Actions derlemesinde doğrulanan bağımlılık sürümleri:

- ESP32 Arduino core: **3.3.12**
- ESP32-BLE-Gamepad: **0.8.0**
- NimBLE-Arduino: **2.5.1**
- NuS-NimBLE-Serial: **4.2.1**

Firmware, BLE üzerinden 14 dijital buton ve 4 analog joystick ekseni gönderir. NuS üzerinden aldığı RPM satırlarına göre 9 LED'i kademeli yakar. Bu işlevlerin fiziksel kartta ve oyunda çalıştığı henüz doğrulanmış değildir.

Mevcut pin eşlemesi:

- Butonlar: GPIO 6–18 ve GPIO 21 (toplam 14)
- Joystick eksenleri: GPIO 1, 2, 3, 4
- RPM LED'leri: GPIO 5, 38, 39, 40, 41, 42, 47, 48, 33

**Kablolamadan önce kartın gerçek pinlerini doğrula.** GPIO3 strapping pinidir; joystick devresinin reset sırasında uygunsuz seviyeye çekmediğinden emin ol. Resmî ESP32-S3-DevKitC-1 v1.0 kartında yerleşik RGB LED GPIO48'e, v1.1'de GPIO38'e bağlıdır; pin listemiz bu iki pini de ayrı LED çıkışı olarak kullandığından, kartında hangisi yerleşik RGB LED ise o çıkış çakışır ve pin eşlemesi kart revizyonuna göre değiştirilmelidir. Octal SPI flash/PSRAM içeren bazı modüllerde GPIO35–37 harici kullanım için uygun değildir. Her ayrı LED'e seri akım sınırlama direnci koy (genellikle 220–330 Ω, LED türüne göre doğrula). **12 V LED şeridini doğrudan ESP32 GPIO'suna bağlama.**

## Hızlı kontrol

- SimHub eklentisinde gönderilen paket sayısı artıyorsa SimHub → UDP gönderimi çalışıyordur.
- Python penceresinde \`UDP RPM listener: 127.0.0.1:PORT\` görünmelidir.
- Panel bulunduğunda Python penceresinde BLE cihaz adı ve bağlantı durumu görünmelidir.
- RPM paketi geliyor ama LED'ler yanmıyorsa önce NuS BLE bağlantısını, doğru karakteristiği ve ESP32 kablolamasını kontrol et.

## Test sınırı

Python parser testleri ve ESP32-S3 firmware derlemesi otomatik olarak kontrol edilebilir; bu kontroller fiziksel donanım testinin yerine geçmez. SimHub C# eklentisi ve alternatif yerel ETS2 eklentisi Windows'ta, gerçek oyunla ayrıca doğrulanmalıdır.
