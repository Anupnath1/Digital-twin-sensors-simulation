#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <DHT.h>

// ─── Pin Definitions ──────────────────────────────────────────
#define DHTPIN     4
#define DHT_TYPE   DHT22
#define SOIL_PIN   34
#define LDR_PIN    35
#define LED_STATUS 2

// ─── WiFi & Firebase ──────────────────────────────────────────
#define WIFI_SSID     "Wokwi-GUEST"
#define WIFI_PASSWORD ""

#define FIREBASE_HOST "mini-project-a9d3b-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "IGkj6BOba3Bhno8uo1eu8q7SsGYDO937WIMduYNZ"

// ─── Objects ──────────────────────────────────────────────────
DHT          dht(DHTPIN, DHT_TYPE);
FirebaseData fbdo;
FirebaseConfig fbConfig;
FirebaseAuth   fbAuth;

float soilMoisture = -1.0f;

// ─── Simulated Clock ──────────────────────────────────────────
unsigned long simStartMillis = 0;
// Seed sim clock from real compile-time hour so day/night match reality
static const int _H = ((__TIME__[0]-'0')*10 + (__TIME__[1]-'0'));
static const int _M = ((__TIME__[3]-'0')*10 + (__TIME__[4]-'0'));
float simStartHour = _H + _M / 60.0f;

float getSimHour() {
  unsigned long elapsed = (millis() - simStartMillis) / 1000UL;
  return fmod(simStartHour + elapsed / 60.0f, 24.0f);
}

// ─── Get Real Date (fixed date for Wokwi) ────────────────────
String getDate() {
  // Wokwi has no RTC, so we use a fixed start date
  // Date advances every 24 simulated hours (24 real minutes)
  unsigned long totalMins = (millis() - simStartMillis) / 1000UL;
  int dayOffset = totalMins / 1440; // 1440 sec = 1 full sim day

  // Base date: today
  int day   = 1 + dayOffset;
  int month = 5;
  int year  = 2025;

  // Simple month overflow handling
  int daysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  while (day > daysInMonth[month - 1]) {
    day -= daysInMonth[month - 1];
    month++;
    if (month > 12) { month = 1; year++; }
  }

  char buf[12];
  sprintf(buf, "%04d-%02d-%02d", year, month, day);
  return String(buf);
}

String getTimeString(int h, int m) {
  char buf[6];
  sprintf(buf, "%02d:%02d", h, m);
  return String(buf);
}

// ─── Light Intensity (Lux) ────────────────────────────────────
// Night  : 0–5 lux
// Dawn   : 0 → 15,000 lux  (5:30–7:00)
// Day    : peaks 80,000 lux at noon
// Dusk   : 15,000 → 0 lux  (18:00–19:30)
float simulateLux(float hour) {
  if (hour < 5.5f || hour > 19.5f) {
    return random(0, 6); // night moonlight
  }
  if (hour < 7.0f) {
    float t = (hour - 5.5f) / 1.5f;
    return constrain(t * t * 15000.0f + random(-100, 100), 0, 15000);
  }
  if (hour > 18.0f) {
    float t = (19.5f - hour) / 1.5f;
    return constrain(t * t * 15000.0f + random(-100, 100), 0, 15000);
  }
  float t    = (hour - 7.0f) / 11.0f;
  float lux  = 80000.0f * sin(t * PI);
  float noise = (random(-500, 500) / 10000.0f) * lux;
  return constrain(lux + noise, 0, 100000);
}

// ─── Temperature ──────────────────────────────────────────────
// Min 22°C at 5 AM, Max 35°C at 2 PM (tropical)
float simulateTemp(float hour) {
  // Peaks 35°C at 2 PM, dips to 18°C at 4 AM
  float t    = cos((hour - 14.0f) * PI / 12.0f); // -1..+1
  float temp = 26.5f + 8.5f * t;  // midpoint 26.5, swing ±8.5 → 18–35°C
  temp += random(-3, 4) / 10.0f;
  return constrain(temp, 16.0f, 42.0f);
}

// ─── Humidity ─────────────────────────────────────────────────
// High at night (80%), low at noon (40%) — inverse of temperature
float simulateHumidity(float hour) {
  float t   = cos((hour - 14.0f) * PI / 12.0f);
  float hum = 80.0f - (80.0f - 40.0f) * (1.0f - t) / 2.0f;
  hum += random(-20, 20) / 10.0f;
  return constrain(hum, 20.0f, 95.0f);
}

// ─── Soil Moisture ────────────────────────────────────────────
// Slowly drops during hot daytime, stable at night
float simulateSoilMoisture(float hour, float temp) {
  if (soilMoisture < 0.0f) soilMoisture = 65.0f; // first-run init
  // Daytime: evaporation peaks at solar noon, scales with temp
  float evapRate;
  if (hour >= 6.0f && hour <= 20.0f) {
    float t  = sin((hour - 6.0f) / 14.0f * PI); // 0→1→0 over daylight
    evapRate = 0.08f * t * (temp / 30.0f);        // up to ~0.08% per tick
  } else {
    evapRate = 0.005f;  // slow night drain
  }
  soilMoisture -= evapRate;
  soilMoisture  = constrain(soilMoisture, 10.0f, 95.0f);
  float noise   = random(-3, 4) / 10.0f;  // ±0.3% sensor noise
  return constrain(soilMoisture + noise, 10.0f, 95.0f);
}

// ─── Setup ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  pinMode(LED_STATUS, OUTPUT);
  dht.begin();
  simStartMillis = millis();

  Serial.println("\n=============================");
  Serial.println("  Smart Agri Digital Twin");
  Serial.println("=============================");

  // Connect WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    digitalWrite(LED_STATUS, HIGH);
  } else {
    Serial.println("\nWiFi failed!");
  }

  // Firebase init
  fbConfig.host = FIREBASE_HOST;
  fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&fbConfig, &fbAuth);
  Firebase.reconnectWiFi(true);

  Serial.println("Firebase ready.");
  Serial.println("Sending data...\n");
}

// ─── Loop ─────────────────────────────────────────────────────
void loop() {
  float hour = getSimHour();
  int   simH = (int)hour;
  int   simM = (int)((hour - simH) * 60);

  // Simulate all sensors
  float temp     = simulateTemp(hour);
  float humidity = simulateHumidity(hour);
  float moisture = simulateSoilMoisture(hour, temp);
  float lux      = simulateLux(hour);

  // Build date & time strings
  String dateStr     = getDate();
  String timeStr     = getTimeString(simH, simM);
  String datetimeStr = dateStr + " " + timeStr;

  // Serial output
  Serial.println("─────────────────────────────");
  Serial.println("DateTime    : " + datetimeStr);
  Serial.printf("Temperature : %.1f C\n",   temp);
  Serial.printf("Humidity    : %.1f %%\n",  humidity);
  Serial.printf("Moisture    : %.1f %%\n",  moisture);
  Serial.printf("Light       : %.0f lux\n", lux);

  // Send to Firebase — only sensor data + datetime
  if (Firebase.ready()) {
    FirebaseJson json;
    json.set("timestamp",     datetimeStr);
    json.set("soil_moisture", (float)(round(moisture * 10) / 10.0));
    json.set("temperature",   (float)(round(temp     * 10) / 10.0));
    json.set("humidity",      (float)(round(humidity * 10) / 10.0));
    json.set("light_intensity", (float)(round(lux)));

    if (Firebase.updateNode(fbdo, "/farm/current", json)) {
      Serial.println("Firebase    : OK");
      Firebase.pushJSON(fbdo, "/farm/sensors", json);
      digitalWrite(LED_STATUS, LOW);
      delay(100);
      digitalWrite(LED_STATUS, HIGH);
    } else {
      Serial.println("Firebase Err: " + fbdo.errorReason());
    }
    
  }

  delay(5000); // send every 5 seconds
}