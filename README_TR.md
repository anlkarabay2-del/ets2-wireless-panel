# ETS2 Kablosuz ESP32-S3 Paneli — SimHub RPM eklentisi dahil

Bu paket artık SimHub'dan gerçek RPM değerini yerel UDP'ye çıkaran C# eklentisini de içeriyor. Akış:

`ETS2 -> SimHub -> ETS2RpmUdpBridge.dll -> 127.0.0.1:29100/UDP -> ble_udp_rpm_bridge.py -> BLE NuS -> ESP32-S3`

İnternet gerekmez. USB yalnızca ESP32'ye firmware yüklemek / seri hata ayıklamak içindir; panelin oyun kumandası bağlantısı Bluetooth BLE'dir.

## 1. SimHub eklentisini kur

1. SimHub'ı tamamen kapat.
2. ZIP dosyasını normal bir klasöre çıkar.
3. `SimHubPlugin\Install-SimHubPlugin.bat` dosyasını çalıştır. Windows yönetici izni isteyebilir; izin ver.
4. Derleme başarılı olunca dosya `C:\Program Files (x86)\SimHub\ETS2RpmUdpBridge.dll` konumuna kopyalanır (SimHub başka klasördeyse script otomatik arar).
5. SimHub'ı aç. Eklenti etkinleştirme isterse etkinleştir. Sol menüde **RPM UDP Bridge** sayfası, hedef adresi ve gönderilen paket sayısı görünür.

Eklenti Windows'taki .NET Framework C# derleyicisini ve kurulu SimHub DLL'lerini kullanarak kendi bilgisayarında derlenir; harici NuGet paketi indirmez. Derleyici bulunamazsa hata mesajı verir ve DLL kurmaz. Bu çalışma ortamında Windows/SimHub DLL'leri bulunmadığından derlenmiş DLL'yi burada doğrulayamadım.

Eklentiyi kaldırmak için SimHub'ı kapatıp `SimHubPlugin\Uninstall-SimHubPlugin.ps1` dosyasını çalıştır. Scriptin oluşturduğu yedek DLL'lere dokunmaz.

## 2. ESP32 BLE köprüsünü çalıştır

PowerShell'de paket klasörüne gidip:

```powershell
py -m pip install --upgrade bleak
py .\ble_udp_rpm_bridge.py
```

Köprü şu adreste dinler: `127.0.0.1:29100`. SimHub eklentisi 50 ms aralıkla `RPM:1420` biçiminde yerel UDP paketi gönderir. Python köprüsü, BLE NuS bağlantısı kurulunca RPM'yi ESP32'ye yollar.

## 3. ESP32 firmware'i

`ETS2_WirelessPanel.ino`, mevcut BLE Gamepad koduna NuS alıcısını ekleyen firmware'dir. Arduino IDE'de kartın ESP32-S3 DevKitC-1 sürümüne uygun kart seçeneğiyle aç.

Gerekli kütüphaneler:

- `ESP32-BLE-Gamepad` (NuS uyumluluk desteği olan sürüm)
- `NimBLE-Arduino` 2.5.x
- `NuS-NimBLE-Serial` (afpineda)

Mevcut aday pin dizileri kod içinde korunmuştur: 14 buton `{6..18,21}`, dört joystick ekseni `{1,2,3,4}`, dokuz LED `{5,38,39,40,41,42,47,48,33}`.

**Kablolamadan önce pinleri kesinleştir:** GPIO3 ESP32-S3 açılış/strapping davranışı açısından risklidir. GPIO33 ve GPIO48'in kullanılabilirliği ile kartın dahili RGB LED bağlantısı, DevKitC-1 üzerindeki modül ve kart revizyonuna bağlıdır. Sketch'in bu ortamda derlenmiş veya fiziksel donanımda denenmiş olduğu iddia edilmiyor.

## 4. Hızlı kontrol

- SimHub eklenti ekranında `Packets sent` sürekli artıyorsa SimHub -> UDP kısmı çalışıyor.
- Python penceresinde `UDP RPM listener: 127.0.0.1:29100` görünmeli.
- Python, BLE panelini bulup bağlandığında `BLE connected` yazar.
- Eklentinin paket sayısı artıyor ama RPM LED'leri yanmıyorsa sorun UDP kaynak tarafında değil; NuS BLE keşfi/bağlantısı veya ESP32 firmware tarafında aranmalı.

## Kaynak notları

Eklenti, SimHub'ın `IPlugin`, `IDataPlugin` ve `IWPFSettingsV2` SDK arayüzlerini kullanır. Referanslar: [SimHub Plugin SDK rehberi](https://github.com/SHWotever/SimHub/wiki/Plugin-and-extensions-SDKs), [SimHub SDK örnek projesi](https://github.com/blekenbleu/SimHubPluginSdk).
