/*
  ESP32 GPS Tracker -> Firebase (devices + persistent history)
  with LED connection indicator

  - Sends JSON snapshot to: /devices/<TRACKER_NAME>
  - Appends persistent point to: /history/<TRACKER_NAME>
  - Fields: lat, lng, alt, hdop, sats, speed, time
  - LED Behavior:
      * Blinking  = trying to connect Wi-Fi
      * Solid ON  = connected and uploading OK
      * OFF       = lost Wi-Fi connection
*/

// RELIES ON INTERNET CONNECTION FOR TIME SYNC AND FIREBASE HTTPS

#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h

// ================= USER CONFIG =================
const char* WIFI_SSID     = "nexon";
const char* WIFI_PASSWORD = "nexon0101";

const char* FIREBASE_HOST = "tracker-6c988-default-rtdb.firebaseio.com";
const char* FIREBASE_AUTH = "cCCMHSdfL6dF16rvOEzPqcFuetkm9GXi2F4TEr6U";

const char* TRACKER_NAME  = "syndicate1";  // device name in Firebase
const unsigned long UPLOAD_INTERVAL = 10000; // 10 seconds
// ===============================================

// GPS serial pins
#define GPS_RX 16
#define GPS_TX 17
HardwareSerial neogps(2);
TinyGPSPlus gps;

// LED indicator pin (built-in LED = GPIO 2)
#define LED_PIN 2

unsigned long lastUpload = 0;

// helper to build firebase URL
String firebaseUrl(const String &pathAndFile) {
  return "https://" + String(FIREBASE_HOST) + pathAndFile + "?auth=" + String(FIREBASE_AUTH);
}

// format time from GPS
String gpsTimeString() {
  if (!gps.time.isValid()) return String("");
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", gps.time.hour(), gps.time.minute(), gps.time.second());
  return String(buf);
}

// Blink LED while connecting
void blinkWhileConnecting() {
  static unsigned long lastBlink = 0;
  static bool state = false;
  if (millis() - lastBlink >= 400) {
    lastBlink = millis();
    state = !state;
    digitalWrite(LED_PIN, state ? HIGH : LOW);
  }
}

// Wi-Fi connection manager
void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);
    return;
  }

  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    blinkWhileConnecting();
    delay(200);
    Serial.print(".");
    if (millis() - start > 20000) {
      Serial.println("\nRetrying Wi-Fi...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      start = millis();
    }
  }

  Serial.println("\nWi-Fi connected: " + WiFi.localIP().toString());
  digitalWrite(LED_PIN, HIGH);  // connected
}

// HTTP helpers
int httpPutJson(const String &url, const String &json) {
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.PUT(json);
  if (code > 0) {
    Serial.printf("HTTP %d -> %s\n", code, http.getString().c_str());
  } else {
    Serial.printf("HTTP PUT error: %d\n", code);
  }
  http.end();
  return code;
}

int httpPostJson(const String &url, const String &json) {
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(json);
  if (code > 0) {
    Serial.printf("HTTP %d -> %s\n", code, http.getString().c_str());
  } else {
    Serial.printf("HTTP POST error: %d\n", code);
  }
  http.end();
  return code;
}

// Sync NTP time for HTTPS validation
void syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  Serial.print("Waiting for time sync");
  time_t now = time(nullptr);
  unsigned long start = millis();
  while (now < 1609459200) { // before Jan 1 2021 = invalid
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    if (millis() - start > 10000) break;
  }
  Serial.println();
  struct tm tminfo;
  gmtime_r(&now, &tminfo);
  Serial.printf("Time synced: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                tminfo.tm_year + 1900, tminfo.tm_mon + 1, tminfo.tm_mday,
                tminfo.tm_hour, tminfo.tm_min, tminfo.tm_sec);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32 GPS Tracker with LED Status ===");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  neogps.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("GPS serial started");

  ensureWiFi();
  syncTime();
  delay(1000);
}

// Build JSON
String buildJsonPayload() {
  String payload = "{";
  payload += "\"lat\":"   + String(gps.location.lat(), 6) + ",";
  payload += "\"lng\":"   + String(gps.location.lng(), 6) + ",";
  payload += "\"alt\":"   + String(gps.altitude.isValid() ? gps.altitude.meters() : 0.0, 2) + ",";
  payload += "\"hdop\":"  + String(gps.hdop.isValid() ? gps.hdop.hdop() : 99.99, 2) + ",";
  payload += "\"sats\":"  + String(gps.satellites.isValid() ? gps.satellites.value() : 0) + ",";
  payload += "\"speed\":" + String(gps.speed.isValid() ? gps.speed.kmph() : 0.0, 2) + ",";
  payload += "\"time\":\"" + gpsTimeString() + "\"";
  payload += "}";
  return payload;
}

void loop() {
  // Read GPS
  while (neogps.available()) {
    gps.encode(neogps.read());
  }

  // LED OFF if lost Wi-Fi
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, LOW);
    ensureWiFi();
  }

  // upload interval
  if (millis() - lastUpload < UPLOAD_INTERVAL) return;
  lastUpload = millis();

  if (!gps.location.isValid()) {
    Serial.println("No valid GPS fix yet...");
    return;
  }

  String payload = buildJsonPayload();

  Serial.println("---- Uploading ----");
  Serial.println(payload);

  String snapshotPath = "/devices/" + String(TRACKER_NAME) + ".json";
  httpPutJson(firebaseUrl(snapshotPath), payload);

  String historyPath = "/history/" + String(TRACKER_NAME) + ".json";
  httpPostJson(firebaseUrl(historyPath), payload);

  digitalWrite(LED_PIN, HIGH); // confirm success
  Serial.println("-------------------");
}
