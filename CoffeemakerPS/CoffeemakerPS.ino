
/* An Arduino-based RFID payment system for coffeemakers with toptronic logic unit, as Jura Impressa S95
 and others without modifying the coffeemaker itself. Commands may differ from one coffeemaker to another, 
 they have to be changed in the code, if necessary. Make sure not to accidently reset your coffeemaker's 
 EEPROM! Make a backup, if necessary. 
 
 Hardware used: Arduino Uno, 16x2 LCD I2C, "RDM 630" RFID reader 125 kHz, HC-05 bluetooth, buzzer, 
 male/female jumper wires, a housing.
 
 pinouts:
 Analog pins 4,5 - LCD I2C
 Digital pins 0,1 - myBT bluetooth RX, TX (hardware serial)
 Digital pins 2,3 - RFID RX, TX
 Digital pins 4,5 - myCoffeemaker RX, TX (software serial)
 Digital pin 12 - piezo buzzer
 
 Already existing cards can be used! Any 125 kHz RFID tag can be registered. Registering of new cards, 
 charging and deleting of old cards is done via the Android app, but it would be possible to use any 
 other bluetooth client software on smartphone or PC.
 
 The code is provided 'as is', without any guarantuee. Use at your own risk! */

// compile time configuration options
#define BUZZER 1
#define BT 1
#define LCD 1
#define SERLOG 1

#include <Wire.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <Adafruit_PN532.h>

LiquidCrystal_I2C lcd(0x27,16,2);
SoftwareSerial myCoffeemaker(4,5); // RX, TX
#if defined(BT)
SoftwareSerial myBT(7,6);
#endif
// Connect RFID by SPI
#define PN532_SCK  (2)
#define PN532_MOSI (3)
#define PN532_SS   (4)
#define PN532_MISO (5)
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

// product codes send by coffeemaker "?PA<x>\r\n", just <x>
char products[] = "EFABJIG";

// general variables
boolean buttonPress = false;
const int n = 20;  // max total number of cards with access (up to 200 cards max = 199! Do not exceed otherwise you will overwrite price list!)
int creditArray[n] = {      // remaining credit on card
}; 
int price;           // price variable
int priceArray[11];  // price Array contains prices for up to 10 products. 0 = PAA, 1 = PAB usw. Plus standard value for new cards
String BTstring="";  // contains what is received via bluetooth (from app or other bt client)
unsigned long time;  // timer for RFID etc
unsigned long buttonTime;  // timer for button press 
boolean override = false;  // to override payment system by the voice-control/button-press app

unsigned long RFIDcard = 0;
unsigned long RFIDcards[n] = {
}; 

union{
  byte cardByte[4];
  unsigned long cardNr;
} 
cardConvert;

union{
  byte creditByte[2];
  int creditInt;
} 
creditConvert;

void setup()
{
#if defined(SERLOG)
  Serial.begin(9600);
#endif
#if defined(LCD)
  lcd.init();
#endif
  message_print(F("CoffeemakerPS v0.8"), F("starting up"), 0);
  myCoffeemaker.begin(9600);         // start serial communication at 9600bps
#if defined(BT)
  myBT.begin(38400);
#endif
  serlog(F("Reading EEPROM data"));
  for (int i = 0; i < n; i++){  // read card numbers and referring credit from EEPROM
    cardConvert.cardByte[0] = EEPROM.read(i*6);
    cardConvert.cardByte[1] = EEPROM.read(i*6+1);
    cardConvert.cardByte[2] = EEPROM.read(i*6+2);
    cardConvert.cardByte[3] = EEPROM.read(i*6+3);
    RFIDcards[i]  = cardConvert.cardNr;  // union to put the four bytes together
    creditConvert.creditByte[0] = EEPROM.read(i*6+4);
    creditConvert.creditByte[1] = EEPROM.read(i*6+5);
    creditArray[i] = creditConvert.creditInt;
  }
  for (int i = 0; i < 11; i++){   // read price list products 1 to 10 and start value for new cards
    creditConvert.creditByte[0] = EEPROM.read(i*2+1000);
    creditConvert.creditByte[1] = EEPROM.read(i*2+1001);
    priceArray[i] = creditConvert.creditInt;
  }
  // initialized rfid lib
  serlog(F("Initializing rfid reader"));
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    serlog(F("Didn't find PN53x board"));
    while (1); // halt
  }
  // configure board to read RFID tags and cards
  nfc.SAMConfig();
  message_print(F("Ready to brew"), F(""), 2000);
}

void loop()
{
  serlog(F("Entering loop")); 
  // Check if there is a bluetooth connection and command
  BTstring = "";
  //  buttonPress = false;
  // handle serial and bluetooth input
#if defined(BT)
  myBT.listen();
  while( myBT.available()) {
    BTstring +=String(char(myBT.read()));
    delay(7);
  }
#endif
  while( Serial.available() ){  
    BTstring += String(char(Serial.read()));
    delay(7);  
  }
  BTstring.trim();
  
  if (BTstring.length() > 0){
    // BT: Start registering new cards until 10 s no valid, unregistered card
    if( BTstring == "RRR" ){          
      time = millis();
      beep(1);
      message_print(F("Registering"),F("new cards"),0);
      registernewcards();
      message_clear();
    }
    // BT: Send RFID card numbers to app    
    if(BTstring == "LLL"){  // 'L' for 'list' sends RFID card numbers to app   
      for(int i=0;i<n;i++){
#if defined(BT)
        myBT.print(print10digits(RFIDcards[i])); 
        if (i < (n-1)) myBT.write(',');  // write comma after card number if not last
#endif
      }
    }
    // BT: Delete a card and referring credit   
    if(BTstring.startsWith("DDD") == true){
      BTstring.remove(0,3); // removes "DDD" and leaves the index
      int i = BTstring.toInt();
      i--; // list picker index (app) starts at 1, while RFIDcards array starts at 0       
      EEPROM.write(i*6, 0);    // writes card number (4 bytes)
      EEPROM.write(i*6+1, 0);
      EEPROM.write(i*6+2, 0);
      EEPROM.write(i*6+3, 0);
      EEPROM.write(i*6+4, 0);  // writes credit (2 bytes)
      EEPROM.write(i*6+5, 0);
      beep(1);
      RFIDcards[i] = 0;
      message_print(print10digits(RFIDcards[i]), F("deleted!"), 2000);
    }    
    // BT: Charge a card    
    if((BTstring.startsWith("CCC") == true) ){  // && (BTstring.length() >= 7 )
      char a1 = BTstring.charAt(3);  // 3 and 4 => card list picker index (from app)
      char a2 = BTstring.charAt(4);
      char a3 = BTstring.charAt(5);  // 5 and 6 => value to charge
      char a4 = BTstring.charAt(6);    
      BTstring = String(a1)+String(a2); 
      int i = BTstring.toInt();    // index of card
      BTstring = String(a3)+String(a4);
      int j = BTstring.toInt();   // value to charge
      j *= 100;
      i--; // list picker index (app) starts at 1, while RFIDcards array starts at 0  
      creditArray[i] += j;
      creditConvert.creditInt = creditArray[i];     
      EEPROM.write(i*6+4, creditConvert.creditByte[0]);
      EEPROM.write(i*6+5, creditConvert.creditByte[1]); 
      beep(1);
      message_print(print10digits((RFIDcards[i])-j),"+"+printCredit(j),2000);
    }
    // BT: Receives (updated) price list from app.  
    if(BTstring.startsWith("CHA") == true){
      int k = 3;
      for (int i = 0; i < 11;i++){  
        String tempString = "";
        do {
          tempString += BTstring.charAt(k);
          k++;
        } 
        while (BTstring.charAt(k) != ','); 
        int j = tempString.toInt();
        priceArray[i] = j;
        creditConvert.creditInt = j;
        EEPROM.write(i*2+1000, creditConvert.creditByte[0]);
        EEPROM.write(i*2+1001, creditConvert.creditByte[1]);
        k++;
      }
      beep(1);
      message_print(F("Pricelist"), F("updated!"), 2000);
    }
    // BT: Sends price list to app. Product 1 to 10 (0-9), prices divided by commas plus standard value for new cards
    if(BTstring.startsWith("REA") == true){
      // delay(100); // testweise      
      for (int i = 0; i < 11; i++) {
#if defined(BT)
        myBT.print(int(priceArray[i]/100));
        myBT.print('.');
        if ((priceArray[i]%100) < 10){
          myBT.print('0');
        }
        myBT.print(priceArray[i]%100);
        if (i < 10) myBT.write(',');
#endif
      }
    }  

    if(BTstring == "?M3"){  
      toCoffeemaker("?M3\r\n");  // activates incasso mode (= no coffee w/o "ok" from the payment system! May be inactivated by sending "?M3" without quotation marks)
      delay (100);               // wait for answer from coffeemaker
      lcd.backlight();
      if (fromCoffeemaker() == "?ok"){
        beep(1);
        message_print(F("Inkasso mode"),F("activated!"),2000);  
      } 
      else {
        beep(2);
        message_print(F("Coffeemaker"),F("not responding!"),2000);  
      }  
    }

    if(BTstring == "?M1"){  
      toCoffeemaker("?M1\r\n");  // deactivates incasso mode (= no coffee w/o "ok" from the payment system! May be inactivated by sending "?M3" without quotation marks)
      delay (100);               // wait for answer from coffeemaker
      if (fromCoffeemaker() == "?ok"){
        beep(1);
        message_print(F("Inkasso mode"),F("deactivated!"),2000);  
      } 
      else {
        beep(2);
        message_print(F("Coffeemaker"),F("not responding!"),2000);  
      }
    }
    if(BTstring == "FA:04"){        // small cup ordered via app
      toCoffeemaker("FA:04\r\n"); 
      override = true;
    }
    if(BTstring == "FA:06"){        // large cup ordered via app
      toCoffeemaker("FA:06\r\n");  
      override = true;
    }
    if(BTstring == "FA:0C"){        // extra large cup ordered via app
      toCoffeemaker("FA:0C\r\n");  
      override = true;
    }    
  }          

  // Get key pressed on coffeemaker
  serlog(F("Reading Coffeemaker"));
  String message = fromCoffeemaker();   // gets answers from coffeemaker 
  if (message.length() > 0){
    if (message.charAt(0) == '?' && message.charAt(1) == 'P'){     // message starts with '?P' ?
      buttonPress = true;
      buttonTime = millis();
      int product = 0;
      for (int i = 0; i < 8; i++) {
        if (message.charAt(3) == products[i]) {
          product = i;
          break;
        }
      }
      if ( product != 0) {
        String productname;
          switch (product) {
            case 0: productname = F("Small cup"); break;
            case 1: productname = F("2 small cups"); break;
            case 2: productname = F("Large cup"); break;
            case 3: productname = F("2 large cups"); break;
            case 4: productname = F("Steam 2"); break;
            case 5: productname = F("Steam 1"); break;
            case 6: productname = F("Extra large cup"); break;
          }
        price = priceArray[product];
        message_print(productname, printCredit(price), 0);
      } 
      else {
        message_print(F("Error reading"), F("from coffeemaker"), 2000);
        serlog(F("Read error"));
        buttonPress = false;
      }
      // boss mode, he does not pay
      if (override == true){
        price = 0;
      }
    }
  }
  if (millis()-buttonTime > 5000){  
    buttonPress = false;
    price = 0;
    message_clear();
    serlog(F("Timeout getting keypress on machine"));     
  }
  if (buttonPress == true && override == true){
    toCoffeemaker("?ok\r\n");
    buttonPress == false;
    override == false;
  }
  // RFID Identification      
  RFIDcard = 0;  
  time = millis(); 
  do {
    RFIDcard = nfcidread();
    if (RFIDcard != 0) {
      lcd.clear();
      break; 
    }           
  } 
  while ( (millis()-time) < 60 );  

  if (RFIDcard != 0){
    int k = n;
    for(int i=0;i<n;i++){         
      if (((RFIDcard) == (RFIDcards[i])) && (RFIDcard != 0 )){
        k = i;
        if(buttonPress == true){                 // button pressed on coffeemaker?
          if ((creditArray[k] - price) > 0){     // enough credit?
            creditArray[k] -= price;
            message_print(print10digits(RFIDcards[k])+ printCredit(creditArray[k]), F(" "), 0);
            creditConvert.creditInt = creditArray[k];
            EEPROM.write(k*6+4, creditConvert.creditByte[0]);
            EEPROM.write(k*6+5, creditConvert.creditByte[1]);
            toCoffeemaker("?ok\r\n");            // prepare coffee
          } 
          else {                                 // not enough credit!
            beep(2);
            message_print(printCredit(creditArray[k]), F("Not enough credit "), 2000);  
          }
        } 
        else {                                // if no button was pressed on coffeemaker / check credit
          message_print(printCredit(creditArray[k]), F("Remaining credit"), 2000);      
        }
        i = n;      // leave loop (after card has been identified)
      }      
    }
    if (k == n){ 
      k=0; 
      beep(2);
      message_print(String(print10digits(RFIDcard)),F("card unknown!"),2000);
    }     	    
  }
}

String fromCoffeemaker(){
  String inputString = "";
  byte d0, d1, d2, d3;
  char d4 = 255;
  while (myCoffeemaker.available()){    // if data is available to read
    d0 = myCoffeemaker.read();
    delay (1); 
    d1 = myCoffeemaker.read();
    delay (1); 
    d2 = myCoffeemaker.read();
    delay (1); 
    d3 = myCoffeemaker.read();
    delay (7);
    bitWrite(d4, 0, bitRead(d0,2));
    bitWrite(d4, 1, bitRead(d0,5));
    bitWrite(d4, 2, bitRead(d1,2));
    bitWrite(d4, 3, bitRead(d1,5));
    bitWrite(d4, 4, bitRead(d2,2));
    bitWrite(d4, 5, bitRead(d2,5));
    bitWrite(d4, 6, bitRead(d3,2));
    bitWrite(d4, 7, bitRead(d3,5));
    inputString += d4;
  }
  inputString.trim();
  if ( inputString != "") {
        return(inputString);
  } 
}

void toCoffeemaker(String outputString)
{
  for (byte a = 0; a < outputString.length(); a++){
    byte d0 = 255;
    byte d1 = 255;
    byte d2 = 255;
    byte d3 = 255;
    bitWrite(d0, 2, bitRead(outputString.charAt(a),0));
    bitWrite(d0, 5, bitRead(outputString.charAt(a),1));
    bitWrite(d1, 2, bitRead(outputString.charAt(a),2));  
    bitWrite(d1, 5, bitRead(outputString.charAt(a),3));
    bitWrite(d2, 2, bitRead(outputString.charAt(a),4));
    bitWrite(d2, 5, bitRead(outputString.charAt(a),5));
    bitWrite(d3, 2, bitRead(outputString.charAt(a),6));  
    bitWrite(d3, 5, bitRead(outputString.charAt(a),7)); 
    myCoffeemaker.write(d0); 
    delay(1);
    myCoffeemaker.write(d1); 
    delay(1);
    myCoffeemaker.write(d2); 
    delay(1);
    myCoffeemaker.write(d3); 
    delay(7);
  }
}

String printCredit(int credit){
  int euro = ((credit)/100);  //  int euro = ((credit*10)/100);
  int cent = ((credit)%100);  //  int cent = ((credit*10)%100); 
  String(output);
  output = String(euro);
  output += ',';
  output += String(cent);
  if (cent < 10){
    output += '0';
  }
  output += F(" EUR");  
  return output;
}

String print10digits(unsigned long number) {
  String(tempString) = String(number);
  String(newString) = "";
  int i = 10-tempString.length();
  for (int a = 0; a < (10-tempString.length()); a++){
    newString += "0";
  }
  newString += number;
  return newString;
}

String print2digits(int number) {
  String partString;
  if (number >= 0 && number < 10) {
    partString = "0";
    partString += number;
  } 
  else partString = String(number);
  return partString;
}

void serlog(String msg) {
#if defined(SERLOG)
  Serial.println(msg);
#endif
}

void message_print(String msg1, String msg2, int wait) {
#if defined(SERLOG)
  if (msg1 != "") { Serial.print(msg1 + " "); }
  if (msg2 != "") { Serial.print(msg2); }
  if ((msg1 != "") || (msg2 != "")) { Serial.println(""); }
#endif
#if defined(LCD)
  lcd.clear();
  lcd.backlight();
  if (msg1 != "") {
    lcd.setCursor(0, 0);
    lcd.print(msg1);
  }
  if (msg2 != "") {
    lcd.setCursor(0, 1);
    lcd.print(msg2);
  }
  if (wait > 0) { 
    delay(wait);
    lcd.clear();
    lcd.noBacklight();
  }
#endif
}

void message_clear() {
#if defined(LCD)
  lcd.clear();
  lcd.noBacklight();
#endif
}

void beep(byte number){
#if defined (BUZZER)
  int duration = 200;
  switch (number) {
  case 1: // positive feedback
    tone(12,1500,duration);
    delay(duration);
    break;
  case 2: // negative feedback
    tone(12,500,duration);
    delay(duration);
    break;     
  case 3:  // action stopped (e.g. registering) double beep
    tone(12,1000,duration);
    delay(duration);
    tone(12,1500,duration);
    delay(duration);    
    break; 
  case 4:  // alarm (for whatever)
    for (int a = 0; a < 3; a++){
      for (int i = 2300; i > 600; i-=50){
        tone(12,i,20);
        delay(18);
      }     
      for (int i = 600; i < 2300; i+=50){
        tone(12,i,20);
        delay(18);
      }
    }  
  }
#endif
}

void registernewcards() {
do {
        RFIDcard = 0; 
        do {
          RFIDcard = nfcidread();
          if (RFIDcard != 0) {
            message_clear();
            break;
          }
        } 
        while ( (millis()-time) < 60 );  
        int k = 0;
        for(int i=0;i<n;i++){
          if ((RFIDcard == RFIDcards[i]) && (RFIDcard != 0) && (k == 0)){   //  && (RFIDcard>0 (((RFIDcard) == (RFIDcards[i])) || ((card) > 0))
            message_print(print10digits(RFIDcard), F("already exists"), 0);         
            i = n;
            k = 1;
            time = millis();           
            beep(2);
          }
        }
        for(int i=0;i<n;i++){    
          if(RFIDcards[i] == 0 && k == 0 && RFIDcard != 0){
            RFIDcards[i] = RFIDcard;
            message_print( print10digits(RFIDcard), F("registered"),0);
            cardConvert.cardNr = RFIDcard;
            EEPROM.write(i*6, cardConvert.cardByte[0]);
            EEPROM.write(i*6+1, cardConvert.cardByte[1]);
            EEPROM.write(i*6+2, cardConvert.cardByte[2]);
            EEPROM.write(i*6+3, cardConvert.cardByte[3]);
            creditArray[i] = priceArray[10]; // standard credit for newly registered cards          
            creditConvert.creditInt = creditArray[i];
            EEPROM.write(i*6+4, creditConvert.creditByte[0]);  
            EEPROM.write(i*6+5, creditConvert.creditByte[1]);
            beep(1);
            i = n;
            k = 2;
            time = millis();          
          }   
        }
      } 
      while ( (millis()-time) < 10000 );
      serlog(F("Registering ended"));
      beep(3);  
}

unsigned long nfcidread(void) {
  uint8_t success;
  uint8_t uid[] = { 0,0,0,0,0,0,0,0 };
  uint8_t uidLength;
  unsigned long id;

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

  if (success) {
    // ugly hack: fine for mifare classic (4 byte)
    // also fine for our ultras (last 4 bytes ever the same)
    nfc.PrintHex(uid, uidLength);
    id = (unsigned long)uid[0]<<24;
    id += (unsigned long)uid[1]<<16;
    id += (unsigned long)uid[2]<<8;
    id += (unsigned long)uid[3];
    return id;
  }
  return 0;
}

















