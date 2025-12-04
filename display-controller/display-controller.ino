// #include <LedControl.h>
#include "LedControl1.h"
#include <ArduinoJson.h>

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

StaticJsonDocument<512> in;  // buffer

unsigned long lastDataTime = 0;
const unsigned long TIMEOUT = 5000;       // ms of silence before starting fade
const unsigned long FADE_STEP_MS = 1000;  // how often to step the fade
unsigned long lastFadeStepTime = 0;

void setup() {
  Serial.begin(230400);  // Increased baud rate for faster communication

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

  // ---- handle incoming data ----
  if (Serial.available()) {
    String message = Serial.readStringUntil('\n');

    in.clear();
    DeserializationError err = deserializeJson(in, message);

    if (err) {
      // optional debug:
      // Serial.print("JSON error: ");
      // Serial.println(err.c_str());
      return;
    }

    if (!in.is<JsonArray>()) {
      // Serial.println("Not an array");
      return;
    }

    JsonArray arr = in.as<JsonArray>();
    firstFrameReceived = true;

    // VALID DATA RECEIVED → reset timers and brightness
    lastDataTime = millis();
    currentBrightness = BASE_BRIGHTNESS;
    for (uint8_t d = 0; d < NUM_DEVICES; d++) {
      lc.setIntensity(d, currentBrightness);
    }

    setAllDigitsOff();

    for (JsonVariant v : arr) {
      int idx = v.as<int>();
      if (idx >= 0 && idx < TOTAL_DIGITS) {
        setDigitOn((uint8_t)idx);
      }
    }
  }

  // ---- fade out when idle ----
  unsigned long now = millis();

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
    
    delay(800);  // Show each row for 800ms
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