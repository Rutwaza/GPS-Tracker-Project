/*
  ESP32 GPS Tracker -> Firebase (devices + persistent history)
  - Sends JSON snapshot to: https://<FIREBASE_HOST>/devices/<TRACKER_NAME>.json?auth=<AUTH>  (PUT)
  - Appends persistent point to: https://<FIREBASE_HOST>/history/<TRACKER_NAME>.json?auth=<AUTH> (POST)
  - Fields: lat, lng, alt, hdop, sats, speed, time
  - Requires TinyGPSPlus library
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <TinyGPSPlus.h>

// ---------------- USER CONFIG ----------------
const char* WIFI_SSID     = "Gako_RMA_Student";
const char* WIFI_PASSWORD = "GakoHuawei@123";

const char* FIREBASE_HOST = "tracker-6c988-default-rtdb.firebaseio.com";
const char* FIREBASE_AUTH = "cCCMHSdfL6dF16rvOEzPqcFuetkm9GXi2F4TEr6U";

// Set a friendly manual device name (must be unique per device)
const char* TRACKER_NAME  = "tracker1";  // change per device

// Upload timing (ms)
const unsigned long UPLOAD_INTERVAL = 10000; // 10 seconds
// ---------------------------------------------

// GPS serial pins (change if needed)
#define GPS_RX 16   // ESP32 RX (connect to GPS TX)
#define GPS_TX 17   // ESP32 TX (connect to GPS RX)
HardwareSerial neogps(2);
TinyGPSPlus gps;

unsigned long lastUpload = 0;
unsigned long lastFixMillis = 0;

// helper: build HTTPS URL for firebase path
String firebaseUrl(const String & pathAndFile) {
  // pathAndFile should start with '/' e.g. "/devices/tracker1.json"
  String url = "https://" + String(FIREBASE_HOST) + pathAndFile + "?auth=" + String(FIREBASE_AUTH);
  return url;
}

// small helper to format hh:mm:ss from TinyGPS time
String gpsTimeString() {
  if (!gps.time.isValid()) return String("");
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
           gps.time.hour(), gps.time.minute(), gps.time.second());
  return String(buf);
}

// ensure WiFi connected (blocks until connected, with serial feedback)
void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    // after long wait, try to restart WiFi
    if (millis() - start > 20000) {
      Serial.println("\nRetrying Wi-Fi...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      start = millis();
    }
  }
  Serial.println("\nWi-Fi connected: " + WiFi.localIP().toString());
}

// send HTTP request with HTTPClient; return HTTP response code or negative error
int httpPutJson(const String & url, const String & json) {
  HTTPClient http;
  http.begin(url);                 // uses built-in WiFiClient with HTTPS support on ESP32
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

int httpPostJson(const String & url, const String & json) {
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

// sync time via NTP (helps HTTPS certificate validation)
void syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  Serial.print("Waiting for time sync");
  time_t now = time(nullptr);
  unsigned long start = millis();
  while (now < 1609459200) { // until Jan 1 2021
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    if (millis() - start > 10000) break; // don't block too long
  }
  Serial.println();
  struct tm tminfo;
  gmtime_r(&now, &tminfo);
  Serial.printf("Time: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                tminfo.tm_year + 1900, tminfo.tm_mon + 1, tminfo.tm_mday,
                tminfo.tm_hour, tminfo.tm_min, tminfo.tm_sec);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32 GPS -> Firebase (devices + history) ===");

  // start GPS serial
  neogps.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("GPS serial started");

  // connect WiFi & sync time
  ensureWiFi();
  syncTime();

  // small delay to allow GPS to warm up
  delay(500);
}

// Build JSON string for a reading (fields required)
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
  // feed GPS characters
  while (neogps.available()) {
    gps.encode(neogps.read());
  }

  // Upload timer
  if (millis() - lastUpload < UPLOAD_INTERVAL) return;
  lastUpload = millis();

  // If no valid fix, still optionally upload (here we skip until we have a fix)
  if (!gps.location.isValid()) {
    Serial.println("No valid GPS fix yet -> skipping upload");
    return;
  }

  // Build JSON payload
  String payload = buildJsonPayload();

  // Show quick status locally
  Serial.println("---- Uploading reading ----");
  Serial.println(payload);

  // Ensure WiFi (reconnect if needed)
  ensureWiFi();

  // Snapshot PUT to /devices/<TRACKER_NAME>.json
  String snapshotPath = "/devices/" + String(TRACKER_NAME) + ".json";
  String snapshotUrl = firebaseUrl(snapshotPath);
  int putCode = httpPutJson(snapshotUrl, payload);
  if (putCode <= 0) {
    Serial.println("Snapshot PUT failed, code: " + String(putCode));
  }

  // Append to history via POST to /history/<TRACKER_NAME>.json
  String historyPath = "/history/" + String(TRACKER_NAME) + ".json";
  String historyUrl = firebaseUrl(historyPath);
  int postCode = httpPostJson(historyUrl, payload);
  if (postCode <= 0) {
    Serial.println("History POST failed, code: " + String(postCode));
  }

  Serial.println("---------------------------");
}
