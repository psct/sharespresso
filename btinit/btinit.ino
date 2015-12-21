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
  Serial.println("Put HC-05 in config mode (hold button or put high on key while powering)");
  Serial.println("For HC-05 enable cr/lf in serial monitor, set port speed of HC-05 to 38400");
  Serial.println("For HC-06 disable cr/lf in serial monitor, set port speed of HC-06 to 9600");
  
  delay(2000);
  // Set key pin to high (set HC-05 to config mode)
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

/* Common commands:
 * HC-05:
 * AT+NAME:<Name>
 * AT+PSWD:<Pin>
 * AT+ROLE:0
 * AT+VERSION
 * HC-06 (case and timing matters!):
 * AT+NAME<Name>
 * AT+PIN<Pin>
 * AT+VERSION
 */
