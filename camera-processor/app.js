let rows = 5;
let cols = 8;
let digitRatio = 9 / 16; // width : height for each digit
let margin = 10;
let gap = 5;
let threshold = 0.7;
let brightnessThreshold = 128;
let sampleStep = 3;
let showCamera = false;
let processedData = {
  activeSquares: [],
};
let thresholdSlider;
let invertButton;

let digitWidth;
let digitHeight;
let connectBtn;

// Cached grid calculations (don't change after setup)
let gridWidth;
let gridHeight;
let gridOffsetX;
let gridOffsetY;
let gridTextSize;

let video;
let videoBuffer;
let lastSent = [];

// ---- SERIAL ----
let port;
let sendEveryNFrames = 6;
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
  if (thresholdSlider) threshold = thresholdSlider.value();
  background(18);

  generateVideo();

  if (showCamera) {
    image(videoBuffer, 0, 0, width, height);
  }

  processedData.activeSquares = [];
  drawGrid(digitWidth, digitHeight);

  // Only check serial conditions when needed
  if (arduinoReady && port?.opened() && frameCount % sendEveryNFrames === 0) {
    const arr = processedData.activeSquares;
    if (!arraysEqual(arr, lastSent)) {
      sendDataToSerial(arr);
      lastSent = arr.slice(); // copy
    }
  }

  readFromSerial();
}

// ------------------------------------------------------------------
// SETUP HELPERS
// ------------------------------------------------------------------

function setupSerial() {
  port = createSerial();

  connectBtn = createButton("Connect to Arduino");
  connectBtn.parent("controls");
  connectBtn.mousePressed(() => {
    if (!port.opened()) {
      console.log("Opening WebSerial chooser…");
      port.open(57600);
    } else {
      console.log("Closing serial port");
      port.close();
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
  thresholdSlider = createSlider(0, 1, threshold, 0.01);
  thresholdSlider.parent("controls");
  thresholdSlider.addClass("threshold-slider");

  invertButton = createButton("Show darker areas");
  invertButton.parent("controls");
  invertButton.addClass("invert-toggle");
  invertButton.mousePressed(toggleInvertDetection);
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
  const jsonString = JSON.stringify(arr);
  // console.log("Sending to Arduino:", jsonString);
  port.write(jsonString);
  port.write("\n");
}

function readFromSerial() {
  if (!port || !port.opened()) return;
  let incoming = port.readUntil("\n");
  if (incoming && incoming.length > 0) {
    if (incoming.includes("READY")) {
      arduinoReady = true;
      console.log(">>> Arduino is READY, starting data stream");
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
        processedData.activeSquares.push(currentIndex);
      } else {
        noFill();
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
  }
}

function arraysEqual(a, b) {
  // Fast path: same reference or both empty
  if (a === b) return true;
  if (a.length !== b.length) return false;
  // For small arrays, simple loop is fastest
  for (let i = 0; i < a.length; i++) {
    if (a[i] !== b[i]) return false;
  }
  return true;
}
