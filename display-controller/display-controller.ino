#include "LedControl1.h"

// Wiring for Arduino Mega with hardware SPI:
// - DIN -> 51 (MOSI - hardware SPI, fixed pin)
// - CS  -> 12  pin needs to be specified
// - CLK -> 52 (SCK - hardware SPI, fixed pin)
// - VCC -> 5V
// - GND -> GND
// Note: With hardware SPI, MOSI and SCK pins are fixed by hardware

constexpr uint8_t PIN_CS = 12;

// Display grid dimensions
constexpr uint8_t NUM_ROWS = 10;
constexpr uint8_t DIGITS_PER_ROW = 24;
constexpr uint8_t TOTAL_DIGITS = NUM_ROWS * DIGITS_PER_ROW;

// Hardware configuration
constexpr uint8_t DIGITS_PER_DEVICE = 8;

// Derived hardware configuration
constexpr uint8_t DEVICES_PER_ROW = DIGITS_PER_ROW / DIGITS_PER_DEVICE;
constexpr uint8_t NUM_DEVICES = NUM_ROWS * DEVICES_PER_ROW;

int BASE_BRIGHTNESS = 3;  // normal brightness (0–15)
int currentBrightness = BASE_BRIGHTNESS;

LedControl lc = LedControl(51, 52, PIN_CS, NUM_DEVICES);
unsigned long lastReadyTime = 0;

// Buffer for binary data: 30 bytes (8 bits per byte, 240 squares total)
const uint8_t BYTES_TO_READ = 30;  // 240 squares / 8 bits = 30 bytes
uint8_t messageBuffer[BYTES_TO_READ];

void setup() {
  Serial.begin(57600);

  for (uint8_t d = 0; d < NUM_DEVICES; d++) {
    lc.shutdown(d, false);
    delayMicroseconds(100);
    lc.disableDisplayTest(d);
    delayMicroseconds(100);
  }

  // Now configure all devices
  for (uint8_t d = 0; d < NUM_DEVICES; d++) {
    lc.setScanLimit(d, 7);
    lc.setIntensity(d, currentBrightness);
    lc.clearDisplay(d);
  }
  rowTest();
  delay(1000); // Give time for test to complete before starting loop
}


void loop() {

  // HEARTBEAT - send READY periodically
  unsigned long now = millis();
  if (now - lastReadyTime > 1000) {
    Serial.println("READY");
    lastReadyTime = now;
  }

  uint8_t * message = readSerialMessage();
  
  if (message != NULL) {
    // Process the message and update displays
    processMessage(message);
  }
  
}

// ----------------- helpers -----------------

void setAllDigitsOff() {
  // Use clearDisplay - one SPI transfer per device
  for (uint8_t dev = 0; dev < NUM_DEVICES; dev++) {
    lc.clearDisplay(dev);
    if ((dev + 1) % 5 == 0) {
      delayMicroseconds(50);
    }
  }
  delayMicroseconds(100);
}


void rowTest() {
  // Display the row number (0-9) on all digits of all devices in this row
  setAllDigitsOff();
  delay(500);

  for (uint8_t row = 0; row < NUM_ROWS; row++) {
    uint8_t firstDevice = row * DEVICES_PER_ROW;

    for (uint8_t devOffset = 0; devOffset < DEVICES_PER_ROW; devOffset++) {
      uint8_t device = firstDevice + devOffset;
      for (uint8_t digit = 0; digit < DIGITS_PER_DEVICE; digit++) {
        lc.setDigit(device, digit, row, false);
        delayMicroseconds(50);
      }
    }

    delay(50);
  }

  delay(200);
  setAllDigitsOff();
}

// Read 30 bytes from serial (240 squares packed as 8 bits per byte)
uint8_t * readSerialMessage() {
  uint16_t available = Serial.available();
  
  // Only read complete frames - exactly 30 bytes
  if (available >= BYTES_TO_READ) {
    // If we have more than 30 bytes, we're behind - skip to latest frame
    if (available > BYTES_TO_READ) {
      // Discard old frames, keep only the latest 30 bytes
      uint16_t excess = available - BYTES_TO_READ;
      for (uint16_t i = 0; i < excess; i++) {
        Serial.read();
      }
    }
    // Read the complete frame (30 bytes)
    for (uint8_t i = 0; i < BYTES_TO_READ; i++) {
      messageBuffer[i] = Serial.read();
    }
    return messageBuffer;
  }
  return NULL;
}

// Process message bytes and update displays
void processMessage(uint8_t * bytes) {
  uint16_t squareIndex = 0;  // Track which square we're on (0-239)
  
  // Process each of the 30 bytes (240 bits total)
  for (uint8_t byteIndex = 0; byteIndex < BYTES_TO_READ; byteIndex++) {
    uint8_t byte = bytes[byteIndex];
    
    // Extract 8 bits from this byte (MSB first, as JS packs them)
    for (int8_t bit = 7; bit >= 0; bit--) {
      bool isActive = (byte >> bit) & 1;

      uint8_t row = squareIndex / DIGITS_PER_ROW;
      uint8_t physicalDigit = squareIndex % DIGITS_PER_ROW;
      uint8_t device = row * DEVICES_PER_ROW + (physicalDigit / DIGITS_PER_DEVICE);
      uint8_t digit = physicalDigit % DIGITS_PER_DEVICE;
      
      // Update display
      if (isActive) {
        lc.setDigit(device, digit, 8, false);
      } else {
        lc.setChar(device, digit, ' ', false);
      }
      
      squareIndex++;
    }
  }
}