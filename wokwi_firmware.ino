#include <WiFi.h>
#include <HTTPClient.h>

#define PIR_PIN 13
#define LED_PIN 2
#define WINDOW 50

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";
const char* API_KEY = "7XZNBFJ8GYLX1O5T";

int humanCount = 0;
int falseAlarms = 0;

void sendToThingSpeak(int motionType, float confidence) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = "http://api.thingspeak.com/update?api_key=";
  url += API_KEY;
  url += "&field1=" + String(motionType);
  url += "&field2=" + String(confidence);
  http.begin(url);
  int code = http.GET();
  Serial.println("ThingSpeak code: " + String(code));
  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT_PULLDOWN);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("=== LD7182 Smart Door Monitor ===");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
  } else {
    Serial.println("WiFi failed");
  }
  Serial.println("Ready. Press button to simulate motion.");
}

void loop() {
  if (digitalRead(PIR_PIN) == HIGH) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("[PIR] Motion detected!");
    float mean = 0;
    int high_count = 0;
    for (int i = 0; i < WINDOW; i++) {
      float val = (float)digitalRead(PIR_PIN);
      mean += val;
      if (val > 0.5) high_count++;
      delay(100);
    }
    mean = mean / WINDOW;
    Serial.printf("[DATA] Mean: %.2f | HIGH: %d\n", mean, high_count);
    bool isHuman = (mean > 0.3 && high_count > 10);
    float confidence = isHuman ? (mean * 100.0) : ((1.0 - mean) * 100.0);
    if (confidence > 99.0) confidence = 99.0;
    if (confidence < 50.0) confidence = 50.0;
    if (isHuman) {
      humanCount++;
      Serial.printf("[AI] HUMAN MOTION! Confidence: %.1f%%\n", confidence);
      Serial.println("[ALERT] Movement at your front door!");
      sendToThingSpeak(1, confidence);
    } else {
      falseAlarms++;
      Serial.printf("[AI] FALSE ALARM. Confidence: %.1f%%\n", confidence);
      sendToThingSpeak(0, confidence);
    }
    Serial.printf("[STATS] Human: %d | False: %d\n", humanCount, falseAlarms);
    digitalWrite(LED_PIN, LOW);
    delay(2000);
  }
  delay(100);
}