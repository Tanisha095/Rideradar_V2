#include <TinyGPSPlus.h>  // breaks down raw gps data into latitiude, longitude, altitude
#include <BluetoothSerial.h> //esp32 speciifc, lets chip act as bluetooth device
#include <HardwareSerial.h> //lets you use the ESP32's hardware UART ports (dedicated serial communication pins), separate from the main USB serial.

//OBJECT CREATION: 
TinyGPSPlus gps; //Creates a GPS parser object, feed it raw characters; gives back clean data
HardwareSerial GPS(1); //Creates a serial port object using UART1 (the ESP32 has 3 hardware UARTs: 0, 1, 2). This is the channel your GPS module talks through.
BluetoothSerial SerialBT; //Creates a Bluetooth serial object for wireless communication.

// GPS Pins for T-Beam
#define GPS_RX 34  //pin 34 will receive data from GPS Module
#define GPS_TX 12 //pin 12 will transmit data to the GPS module

// Device name for Bluetooth
#define DEVICE_NAME "T-Beam-GPS-Tracker"

// track last time we sent data
unsigned long lastSent = 0;
unsigned long lastPrint = 0;

void setup() {
  Serial.begin(115200);  //starts the USB serial monitor at 115200 baud rate so you cna debug messages on your computer via a cable  
  
  // Pin 4 on T-beam -power up GPS module
  pinMode(4, OUTPUT); //sets pin 4 as a power control pin
  digitalWrite(4, HIGH); // sends 3.3V to turn the GPS on
  delay(2000); // waits 2 secs for Gps module to boot up before talking to it 

  // starts UART1 to communicate with GPS
  GPS.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  /* 9600 → Baud rate . Most GPS modules default to 9600.
     SERIAL_8N1 → Data format: 8 data bits, No parity, 1 stop bit (the standard).
     GPS_RX, GPS_TX → Assigns physical pins 34 and 12 to this serial port. */

  // Initialize Bluetooth
  SerialBT.begin("TbeamGPS"); // Starts Bluetooth and broadcasts the device name so phones/computers can discover it
  Serial.println("Bluetooth Started. Device name: " + String(DEVICE_NAME));
}

void loop() {
  // Read GPS data
  while (GPS.available()) {
    char c = GPS.read(); //one char at a time
    //Serial.print(c);  // Print raw NMEA to Serial (i.e. usb monitor)
    gps.encode(c); //feeds each char into TinyGPSPlus
  }

    // satellite count 
    /* isValid() → Checks if the GPS has received satellite info yet.
        If yes → prints how many satellites are currently locked (you need at least 3 for a 2D fix, 4+ for 3D).
        If no → prints "Searching..." meaning the GPS hasn't gotten a fix yet. */

 // Print status every 2 seconds (not every 100ms)
  if (millis() - lastPrint > 2000) {
    lastPrint = millis();

    Serial.println("─────────────────────────");
    Serial.print("Satellites: ");
    Serial.println(gps.satellites.isValid() ? 
      String(gps.satellites.value()) : 
      "searching...");

    Serial.print("Location fix: ");
    Serial.println(gps.location.isValid() ? "YES" : "NO");

    if (gps.location.isValid()) {
      Serial.print(gps.location.lat(), 6);
      Serial.print(",");
      Serial.println(gps.location.lng(), 6);
      //Serial.print("Speed: ");
      //Serial.print(gps.speed.kmph());
      //Serial.println(" km/h");
      //Serial.print("Altitude: ");
      //Serial.print(gps.altitude.meters());
      //Serial.println(" m");
    }
  }

  // Send via Bluetooth every 2 seconds
  if (millis() - lastSent > 2000) {
    lastSent = millis();

    if (gps.location.isValid()) {
      String data =
        "LAT:" + String(gps.location.lat(), 6) +
        ",LNG:" + String(gps.location.lng(), 6) +
        ",SAT:" + String(gps.satellites.value()) +
        ",SPD:" + String(gps.speed.kmph()) +
        ",ALT:" + String(gps.altitude.meters());

      SerialBT.println(data);
      Serial.println("Sent via BT: " + data);
    } 
    else {
      SerialBT.println("NO_FIX");
    }
  }
}