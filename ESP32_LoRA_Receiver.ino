#include <Wire.h>
#include <SPI.h>
#include <LiquidCrystal_I2C.h>

// =====================================================
// I2C LCD
// =====================================================
#define LCD_SDA 21
#define LCD_SCL 22

LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================================================
// SX1278 LoRa
// =====================================================
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_NSS    5
#define LORA_RST   14
#define LORA_DIO0  26

// =====================================================
// SX1278 REGISTERS
// =====================================================
#define REG_FIFO                 0x00
#define REG_OP_MODE              0x01
#define REG_FRF_MSB              0x06
#define REG_FRF_MID              0x07
#define REG_FRF_LSB              0x08
#define REG_MODEM_CONFIG1        0x1D
#define REG_MODEM_CONFIG2        0x1E
#define REG_MODEM_CONFIG3        0x26
#define REG_IRQ_FLAGS            0x12
#define REG_FIFO_RX_CURRENT_ADDR 0x10
#define REG_FIFO_RX_BASE_ADDR    0x0F
#define REG_FIFO_ADDR_PTR        0x0D
#define REG_RX_NB_BYTES          0x13
#define REG_VERSION              0x42

// =====================================================
// SX1278 MODES
// =====================================================
#define MODE_LONG_RANGE_MODE 0x80
#define MODE_SLEEP           0x00
#define MODE_STDBY           0x01
#define MODE_RX_CONTINUOUS   0x05

#define IRQ_RX_DONE_MASK     0x40
#define IRQ_PAYLOAD_CRC_ERROR 0x20

SPIClass loraSPI(VSPI);

// =====================================================
// WRITE REGISTER
// =====================================================
void writeRegister(uint8_t address, uint8_t value)
{
  digitalWrite(LORA_NSS, LOW);

  loraSPI.transfer(address | 0x80);
  loraSPI.transfer(value);

  digitalWrite(LORA_NSS, HIGH);
}

// =====================================================
// READ REGISTER
// =====================================================
uint8_t readRegister(uint8_t address)
{
  digitalWrite(LORA_NSS, LOW);

  loraSPI.transfer(address & 0x7F);
  uint8_t value = loraSPI.transfer(0x00);

  digitalWrite(LORA_NSS, HIGH);

  return value;
}

// =====================================================
// RESET LoRa
// =====================================================
void resetLoRa()
{
  digitalWrite(LORA_RST, LOW);
  delay(10);

  digitalWrite(LORA_RST, HIGH);
  delay(10);
}

// =====================================================
// INITIALIZE LoRa RECEIVER
// =====================================================
void initLoRa()
{
  resetLoRa();

  // LoRa + Sleep
  writeRegister(
    REG_OP_MODE,
    MODE_LONG_RANGE_MODE | MODE_SLEEP
  );

  delay(10);

  // LoRa + Standby
  writeRegister(
    REG_OP_MODE,
    MODE_LONG_RANGE_MODE | MODE_STDBY
  );

  delay(10);

  // ===================================================
  // 433 MHz
  // ===================================================
  writeRegister(REG_FRF_MSB, 0x6C);
  writeRegister(REG_FRF_MID, 0x40);
  writeRegister(REG_FRF_LSB, 0x00);

  // ===================================================
  // Same modem configuration as ESP1
  // ===================================================
  // BW = 125 kHz
  // Coding Rate = 4/5
  // Explicit Header
  writeRegister(REG_MODEM_CONFIG1, 0x72);

  // SF7
  // CRC enabled
  writeRegister(REG_MODEM_CONFIG2, 0x74);

  // Low Data Rate Optimization OFF
  writeRegister(REG_MODEM_CONFIG3, 0x04);

  // ===================================================
  // RX FIFO base address
  // ===================================================
  writeRegister(REG_FIFO_RX_BASE_ADDR, 0x00);

  writeRegister(REG_FIFO_ADDR_PTR, 0x00);

  // ===================================================
  // Continuous receive mode
  // ===================================================
  writeRegister(
    REG_OP_MODE,
    MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS
  );
}

// =====================================================
// RECEIVE LoRa PACKET
// =====================================================
String receiveLoRa()
{
  uint8_t irqFlags = readRegister(REG_IRQ_FLAGS);

  // No packet received
  if ((irqFlags & IRQ_RX_DONE_MASK) == 0)
  {
    return "";
  }

  // Clear RX DONE flag
  writeRegister(
    REG_IRQ_FLAGS,
    IRQ_RX_DONE_MASK
  );

  // Check CRC error
  if (irqFlags & IRQ_PAYLOAD_CRC_ERROR)
  {
    Serial.println("CRC ERROR!");

    // Clear CRC error
    writeRegister(
      REG_IRQ_FLAGS,
      IRQ_PAYLOAD_CRC_ERROR
    );

    return "";
  }

  // Get current RX FIFO address
  uint8_t currentAddress =
    readRegister(REG_FIFO_RX_CURRENT_ADDR);

  // Set FIFO pointer
  writeRegister(
    REG_FIFO_ADDR_PTR,
    currentAddress
  );

  // Number of received bytes
  uint8_t receivedLength =
    readRegister(REG_RX_NB_BYTES);

  String message = "";

  // Read FIFO
  for (uint8_t i = 0; i < receivedLength; i++)
  {
    char c = readRegister(REG_FIFO);
    message += c;
  }

  return message;
}

// =====================================================
// DISPLAY RECEIVED DATA
// =====================================================
void displayReceivedData(String packet)
{
  String hr = "NA";
  String spo2 = "NA";
  String temp = "NA";
  String hum = "NA";

  // ---------------------------------------------------
  // Find HR
  // ---------------------------------------------------
  int hrStart = packet.indexOf("HR:");

  if (hrStart >= 0)
  {
    int comma = packet.indexOf(",", hrStart);

    if (comma >= 0)
      hr = packet.substring(hrStart + 3, comma);
  }

  // ---------------------------------------------------
  // Find SPO2
  // ---------------------------------------------------
  int spo2Start = packet.indexOf("SPO2:");

  if (spo2Start >= 0)
  {
    int comma = packet.indexOf(",", spo2Start);

    if (comma >= 0)
      spo2 = packet.substring(spo2Start + 5, comma);
  }

  // ---------------------------------------------------
  // Find TEMP
  // ---------------------------------------------------
  int tempStart = packet.indexOf("TEMP:");

  if (tempStart >= 0)
  {
    int comma = packet.indexOf(",", tempStart);

    if (comma >= 0)
      temp = packet.substring(tempStart + 5, comma);
  }

  // ---------------------------------------------------
  // Find HUM
  // ---------------------------------------------------
  int humStart = packet.indexOf("HUM:");

  if (humStart >= 0)
  {
    hum = packet.substring(humStart + 4);
  }

  // ===================================================
  // Show on LCD
  // ===================================================

  // Screen 1
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("HR:");
  lcd.print(hr);
  lcd.print(" BPM");

  lcd.setCursor(0, 1);
  lcd.print("SpO2:");
  lcd.print(spo2);
  lcd.print("%");

  delay(1500);

  // Screen 2
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Temp:");
  lcd.print(temp);
  lcd.print(" C");

  lcd.setCursor(0, 1);
  lcd.print("Hum:");
  lcd.print(hum);
  lcd.print(" %");

  delay(1500);
}

// =====================================================
// SETUP
// =====================================================
void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("==========================================");
  Serial.println(" ESP2 - LoRa RECEIVER");
  Serial.println("==========================================");

  // ===================================================
  // I2C LCD
  // ===================================================
  Wire.begin(LCD_SDA, LCD_SCL);

  lcd.init();
  lcd.backlight();

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("LoRa Receiver");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  delay(1000);

  // ===================================================
  // LoRa pins
  // ===================================================
  pinMode(LORA_NSS, OUTPUT);
  digitalWrite(LORA_NSS, HIGH);

  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, HIGH);

  pinMode(LORA_DIO0, INPUT);

  // ===================================================
  // SPI
  // ===================================================
  loraSPI.begin(
    LORA_SCK,
    LORA_MISO,
    LORA_MOSI,
    LORA_NSS
  );

  Serial.println("SPI started.");

  // ===================================================
  // Initialize LoRa
  // ===================================================
  initLoRa();

  // ===================================================
  // Check SX1278
  // ===================================================
  uint8_t version = readRegister(REG_VERSION);

  Serial.print("SX1278 Version = 0x");
  Serial.println(version, HEX);

  if (version != 0x12)
  {
    Serial.println("ERROR: SX1278 not detected!");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("LoRa ERROR!");
    lcd.setCursor(0, 1);
    lcd.print("Check wiring");

    while (1)
    {
      delay(1000);
    }
  }

  Serial.println("SX1278 detected successfully.");

  // ===================================================
  // Ready
  // ===================================================
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("LoRa Receiver");
  lcd.setCursor(0, 1);
  lcd.print("Waiting Data...");

  Serial.println("------------------------------------------");
  Serial.println("ESP2 RECEIVER READY");
  Serial.println("Waiting for LoRa data...");
  Serial.println("------------------------------------------");
}

// =====================================================
// LOOP
// =====================================================
void loop()
{
  String packet = receiveLoRa();

  if (packet.length() > 0)
  {
    Serial.println();
    Serial.println("========== LoRa RX ==========");
    Serial.print("Received: ");
    Serial.println(packet);
    Serial.println("=============================");

    displayReceivedData(packet);

    // Return to receive mode
    writeRegister(
      REG_OP_MODE,
      MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS
    );
  }
}
