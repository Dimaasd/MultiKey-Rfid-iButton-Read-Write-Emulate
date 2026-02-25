#include <OneWire.h>                    // Library
#include <LiquidCrystal_I2C.h>          /// Library
LiquidCrystal_I2C lcd(0x27,16,2);       // 0x27 screen address on the I2C line.
#define pin 10                          // Pin D10 for connecting iButton (Data)
OneWire ibutton (pin);                  
byte addr[8];
byte ReadID[8] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x2F }; // "Universal" key. It is stitched if you do not attach the original key.

const int buttonPin = 2;               // Pin button D2
const int ledPin = 13;                 // LED pin plus D13
int buttonState = 0;                   // Variables
int writeflag = 0;                     // Variables
int readflag = 0;                      // Variables

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  lcd.init();                          // Initialization of Lcd display 16x2
  lcd.backlight();                     // Turn on the backlight of the Lcd display 16x2
  lcd.setCursor(1,0);                  // Set the cursor to the beginning of line 1
  lcd.print("Attach the key");         // First line when enabled
  lcd.setCursor(1,1);                  // Set the cursor to the beginning of line 2
  lcd.print("to the reader!");         // Second line when enabled

}

void loop() {

  buttonState = digitalRead(buttonPin);
  if (buttonState == LOW) {
    readflag = 1;
    writeflag = 1;
    digitalWrite(ledPin, HIGH);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Attach the blank");      // Set the cursor to the beginning of line 1
    lcd.setCursor(1,1);
    lcd.print("to the reader!");        // Set the cursor to the beginning of line 2
  }
  if (!ibutton.search (addr)) {
    ibutton.reset_search();
    delay(50);
    return;
  }

    digitalWrite(ledPin, HIGH);
     delay(50);
     lcd.clear();
     lcd.setCursor(0,0);
     lcd.print("Key number:");      
     lcd.setCursor(0,1);               // Checksum crc Not displayed on the screen.
     for (byte x = 0; x < 8; x++) {
    
    lcd.print(addr[x], HEX);
     if (readflag == 0) {
     ReadID[x] = (addr[x]);
    }
  }
  
     byte crc; // Checking the checksum
     crc = ibutton.crc8(addr, 7);
     digitalWrite(ledPin, LOW);

    if (writeflag == 1) {
    ibutton.skip(); ibutton.reset(); ibutton.write(0x33);
    lcd.clear();
    lcd.setCursor(4,0);
    lcd.print("The blank");              // Set the cursor to the beginning of line 1
    
    
    // send reset
    ibutton.skip();
    ibutton.reset();
    // send 0xD1
    ibutton.write(0xD1);
    // send logical 0
    digitalWrite(pin, LOW); pinMode(pin, OUTPUT); delayMicroseconds(60);
    pinMode(pin, INPUT); digitalWrite(pin, HIGH); delay(10);
    byte newID[8] = { (ReadID[0]), (ReadID[1]), (ReadID[2]), (ReadID[3]), (ReadID[4]), (ReadID[5]), (ReadID[6]), (ReadID[7]) };
    ibutton.skip();
    ibutton.reset();
    ibutton.write(0xD5);
    for (byte x = 0; x < 8; x++) {
      writeByte(newID[x]);
      
    }
    
    ibutton.reset();
    // send 0xD1
    ibutton.write(0xD1);
    //send logical 1
    digitalWrite(pin, LOW); pinMode(pin, OUTPUT); delayMicroseconds(10);
    pinMode(pin, INPUT); digitalWrite(pin, HIGH); delay(10);
    writeflag = 0;
    readflag = 0;
    digitalWrite(ledPin, LOW);
    lcd.setCursor(3,1);
    lcd.print("is written!");        // Set the cursor to the beginning of line 2
    delay(2000);
  }
}

    int writeByte(byte data) {
    int data_bit;
    for (data_bit = 0; data_bit < 8; data_bit++) {
    if (data & 1) {
      digitalWrite(pin, LOW); pinMode(pin, OUTPUT);
      delayMicroseconds(60);
      pinMode(pin, INPUT); digitalWrite(pin, HIGH);
      delay(10);
    } else {
      digitalWrite(pin, LOW); pinMode(pin, OUTPUT);
      pinMode(pin, INPUT); digitalWrite(pin, HIGH);
      delay(10);
    }
    data = data >> 1;
  }
  return 0;
}
