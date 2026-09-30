/*
  🔥 ULTRA-OPTIMIZED GPS TRACKER (UPDATED)
  ESP32 + A7670C + Firebase
  Fix: Upload ONLY when GPS location updates
*/

#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

// ================== HARDWARE PINS ==================
#define GPS_RX 16
#define GPS_TX 17
#define MODEM_RX 21
#define MODEM_TX 22

// ================== CONFIGURATION ==================
const char* APN = "internet";
const char* FIREBASE_HOST = "tracker-6c988-default-rtdb.firebaseio.com";
const char* FIREBASE_AUTH = "cCCMHSdfL6dF16rvOEzPqcFuetkm9GXi2F4TEr6U";
const char* TRACKER_NAME = "syndicate1";

const unsigned long UPLOAD_INTERVAL = 4000;

// ================== GLOBALS ==================
HardwareSerial neogps(2);
HardwareSerial SerialAT(1);
TinyGPSPlus gps;

unsigned long lastUpload = 0;

bool gsmReady = false;
int failCount = 0;

// Track last uploaded coordinate
double lastLat = 0;
double lastLng = 0;

// ================== AT COMMAND ==================
String sendAT(String cmd, int timeout = 2000)
{
  SerialAT.println(cmd);
  delay(80);

  unsigned long start = millis();
  String resp = "";

  while (millis() - start < timeout)
  {
    while (SerialAT.available())
      resp += (char)SerialAT.read();
  }

  return resp;
}

bool checkAT(String cmd, String expected, int timeout = 2000)
{
  return sendAT(cmd, timeout).indexOf(expected) >= 0;
}

// ================== GSM INIT ==================
bool initGSM()
{
  Serial.println("📡 Initializing GSM...");

  if (!checkAT("AT", "OK")) return false;
  sendAT("ATE0");

  if (!checkAT("AT+CPIN?", "READY")) return false;

  for (int i = 0; i < 10; i++)
  {
    if (checkAT("AT+CREG?", "0,1") || checkAT("AT+CREG?", "0,5"))
      break;

    delay(1500);
    if (i == 9) return false;
  }

  sendAT("AT+CGDCONT=1,\"IP\",\"" + String(APN) + "\"");
  sendAT("AT+CGATT=1");
  delay(2000);

  if (!checkAT("AT+CGACT=1,1", "OK", 6000))
    return false;

  gsmReady = true;
  Serial.println("✅ GSM READY");
  return true;
}

// ================== JSON BUILD ==================
String buildJson()
{
  String json = "{";
  json += "\"lat\":" + String(gps.location.lat(), 6);
  json += ",\"lng\":" + String(gps.location.lng(), 6);
  json += ",\"spd\":" + String(gps.speed.isValid() ? gps.speed.kmph() : 0.0, 1);
  json += ",\"hdop\":" + String(gps.hdop.isValid() ? gps.hdop.hdop() : 99.9, 1);
  json += ",\"alt\":" + String(gps.altitude.isValid() ? gps.altitude.meters() : 0.0, 1);
  json += "}";
  return json;
}

// ================== FIREBASE UPLOAD ==================
bool uploadFirebase(String json)
{
  sendAT("AT+HTTPTERM", 1000);
  if (sendAT("AT+HTTPINIT", 3000).indexOf("OK") < 0)
    return false;

  sendAT("AT+HTTPSSL=1");
  sendAT("AT+HTTPPARA=\"CID\",1");

  String url = "https://" + String(FIREBASE_HOST) +
               "/devices/" + TRACKER_NAME +
               ".json?auth=" + FIREBASE_AUTH;

  sendAT("AT+HTTPPARA=\"URL\",\"" + url + "\"");
  sendAT("AT+HTTPPARA=\"CONTENT\",\"application/json\"");

  sendAT("AT+HTTPDATA=" + String(json.length()) + ",5000");
  delay(300);

  SerialAT.print(json);
  delay(300);

  String resp = sendAT("AT+HTTPACTION=1", 10000);

  sendAT("AT+HTTPTERM", 1000);

  return resp.indexOf("200") >= 0 || resp.indexOf("201") >= 0;
}

// ================== SETUP ==================
void setup()
{
  Serial.begin(115200);
  delay(2000);

  neogps.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

  if (!initGSM())
  {
    Serial.println("❌ GSM FAILED - Restarting");
    delay(4000);
    ESP.restart();
  }
}

// ================== LOOP ==================
void loop()
{
  // Always read GPS stream
  while (neogps.available())
    gps.encode(neogps.read());

  // Upload logic
  if (millis() - lastUpload >= UPLOAD_INTERVAL && gsmReady)
  {
    lastUpload = millis();

    if (gps.location.isValid() && gps.location.isUpdated())
    {
      double lat = gps.location.lat();
      double lng = gps.location.lng();

      // Ignore ultra tiny jitter
      if (abs(lat - lastLat) < 0.00001 && abs(lng - lastLng) < 0.00001)
      {
        Serial.println("🟡 Same position skipped");
        return;
      }

      lastLat = lat;
      lastLng = lng;

      String json = buildJson();

      Serial.print("📤 Uploading: ");
      Serial.println(json);

      if (uploadFirebase(json))
      {
        Serial.println("✅ Upload success");
        failCount = 0;
      }
      else
      {
        Serial.println("❌ Upload failed");
        failCount++;

        if (failCount >= 3)
        {
          gsmReady = false;
          initGSM();
          failCount = 0;
        }
      }
    }
    else
    {
      Serial.println("⌛ Waiting new GPS update...");
    }
  }

  delay(50);
}
