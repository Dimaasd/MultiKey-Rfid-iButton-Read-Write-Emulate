/*

Подключение LCD дисплея
GND-GND
+5V-VCC
A4-SDA
A5-SCL

*/
#define _LCD_TYPE 1
#include <OneWire.h>                    // Библиотека
#include <LCD_1602_RUS_ALL.h>           // Библиотека 
LCD_1602_RUS <LiquidCrystal_I2C> lcd(0x27, 16, 2);  // Адрес Lcd дисплея 16х2
#define pin 10                          // Пин D10 для подлючения iButton (Data)
OneWire ibutton (pin);                  
byte addr[8];
byte ReadID[8] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x2F }; // "Универсальный" ключ. Прошивается если не приложить исходный ключ.

const int buttonPin = 2;               // Пин кнопки D2
const int ledPin = 13;                 // Пин светодиода (Плюс) D13
int buttonState = 0;                   // переменные
int writeflag = 0;                     // переменные
int readflag = 0;                      // переменные

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  lcd.init();                           // Initialization of Lcd display 16x2
  lcd.backlight();                      // Включаем подсветку Lcd дисплея 16х2
  lcd.setCursor(3,0);                   // Устанавливаем курсор в начало 1 строки
  lcd.print("Прислоните");              // Первая строка при включении
  lcd.setCursor(6,1);                   // Устанавливаем курсор в начало 2 строки
  lcd.print("ключ!");                   // Вторая строка при включении  

}

void loop() {

  buttonState = digitalRead(buttonPin);
  if (buttonState == LOW) {
    readflag = 1;
    writeflag = 1;
    digitalWrite(ledPin, HIGH);
    lcd.clear();
    lcd.setCursor(3,0);
    lcd.print("Прислоните");
    lcd.setCursor(4,1);
    lcd.print("болванку!");
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
  lcd.print("Номер ключа:");     
  lcd.setCursor(0,1);               // Контрольная сумма crc На экран не выводится.
  for (byte x = 0; x < 8; x++) {
    
  lcd.print(addr[x], HEX);
  if (readflag == 0) {
      ReadID[x] = (addr[x]);
    }
  }
  
  byte crc; // Проверка контрольной суммы
  crc = ibutton.crc8(addr, 7);
  digitalWrite(ledPin, LOW);

  if (writeflag == 1) {
    ibutton.skip(); ibutton.reset(); ibutton.write(0x33);
    lcd.clear();
    lcd.setCursor(4,0);
    lcd.print("Болванка");  
    
    
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
    lcd.setCursor(4,1);
    lcd.print("записана");  
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