#include <HardwareSerial.h>

// ───────────── PINS ─────────────
#define GPS_RX     34
#define GPS_TX     12
#define GPS_POWER  4
#define MODEM_RX   13
#define MODEM_TX   25

// ───────────── NETWORK ─────────────
#define APN "www"

// ───────────── SUPABASE ─────────────
#define SUPABASE_HOST "mvlwmqtwecbzgdcomxxd.supabase.co"
#define SUPABASE_KEY  "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im12bHdtcXR3ZWNiemdkY29teHhkIiwicm9sZSI6ImFub24iLCJpYXQiOjE3OTE1NTc4NzEsImV4cCI6MjEwNzEzMzg3MX0.m_Oo3DiqCa1eaA722OWgrMqdw_8W6HpyPhHjQPsL64s"   // anon/public key ONLY
#define DEVICE_ID     "tbeam_001"

// ───────────── OBJECTS ─────────────
HardwareSerial GPS(1);
HardwareSerial modem(2);

// ───────────── GPS DATA ─────────────
double gpsLat = 0, gpsLng = 0;
float  gpsSpeed = 0, gpsHeading = 0;
int    gpsSats = 0;
bool   gpsFix = false;

// ───────────── TIMING / STATE ─────────────
unsigned long lastSend = 0, lastPrint = 0;
const unsigned long SEND_INTERVAL = 15000;
int failCount = 0;
String lastReply = "";

// ───────────── HELPERS ─────────────
double nmeaToDecimal(String val, String dir) {
  if (val.length() == 0) return 0;
  double raw = val.toDouble();
  int deg = (int)(raw / 100);
  double dec = deg + (raw - deg * 100) / 60.0;
  if (dir == "S" || dir == "W") dec = -dec;
  return dec;
}

void splitFields(const String &s, String *f, int maxFields) {
  int fi = 0, start = 0;
  for (int i = 0; i <= (int)s.length() && fi < maxFields; i++) {
    if (i == (int)s.length() || s[i] == ',') {
      f[fi++] = s.substring(start, i);
      start = i + 1;
    }
  }
}

void parseNMEA(const String &s) {
  if (s.startsWith("$GNGGA") || s.startsWith("$GPGGA")) {
    String f[15];
    splitFields(s, f, 15);
    int fix = f[6].toInt();
    gpsSats = f[7].toInt();
    if (fix > 0 && gpsSats >= 4 && f[2].length() > 0) {
      gpsFix = true;
      gpsLat = nmeaToDecimal(f[2], f[3]);
      gpsLng = nmeaToDecimal(f[4], f[5]);
    } else {
      gpsFix = false;
    }
  }
  if (s.startsWith("$GNRMC") || s.startsWith("$GPRMC")) {
    String f[15];
    splitFields(s, f, 15);
    if (f[2] == "A") {
      gpsSpeed = f[7].toFloat() * 1.852;
      gpsHeading = f[8].toFloat();
    }
  }
}

String sendAT(String cmd, int waitMs = 1000) {
  while (modem.available()) modem.read();
  modem.println(cmd);
  delay(waitMs);
  String r = "";
  while (modem.available()) r += (char)modem.read();
  Serial.println(">>> " + cmd);
  Serial.println("<<< " + r);
  return r;
}

bool waitFor(const char *target, unsigned long timeoutMs) {
  lastReply = "";
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    while (modem.available()) {
      char c = modem.read();
      Serial.write(c);
      lastReply += c;
    }
    if (lastReply.indexOf(target) >= 0) return true;
    if (lastReply.indexOf("ERROR") >= 0) return false;
    delay(10);
  }
  return false;
}

// ───────────── NETWORK ─────────────
void configureSSL() {
  sendAT("AT+CSSLCFG=\"sslversion\",0,4", 500);
  sendAT("AT+CSSLCFG=\"authmode\",0,0", 500);
  sendAT("AT+CSSLCFG=\"ignorertctime\",0,1", 500);
  sendAT("AT+CSSLCFG=\"enableSNI\",0,1", 500);
}

bool activateData() {
  sendAT("AT+CGDCONT=1,\"IP\",\"" + String(APN) + "\"", 1000);
  sendAT("AT+CGACT=1,1", 4000);
  String ip = sendAT("AT+CGPADDR=1", 1500);
  if (ip.indexOf("+CGPADDR:") < 0 || ip.indexOf("0.0.0.0") >= 0) return false;
  return true;
}

bool connectNetwork() {
  Serial.println("\n=== CONNECTING TO VI 4G ===");
  sendAT("AT", 500);
  sendAT("ATE0", 500);

  if (sendAT("AT+CPIN?", 1500).indexOf("READY") < 0) {
    Serial.println("ERROR: SIM not ready");
    return false;
  }

  bool registered = false;
  for (int i = 0; i < 20; i++) {
    String creg = sendAT("AT+CREG?", 1500);
    if (creg.indexOf(",1") >= 0 || creg.indexOf(",5") >= 0) {
      registered = true;
      break;
    }
    delay(1500);
  }
  if (!registered) {
    Serial.println("ERROR: not registered on network");
    return false;
  }

  if (!activateData()) {
    Serial.println("ERROR: no IP address");
    return false;
  }

  configureSSL();
  Serial.println("=== NETWORK READY ===");
  return true;
}

// ───────────── SEND TO SUPABASE (HTTPS PUT upsert) ─────────────
bool sendToSupabase() {
  // Keys must match the SQL function's parameter names
  String body = "{";
  body += "\"p_id\":\"" + String(DEVICE_ID) + "\",";
  body += "\"p_lat\":" + String(gpsLat, 6) + ",";
  body += "\"p_lng\":" + String(gpsLng, 6) + ",";
  body += "\"p_speed\":" + String(gpsSpeed, 1) + ",";
  body += "\"p_heading\":" + String(gpsHeading, 1) + ",";
  body += "\"p_satellites\":" + String(gpsSats);
  body += "}";

  Serial.println("\n=== SENDING TO SUPABASE ===");
  Serial.println(body);

  sendAT("AT+HTTPTERM", 300);
  while (modem.available()) modem.read();

  modem.println("AT+HTTPINIT");
  if (!waitFor("OK", 5000)) {
    Serial.println("STEP FAILED: HTTPINIT");
    return false;
  }

  sendAT("AT+HTTPPARA=\"CID\",1", 300);
  sendAT("AT+HTTPPARA=\"SSLCFG\",0", 300);
  sendAT("AT+HTTPPARA=\"URL\",\"https://" + String(SUPABASE_HOST) +
         "/rest/v1/rpc/upsert_bus\"", 500);
  sendAT("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 300);
  sendAT("AT+HTTPPARA=\"USERDATA\",\"apikey: " + String(SUPABASE_KEY) + "\"", 500);

  while (modem.available()) modem.read();
  modem.println("AT+HTTPDATA=" + String(body.length()) + ",10000");
  if (!waitFor("DOWNLOAD", 10000)) {
    Serial.println("STEP FAILED: HTTPDATA prompt");
    sendAT("AT+HTTPTERM", 300);
    return false;
  }

  modem.print(body);
  if (!waitFor("OK", 10000)) {
    Serial.println("STEP FAILED: body upload");
    sendAT("AT+HTTPTERM", 300);
    return false;
  }

  while (modem.available()) modem.read();
  modem.println("AT+HTTPACTION=1");   // 1 = POST (4/PUT is not supported)
  if (!waitFor("+HTTPACTION:", 60000)) {
    Serial.println("STEP FAILED: no HTTPACTION reply");
    sendAT("AT+HTTPTERM", 300);
    return false;
  }

  // keep reading so the status digits (e.g. 1,204,0) arrive
  unsigned long t = millis();
  while (millis() - t < 1500) {
    while (modem.available()) {
      char c = modem.read();
      Serial.write(c);
      lastReply += c;
    }
    delay(5);
  }

  int status = 0;
  int p = lastReply.indexOf("+HTTPACTION:");
  if (p >= 0) {
    int c1 = lastReply.indexOf(',', p);
    if (c1 > 0) {
      int c2 = lastReply.indexOf(',', c1 + 1);
      if (c2 > c1) status = lastReply.substring(c1 + 1, c2).toInt();
    }
  }
  Serial.println("\nHTTP STATUS: " + String(status));

  bool ok = (status == 200 || status == 201 || status == 204);
  if (!ok) {
    Serial.println("Server message:");
    modem.println("AT+HTTPREAD=0,300");
    delay(2000);
    while (modem.available()) Serial.write(modem.read());
  }

  sendAT("AT+HTTPTERM", 300);
  Serial.println(ok ? "SUCCESS: row updated" : "FAILED: see HTTP STATUS above");
  return ok;
}

// ───────────── SETUP ─────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n===== RIDERADAR STARTING =====");

  pinMode(GPS_POWER, OUTPUT);
  digitalWrite(GPS_POWER, HIGH);
  delay(2000);

  GPS.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("GPS started");

  modem.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  Serial.println("Waiting 10s for modem...");
  delay(10000);

  if (!connectNetwork()) {
    Serial.println("Network failed, restarting...");
    delay(5000);
    ESP.restart();
  }
  Serial.println("RIDERADAR READY");
}

// ───────────── LOOP ─────────────
void loop() {
  static String nmeaLine = "";

  while (GPS.available()) {
    char c = GPS.read();
    if (c == '\n') {
      nmeaLine.trim();
      if (nmeaLine.length() > 5) parseNMEA(nmeaLine);
      nmeaLine = "";
    } else if (c != '\r') {
      if (nmeaLine.length() < 120) nmeaLine += c;
    }
  }

  if (millis() - lastPrint >= 5000) {
    lastPrint = millis();
    Serial.println("\n--- GPS --- Sats: " + String(gpsSats) +
                   " | Fix: " + String(gpsFix ? "YES" : "NO"));
    if (gpsFix) {
      Serial.println("Lat: " + String(gpsLat, 6) + "  Lng: " + String(gpsLng, 6) +
                     "  Spd: " + String(gpsSpeed, 1) + " km/h");
    }
  }

  if (millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();

    if (!gpsFix) {
      Serial.println("No GPS fix yet, skipping upload");
      return;
    }

    if (sendToSupabase()) {
      failCount = 0;
    } else {
      failCount++;
      Serial.println("Fail count: " + String(failCount));
      if (failCount >= 3) {
        Serial.println("Re-activating data connection...");
        sendAT("AT+CGACT=0,1", 1500);
        if (!activateData()) {
          Serial.println("Reconnect failed, restarting");
          delay(2000);
          ESP.restart();
        }
        configureSSL();
        failCount = 0;
      }
    }
  }
}