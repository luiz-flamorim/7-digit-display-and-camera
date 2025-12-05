#include "LedControl1.h"
#include <string.h>  // For memcpy

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

// Buffer for binary data: 240 bytes (one per digit)
uint8_t dataBuffer[TOTAL_DIGITS];
uint8_t previousBuffer[TOTAL_DIGITS];  // Track previous frame state for selective updates
const uint8_t DATA_MARKER = 0xFF;      // Marker byte sent before binary data

unsigned long lastDataTime = 0;
const unsigned long TIMEOUT = 5000;
const unsigned long FADE_STEP_MS = 1000;
unsigned long lastFadeStepTime = 0;

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
  // Initialize previous buffer to all zeros
  for (uint8_t i = 0; i < TOTAL_DIGITS; i++) {
    previousBuffer[i] = 0;
  }
  rowTest();
}


void loop() {

  // HEARTBEAT - send READY periodically
  unsigned long now = millis();
  if (now - lastReadyTime > 1000) {
    Serial.println("READY");
    lastReadyTime = now;
  }

  // ---- handle incoming data ----
  if (Serial.available()) {
    // Check if incoming byte is the data marker
    uint8_t peekByte = Serial.peek();

    if (peekByte == DATA_MARKER) {
      Serial.read();  // Consume marker byte

      // Read 240 data bytes
      uint8_t bytesRead = 0;
      unsigned long startWait = millis();
      const unsigned long MAX_WAIT = 1000;

      while (bytesRead < TOTAL_DIGITS && (millis() - startWait) < MAX_WAIT) {
        while (Serial.available() > 0 && bytesRead < TOTAL_DIGITS) {
          dataBuffer[bytesRead] = Serial.read();
          bytesRead++;
        }

        // If bytes are not available yet, wait a bit for more to arrive
        if (bytesRead < TOTAL_DIGITS) {
          delay(2);
        }
      }

      if (bytesRead == TOTAL_DIGITS) {
        // Successfully received all 240 bytes - process data
        lastDataTime = millis();
        currentBrightness = BASE_BRIGHTNESS;
        setAllIntensity(currentBrightness);

        // Selective updates: Process device-by-device, only update changed digits
        for (uint8_t device = 0; device < NUM_DEVICES; device++) {
          for (uint8_t digit = 0; digit < DIGITS_PER_DEVICE; digit++) {

            uint8_t row = device / DEVICES_PER_ROW;
            uint8_t deviceInRow = device % DEVICES_PER_ROW;
            uint8_t col = deviceInRow * DIGITS_PER_DEVICE + digit;
            uint8_t globalIndex = row * DIGITS_PER_ROW + col;

            if (globalIndex >= TOTAL_DIGITS) continue;  // Safety check

            uint8_t currentValue = dataBuffer[globalIndex];
            uint8_t previousValue = previousBuffer[globalIndex];

            // Only update if value changed
            if (currentValue != previousValue) {
              if (currentValue == 8) {
                lc.setDigit(device, digit, 8, false);
              } else {
                lc.setChar(device, digit, ' ', false);
              }
              delayMicroseconds(10);
            }
          }
          delayMicroseconds(20);
        }

        // Update previous buffer for next comparison (memcpy library for efficiency)
        memcpy(previousBuffer, dataBuffer, TOTAL_DIGITS);
        delayMicroseconds(100);

        if (Serial.available() > 50) {
          while (Serial.available() > 0 && Serial.peek() != DATA_MARKER) {
            Serial.read();  // Discard non-marker bytes
          }
        }
      }
      // If not enough bytes received, just wait for next frame
    } else {
      if (Serial.available() > 100) {
        while (Serial.available() > 0 && Serial.peek() != DATA_MARKER) {
          Serial.read();
        }
      } else {
        // Just discard this one byte
        Serial.read();
      }
    }
  }

  // Fade out when idle
  if (now - lastDataTime > TIMEOUT) {
    if (now - lastFadeStepTime > FADE_STEP_MS) {
      lastFadeStepTime = now;

      if (currentBrightness > 0) {
        currentBrightness--;
        setAllIntensity(currentBrightness);
      } else {
        // option to set all off
        // setAllDigitsOff();
      }
    }
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

// Helper function to flush serial buffer
void flushSerialBuffer() {
  while (Serial.available() > 0) {
    Serial.read();
  }
}

// Helper function to set intensity for all devices
void setAllIntensity(uint8_t intensity) {
  for (uint8_t d = 0; d < NUM_DEVICES; d++) {
    lc.setIntensity(d, intensity);
  }
}