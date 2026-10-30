#include <HardwareSerial.h>

#define GPS_RX    34
#define GPS_TX    12
#define GPS_POWER 4

HardwareSerial GPS(1);

// GPS data
float gpsLat   = 0;
float gpsLng   = 0;
float gpsSpeed = 0;
int   gpsSats  = 0;
bool  gpsFix   = false;

// Convert NMEA to decimal degrees
float nmeaToDecimal(String val, String dir) {
  if (val.length() == 0) return 0;
  float raw = val.toFloat();
  int deg   = (int)(raw / 100);
  float min = raw - (deg * 100);
  float dec = deg + (min / 60.0);
  if (dir == "S" || dir == "W") dec = -dec;
  return dec;
}

// Parse NMEA sentence
void parseNMEA(String s) {

  // Parse GGA for position and fix
  if (s.startsWith("$GNGGA") ||
      s.startsWith("$GPGGA")) {

    String f[15];
    int fi = 0;
    int start = 0;
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

  // Parse RMC for speed
  if (s.startsWith("$GNRMC") ||
      s.startsWith("$GPRMC")) {

    String f[15];
    int fi = 0;
    int start = 0;
    for (int i = 0; i <= s.length()
         && fi < 15; i++) {
      if (i == s.length() || s[i] == ',') {
        f[fi++] = s.substring(start, i);
        start = i + 1;
      }
    }
    // Convert knots to km/h
    gpsSpeed = f[7].toFloat() * 1.852;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("RideRadar GPS Ready!");

  pinMode(GPS_POWER, OUTPUT);
  digitalWrite(GPS_POWER, HIGH);
  delay(3000);

  GPS.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
}

void loop() {
  static String line = "";
  static unsigned long lastPrint = 0;

  // Read GPS
  while (GPS.available()) {
    char c = GPS.read();
    if (c == '\n') {
      line.trim();
      if (line.length() > 5) {
        parseNMEA(line);
      }
      line = "";
    } else if (c != '\r') {
      line += c;
    }
  }

  // Print every 2 seconds
  if (millis() - lastPrint > 2000) {
    lastPrint = millis();
    Serial.println("─────────────────────");
    Serial.println("Sats: " + String(gpsSats));
    Serial.println("Fix:  " +
      String(gpsFix ? "YES ✅" : "NO ❌"));
    if (gpsFix) {
      Serial.println("LAT:  " +
        String(gpsLat, 6));
      Serial.println("LNG:  " +
        String(gpsLng, 6));
      Serial.println("SPD:  " +
        String(gpsSpeed) + " km/h");
    }
  }
}
