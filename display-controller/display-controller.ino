// #include <LedControl.h>
#include "LedControl1.h"
// ArduinoJson no longer needed - using binary bytes instead

// Wiring for Arduino Mega with hardware SPI:
// - DIN -> 51 (MOSI - hardware SPI, fixed pin)
// - CS  -> 12  || blue (user-selectable)
// - CLK -> 52 (SCK - hardware SPI, fixed pin)
// - VCC -> 5V  || red
// - GND -> GND || black
// Note: With hardware SPI, MOSI and SCK pins are fixed by hardware
// Only CS pin needs to be specified

constexpr uint8_t PIN_CS = 12;

constexpr uint8_t NUM_DEVICES = 30;                                 // 30 devices total
constexpr uint8_t DIGITS_PER_DEVICE = 8;                           // 8 digits each
constexpr uint8_t TOTAL_DIGITS = NUM_DEVICES * DIGITS_PER_DEVICE;  // 240 total digits
constexpr uint8_t DEVICES_PER_ROW = 3;                             // 3 devices per row
constexpr uint8_t NUM_ROWS = 10;                                    // 10 rows
int BASE_BRIGHTNESS = 3;                                           // normal brightness (0–15)
int currentBrightness = BASE_BRIGHTNESS;                           // 0–15

// Note: dataPin and clkPin parameters are ignored with hardware SPI
// Hardware SPI uses fixed pins: MOSI=51, SCK=52 on Mega
LedControl lc = LedControl(51, 52, PIN_CS, NUM_DEVICES);
unsigned long lastReadyTime = 0;
bool firstFrameReceived = false;

// Buffer for binary data: 240 bytes (one per digit)
uint8_t dataBuffer[TOTAL_DIGITS];
const uint8_t DATA_MARKER = 0xFF;  // Marker byte sent before binary data

unsigned long lastDataTime = 0;
const unsigned long TIMEOUT = 5000;       // ms of silence before starting fade
const unsigned long FADE_STEP_MS = 1000;  // how often to step the fade
unsigned long lastFadeStepTime = 0;

// Sync recovery: if no valid data for this long, flush buffer and resync
const unsigned long SYNC_RECOVERY_TIMEOUT = 2000;  // 2 seconds
unsigned long lastValidDataTime = 0;

void setup() {
  Serial.begin(250000);  // Increased baud rate for faster communication

  // Explicitly disable display test for all devices first
  // This ensures display test mode is off before other operations
  for (uint8_t d = 0; d < NUM_DEVICES; d++) {
    lc.shutdown(d, false);  // Wake up device
    delayMicroseconds(100);  // Small delay for command processing
    lc.disableDisplayTest(d);  // Explicitly disable display test mode
    delayMicroseconds(100);  // Small delay for command processing
  }
  
  // Now configure all devices
  for (uint8_t d = 0; d < NUM_DEVICES; d++) {
    lc.setScanLimit(d, 7);
    lc.setIntensity(d, currentBrightness);
    lc.clearDisplay(d);
  }

  setAllDigitsOff();
  rowTest();
}


void loop() {

  // HEARTBEAT until first valid frame
  if (!firstFrameReceived && millis() - lastReadyTime > 1000) {
    Serial.println("READY");
    lastReadyTime = millis();
  }

  // ---- Sync recovery: if we haven't received valid data for a while, flush and resync ----
  unsigned long now = millis();
  if (firstFrameReceived && (now - lastValidDataTime > SYNC_RECOVERY_TIMEOUT)) {
    // We're out of sync - flush the buffer completely
    while (Serial.available() > 0) {
      Serial.read();
    }
    lastValidDataTime = now; // Reset timer
  }

  // ---- handle incoming data ----
  if (Serial.available()) {
    // Check if incoming byte is the data marker (0xFF)
    uint8_t peekByte = Serial.peek();
    
    if (peekByte == DATA_MARKER) {
      // Binary data incoming: consume marker and read 240 bytes
      Serial.read(); // Consume the marker byte
      
      // Read bytes incrementally as they arrive (data comes in chunks)
      uint8_t bytesRead = 0;
      unsigned long startWait = millis();
      const unsigned long MAX_WAIT = 1000; // Timeout to 1000ms
      
      while (bytesRead < TOTAL_DIGITS && (millis() - startWait) < MAX_WAIT) {
        // Read as many bytes as are available, up to what we need
        while (Serial.available() > 0 && bytesRead < TOTAL_DIGITS) {
          dataBuffer[bytesRead] = Serial.read();
          bytesRead++;
        }
        
        // If we don't have all bytes yet, wait a bit for more to arrive
        if (bytesRead < TOTAL_DIGITS) {
          delay(2); // Allow more bytes to accumulate
        }
      }
      
      if (bytesRead == TOTAL_DIGITS) {
        // Validate data: bytes should only be 0 (off) or 8 (on)
        // Reject corrupted data that has invalid values
        bool dataValid = true;
        for (uint8_t idx = 0; idx < TOTAL_DIGITS; idx++) {
          if (dataBuffer[idx] != 0 && dataBuffer[idx] != 8) {
            dataValid = false;
            break;
          }
        }
        
        if (dataValid) {
          // Successfully received all 240 bytes with valid data
          firstFrameReceived = true;
          lastValidDataTime = millis(); // Update sync timer

          // VALID DATA RECEIVED → reset timers and brightness
          lastDataTime = millis();
          currentBrightness = BASE_BRIGHTNESS;
          for (uint8_t d = 0; d < NUM_DEVICES; d++) {
            lc.setIntensity(d, currentBrightness);
          }

          // Clear ALL digits first - ensure clean state
          setAllDigitsOff();
          delayMicroseconds(500); // Small delay to ensure clear is complete

          // Process all 240 bytes: if byte == 8, turn on that digit
          for (uint8_t idx = 0; idx < TOTAL_DIGITS; idx++) {
            if (dataBuffer[idx] == 8) {
              setDigitOn(idx);
            }
          }
          
          // Conservative buffer clearing: only clear bytes that are NOT a new marker
          // This prevents clearing bytes from the next message if they've already started arriving
          delay(10); // Small delay to let next message's marker arrive if it's coming
          while (Serial.available() > 0) {
            uint8_t nextByte = Serial.peek();
            if (nextByte == DATA_MARKER) {
              // Stop clearing - we've found the next message's marker
              break;
            }
            Serial.read(); // Discard non-marker bytes
          }
        } else {
          // Invalid data detected - reject and clear buffer
          while (Serial.available() > 0) {
            Serial.read();
          }
        }
      } else {
        // Not enough bytes received - partial message, clear it
        while (Serial.available() > 0) {
          Serial.read();
        }
      }
    } else {
      // Not the marker byte - could be leftover data or "READY" text
      // Only discard if buffer is getting full to prevent overflow
      if (Serial.available() > 100) {
        // Buffer is getting full - flush it to prevent overflow
        while (Serial.available() > 0 && Serial.peek() != DATA_MARKER) {
          Serial.read();
        }
      } else {
        // Just discard this one byte
        Serial.read();
      }
    }
  }

  // ---- fade out when idle ----

  // Only start fading after TIMEOUT has passed since last data
  if (now - lastDataTime > TIMEOUT) {
    // Step the fade every FADE_STEP_MS
    if (now - lastFadeStepTime > FADE_STEP_MS) {
      lastFadeStepTime = now;

      if (currentBrightness > 0) {
        currentBrightness--;
        for (uint8_t d = 0; d < NUM_DEVICES; d++) {
          lc.setIntensity(d, currentBrightness);
        }
      } else {
        // once fully dim, you can also clear segments if you like
        // setAllDigitsOff();
      }
    }
  }
}




// ----------------- helpers -----------------

void setAllDigitsOff() {
  // Optimized: Use clearDisplay which is much faster - one SPI transfer per device
  // instead of 8 individual setChar calls (8x faster)
  for (uint8_t dev = 0; dev < NUM_DEVICES; dev++) {
    lc.clearDisplay(dev);
  }
}

void setDigitOn(uint8_t globalIndex) {
  if (globalIndex >= TOTAL_DIGITS) return;

  uint8_t device = globalIndex / DIGITS_PER_DEVICE;  // 0..4
  uint8_t digit = globalIndex % DIGITS_PER_DEVICE;   // 0..7

  lc.setDigit(device, digit, 8, false);  // show "8"
}

void rowTest() {
  // Clear all displays first
  setAllDigitsOff();
  delay(500);
  
  // For each row (0-9), show the row number on all devices in that row
  for (uint8_t row = 0; row < NUM_ROWS; row++) {
    // Calculate the first device index for this row
    uint8_t firstDevice = row * DEVICES_PER_ROW;
    
    // Display the row number (0-9) on all digits of all 3 devices in this row
    for (uint8_t devOffset = 0; devOffset < DEVICES_PER_ROW; devOffset++) {
      uint8_t device = firstDevice + devOffset;
      if (device < NUM_DEVICES) {
        // Show the row number on all 8 digits of this device
        for (uint8_t digit = 0; digit < DIGITS_PER_DEVICE; digit++) {
          lc.setDigit(device, digit, row, false);
          delayMicroseconds(50);  // Small delay between digit updates to allow processing
        }
      }
    }
    
    delay(50);  // Show each row for 800ms
  }
  
  // Clear all at the end
  delay(500);
  setAllDigitsOff();
}

void initialTest(uint8_t numDevices, uint8_t digitsPerDevice) {
  // show module index in all 8 digits
  for (uint8_t d = 0; d < numDevices; d++) {
    lc.clearDisplay(d);
    for (uint8_t i = 0; i < digitsPerDevice; i++) {
      lc.setDigit(d, i, d, false);  // show 0..4
    }
    delay(400);
    lc.clearDisplay(d);
  }

  // quick scan test (all segments on/off)
  for (uint8_t d = 0; d < numDevices; d++) lc.clearDisplay(d);

  for (uint8_t d = 0; d < numDevices; d++) {
    for (uint8_t i = 0; i < digitsPerDevice; i++) {
      lc.setChar(d, i, ' ', true);
    }
    delay(200);
    for (uint8_t i = 0; i < digitsPerDevice; i++) {
      lc.setChar(d, i, ' ', false);
    }
  }
}