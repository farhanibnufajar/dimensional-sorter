#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// PIN KONFIGURASI
#define SENSOR_PANJANG_PIN 2
#define SENSOR_TINGGI_PIN 15
#define SENSOR_IR_KONVEYOR_PIN 2
#define SENSOR_LEBAR_PIN 4

#define SERVO1_PIN 13
#define SERVO2_PIN 12
#define SERVO3_PIN 27
#define SERVO_LEBAR_PIN 26

#define LED_DETEKSI_PIN 25
#define LED_TIDAK_DETEKSI_PIN 16
#define KONVEYOR_EN_PIN 23
#define KONVEYOR_IN1_PIN 18
#define KONVEYOR_IN2_PIN 19
#define PIN_SWITCH 5

// Servo
Servo servo1, servo2, servo3, servoLebar;

// Variabel pengukuran
float kecepatan_cm_per_s = 7.5;
unsigned long startTime = 0;
unsigned long endTime = 0;
bool sedangMendeteksi = false;

// Variabel lebar
const int durasiMajuLebar = 4800;
const int durasiMundurLebar = 4100;
const int kecepatanMajuLebar = 160;
const int kecepatanMundurLebar = 60;
const float kecepatanServoLebar = 2.5;
unsigned long waktuDeteksiMulai = 0;
unsigned long totalWaktuDeteksi = 0;
bool sedangDeteksiLebar = false;
float lebarBenda = 0.0;

// Tampilan
float lastPanjang = 0;
bool sistemAktif = false;
bool lastTinggi = false;
String lastKategori = "-";

// WiFi
const char* ssid = "CEMPAKA";
const char* password = "1sampai8";
WebServer server(80);

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Monitoring Dimensi</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: 'Segoe UI', sans-serif; background: #f1f3f6; color: #333; display: flex; flex-direction: column; align-items: center; padding: 20px; }
    h1 { color: #4a90e2; margin-bottom: 10px; }
    .card { background: white; border-radius: 16px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); padding: 24px; width: 90%; max-width: 400px; margin-top: 20px; text-align: center; }
    .label { font-weight: bold; font-size: 18px; margin-top: 15px; }
    .value { font-size: 24px; color: #2d2d2d; }
    .progress { width: 100%; background: #ddd; border-radius: 12px; overflow: hidden; margin-top: 10px; }
    .progress-bar { height: 20px; background-color: #4a90e2; width: 0%; transition: width 0.3s ease-in-out; }
    .status { margin-top: 20px; padding: 10px; border-radius: 12px; background-color: #e0f7fa; color: #00796b; font-weight: bold; }
  </style>
</head>
<body>
  <h1>Monitoring Dimensi</h1>
  <div class="card">
    <div class="label">Panjang:</div>
    <div class="value" id="panjangText">- cm</div>
    <div class="progress"><div class="progress-bar" id="progressBar"></div></div>
    <div class="label">Tinggi:</div>
    <div class="value" id="tinggi">-</div>
    <div class="label">Kategori:</div>
    <div class="status" id="kategori">-</div>
    <div class="label">Lebar:</div>
    <div class="value" id="lebar">- cm</div>
  </div>
  <script>
    function updateData() {
      fetch("/data")
        .then(res => res.json())
        .then(data => {
          const panjang = parseFloat(data.panjang);
          const maxPanjang = 12.0;
          document.getElementById("panjangText").innerText = panjang.toFixed(2) + " cm";
          let persen = Math.min((panjang / maxPanjang) * 100, 100);
          document.getElementById("progressBar").style.width = persen + "%";
          document.getElementById("tinggi").innerText = data.tinggi ? "Tinggi" : "Pendek";
          document.getElementById("kategori").innerText = data.kategori;
          document.getElementById("lebar").innerText = parseFloat(data.lebar).toFixed(2) + " cm";
        })
        .catch(err => console.error(err));
    }
    setInterval(updateData, 1000);
    updateData();
  </script>
</body>
</html>
)rawliteral";

void updateLCD(String status) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Monitoring Servo");
  lcd.setCursor(0, 1);
  lcd.print("Aktif: " + status);
}

void gerakkanServoPerlahan(Servo &servo, int dari, int ke, int delayPerStep) {
  if (dari < ke) {
    for (int pos = dari; pos <= ke; pos++) {
      servo.write(pos);
      delay(delayPerStep);
    }
  } else {
    for (int pos = dari; pos >= ke; pos--) {
      servo.write(pos);
      delay(delayPerStep);
    }
  }
}


void gerakkanServo1() {
  updateLCD("Servo 1");
  Serial.println("Servo 1: 0 → 45 → 0");

  gerakkanServoPerlahan(servo1, 0, 45, 15);  // Bergerak perlahan ke 45 derajat
  delay(2000);  // Tahan di posisi 45
  gerakkanServoPerlahan(servo1, 45, 0, 15);  // Kembali perlahan ke 0 derajat
  delay(1500);  // Tahan di posisi awal
}


void gerakkanServo2() {
  updateLCD("Servo 2");
  Serial.println("Servo 2: 90 → 30 → 90");

  gerakkanServoPerlahan(servo2, 90, 30, 15);  // Bergerak dari 90 ke 30 dengan delay 15ms per langkah
  delay(1500);  // Tunggu di posisi 30
  gerakkanServoPerlahan(servo2, 30, 90, 15);  // Kembali ke posisi awal
  delay(1000);  // Tunggu setelah kembali
}


void gerakkanServo3() {
  updateLCD("Servo 3");
  Serial.println("Servo 3: 90 → 30 → 90");

  gerakkanServoPerlahan(servo3, 90, 30, 15);  // Bergerak perlahan dari 90 ke 30 derajat
  delay(2500);                                // Tahan di posisi 30
  gerakkanServoPerlahan(servo3, 30, 90, 15);  // Kembali perlahan ke 90 derajat
  delay(2000);                                // Tahan di posisi 90
}


void ukurLebarDenganServo() {
  Serial.println("Mengukur lebar benda...");
  servoLebar.write(kecepatanMajuLebar);
  unsigned long waktuMulai = millis();
  totalWaktuDeteksi = 0;
  sedangDeteksiLebar = false;

  while (millis() - waktuMulai < durasiMajuLebar) {
    int sensor = digitalRead(SENSOR_LEBAR_PIN);
    if (sensor == LOW && !sedangDeteksiLebar) {
      waktuDeteksiMulai = millis();
      sedangDeteksiLebar = true;
    } else if (sensor == HIGH && sedangDeteksiLebar) {
      totalWaktuDeteksi += millis() - waktuDeteksiMulai;
      sedangDeteksiLebar = false;
    }
  }

  if (sedangDeteksiLebar) {
    totalWaktuDeteksi += millis() - waktuDeteksiMulai;
    sedangDeteksiLebar = false;
  }

  servoLebar.write(90); delay(300);
  lebarBenda = (totalWaktuDeteksi / 1000.0) * kecepatanServoLebar;
  Serial.print("Lebar benda: "); Serial.println(lebarBenda);
  servoLebar.write(kecepatanMundurLebar);
  delay(durasiMundurLebar);
  servoLebar.write(90);
}

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleData() {
  String json = "{";
  json += "\"panjang\":" + String(lastPanjang, 2) + ",";
  json += "\"tinggi\":" + String(lastTinggi ? "true" : "false") + ",";
  json += "\"kategori\":\"" + lastKategori + "\",";
  json += "\"lebar\":" + String(lebarBenda, 2);
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  pinMode(PIN_SWITCH, INPUT_PULLUP);
  pinMode(SENSOR_PANJANG_PIN, INPUT);
  pinMode(SENSOR_TINGGI_PIN, INPUT);
  pinMode(SENSOR_IR_KONVEYOR_PIN, INPUT);
  pinMode(SENSOR_LEBAR_PIN, INPUT);
  pinMode(LED_DETEKSI_PIN, OUTPUT);
  pinMode(LED_TIDAK_DETEKSI_PIN, OUTPUT);
  pinMode(KONVEYOR_EN_PIN, OUTPUT);
  pinMode(KONVEYOR_IN1_PIN, OUTPUT);
  pinMode(KONVEYOR_IN2_PIN, OUTPUT);

  digitalWrite(LED_DETEKSI_PIN, LOW);
  digitalWrite(LED_TIDAK_DETEKSI_PIN, HIGH);

  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  servo3.attach(SERVO3_PIN);
  servoLebar.attach(SERVO_LEBAR_PIN);
  servo1.write(0); servo2.write(90); servo3.write(90); servoLebar.write(90);

  lcd.setCursor(0, 0); lcd.print("Monitoring Servo");
  lcd.setCursor(0, 1); lcd.print("Status: -");

  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nTerhubung ke WiFi!");
  Serial.print("IP Address: "); Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  jalankanKonveyor();
}

void loop() {
  static bool sedangDelay = false;
  static bool objekMasihDiDepanSensor = false;
  static unsigned long waktuBerhenti = 0;
  static bool servoSiap = true;
  static bool sedangUkurPanjang = false;
  static unsigned long waktuAwalPanjang = 0;
  static unsigned long waktuAkhirPanjang = 0;
  static float panjangTerukur = 0.0;

  // === Cek status switch ON/OFF ===
  if (digitalRead(PIN_SWITCH) == LOW) {
    sistemAktif = true;
  } else {
    sistemAktif = false;
    hentikanKonveyor();  // matikan konveyor jika OFF
    server.handleClient(); // tetap layani web
    delay(50);
    return;  // keluar dari loop
  }

  int irKonveyor = digitalRead(SENSOR_IR_KONVEYOR_PIN);

  // === 1. Saat benda baru terdeteksi ===
  if (irKonveyor == LOW && !sedangDelay && !objekMasihDiDepanSensor) {
    hentikanKonveyor();
    sedangDelay = true;
    waktuBerhenti = millis();
    objekMasihDiDepanSensor = true;
    servoSiap = false;

    // === 2. Ukur lebar dan tinggi benda ===
    ukurLebarDenganServo();
    lastTinggi = digitalRead(SENSOR_TINGGI_PIN) == LOW;

    // Klasifikasi awal tinggi/rendah saja (opsional)
    if (lastTinggi) {
      lastKategori = "Tinggi";
    } else {
      lastKategori = "Pendek";
    }
  }

  // === 3. Setelah delay selesai, jalankan kembali konveyor ===
  if (sedangDelay && (millis() - waktuBerhenti >= 7000)) {
    jalankanKonveyor();
    sedangDelay = false;
    servoSiap = true;
  }

  // === 4. Saat benda sedang diukur panjangnya ===
  if (irKonveyor == LOW && !sedangUkurPanjang && !sedangDelay) {
    waktuAwalPanjang = millis();
    sedangUkurPanjang = true;
  }

  // === 5. Saat benda sudah lewat sensor, hitung panjang dan klasifikasi ===
  if (irKonveyor == HIGH && sedangUkurPanjang) {
    waktuAkhirPanjang = millis();
    unsigned long durasi = waktuAkhirPanjang - waktuAwalPanjang;
    float kecepatan_konveyor = 5.0;
    panjangTerukur = kecepatan_konveyor * (durasi / 600.0);
    lastPanjang = panjangTerukur;

    // Klasifikasi sesuai aturan baru
    if (lastPanjang >= 2.0 && lastPanjang <= 5.0) {
      if (!lastTinggi) {
        gerakkanServo2();  // sesuai permintaan revisi: pendek -> servo 2
        lastKategori = "2–5 cm Pendek";
      } else {
        gerakkanServo3();  // tinggi -> servo 1
        lastKategori = "2–5 cm Tinggi";
      }
    } else if (lastPanjang > 5.0 && lastPanjang <= 8.0) {
      if (!lastTinggi) {
        gerakkanServo1();  // pendek -> servo 1
        lastKategori = "5.1–8 cm Pendek";
      } else {
        lastKategori = "5.1–8 cm Tinggi (Lanjut)";
        // Tidak ada gerakan servo
      }
    } else {
      lastKategori = "Tidak cocok";
    }

    Serial.print("Panjang benda terukur: "); Serial.print(lastPanjang); Serial.println(" cm");
    Serial.print("Lebar benda terukur: "); Serial.println(lebarBenda);
    sedangUkurPanjang = false;
  }

  // === 6. Reset status saat benda sudah tidak terdeteksi ===
  if (irKonveyor == HIGH && !sedangDelay) {
    objekMasihDiDepanSensor = false;
  }

  server.handleClient();
  delay(50);
}


void jalankanKonveyor() {
  digitalWrite(KONVEYOR_IN1_PIN, HIGH);
  digitalWrite(KONVEYOR_IN2_PIN, LOW);
  analogWrite(KONVEYOR_EN_PIN, 150);
}

void hentikanKonveyor() {
  digitalWrite(KONVEYOR_IN1_PIN, LOW);
  digitalWrite(KONVEYOR_IN2_PIN, LOW);
  analogWrite(KONVEYOR_EN_PIN, 0);
}