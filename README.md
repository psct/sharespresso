Sharespresso
============

is an Arduino-based RFID payment system for coffeemakers with
toptronic logic unit, as Jura Impressa S95 and others without
modifying the coffeemaker itself.
 
Based on Oliver Krohns famous Coffeemaker-Payment-System at
https://github.com/oliverk71/Coffeemaker-Payment-System

Hardware used: Arduino Uno, 16x2 LCD I2C, pn532/mfrc522 rfid card
reader (13.56MHz), HC-05 Bluetooth, male/female jumper wires
(optional: ethernet shield, buzzer, button)

The code is provided 'as is', without any guarantuee. Use at your own
risk!

The difference compared to the original in short:

- slightly different directory structure
- modfied pinout for connecting the hardware (see fritzing)
- pin 0/1, hw serial stays free for debugging/monitoring
- introduce service mode to toggle between coffeemaker and bluetooth
- switched to 13,56MHz RFID readers
- optional logging to an unix syslog host
- move payment data to eeprom (no global variables)
- be more chatty about what is going on
- add product list for Jura x7/Franke Saphira
- some small fixes in Android App
- a bunch of other small fixes and changes
- add sketch to setup Bluetooth dongle

A reference for the several tools can be found here:
https://github.com/oliverk71/Coffeemaker-Payment-System

We've used the software with two different coffemakers: x95 and
x7. The pinout we are using:
    
Jura 9-pin RS232 interface (e.g. Jura X7)

pin 1 - TX   
pin 2 - GND    
pin 3 - RX   
pin 4 - +5v (never tried this pin)
pin 5 - not used   
pin 6 - not used  
pin 7 - not used   
pin 8 - not used  
pin 9 - not used  

Jura 4-pin interface (e.g. Jura Impressa S95):   
     
(from left to right)    
pin 4 - +5V
pin 3 - RX  
pin 2 - GND  
pin 1 - TX  
