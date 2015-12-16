// Helper to do initial setup for Bluetooth module
// https://myraspberryandme.wordpress.com/2013/11/20/bluetooth-serial-communication-with-hc-05/
// https://arduino-info.wikispaces.com/BlueTooth-HC05-HC06-Modules-How-To

// Connect Bluetooth rx and tx to D7 and D6
// and key pin to D3

// After uploading/powercycling arduino disconnect Bluetooth 
// module from current (disconnect vcc or gnd) for a short 
// moment so it recognizes high on key pin and switches into
// configuration mode

#include <SoftwareSerial.h>

#define KEY 3
SoftwareSerial mySerial(7,6); // RX, TX

void setup(void){ 
  // Init serials
  Serial.begin(9600);
  mySerial.begin(38400);
  Serial.println("Waiting to settle");
  delay(2000);
  // Set key pin to high
  pinMode(KEY, OUTPUT);
  digitalWrite(KEY, HIGH);
  Serial.println("Ready to send AT commands ...");
}

void loop(void) {
  // shuffle data back and forth
  if (mySerial.available())
    Serial.write(mySerial.read());
  if (Serial.available())
    mySerial.write(Serial.read());
}



/* Mitschnitt des Konfigurierens:
Waiting to settle
Ready to send AT commands ...
ERROR:(0)
OK
+VERSION:2.0-20100601
OK
+PSWD:1234
OK
+ADDR:98d3:31:fd16c9
OK
+NAME:HC-05
OK
+ROLE:0
OK
+SNIFF:0,0,0,0
OK
OK
+NAME:Coffee
OK
ERROR:(0)
OK
+PSWD:2503
OK
OK
+ROLE:0
OK
OK

/*
 */

