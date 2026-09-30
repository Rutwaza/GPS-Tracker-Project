/*
  Author: Nelson RUTWAZA

  🔥 ULTRA-OPTIMIZED GPS TRACKER (UPDATED)
  ESP32 + A7670C + Firebase
  Blue LED  (GPIO19) = GPS FIX
  Green LED (GPIO23) = GSM READY
*/

#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

// ================== HARDWARE PINS ==================
#define GPS_RX 16
#define GPS_TX 17
#define MODEM_RX 21
#define MODEM_TX 22

#define LED_GPS 19   // Blue
#define LED_GSM 23   // Green

// ================== CONFIGURATION ==================
const char* APN = "internet";
const char* FIREBASE_HOST = "tracker-6c988-default-rtdb.firebaseio.com";
const char* FIREBASE_AUTH = "cCCMHSdfL6dF16rvOEzPqcFuetkm9GXi2F4TEr6U";
const char* TRACKER_NAME = "syndicate5";

const unsigned long UPLOAD_INTERVAL = 3000; // 3 seconds

// ================== GLOBALS ==================
HardwareSerial neogps(2);
HardwareSerial SerialAT(1);
TinyGPSPlus gps;

unsigned long lastUpload = 0;

bool gsmReady = false;
int failCount = 0;

double lastLat = 0;
double lastLng = 0;

// ================== AT COMMAND ==================
String sendAT(String cmd, int timeout = 2000)
{
  SerialAT.println(cmd);
  delay(60);

  unsigned long start = millis();
  String resp = "";

  while (millis() - start < timeout)
  {
    while (SerialAT.available())
      resp += (char)SerialAT.read();
  }

  Serial.println(resp);
  return resp;
}

bool checkAT(String cmd, String expected, int timeout = 2000)
{
  return sendAT(cmd, timeout).indexOf(expected) >= 0;
}

// ================== BATTERY ==================
float getBatteryVoltage()
{
  String resp = sendAT("AT+CBC", 2000);

  int idx = resp.indexOf("+CBC:");
  if (idx < 0) return 0;

  String v = resp.substring(idx + 5);
  v.replace("V", "");
  v.trim();

  return v.toFloat();
}

// ================== GSM INIT ==================
bool initGSM()
{
  digitalWrite(LED_GSM, LOW);

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
  delay(1500);

  if (!checkAT("AT+CGACT=1,1", "OK", 6000))
    return false;

  gsmReady = true;
  digitalWrite(LED_GSM, HIGH);
  Serial.println("GSM READY");
  return true;
}

// ================== JSON ==================
String buildJson(float bat)
{
  String json = "{";
  json += "\"lat\":" + String(gps.location.lat(), 6);
  json += ",\"lng\":" + String(gps.location.lng(), 6);
  json += ",\"spd\":" + String(gps.speed.isValid() ? gps.speed.kmph() : 0.0, 1);
  json += ",\"hdop\":" + String(gps.hdop.isValid() ? gps.hdop.hdop() : 99.9, 1);
  json += ",\"alt\":" + String(gps.altitude.isValid() ? gps.altitude.meters() : 0.0, 1);
  json += ",\"bat\":" + String(bat, 2);
  json += "}";
  return json;
}

// ================== FIREBASE ==================
bool uploadFirebase(String json)
{
  Serial.println(">>> UPLOAD START");

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
  delay(200);
  SerialAT.print(json);
  delay(200);

  String resp = sendAT("AT+HTTPACTION=1", 10000);
  sendAT("AT+HTTPTERM", 1000);

  Serial.println(">>> UPLOAD END");

  return resp.indexOf("200") >= 0 || resp.indexOf("201") >= 0;
}

// ================== SETUP ==================
void setup()
{
  Serial.begin(115200);

  pinMode(LED_GPS, OUTPUT);
  pinMode(LED_GSM, OUTPUT);
  digitalWrite(LED_GPS, LOW);
  digitalWrite(LED_GSM, LOW);

  neogps.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);

  // Startup blink (hardware test)
  digitalWrite(LED_GPS, HIGH);
  digitalWrite(LED_GSM, HIGH);
  delay(400);
  digitalWrite(LED_GPS, LOW);
  digitalWrite(LED_GSM, LOW);

  if (!initGSM())
  {
    Serial.println("GSM FAIL - RESTART");
    delay(3000);
    ESP.restart();
  }
}

// ================== LOOP ==================
void loop()
{
  while (neogps.available())
    gps.encode(neogps.read());

  // GPS LED status
  digitalWrite(LED_GPS, gps.location.isValid() ? HIGH : LOW);

  if (millis() - lastUpload >= UPLOAD_INTERVAL && gsmReady)
  {
    lastUpload = millis();

    if (gps.location.isValid() && gps.location.isUpdated())
    {
      double lat = gps.location.lat();
      double lng = gps.location.lng();

      if (abs(lat - lastLat) < 0.00001 && abs(lng - lastLng) < 0.00001)
        return;

      lastLat = lat;
      lastLng = lng;

      float bat = getBatteryVoltage();
      Serial.print("BAT = ");
      Serial.println(bat);

      String json = buildJson(bat);
      Serial.print("JSON => ");
      Serial.println(json);

      if (uploadFirebase(json))
      {
        Serial.println("UPLOAD OK");
        failCount = 0;
      }
      else
      {
        Serial.println("UPLOAD FAIL");
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
      Serial.println("WAIT GPS UPDATE...");
    }
  }

  delay(30);
}