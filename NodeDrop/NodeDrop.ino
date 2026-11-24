// NodeDrop v2 - Tam sistem (ESP32-WROOM-32)
// Kütüphaneler: ESP32Servo, Keypad
// Kart: ESP32 Dev Module
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESP32Servo.h>
#include <Keypad.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ---------- AYARLAR ----------
const char* AG_ADI   = "NodeDrop";
const char* AG_SIFRE = "nodedrop123";   // en az 8 karakter

#define SERVO_PIN 13
#define TRIG_PIN  33
#define ECHO_PIN  34
#define LED_KIRMIZI 22
#define LED_YESIL   21
#define BUZZER_PIN  23

// Servo mikrosaniye (darbe süresi) ile sürülür: 1000 us ile 2000 us arası yaklaşık 90 derece.
// Mil az dönüyorsa farkı büyüt (örn. 900 ve 2100), çok dönüyorsa küçült.
const int   SERVO_KILIT = 2000;   // Teslim Et'e basınca bu konuma gider, kilitler
const int   SERVO_ACIK  = 1000;   // şifre girilince bu konuma gider, açar
const float ESIK_CM     = 35.5;   // bundan YAKIN ise kargo var (bos kutu ~38.5 cm; test sonrasi ayarla)
const char* MASTER_PIN  = "9876"; // yönetici şifresi (her durumda kilidi açar)

// Test tuşları: A = servo açık, B = servo kilit, C = mesafeyi yaz. Sunumda false yapın.
const bool TEST_TUSLARI = true;
// Reset döngüsü (Brownout) yaşıyorsanız true yapın: güç dalgalanmasında kart resetlenmez.
const bool BROWNOUT_KAPAT = false;

// Tuş takımı şeridi ters sırada bağlı olduğu için pin sırası ters çevrildi.
byte satirPin[4] = {32, 25, 26, 27};
byte sutunPin[4] = {16, 17, 18, 19};
char tuslar[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// ---------- NESNELER ----------
Keypad keypad = Keypad(makeKeymap(tuslar), satirPin, sutunPin, 4, 4);
Servo servo;
WebServer server(80);
DNSServer dns;

enum Durum { BOS, KILITLI, ACIK };
Durum durum = BOS;
String pin = "";
String girilen = "";
unsigned long sonTusMs = 0, kilitCezaBitis = 0, sonOlcumMs = 0;
int yanlisSayisi = 0, bosSayac = 0;
float sonMesafe = -1;
int servoKonum = -1;   // bilinmiyor

// ---------- WEB PANELI ----------
const char SAYFA[] PROGMEM = R"rawliteral(<!DOCTYPE html><html lang="tr"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>NodeDrop</title><style>
:root{--bg:#0b0f17;--card:#121826;--bd:#1f2937;--tx:#e5e7eb;--mu:#8b95a7;--ac:#22c55e}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,"Segoe UI",Roboto,sans-serif;background:radial-gradient(1000px 500px at 50% -10%,rgba(34,197,94,.12),transparent 60%),var(--bg);color:var(--tx);min-height:100vh;display:flex;flex-direction:column;align-items:center;justify-content:center;padding:20px 16px}
header{width:100%;max-width:440px;display:flex;justify-content:space-between;align-items:center;margin-bottom:18px;font-weight:700;font-size:18px}
.pill{font-size:12px;color:var(--mu);border:1px solid var(--bd);border-radius:99px;padding:6px 10px;font-weight:400}
.card{width:100%;max-width:440px;background:var(--card);border:1px solid var(--bd);border-radius:20px;padding:26px 22px;text-align:center;box-shadow:0 20px 60px rgba(0,0,0,.45)}
h1{font-size:20px}.sub{color:var(--mu);font-size:14px;margin:6px 0 20px;line-height:1.5}
button{width:100%;border:0;cursor:pointer;padding:18px;border-radius:16px;font-size:17px;font-weight:650;color:#04130a;background:linear-gradient(135deg,#22c55e,#10b981);box-shadow:0 10px 30px rgba(34,197,94,.3)}
button:disabled{opacity:.6;cursor:progress}
.sms{display:none;margin-top:20px;text-align:left;background:#0d1320;border:1px solid rgba(34,197,94,.4);border-radius:16px;padding:14px 16px}
.sms.on{display:block}.sms small{color:var(--mu);font-size:12px}.sms p{margin-top:6px;font-size:15px;line-height:1.5}
.pin{font-size:42px;font-weight:700;letter-spacing:.3em;color:#fff;text-align:center;margin:10px 0 2px;font-variant-numeric:tabular-nums}
.note{margin-top:16px;display:none;padding:12px 14px;border-radius:12px;font-size:14px;text-align:left;border:1px solid}
.note.ok{display:block;color:#bbf7d0;background:rgba(34,197,94,.08);border-color:rgba(34,197,94,.3)}
.note.err{display:block;color:#fecaca;background:rgba(239,68,68,.08);border-color:rgba(239,68,68,.3)}
.meta{margin-top:18px;padding-top:14px;border-top:1px dashed var(--bd);display:flex;justify-content:space-between;font-size:12px;color:var(--mu)}
</style></head><body>
<header><span>NodeDrop</span><span class="pill">Yerel Ağ</span></header>
<main class="card"><h1>Kargo Teslimi</h1>
<p class="sub">Kargoyu kutuya koyun, kapağı kapatın, sonra butona basın.</p>
<button id="btn">Kargoyu Teslim Et</button>
<div class="sms" id="sms"><small>SMS · NodeDrop</small>
<p>Kargonuz teslim edildi. Tek kullanımlık şifreniz:</p><div class="pin" id="pin">----</div></div>
<div class="note" id="note"></div>
<div class="meta"><span>Durum: <b id="st">...</b></span><span>Mesafe: <b id="dist">-</b></span></div></main>
<script>
const $=id=>document.getElementById(id);
const adlar={bos:'Boş, kargo bekleniyor',kilitli:'Kilitli, şifre bekleniyor',acik:'Açık, kargo alınıyor'};
let gosterildi=false;
function not(t,m){const n=$('note');n.className='note '+t;n.textContent=m}
async function poll(){try{const r=await fetch('/status',{cache:'no-store'});const s=await r.json();
$('st').textContent=adlar[s.durum]||s.durum;$('dist').textContent=s.mesafe>0?s.mesafe.toFixed(1)+' cm':'-';
if(s.durum!=='kilitli'&&gosterildi){$('sms').classList.remove('on');gosterildi=false;not('ok','Kilit açıldı, kargo teslim alındı.')}}
catch(e){$('st').textContent='Bağlantı yok'}}
setInterval(poll,2000);poll();
$('btn').onclick=async()=>{const b=$('btn');b.disabled=true;$('note').className='note';
try{const r=await fetch('/deliver',{method:'POST'});const j=await r.json();
if(j.ok){$('pin').textContent=j.pin;$('sms').classList.add('on');gosterildi=true;not('ok','Şifre cihaza iletildi, kilit aktif.')}
else not('err',j.hata)}catch(e){not('err','Cihaza ulaşılamadı. NodeDrop ağına bağlı mısınız?')}
finally{b.disabled=false;poll()}};
</script></body></html>)rawliteral";

// ---------- YARDIMCI ----------
// tone() kullanılmaz: ESP32'de servo ile aynı PWM zamanlayıcısını bozuyor.
// Buzzer yazılımla sürülür (aktif ya da pasif buzzerda çalışır).
void bip(int frekans, int ms) {
  unsigned long yarim = 500000UL / frekans;      // mikrosaniye
  unsigned long dongu = (unsigned long)ms * 1000UL / (2 * yarim);
  for (unsigned long i = 0; i < dongu; i++) {
    digitalWrite(BUZZER_PIN, HIGH); delayMicroseconds(yarim);
    digitalWrite(BUZZER_PIN, LOW);  delayMicroseconds(yarim);
  }
  delay(20);
}

void ledler() {
  digitalWrite(LED_KIRMIZI, durum == KILITLI);
  digitalWrite(LED_YESIL, durum != KILITLI);
}

// Servoyu yavaşça hedef açıya götürür. Güç dalgalanmasını azaltmak için
// her hareketten önce ve sonra kısa bekleme vardır; iş bitince sinyal kesilir.
void servoGit(int hedef) {
  Serial.printf("Servo -> %d us\n", hedef);
  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2500);
  delay(100);
  if (servoKonum < 0) {
    // konum bilinmiyor: karşı uçtan başla (ani sıçrama olmasın)
    servoKonum = (hedef == SERVO_ACIK) ? SERVO_KILIT : SERVO_ACIK;
    servo.writeMicroseconds(servoKonum);
    delay(300);
  }
  while (servoKonum != hedef) {
    int fark = hedef - servoKonum;
    int adim = (abs(fark) < 10) ? abs(fark) : 10;
    servoKonum += (fark > 0) ? adim : -adim;
    servo.writeMicroseconds(servoKonum);
    delay(11);
  }
  delay(400);
  servo.detach();   // titremeyi ve gereksiz akimi onler
}

float mesafeOlc() {
  float olcum[5]; int n = 0;
  for (int i = 0; i < 5; i++) {
    digitalWrite(TRIG_PIN, LOW); delayMicroseconds(3);
    digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long sure = pulseIn(ECHO_PIN, HIGH, 30000);
    if (sure > 0) olcum[n++] = sure * 0.0343f / 2.0f;
    delay(15);
  }
  if (n == 0) return -1;
  for (int i = 0; i < n - 1; i++)
    for (int j = i + 1; j < n; j++)
      if (olcum[j] < olcum[i]) { float t = olcum[i]; olcum[i] = olcum[j]; olcum[j] = t; }
  return olcum[n / 2];   // ortanca deger
}

bool kargoVar(float d) { return d > 0 && d < ESIK_CM; }

void pinUret() {
  do {
    char b[5];
    snprintf(b, sizeof(b), "%04u", (unsigned)(esp_random() % 10000));
    pin = String(b);
  } while (pin == MASTER_PIN);
}

void kilitle() {
  servoGit(SERVO_KILIT);
  durum = KILITLI;
  girilen = ""; yanlisSayisi = 0;
  ledler();
  bip(2000, 80); bip(2000, 80); bip(2000, 80);
}

void kilidiAc() {
  servoGit(SERVO_ACIK);
  durum = ACIK;
  pin = ""; girilen = ""; bosSayac = 0;
  ledler();
  bip(1800, 120); bip(2400, 200);
}

// ---------- WEB ----------
void jsonGonder(int kod, const String& s) { server.send(kod, "application/json", s); }

void teslimEt() {
  if (durum == KILITLI) { jsonGonder(409, "{\"ok\":false,\"hata\":\"Kutu zaten kilitli.\"}"); return; }
  if (durum == ACIK)    { jsonGonder(409, "{\"ok\":false,\"hata\":\"Önceki kargo henüz alınmadı.\"}"); return; }
  float d = mesafeOlc();
  if (!kargoVar(d)) { jsonGonder(409, "{\"ok\":false,\"hata\":\"Kargo algılanmadı. Kargoyu koyup kapağı kapatın.\"}"); return; }
  pinUret();
  String cevap = "{\"ok\":true,\"pin\":\"" + pin + "\"}";
  jsonGonder(200, cevap);   // önce yanıt, sonra kilitle (servo ~2 sn sürer)
  kilitle();
}

void durumGonder() {
  const char* ad = (durum == BOS) ? "bos" : (durum == KILITLI) ? "kilitli" : "acik";
  jsonGonder(200, String("{\"durum\":\"") + ad + "\",\"mesafe\":" + String(sonMesafe, 1) + "}");
}

// ---------- TUS TAKIMI ----------
void girisKontrol() {
  if (girilen == MASTER_PIN) { kilidiAc(); return; }                 // yönetici: her durumda açar
  if (durum == KILITLI && girilen == pin) { kilidiAc(); return; }
  girilen = "";
  yanlisSayisi++;
  bip(300, 600);
  if (yanlisSayisi >= 3) { kilitCezaBitis = millis() + 30000; yanlisSayisi = 0; }
}

void tusIsle(char k) {
  if (TEST_TUSLARI) {
    if (k == 'A') { servoGit(SERVO_ACIK); return; }
    if (k == 'B') { servoGit(SERVO_KILIT); return; }
    if (k == 'C') { Serial.printf("Mesafe: %.1f cm (esik %.1f)\n", mesafeOlc(), ESIK_CM); return; }
  }
  if (millis() < kilitCezaBitis) { bip(300, 200); return; }
  sonTusMs = millis();
  if (k == '*') { girilen = ""; bip(1500, 60); return; }
  if (k >= '0' && k <= '9') {
    girilen += k;
    bip(2000, 60);
    if (girilen.length() == 4) girisKontrol();
  }
}

// ---------- KURULUM / DONGU ----------
void setup() {
  if (BROWNOUT_KAPAT) WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  ESP32PWM::allocateTimer(0);   // servo PWM zamanlayıcısını ayır
  Serial.begin(115200);
  pinMode(LED_KIRMIZI, OUTPUT); pinMode(LED_YESIL, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  durum = BOS;
  ledler();

  // Açılışta servo OYNATILMAZ (ani akım çekişi olmasın).
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AG_ADI, AG_SIFRE, 1, 0, 4);   // kanal 1, görünür, en fazla 4 cihaz
  WiFi.setTxPower(WIFI_POWER_15dBm);        // Wi-Fi başladıktan sonra ayarlanır
  IPAddress ip = WiFi.softAPIP();
  dns.start(53, "*", ip);   // captive portal: bağlanınca panel kendiliğinden açılır

  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html; charset=utf-8", SAYFA); });
  server.on("/deliver", HTTP_POST, teslimEt);
  server.on("/status", HTTP_GET, durumGonder);
  server.onNotFound([]() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
  });
  server.begin();

  Serial.print("Panel: http://"); Serial.println(ip);
  bip(2000, 100); bip(2600, 150);
}

void loop() {
  dns.processNextRequest();
  server.handleClient();

  char k = keypad.getKey();
  if (k) { Serial.printf("Tus: %c\n", k); tusIsle(k); }
  if (girilen.length() > 0 && millis() - sonTusMs > 10000) girilen = "";   // yarim giris zaman asimi

  if (millis() - sonOlcumMs > 500) {
    sonOlcumMs = millis();
    sonMesafe = mesafeOlc();
    if (durum == ACIK) {
      // kargo 3 sn boyunca yoksa kutu bos sayilir (mandal acik kalir, yeni kurye serbestce kullanir)
      if (!kargoVar(sonMesafe)) bosSayac++; else bosSayac = 0;
      if (bosSayac >= 6) { durum = BOS; ledler(); }
    }
  }
}
