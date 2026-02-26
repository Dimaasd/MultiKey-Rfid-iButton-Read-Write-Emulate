/*
  Sketch for "MultiKey-Rfid-iButton-Read-Write-Emulate"
  RFID Key Copier with OLED display and 42 keys storage in EEPROM
  Hardware based on Arduino Nano
  v 3.2 fix cyfral bug
  v 3.3 added RFID output in format FF:A9:8A:A1:81:D6:5E:8C: ( id 86 key 1513154234) Type: EM-Marine
  v 3.4 replaced encoder with three buttons control
  v 3.5 Simplified key adding - now keys are added through array with names
  v 3.6 Removed Luse_Led and all Serial outputs except e command for clearing
  v 3.7 Key name displayed in 4th line, key info in EEPROM - in first line
  v 4.0 Added iButton and RFID emulation
  v 4.1 Added key deletion via MODE + LEFT
*/

// Settings
#include <OneWire.h>
#include <OneWireSlave.h>
#include <OneWireHub.h>     // Added for iButton emulation
#include <DS2401.h>         // Added for Dallas emulation
#include "pitches.h"
#include <EEPROM.h>
#include "TimerOne.h"
#include "GyverOLED.h"

//settings
#define rfidUsePWD 0        // key uses password for change
#define rfidPWD 123456      // key password
#define rfidBitRate 2       // RFID exchange speed in kbps

//pins
#define iButtonPin A3      // ibutton data line
#define iBtnEmulPin 10     // emulator ibutton line
#define R_Led 8            // RGB Led
#define G_Led 2
#define B_Led 12
#define ACpin 6            // Input Ain0 of analog comparator 0.1V for EM-Marie 
#define speakerPin 3       // Speaker, buzzer, beeper
#define FreqGen 11         // 125 kHz generator
#define BTN_MODE A1         // Mode switch button read/write
#define BTN_LEFT A2         // "Left" button (previous key)
#define BTN_RIGHT A0       // "Right" button (next key)

OneWire ibutton (iButtonPin);
OneWireSlave iBtnEmul(iBtnEmulPin);       // iButton emulator for BlueMode

// ============ FOR EMULATION ============
auto hub = OneWireHub(iBtnEmulPin);      // Main OneWire hub for iButton emulation
DS2401* currentDS = nullptr;              // Current device for Dallas emulation
bool emulating = false;                   // Is emulation active
// =====================================

byte maxKeyCount;                         // maximum number of keys that fit in EEPROM, but not > 42
byte EEPROM_key_count;                    // number of keys 0..maxKeyCount stored in EEPROM
byte EEPROM_key_index = 0;                // 1..EEPROM_key_count number of the last key written to EEPROM  
byte addr[8];                             // temporary buffer
byte keyID[8];                            // key ID for writing
byte rfidData[5];                         // significant frid em-marine data
byte halfT;                               // half period for metacom

// Button processing variables
unsigned long lastDebounceTime[3] = {0, 0, 0};
unsigned long debounceDelay = 50;
unsigned long holdTimeStart = 0;
unsigned long comboTimeStart = 0;          // Combination start time
bool holdMode = false;
bool comboMode = false;                    // Combination waiting mode
bool lastBtnState[3] = {HIGH, HIGH, HIGH};
bool btnState[3] = {HIGH, HIGH, HIGH};

// ============ SIMPLIFIED KEY ADDING ============
// Structure for storing key with name
struct UniversalKey {
  const char* name;        // Key name for OLED display
  byte data[8];            // Key data
};

  // Array of universal keys with names
UniversalKey universalKeys[] = {
  // Dallas/Metakom/Cyfral keys (start with 0x01)
  {"Univer 1F",              {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F, 0xFF, 0x00}}, // - 1 Univer 1F
  {"Univer 2F",              {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x2F, 0xFF, 0x00}}, // - 2 Univer 2F
//  {"UK-1 Metakom 2003",      {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x9B}}, // - 3 UK-1 Metakom 2003
//  {"UK-2 Vizit",             {0x01, 0xBE, 0x40, 0x11, 0x5A, 0x36, 0x00, 0xE1}}, // - 4 UK-2 Vizit
//  {"UK-3 Cyfral",            {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3D}}, // - 5 UK-3 Cyfral
//  {"Standard Universal",     {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x2F}}, // - 6 Standard universal key
//  {"Ordinary",               {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00}}, // - 7 Ordinary
//  {"Old Door Work",          {0x01, 0x00, 0x00, 0x00, 0x00, 0x90, 0x19, 0xFF}}, // - 8 Works great on old intercoms
//  {"Cyfral Metakom 1",       {0x01, 0x53, 0xD4, 0xFE, 0x00, 0x00, 0x7E, 0x88}}, // - 9 Cyfral, Metakom
//  {"Cyfral Metakom 2",       {0x01, 0x53, 0xD4, 0xFE, 0x00, 0x00, 0x7E, 0x00}}, // - 10 Cyfral, Metakom
  {"Metakom 98%",            {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x14}}, // - 11 Opens 98% Metakom and some Cyfral
//  {"Cyfral Filter",          {0x01, 0xFF, 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00}}, // - 12 Cyfral intercoms + filter and protection
//  {"Metakom",                {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00}}, // - 13 Metakom
  {"Metakom 95%",            {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0xA0}}, // - 14 Metakom 95%
//  {"KeyMan",                 {0x01, 0x00, 0xBE, 0x11, 0xAA, 0x00, 0x00, 0xFB}}, // - 15 KeyMan intercoms
//  {"Vizit KeyMan",           {0x01, 0xBE, 0x40, 0x11, 0x0A, 0x00, 0x00, 0x1D}}, // - 16 tested works Vizit sometimes KeyMan
//  {"Vizit 99%",              {0x01, 0x53, 0xD4, 0xFE, 0x00, 0x00, 0x00, 0x6F}}, // - 17 Vizit intercoms - up to 99%
//  {"Vizit 99% Plus",         {0x01, 0xBE, 0x40, 0x11, 0x5A, 0x36, 0x00, 0x00}}, // - 18 Vizit 99%
//  {"Forward",                {0x01, 0x76, 0xB8, 0x2E, 0x0F, 0x00, 0x00, 0x5C}}, // - 19 Forward intercoms
//  {"Eltis 90%",              {0x01, 0xA9, 0xE4, 0x3C, 0x09, 0x00, 0x00, 0x00}}, // - 20 Eltis intercoms - up to 90%
//  {"Vizit Tested 1",         {0x01, 0xBE, 0x40, 0x11, 0x5A, 0x56, 0x00, 0xBB}}, // - 21 tested works
//  {"Vizit Tested 2",         {0x01, 0xBE, 0x40, 0x11, 0x00, 0x00, 0x00, 0x77}}, // - 22 tested works
  {"Metakom 22",             {0x01, 0xAA, 0x69, 0xC9, 0x36, 0x00, 0x00, 0xE9}}, // Your original Metakom key
//  {"EM-Marine 76",           {0xFF, 0xA9, 0x8A, 0xB8, 0x13, 0x14, 0x8F, 0xB4}}, // - EM-Marine 76
//  {"EM-Marine 42a",          {0xFF, 0xA9, 0x8A, 0xA7, 0x65, 0xBC, 0x16, 0xE6}}, // - EM-Marine 42a
//  {"EM-Marine 42b",          {0xFF, 0xA9, 0x8A, 0xA7, 0x65, 0xB9, 0x3D, 0x9C}}, // - EM-Marine 42b
  {"Garage EM-Marine",       {0xFF, 0x94, 0x60, 0x05, 0xF7, 0xBF, 0x5D, 0x30}}  // Your original Garage EM-Marine key
};

// Number of keys in array
const int UNIVERSAL_KEYS_COUNT = sizeof(universalKeys) / sizeof(UniversalKey);
// ==================================================

enum emRWType {rwUnknown, TM01, RW1990_1, RW1990_2, TM2004, T5557, EM4305};               // blank type
enum emkeyType {keyUnknown, keyDallas, keyTM2004, keyCyfral, keyMetacom, keyEM_Marine};    // original key type  
emkeyType keyType;
enum emMode {md_empty, md_read, md_write, md_blueMode};               // copier operating mode
emMode copierMode = md_empty;

GyverOLED<SSD1306_128x32, OLED_NO_BUFFER> oled; // Using without buffer

//***************** sounds ****************
void Sd_StartOK() {   // sound "Successful startup"
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

void Sd_ReadOK() {  // sound OK
  for (int i=400; i<6000; i=i*1.5) { tone(speakerPin, i); delay(20); }
  noTone(speakerPin);
}

void Sd_WriteStep(){  // sound "next step"
  for (int i=2500; i<6000; i=i*1.5) { tone(speakerPin, i); delay(10); }
  noTone(speakerPin);
}

void Sd_ErrorBeep() {  // sound "ERROR"
  for (int j=0; j <3; j++){
    for (int i=1000; i<2000; i=i*1.1) { tone(speakerPin, i); delay(10); }
    delay(50);
    for (int i=1000; i>500; i=i*1.9) { tone(speakerPin, i); delay(10); }
    delay(50);
  }
  noTone(speakerPin);
}

void Sd_DeleteOK() {  // sound "Deletion successful"
  tone(speakerPin, NOTE_C7); delay(100);
  tone(speakerPin, NOTE_E7); delay(100);
  tone(speakerPin, NOTE_G7); delay(100);
  tone(speakerPin, NOTE_C8); delay(200);
  noTone(speakerPin);
}

void Sd_ComboStart() { // sound "Combination activated"
  tone(speakerPin, NOTE_C7); delay(50);
  tone(speakerPin, NOTE_E7); delay(50);
  tone(speakerPin, NOTE_G7); delay(50);
  noTone(speakerPin);
}

// Function to get key name (if exists)
String getKeyName(byte buf[8]) {
  for (int i = 0; i < UNIVERSAL_KEYS_COUNT; i++) {
    bool match = true;
    for (int j = 0; j < 8; j++) {
      if (universalKeys[i].data[j] != buf[j]) {
        match = false;
        break;
      }
    }
    if (match) {
      return String(universalKeys[i].name);
    }
  }
  return "";
}

// Function for displaying RFID information - 4 lines
void OLED_printRFIDInfo(byte buf[8], byte msgType = 0) {
  oled.clear();
  oled.home();
  
  // First line - key info in EEPROM
  String st1 = "";
  switch (msgType){
    case 0: st1 = "Key " + String(EEPROM_key_index) + " of " + String(EEPROM_key_count) + " in Memory"; break;      
    case 1: st1 = "Hold to save";  break; 
    case 3: st1 = "Key " + String(indxKeyInROM(buf)) + " exists in Memory";  break;
    case 4: st1 = "Delete key?"; break;
    case 5: st1 = "Key deleted!"; break;
  }
  oled.println(st1);
  
  // Second line - HEX values with colon
  String st2 = "";
  for (byte i = 0; i < 8; i++) {
    if (buf[i] < 0x10) st2 += "0";
    st2 += String(buf[i], HEX);
    if (i < 7) st2 += "";
  }
  oled.println(st2);
  
  // Third line - id and key
  String st3 = "";
  if (vertEvenCheck(buf)) {
    unsigned long keyNum = (unsigned long)rfidData[1]<<24 | (unsigned long)rfidData[2]<<16 | 
                           (unsigned long)rfidData[3]<<8 | (unsigned long)rfidData[4];
    st3 = "id " + String(rfidData[0]) + " key " + String(keyNum);
  } else {
    st3 = "(id ? key ?)";
  }
  oled.println(st3);
  
  // Fourth line - key name (if exists) or key type
  String st4 = "";
  String keyName = getKeyName(buf);
  if (keyName.length() > 0) {
    st4 = keyName;
  } else {
    st4 = "Type: ";
    switch (keyType){
      case keyDallas: st4 += "Dallas"; break;      
      case keyCyfral: st4 += "Cyfral"; break;  
      case keyMetacom: st4 += "Metakom"; break;             
      case keyEM_Marine: st4 += "EM-Marine"; break;
      case keyTM2004: st4 += "TM2004"; break;
      case keyUnknown: st4 += "Unknown"; break;
    }
  }
  oled.println(st4);
  
  oled.update();
}

// Function for displaying regular keys - 4 lines
void OLED_printKey(byte buf[8], byte msgType = 0){
  // For RFID keys use special format
  if (keyType == keyEM_Marine) {
    OLED_printRFIDInfo(buf, msgType);
    return;
  }
  
  oled.clear();
  oled.home();
  
  // First line - key info in EEPROM
  String st1 = "";
  switch (msgType){
    case 0: st1 = "Key " + String(EEPROM_key_index) + " of " + String(EEPROM_key_count) + " in Memory"; break;      
    case 1: st1 = "Hold to save";  break; 
    case 3: st1 = "Key " + String(indxKeyInROM(buf)) + " exists in Memory";  break;
    case 4: st1 = "Delete key?"; break;
    case 5: st1 = "Key deleted!"; break;
  }
  oled.println(st1);
  
  // Second line - HEX values with colon
  String st2 = "";
  for (byte i = 0; i < 8; i++) {
    if (buf[i] < 0x10) st2 += "0";
    st2 += String(buf[i], HEX);
    if (i < 7) st2 += "";
  }
  oled.println(st2);
  
  // Third line - empty (for alignment) or key type
  String st3 = "";
  st3 = "Type: ";
  switch (keyType){
    case keyDallas: st3 += "Dallas"; break;      
    case keyCyfral: st3 += "Cyfral"; break;  
    case keyMetacom: st3 += "Metakom"; break;             
    case keyTM2004: st3 += "TM2004"; break;
    case keyUnknown: st3 += "Unknown"; break;
  }
  oled.println(st3);
  
  // Fourth line - key name (if exists)
  String st4 = getKeyName(buf);
  oled.println(st4);
  
  oled.update();
}

void OLED_printError(String st, bool err = true){
  oled.clear();
  oled.home();
  if (err) oled.println("Error!");
    else oled.println("OK");
  oled.println(st);  
  oled.update();
}

// Function to add all universal keys
void addAllUniversalKeys() {
  for (int i = 0; i < UNIVERSAL_KEYS_COUNT; i++) {
    if (EPPROM_AddKey(universalKeys[i].data)) {
      oled.clear();
      oled.home();
      oled.println("Key added:");
      oled.println(String(universalKeys[i].name));
      oled.update();
      Sd_ReadOK();
      delay(1000);
    }
  }
}

// ============ KEY DELETION FUNCTION ============
void deleteCurrentKey() {
  if (EEPROM_key_count == 0) {
    OLED_printError("No keys");
    Sd_ErrorBeep();
    delay(1000);
    return;
  }
  
  // Show confirmation
  OLED_printKey(keyID, 4);
  
  // Wait 2 seconds for confirmation
  unsigned long confirmStart = millis();
  bool confirmed = false;
  
  while (millis() - confirmStart < 2000) {
    // If MODE button pressed again - confirm deletion
    if (digitalRead(BTN_MODE) == LOW) {
      delay(50);
      if (digitalRead(BTN_MODE) == LOW) {
        confirmed = true;
        break;
      }
    }
    delay(10);
  }
  
  if (confirmed) {
    // Delete current key
    byte buf1[8];
    
    // Shift all keys after current one back by one position
    for (byte j = EEPROM_key_index; j < EEPROM_key_count; j++) {
      EEPROM.get((j + 1) * sizeof(buf1), buf1);
      EEPROM.put(j * sizeof(buf1), buf1);
    }
    
    // Decrease key counter
    EEPROM_key_count--;
    EEPROM.update(0, EEPROM_key_count);
    
    // Adjust index
    if (EEPROM_key_index > EEPROM_key_count) {
      EEPROM_key_index = EEPROM_key_count;
    }
    if (EEPROM_key_index < 1 && EEPROM_key_count > 0) {
      EEPROM_key_index = 1;
    }
    EEPROM.update(1, EEPROM_key_index);
    
    // Update current key
    if (EEPROM_key_count > 0) {
      EEPROM_get_key(EEPROM_key_index, keyID);
    }
    
    // Show result
    OLED_printKey(keyID, 5);
    Sd_DeleteOK();
    delay(1500);
    
    // Return to normal display
    if (EEPROM_key_count > 0) {
      OLED_printKey(keyID);
    } else {
      oled.clear();
      oled.home();
      oled.println("Memory empty");
      oled.update();
    }
  } else {
    // Cancel deletion
    OLED_printKey(keyID);
  }
}
// ==================================================

// ============ EMULATION FUNCTIONS ============

// CRC8 for iButton
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

// Start iButton emulation (Dallas, Cyfral, Metakom)
void startIButtonEmulation() {
  // Remove previous device if existed
  if (currentDS != nullptr) {
    hub.detach(*currentDS);
    delete currentDS;
    currentDS = nullptr;
  }
  
  // Fix CRC for Dallas keys
  byte emulKey[8];
  memcpy(emulKey, keyID, 8);
  
  if (keyType == keyDallas || keyType == keyTM2004) { // Dallas key
    emulKey[7] = onewire_crc8(emulKey, 7);
  }
  
  // For Cyfral and Metakom - emulate as Dallas with correct first byte
  if (keyType == keyCyfral) {
    emulKey[0] = 0x01; // Pretend it's Dallas
    emulKey[7] = onewire_crc8(emulKey, 7);
  }
  if (keyType == keyMetacom) {
    emulKey[0] = 0x01; // Pretend it's Dallas
    emulKey[7] = onewire_crc8(emulKey, 7);
  }
  
  // Create new device
  currentDS = new DS2401(emulKey[0], emulKey[1], emulKey[2], emulKey[3], 
                         emulKey[4], emulKey[5], emulKey[6]);
  
  hub.attach(*currentDS);  // Connect to hub
  
  // Show on display
  oled.clear();
  oled.home();
  oled.println(">> iButton Emulation <");
  oled.println("Key:");
  
  char buf[20];
  sprintf(buf, "%02X %02X %02X %02X", emulKey[0], emulKey[1], emulKey[2], emulKey[3]);
  oled.println(buf);
  sprintf(buf, "%02X %02X %02X %02X", emulKey[4], emulKey[5], emulKey[6], emulKey[7]);
  oled.println(buf);
  oled.println("MODE=stop");
  oled.update();
  
  emulating = true;
  digitalWrite(B_Led, HIGH); // Blue LED - emulation mode
}

// Start RFID emulation (EM-Marine)
void startRFIDEmulation() {
  // Show on display
  oled.clear();
  oled.home();
  oled.println(">>  RFID Emulation  <<");
  oled.println("Key:");
  
  char buf[20];
  sprintf(buf, "%02X %02X %02X %02X", keyID[0], keyID[1], keyID[2], keyID[3]);
  oled.println(buf);
  sprintf(buf, "%02X %02X %02X %02X", keyID[4], keyID[5], keyID[6], keyID[7]);
  oled.println(buf);
  
  if (vertEvenCheck(keyID)) {
    oled.println("id " + String(rfidData[0]) + " key " + String((unsigned long)rfidData[1]<<24 | (unsigned long)rfidData[2]<<16 | (unsigned long)rfidData[3]<<8 | (unsigned long)rfidData[4]));
  }
  oled.println("MODE=stop");
  oled.update();
  
  emulating = true;
  digitalWrite(B_Led, HIGH); // Blue LED - emulation mode
}

// Send RFID key (EM-Marine emulation)
void sendRFIDKey(byte* buf) {
  // Turn on 125kHz generator
  rfidACsetOn();
  
  // Configure timer for modulation
  TCCR2A &=0b00111111; // disable PWM for modulation
  digitalWrite(FreqGen, LOW);
  delay(20);
  
  // Send key 10 times (like typical RFID transponder)
  for (byte k = 0; k < 10; k++) {
    for (byte i = 0; i < 8; i++) {
      for (byte j = 0; j < 8; j++) {
        if (1 & (buf[i] >> (7 - j))) {
          // Bit 1 - modulation (short field off)
          pinMode(FreqGen, INPUT);
          delayMicroseconds(250);
          pinMode(FreqGen, OUTPUT);
          delayMicroseconds(250);
        } else {
          // Bit 0 - no modulation
          pinMode(FreqGen, OUTPUT);
          delayMicroseconds(250);
          pinMode(FreqGen, INPUT);
          delayMicroseconds(250);
        }
      }
    }
  }
  
  // Turn off generator
  TCCR2A &=0b00111111;
}

// Stop emulation
void stopEmulation() {
  emulating = false;
  
  // Stop iButton emulation if it was active
  if (currentDS != nullptr) {
    hub.detach(*currentDS);
    delete currentDS;
    currentDS = nullptr;
  }
  
  // Turn off RFID generator
  TCCR2A &=0b00111111;
  
  oled.clear();
  oled.home();
  oled.println(">>>  STOPPED  <<<");
  oled.update();
  delay(500);
  
  OLED_printKey(keyID);
  digitalWrite(B_Led, LOW);
}

// ========================================

void setup() {
  // Button pins setup
  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  
  oled.init(); // Display initialization
  oled.setScale(1); // Set text scale
  pinMode(speakerPin, OUTPUT);
  pinMode(ACpin, INPUT);
  pinMode(R_Led, OUTPUT); pinMode(G_Led, OUTPUT); pinMode(B_Led, OUTPUT);
  clearLed();
  pinMode(FreqGen, OUTPUT);                               
  Serial.begin(115200);
  
  oled.clear();
  oled.home();
  oled.println("MultiKey------Rfid");
  oled.println("-----------iButton");
  oled.println("------------------");
  oled.println("Read Write Emulate");
  oled.update();
  
  Sd_StartOK();
  EEPROM_key_count = EEPROM[0];
  maxKeyCount = EEPROM.length() / 8 - 1; if (maxKeyCount > 42) maxKeyCount = 42;
  
  if (EEPROM_key_count > maxKeyCount) EEPROM_key_count = 0;
  if (EEPROM_key_count != 0 ) {
    EEPROM_key_index = EEPROM[1];
    EEPROM_get_key(EEPROM_key_index, keyID);
    delay(3000);
    OLED_printKey(keyID);
    copierMode = md_read;
    digitalWrite(G_Led, HIGH);
  } else {
    oled.clear();
    oled.home();
    oled.println("No keys in Memory yet");
    oled.update();  
  }

  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);
  
  // Adding all universal keys with one call
  addAllUniversalKeys();
}

void timerIsr() {
  // Timer used for other purposes
}

void clearLed(){
  digitalWrite(R_Led, LOW);
  digitalWrite(G_Led, LOW);
  digitalWrite(B_Led, LOW);  
}

byte indxKeyInROM(byte buf[]){
  byte buf1[8]; bool eq = true;
  for (byte j = 1; j<=EEPROM_key_count; j++){
    EEPROM.get(j*sizeof(buf1), buf1);
    for (byte i = 0; i < 8; i++) 
      if (buf1[i] != buf[i]) { eq = false; break;}
    if (eq) return j;
    eq = true;
  }
  return 0;
}

bool EPPROM_AddKey(byte buf[]){
  byte buf1[8]; byte indx;
  indx = indxKeyInROM(buf);
  if ( indx != 0) { 
    EEPROM_key_index = indx;
    EEPROM.update(1, EEPROM_key_index);
    return false; 
  }
  if (EEPROM_key_count <= maxKeyCount) EEPROM_key_count++;
  if (EEPROM_key_count < maxKeyCount) EEPROM_key_index = EEPROM_key_count;
    else EEPROM_key_index++;
  if (EEPROM_key_index > EEPROM_key_count) EEPROM_key_index = 1;
  
  for (byte i = 0; i < 8; i++) {
    buf1[i] = buf[i];
  }
  EEPROM.put(EEPROM_key_index*sizeof(buf1), buf1);
  EEPROM.update(0, EEPROM_key_count);
  EEPROM.update(1, EEPROM_key_index);
  return true;
}

void EEPROM_get_key(byte EEPROM_key_index1, byte buf[8]){
  byte buf1[8];
  int address = EEPROM_key_index1*sizeof(buf1);
  if (address > EEPROM.length()) return;
  EEPROM.get(address, buf1);
  for (byte i = 0; i < 8; i++) buf[i] = buf1[i];
  keyType = getKeyType(buf1);
}

emkeyType getKeyType(byte* buf){
  if (buf[0] == 0x01) return keyDallas;
  if ((buf[0] >> 4) == 0b0001) return keyCyfral;
  if ((buf[0] >> 4) == 0b0010) return keyMetacom;
  if ((buf[0] == 0xFF) && vertEvenCheck(buf)) return keyEM_Marine;
  return keyUnknown;
}

// Function for reading button state with debounce
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
  return btnState[btnIndex];
}

//*************** dallas **************
emRWType getRWtype(){    
   byte answer;
  ibutton.reset(); ibutton.write(0xD1);
  ibutton.write_bit(1);
  delay(10); pinMode(iButtonPin, INPUT);
  ibutton.reset(); ibutton.write(0xB5);
  answer = ibutton.read();
  if (answer == 0xFE){
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
    ibutton.reset();
    return TM2004;
  }
  ibutton.reset();
  return TM01;
}

bool write2iBtnTM2004(){
  byte answer; bool result = true;
  ibutton.reset();
  ibutton.write(0x3C);
  ibutton.write(0x00); ibutton.write(0x00);
  for (byte i = 0; i<8; i++){
    digitalWrite(R_Led, !digitalRead(R_Led));
    ibutton.write(keyID[i]);
    answer = ibutton.read();
    delayMicroseconds(600); ibutton.write_bit(1); delay(50);
    pinMode(iButtonPin, INPUT);
    Sd_WriteStep();
    if (keyID[i] != ibutton.read()) { result = false; break;}
  } 
  if (!result){
    ibutton.reset();
    OLED_printError("Copy error");
    oled.println("        key");
    Sd_ErrorBeep();
    digitalWrite(R_Led, HIGH);
    return false;    
  }
  ibutton.reset();
  OLED_printError("Key successfully", false);
  oled.println("      copied");
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
  if (bitCnt == 36) BurnByteMC(keyID);
  else for (byte i = 0; i< (bitCnt >> 3); i++){
    digitalWrite(R_Led, !digitalRead(R_Led));
    if (rwType == RW1990_1) BurnByte(~keyID[i]);
      else BurnByte(keyID[i]);
    Sd_WriteStep();
  }
  if (bitCnt == 64) {
      ibutton.write(rwCmd);
      ibutton.write_bit(!rwFlag);
      delay(5); pinMode(iButtonPin, INPUT);
    }
  digitalWrite(R_Led, LOW);       
  if (!dataIsBurningOK(bitCnt)){
    OLED_printError("Copy error");
    oled.println("        key");
    Sd_ErrorBeep();
    digitalWrite(R_Led, HIGH);
    return false;
  }
  if ((keyType == keyMetacom)||(keyType == keyCyfral)){
    ibutton.reset();
    if (keyType == keyCyfral) ibutton.write(0xCA);
      else ibutton.write(0xCB);
    ibutton.write_bit(1);
    delay(10); pinMode(iButtonPin, INPUT);
  }
  OLED_printError("Key successfully", false);
  oled.println("      copied");
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
    if (keyID[i] == buff[i]) Check++;
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
  for (byte i = 0; i < 8; i++) {
    if (keyID[i] == addr[i]) Check++;
  }
  if (Check == 8) {
    digitalWrite(R_Led, LOW); 
    OLED_printError("This is the same key");
    Sd_ErrorBeep();
    digitalWrite(R_Led, HIGH);
    delay(1000);
    return false;
  }
  emRWType rwType = getRWtype();
  if (rwType == TM2004) return write2iBtnTM2004();
    else return write2iBtnRW1990_1_2_TM01(rwType);
}

bool searchIbutton(){
  if (!ibutton.search(addr)) { 
    ibutton.reset_search(); 
    return false;
  }  
  for (byte i = 0; i < 8; i++) {
    keyID[i] = addr[i];
  }
  if (addr[0] == 0x01) {
    keyType = keyDallas;
    if (getRWtype() == TM2004) keyType = keyTM2004;
    if (OneWire::crc8(addr, 7) != addr[7]) {
      OLED_printError("CRC error!");
      Sd_ErrorBeep();
      digitalWrite(B_Led, HIGH);
      return false;
    }
    return true;
  }
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
    keyID[i] = addr[i];
  }
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
    keyID[i] = addr[i];
  }
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
  rfidACsetOn();            // turn on 125kHz generator and comparator
  delay(6);                // 13 ms for detector transients
  if (!readEM_Marie(addr)) {
    if (!copyKey) TCCR2A &=0b00111111;              // Turn off PWM COM2A (pin 11)
    digitalWrite(G_Led, gr);
    return rez;
  }
  rez = true;
  keyType = keyEM_Marine;
  for (byte i = 0; i<8; i++){
    if (copyKey) keyID[i] = addr [i];
  }
  
  if (!copyKey) TCCR2A &=0b00111111;              //Turn off PWM COM2A (pin 11)
  digitalWrite(G_Led, gr);
  return rez;
}

void TxBitRfid(byte data){
  if (data & 1) delayMicroseconds(54*8); 
    else delayMicroseconds(24*8);
  rfidGap(19*8);                       //write gap
}

void TxByteRfid(byte data){
  for(byte n_bit=0; n_bit<8; n_bit++){
    TxBitRfid(data & 1);
    data = data >> 1;                   // go to next bit
  }
}

void rfidGap(unsigned int tm){
  TCCR2A &=0b00111111;                //Turn off PWM COM2A 
  delayMicroseconds(tm);
  TCCR2A |= _BV(COM2A0);              // Turn on PWM COM2A (pin 11)      
}

bool T5557_blockRead(byte* buf){
  byte ti; byte j = 0, k=0;
  for (int i = 0; i<33; i++){                           // read start 0 and 32 significant bits
    ti = ttAComp(2000);
    if (ti == 2)  break;                                //timeout
    if ( ( ti == 1 ) && ( i == 0)) { ti=2; break; }     // if start 0 not found - error
    if (i > 0){                                         //starting from 1st bit write to buffer
      if (ti) bitSet(buf[(i-1) >> 3], 7-j);
        else bitClear(buf[(i-1) >> 3], 7-j);
      j++; if (j>7) j=0;
    }
  }
  if (ti == 2) return false;                           //timeout
  return true;
}

bool sendOpT5557(byte opCode, unsigned long password = 0, byte lockBit = 0, unsigned long data = 0, byte blokAddr = 1){
  TxBitRfid(opCode >> 1); TxBitRfid(opCode & 1); // transmit operation code 10
  if (opCode == 0b00) return true;
  // password
  TxBitRfid(lockBit & 1);               // lockbit 0
  if (data != 0){
    for (byte i = 0; i<32; i++) {
      TxBitRfid((data>>(31-i)) & 1);
    }
  }
  TxBitRfid(blokAddr>>2); TxBitRfid(blokAddr>>1); TxBitRfid(blokAddr & 1);      // block address for writing
  delay(4);                                                                     // wait while data is written
  return true;
}

bool write2rfidT5557(byte* buf){
  bool result; unsigned long data32;
  delay(6);
  for (byte k = 0; k<2; k++){                                       // send key data
    data32 = (unsigned long)buf[0 + (k<<2)]<<24 | (unsigned long)buf[1 + (k<<2)]<<16 | (unsigned long)buf[2 + (k<<2)]<<8 | (unsigned long)buf[3 + (k<<2)];
    rfidGap(30 * 8);                                                 //start gap
    sendOpT5557(0b10, 0, 0, data32, k+1);                            //transmit 32 key bits to block k
    delay(6);
  }
  delay(6);
  rfidGap(30 * 8);                  //start gap
  sendOpT5557(0b00);
  delay(4);
  result = readEM_Marie(addr);
  TCCR2A &=0b00111111;              //Turn off PWM COM2A (pin 11)
  for (byte i = 0; i < 8; i++)
    if (addr[i] != keyID[i]) { result = false; break; }
  if (!result){
    OLED_printError("Copy error");
    oled.println("        key");
    Sd_ErrorBeep();
  } else {
    OLED_printError("Key successfully", false);
    oled.println("      copied");
    Sd_ReadOK();
    delay(2000);
  }
  digitalWrite(R_Led, HIGH);
  return result;  
}

emRWType getRfidRWtype(){
  unsigned long data32, data33; byte buf[4] = {0, 0, 0, 0}; 
  rfidACsetOn();                // turn on 125kHz generator and comparator
  delay(13);                    //13 ms for detector transients
  rfidGap(30 * 8);              //start gap
  sendOpT5557(0b11, 0, 0, 0, 1); //switch to Vendor ID read mode 
  if (!T5557_blockRead(buf)) return rwUnknown; 
  data32 = (unsigned long)buf[0]<<24 | (unsigned long)buf[1]<<16 | (unsigned long)buf[2]<<8 | (unsigned long)buf[3];
  delay(4);
  rfidGap(20 * 8);          //gap  
  data33 = 0b00000000000101001000000001000000 | (rfidUsePWD << 4);   //config register 0b00000000000101001000000001000000
  sendOpT5557(0b10, 0, 0, data33, 0);   //transmit config register
  delay(4);
  rfidGap(30 * 8);          //start gap
  sendOpT5557(0b11, 0, 0, 0, 1); //switch to Vendor ID read mode 
  if (!T5557_blockRead(buf)) return rwUnknown; 
  data33 = (unsigned long)buf[0]<<24 | (unsigned long)buf[1]<<16 | (unsigned long)buf[2]<<8 | (unsigned long)buf[3];
  sendOpT5557(0b00, 0, 0, 0, 0);  // send Reset
  delay(6);
  if (data32 != data33) return rwUnknown;    
  return T5557;
}

bool write2rfid(){
  bool Check = true;
  if (searchEM_Marine(false)) {
    for (byte i = 0; i < 8; i++)
      if (addr[i] != keyID[i]) { Check = false; break; }  // compare code for writing with what's already written in the key.
    if (Check) {                                          // if codes match, no need to write
      digitalWrite(R_Led, LOW); 
      OLED_printError("This is the same key");
      Sd_ErrorBeep();
      digitalWrite(R_Led, HIGH);
      delay(1000);
      return false;
    }
  }
  emRWType rwType = getRfidRWtype(); // determine type T5557 (T5577) or EM4305
  switch (rwType){
    case T5557: return write2rfidT5557(keyID); break;                    //write T5557
    //case EM4305: return write2rfidEM4305(keyID); break;                  //write EM4305
    case rwUnknown: break;
  }
  return false;
}

void SendEM_Marine(byte* buf){ 
  TCCR2A &=0b00111111; // disable pwm 
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
  // Dallas emulation code
  // Need to implement
}

void BM_SendKey(byte* buf){
  switch (keyType){
    case keyEM_Marine: SendEM_Marine(buf); break;
    default: SendDallas(buf); break;
  }
}

unsigned long stTimer = millis();

void loop() {
  char echo = Serial.read();
  
  // Only e command for memory clearing remains
  if (echo == 'e'){
    oled.clear();
    oled.home();
    oled.println("Memory cleared successfully!");
    EEPROM.update(0, 0); EEPROM.update(1, 0);
    EEPROM_key_count = 0; EEPROM_key_index = 0;
    Sd_ReadOK();
    oled.update();
  }
  
  // Reading buttons
  bool modeBtn = readButton(BTN_MODE, 0);
  bool leftBtn = readButton(BTN_LEFT, 1);
  bool rightBtn = readButton(BTN_RIGHT, 2);
  
  // ===== MODE PROCESSING =====
  if (!emulating) {
    // Normal mode (not emulation)
    
    // ===== MODE + LEFT COMBINATION PROCESSING (DELETION) =====
    if (modeBtn == LOW && leftBtn == LOW) {
      if (!comboMode) {
        comboTimeStart = millis();
        comboMode = true;
        Sd_ComboStart(); // Sound signal for combination start
      } else if (millis() - comboTimeStart > 1000) { // Hold for 1 second
        if (digitalRead(BTN_MODE) == LOW && digitalRead(BTN_LEFT) == LOW) {
          deleteCurrentKey();
          while (digitalRead(BTN_MODE) == LOW || digitalRead(BTN_LEFT) == LOW); // Wait for release
        }
        comboMode = false;
      }
    } else {
      comboMode = false;
    }
    
    // MODE button processing (mode switching) - only if not in combination
    static bool lastModeBtnState = HIGH;
    if (modeBtn == LOW && lastModeBtnState == HIGH && !comboMode) {
      delay(50); // additional debounce
      if (digitalRead(BTN_MODE) == LOW && !comboMode) {
        switch (copierMode){
          case md_empty: Sd_ErrorBeep(); break;
          case md_read: 
            copierMode = md_write; 
            clearLed(); 
            digitalWrite(R_Led, HIGH);  
            break;
          case md_write: 
            copierMode = md_blueMode; 
            clearLed(); 
            // In md_blueMode start appropriate emulation
            if (keyType == keyEM_Marine) {
              startRFIDEmulation();
            } else {
              startIButtonEmulation();
            }
            break;
          case md_blueMode: 
            copierMode = md_read; 
            clearLed(); 
            digitalWrite(G_Led, HIGH); 
            break;
        }
        if (copierMode != md_blueMode) {
          OLED_printKey(keyID);
          Sd_WriteStep();
        }
      }
    }
    lastModeBtnState = modeBtn;
    
    // LEFT button processing (previous key) - only if not in combination
    static bool lastLeftBtnState = HIGH;
    if (leftBtn == LOW && lastLeftBtnState == HIGH && EEPROM_key_count > 0 && copierMode != md_blueMode && !comboMode) {
      delay(50);
      if (digitalRead(BTN_LEFT) == LOW && !comboMode) {
        EEPROM_key_index--;
        if (EEPROM_key_index < 1) EEPROM_key_index = EEPROM_key_count;
        EEPROM_get_key(EEPROM_key_index, keyID);
        OLED_printKey(keyID);
        Sd_WriteStep();
      }
    }
    lastLeftBtnState = leftBtn;
    
    // RIGHT button processing (next key)
    static bool lastRightBtnState = HIGH;
    if (rightBtn == LOW && lastRightBtnState == HIGH && EEPROM_key_count > 0 && copierMode != md_blueMode && !comboMode) {
      delay(50);
      if (digitalRead(BTN_RIGHT) == LOW) {
        EEPROM_key_index++;
        if (EEPROM_key_index > EEPROM_key_count) EEPROM_key_index = 1;
        EEPROM_get_key(EEPROM_key_index, keyID);
        OLED_printKey(keyID);
        Sd_WriteStep();
      }
    }
    lastRightBtnState = rightBtn;
    
    // MODE button hold processing (key saving) - only if not in combination
    if (copierMode != md_empty && modeBtn == LOW && copierMode != md_blueMode && !comboMode) {
      if (!holdMode) {
        holdTimeStart = millis();
        holdMode = true;
      } else if (millis() - holdTimeStart > 1000) { // Hold for 1 second
        if (digitalRead(BTN_MODE) == LOW && !comboMode) {
          if (EPPROM_AddKey(keyID)) {
            OLED_printError("Key saved", false);
            Sd_ReadOK();
            delay(1000); 
          } else {
            Sd_ErrorBeep();
          }
          OLED_printKey(keyID);
          while (digitalRead(BTN_MODE) == LOW); // Wait for release
        }
        holdMode = false;
      }
    } else {
      holdMode = false;
    }
    
    // Delay for stable operation
    if (millis() - stTimer < 100) return;
    stTimer = millis();
    
    // Main operation cycle depending on mode
    switch (copierMode){
      case md_empty: 
      case md_read: 
        if (searchCyfral() || searchMetacom() || searchEM_Marine() || searchIbutton() ) {
          Sd_ReadOK();
          copierMode = md_read;
          digitalWrite(G_Led, HIGH);
          if (indxKeyInROM(keyID) == 0) OLED_printKey(keyID, 1);
            else OLED_printKey(keyID, 3);
        } 
        break;
      case md_write:
        if (keyType == keyEM_Marine) write2rfid();
          else write2iBtn(); 
        break;
      case md_blueMode:
        // Already processed in emulation mode
        break;
    }
  } else {
    // ===== EMULATION MODE =====
    
    // MODE button - stop emulation
    static bool lastModeBtnState = HIGH;
    if (modeBtn == LOW && lastModeBtnState == HIGH) {
      delay(50);
      if (digitalRead(BTN_MODE) == LOW) {
        stopEmulation();
        copierMode = md_read;
        clearLed();
        digitalWrite(G_Led, HIGH);
        while (digitalRead(BTN_MODE) == LOW);
      }
    }
    lastModeBtnState = modeBtn;
    
    // Continue emulation depending on key type
    if (keyType == keyEM_Marine) {
      // For RFID - constantly send key
      sendRFIDKey(keyID);
    } else {
      // For iButton - process requests
      hub.poll();
    }
    
    // Blink blue LED
    digitalWrite(B_Led, (millis() / 500) % 2);
  }
  
  delay(1);
}
