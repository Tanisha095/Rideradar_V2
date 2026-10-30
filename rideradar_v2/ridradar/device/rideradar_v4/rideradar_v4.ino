#include <HardwareSerial.h>

// ── Pins ─────────────────────────────────────────
#define GPS_RX     34
#define GPS_TX     12
#define GPS_POWER  4
#define MODEM_RX   13
#define MODEM_TX   25

// ── Network ──────────────────────────────────────
#define APN  "www"

// ── Supabase ─────────────────────────────────────
#define SUPABASE_HOST "mvlwmqtwecbzgdcomxxd.supabase.co"
#define SUPABASE_KEY  "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im12bHdtcXR3ZWNiemdkY29teHhkIiwicm9sZSI6ImFub24iLCJpYXQiOjE3OTE1NTc4NzEsImV4cCI6MjEwNzEzMzg3MX0.m_Oo3DiqCa1eaA722OWgrMqdw_8W6HpyPhHjQPsL64s"
#define DEVICE_ID     "tbeam_001"

// ── Serial ports ─────────────────────────────────
HardwareSerial GPS(1);
HardwareSerial modem(2);

// ── GPS Data ─────────────────────────────────────
float gpsLat   = 0;
float gpsLng   = 0;
float gpsSpeed = 0;
int   gpsSats  = 0;
bool  gpsFix   = false;

// ── Timing ───────────────────────────────────────
unsigned long lastSend  = 0;
unsigned long lastPrint = 0;

// ─────────────────────────────────────────────────
// NMEA to decimal degrees
// ─────────────────────────────────────────────────
float nmeaToDecimal(String val, String dir) {
  if (val.length() == 0) return 0;
  float raw = val.toFloat();
  int deg   = (int)(raw / 100);
  float min = raw - (deg * 100);
  float dec = deg + (min / 60.0);
  if (dir == "S" || dir == "W") dec = -dec;
  return dec;
}

// ─────────────────────────────────────────────────
// Parse NMEA sentence
// ─────────────────────────────────────────────────
void parseNMEA(String s) {
  if (s.startsWith("$GNGGA") ||
      s.startsWith("$GPGGA")) {
    String f[15];
    int fi = 0, start = 0;
    for (int i = 0; i <= s.length()
         && fi < 15; i++) {
      if (i == s.length() || s[i] == ',') {
        f[fi++] = s.substring(start, i);
        start = i + 1;
      }
    }
    int fix  = f[6].toInt();
    int sats = f[7].toInt();
    if (fix > 0 && sats >= 4) {
      gpsFix  = true;
      gpsSats = sats;
      gpsLat  = nmeaToDecimal(f[2], f[3]);
      gpsLng  = nmeaToDecimal(f[4], f[5]);
    } else {
      gpsFix  = false;
      gpsSats = sats;
    }
  }
  if (s.startsWith("$GNRMC") ||
      s.startsWith("$GPRMC")) {
    String f[15];
    int fi = 0, start = 0;
    for (int i = 0; i <= s.length()
         && fi < 15; i++) {
      if (i == s.length() || s[i] == ',') {
        f[fi++] = s.substring(start, i);
        start = i + 1;
      }
    }
    gpsSpeed = f[7].toFloat() * 1.852;
  }
}

// ─────────────────────────────────────────────────
// Send AT command
// ─────────────────────────────────────────────────
String sendAT(String cmd, int waitMs = 2000) {
  modem.println(cmd);
  delay(waitMs);
  String response = "";
  while (modem.available()) {
    response += (char)modem.read();
  }
  Serial.println(">>> " + cmd);
  Serial.println("<<< " + response);
  return response;
}

// ─────────────────────────────────────────────────
// Connect to Vi 4G
// ─────────────────────────────────────────────────
bool connectNetwork() {
  Serial.println("\n=== Connecting to Vi 4G ===");

  sendAT("AT", 1000);
  sendAT("ATE0", 1000);

  String cpin = sendAT("AT+CPIN?", 2000);
  if (cpin.indexOf("READY") < 0) {
    Serial.println("SIM not ready!");
    return false;
  }
  Serial.println("SIM ready");

  for (int i = 0; i < 15; i++) {
    String creg = sendAT("AT+CREG?", 2000);
    if (creg.indexOf(",1") > 0 ||
        creg.indexOf(",5") > 0) {
      Serial.println("On Vi network");
      break;
    }
    if (i == 14) {
      Serial.println("Network failed");
      return false;
    }
    Serial.println("Waiting for network...");
    delay(2000);
  }

  sendAT("AT+CGDCONT=1,\"IP\",\""
    + String(APN) + "\"", 2000);
  sendAT("AT+CGACT=1,1", 5000);

  String ip = sendAT("AT+CGPADDR=1", 3000);
  if (ip.indexOf("0.0.0.0") > 0 ||
      ip.length() < 15) {
    Serial.println("No IP!");
    return false;
  }

  Serial.println("Internet connected!");
  Serial.println("IP: " + ip);

  // Configure SSL
  Serial.println("Configuring SSL...");
  sendAT("AT+CSSLCFG=\"sslversion\",0,3",
    1000);
  sendAT("AT+CSSLCFG=\"ignorertctime\",0,1",
    1000);
  sendAT("AT+CSSLCFG=\"ctxindex\",0", 1000);
  Serial.println("SSL configured");

  return true;
}

// ─────────────────────────────────────────────────
// Send GPS data to Supabase
// ─────────────────────────────────────────────────
bool sendToSupabase() {

  String body = "{";
  body += "\"id\":\"" +
    String(DEVICE_ID) + "\",";
  body += "\"lat\":" +
    String(gpsLat, 6) + ",";
  body += "\"lng\":" +
    String(gpsLng, 6) + ",";
  body += "\"speed\":" +
    String(gpsSpeed, 1) + ",";
  body += "\"satellites\":" +
    String(gpsSats);
  body += "}";

  Serial.println("\n=== Sending to Supabase ===");
  Serial.println(body);

  // Close previous session
  sendAT("AT+HTTPTERM", 1000);
  delay(1000);

  // Init HTTP
  String init = sendAT("AT+HTTPINIT", 2000);
  if (init.indexOf("OK") < 0) {
    Serial.println("HTTP init failed");
    return false;
  }

  // Enable SSL
  sendAT("AT+HTTPPARA=\"CID\",1", 1000);
  sendAT("AT+HTTPPARA=\"SSLCFGID\",0", 1000);

  // Set URL
  sendAT("AT+HTTPPARA=\"URL\","
    "\"https://" +
    String(SUPABASE_HOST) +
    "/rest/v1/buses\"", 2000);

  // Content type
  sendAT("AT+HTTPPARA=\"CONTENT\","
    "\"application/json\"", 1000);

  // API key header
  String key = String(SUPABASE_KEY);
  sendAT("AT+HTTPPARA=\"USERDATA\","
    "\"apikey:" + key + "\"", 3000);

  // Send body
  modem.println("AT+HTTPDATA=" +
    String(body.length()) + ",10000");
  delay(2000);
  modem.print(body);
  delay(3000);

  // POST request
  String result = sendAT(
    "AT+HTTPACTION=1", 30000);
  Serial.println("HTTP Result: " + result);

  String response = sendAT(
    "AT+HTTPREAD", 5000);
  Serial.println("Response: " + response);

  sendAT("AT+HTTPTERM", 1000);

  if (result.indexOf(",201,") > 0 ||
      result.indexOf(",200,") > 0) {
    Serial.println("Supabase updated!");
    Serial.println("LAT: " +
      String(gpsLat, 6));
    Serial.println("LNG: " +
      String(gpsLng, 6));
    return true;
  }

  Serial.println("Failed: " + result);
  return false;
}

// ─────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\nRideRadar Starting...");

  pinMode(GPS_POWER, OUTPUT);
  digitalWrite(GPS_POWER, HIGH);
  delay(2000);

  GPS.begin(9600, SERIAL_8N1,
    GPS_RX, GPS_TX);
  Serial.println("GPS started");

  modem.begin(115200, SERIAL_8N1,
    MODEM_RX, MODEM_TX);
  Serial.println("Waiting 10s for modem...");
  delay(10000);

  if (!connectNetwork()) {
    Serial.println("Restarting...");
    delay(5000);
    ESP.restart();
  }
}

// ─────────────────────────────────────────────────
void loop() {
  static String nmeaLine = "";

  while (GPS.available()) {
    char c = GPS.read();
    if (c == '\n') {
      nmeaLine.trim();
      if (nmeaLine.length() > 5) {
        parseNMEA(nmeaLine);
      }
      nmeaLine = "";
    } else if (c != '\r') {
      nmeaLine += c;
    }
  }

  if (millis() - lastPrint > 5000) {
    lastPrint = millis();
    Serial.println("\n─────────────────────");
    Serial.println("Sats: " +
      String(gpsSats));
    Serial.println("Fix:  " +
      String(gpsFix ? "YES" : "NO"));
    if (gpsFix) {
      Serial.println("LAT:  " +
        String(gpsLat, 6));
      Serial.println("LNG:  " +
        String(gpsLng, 6));
      Serial.println("SPD:  " +
        String(gpsSpeed, 1) + " km/h");
    }
  }

  if (millis() - lastSend > 15000) {
    lastSend = millis();
    if (gpsFix) {
      sendToSupabase();
    } else {
      Serial.println("No GPS fix yet");
    }
  }
}