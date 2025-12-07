# Workshop_Mikro_Lanjut_dan_antar_muka
# Multi-Slave SPI Communication System

Sistem komunikasi SPI Master-Slave yang mendukung hingga 3 slave device dengan protokol komunikasi custom, LCD display, dan EEPROM storage. Sistem ini dirancang untuk komunikasi cepat dan non-blocking dengan PC request interface.

open link presentasi:
https://vinn17.github.io/Workshop_Mikro_Lanjut_dan_antar_muka/

## 🎯 Fitur Utama

- ✅ **Master-Multi Slave Architecture** - 1 Master dapat berkomunikasi dengan 3 Slave
- ✅ **Non-Blocking Communication** - Tidak ada blocking di Master
- ✅ **Fast Data Transfer** - SPI Clock DIV8, Baud Rate 115200
- ✅ **PC Request Interface** - Kontrol data display via Serial
- ✅ **LCD Display** - Monitoring real-time 16x2 LCD
- ✅ **EEPROM Storage** - Backup data otomatis
- ✅ **Custom Protocol** - Sync pattern + Checksum validation
- ✅ **Real-time Monitoring** - Data refresh hingga 100x per detik

---

## 📡 Komunikasi SPI

### Hardware Connection

```
Master (Arduino Mega)          Slave 1/2/3 (Arduino Uno/Nano)
├─ MOSI (51)    ────────────>  MOSI (11)
├─ MISO (50)    <────────────  MISO (12)
├─ SCK  (52)    ────────────>  SCK  (13)
├─ SS_1 (A13)   ────────────>  SS   (2)    [Slave 1]
├─ SS_2 (A14)   ────────────>  SS   (2)    [Slave 2]
└─ SS_3 (A15)   ────────────>  SS   (2)    [Slave 3]
```

### SPI Configuration

| Parameter | Master | Slave |
|-----------|--------|-------|
| Mode | Master | Slave |
| Clock | 2 MHz (DIV8) | Slave follows Master |
| Data Mode | SPI_MODE0 | SPI_MODE0 |
| Bit Order | MSBFIRST | MSBFIRST |
| Baud Rate | 115200 | 9600 |

---

## 📦 Protokol Komunikasi

### Format Paket Request (Master → Slave)

```
┌──────┬──────┬──────┬──────┬──────────┐
│ SYNC1│ SYNC2│  ID  │ CMD  │ CHECKSUM │
├──────┼──────┼──────┼──────┼──────────┤
│ 0xFF │ 0xFE │ 0x01 │ 0x01 │ ID ^ CMD │
└──────┴──────┴──────┴──────┴──────────┘
  1B     1B     1B     1B       1B
```

**Field Description:**
- `SYNC1` (0xFF): Synchronization byte 1
- `SYNC2` (0xFE): Synchronization byte 2
- `ID`: Slave ID (0x01, 0x02, 0x03)
- `CMD`: Command byte (0x01, 0x02, 0x03)
- `CHECKSUM`: XOR checksum (ID ^ CMD)

### Format Paket Response (Slave → Master)

#### Response CMD 0x01 & 0x03 (Single Byte Data)
```
┌──────┬──────┬──────┬──────┬──────────┐
│ SYNC1│ SYNC2│  ID  │ DATA │ CHECKSUM │
├──────┼──────┼──────┼──────┼──────────┤
│ 0xFF │ 0xFE │ 0x01 │ 0x01 │ ID ^ DATA│
└──────┴──────┴──────┴──────┴──────────┘
  1B     1B     1B     1B       1B
```

#### Response CMD 0x02 (16-bit Sensor Data)
```
┌──────┬──────┬──────┬──────┬──────┬──────────┐
│ SYNC1│ SYNC2│  ID  │ HIGH │ LOW  │ CHECKSUM │
├──────┼──────┼──────┼──────┼──────┼──────────┤
│ 0xFF │ 0xFE │ 0x01 │ 0x03 │ 0x20 │ID^H^L    │
└──────┴──────┴──────┴──────┴──────┴──────────┘
  1B     1B     1B     1B     1B       1B
```

**Field Description:**
- `DATA`: 8-bit data value
- `HIGH`: Upper 8 bits of 16-bit value
- `LOW`: Lower 8 bits of 16-bit value
- `CHECKSUM`: XOR checksum

---

## 🎮 Command Set

### Master Commands

| Command | Value | Description | Data Type | Response Size |
|---------|-------|-------------|-----------|---------------|
| `CMD_READ_VALUE` | 0x01 | Read PB2 button state | uint8_t | 5 bytes |
| `CMD_READ_SENSOR` | 0x02 | Read analog sensor (A0) | uint16_t | 6 bytes |
| `CMD_READ_COUNTER` | 0x03 | Read interrupt counter | uint8_t | 5 bytes |

### PC Request Commands

Format: `XY` (2 digit)

**X = Slave ID (1-3)**
- `1` = Slave 1
- `2` = Slave 2
- `3` = Slave 3

**Y = Data Selection (1-7)**
- `1` = PB2 only
- `2` = ADC only
- `3` = PB2 + ADC
- `4` = Counter only
- `5` = PB2 + Counter
- `6` = ADC + Counter
- `7` = All data (PB2 + ADC + Counter)

**Special Command:**
- `STOP` = Stop continuous display

---

## 🔄 Cara Kerja Sistem

### 1. Master Operation Flow

```
┌─────────────────────────────────────────┐
│         MASTER MAIN LOOP                │
│  (Non-Blocking, Event-Driven)           │
└─────────────────────────────────────────┘
            │
            ├─> LCD Update (500ms)
            │   └─> Rotate display: S1→S2→S3
            │
            ├─> PC Request Active?
            │   └─> YES: Fetch & Display (10ms)
            │       ├─> Read from Slave via SPI
            │       ├─> Save to EEPROM
            │       └─> Display to Serial
            │
            └─> Serial Available?
                └─> Handle PC Command
                    ├─> Parse XY or STOP
                    └─> Activate request loop
```

### 2. Slave Operation Flow

```
┌─────────────────────────────────────────┐
│         SLAVE MAIN LOOP                 │
└─────────────────────────────────────────┘
            │
            ├─> Read Analog A0
            │   └─> If 300-700: Toggle PB1
            │
            ├─> Read Button A7
            │   └─> Debounce & Toggle PB2
            │
            ├─> Check Counter (INT1)
            │   └─> If cnt>5: Blink LED 3x
            │
            └─> Update LED Output
                └─> LED = PB1 | PB2 | Blink

┌─────────────────────────────────────────┐
│         SPI INTERRUPT (ISR)             │
└─────────────────────────────────────────┘
            │
            ├─> Receive byte from Master
            │
            ├─> State Machine:
            │   ├─> WAIT_SYNC1
            │   ├─> WAIT_SYNC2
            │   ├─> WAIT_ID
            │   ├─> WAIT_CMD
            │   ├─> WAIT_CHECKSUM
            │   └─> SEND_RESPONSE
            │
            └─> Load next byte to SPDR
```

### 3. Communication Sequence Diagram

```
PC          Master                    Slave
│             │                         │
│─── "13" ───>│                         │
│             │                         │
│             │─── [FF FE 01 01 CS] ──>│ CMD_READ_VALUE
│             │<── [FF FE 01 DATA CS]──│
│             │                         │
│             │─── [FF FE 01 02 CS] ──>│ CMD_READ_SENSOR
│             │<── [FF FE 01 H L CS]───│
│             │                         │
│             │─── [FF FE 01 03 CS] ──>│ CMD_READ_COUNTER
│             │<── [FF FE 01 DATA CS]──│
│             │                         │
│             │─── Save to EEPROM ────>│
│             │                         │
│<── ">>> 1 832" ──│                    │
│<── ">>> 1 832" ──│ (repeat 10ms)     │
│<── ">>> 1 833" ──│                    │
│             │                         │
│─── "STOP" ──>│                        │
│             │                         │
```

---

## 🚀 Quick Start Guide

### Step 1: Upload Firmware

**Master (Arduino Mega):**
```bash
# Upload master code ke Arduino Mega
# Port: /dev/ttyUSB0 atau COM3
# Board: Arduino Mega 2560
# Baud: 115200
```

**Slave (Arduino Uno/Nano):**
```bash
# Upload slave code ke setiap Arduino
# Ubah SLAVE_ID:
#   Slave 1: #define SLAVE_ID 0x01
#   Slave 2: #define SLAVE_ID 0x02
#   Slave 3: #define SLAVE_ID 0x03
# Port: /dev/ttyUSB1 atau COM4
# Baud: 9600
```

### Step 2: Hardware Setup

1. **Hubungkan SPI Bus:**
   - MOSI, MISO, SCK → Parallel ke semua Slave
   - SS terpisah untuk setiap Slave (A13, A14, A15)

2. **Hubungkan LCD ke Master:**
   - RS=47, EN=45, D4=37, D5=39, D6=43, D7=41

3. **Setup Slave Peripherals:**
   - A0: Analog sensor input
   - A7: Push button (PB2)
   - Pin 3: Interrupt input (INT1)
   - Pin 5: LED output
   - Pin 10: Additional input

### Step 3: Test Communication

1. **Open Serial Monitor (Master)**
   - Baud: 115200
   - Send: `13` → Read Slave 1 (PB2 + ADC)
   - Expected output:
     ```
     [S1 D3 - STOP to stop]
     >>> 1 832
     >>> 1 832
     >>> 1 833
     ```

2. **Test All Slaves**
   ```
   11  → Slave 1: PB2 only
   22  → Slave 2: ADC only
   37  → Slave 3: All data
   STOP → Stop display
   ```

---

## 📊 EEPROM Memory Map

```
Address Range  | Slave | Data
---------------|-------|------------------
0x00 - 0x03    | S1    | [PB2][ADC_H][ADC_L][CNT]
0x04 - 0x07    | S2    | [PB2][ADC_H][ADC_L][CNT]
0x08 - 0x0B    | S3    | [PB2][ADC_H][ADC_L][CNT]
```

**Data Format:**
- Byte 0: PB2 state (0 or 1)
- Byte 1: ADC High byte
- Byte 2: ADC Low byte
- Byte 3: Counter value

**Auto-Save:**
Data otomatis disimpan ke EEPROM setiap kali berhasil polling slave.

---

## 🖥️ LCD Display Format

LCD berganti tampilan setiap 500ms (rotasi S1→S2→S3):

```
┌────────────────┐
│S1 P:1 A:832    │  Line 1: Slave ID, PB2, ADC
│   C:5 OK       │  Line 2: Counter, Status
└────────────────┘

Status:
- OK = Komunikasi berhasil
- ER = Error komunikasi
```

---

## 🔧 Configuration & Tuning

### Timing Parameters (Master)

```cpp
// Display update interval
pcDisplayInterval = 10;     // 10ms = 100 updates/sec
                            // Range: 1-1000ms

// LCD refresh rate
lastLCDUpdate = 500;        // 500ms per page

// SPI Speed
SPI_CLOCK_DIV8             // 2 MHz
// Bisa diganti: DIV4 (4MHz), DIV16 (1MHz)
```

### Timing Parameters (Slave)

```cpp
// Debounce delay
lastDeb = 50;              // 50ms button debounce

// A0 toggle delay
lastA0ToggleTime = 1000;   // 1 second

// LED blink interval
lastBlinkTime = 300;       // 300ms per blink
```

### SPI Delays (Microseconds)

```cpp
// Dapat dikurangi jika komunikasi stabil:
delayMicroseconds(20);     // CS activation
delayMicroseconds(10);     // Between transfers
delayMicroseconds(50);     // Wait for slave response
```

---

## 🐛 Troubleshooting

### Problem: No data received (0xFF or 0xFFFF)

**Kemungkinan Penyebab:**
- SS pin tidak terhubung dengan benar
- Slave tidak aktif atau hang
- Checksum mismatch
- Timing terlalu cepat

**Solusi:**
```cpp
// Tambahkan delay di Master:
delayMicroseconds(100);    // After command send

// Check koneksi hardware
// Pastikan GND common
```

### Problem: Data corrupt atau acak

**Kemungkinan Penyebab:**
- SPI clock terlalu cepat
- Noise pada bus
- Missing pull-up/pull-down resistor

**Solusi:**
```cpp
// Perlambat SPI clock:
SPI.setClockDivider(SPI_CLOCK_DIV16);  // 1 MHz

// Tambah capacitor 100nF di setiap VCC/GND
// Gunakan kabel lebih pendek (<30cm)
```

### Problem: LCD tidak update

**Kemungkinan Penyebab:**
- Pin LCD salah
- Contrast tidak tepat

**Solusi:**
```cpp
// Check pin connection di:
LiquidCrystal lcd(47, 45, 37, 39, 43, 41);

// Adjust contrast potentiometer (biasanya 0.5-1.5V)
```

### Problem: Serial output terlalu lambat

**Kemungkinan Penyebab:**
- Baud rate tidak match
- Buffer overflow

**Solusi:**
```cpp
// Master: 115200
Serial.begin(115200);

// Pastikan Serial Monitor juga 115200
// Kurangi pcDisplayInterval jika perlu
```

---

## 📈 Performance Metrics

| Metric | Value | Notes |
|--------|-------|-------|
| Max Update Rate | 100 Hz | With 10ms interval |
| SPI Transfer Speed | 2 Mbps | DIV8 on 16MHz |
| Command Latency | ~2ms | Per slave poll |
| LCD Refresh | 2 Hz | 500ms rotation |
| Serial Baud | 115200 | Master only |

---

## 🔐 Error Handling

### Checksum Validation

Setiap paket menggunakan XOR checksum:

```cpp
// Request checksum
uint8_t checksum = slaveID ^ command;

// Response checksum (single byte)
uint8_t checksum = slaveID ^ data;

// Response checksum (multi-byte)
uint8_t checksum = slaveID ^ dataHigh ^ dataLow;
```

### Timeout Handling

Master menganggap gagal jika:
- Tidak menemukan sync pattern dalam 15 byte
- Checksum tidak valid
- Return value 0xFF atau 0xFFFF

### Slave State Reset

Slave reset state machine jika:
- SS pin HIGH (deselected)
- Invalid sync sequence
- Checksum mismatch

---

## 📚 Code Structure

### Master Components

```
master.ino
├── Setup & Init
│   ├── Serial (115200)
│   ├── SPI Master
│   ├── LCD 16x2
│   └── SS Pins
│
├── Main Loop
│   ├── LCD Update (non-blocking)
│   ├── PC Request Handler
│   └── Data Display Loop
│
├── Communication Functions
│   ├── readValue()       → CMD 0x01, 0x03
│   ├── readSensor()      → CMD 0x02
│   └── quickPollSlave()  → Full slave poll
│
├── Storage Functions
│   ├── saveSlaveToEEPROM()
│   └── loadSlaveFromEEPROM()
│
└── UI Functions
    ├── updateLCD()
    ├── handlePCRequest()
    └── fetchAndDisplay()
```

### Slave Components

```
slave.ino
├── Setup & Init
│   ├── Serial (9600)
│   ├── SPI Slave
│   ├── GPIO & Interrupts
│   └── ISR Registration
│
├── Main Loop
│   ├── Analog Read (A0)
│   ├── Button Read (A7)
│   ├── Counter Handler
│   └── LED Control
│
├── SPI ISR (Interrupt)
│   ├── State Machine
│   ├── Command Parser
│   └── Response Generator
│
└── Helper Functions
    ├── prepareResponse()
    └── isrInt1() [External INT]
```

---

## 🎓 Protocol Details

### Sync Pattern Recognition

Master mencari pattern `[0xFF][0xFE][SlaveID]` dalam buffer 15 byte:

```cpp
for (int i = 0; i < 10; i++) {
  if (response[i] == SYNC1 && 
      response[i+1] == SYNC2 && 
      response[i+2] == slaveID) {
    // Valid response found at position i
    uint8_t data = response[i+3];
    uint8_t checksum = response[i+4];
    // Validate checksum...
  }
}
```

### State Machine (Slave ISR)

```
WAIT_SYNC1 ──[0xFF]──> WAIT_SYNC2 ──[0xFE]──> WAIT_ID
                │                       │           │
                └──[other]───────────────┘           │
                                                     │
                                              [match ID]
                                                     │
                                                     ▼
                                                WAIT_CMD
                                                     │
                                              [any command]
                                                     │
                                                     ▼
                                              WAIT_CHECKSUM
                                                     │
                                          [validate checksum]
                                                     │
                                    ┌────────────────┴─────────────┐
                                [valid]                        [invalid]
                                    │                              │
                                    ▼                              │
                              SEND_RESPONSE                        │
                                    │                              │
                         [send all response bytes]                 │
                                    │                              │
                                    └──────────────────────────────┘
                                                     │
                                                     ▼
                                                WAIT_SYNC1
```

---

## 📝 Example Usage

### Example 1: Monitor Single Slave

```cpp
// PC Terminal:
13  // Start monitoring Slave 1 (PB2 + ADC)

// Output:
[S1 D3 - STOP to stop]
>>> 1 832
>>> 1 832
>>> 1 831
>>> 0 833
>>> 0 834

STOP  // Stop monitoring
[Display stopped]
```

### Example 2: Switch Between Slaves

```cpp
// Monitor Slave 1
17  // All data from Slave 1
>>> 1 832 5
>>> 1 832 5

// Switch to Slave 2
27  // All data from Slave 2
>>> 0 1023 10
>>> 0 1023 11

// Switch to Slave 3 specific data
32  // Only ADC from Slave 3
>>> 512
>>> 513
```

### Example 3: Read from EEPROM

Data di EEPROM selalu update setiap kali polling berhasil. PC request langsung baca dari EEPROM untuk performa maksimal.

```cpp
// Data disimpan otomatis:
Slave 1 → EEPROM 0x00-0x03
Slave 2 → EEPROM 0x04-0x07
Slave 3 → EEPROM 0x08-0x0B
```

---

## 🛠️ Advanced Features

### Custom Slave ID

Ubah ID di setiap slave:

```cpp
// slave.ino
#define SLAVE_ID 0x01  // Slave 1
#define SLAVE_ID 0x02  // Slave 2
#define SLAVE_ID 0x03  // Slave 3
```

### Add More Commands

Tambah command baru di protocol:

```cpp
// Define new command
#define CMD_READ_CUSTOM 0x04

// Implement di Slave (prepareResponse)
case CMD_READ_CUSTOM:
  responseBuffer[3] = customData;
  responseLength = 5;
  break;

// Implement di Master (buat fungsi baru)
uint8_t readCustom(uint8_t slaveID, uint8_t ssPin) {
  // Similar to readValue()
}
```

### Adjust Display Speed

```cpp
// Ultra fast (1000 updates/sec)
pcDisplayInterval = 1;

// Normal (100 updates/sec)
pcDisplayInterval = 10;

// Slow (10 updates/sec)
pcDisplayInterval = 100;
```

---

## 📜 License

MIT License - Feel free to use and modify

## 👥 Contributing

Pull requests are welcome! Please ensure:
- Code is well commented
- Timing parameters are documented
- Testing on real hardware

## 📧 Contact

For issues and questions, please open an issue on GitHub.
No WA: 085772644387 - Imam Arifin
IG: immarfn17

---

**Built with ❤️ for reliable embedded communication**
