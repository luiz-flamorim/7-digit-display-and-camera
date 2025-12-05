// Grid dimensions
let rows = 10;
let cols = 24;
const TOTAL_SQUARES = rows * cols; // 240

// 8 Digit Display dimensions
let digitRatio = 9 / 16;
let margin = 10;
let gap = 5;

// Threshold for detecting active squares
let threshold = 0.7;
let brightnessThreshold = 128;

// Sampling step for pixel analysis
let sampleStep = 3;

// Show camera feed
let showCamera = false;

let processedData = {
  // Message template with a total number of bytes equals to the number of squares: 0 = off, 8 = on
  activeSquares: new Uint8Array(TOTAL_SQUARES).fill(0), 
};

let thresholdSlider, invertButton, connectBtn;

let digitWidth,
  digitHeight,
  gridWidth,
  gridHeight,
  gridOffsetX,
  gridOffsetY,
  gridTextSize;

let video,
  videoBuffer,
  lastSent = new Uint8Array(TOTAL_SQUARES); // For comparison

// Serial communication
let port;
let sendInterval = 500; // Send data every 100ms
let lastSendTime = 0;
let arduinoReady = false;

let invertDetection = false;

function setup() {
  createCanvas(640, 480).parent("camera-container");

  setupSerial();
  calculateGridDimensions();
  setupVideo();
  setupControls();
}

function draw() {
  if (thresholdSlider) {
    threshold = thresholdSlider.value();
  }
  background(11, 11, 11);

  generateVideo();

  if (showCamera) {
    image(videoBuffer, 0, 0, width, height);
  }

  // Reset all bytes to 0 (off)
  processedData.activeSquares.fill(0);
  drawGrid(digitWidth, digitHeight);

  // Send data every 100ms (time-based) and only when data changes
  if (arduinoReady && port?.opened()) {
    const currentTime = millis();
    if (currentTime - lastSendTime >= sendInterval) {
      const arr = processedData.activeSquares;
      if (!arraysEqual(arr, lastSent)) {
        sendDataToSerial(arr);
        // Compare arrays to send only if it changes
        lastSent = new Uint8Array(arr);
      }
      lastSendTime = currentTime;
    }
  }

  readFromSerial();
}

// ------------------------------------------------------------------
// SETUP HELPERS
// ------------------------------------------------------------------

function setupSerial() {
  port = createSerial();

  connectBtn = select("#connect-btn");
  connectBtn.mousePressed(() => {
    if (!port.opened()) {
      console.log("Opening WebSerial chooser…");
      port.open(250000);
      connectBtn.html("Arduino Connected");
      connectBtn.addClass("active");
    } else {
      console.log("Closing serial port");
      port.close();
      connectBtn.html("Connect to Arduino");
      connectBtn.removeClass("active");
      arduinoReady = false;
      lastSendTime = 0; // Reset send timer on disconnect
    }
  });
}

function setupVideo() {
  video = createCapture(VIDEO);
  video.size(width, height);
  video.hide();
  videoBuffer = createGraphics(width, height);
  videoBuffer.pixelDensity(1);
}

function setupControls() {
  thresholdSlider = select("#threshold-slider");
  if (thresholdSlider) {
    thresholdSlider.value(threshold);

    const sliderElement = thresholdSlider.elt;

    const updateSliderFill = () => {
      const value = parseFloat(sliderElement.value);
      threshold = value;
      document.documentElement.style.setProperty("--slider-value", value);
    };

    sliderElement.addEventListener("input", updateSliderFill);
    sliderElement.addEventListener("mousemove", updateSliderFill);

    document.documentElement.style.setProperty("--slider-value", threshold);
  }

  invertButton = select("#invert-btn");
  if (invertButton) {
    invertButton.mousePressed(toggleInvertDetection);
    if (invertDetection) {
      invertButton.addClass("active");
    }
  }
}

function calculateGridDimensions() {
  const availableWidth = width - margin * 2;
  const availableHeight = height - margin * 2;
  const maxWidthFromHorizontal = (availableWidth - gap * (cols - 1)) / cols;
  const maxWidthFromVertical =
    ((availableHeight - gap * (rows - 1)) / rows) * digitRatio;
  digitWidth = min(maxWidthFromHorizontal, maxWidthFromVertical);
  digitHeight = digitWidth * (16 / 9); // maintain 9:16 proportion

  gridWidth = cols * digitWidth + gap * (cols - 1);
  gridHeight = rows * digitHeight + gap * (rows - 1);
  gridOffsetX = (width - gridWidth) / 2;
  gridOffsetY = (height - gridHeight) / 2;
  gridTextSize = min(digitWidth, digitHeight) * 0.4;
}

function sendDataToSerial(arr) {
  if (!port || !port.opened()) return;
  
  // Combine marker and data into a single buffer

  const marker = 0xFF;  // Marker helps Arduino distinguish binary data from "READY" text
  const combined = new Uint8Array(1 + arr.length);  // 1 byte for marker, arr.length bytes for data
  combined[0] = marker;
  combined.set(arr, 1);
  
  port.write(combined);
}

function readFromSerial() {
  if (!port || !port.opened()) return;
  let incoming = port.readUntil("\n"); // Read until newline
  if (incoming && incoming.length > 0) {
    if (incoming.includes("READY")) {
      arduinoReady = true;
      console.log(">>> Arduino is READY, starting data stream");
      // Update button text when Arduino confirms readiness
      if (connectBtn) {
        connectBtn.html("Arduino Connected");
        connectBtn.addClass("active");
      }
    }
  }
}

function drawGrid(digitWidth, digitHeight) {
  // Use cached grid calculations (calculated once in setup)
  stroke(200);
  strokeWeight(1);
  textAlign(CENTER, CENTER);
  textSize(gridTextSize);

  for (let row = 0; row < rows; row++) {
    for (let col = 0; col < cols; col++) {
      let x = gridOffsetX + col * (digitWidth + gap);
      let y = gridOffsetY + row * (digitHeight + gap);
      const avgBrightness = getRegionAverageBrightness(
        x,
        y,
        digitWidth,
        digitHeight
      );
      const currentIndex = row * cols + (cols - 1 - col);
      const isActive =
        avgBrightness !== null &&
        (invertDetection
          ? avgBrightness > brightnessThreshold
          : avgBrightness < brightnessThreshold);

      push();
      stroke(200);
      strokeWeight(1);
      if (isActive) {
        fill(220, 40, 40, 180);
        processedData.activeSquares[currentIndex] = 8; // Set to 8 (B01000) for on
      } else {
        noFill();
        // Already 0 from fill() above, but explicit for clarity
        processedData.activeSquares[currentIndex] = 0;
      }
      rect(x, y, digitWidth, digitHeight, 2);
      pop();

      push();
      noStroke();
      fill(240);
      text(currentIndex, x + digitWidth / 2, y + digitHeight / 2);
      pop();
    }
  }
}

function generateVideo() {
  if (!videoBuffer) return;
  videoBuffer.push();
  videoBuffer.clear();
  videoBuffer.translate(videoBuffer.width, 0);
  videoBuffer.scale(-1, 1);
  videoBuffer.image(video, 0, 0, videoBuffer.width, videoBuffer.height);
  videoBuffer.pop();
  videoBuffer.filter(THRESHOLD, threshold);
  videoBuffer.loadPixels();
}

function getRegionAverageBrightness(x, y, regionWidth, regionHeight) {
  if (!videoBuffer || !videoBuffer.pixels || videoBuffer.pixels.length === 0) {
    return null;
  }

  const xStart = constrain(Math.floor(x), 0, videoBuffer.width - 1);
  const yStart = constrain(Math.floor(y), 0, videoBuffer.height - 1);
  const xEnd = constrain(Math.floor(x + regionWidth), 0, videoBuffer.width);
  const yEnd = constrain(Math.floor(y + regionHeight), 0, videoBuffer.height);

  if (xEnd <= xStart || yEnd <= yStart) return null;

  let sum = 0;
  let count = 0;

  for (let yy = yStart; yy < yEnd; yy += sampleStep) {
    for (let xx = xStart; xx < xEnd; xx += sampleStep) {
      const idx = (yy * videoBuffer.width + xx) * 4;
      sum += videoBuffer.pixels[idx];
      count++;
    }
  }

  if (count === 0) return null;
  return sum / count;
}

function toggleInvertDetection() {
  invertDetection = !invertDetection;
  if (invertButton) {
    invertButton.html(
      invertDetection ? "Show brighter areas" : "Show darker areas"
    );
    if (invertDetection) {
      invertButton.addClass("active");
    } else {
      invertButton.removeClass("active");
    }
  }
}

function arraysEqual(a, b) {
  if (a === b) return true;
  if (!a || !b || a.length !== b.length) return false;
  // Compare Uint8Array byte by byte
  for (let i = 0; i < a.length; i++) {
    if (a[i] !== b[i]) return false;
  }
  return true;
}
