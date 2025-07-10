#include <Servo.h>
#include "DHT.h"
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

// --- PIN SETUP ---
#define DHTPIN 4
#define DHTTYPE DHT22
#define FAN_ENA 5
#define FAN_IN1 7
#define FAN_IN2 8
#define SERVO_PIN 9
#define POT_PIN A0
#define RELAY_PIN 10

// --- OBJEK ---
DHT dht(DHTPIN, DHTTYPE);
Servo ventilasi;
hd44780_I2Cexp lcd;

// --- VARIABEL GLOBAL ---
bool lampuNyala = false;
unsigned long previousMillis = 0;
const long interval = 3000;
int lcdPage = 0;

// --- Struct Output ---
struct Output {
  float kipas_pwm;
  float sudut_servo;
};

// Fungsi keanggotaan linear naik
float fuzzyUp(float x, float a, float b) {
  return constrain((x - a) / (b - a), 0, 1);
}

// Fungsi keanggotaan linear turun
float fuzzyDown(float x, float a, float b) {
  return constrain((b - x) / (b - a), 0, 1);
}

// Fungsi keanggotaan segitiga
float fuzzyTri(float x, float a, float b, float c) {
  if (x <= a || x >= c) return 0;
  else if (x == b) return 1;
  else if (x < b) return (x - a) / (b - a);
  else return (c - x) / (c - b);
}

// --- FIS Sugeno (36 aturan) ---
Output sugenoFIS(float suhu, float kelembapan, int usia) {
  // Fuzzifikasi
  float dingin = fuzzyDown(suhu, 25, 30);
  float normal = fuzzyUp(suhu, 28, 31) * fuzzyDown(suhu, 28, 33);
  float panas = fuzzyUp(suhu, 31, 36);

  float kering = fuzzyDown(kelembapan, 50, 60);
  float ideal = fuzzyUp(kelembapan, 50, 60) * fuzzyDown(kelembapan, 60, 70);
  float basah = fuzzyUp(kelembapan, 65, 75);

  float bayi = fuzzyDown(usia, 0, 7);
  float muda = fuzzyTri(usia, 7, 10, 14);
  float remaja = fuzzyTri(usia, 14, 18, 23);
  float tua = fuzzyUp(usia, 24, 30);

  // Aturan (36 aturan, contoh disederhanakan)
  struct Rule {
    float alpha;
    float pwm;
    float servo;
  };

  Rule rules[36];
  int i = 0;
  float suhuMF[3] = {dingin, normal, panas};
  float humMF[3] = {kering, ideal, basah};
  float usiaMF[4] = {bayi, muda, remaja, tua};
  float pwmTable[4][3][3] = {
    // bayi
    {
      {50, 80, 100},  // dingin
      {60, 100, 150}, // normal
      {80, 150, 200}  // panas
    },
    // muda
    {
      {30, 70, 100},
      {60, 120, 170},
      {100, 170, 220}
    },
    // remaja
    {
      {20, 50, 100},
      {50, 100, 180},
      {100, 180, 240}
    },
    // tua
    {
      {10, 50, 90},
      {40, 100, 180},
      {100, 180, 255}
    }
  };
  float servoTable[4][3][3] = {
    // bayi
    {
      {10, 20, 30},
      {20, 40, 60},
      {30, 60, 90}
    },
    // muda
    {
      {10, 20, 30},
      {30, 45, 60},
      {40, 60, 90}
    },
    // remaja
    {
      {10, 20, 30},
      {30, 45, 60},
      {50, 70, 90}
    },
    // tua
    {
      {10, 20, 30},
      {30, 50, 70},
      {60, 75, 90}
    }
  };

  for (int u = 0; u < 4; u++) {
    for (int s = 0; s < 3; s++) {
      for (int h = 0; h < 3; h++) {
        float alpha = min(min(suhuMF[s], humMF[h]), usiaMF[u]);
        rules[i++] = { alpha, pwmTable[u][s][h], servoTable[u][s][h] };
      }
    }
  }

  float sumAlpha = 0, sumPWM = 0, sumServo = 0;
  for (int j = 0; j < 36; j++) {
    sumAlpha += rules[j].alpha;
    sumPWM += rules[j].alpha * rules[j].pwm;
    sumServo += rules[j].alpha * rules[j].servo;
  }

  Output result;
  result.kipas_pwm = sumPWM / (sumAlpha + 0.0001);
  result.sudut_servo = sumServo / (sumAlpha + 0.0001);
  return result;
}

// Kontrol relay
void setLampu(bool nyala) {
  digitalWrite(RELAY_PIN, nyala ? LOW : HIGH);
}

void setup() {
  Serial.begin(9600);
  dht.begin();
  ventilasi.attach(SERVO_PIN);

  pinMode(FAN_ENA, OUTPUT);
  pinMode(FAN_IN1, OUTPUT);
  pinMode(FAN_IN2, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  lcd.begin(16, 2);
  lcd.clear();

  digitalWrite(FAN_IN1, HIGH);
  digitalWrite(FAN_IN2, LOW);
  setLampu(false);
}

void loop() {
  float suhu = dht.readTemperature();
  float kelembapan = dht.readHumidity();
  int potValue = analogRead(POT_PIN);
  int usia_ayam = map(potValue, 0, 1023, 1, 30);

  // Target suhu berdasarkan usia
  float target_suhu = (usia_ayam <= 7) ? 34 :
                      (usia_ayam <= 14) ? 30 :
                      (usia_ayam <= 23) ? 28 : 26.6;

  // Kontrol lampu dengan histeresis
  const float hysteresis = 0.5;
  if (suhu < target_suhu - hysteresis) lampuNyala = true;
  else if (suhu > target_suhu + hysteresis) lampuNyala = false;
  setLampu(lampuNyala);

  // FIS Sugeno
  Output out = sugenoFIS(suhu, kelembapan, usia_ayam);

  analogWrite(FAN_ENA, (int)out.kipas_pwm);
  ventilasi.write((int)out.sudut_servo);

  // --- SERIAL MONITOR ---
  Serial.print("Usia Ayam: "); Serial.print(usia_ayam); Serial.print(" hari | ");
  Serial.print("Target Suhu: "); Serial.print(target_suhu); Serial.print(" °C | ");
  Serial.print("Suhu: "); Serial.print(suhu); Serial.print(" °C | ");
  Serial.print("Kelembapan: "); Serial.println(kelembapan);

  Serial.print("Kipas PWM: "); Serial.print(out.kipas_pwm); Serial.println(" / 255");
  Serial.print("Sudut Ventilasi: "); Serial.print(out.sudut_servo); Serial.println("°");
  Serial.print("Lampu: "); Serial.println(lampuNyala ? "ON" : "OFF");
  Serial.println("--------------------------");

  // --- LCD ---
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    lcdPage = (lcdPage + 1) % 2;
    lcd.clear();
  }

  if (lcdPage == 0) {
    lcd.setCursor(0, 0);
    lcd.print("Usia: "); lcd.print(usia_ayam); lcd.print(" hr");
    lcd.setCursor(0, 1);
    lcd.print("T:"); lcd.print(suhu, 1); lcd.print((char)223); lcd.print("C ");
    lcd.print("H:"); lcd.print(kelembapan, 0); lcd.print("%");
  } else {
    lcd.setCursor(0, 0);
    lcd.print("Kipas: "); lcd.print(map(out.kipas_pwm, 0, 255, 0, 100)); lcd.print("%");
    lcd.setCursor(0, 1);
    lcd.print("Lampu: "); lcd.print(lampuNyala ? "ON " : "OFF");
    lcd.print(" S:"); lcd.print((int)out.sudut_servo);
  }

  delay(500);
}
