// Налаштування
#include <OneWire.h>
#include <OneWireSlave.h>
#include "pitches.h"
#include <EEPROM.h>
#include "TimerOne.h"
#include "GyverOLED.h"
#include <OneWireHub.h>
#include <DS2401.h>

//settings
#define rfidUsePWD 0        // ключ використовує пароль для змiни
#define rfidPWD 123456      // пароль для ключа
#define rfidBitRate 2       // Швидкiсть обмiну з rfid в kbps
#define MAX_KEYS 42         // Максимальна кількість ключів (42 * 24 = 1008 байт)

//pins
#define iButtonPin A3      // Лiнiя data ibutton
#define iBtnEmulPin 10     // Лiнiя емулятора ibutton
#define R_Led 8            // RGB Led
#define G_Led 2            
#define B_Led 12           
#define ACpin 6            // Вхiд Ain0 аналогового компаратора, замкнути на gnd
#define speakerPin 4       // Спiкер
#define FreqGen 11         // генератор 125 кГц
#define BTN_MODE A1        // Кнопка режиму
#define BTN_LEFT A2        // Кнопка "Лівий"
#define BTN_RIGHT A0       // Кнопка "Правий"

// OneWire об'єкти
OneWire ibutton (iButtonPin);
OneWireSlave iBtnEmul(iBtnEmulPin);

// OneWireHub для емуляції
auto hub = OneWireHub(iBtnEmulPin);
DS2401* currentDS = nullptr; // Поточний віртуальний пристрій

// Структура для зберігання ключа з назвою
struct KeyData {
  byte id[8];
  char name[16];  // Назва ключа (до 15 символів + нуль-термінатор)
};

byte maxKeyCount = MAX_KEYS;               // максимальна кiлькiсть ключiв - 42
byte EEPROM_key_count;                    // кiлькiсть ключiв 0..maxKeyCount, що зберiгаються в EEPROM
byte EEPROM_key_index = 0;                // 1..EEPROM_key_count номер останнього записаного в EEPROM ключа  
byte addr[8];                             // тимчасовий буфер
KeyData currentKey;                       // поточний ключ з назвою
byte rfidData[5];                         // значущi данi frid em-marine
byte halfT;                               // напiвперiод для метаком

// Змінні для обробки кнопок
unsigned long lastDebounceTime[3] = {0, 0, 0};
unsigned long debounceDelay = 50;
unsigned long holdTimeStart = 0;
bool holdMode = false;
bool lastBtnState[3] = {HIGH, HIGH, HIGH};
bool btnState[3] = {HIGH, HIGH, HIGH};
bool modeButtonPressed = false;
bool leftButtonPressed = false;
bool rightButtonPressed = false;

// Змінні для емуляції Dallas
bool emulationActive = false;

// ==================== 11 УНІВЕРСАЛЬНИХ КЛЮЧІВ ====================

// Масив з назвами для 11 універсальних ключів
const char* keyNames[11] = {
  
  "Домофон 22",
  "Домофон 76",
  "Домофон 42-1",
  "Домофон 42-2",
  "Гараж",
  "UK-3 Cyfral",
  "Universal",
  "Vizit 99%",
  "Forward",
  "Cyfral",
  "Metakom 95%",
  
};

// Масив з 11 ключами (для початку)
byte ReadID[11][8] = {
  {0x01, 0xAA, 0x69, 0xC9, 0x36, 0x00, 0x00, 0xE9}, // 5 - Metakom alt
  {0xFF, 0xA9, 0x8A, 0xB8, 0x13, 0x14, 0x8F, 0xB4}, // 6 - EM-Marine 1
  {0xFF, 0xA9, 0x8A, 0xA7, 0x65, 0xBC, 0x16, 0xE6}, // 7 - EM-Marine 2
  {0xFF, 0xA9, 0x8A, 0xA7, 0x65, 0xB9, 0x3D, 0x9C}, // 8 - EM-Marine 3
  {0xFF, 0x94, 0x60, 0x05, 0xF7, 0xBF, 0x5D, 0x30}, // 9 - EM-Marine 4
  
  {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3D}, // 1 - UK-3 Cyfral
  {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x2F}, // 2 - Стандартний універсальний
  {0x01, 0x53, 0xD4, 0xFE, 0x00, 0x00, 0x00, 0x6F}, // 3 - Vizit 99%
  {0x01, 0x76, 0xB8, 0x2E, 0x0F, 0x00, 0x00, 0x5C}, // 4 - Форвард
  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x14}, // - 11 Открываает 98% Metakom и некоторые Cyfral
  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0xA0}  // - 14 Metakom 95%
  
//  {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F, 0xFF, 0x00}, // - 1 Univer 1F
//  {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x2F, 0xFF, 0x00}, // - 2 Univer 2F
//  {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x9B}, // - 3 UK-1 Metakom 2003
//  {0x01, 0xBE, 0x40, 0x11, 0x5A, 0x36, 0x00, 0xE1}, // - 4 UK-2 Vizit – код универсального ключа, для Vizit 
//  {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3D}, // - 5 UK-3 Cyfral----------------------------------------------------1
//  {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x2F}, // - 6 Стандартный универсальный ключ --------------------------------2
//  {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00}, // - 7 Обычный
//  {0x01, 0x00, 0x00, 0x00, 0x00, 0x90, 0x19, 0xFF}, // - 8 Отлично работает на старых домофонах
//  {0x01, 0x53, 0xD4, 0xFE, 0x00, 0x00, 0x7E, 0x88}, // - 9 Cyfral, Metakom
//  {0x01, 0x53, 0xD4, 0xFE, 0x00, 0x00, 0x7E, 0x00}, // - 10 Cyfral,Metakom
//  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x14}, // - 11 Открываает 98% Metakom и некоторые Cyfral
//  {0x01, 0xFF, 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00}, // - 12 домофоны Cyfral + фильтр и защита
//  {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00}, // - 13 Metakom
//  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0xA0}, // - 14 Metakom 95%
//  {0x01, 0x00, 0xBE, 0x11, 0xAA, 0x00, 0x00, 0xFB}, // - 15 домофоны KeyMan
//  {0x01, 0xBE, 0x40, 0x11, 0x0A, 0x00, 0x00, 0x1D}, // - 16 проверен работает Vizit иногда KeyMan
//  {0x01, 0x53, 0xD4, 0xFE, 0x00, 0x00, 0x00, 0x6F}, // - 17 домофоны Vizit - до 99%---------------------------------------3
//  {0x01, 0xBE, 0x40, 0x11, 0x5A, 0x36, 0x00, 0x00}, // - 18 Vizit 99%
//  {0x01, 0x76, 0xB8, 0x2E, 0x0F, 0x00, 0x00, 0x5C}, // - 19 домофоны Форвард----------------------------------------------4
//  {0x01, 0xA9, 0xE4, 0x3C, 0x09, 0x00, 0x00, 0x00}, // - 20 домофоны Eltis - до 90%
//  {0x01, 0xBE, 0x40, 0x11, 0x5A, 0x56, 0x00, 0xBB}, // - 21 проверен работает 
//  {0x01, 0xBE, 0x40, 0x11, 0x00, 0x00, 0x00, 0x77}, // - 22 проверен работает 
};

enum emRWType {rwUnknown, TM01, RW1990_1, RW1990_2, TM2004, T5557, EM4305};               // тип заготовки
enum emkeyType {keyUnknown, keyDallas, keyTM2004, keyCyfral, keyMetacom, keyEM_Marine};    // тип оригiнального ключа  
emkeyType keyType;
enum emMode {md_empty, md_read, md_write, md_blueMode};               // режим роботи копiювальника
emMode copierMode = md_empty;

GyverOLED<SSD1306_128x32, OLED_NO_BUFFER> oled;

// CRC8 для iButton
uint8_t onewire_crc8(const uint8_t *addr, uint8_t len) {
  uint8_t crc = 0;
  while (len--) {
    uint8_t inbyte = *addr++;
    for (uint8_t i = 8; i; i--) {
      uint8_t mix = (crc ^ inbyte) & 0x01;
      crc >>= 1;
      if (mix) crc ^= 0x8C;
      inbyte >>= 1;
    }
  }
  return crc;
}

// Проста функцiя для використання латинських букв замiсть українських
String latinText(String input) {
  String result = "";
  for (unsigned int i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c == 'і' || c == 'І') result += 'i';
    else if (c == 'ї' || c == 'Ї') result += 'i';
    else if (c == 'є' || c == 'Є') result += 'e';
    else if (c == 'ґ' || c == 'Ґ') result += 'g';
    else result += c;
  }
  return result;
}

//***************** звуки ****************
void Sd_StartOK() {
  tone(speakerPin, NOTE_A7); delay(100);
  tone(speakerPin, NOTE_G7); delay(100);
  tone(speakerPin, NOTE_E7); delay(100); 
  tone(speakerPin, NOTE_C7); delay(100);  
  tone(speakerPin, NOTE_D7); delay(100); 
  tone(speakerPin, NOTE_B7); delay(100); 
  tone(speakerPin, NOTE_F7); delay(100); 
  tone(speakerPin, NOTE_C7); delay(100);
  noTone(speakerPin); 
}

void Sd_ReadOK() {
  for (int i=400; i<6000; i=i*1.5) { tone(speakerPin, i); delay(20); }
  noTone(speakerPin);
}

void Sd_WriteStep(){
  for (int i=2500; i<6000; i=i*1.5) { tone(speakerPin, i); delay(10); }
  noTone(speakerPin);
}

void Sd_ErrorBeep() {
  for (int j=0; j <3; j++){
    for (int i=1000; i<2000; i=i*1.1) { tone(speakerPin, i); delay(10); }
    delay(50);
    for (int i=1000; i>500; i=i*1.9) { tone(speakerPin, i); delay(10); }
    delay(50);
  }
  noTone(speakerPin);
}

void Sd_DeleteOK() {
  tone(speakerPin, NOTE_C6); delay(100);
  tone(speakerPin, NOTE_G5); delay(100);
  tone(speakerPin, NOTE_E5); delay(100);
  noTone(speakerPin);
}

void Sd_EmulationStart() {
  tone(speakerPin, NOTE_C6); delay(50);
  tone(speakerPin, NOTE_E6); delay(50);
  tone(speakerPin, NOTE_G6); delay(50);
  tone(speakerPin, NOTE_C7); delay(50);
  noTone(speakerPin);
}

// Функція для отримання назви ключа за його ID
String getKeyName(byte buf[8]) {
  // Спочатку перевіряємо в 9 універсальних ключах
  for (int i = 0; i < 11; i++) {
    if (memcmp(buf, ReadID[i], 8) == 0) {
      return String(keyNames[i]);
    }
  }
  
  // Якщо ключ збережений в EEPROM, перевіряємо чи є в нього назва
  if (currentKey.name[0] != '\0') {
    return String(currentKey.name);
  }
  
  // Якщо назви немає, повертаємо "Key X"
  return "Key " + String(EEPROM_key_index);
}

// Функцiя для виведення RFID iнформацiї
void OLED_printRFIDInfo(byte buf[8], byte msgType = 0) {
  oled.clear();
  oled.home();
  
  String st;
  switch (msgType){
    case 0: st = "KEY " + String(EEPROM_key_index) + "/" + String(EEPROM_key_count); break;      
    case 1: st = "Hold to save";  break; 
    case 2: st = "Hold to DELETE"; break;
    case 3: st = "Key " + String(indxKeyInROM(buf)) + " in ROM";  break;   
    case 4: st = "--EMULATION MODE--"; break;
  }
  oled.println(latinText(st));
  
  String hexStr = "";
  for (byte i = 0; i < 8; i++) {
    if (buf[i] < 0x10) hexStr += "0";
    hexStr += String(buf[i], HEX);
    if (i < 7) hexStr += "";
  }
  hexStr += ":";
  oled.println(hexStr);
  
  unsigned long keyNum = 0;
  byte idValue = 0;
  
  if (vertEvenCheck(buf)) {
    idValue = rfidData[0];
    keyNum = (unsigned long)rfidData[1]<<24 | (unsigned long)rfidData[2]<<16 | 
             (unsigned long)rfidData[3]<<8 | (unsigned long)rfidData[4];
    
    String infoStr = "id " + String(idValue) + " key " + String(keyNum);
    oled.println(infoStr);
  } else {
    oled.println("(id ? key ?)");
  }
  
  String nameStr = getKeyName(buf);
  oled.println(latinText(nameStr));
  
  oled.update();
}

void OLED_printKey(byte buf[8], byte msgType = 0){
  if (keyType == keyEM_Marine) {
    OLED_printRFIDInfo(buf, msgType);
    return;
  }
  
  oled.clear();
  oled.home();
  
  String st;
  switch (msgType){
    case 0: st = "Key " + String(EEPROM_key_index) + "/" + String(EEPROM_key_count); break;      
    case 1: st = "Hold to save";  break; 
    case 2: st = "Hold to DELETE"; break;
    case 3: st = "Key " + String(indxKeyInROM(buf)) + " in ROM";  break;   
    case 4: st = "--EMULATION MODE--"; break;
  }
  oled.println(latinText(st));
  
  st = "";
  for (byte i = 0; i < 8; i++) {
    if (buf[i] < 0x10) st += "0";
    st += String(buf[i], HEX) + "";
  }
  oled.println(st);
  
  st = "Type: ";
  switch (keyType){
    case keyDallas: st += "Dallas"; break;      
    case keyCyfral: st += "Cyfral";  break;  
    case keyMetacom: st += "Metakom"; break;             
    case keyEM_Marine: st += "EM-Marine"; break;
    case keyTM2004: st += "TM2004"; break;
    case keyUnknown: st += "Unknown"; break;
  }
  oled.println(latinText(st));
  
  String nameStr = getKeyName(buf);
  oled.println(latinText(nameStr));
  
  oled.update();
}

void OLED_printError(String st, bool err = true){
  oled.clear();
  oled.home();
  if (err) oled.println(latinText("Error!"));
    else oled.println("OK");
  oled.println(latinText(st));  
  oled.update();
}

// ==================== ВИДАЛЕННЯ КЛЮЧА ====================
void deleteCurrentKey() {
  if (EEPROM_key_count == 0) {
    OLED_printError("No keys to delete");
    Sd_ErrorBeep();
    delay(1000);
    return;
  }
  
  // Показуємо підтвердження
  oled.clear();
  oled.home();
  oled.println("DELETE KEY?");
  oled.println(String(EEPROM_key_index) + "/" + String(EEPROM_key_count));
  oled.println("Hold MODE to confirm");
  oled.update();
  
  // Чекаємо підтвердження (ще 1 секунду)
  unsigned long confirmStart = millis();
  bool confirmed = false;
  
  while (millis() - confirmStart < 1000) {
    if (digitalRead(BTN_MODE) == LOW) {
      confirmed = true;
      break;
    }
    delay(50);
  }
  
  if (!confirmed) {
    OLED_printKey(currentKey.id);
    return;
  }
  
  // Видаляємо ключ
  if (EEPROM_key_count == 1) {
    // Якщо це останній ключ
    EEPROM_key_count = 0;
    EEPROM_key_index = 0;
    EEPROM.update(0, 0);
    EEPROM.update(1, 0);
    
    oled.clear();
    oled.home();
    oled.println("All keys deleted");
    oled.update();
    Sd_DeleteOK();
    delay(1000);
    
    copierMode = md_empty;
    clearLed();
    
  } else {
    // Зсуваємо всі ключі після видаленого
    for (byte i = EEPROM_key_index; i < EEPROM_key_count; i++) {
      KeyData nextKey;
      EEPROM.get((i + 1) * sizeof(KeyData), nextKey);
      EEPROM.put(i * sizeof(KeyData), nextKey);
    }
    
    EEPROM_key_count--;
    if (EEPROM_key_index > EEPROM_key_count) EEPROM_key_index = EEPROM_key_count;
    
    EEPROM.update(0, EEPROM_key_count);
    EEPROM.update(1, EEPROM_key_index);
    
    // Завантажуємо поточний ключ
    EEPROM_get_key(EEPROM_key_index, currentKey.id);
    
    oled.clear();
    oled.home();
    oled.println("Key deleted");
    oled.update();
    Sd_DeleteOK();
    delay(1000);
  }
  
  // Показуємо поточний ключ
  if (EEPROM_key_count > 0) {
    OLED_printKey(currentKey.id);
    digitalWrite(G_Led, HIGH);
  } else {
    oled.clear();
    oled.home();
    oled.println("No keys in ROM");
    oled.update();
  }
}

// ==================== ЕМУЛЯЦІЯ DALLAS ====================
void startDallasEmulation(byte* keyData) {
  // Зупиняємо попередню емуляцію якщо була
  if (currentDS != nullptr) {
    hub.detach(*currentDS);
    delete currentDS;
    currentDS = nullptr;
  }
  
  // Виправляємо CRC
  byte emulKey[8];
  memcpy(emulKey, keyData, 8);
  
  if (keyType == keyDallas || keyType == keyTM2004 || keyType == keyMetacom || keyType == keyCyfral) {
    emulKey[7] = onewire_crc8(emulKey, 7);
  }
  
  // Створюємо віртуальний пристрій
  currentDS = new DS2401(emulKey[0], emulKey[1], emulKey[2], emulKey[3],
                         emulKey[4], emulKey[5], emulKey[6]);
  
  hub.attach(*currentDS);
  
  emulationActive = true;
  digitalWrite(B_Led, HIGH);
  
  // Видалено Serial.println(F("Dallas emulation started"));
  Sd_EmulationStart();
  
  // Показуємо на екрані
  oled.clear();
  oled.home();
  oled.println(">>> EMULATING <<<");
  
  char buf[20];
  sprintf(buf, "%02X %02X %02X %02X", emulKey[0], emulKey[1], emulKey[2], emulKey[3]);
  oled.println(buf);
  sprintf(buf, "%02X %02X %02X %02X", emulKey[4], emulKey[5], emulKey[6], emulKey[7]);
  oled.println(buf);
  oled.println("MODE=stop");
  oled.update();
}

void stopDallasEmulation() {
  emulationActive = false;
  
  if (currentDS != nullptr) {
    hub.detach(*currentDS);
    delete currentDS;
    currentDS = nullptr;
  }
  
  digitalWrite(B_Led, LOW);
  // Видалено Serial.println(F("Dallas emulation stopped"));
}

// ==================== РОБОТА З EEPROM ====================
byte indxKeyInROM(byte buf[]){
  KeyData keyBuf; bool eq = true;
  for (byte j = 1; j <= EEPROM_key_count; j++){
    EEPROM.get(j * sizeof(KeyData), keyBuf);
    eq = true;
    for (byte i = 0; i < 8; i++) 
      if (keyBuf.id[i] != buf[i]) { eq = false; break; }
    if (eq) return j;
  }
  return 0;
}

bool EPPROM_AddKey(KeyData key){
  byte indx = indxKeyInROM(key.id);
  if (indx != 0) { 
    EEPROM_key_index = indx;
    EEPROM.update(1, EEPROM_key_index);
    return false; 
  }
  
  if (EEPROM_key_count < maxKeyCount) {
    EEPROM_key_count++;
    EEPROM_key_index = EEPROM_key_count;
  } else {
    EEPROM_key_index++;
    if (EEPROM_key_index > maxKeyCount) EEPROM_key_index = 1;
  }
  
  // Видалено Serial.println(F("Adding to ROM"));
  // for (byte i = 0; i < 8; i++) {
  //   Serial.print(key.id[i], HEX); Serial.print(F(":"));  
  // }
  // Serial.print(F(" Name: "));
  // Serial.println(key.name);
  
  EEPROM.put(EEPROM_key_index * sizeof(KeyData), key);
  EEPROM.update(0, EEPROM_key_count);
  EEPROM.update(1, EEPROM_key_index);
  return true;
}

void EEPROM_get_key(byte EEPROM_key_index1, byte buf[8]){
  KeyData keyBuf;
  int address = EEPROM_key_index1 * sizeof(KeyData);
  if (address + sizeof(KeyData) > EEPROM.length()) return;
  EEPROM.get(address, keyBuf);
  for (byte i = 0; i < 8; i++) buf[i] = keyBuf.id[i];
  strcpy(currentKey.name, keyBuf.name);
  keyType = getKeyType(keyBuf.id);
}

emkeyType getKeyType(byte* buf){
  if (buf[0] == 0x01) return keyDallas;
  if ((buf[0] >> 4) == 0b0001) return keyCyfral;
  if ((buf[0] >> 4) == 0b0010) return keyMetacom;
  if ((buf[0] == 0xFF) && vertEvenCheck(buf)) return keyEM_Marine;
  return keyUnknown;
}

// Функція для читання стану кнопки з антидребезгом
bool readButton(int pin, int btnIndex) {
  bool reading = digitalRead(pin);
  
  if (reading != lastBtnState[btnIndex]) {
    lastDebounceTime[btnIndex] = millis();
  }
  
  if ((millis() - lastDebounceTime[btnIndex]) > debounceDelay) {
    if (reading != btnState[btnIndex]) {
      btnState[btnIndex] = reading;
    }
  }
  
  lastBtnState[btnIndex] = reading;
  return btnState[btnIndex] == LOW;
}

//*************** Dallas **************
emRWType getRWtype(){    
   byte answer;
  ibutton.reset(); ibutton.write(0xD1);
  ibutton.write_bit(1);
  delay(10); pinMode(iButtonPin, INPUT);
  ibutton.reset(); ibutton.write(0xB5);
  answer = ibutton.read();
  if (answer == 0xFE){
    // Видалено Serial.println(F(" Type: dallas RW-1990.1 "));
    return RW1990_1;
  }
  
  ibutton.reset(); ibutton.write(0x1D);
  ibutton.write_bit(1);
  delay(10); pinMode(iButtonPin, INPUT);
  ibutton.reset(); ibutton.write(0x1E);
  answer = ibutton.read();
  if (answer == 0xFE){
    ibutton.reset(); ibutton.write(0x1D);
    ibutton.write_bit(0);
    delay(10); pinMode(iButtonPin, INPUT);
    // Видалено Serial.println(F(" Type: dallas RW-1990.2 "));
    return RW1990_2;
  }
  
  ibutton.reset(); ibutton.write(0x33);
  for ( byte i=0; i<8; i++) ibutton.read();
  ibutton.write(0xAA);
  ibutton.write(0x00); ibutton.write(0x00);
  answer = ibutton.read();
  byte m1[3] = {0xAA, 0,0};
  if (OneWire::crc8(m1, 3) == answer) {
    answer = ibutton.read();
    // Видалено Serial.println(F(" Type: dallas TM2004"));
    ibutton.reset();
    return TM2004;
  }
  ibutton.reset();
  // Видалено Serial.println(F(" Type: dallas unknown, trying TM-01! "));
  return TM01;
}

bool write2iBtnTM2004(){
  byte answer; bool result = true;
  ibutton.reset();
  ibutton.write(0x3C);
  ibutton.write(0x00); ibutton.write(0x00);
  for (byte i = 0; i<8; i++){
    digitalWrite(R_Led, !digitalRead(R_Led));
    ibutton.write(currentKey.id[i]);
    answer = ibutton.read();
    delayMicroseconds(600); ibutton.write_bit(1); delay(50);
    pinMode(iButtonPin, INPUT);
    // Видалено Serial.print('*');
    Sd_WriteStep();
    if (currentKey.id[i] != ibutton.read()) { result = false; break;}
  } 
  if (!result){
    ibutton.reset();
    // Видалено Serial.println(F(" Copy error"));
    OLED_printError(latinText("Copy error"));
    Sd_ErrorBeep();
    digitalWrite(R_Led, HIGH);
    return false;    
  }
  ibutton.reset();
  // Видалено Serial.println(F(" Key copied successfully"));
  OLED_printError(latinText("Key copied"), false);
  Sd_ReadOK();
  delay(2000);
  digitalWrite(R_Led, HIGH);
  return true;
}

bool write2iBtnRW1990_1_2_TM01(emRWType rwType){
  byte rwCmd, bitCnt = 64, rwFlag = 1;
  switch (rwType){
    case TM01: rwCmd = 0xC1; if ((keyType == keyMetacom)||(keyType == keyCyfral)) bitCnt = 36; break;
    case RW1990_1: rwCmd = 0xD1; rwFlag = 0; break;
    case RW1990_2: rwCmd = 0x1D; break;
  }
  ibutton.reset(); ibutton.write(rwCmd);
  ibutton.write_bit(rwFlag);
  delay(5); pinMode(iButtonPin, INPUT);
  ibutton.reset(); 
  if (rwType == TM01) ibutton.write(0xC5);
    else ibutton.write(0xD5);
  if (bitCnt == 36) BurnByteMC(currentKey.id);
  else for (byte i = 0; i< (bitCnt >> 3); i++){
    digitalWrite(R_Led, !digitalRead(R_Led));
    if (rwType == RW1990_1) BurnByte(~currentKey.id[i]);
      else BurnByte(currentKey.id[i]);
    // Видалено Serial.print('*');
    Sd_WriteStep();
  }
  if (bitCnt == 64) {
      ibutton.write(rwCmd);
      ibutton.write_bit(!rwFlag);
      delay(5); pinMode(iButtonPin, INPUT);
    }
  digitalWrite(R_Led, LOW);       
  if (!dataIsBurningOK(bitCnt)){
    // Видалено Serial.println(F(" Copy error"));
    OLED_printError(latinText("Copy error"));
    Sd_ErrorBeep();
    digitalWrite(R_Led, HIGH);
    return false;
  }
  // Видалено Serial.println(F(" Key copied successfully"));
  if ((keyType == keyMetacom)||(keyType == keyCyfral)){
    ibutton.reset();
    if (keyType == keyCyfral) ibutton.write(0xCA);
      else ibutton.write(0xCB);
    ibutton.write_bit(1);
    delay(10); pinMode(iButtonPin, INPUT);
  }
  OLED_printError(latinText("Key copied"), false);
  Sd_ReadOK();
  delay(2000);
  digitalWrite(R_Led, HIGH);
  return true;
}

void BurnByte(byte data){
  for(byte n_bit = 0; n_bit < 8; n_bit++){ 
    ibutton.write_bit(data & 1);  
    delay(5);
    data = data >> 1;
  }
  pinMode(iButtonPin, INPUT);
}

void BurnByteMC(byte buf[8]){
  byte j = 0;
  for(byte n_bit = 0; n_bit < 36; n_bit++){ 
    ibutton.write_bit(((~buf[n_bit>>3]) >> (7-j) ) & 1);  
    delay(5);
    j++;
    if (j > 7) j = 0;
  }
  pinMode(iButtonPin, INPUT);
}

void convetr2MC(byte buff[8]){
  byte data;
  for (byte i = 0; i < 5; i++){
    data = ~buff[i];
    buff[i] = 0;
    for (byte j = 0; j < 8; j++) 
      if ( (data>>j)&1) bitSet(buff[i], 7-j);
  }
  buff[4] &= 0xf0;  buff[5] = 0; buff[6] = 0; buff[7] = 0;
}

bool dataIsBurningOK(byte bitCnt){
  byte buff[8];
  if (!ibutton.reset()) return false;
  ibutton.write(0x33);
  ibutton.read_bytes(buff, 8);
  if (bitCnt == 36) convetr2MC(buff);
  byte Check = 0;
  for (byte i = 0; i < 8; i++){ 
    if (currentKey.id[i] == buff[i]) Check++;
    // Видалено Serial.print(buff[i], HEX); Serial.print(":");
  }
  if (Check != 8) return false;
  return true;
}

bool write2iBtn(){
  int Check = 0;
  if (!ibutton.search(addr)) { 
    ibutton.reset_search(); 
    return false;
  }
  // Видалено Serial.print(F("New key code: "));
  for (byte i = 0; i < 8; i++) {
    // Видалено Serial.print(addr[i], HEX); Serial.print(":");  
    if (currentKey.id[i] == addr[i]) Check++;
  }
  if (Check == 8) {
    digitalWrite(R_Led, LOW); 
    // Видалено Serial.println(F("same key. No write needed"));
    OLED_printError(latinText("Same key"));
    Sd_ErrorBeep();
    digitalWrite(R_Led, HIGH);
    delay(1000);
    return false;
  }
  emRWType rwType = getRWtype();
  // Видалено Serial.print(F("\n Writing iButton ID: "));
  if (rwType == TM2004) return write2iBtnTM2004();
    else return write2iBtnRW1990_1_2_TM01(rwType);
}

bool searchIbutton(){
  if (!ibutton.search(addr)) { 
    ibutton.reset_search(); 
    return false;
  }  
  for (byte i = 0; i < 8; i++) {
    // Видалено Serial.print(addr[i], HEX); Serial.print(":");
    currentKey.id[i] = addr[i];
  }
  if (addr[0] == 0x01) {
    keyType = keyDallas;
    if (getRWtype() == TM2004) keyType = keyTM2004;
    if (OneWire::crc8(addr, 7) != addr[7]) {
      // Видалено Serial.println(F("CRC error!"));
      OLED_printError(latinText("CRC error!"));
      Sd_ErrorBeep();
      digitalWrite(B_Led, HIGH);
      return false;
    }
    return true;
  }
  // Видалено switch (addr[0]>>4){
  //   case 1: Serial.println(F(" Type: Maybe cyfral in Dallas")); break;      
  //   case 2: Serial.println(F(" Type: Maybe metacom in Dallas"));  break;  
  //   case 3: Serial.println(F(" Type: unknown Dallas family")); break;             
  // }
  keyType = keyUnknown;
  return true;
}

//************ Cyfral ***********************
unsigned long pulseACompA(bool pulse, byte Average = 80, unsigned long timeOut = 1500){
  bool AcompState;
  unsigned long tEnd = micros() + timeOut;
  do {
    ADCSRA |= (1<<ADSC);
    while(ADCSRA & (1 << ADSC));
    if (ADCH > 200) return 0;
    if (ADCH > Average) AcompState = HIGH;
      else AcompState = LOW;
    if (AcompState == pulse) {
      tEnd = micros() + timeOut;
      do {
          ADCSRA |= (1<<ADSC);
          while(ADCSRA & (1 << ADSC));
        if (ADCH > Average) AcompState = HIGH;
          else AcompState = LOW;
        if (AcompState != pulse) return (unsigned long)(micros() + timeOut - tEnd);  
      } while (micros() < tEnd);
      return 0;
    }
  } while (micros() < tEnd);
  return 0;
}

void ADCsetOn(){
  ADMUX = (ADMUX&0b11110000) | 0b0011 | (1<<ADLAR);
  ADCSRB = (ADCSRB & 0b11111000) | (1<<ACME);
  ADCSRA = (ADCSRA & 0b11111000) |0b011 | (1<<ADEN) | (1<<ADSC);
}

void ACsetOn(){
  ACSR |= 1<<ACBG;
  ADCSRA &= ~(1<<ADEN);
  ADMUX = (ADMUX&0b11110000) | 0b0011;
  ADCSRB |= 1<<ACME;
}

bool read_cyfral(byte* buf, byte CyfralPin){
  unsigned long ti; byte i=0, j = 0, k = 0;
  analogRead(iButtonPin);
  ADCsetOn(); 
  byte aver = calcAverage();
  unsigned long tEnd = millis() + 30;
  do{
    ti = pulseACompA(HIGH, aver);
    if ((ti == 0) || (ti > 260) || (ti < 10)) {i = 0; j=0; k = 0; continue;}
    if ((i < 3) && (ti > halfT)) {i = 0; j = 0; k = 0; continue;}
    if ((i == 3) && (ti < halfT)) continue;      
    if (ti > halfT) bitSet(buf[i >> 3], 7-j);
      else if (i > 3) k++; 
    if ((i > 3) && ((i-3)%4 == 0) ){
      if (k != 1) {for (byte n = 0; n < (i >> 3)+2; n++) buf[n] = 0; i = 0; j = 0; k = 0; continue;}
      k = 0; 
    }
    j++; if (j>7) j=0;
    i++;
  } while ((millis() < tEnd) && (i < 36));
  if (i < 36) return false;
  return true;
}

bool searchCyfral(){
  byte buf[8];
  for (byte i = 0; i < 8; i++) {addr[i] =0; buf[i] = 0;}
  if (!read_cyfral(addr, iButtonPin)) return false;
  if (!read_cyfral(buf, iButtonPin)) return false;
  for (byte i = 0; i < 8; i++) 
    if (addr[i] != buf[i]) return false;
  keyType = keyCyfral;
  for (byte i = 0; i < 8; i++) {
    // Видалено Serial.print(addr[i], HEX); Serial.print(":");
    currentKey.id[i] = addr[i];
  }
  currentKey.name[0] = '\0';
  // Видалено Serial.println(F(" Type: Cyfral "));
  return true;  
}

byte calcAverage(){
  unsigned int sum = 127; byte preADCH = 0, j = 0; 
  for (byte i = 0; i<255; i++) {
    ADCSRA |= (1<<ADSC);
    delayMicroseconds(10);
    while(ADCSRA & (1 << ADSC));
    sum += ADCH;
  }
  sum = sum >> 8;
  unsigned long tSt = micros();
  for (byte i = 0; i<255; i++) {
    delayMicroseconds(4);
    ADCSRA |= (1<<ADSC);
    while(ADCSRA & (1 << ADSC));
    if (((ADCH > sum)&&(preADCH < sum)) | ((ADCH < sum)&&(preADCH > sum))) {
      j++;
      preADCH = ADCH;
    }   
  }
  halfT = (byte)((micros() - tSt) / j);
  return (byte)sum;
}

bool read_metacom(byte* buf, byte MetacomPin){
  unsigned long ti; byte i = 0, j = 0, k = 0;
  analogRead(iButtonPin);
  ADCsetOn();
  byte aver = calcAverage();
  unsigned long tEnd = millis() + 30;
  do{
    ti = pulseACompA(LOW, aver);
    if ((ti == 0) || (ti > 500)) {i = 0; j=0; k = 0; continue;}
    if ((i == 0) && (ti+30 < (halfT<<1))) continue;
    if ((i == 2) && (ti > halfT)) {i = 0; j = 0;  continue;}
    if (((i == 1) || (i == 3)) && (ti < halfT)) {i = 0; j = 0; continue;}
    if (ti < halfT) {   
      bitSet(buf[i >> 3], 7-j);
      if (i > 3) k++;
    }
    if ((i > 3) && ((i-3)%8 == 0) ){
      if (k & 1) { for (byte n = 0; n < (i >> 3)+1; n++) buf[n] = 0; i = 0; j = 0;  k = 0; continue;}
      k = 0;
    }   
    j++; if (j>7) j=0;
    i++;
  }  while ((millis() < tEnd) && (i < 36));
  if (i < 36) return false;
  return true;
}

bool searchMetacom(){
  byte buf[8];
  for (byte i = 0; i < 8; i++) {addr[i] =0; buf[i] = 0;}
  if (!read_metacom(addr, iButtonPin)) return false;
  if (!read_metacom(buf, iButtonPin)) return false;
  for (byte i = 0; i < 8; i++) 
    if (addr[i] != buf[i]) return false; 
  keyType = keyMetacom;
  for (byte i = 0; i < 8; i++) {
    // Видалено Serial.print(addr[i], HEX); Serial.print(":");
    currentKey.id[i] = addr[i];
  }
  currentKey.name[0] = '\0';
  // Видалено Serial.println(F(" Type: Metacom "));
  return true;  
}

//**********EM-Marine***************************
bool vertEvenCheck(byte* buf){
  byte k;
  k = 1&buf[1]>>6 + 1&buf[1]>>1 + 1&buf[2]>>4 + 1&buf[3]>>7 + 1&buf[3]>>2 + 1&buf[4]>>5 + 1&buf[4] + 1&buf[5]>>3 + 1&buf[6]>>6 + 1&buf[6]>>1 + 1&buf[7]>>4;
  if (k&1) return false;
  k = 1&buf[1]>>5 + 1&buf[1] + 1&buf[2]>>3 + 1&buf[3]>>6 + 1&buf[3]>>1 + 1&buf[4]>>4 + 1&buf[5]>>7 + 1&buf[5]>>2 + 1&buf[6]>>5 + 1&buf[6] + 1&buf[7]>>3;
  if (k&1) return false;
  k = 1&buf[1]>>4 + 1&buf[2]>>7 + 1&buf[2]>>2 + 1&buf[3]>>5 + 1&buf[3] + 1&buf[4]>>3 + 1&buf[5]>>6 + 1&buf[5]>>1 + 1&buf[6]>>4 + 1&buf[7]>>7 + 1&buf[7]>>2;
  if (k&1) return false;
  k = 1&buf[1]>>3 + 1&buf[2]>>6 + 1&buf[2]>>1 + 1&buf[3]>>4 + 1&buf[4]>>7 + 1&buf[4]>>2 + 1&buf[5]>>5 + 1&buf[5] + 1&buf[6]>>3 + 1&buf[7]>>6 + 1&buf[7]>>1;
  if (k&1) return false;
  if (1&buf[7]) return false;
  
  rfidData[0] = (0b01111000&buf[1])<<1 | (0b11&buf[1])<<2 | buf[2]>>6;
  rfidData[1] = (0b00011110&buf[2])<<3 | buf[3]>>4;
  rfidData[2] = buf[3]<<5 | (0b10000000&buf[4])>>3 | (0b00111100&buf[4])>>2;
  rfidData[3] = buf[4]<<7 | (0b11100000&buf[5])>>1 | 0b1111&buf[5];
  rfidData[4] = (0b01111000&buf[6])<<1 | (0b11&buf[6])<<2 | buf[7]>>6;
  return true;
}

byte ttAComp(unsigned long timeOut = 7000){
  byte AcompState, AcompInitState;
  unsigned long tEnd = micros() + timeOut;
  AcompInitState = (ACSR >> ACO)&1;
  do {
    AcompState = (ACSR >> ACO)&1;
    if (AcompState != AcompInitState) {
      delayMicroseconds(1000/(rfidBitRate*4));
      AcompState = (ACSR >> ACO)&1;
      delayMicroseconds(1000/(rfidBitRate*2));
      return AcompState;  
    }
  } while (micros() < tEnd);
  return 2;
}

bool readEM_Marie(byte* buf){
  unsigned long tEnd = millis() + 50;
  byte ti; byte j = 0, k=0;
  for (int i = 0; i<64; i++){
    ti = ttAComp();
    if (ti == 2)  break;
    if ( ( ti == 0 ) && ( i < 9)) {
      if (millis() > tEnd) { ti=2; break;}
      i = -1; j=0; continue;
    }
    if ((i > 8) && (i < 59)){
      if (ti) k++;
      if ( (i-9)%5 == 4 ){
        if (k & 1) {
          i = -1; j = 0; k = 0; continue; 
        }
        k = 0;
      }
    }
    if (ti) bitSet(buf[i >> 3], 7-j);
      else bitClear(buf[i >> 3], 7-j);
    j++; if (j>7) j=0; 
  }
  if (ti == 2) return false;
  return vertEvenCheck(buf);
}

void rfidACsetOn(){
  pinMode(FreqGen, OUTPUT);
  TCCR2A = _BV(COM2A0) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(WGM22) | _BV(CS20);
  OCR2A = 63;
  OCR2B = 31;
  ADCSRB &= ~(1<<ACME);
  ACSR &= ~(1<<ACBG);
}

bool searchEM_Marine(bool copyKey = true){
  byte gr = digitalRead(G_Led);
  bool rez = false;
  rfidACsetOn();
  delay(6);
  if (!readEM_Marie(addr)) {
    if (!copyKey) TCCR2A &=0b00111111;
    digitalWrite(G_Led, gr);
    return rez;
  }
  rez = true;
  keyType = keyEM_Marine;
  for (byte i = 0; i<8; i++){
    if (copyKey) currentKey.id[i] = addr[i];
    // Видалено Serial.print(addr[i], HEX); Serial.print(":");
  }
  
  // Видалено Serial.print(F(" ( id "));
  // Serial.print(rfidData[0]); Serial.print(" key ");
  // unsigned long keyNum = (unsigned long)rfidData[1]<<24 | (unsigned long)rfidData[2]<<16 | (unsigned long)rfidData[3]<<8 | (unsigned long)rfidData[4];
  // Serial.print(keyNum);
  // Serial.println(F(") Type: EM-Marine "));
  
  if (copyKey) currentKey.name[0] = '\0';
  
  if (!copyKey) TCCR2A &=0b00111111;
  digitalWrite(G_Led, gr);
  return rez;
}

void TxBitRfid(byte data){
  if (data & 1) delayMicroseconds(54*8); 
    else delayMicroseconds(24*8);
  rfidGap(19*8);
}

void TxByteRfid(byte data){
  for(byte n_bit=0; n_bit<8; n_bit++){
    TxBitRfid(data & 1);
    data = data >> 1;
  }
}

void rfidGap(unsigned int tm){
  TCCR2A &=0b00111111;
  delayMicroseconds(tm);
  TCCR2A |= _BV(COM2A0);
}

bool T5557_blockRead(byte* buf){
  byte ti; byte j = 0, k=0;
  for (int i = 0; i<33; i++){
    ti = ttAComp(2000);
    if (ti == 2) break;
    if ( ( ti == 1 ) && ( i == 0)) { ti=2; break; }
    if (i > 0){
      if (ti) bitSet(buf[(i-1) >> 3], 7-j);
        else bitClear(buf[(i-1) >> 3], 7-j);
      j++; if (j>7) j=0;
    }
  }
  if (ti == 2) return false;
  return true;
}

bool sendOpT5557(byte opCode, unsigned long password = 0, byte lockBit = 0, unsigned long data = 0, byte blokAddr = 1){
  TxBitRfid(opCode >> 1); TxBitRfid(opCode & 1);
  if (opCode == 0b00) return true;
  TxBitRfid(lockBit & 1);
  if (data != 0){
    for (byte i = 0; i<32; i++) {
      TxBitRfid((data>>(31-i)) & 1);
    }
  }
  TxBitRfid(blokAddr>>2); TxBitRfid(blokAddr>>1); TxBitRfid(blokAddr & 1);
  delay(4);
  return true;
}

bool write2rfidT5557(byte* buf){
  bool result; unsigned long data32;
  delay(6);
  for (byte k = 0; k<2; k++){
    data32 = (unsigned long)buf[0 + (k<<2)]<<24 | (unsigned long)buf[1 + (k<<2)]<<16 | (unsigned long)buf[2 + (k<<2)]<<8 | (unsigned long)buf[3 + (k<<2)];
    rfidGap(30 * 8);
    sendOpT5557(0b10, 0, 0, data32, k+1);
    // Видалено Serial.print('*'); 
    delay(6);
  }
  delay(6);
  rfidGap(30 * 8);
  sendOpT5557(0b00);
  delay(4);
  result = readEM_Marie(addr);
  TCCR2A &=0b00111111;
  for (byte i = 0; i < 8; i++)
    if (addr[i] != buf[i]) { result = false; break; }
  if (!result){
    // Видалено Serial.println(F("Copy error"));
    OLED_printError(latinText("Copy error"));
    Sd_ErrorBeep();
  } else {
    // Видалено Serial.println(F("Key copied successfully"));
    OLED_printError(latinText("Key copied"), false);
    Sd_ReadOK();
    delay(2000);
  }
  digitalWrite(R_Led, HIGH);
  return result;  
}

emRWType getRfidRWtype(){
  unsigned long data32, data33; byte buf[4] = {0, 0, 0, 0}; 
  rfidACsetOn();
  delay(13);
  rfidGap(30 * 8);
  sendOpT5557(0b11, 0, 0, 0, 1);
  if (!T5557_blockRead(buf)) return rwUnknown; 
  data32 = (unsigned long)buf[0]<<24 | (unsigned long)buf[1]<<16 | (unsigned long)buf[2]<<8 | (unsigned long)buf[3];
  delay(4);
  rfidGap(20 * 8);
  data33 = 0b00000000000101001000000001000000 | (rfidUsePWD << 4);
  sendOpT5557(0b10, 0, 0, data33, 0);
  delay(4);
  rfidGap(30 * 8);
  sendOpT5557(0b11, 0, 0, 0, 1);
  if (!T5557_blockRead(buf)) return rwUnknown; 
  data33 = (unsigned long)buf[0]<<24 | (unsigned long)buf[1]<<16 | (unsigned long)buf[2]<<8 | (unsigned long)buf[3];
  sendOpT5557(0b00, 0, 0, 0, 0);
  delay(6);
  if (data32 != data33) return rwUnknown;    
  // Видалено Serial.print(F(" The rfid RW-key is T5557. Vendor ID is "));
  // Serial.println(data32, HEX);
  return T5557;
}

bool write2rfid(){
  bool Check = true;
  if (searchEM_Marine(false)) {
    for (byte i = 0; i < 8; i++)
      if (addr[i] != currentKey.id[i]) { Check = false; break; }
    if (Check) {
      digitalWrite(R_Led, LOW); 
      // Видалено Serial.println(F(" same key. No write needed."));
      OLED_printError(latinText("Same key"));
      Sd_ErrorBeep();
      digitalWrite(R_Led, HIGH);
      delay(1000);
      return false;
    }
  }
  emRWType rwType = getRfidRWtype();
  // Видалено if (rwType != rwUnknown) Serial.print(F("\n Burning rfid ID: "));
  switch (rwType){
    case T5557: return write2rfidT5557(currentKey.id); break;
    case rwUnknown: break;
  }
  return false;
}

void SendEM_Marine(byte* buf){ 
  TCCR2A &=0b00111111;
  digitalWrite(FreqGen, LOW);
  delay(20);
  for (byte k = 0; k<10; k++){
    for (byte i = 0; i<8; i++){
      for (byte j = 0; j<8; j++){
        if (1 & (buf[i]>>(7-j))) {
          pinMode(FreqGen, INPUT);
          delayMicroseconds(250);
          pinMode(FreqGen, OUTPUT); 
          delayMicroseconds(250);
        } else {
          pinMode(FreqGen, OUTPUT);
          delayMicroseconds(250);
          pinMode(FreqGen, INPUT);
          delayMicroseconds(250);
        }
      }
    }
  }  
}

void SendDallas(byte* buf){
  startDallasEmulation(buf);
  
  // Триваємо в режимі емуляції поки активний BlueMode
  unsigned long lastHubPoll = 0;
  
  while (copierMode == md_blueMode && emulationActive) {
    if (millis() - lastHubPoll > 1) {
      hub.poll();
      lastHubPoll = millis();
    }
    
    // Перевіряємо кнопку MODE для виходу
    if (digitalRead(BTN_MODE) == LOW) {
      delay(50);
      if (digitalRead(BTN_MODE) == LOW) {
        stopDallasEmulation();
        copierMode = md_read;
        clearLed();
        digitalWrite(G_Led, HIGH);
        OLED_printKey(currentKey.id);
        Sd_WriteStep();
        break;
      }
    }
    
    // Блимаємо синім світлодіодом
    digitalWrite(B_Led, (millis() / 500) % 2);
    
    delay(1);
  }
  
  // Якщо вийшли через таймаут
  if (emulationActive) {
    stopDallasEmulation();
    copierMode = md_read;
    clearLed();
    digitalWrite(G_Led, HIGH);
    OLED_printKey(currentKey.id);
  }
}

void BM_SendKey(byte* buf){
  switch (keyType){
    case keyEM_Marine: SendEM_Marine(buf); break;
    default: SendDallas(buf); break;
  }
}

void clearLed(){
  digitalWrite(R_Led, LOW);
  digitalWrite(G_Led, LOW);
  digitalWrite(B_Led, LOW);  
}

void timerIsr() {
  // Таймер використовується для інших цілей
}

// ==================== SETUP ====================
void setup() {
  pinMode(Luse_Led, OUTPUT); digitalWrite(Luse_Led, HIGH);
  
  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  
  oled.init();
  oled.setScale(1);
  pinMode(speakerPin, OUTPUT);
  pinMode(ACpin, INPUT);
  pinMode(R_Led, OUTPUT); pinMode(G_Led, OUTPUT); pinMode(B_Led, OUTPUT);
  clearLed();
  pinMode(FreqGen, OUTPUT);                               
  Serial.begin(115200);  // Поки залишаємо для команд 's' та 'e'
  
  oled.clear();
  oled.home();
  oled.println(latinText("MultiKey------Rfid"));
  oled.println          ("-----------iButton");
  oled.println          ("------------------");
  oled.println          ("Read Write Emulate");
  oled.update();
  
  Sd_StartOK();
  
  EEPROM_key_count = EEPROM[0];
  if (EEPROM_key_count > maxKeyCount) EEPROM_key_count = 0;
  
  if (EEPROM_key_count != 0 ) {
    EEPROM_key_index = EEPROM[1];
    if (EEPROM_key_index > EEPROM_key_count || EEPROM_key_index == 0) EEPROM_key_index = 1;
    
    // Видалено Serial.print(F("Key from ROM: "));
    EEPROM_get_key(EEPROM_key_index, currentKey.id);
    // for (byte i = 0; i < 8; i++) {
    //   Serial.print(currentKey.id[i], HEX); Serial.print(F(":"));  
    // }
    // Serial.println();
    delay(3000);
    OLED_printKey(currentKey.id);
    copierMode = md_read;
    digitalWrite(G_Led, HIGH);
  } else {
    oled.clear();
    oled.home();
    oled.println(latinText("No keys in ROM"));
    oled.update();  
  }

  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);
  digitalWrite(Luse_Led, !digitalRead(Luse_Led));
  
  // Додавання 11 універсальних ключів з назвами
  KeyData newKey;
  
  for (int i = 0; i < 11; i++) {
    memcpy(newKey.id, ReadID[i], 8);
    strcpy(newKey.name, keyNames[i]);
    
    if (EPPROM_AddKey(newKey)) {
      OLED_printError(latinText("Uni key added"), false);
      Sd_ReadOK();
      delay(300);
    }
  }
}

// ==================== LOOP ====================
unsigned long stTimer = millis();

void loop() {
  char echo = Serial.read();
  
  if (echo == 's') {
    Serial.print(F("RFID Mode: "));
    Serial.println(copierMode == md_read ? "Read" : 
                   copierMode == md_write ? "Write" :
                   copierMode == md_blueMode ? "BlueMode" : "Off");
    
    Serial.print(F("A0 value: "));
    int val = analogRead(A0);
    Serial.print(val);
    Serial.print(" (");
    Serial.print(val * (5.0 / 1023.0), 2);
    Serial.println("V)");
  }
  
  if (echo == 'e'){
    oled.clear();
    oled.home();
    oled.println(latinText("ROM cleared!"));
    Serial.println(F("EEPROM cleared"));
    EEPROM.update(0, 0); EEPROM.update(1, 0);
    EEPROM_key_count = 0; EEPROM_key_index = 0;
    Sd_ReadOK();
    oled.update();
  }
  
  bool modeReading = digitalRead(BTN_MODE);
  bool leftReading = digitalRead(BTN_LEFT);
  bool rightReading = digitalRead(BTN_RIGHT);
  
  // Обробка кнопки MODE
  if (modeReading == LOW && !modeButtonPressed) {
    delay(debounceDelay);
    if (digitalRead(BTN_MODE) == LOW) {
      modeButtonPressed = true;
      
      if (emulationActive) {
        stopDallasEmulation();
      }
      
      // Коротке натискання - перемикання режиму
      if (millis() - holdTimeStart < 1000) {
        switch (copierMode){
          case md_empty: Sd_ErrorBeep(); break;
          case md_read: copierMode = md_write; clearLed(); digitalWrite(R_Led, HIGH);  break;
          case md_write: copierMode = md_blueMode; clearLed(); digitalWrite(B_Led, HIGH); 
            digitalWrite(Luse_Led, !digitalRead(Luse_Led)); break;
          case md_blueMode: copierMode = md_read; clearLed(); digitalWrite(G_Led, HIGH); 
            digitalWrite(Luse_Led, !digitalRead(Luse_Led)); break;
        }
        OLED_printKey(currentKey.id);
        // Видалено Serial.print(F("Mode: ")); Serial.println(copierMode);
        Sd_WriteStep();
      }
    }
  }
  
  // Обробка кнопки LEFT
  if (leftReading == LOW && !leftButtonPressed && EEPROM_key_count > 0 && !emulationActive) {
    delay(debounceDelay);
    if (digitalRead(BTN_LEFT) == LOW) {
      leftButtonPressed = true;
      EEPROM_key_index--;
      if (EEPROM_key_index < 1) EEPROM_key_index = EEPROM_key_count;
      EEPROM_get_key(EEPROM_key_index, currentKey.id);
      OLED_printKey(currentKey.id);
      Sd_WriteStep();
    }
  }
  
  // Обробка кнопки RIGHT
  if (rightReading == LOW && !rightButtonPressed && EEPROM_key_count > 0 && !emulationActive) {
    delay(debounceDelay);
    if (digitalRead(BTN_RIGHT) == LOW) {
      rightButtonPressed = true;
      EEPROM_key_index++;
      if (EEPROM_key_index > EEPROM_key_count) EEPROM_key_index = 1;
      EEPROM_get_key(EEPROM_key_index, currentKey.id);
      OLED_printKey(currentKey.id);
      Sd_WriteStep();
    }
  }
  
  if (modeReading == HIGH && modeButtonPressed) modeButtonPressed = false;
  if (leftReading == HIGH && leftButtonPressed) leftButtonPressed = false;
  if (rightReading == HIGH && rightButtonPressed) rightButtonPressed = false;
  
  // Обробка утримання кнопки MODE
  if (modeReading == LOW && !emulationActive) {
    if (!holdMode) {
      holdTimeStart = millis();
      holdMode = true;
    } else if (millis() - holdTimeStart > 1000) {
      if (digitalRead(BTN_MODE) == LOW) {
        if (copierMode == md_read) {
          // В режимі читання - збереження ключа
          KeyData newKey;
          memcpy(newKey.id, currentKey.id, 8);
          String nameStr = getKeyName(currentKey.id);
          strcpy(newKey.name, nameStr.c_str());
          
          if (EPPROM_AddKey(newKey)) {
            OLED_printError(latinText("Key saved"), false);
            Sd_ReadOK();
          } else {
            OLED_printError(latinText("Key exists"));
            Sd_ErrorBeep();
          }
        } 
        else if (copierMode == md_write) {
          // В режимі запису - видалення ключа
          if (EEPROM_key_count > 0) {
            // Показуємо повідомлення про видалення
            oled.clear();
            oled.home();
            oled.println("Hold to DELETE");
            oled.println(String(EEPROM_key_index) + "/" + String(EEPROM_key_count));
            oled.println("Release to cancel");
            oled.update();
            
            delay(500); // Коротка пауза для читання повідомлення
            
            // Викликаємо функцію видалення з підтвердженням
            deleteCurrentKey();
          }
        }
        
        OLED_printKey(currentKey.id);
        holdMode = false;
      }
    }
  } else {
    holdMode = false;
  }
  
  if (millis() - stTimer < 100) return;
  stTimer = millis();
  
  switch (copierMode){
      case md_empty: case md_read: 
        if (!emulationActive && (searchCyfral() || searchMetacom() || searchEM_Marine() || searchIbutton() )){
          Sd_ReadOK();
          copierMode = md_read;
          digitalWrite(G_Led, HIGH);
          if (indxKeyInROM(currentKey.id) == 0) OLED_printKey(currentKey.id, 1);
            else OLED_printKey(currentKey.id, 3);
          } 
        break;
      case md_write:
        if (!emulationActive) {
          if (keyType == keyEM_Marine) write2rfid();
            else write2iBtn(); 
        }
        break;
      case md_blueMode: 
        if (!emulationActive) {
          OLED_printKey(currentKey.id, 4);
          BM_SendKey(currentKey.id);
        }
        break;
    }
}
