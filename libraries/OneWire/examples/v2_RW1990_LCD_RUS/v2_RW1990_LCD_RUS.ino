/*

Подключение LCD дисплея
GND-GND
+5V-VCC
A4-SDA
A5-SCL

*/
#include <OneWire.h>
#include <Wire.h> 
#include <LiquidCrystal.h>
#include <LiquidCrystal_I2C.h>
#include <LiquidCrystalRus_I2C.h>
#define pin 11
OneWire ibutton (pin); // Пин D11 для подлючения iButton (Data)
byte addr[8];
byte ReadID[8] = {0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x2f};  // "Универсальный" ключ. Прошивается последовательность 1:FF:FF:FF:FF:FF:FF:2F:CRC: 2F
const int buttonPin = 6;  // Пин D6 Подключение кнопки
const int ledPin = 13; // Пин D13 Подключение плюса светодиода
int buttonState = 0;
int writeflag = 0;
int readflag = 0;
LiquidCrystalRus_I2C lcd(0x27, 16, 2); // Адрес Lcd дисплея 16х2
void setup() {
  lcd.init();                             //Инициализация
  lcd.begin(16,2);
  lcd.backlight();                        //Подсветка (с некоторыми экранами не обязательно)
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT);
  Serial.begin(9600);                    // Скорость монитора порта
  lcd.setCursor(0, 0);                  // Устанавливаем курсор в начало 1 строки
  lcd.print(L"  Программатор");         //выводим русский текст
  lcd.setCursor(0, 1);               // Устанавливаем курсор в начало 2 строки
  lcd.print(L" RW-1990 Ключей");   // Вторая строка при включении  
}

void loop() {
  buttonState = digitalRead(buttonPin);
  if (buttonState == HIGH) {
    readflag = 1;
    writeflag = 1;
    digitalWrite(ledPin, HIGH);
  }
  if (!ibutton.search (addr)) {
    ibutton.reset_search();
    delay(50);
    return;
  }

  digitalWrite(ledPin, HIGH);
  delay(50);
    lcd.clear();
  for (byte x = 0; x < 8; x++) {
        Serial.print(addr[x], HEX);
    lcd.print(addr[x], HEX);
    if (readflag == 0) {
      ReadID[x] = (addr[x]);
    }
     Serial.print(":");
      if (x==7)  lcd.setCursor (0,1);
  }



  byte crc; // Проверка контрольной суммы
  crc = ibutton.crc8(addr, 7);
 lcd.setCursor(5, 1);
  lcd.print("CRC:");
  lcd.print(crc, HEX);
  Serial.print("  CRC:");
  Serial.println(crc, HEX);
  digitalWrite(ledPin, LOW);

  if ((writeflag == 1) or (Serial.read() == 'w')) {
    ibutton.skip(); ibutton.reset(); ibutton.write(0x33);
    Serial.println();
    Serial.println("    ID ключа перед записью:");
                                                                                                                                                                lcd.clear();
     //lcd.print("ID before write:");
    for (byte x = 0; x < 8; x++) {
      Serial.print(ibutton.read(), HEX);
      // lcd.print(' ');
     // lcd.print(ibutton.read(), HEX);
     Serial.print(':');     
          }
          Serial.print("  CRC:");
     Serial.println(crc, HEX);
      // send reset
    ibutton.skip();
    ibutton.reset();
    // send 0xD1
    ibutton.write(0xD1);
    // send logical 0
    digitalWrite(pin, LOW); pinMode(pin, OUTPUT); delayMicroseconds(60);
    pinMode(pin, INPUT); digitalWrite(pin, HIGH); delay(10);
    Serial.println();
    Serial.println("     ID ключа после записи:");
    //lcd.print('\n');
    lcd.print("Writing iButton:\n    ");
    byte newID[8] = { (ReadID[0]), (ReadID[1]), (ReadID[2]), (ReadID[3]), (ReadID[4]), (ReadID[5]), (ReadID[6]), (ReadID[7]) };
    ibutton.skip();
    ibutton.reset();
    ibutton.write(0xD5);
    for (byte x = 0; x < 8; x++) {
      writeByte(newID[x]);
      //lcd.print('*');
      Serial.print(newID[x], HEX);
      Serial.print(':');
     
    }
  Serial.print("  CRC:");
  Serial.println(crc, HEX);
  lcd.print('\n');
  Serial.println();
    ibutton.reset();
    // send 0xD1
    ibutton.write(0xD1);
    //send logical 1
    digitalWrite(pin, LOW); pinMode(pin, OUTPUT); delayMicroseconds(10);
    pinMode(pin, INPUT); digitalWrite(pin, HIGH); delay(10);
    writeflag = 0;
    readflag = 0;
    digitalWrite(ledPin, LOW);
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
