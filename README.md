# Camera Processor - 7-Segment Display Matrix Controller

A real-time video processing system that analyzes camera input and controls a matrix of 7-segment displays via Arduino. The system divides the camera feed into a configurable grid, detects brightness in each cell, and activates corresponding digits on a display matrix.

## Overview

This project consists of two main components:

1. **JavaScript/Web Application** (p5.js): Captures video, processes it into a grid, and sends data to Arduino
2. **Arduino Firmware**: Receives binary data via serial and controls MAX7219 7-segment display modules

The system is scalable - grid dimensions can be configured by adjusting constants in both files.

## How It Works

### JavaScript Application

**Video Processing:**
1. Captures video from webcam using p5.js
2. Applies threshold filter to convert to black/white
3. Divides canvas into a configurable grid (rows × cols)
4. Calculates average brightness for each grid cell
5. Determines if a cell is "active" based on brightness threshold

**Data Transmission:**
- Packs 8 grid cells into each byte (bit-packed format)
- Bits are packed MSB-first: first cell → MSB, 8th cell → LSB
- Sends at configurable intervals (default 200ms)
- Total bytes = (rows × cols) / 8
- Validates array length before sending

**Configuration:**
- `rows`: Number of grid rows
- `cols`: Number of grid columns
- `threshold`: Threshold filter intensity (0-1)
- `brightnessThreshold`: Brightness cutoff for active detection (0-255)
- `sendInterval`: Time interval between transmissions (milliseconds)

### Communication Protocol

**Format:**
- Bit-packed binary data (no marker byte)
- 8 grid cells packed into each byte (MSB-first)
- Each bit: 0 = off, 1 = on
- Total size: (rows × cols) / 8 bytes
- Baud rate: 57600

**Handshake:**
- Arduino sends "READY" message every second
- JavaScript waits for "READY" before sending data

**Frame Synchronization:**
- Arduino only processes complete frames (exact byte count)
- If buffer overflow, discards excess bytes and reads latest frame
- Prevents glitches from partial frame reads

### Arduino Firmware

**Hardware Setup:**
- Arduino Mega with hardware SPI
- MAX7219 modules (each drives 8 digits)
- Wiring:
  - `DIN` → Pin 51 (MOSI)
  - `CLK` → Pin 52 (SCK)
  - `CS` → Pin 12
  - `VCC` → 5V
  - `GND` → GND

**Operation:**
1. Sends "READY" heartbeat every second
2. Waits for complete frame (exact byte count)
3. Unpacks bits from each byte (MSB-first)
4. Maps each bit to physical display position (row, device, digit)
5. Updates displays: active = "8", inactive = blank

**Display Mapping:**
- Each row has digits numbered right to left
- Devices are organized: Device 0 (rightmost), Device 1 (middle), Device 2 (leftmost), etc.
- Number of devices = rows × (digits_per_row / 8)
- JavaScript processes columns in reverse and packs bits MSB-first
- Arduino maps squareIndex directly to physical digit position

## Usage

### Prerequisites

- Web browser with WebSerial API support (Chrome, Edge)
- Arduino IDE
- Required libraries:
  - `LedControl1.h` (custom version for MAX7219)
  - Hardware SPI library (built-in)

### Setup

1. **Arduino:**
   - Upload firmware to Arduino
   - Connect MAX7219 modules as specified
   - Open Serial Monitor at 57600 baud (optional)

2. **Web Application:**
   - Open `index.html` in browser
   - Grant camera permissions
   - Click "Connect to Arduino"
   - Select Arduino from serial port list

### Controls

- **Threshold Slider**: Adjusts threshold filter intensity
- **Invert Button**: Toggles between detecting dark vs. bright areas
- **Connect Button**: Opens/closes serial connection

## File Structure

```
camera-processor/
├── app.js          # Main JavaScript application (p5.js)
├── index.html      # HTML page with p5.js setup
├── style.css       # Styling
└── README.md       # This file

display-controller/
├── display-controller.ino  # Arduino firmware
├── LedControl1.h          # Custom LedControl library header
├── LedControl1.cpp        # Custom LedControl library implementation
└── README.md              # Arduino-specific documentation
```

## Dependencies

### JavaScript
- p5.js - Creative coding library
- p5.webserial - WebSerial API wrapper for p5.js

### Arduino
- LedControl1.h - Custom MAX7219 LED matrix driver
- Hardware SPI library (built-in)

## Scalability

To change the display size:

1. **JavaScript** (`app.js`): Modify `rows` and `cols` constants
2. **Arduino** (`display-controller.ino`): Modify `NUM_ROWS` and `DIGITS_PER_ROW` constants

Number of devices is automatically calculated: `NUM_DEVICES = NUM_ROWS × (DIGITS_PER_ROW / 8)`

The system works with any grid size as long as both files use matching dimensions.
