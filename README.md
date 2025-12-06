
# Project in Development


## Next steps

# Camera Processor - 7-Segment Display Matrix Controller

A scalable real-time video processing system that analyzes camera input and controls a matrix of 7-segment displays via Arduino. The system divides the camera feed into a configurable grid, detects brightness in each cell, and activates corresponding digits on a display matrix of any size.

## Overview

This project consists of two main components:

1. **JavaScript/Web Application** (p5.js): Captures video, processes it into a configurable grid, and sends active cell data to Arduino
2. **Arduino Firmware**: Receives binary data via serial communication and controls a scalable matrix of MAX7219 7-segment display modules

The system is designed to be scalable - grid dimensions and display size can be configured by adjusting constants in both the JavaScript and Arduino code. The grid maps directly to 7-segment digits arranged in a matrix layout.

## How It Works

### JavaScript Application (`app.js`)

#### Video Processing Pipeline

1. **Video Capture**: Uses p5.js `createCapture()` to access the webcam
2. **Image Processing**: Applies threshold filter to convert to black/white, samples pixels to calculate average brightness per grid cell

3. **Grid Analysis**:
   - Divides the 640×480 canvas into a grid defined by the user
   - Calculates average brightness for each cell region
   - Determines if a cell is "active" based on brightness threshold

4. **Data Transmission**:
   - Creates a binary array where each byte represents one grid cell (0 = off, 8 = on)
   - Only sends data when the array changes (optimization)
   - Sends at configurable intervals (time-based, default 200ms)
   - Uses binary format: marker byte (0xFF) + 240 data bytes (for 10×24 grid)
   - Format is scalable - array size = rows × cols

#### Key Functions

- **`setupVideo()`**: Initializes camera capture and video buffer
- **`generateVideo()`**: Processes video frame (flip, threshold, load pixels)
- **`calculateGridDimensions()`**: Calculates grid layout and cell positions
- **`drawGrid()`**: Renders the grid overlay and detects active cells
- **`getRegionAverageBrightness()`**: Samples pixels in a grid cell region
- **`sendDataToSerial()`**: Sends active cell indices to Arduino
- **`readFromSerial()`**: Listens for "READY" message from Arduino

#### Configuration Variables

- `rows = 10`: Number of grid rows (scalable)
- `cols = 24`: Number of grid columns (scalable)
- `threshold = 0.7`: Threshold filter intensity (0-1)
- `brightnessThreshold = 128`: Brightness cutoff for active detection (0-255)
- `sampleStep = 3`: Pixel sampling step (higher = faster, less accurate)
- `sendInterval = 200`: Time interval in milliseconds between data transmissions

### Arduino Firmware

#### Hardware Setup

The Arduino controls a scalable number of MAX7219 modules, each driving 8 7-segment digits. The number of modules is automatically calculated based on grid dimensions (rows × cols / 8).

- **Wiring** (Arduino Mega with hardware SPI):
  - `DIN` → Pin 51 (MOSI - hardware SPI, fixed pin)
  - `CLK` → Pin 52 (SCK - hardware SPI, fixed pin)
  - `CS` → Pin 12 (configurable)
  - `VCC` → 5V
  - `GND` → GND

#### Communication Protocol

1. **Handshake**: Arduino sends "READY" message every second
2. **Data Format**: Binary format - marker byte (0xFF) followed by data bytes (one per grid cell)
   - Each byte: 0 = off, 8 = on
   - Total bytes = rows × cols (e.g., 240 bytes for 10×24 grid)
3. **Baud Rate**: 57600

#### Main Loop Behavior

1. **Heartbeat**: Sends "READY" every second
2. **Data Reception**:
   - Detects marker byte (0xFF) to identify data packets
   - Reads binary data bytes (one per grid cell)
   - Resets brightness to normal level
   - Processes data using selective updates (only changes digits that changed)
   - Updates display device-by-device for optimal SPI performance

3. **Idle Fade-Out**:
   - If no data received for 5 seconds, starts fading
   - Decreases brightness by 1 every second (0-15 scale)
   - Creates smooth visual feedback when idle

#### Key Functions

- **`setAllDigitsOff()`**: Clears all digits across all devices
- **`setAllIntensity(uint8_t intensity)`**: Sets brightness for all devices
- **`rowTest()`**: Startup sequence showing row numbers on all devices
- **Selective Updates**: Only updates digits that changed from previous frame (optimization)
- **Device-by-Device Processing**: Processes all 8 digits of each device together for optimal SPI timing

#### Display Mapping
- Grid dimensions are configurable via constants (`NUM_ROWS`, `DIGITS_PER_ROW`)
- Number of devices is automatically calculated: `NUM_DEVICES = NUM_ROWS × (DIGITS_PER_ROW / 8)`
- JavaScript maps grid cells to display positions, accounting for column reversal
- When a cell is active, Arduino displays "8" (all segments) on the corresponding digit
- The system is scalable - change grid dimensions in both files to support different display sizes

## Usage

### Prerequisites

- Web browser with WebSerial API support (Chrome, Edge)
- Arduino IDE
- Required libraries:
  - `LedControl` (custom version: `LedControl1.h` for MAX7219)
  - Standard Arduino libraries: `string.h` (for `memcpy`)

### Setup

1. **Arduino**:
   - Upload the firmware to your Arduino
   - Connect MAX7219 modules as specified
   - Open Serial Monitor at 57600 baud (optional, for debugging)

2. **Web Application**:
   - Open `index.html` in a browser
   - Grant camera permissions when prompted
   - Click "Connect to Arduino" button
   - Select your Arduino from the serial port list

### Controls

- **Threshold Slider**: Adjusts the threshold filter intensity (0-1)
- **"Show darker areas" Button**: Toggles between detecting dark vs. bright areas
- **"Connect to Arduino" Button**: Opens/closes serial connection

### How to Use

1. Position yourself or objects in front of the camera
2. Adjust the threshold slider to fine-tune detection sensitivity
3. Active cells (detected areas) will light up corresponding digits on the display
4. The display automatically fades out after 5 seconds of inactivity

## Technical Details

### Optimization Features

- **Change Detection**: Only sends data when active cells change
- **Time-Based Transmission**: Sends at configurable intervals (default 200ms) instead of frame-based
- **Selective Updates**: Arduino only updates digits that changed from previous frame
- **Device-by-Device Processing**: Processes all digits of each device together for optimal SPI timing
- **Binary Data Format**: Efficient binary transmission instead of JSON (faster parsing)
- **Cached Calculations**: Grid dimensions calculated once at startup
- **Pixel Sampling**: Uses step size of 3 for faster processing
- **Previous Buffer Tracking**: Maintains previous frame state for change detection

### Serial Communication

- **Format**: Binary data
  - Marker byte: `0xFF` (identifies data packet)
  - Data bytes: One byte per grid cell (0 = off, 8 = on)
  - Total size: 1 marker + (rows × cols) data bytes
- **Example**: For 10×24 grid: `[0xFF, 0, 8, 0, 8, ...]` (241 bytes total)
- **Baud Rate**: 57600
- **No line endings**: Raw binary data

### Performance

- Video processing: ~60 FPS (depending on hardware)
- Serial transmission: ~5 updates/second (configurable, default 200ms interval)
- Grid analysis: Scalable - (rows × cols) cells analyzed per frame
- Pixel sampling: ~3×3 step reduces computation by ~9× per cell
- Display updates: Only changed digits updated (typically 10-50 operations vs 240)
- SPI efficiency: Device-by-device processing reduces bus contention

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
- p5.js (v1.7.0) - Creative coding library
- p5.webserial - WebSerial API wrapper for p5.js

### Arduino
- LedControl (custom version) - MAX7219 LED matrix driver
- Standard libraries: `string.h` (for memory operations)

## Scalability

This project is designed to be scalable. To change the display size:

1. **JavaScript** (`app.js`):
   - Modify `rows` and `cols` constants
   - Grid will automatically adjust

2. **Arduino** (`display-controller.ino`):
   - Modify `NUM_ROWS` and `DIGITS_PER_ROW` constants
   - Number of devices is automatically calculated: `NUM_DEVICES = NUM_ROWS × (DIGITS_PER_ROW / 8)`
   - All other constants are derived automatically

The system will work with any grid size as long as both files use matching dimensions.

