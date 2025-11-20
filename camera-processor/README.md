# Camera Processor - 7-Segment Display Matrix Controller

A real-time video processing system that analyzes camera input and controls a matrix of 7-segment displays via Arduino. The system divides the camera feed into a grid, detects brightness in each cell, and activates corresponding digits on a 40-digit display matrix.

## Overview

This project consists of two main components:

1. **JavaScript/Web Application** (p5.js): Captures video, processes it into a grid, and sends active cell data to Arduino
2. **Arduino Firmware**: Receives data via serial communication and controls a matrix of MAX7219 7-segment display modules

The system creates a 5×8 grid (40 cells) that maps directly to 40 7-segment digits arranged in 5 modules of 8 digits each.

## How It Works

### JavaScript Application (`app.js`)

#### Video Processing Pipeline

1. **Video Capture**: Uses p5.js `createCapture()` to access the webcam
2. **Image Processing**:
   - Flips the video horizontally (mirror effect)
   - Applies threshold filter to convert to black/white
   - Samples pixels to calculate average brightness per grid cell

3. **Grid Analysis**:
   - Divides the 640×480 canvas into a 5×8 grid (40 cells)
   - Each cell corresponds to one 7-segment digit
   - Calculates average brightness for each cell region
   - Determines if a cell is "active" based on brightness threshold

4. **Data Transmission**:
   - Collects indices of active cells into an array
   - Only sends data when the array changes (optimization)
   - Sends every 6th frame to reduce serial traffic
   - Uses JSON format: `[16, 2, 3, 4, 30, 1]` (array of active digit indices)

#### Key Functions

- **`setupVideo()`**: Initializes camera capture and video buffer
- **`generateVideo()`**: Processes video frame (flip, threshold, load pixels)
- **`calculateGridDimensions()`**: Calculates grid layout and cell positions
- **`drawGrid()`**: Renders the grid overlay and detects active cells
- **`getRegionAverageBrightness()`**: Samples pixels in a grid cell region
- **`sendDataToSerial()`**: Sends active cell indices to Arduino
- **`readFromSerial()`**: Listens for "READY" message from Arduino

#### Configuration Variables

- `rows = 5`: Number of grid rows
- `cols = 8`: Number of grid columns
- `threshold = 0.7`: Threshold filter intensity (0-1)
- `brightnessThreshold = 128`: Brightness cutoff for active detection (0-255)
- `sampleStep = 3`: Pixel sampling step (higher = faster, less accurate)
- `sendEveryNFrames = 6`: Frame skip for serial transmission

### Arduino Firmware

#### Hardware Setup

The Arduino controls **5 MAX7219 modules**, each driving **8 7-segment digits**:
- **Total digits**: 40 (5 modules × 8 digits)
- **Wiring**:
  - `DIN` → Pin 11 (green)
  - `CLK` → Pin 13 (orange)
  - `CS` → Pin 12 (blue)
  - `VCC` → 5V (red)
  - `GND` → GND (black)

#### Communication Protocol

1. **Handshake**: Arduino sends "READY" message every second until first data received
2. **Data Format**: JSON array of digit indices `[0, 1, 2, ...]`
3. **Baud Rate**: 57600

#### Main Loop Behavior

1. **Heartbeat**: Sends "READY" until first valid frame received
2. **Data Reception**:
   - Reads JSON array from serial
   - Validates data format
   - Resets brightness to normal level
   - Clears all digits
   - Activates digits corresponding to received indices

3. **Idle Fade-Out**:
   - If no data received for 5 seconds, starts fading
   - Decreases brightness by 1 every second (0-15 scale)
   - Creates smooth visual feedback when idle

#### Key Functions

- **`setAllDigitsOff()`**: Clears all 40 digits
- **`setDigitOn(uint8_t globalIndex)`**: 
  - Converts global index (0-39) to device/digit coordinates
  - Displays "8" on the specified digit (all segments lit)
- **`initialTest()`**: Startup sequence showing module indices

#### Display Mapping

The 40 digits are indexed linearly:
- **Device 0**: Digits 0-7
- **Device 1**: Digits 8-15
- **Device 2**: Digits 16-23
- **Device 3**: Digits 24-31
- **Device 4**: Digits 32-39

When a cell is active in the JavaScript grid, the corresponding digit index is sent, and Arduino displays "8" (all segments) on that digit.

## Grid Mapping

The JavaScript grid (5 rows × 8 cols) maps to Arduino digits (0-39):

```
Row 0: [39, 38, 37, 36, 35, 34, 33, 32]  ← Reversed
Row 1: [31, 30, 29, 28, 27, 26, 25, 24]
Row 2: [23, 22, 21, 20, 19, 18, 17, 16]
Row 3: [15, 14, 13, 12, 11, 10,  9,  8]
Row 4: [ 7,  6,  5,  4,  3,  2,  1,  0]
```

Note: The grid is reversed horizontally (right-to-left) to match the physical display layout.

## Usage

### Prerequisites

- Web browser with WebSerial API support (Chrome, Edge)
- Arduino IDE
- Required libraries:
  - `LedControl` (for MAX7219)
  - `ArduinoJson` (for JSON parsing)

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
- **Frame Skipping**: Sends every 6th frame to reduce serial traffic
- **Cached Calculations**: Grid dimensions calculated once at startup
- **Pixel Sampling**: Uses step size of 3 for faster processing

### Serial Communication

- **Format**: JSON array of integers
- **Example**: `[16, 2, 3, 4, 30, 1]`
- **Baud Rate**: 57600
- **Line Ending**: Newline (`\n`)

### Performance

- Video processing: ~60 FPS (depending on hardware)
- Serial transmission: ~10 updates/second (every 6th frame)
- Grid analysis: 40 cells analyzed per frame
- Pixel sampling: ~3×3 step reduces computation by ~9× per cell

## File Structure

```
camera-processor/
├── app.js          # Main JavaScript application (p5.js)
├── index.html      # HTML page with p5.js setup
├── style.css       # Styling (if present)
└── README.md       # This file
```

## Dependencies

### JavaScript
- p5.js (v1.7.0) - Creative coding library
- p5.webserial - WebSerial API wrapper for p5.js

### Arduino
- LedControl - MAX7219 LED matrix driver
- ArduinoJson - JSON parsing library

## Future Enhancements

- Camera selection dropdown (for multiple cameras)
- Configurable grid size
- Different display patterns (not just "8")
- Color mapping for different brightness levels
- Save/load configuration presets

