#define SLAVE_ID 0x01

// Pin definitions
#define SS_PIN 10
#define MOSI_PIN 11
#define MISO_PIN 12
#define SCK_PIN 13

// Protocol
#define SYNC1 0xFF
#define SYNC2 0xFE

// Commands
#define CMD_READ_VALUE   0x01
#define CMD_READ_SENSOR  0x02
#define CMD_READ_COUNTER 0x03

// State machine
enum SpiState {
  WAIT_SYNC1,
  WAIT_SYNC2,
  WAIT_ID,
  WAIT_CMD,
  WAIT_CHECKSUM,
  SEND_RESPONSE
};

volatile SpiState currentState = WAIT_SYNC1;
volatile uint8_t responseBuffer[8];
volatile uint8_t responseLength = 0;
volatile uint8_t responseIndex = 0;
volatile uint8_t receivedID = 0;
volatile uint8_t receivedCMD = 0;
volatile bool responseReady = false;

// Data variables
int analogVal = 0;
volatile uint8_t cnt = 0;
bool pb2 = false;
bool pb1 = false;

// Snapshot untuk konsistensi data
volatile uint16_t analogSnapshot = 0;
volatile uint8_t cntSnapshot = 0;
volatile uint8_t pb2Snapshot = 0;

// Timing
unsigned long lastA0ToggleTime = 0;
unsigned long lastDeb = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastPrint = 0;

// States
bool blinkState = false;
bool blinking = false;
int blinkCount = 0;
int lastPB2 = HIGH;

void isrInt1() {
  static unsigned long last = 0;
  unsigned long now = micros();
  if (now - last > 50000) {
    last = now;
    cnt++;
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println(F("================================"));
  Serial.print(F("Slave ID: 0x"));
  Serial.println(SLAVE_ID, HEX);
  Serial.println(F("CMD 0x01: Read Value (pb2)"));
  Serial.println(F("CMD 0x02: Read Sensor (16-bit)"));
  Serial.println(F("CMD 0x03: Read Counter"));
  Serial.println(F("================================"));
  
  // Setup pins
  pinMode(MISO_PIN, OUTPUT);
  pinMode(MOSI_PIN, INPUT);
  pinMode(SCK_PIN, INPUT);
  pinMode(SS_PIN, INPUT);
  pinMode(3, INPUT_PULLUP);
  pinMode(A7, INPUT_PULLUP);
  pinMode(A0, INPUT);
  pinMode(5, OUTPUT);
  pinMode(10, INPUT);

  // Interrupt
  attachInterrupt(digitalPinToInterrupt(3), isrInt1, FALLING);
  
  // Initialize SPI slave
  SPCR = (1 << SPE) | (1 << SPIE);
  SPCR &= ~(1 << MSTR);  // Ensure slave mode
  
  // Clear SPI registers
  uint8_t clr = SPSR;
  clr = SPDR;
  (void)clr;
  
  // Pre-load SPDR
  SPDR = 0x00;
  
  Serial.println(F("Ready and waiting..."));
}

void loop() {
  // =============================
  // READ ANALOG A0 → TOGGLE pb1
  // =============================
  analogVal = analogRead(A0);

  if (analogVal > 300 && analogVal < 700) {
    if (millis() - lastA0ToggleTime > 1000) {
      lastA0ToggleTime = millis();
      pb1 = !pb1;
    }
  } else {
    pb1 = false;
  }

  // =============================
  // TOGGLE BUTTON PB2 (A7)
  // =============================
  int readPB2 = (analogRead(A7) == 0) ? 0 : 1;

  if (readPB2 != lastPB2 && millis() - lastDeb > 50) {
    lastDeb = millis();
    if (readPB2 == LOW) {
      pb2 = !pb2;
    }
  }
  lastPB2 = readPB2;

  // =============================
  // BLINK LED by CNT INTERRUPT
  // =============================
  if (cnt > 5 && !blinking) {
    cnt = 0;
    blinking = true;
    blinkCount = 0;
    blinkState = false;
    lastBlinkTime = millis();
  }

  if (blinking) {
    if (millis() - lastBlinkTime > 300) {
      lastBlinkTime = millis();
      blinkState = !blinkState;
      if (blinkState) blinkCount++;

      if (blinkCount > 3) {
        blinking = false;
        blinkState = false;
      }
    }
  } else {
    blinkState = false;
  }

  // OUTPUT LED
  digitalWrite(5, pb1 || pb2 || blinkState);
  
  // Debug print
  if (millis() - lastPrint >= 1000) {
    lastPrint = millis();
    Serial.print(F("A:"));
    Serial.print(analogVal);
    Serial.print(F(" CNT:"));
    Serial.print(cnt);
    Serial.print(F(" PB2:"));
    Serial.print(pb2);
    Serial.print(F(" PB1:"));
    Serial.println(pb1);
  }
}

void prepareResponse(uint8_t cmd) {
  // Snapshot data untuk konsistensi
  noInterrupts();
  analogSnapshot = analogVal;
  cntSnapshot = cnt;
  pb2Snapshot = pb2 ? 1 : 0;
  interrupts();
  
  responseLength = 0;
  responseIndex = 0;
  
  switch (cmd) {
    case CMD_READ_VALUE:
      // Response: [SYNC1][SYNC2][ID][DATA][CHECKSUM]
      responseBuffer[0] = SYNC1;
      responseBuffer[1] = SYNC2;
      responseBuffer[2] = SLAVE_ID;
      responseBuffer[3] = pb2Snapshot;
      responseBuffer[4] = (uint8_t)(SLAVE_ID ^ pb2Snapshot);
      responseLength = 5;
      break;
      
    case CMD_READ_SENSOR:
      // Response: [SYNC1][SYNC2][ID][HIGH][LOW][CHECKSUM]
      {
        uint8_t analogH = (analogSnapshot >> 8) & 0xFF;
        uint8_t analogL = analogSnapshot & 0xFF;
        responseBuffer[0] = SYNC1;
        responseBuffer[1] = SYNC2;
        responseBuffer[2] = SLAVE_ID;
        responseBuffer[3] = analogH;
        responseBuffer[4] = analogL;
        responseBuffer[5] = (uint8_t)(SLAVE_ID ^ analogH ^ analogL);
        responseLength = 6;
      }
      break;
      
    case CMD_READ_COUNTER:
      // Response: [SYNC1][SYNC2][ID][DATA][CHECKSUM]
      responseBuffer[0] = SYNC1;
      responseBuffer[1] = SYNC2;
      responseBuffer[2] = SLAVE_ID;
      responseBuffer[3] = cntSnapshot;
      responseBuffer[4] = (uint8_t)(SLAVE_ID ^ cntSnapshot);
      responseLength = 5;
      break;
      
    default:
      // Error response
      responseBuffer[0] = SYNC1;
      responseBuffer[1] = SYNC2;
      responseBuffer[2] = SLAVE_ID;
      responseBuffer[3] = 0xFF;
      responseBuffer[4] = (uint8_t)(SLAVE_ID ^ 0xFF);
      responseLength = 5;
      break;
  }
  
  responseReady = true;
}

ISR(SPI_STC_vect) {
  // reset
  if(digitalRead(SS_PIN)==HIGH){
    currentState=WAIT_SYNC1;
    responseReady=false;
    responseIndex=0;
    SPDR=0x00;
    return;
  }
  uint8_t received = SPDR;
  uint8_t toSend = 0x00;
  
  switch (currentState) {
    case WAIT_SYNC1:
      if (received == SYNC1) {
        currentState = WAIT_SYNC2;
        responseReady = false;
      }
      toSend = 0x00;
      break;
      
    case WAIT_SYNC2:
      if (received == SYNC2) {
        currentState = WAIT_ID;
      } else {
        currentState = WAIT_SYNC1;
      }
      toSend = 0x00;
      break;
      
    case WAIT_ID:
      receivedID = received;
      if (receivedID == SLAVE_ID) {
        currentState = WAIT_CMD;
      } else {
        currentState = WAIT_SYNC1;
      }
      toSend = 0x00;
      break;
      
    case WAIT_CMD:
      receivedCMD = received;
      currentState = WAIT_CHECKSUM;
      toSend = 0x00;
      break;
      
    case WAIT_CHECKSUM:
      {
        uint8_t checksum = received;
        uint8_t expectedChecksum = (uint8_t)(receivedID ^ receivedCMD);
        
        if (checksum == expectedChecksum) {
          // Valid command - prepare response
          prepareResponse(receivedCMD);
          currentState = SEND_RESPONSE;
          responseIndex = 0;
          toSend = responseBuffer[responseIndex];
        } else {
          // Invalid checksum
          currentState = WAIT_SYNC1;
          toSend = 0x00;
        }
      }
      break;
      
    case SEND_RESPONSE:
      if (responseReady && responseIndex < responseLength) {
        responseIndex++;
        if (responseIndex < responseLength) {
          toSend = responseBuffer[responseIndex];
        } else {
          // Response complete
          currentState = WAIT_SYNC1;
          responseReady = false;
          toSend = 0x00;
        }
      } else {
        currentState = WAIT_SYNC1;
        responseReady = false;
        toSend = 0x00;
      }
      break;
      
    default:
      currentState = WAIT_SYNC1;
      responseReady = false;
      toSend = 0x00;
      break;
  }
  
  SPDR = toSend;
}
