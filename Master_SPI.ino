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

unsigned long lastPollTime = 0;
unsigned long pollInterval = 2000;
unsigned long lastLCDUpdate = 0;
uint8_t currentLCDPage = 0;

void setup() {
  Serial.begin(9600);
  Serial.println(F("================================="));
  Serial.println(F("Master SPI Controller - FIXED"));
  Serial.println(F("================================="));
  Serial.println(F("Commands: XY"));
  Serial.println(F("  X = Slave ID (1-3)"));
  Serial.println(F("  Y = Command (1=pb2, 2=analog, 3=counter)"));
  Serial.println(F("================================="));
  
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("Master Ready");
  delay(1000);
  
  // Initialize SPI
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV128);
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  
  // Setup SS pins
  pinMode(SS_SLAVE1, OUTPUT);
  pinMode(SS_SLAVE2, OUTPUT);
  pinMode(SS_SLAVE3, OUTPUT);
  digitalWrite(SS_SLAVE1, HIGH);
  digitalWrite(SS_SLAVE2, HIGH);
  digitalWrite(SS_SLAVE3, HIGH);
  
  loadFromEEPROM();
  Serial.println(F("Ready!"));
}

void loop() {
  if (millis() - lastPollTime >= pollInterval) {
    lastPollTime = millis();
    pollAllSlaves();
  }
  
  if (millis() - lastLCDUpdate >= 2000) {
    lastLCDUpdate = millis();
    updateLCD();
  }
  
  if (Serial.available() > 0) {
    handleSerialCommand();
  }
}

void pollAllSlaves() {
  Serial.println(F("\n--- Polling All Slaves ---"));
  
  pollSlave(0x01, SS_SLAVE1, slave1Data, EEPROM_SLAVE1_BASE);
  delay(100);
  
  pollSlave(0x02, SS_SLAVE2, slave2Data, EEPROM_SLAVE2_BASE);
  delay(100);
  
  pollSlave(0x03, SS_SLAVE3, slave3Data, EEPROM_SLAVE3_BASE);
  delay(100);
  
  Serial.println(F("--- Poll Complete ---\n"));
}

void pollSlave(uint8_t slaveID, uint8_t ssPin, SlaveData &data, int eepromBase) {
  bool success = true;
  
  Serial.print(F("Polling Slave 0x"));
  Serial.println(slaveID, HEX);
  
  // Read PB2
  uint8_t pb2 = readValue(slaveID, ssPin, CMD_READ_VALUE);
  if (pb2 == 0xFF) {
    success = false;
  } else {
    Serial.print(F("  PB2 = "));
    Serial.println(pb2);
  }
  delay(100);
  
  // Read Analog
  uint16_t analog = readSensor(slaveID, ssPin, CMD_READ_SENSOR);
  if (analog == 0xFFFF) {
    success = false;
  } else {
    Serial.print(F("  Analog = "));
    Serial.println(analog);
  }
  delay(100);
  
  // Read Counter
  uint8_t counter = readValue(slaveID, ssPin, CMD_READ_COUNTER);
  if (counter == 0xFF) {
    success = false;
  } else {
    Serial.print(F("  Counter = "));
    Serial.println(counter);
  }
  
  if (success) {
    data.pb2 = pb2;
    data.analog = analog;
    data.counter = counter;
    data.valid = true;
    saveSlaveToEEPROM(eepromBase, data);
    Serial.println(F("  ✓ SUCCESS"));
  } else {
    Serial.println(F("  ✗ FAILED"));
    data.valid = false;
  }
}

uint8_t readValue(uint8_t slaveID, uint8_t ssPin, uint8_t cmd) {
  uint8_t checksum = slaveID ^ cmd;
  
  // Activate slave
  digitalWrite(ssPin, LOW);
  delayMicroseconds(50);
  
  // Send command
  SPI.transfer(SYNC1);
  delayMicroseconds(20);
  SPI.transfer(SYNC2);
  delayMicroseconds(20);
  SPI.transfer(slaveID);
  delayMicroseconds(20);
  SPI.transfer(cmd);
  delayMicroseconds(20);
  SPI.transfer(checksum);
  delayMicroseconds(100); // Wait for slave to prepare
  
  // Read response - increased buffer
  uint8_t response[15];
  for (int i = 0; i < 15; i++) {
    response[i] = SPI.transfer(0x00);
    delayMicroseconds(20);
  }
  
  digitalWrite(ssPin, HIGH);
  delayMicroseconds(50);
  
  // Find sync pattern
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

uint16_t readSensor(uint8_t slaveID, uint8_t ssPin, uint8_t cmd) {
  uint8_t checksum = slaveID ^ cmd;
  
  digitalWrite(ssPin, LOW);
  delayMicroseconds(50);
  
  // Send command
  SPI.transfer(SYNC1);
  delayMicroseconds(20);
  SPI.transfer(SYNC2);
  delayMicroseconds(20);
  SPI.transfer(slaveID);
  delayMicroseconds(20);
  SPI.transfer(cmd);
  delayMicroseconds(20);
  SPI.transfer(checksum);
  delayMicroseconds(100);
  
  // Read response
  uint8_t response[15];
  for (int i = 0; i < 15; i++) {
    response[i] = SPI.transfer(0x00);
    delayMicroseconds(20);
  }
  
  digitalWrite(ssPin, HIGH);
  delayMicroseconds(50);
  
  // Find sync pattern
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

void saveSlaveToEEPROM(int base, SlaveData &data) {
  EEPROM.update(base + 0, data.pb2);
  EEPROM.update(base + 1, (data.analog >> 8) & 0xFF);
  EEPROM.update(base + 2, data.analog & 0xFF);
  EEPROM.update(base + 3, data.counter);
}

void loadFromEEPROM() {
  slave1Data.pb2 = EEPROM.read(EEPROM_SLAVE1_BASE + 0);
  slave1Data.analog = (EEPROM.read(EEPROM_SLAVE1_BASE + 1) << 8) | 
                      EEPROM.read(EEPROM_SLAVE1_BASE + 2);
  slave1Data.counter = EEPROM.read(EEPROM_SLAVE1_BASE + 3);
  
  slave2Data.pb2 = EEPROM.read(EEPROM_SLAVE2_BASE + 0);
  slave2Data.analog = (EEPROM.read(EEPROM_SLAVE2_BASE + 1) << 8) | 
                      EEPROM.read(EEPROM_SLAVE2_BASE + 2);
  slave2Data.counter = EEPROM.read(EEPROM_SLAVE2_BASE + 3);
  
  slave3Data.pb2 = EEPROM.read(EEPROM_SLAVE3_BASE + 0);
  slave3Data.analog = (EEPROM.read(EEPROM_SLAVE3_BASE + 1) << 8) | 
                      EEPROM.read(EEPROM_SLAVE3_BASE + 2);
  slave3Data.counter = EEPROM.read(EEPROM_SLAVE3_BASE + 3);
  
  Serial.println(F("Data loaded from EEPROM"));
}

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
      if (slave1Data.valid) {
        lcd.print(" OK");
      } else {
        lcd.print(" ER");
      }
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
      if (slave2Data.valid) {
        lcd.print(" OK");
      } else {
        lcd.print(" ER");
      }
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
      if (slave3Data.valid) {
        lcd.print(" OK");
      } else {
        lcd.print(" ER");
      }
      break;
  }
  
  currentLCDPage = (currentLCDPage + 1) % 3;
}

void handleSerialCommand() {
  String input = Serial.readStringUntil('\n');
  input.trim();
  
  if (input.length() != 2) {
    Serial.println(F("Error: Command must be 2 digits (XY)"));
    return;
  }
  
  uint8_t slaveNum = input.charAt(0) - '0';
  uint8_t cmdNum = input.charAt(1) - '0';
  
  if (slaveNum < 1 || slaveNum > 3) {
    Serial.println(F("Error: Slave ID must be 1-3"));
    return;
  }
  
  if (cmdNum < 1 || cmdNum > 3) {
    Serial.println(F("Error: Command must be 1-3"));
    return;
  }
  
  SlaveData data;
  int eepromBase;
  
  switch (slaveNum) {
    case 1:
      eepromBase = EEPROM_SLAVE1_BASE;
      data = slave1Data;
      break;
    case 2:
      eepromBase = EEPROM_SLAVE2_BASE;
      data = slave2Data;
      break;
    case 3:
      eepromBase = EEPROM_SLAVE3_BASE;
      data = slave3Data;
      break;
  }
  
  data.pb2 = EEPROM.read(eepromBase + 0);
  data.analog = (EEPROM.read(eepromBase + 1) << 8) | 
                EEPROM.read(eepromBase + 2);
  data.counter = EEPROM.read(eepromBase + 3);
  
  Serial.print(F("\n--- Slave "));
  Serial.print(slaveNum);
  Serial.print(F(" - "));
  
  switch (cmdNum) {
    case 1:
      Serial.print(F("PB2: "));
      Serial.println(data.pb2);
      break;
    case 2:
      Serial.print(F("Analog: "));
      Serial.println(data.analog);
      break;
    case 3:
      Serial.print(F("Counter: "));
      Serial.println(data.counter);
      break;
  }
  Serial.println();
}
