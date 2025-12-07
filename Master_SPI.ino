// Master
#include <SPI.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>

// LCD Setup
LiquidCrystal lcd(47, 45, 37, 39, 43, 41);

// Slave Select Pins
#define SS_SLAVE1 A13
#define SS_SLAVE2 A14
#define SS_SLAVE3 A15

// SPI Protocol
#define SYNC1 0xFF
#define SYNC2 0xFE

// Commands
#define CMD_READ_VALUE   0x01
#define CMD_READ_SENSOR  0x02
#define CMD_READ_COUNTER 0x03

// EEPROM Addresses
#define EEPROM_SLAVE1_BASE 0
#define EEPROM_SLAVE2_BASE 4
#define EEPROM_SLAVE3_BASE 8

struct SlaveData {
  uint8_t pb2;
  uint16_t analog;
  uint8_t counter;
  bool valid;
};

SlaveData slave1Data = {0, 0, 0, false};
SlaveData slave2Data = {0, 0, 0, false};
SlaveData slave3Data = {0, 0, 0, false};

unsigned long lastLCDUpdate = 0;
unsigned long lastPCDisplay = 0;
unsigned long pcDisplayInterval = 10;  // Cepat: 10ms
uint8_t currentLCDPage = 0;

// PC Request variables
bool pcRequestActive = false;
uint8_t requestedSlave = 0;
uint8_t requestedData = 0;

void setup() {
  Serial.begin(115200);  // Baud rate tinggi untuk kecepatan
  Serial.println(F("================================="));
  Serial.println(F("Master SPI Controller - FAST"));
  Serial.println(F("================================="));
  Serial.println(F("PC Commands: XY"));
  Serial.println(F("  X = Slave ID (1-3)"));
  Serial.println(F("  Y = Data (1-7):"));
  Serial.println(F("    1=PB2, 2=ADC, 3=PB2+ADC"));
  Serial.println(F("    4=CNT, 5=PB2+CNT, 6=ADC+CNT, 7=ALL"));
  Serial.println(F("Send 'STOP' to stop display"));
  Serial.println(F("================================="));
  
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("Master Ready");
  delay(1000);
  
  // Initialize SPI dengan clock cepat
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV8);  // Lebih cepat
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  
  // Setup SS pins
  pinMode(SS_SLAVE1, OUTPUT);
  pinMode(SS_SLAVE2, OUTPUT);
  pinMode(SS_SLAVE3, OUTPUT);
  digitalWrite(SS_SLAVE1, HIGH);
  digitalWrite(SS_SLAVE2, HIGH);
  digitalWrite(SS_SLAVE3, HIGH);
  
  Serial.println(F("Ready! (Non-blocking mode)"));
  Serial.println();
}

void loop() {
  // Update LCD (non-blocking)
  if (millis() - lastLCDUpdate >= 500) {
    lastLCDUpdate = millis();
    updateLCD();
  }
  
  // Tampilkan PC request secara periodik (cepat, non-blocking)
  if (pcRequestActive && (millis() - lastPCDisplay >= pcDisplayInterval)) {
    lastPCDisplay = millis();
    fetchAndDisplay();  // Ambil data fresh dari slave dan tampilkan
  }
  
  // Handle request dari PC
  if (Serial.available() > 0) {
    handlePCRequest();
  }
}

// Ambil data dari slave dan tampilkan langsung (non-blocking)
void fetchAndDisplay() {
  uint8_t ssPin;
  int eepromBase;
  SlaveData* dataPtr;
  
  switch (requestedSlave) {
    case 1:
      ssPin = SS_SLAVE1;
      eepromBase = EEPROM_SLAVE1_BASE;
      dataPtr = &slave1Data;
      break;
    case 2:
      ssPin = SS_SLAVE2;
      eepromBase = EEPROM_SLAVE2_BASE;
      dataPtr = &slave2Data;
      break;
    case 3:
      ssPin = SS_SLAVE3;
      eepromBase = EEPROM_SLAVE3_BASE;
      dataPtr = &slave3Data;
      break;
  }
  
  // Ambil data dari slave secara cepat
  bool success = quickPollSlave(requestedSlave, ssPin, *dataPtr, eepromBase);
  
  if (success) {
    // Tampilkan data sesuai request
    Serial.print(F(">>> "));
    
    switch (requestedData) {
      case 1: // PB2 only
        Serial.println(dataPtr->pb2);
        break;
        
      case 2: // ADC only
        Serial.println(dataPtr->analog);
        break;
        
      case 3: // PB2 + ADC
        Serial.print(dataPtr->pb2);
        Serial.print(F(" "));
        Serial.println(dataPtr->analog);
        break;
        
      case 4: // Counter only
        Serial.println(dataPtr->counter);
        break;
        
      case 5: // PB2 + Counter
        Serial.print(dataPtr->pb2);
        Serial.print(F(" "));
        Serial.println(dataPtr->counter);
        break;
        
      case 6: // ADC + Counter
        Serial.print(dataPtr->analog);
        Serial.print(F(" "));
        Serial.println(dataPtr->counter);
        break;
        
      case 7: // ALL
        Serial.print(dataPtr->pb2);
        Serial.print(F(" "));
        Serial.print(dataPtr->analog);
        Serial.print(F(" "));
        Serial.println(dataPtr->counter);
        break;
    }
  }
}

// Quick polling slave (optimized, minimal delay)
bool quickPollSlave(uint8_t slaveID, uint8_t ssPin, SlaveData &data, int eepromBase) {
  bool success = true;
  
  // Read PB2
  uint8_t pb2 = readValue(slaveID, ssPin, CMD_READ_VALUE);
  if (pb2 == 0xFF) success = false;
  
  // Read Analog
  uint16_t analog = readSensor(slaveID, ssPin, CMD_READ_SENSOR);
  if (analog == 0xFFFF) success = false;
  
  // Read Counter
  uint8_t counter = readValue(slaveID, ssPin, CMD_READ_COUNTER);
  if (counter == 0xFF) success = false;
  
  if (success) {
    data.pb2 = pb2;
    data.analog = analog;
    data.counter = counter;
    data.valid = true;
    
    // Simpan ke EEPROM
    saveSlaveToEEPROM(eepromBase, data);
    return true;
  }
  
  data.valid = false;
  return false;
}

// Baca single byte value dari slave (optimized)
uint8_t readValue(uint8_t slaveID, uint8_t ssPin, uint8_t cmd) {
  uint8_t checksum = slaveID ^ cmd;
  
  digitalWrite(ssPin, LOW);
  delayMicroseconds(20);
  
  // Kirim command
  SPI.transfer(SYNC1);
  delayMicroseconds(10);
  SPI.transfer(SYNC2);
  delayMicroseconds(10);
  SPI.transfer(slaveID);
  delayMicroseconds(10);
  SPI.transfer(cmd);
  delayMicroseconds(10);
  SPI.transfer(checksum);
  delayMicroseconds(50);
  
  // Baca response
  uint8_t response[15];
  for (int i = 0; i < 15; i++) {
    response[i] = SPI.transfer(0x00);
    delayMicroseconds(10);
  }
  
  digitalWrite(ssPin, HIGH);
  
  // Cari sync pattern
  for (int i = 0; i < 10; i++) {
    if (response[i] == SYNC1 && 
        response[i+1] == SYNC2 && 
        response[i+2] == slaveID) {
      uint8_t data = response[i+3];
      uint8_t recvChecksum = response[i+4];
      uint8_t expectedChecksum = slaveID ^ data;
      
      if (recvChecksum == expectedChecksum) {
        return data;
      }
    }
  }
  
  return 0xFF;
}

// Baca sensor (2 bytes) dari slave (optimized)
uint16_t readSensor(uint8_t slaveID, uint8_t ssPin, uint8_t cmd) {
  uint8_t checksum = slaveID ^ cmd;
  
  digitalWrite(ssPin, LOW);
  delayMicroseconds(20);
  
  // Kirim command
  SPI.transfer(SYNC1);
  delayMicroseconds(10);
  SPI.transfer(SYNC2);
  delayMicroseconds(10);
  SPI.transfer(slaveID);
  delayMicroseconds(10);
  SPI.transfer(cmd);
  delayMicroseconds(10);
  SPI.transfer(checksum);
  delayMicroseconds(50);
  
  // Baca response
  uint8_t response[15];
  for (int i = 0; i < 15; i++) {
    response[i] = SPI.transfer(0x00);
    delayMicroseconds(10);
  }
  
  digitalWrite(ssPin, HIGH);
  
  // Cari sync pattern
  for (int i = 0; i < 9; i++) {
    if (response[i] == SYNC1 && 
        response[i+1] == SYNC2 && 
        response[i+2] == slaveID) {
      uint8_t analogH = response[i+3];
      uint8_t analogL = response[i+4];
      uint8_t recvChecksum = response[i+5];
      uint8_t expectedChecksum = slaveID ^ analogH ^ analogL;
      
      if (recvChecksum == expectedChecksum) {
        return (uint16_t)((analogH << 8) | analogL);
      }
    }
  }
  
  return 0xFFFF;
}

// Simpan data slave ke EEPROM
void saveSlaveToEEPROM(int base, SlaveData &data) {
  EEPROM.update(base + 0, data.pb2);
  EEPROM.update(base + 1, (data.analog >> 8) & 0xFF);
  EEPROM.update(base + 2, data.analog & 0xFF);
  EEPROM.update(base + 3, data.counter);
}

// Update LCD display
void updateLCD() {
  lcd.clear();
  
  switch (currentLCDPage) {
    case 0:
      lcd.setCursor(0, 0);
      lcd.print("S1 P:");
      lcd.print(slave1Data.pb2);
      lcd.print(" A:");
      lcd.print(slave1Data.analog);
      lcd.setCursor(0, 1);
      lcd.print("   C:");
      lcd.print(slave1Data.counter);
      lcd.print(slave1Data.valid ? " OK" : " ER");
      break;
      
    case 1:
      lcd.setCursor(0, 0);
      lcd.print("S2 P:");
      lcd.print(slave2Data.pb2);
      lcd.print(" A:");
      lcd.print(slave2Data.analog);
      lcd.setCursor(0, 1);
      lcd.print("   C:");
      lcd.print(slave2Data.counter);
      lcd.print(slave2Data.valid ? " OK" : " ER");
      break;
      
    case 2:
      lcd.setCursor(0, 0);
      lcd.print("S3 P:");
      lcd.print(slave3Data.pb2);
      lcd.print(" A:");
      lcd.print(slave3Data.analog);
      lcd.setCursor(0, 1);
      lcd.print("   C:");
      lcd.print(slave3Data.counter);
      lcd.print(slave3Data.valid ? " OK" : " ER");
      break;
  }
  
  currentLCDPage = (currentLCDPage + 1) % 3;
}

// Handle request dari PC
void handlePCRequest() {
  String input = Serial.readStringUntil('\n');
  input.trim();
  input.toUpperCase();
  
  // Check untuk STOP command
  if (input == "STOP") {
    pcRequestActive = false;
    Serial.println(F("\n[Display stopped]"));
    Serial.println();
    return;
  }
  
  if (input.length() != 2) {
    Serial.println(F("Error: Command harus 2 digit (XY) atau 'STOP'"));
    return;
  }
  
  uint8_t slaveNum = input.charAt(0) - '0';
  uint8_t dataSelect = input.charAt(1) - '0';
  
  if (slaveNum < 1 || slaveNum > 3) {
    Serial.println(F("Error: Slave ID harus 1-3"));
    return;
  }
  
  if (dataSelect < 1 || dataSelect > 7) {
    Serial.println(F("Error: Data Select harus 1-7"));
    return;
  }
  
  // Simpan request dan aktifkan display periodik
  requestedSlave = slaveNum;
  requestedData = dataSelect;
  pcRequestActive = true;
  lastPCDisplay = 0; // Langsung tampilkan
  
  Serial.print(F("\n[S"));
  Serial.print(slaveNum);
  Serial.print(F(" D"));
  Serial.print(dataSelect);
  Serial.println(F(" - STOP to stop]"));
}
