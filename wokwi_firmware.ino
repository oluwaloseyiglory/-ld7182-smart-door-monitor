#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"

// Pin definitions
#define PIR_PIN   13  // PIR sensor (pushbutton in Wokwi)
#define LED_RED   2   // Alert LED - lights when human detected
#define LED_GREEN 4   // Status LED - blinks to show system is running
#define BUZZER    5   // Buzzer - beeps on human detection
#define DHT_PIN   15  // DHT22 temperature sensor
#define DHT_TYPE  DHT22

// WiFi - Wokwi virtual network
const char* WIFI_SSID     = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// ThingSpeak
const char* API_KEY = "7XZNBFJ8GYLX1O5T";

// Settings
#define WINDOW    50    // 50 readings = 5 seconds
#define THRESHOLD 0.3   // minimum mean to trigger detection

DHT dht(DHT_PIN, DHT_TYPE);

int   humanCount  = 0;
int   falseAlarms = 0;
int   totalCount  = 0;
bool  wifiOK      = false;

void sendToThingSpeak(int motion, float confidence, float temp, float humidity) {
  if (!wifiOK) return;
  HTTPClient http;
  String url = "http://api.thingspeak.com/update?api_key=";
  url += API_KEY;
  url += "&field1=" + String(motion);
  url += "&field2=" + String(confidence, 1);
  url += "&field3=" + String(temp, 1);
  url += "&field4=" + String(humidity, 1);
  http.begin(url);
  int code = http.GET();
  Serial.println("[CLOUD] ThingSpeak response: " + String(code));
  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN,   INPUT_PULLDOWN);
  pinMode(LED_RED,   OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(BUZZER,    OUTPUT);

  dht.begin();

  Serial.println("=========================================");
  Serial.println("  LD7182 Smart Door Monitor");
  Serial.println("  ESP32-S3 | PIR + DHT22 + ThingSpeak");
  Serial.println("=========================================");

  // Connect WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Connecting");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    Serial.print(".");
    digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiOK = true;
    Serial.println("\n[WiFi] Connected: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[WiFi] Failed - offline mode");
  }

  digitalWrite(LED_GREEN, HIGH);
  Serial.println("[READY] System ready. Press PIR button to simulate motion.");
}

void loop() {
  // Blink green LED to show system is running
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 2000) {
    digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
    lastBlink = millis();
  }

  if (digitalRead(PIR_PIN) == HIGH) {
    totalCount++;
    digitalWrite(LED_RED, HIGH);
    Serial.println("\n[PIR] Motion detected! Collecting 5 second window...");

    // Read temperature and humidity
    float temp     = dht.readTemperature();
    float humidity = dht.readHumidity();
    if (isnan(temp))     temp     = 22.5;
    if (isnan(humidity)) humidity = 55.0;
    Serial.printf("[ENV] Temperature: %.1f°C | Humidity: %.1f%%\n", temp, humidity);

    // Collect 50 PIR readings
    float mean       = 0;
    int   high_count = 0;

    for (int i = 0; i < WINDOW; i++) {
      float val = (float)digitalRead(PIR_PIN);
      mean += val;
      if (val > 0.5) high_count++;
      delay(100);
    }
    mean = mean / WINDOW;

    Serial.printf("[DATA] Mean: %.2f | HIGH samples: %d/50\n", mean, high_count);

    // AI Classification
    bool  isHuman    = (mean > THRESHOLD && high_count > 10);
    float confidence = isHuman
                     ? min(99.0f, mean * 100.0f)
                     : min(99.0f, (1.0f - mean) * 100.0f);

    if (isHuman) {
      humanCount++;
      Serial.printf("[AI] *** HUMAN DETECTED *** Confidence: %.1f%%\n", confidence);
      Serial.println("[ALERT] Movement at your front door!");

      // Sound buzzer
      digitalWrite(BUZZER, HIGH);
      delay(500);
      digitalWrite(BUZZER, LOW);

      // Send to ThingSpeak
      sendToThingSpeak(1, confidence, temp, humidity);

    } else {
      falseAlarms++;
      Serial.printf("[AI] False alarm. Confidence: %.1f%%\n", confidence);
      sendToThingSpeak(0, confidence, temp, humidity);
    }

    Serial.printf("[STATS] Total: %d | Human: %d | False: %d\n",
                  totalCount, humanCount, falseAlarms);

    digitalWrite(LED_RED, LOW);
    delay(2000);
  }

  delay(100);
}