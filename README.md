# NodeDrop

İnternetsiz çalışan, şifreli akıllı kargo teslim kutusu. ESP32 kendi Wi-Fi ağını kurar, kurye telefonundan tek tuşla teslimatı onaylar, alıcı 4 haneli şifreyi tuş takımına girerek kutuyu açar.

## Amaç

Kargo teslimatında kurye ile alıcının aynı anda aynı yerde olması gerekir. Alıcı evde değilse kargo kapıda bırakılır, çalınır ya da şubeye geri döner. NodeDrop bu sorunu çözer: kurye kargoyu kutuya bırakır, kutu kilitlenir, alıcıya tek kullanımlık bir şifre verilir. Alıcı istediği zaman gelip şifreyle kargosunu alır.

Sistem **internet, SIM kart veya bulut hizmeti gerektirmez.** Bütün işlem ESP32'nin kendi yerel ağı üzerinde yapılır.

## Nasıl çalışır?

1. Kurye telefonuyla `NodeDrop` Wi-Fi ağına bağlanır (şifre: `nodedrop123`). Panel `192.168.4.1` adresinde otomatik açılır.
2. Kargoyu kutuya koyar ve kapağı kapatır.
3. Panelde **"Kargoyu Teslim Et"** düğmesine basar.
4. Tavandaki HC-SR04 mesafe sensörü kutuda kargo olup olmadığına bakar. Kargo yoksa kilit çalışmaz ve hata gösterilir.
5. Kargo varsa MG996R servo mandalı kapatır, kırmızı LED yanar, panelde SMS benzeri bir kartta rastgele 4 haneli şifre görünür.
6. Alıcı şifreyi 4x4 tuş takımına girer. Doğruysa servo açar, yeşil LED yanar, buzzer onay sesi verir.
7. Kargo alınınca sensör kutunun boşaldığını algılar ve sistem yeni teslimata hazır hale gelir.

### Güvenlik

- Her teslimat için rastgele (ESP32 donanım rastgele sayı üreteci) tek kullanımlık 4 haneli şifre üretilir.
- 3 yanlış denemeden sonra tuş takımı 30 saniye kilitlenir.
- Yönetici şifresi (`MASTER_PIN`) kutuyu her durumda açar. Kodda değiştirin.
- Kargo algılanmadan kilit kapanmaz, yani boş kutu kilitlenmez.

## Malzeme listesi

| Parça | Adet | Görev |
|---|---|---|
| ESP32-WROOM-32 geliştirme kartı (Type-C) | 1 | Beyin, Wi-Fi erişim noktası, web sunucusu |
| HC-SR04 ultrasonik mesafe sensörü | 1 | Kutuda kargo var mı, yok mu |
| MG996R servo motor | 1 | Kilit mandalını hareket ettirir |
| 4x4 membran tuş takımı | 1 | Alıcı şifre girişi |
| Kırmızı LED | 1 | Kilitli göstergesi |
| Yeşil LED | 1 | Açık / boş göstergesi |
| Buzzer modülü (3 pinli) | 1 | Sesli geri bildirim |
| 220 Ω direnç | 2 | LED akım sınırlama |
| 1 kΩ direnç | 1 | Echo hattı gerilim bölücü (üst) |
| 2.2 kΩ direnç | 1 | Echo hattı gerilim bölücü (alt) |
| Breadboard | 1 | Güç rayları ve bağlantılar |
| Jumper kablo (erkek-dişi, erkek-erkek) | 1 takım | Bağlantılar |
| Powerbank (20000 mAh) + USB-C kablo | 1 | Güç kaynağı |
| Kontrplak kutu (50x32x42 cm, 10 mm veya 6 mm), menteşe, mandal kancası | 1 | Gövde ve kapak |

## Pin bağlantıları

| Bileşen | ESP32 pini |
|---|---|
| Servo sinyal | GPIO13 |
| HC-SR04 TRIG | GPIO33 |
| HC-SR04 ECHO | GPIO34 (1 kΩ + 2.2 kΩ bölücü ile 5V → 3.3V) |
| Kırmızı LED | GPIO22 (220 Ω ile) |
| Yeşil LED | GPIO21 (220 Ω ile) |
| Buzzer | GPIO23 |
| Tuş takımı satırlar | GPIO32, 25, 26, 27 |
| Tuş takımı sütunlar | GPIO16, 17, 18, 19 |

Servo ve HC-SR04 5V ihtiyacı için `VIN` pininden beslenir. Buzzer modülü 3V3'e bağlanır.

> Tuş takımı pin dizileri, şerit kablonun ters bağlanması nedeniyle ters sırada yazılmıştır. Kendi kablonuza göre `satirPin` ve `sutunPin` dizilerini değiştirin.

## Kurulum

1. [Arduino IDE 2.x](https://www.arduino.cc/en/software) kurun.
2. **File → Preferences → Additional boards manager URLs** alanına ekleyin:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. **Boards Manager**'dan **esp32 by Espressif Systems** paketini kurun (test edilen sürüm: 3.3.12).
4. **Library Manager**'dan şunları kurun: **ESP32Servo** ve **Keypad**.
5. `NodeDrop/NodeDrop.ino` dosyasını açın. Kart: **ESP32 Dev Module**.
6. Doğru portu seçip **Upload** edin. "Connecting..." yazısında kalırsa karttaki BOOT düğmesine basılı tutun.

## Ayarlar (kodun başında)

| Değişken | Anlamı |
|---|---|
| `AG_ADI`, `AG_SIFRE` | Wi-Fi ağ adı ve şifresi (en az 8 karakter) |
| `ESIK_CM` | Bu mesafeden yakınsa kargo var sayılır. Boş kutunun ölçtüğü mesafenin birkaç cm altına ayarlayın |
| `SERVO_KILIT`, `SERVO_ACIK` | Servo konumları (mikrosaniye, 1000-2000) |
| `MASTER_PIN` | Yönetici şifresi |
| `TEST_TUSLARI` | `true` iken A = aç, B = kilitle, C = mesafeyi yaz. Sunumda `false` yapın |
| `BROWNOUT_KAPAT` | Güç dalgalanmasında kart sürekli resetleniyorsa `true` yapın |

## Teknik notlar

- Panel, ESP32 içine gömülüdür; dış kaynak, CDN veya internet gerekmez. Captive portal sayesinde ağa bağlanınca panel kendiliğinden açılır.
- `tone()` kullanılmaz; ESP32'de servo PWM zamanlayıcısıyla çakışır. Buzzer yazılımla sürülür.
- Servo mikrosaniye ile sürülür, yavaş adımlarla hareket eder ve iş bitince sinyali keser (titreme ve akım çekişi azalır).
- Mesafe 5 ölçümün ortancası alınarak okunur, gürültüye karşı daha kararlıdır.
- Powerbank'ler düşük akımda kendini kapatabilir; uzun süreli kullanımda test edin.

## Hangi sektöre, kime kolaylık sağlar?

**Sektörler**
- **Kargo ve e-ticaret:** Başarısız teslimat, ikinci kez dağıtım ve şubeye iade maliyetini azaltır.
- **Apartman ve site yönetimi:** Kapıcı ya da güvenliğe emanet bırakılan kargo karmaşasını bitirir.
- **Okul ve yurtlar:** Öğrencilere gelen kargolar şifreyle teslim edilir.
- **Ofis ve iş merkezleri:** Resepsiyon yükünü azaltır, evrak ve numune teslimini kayıt altına alır.
- **Eczane ve küçük işletmeler:** Mesai dışı sipariş teslimi için güvenli bırakma noktası olur.
- **Kırsal ve internetin zayıf olduğu bölgeler:** Çalışması için internet gerekmez.

**Kimlere faydası var**
- **Alıcılara:** Evde olmak zorunda kalmazlar, kargo çalınmaz, istedikleri zaman alırlar.
- **Kuryelere:** Kapıda bekleme, telefon açma ve geri dönme süresi azalır. Günde daha çok teslimat yaparlar.
- **Kargo firmalarına:** Teslim edilemeyen paket oranı ve yakıt/işçilik maliyeti düşer.
- **Site ve bina yönetimine:** Kargo saklama sorumluluğundan kurtulur.
- **Öğrenci ve maker topluluğuna:** Ucuz, açık kaynak, ESP32 ile yapılabilecek tam bir IoT örnek projesidir.

## Geliştirme fikirleri

- Telefondan eşik ve servo konumlarını ayarlama sayfası (`Preferences` ile kalıcı kayıt)
- Birden fazla bölmeli kutu
- Teslimat kayıt günlüğü (zaman damgası)
- Gerçek SMS / bildirim için isteğe bağlı GSM modülü
- Daha güçlü kilit için solenoid veya ikinci mandal

## Lisans

MIT. Dilediğiniz gibi kullanın, geliştirin ve paylaşın.
