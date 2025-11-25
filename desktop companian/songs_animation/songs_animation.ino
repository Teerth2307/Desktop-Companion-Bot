/*
 * IMU-Controlled Animated Face & Media Controller
 *
 * This sketch combines the "Animated Eyes for TFT_eSPI" with the
 * "IMU Media Control" sketch.
 *
 * It uses an MPU6050 IMU to detect gestures. These gestures:
 * 1. Send Serial commands (for the Python script)
 * 2. Trigger animations on the TFT display (face movement, expressions, size changes)
 *
 * GESTURES:
 * - Tilt Left:   Face moves left, sends "PREV"
 * - Tilt Right:  Face moves right, sends "NEXT"
 * - Tilt Forward: Eyes smaller, smile, sends "VOL_UP"
 * - Tilt Back:   Eyes larger, sad face, sends "VOL_DOWN"
 * - Lift Z-Axis: Screen flash animation, sends "PLAY_PAUSE"
 * - Flat:        Face centers, normal expression
 *
 * ======================================================================
 * ======================= IMPORTANT ====================================
 * ======================================================================
 * * This code WILL NOT WORK until you have correctly configured
 * the TFT_eSPI library for your specific display.
 * * You MUST edit the "User_Setup.h" file inside the
 * "TFT_eSPI" library folder to match your display driver (e.g., ILI9341)
 * and the ESP32 pins you are using for SPI (MOSI, SCLK, CS, DC, RST).
 *
 * You also need the Adafruit_MPU6050 and Adafruit_Sensor libraries.
 * ======================================================================
 */

// --- Display Libraries ---
#include "SPI.h"
#include "TFT_eSPI.h"
// --- IMU Libraries ---
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "logo.h"
#include <TJpg_Decoder.h>

// --- Display Object ---
TFT_eSPI tft = TFT_eSPI();

// --- IMU Object ---
Adafruit_MPU6050 mpu;

// --- IMU Thresholds ---
const float TILT_THRESHOLD = 5.0;
const float RESET_THRESHOLD = 2.0;
const float LIFT_THRESHOLD_Z = 10.5;

// --- IMU State Variables ---
bool trackLatched = false;     // Only locks Tracks and Play/Pause
unsigned long lastVolTime = 0; // Timer for Volume
const long VOL_COOLDOWN = 300; // 300ms delay between volume steps

// --- Face Dimensions (Base) ---
int baseEyeWidth = 60;
int baseEyeHeight = 70;
int eyeGap = 25;
int baseMouthWidth = 60;

// --- Face Positions (Calculated in setup) ---
int leftEyeX;
int rightEyeX;
int eyeY;
int mouthX;
int mouthY;

// --- Animation & Logic Variables ---
int moveSpeed = 5; // Speed for X-axis movement
int sizeSpeed = 4; // Speed for eye size change

// Blinking
int blinkState = 0; // 0 = open, 1 = closed
unsigned long lastBlinkTime = 0;
int blinkDelay = 4000;

// --- Expression State ---
enum Expression {
  NORMAL,
  SMILE,
  SAD,
  KAWAII
};
Expression currentExpression = NORMAL;
Expression previousExpression = NORMAL;

// --- Target Animation State ---
// These are the "goal" values set by the IMU
int targetOffsetX = 0;
int targetEyeWidth = baseEyeWidth;
int targetEyeHeight = baseEyeHeight;

// --- Current Animation State ---
// These values smoothly interpolate towards the targets
int offsetX = 0;
int currentEyeWidth = baseEyeWidth;
int currentEyeHeight = baseEyeHeight;

// --- Previous Animation State ---
// Used for flicker-free erasing
int previousOffsetX = 0;
int previousEyeWidth = baseEyeWidth;
int previousEyeHeight = baseEyeHeight;
int previousBlinkState = 0;


bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  // Stop further decoding as image is running off bottom of screen
  if ( y >= tft.height() ) return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);

  // Return 1 to decode next block
  return 1;
}


void setup() {
  // Start Serial for media control commands
  Serial.begin(115200);

  // Initialize the TFT display
  TJpgDec.setSwapBytes(true);

  // The jpeg decoder must be given the exact name of the rendering function above
  TJpgDec.setCallback(tft_output);
  tft.init();
  tft.setRotation(1); // Landscape mode

  

  TJpgDec.drawJpg(0, 0, logo, sizeof(logo));

  // Get screen dimensions
  int screenW = tft.width() ;
  int screenH = tft.height();

  // Calculate base face positions
  leftEyeX = (screenW / 2) - baseEyeWidth - (eyeGap / 2);
  rightEyeX = (screenW / 2) + (eyeGap / 2);
  eyeY = (screenH / 2) - (baseEyeHeight / 2) - 10;
  mouthX = (screenW / 2) - (baseMouthWidth / 2);
  mouthY = eyeY + baseEyeHeight + 25;

  // Fill the screen with black ONCE at the start
  tft.fillScreen(TFT_BLACK);

  // Initialize MPU6050
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    tft.println("MPU6050 FAILED");
    while (1) delay(10);
  }
  Serial.println("MPU6050 Found!");

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  delay(100);
}

void loop() {
  unsigned long currentTime = millis();

  // --- 1. Blinking logic (runs independently) ---
  if (currentTime - lastBlinkTime > blinkDelay && blinkState == 0) {
    blinkState = 1; // Blink starts
    lastBlinkTime = currentTime;
  } else if (currentTime - lastBlinkTime > 150 && blinkState == 1) {
    blinkState = 0; // Blink ends
    lastBlinkTime = currentTime;
  }

  // --- 2. Read IMU ---
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // --- 3. Process IMU Gestures ---
  // This is the main logic block. It checks the IMU data and sets
  // the "target" state for the face. The animations will then
  // smoothly interpolate towards these targets.

  // Start by assuming the state won't change
  Expression newExpression = currentExpression;
  int newTargetOffsetX = targetOffsetX;
  int newTargetEyeWidth = targetEyeWidth;
  int newTargetEyeHeight = targetEyeHeight;

  // --- GESTURE: NEUTRAL RESET ---
  // Checks if the device is flat (within RESET_THRESHOLD)
  // ***** FIX: Replaced broken line with the full, correct 'if' statement *****
  if (abs(a.acceleration.x) < RESET_THRESHOLD &&
      abs(a.acceleration.y) < RESET_THRESHOLD &&
      a.acceleration.z < 11.0 && a.acceleration.z > 8.0)
  {
    trackLatched = false; // Unlock tracks
    
    // --- COMMAND: SET FACE TO NORMAL ---
    newExpression = NORMAL;
    newTargetOffsetX = 0; // Center X
    newTargetEyeWidth = baseEyeWidth; // Normal eye size
    newTargetEyeHeight = baseEyeHeight;
  }
  
  // --- GESTURE: VOLUME (Forward/Back) ---
  // Checks for tilt on the Y-axis (forward/back)
  // Only runs if the VOL_COOLDOWN timer has expired
  if (millis() - lastVolTime > VOL_COOLDOWN)
  {
    // Tilted FORWARD (Y-axis positive)
    if (a.acceleration.y > TILT_THRESHOLD) {
      Serial.println("VOL_UP"); // Send command to Python
      lastVolTime = millis();
      
      // --- COMMAND: SET FACE TO "VOL UP" ---
      newExpression = SMILE;
      newTargetEyeWidth = baseEyeWidth - 15; // Smaller eyes
      newTargetEyeHeight = baseEyeHeight - 15;
    }
    // Tilted BACK (Y-axis negative)
    else if (a.acceleration.y < -TILT_THRESHOLD) {
      Serial.println("VOL_DOWN"); // Send command to Python
      lastVolTime = millis();
      
      // --- COMMAND: SET FACE TO "VOL DOWN" ---
      newExpression = SAD;
      newTargetEyeWidth = baseEyeWidth + 10; // Bigger eyes
      newTargetEyeHeight = baseEyeHeight + 10;
    }
  }
  
  // --- GESTURE: PLAY/PAUSE (Lift Z-Axis) ---
  // Checks for a sharp "lift" on the Z-axis (up)
  // Only runs if tracks are not "latched"
  if (a.acceleration.z > LIFT_THRESHOLD_Z && !trackLatched)
  {
    Serial.println("PLAY_PAUSE"); // Send command to Python
    trackLatched = true; // Lock it!
    
    // --- COMMAND: RUN PLAY/PAUSE ANIMATION ---
    playPauseAnimation();
    
    // After animation, force a redraw of the face
    previousOffsetX = -999; // Hack to force redraw
    return; // Skip the rest of this loop
  }
  
  // --- GESTURE: TRACK (Left/Right) ---
  // Checks for tilt on the X-axis (left/right)
  // Only runs if tracks are not "latched"
  
  // Tilted RIGHT (X-axis positive)
  if (a.acceleration.x > TILT_THRESHOLD && !trackLatched)
  {
    Serial.println("NEXT"); // Send command to Python
    trackLatched = true; // Lock it!
    
    // --- COMMAND: SET FACE TO "NEXT" ---
    newTargetOffsetX = 40; // Move right
    return;
  }
  // Tilted LEFT (X-axis negative)
  else if (a.acceleration.x < -TILT_THRESHOLD && !trackLatched)
  {
    Serial.println("PREV"); // Send command to Python
    trackLatched = true; // Lock it!
    
    // --- COMMAND: SET FACE TO "PREV" ---
    newTargetOffsetX = -40; // Move left
    return;
  }

  // --- 4. Update Target Animation States ---
  targetOffsetX = newTargetOffsetX;
  targetEyeWidth = newTargetEyeWidth;
  targetEyeHeight = newTargetEyeHeight;
  currentExpression = newExpression; // Expressions change instantly

  // --- 5. Smooth Interpolation ---
  // Move current values towards target values
  offsetX += (targetOffsetX - offsetX) / moveSpeed;
  currentEyeWidth += (targetEyeWidth - currentEyeWidth) / sizeSpeed;
  currentEyeHeight += (targetEyeHeight - currentEyeHeight) / sizeSpeed;

  // --- 6. Drawing Code ---
  // Only redraw if the face has moved, blinked, changed expression, or resized
  if (offsetX != previousOffsetX ||
      blinkState != previousBlinkState ||
      currentExpression != previousExpression ||
      currentEyeWidth != previousEyeWidth)
  {
    // STEP 1: Erase the OLD face (at old position and size)
    
    // Erase left eye
    if (previousBlinkState == 0) {
      // ***** FLICKER FIX *****
      // Use fillRect to erase the *entire* bounding box
      // of the previous eye, preventing corner artifacts.
      tft.fillRect(leftEyeX + previousOffsetX, eyeY, previousEyeWidth, previousEyeHeight, TFT_BLACK);
    } else {
      // This was already a fillRect, so it's fine.
      tft.fillRect(leftEyeX + previousOffsetX, eyeY + previousEyeHeight / 2 - 2, previousEyeWidth, 4, TFT_BLACK);
    }
    
    // Erase right eye
    if (previousBlinkState == 0) {
      // ***** FLICKER FIX *****
      // Use fillRect here too.
      tft.fillRect(rightEyeX + previousOffsetX, eyeY, previousEyeWidth, previousEyeHeight, TFT_BLACK);
    } else {
      tft.fillRect(rightEyeX + previousOffsetX, eyeY + previousEyeHeight / 2 - 2, previousEyeWidth, 4, TFT_BLACK);
    }
    
    // Erase old mouth
    drawMouth(previousExpression, TFT_BLACK, previousOffsetX);
    
    



    // STEP 2: Draw the NEW face (at new position and size)
    // Draw left eye
    if (blinkState == 0) {
      drawEye(leftEyeX + offsetX, eyeY, currentEyeWidth, currentEyeHeight, TFT_WHITE);
    } else {
      tft.fillRect(leftEyeX + offsetX, eyeY + currentEyeHeight / 2 - 2, currentEyeWidth, 4, TFT_WHITE);
    }
    // Draw right eye
    if (blinkState == 0) {
      drawEye(rightEyeX + offsetX, eyeY, currentEyeWidth, currentEyeHeight, TFT_WHITE);
    } else {
      tft.fillRect(rightEyeX + offsetX, eyeY + currentEyeHeight / 2 - 2, currentEyeWidth, 4, TFT_WHITE);
    }
    // Draw new mouth
    drawMouth(currentExpression, TFT_WHITE, offsetX);

    // STEP 3: Update the "previous" state
    previousOffsetX = offsetX;
    previousEyeWidth = currentEyeWidth;
    previousEyeHeight = currentEyeHeight;
    previousBlinkState = blinkState;
    previousExpression = currentExpression;
  }

  delay(30); // Control loop speed
}

// --- Face Drawing Functions ---

// Special animation for Play/Pause
void playPauseAnimation() {
  //uint16_t colors[] = { TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW, TFT_CYAN, TFT_MAGENTA };
   tft.fillScreen(TFT_BLACK);
  for (int i = 0; i < 3; i++) {
    
    TJpgDec.drawJpg(80, 40, logo, sizeof(logo));
    //tft.fillScreen(colors[random(0, 6)]);
    delay(50);
  }
  tft.fillScreen(TFT_BLACK); // End on black
  lastBlinkTime = millis(); // Reset blink timer after animation
}

// Function to draw a single eye
void drawEye(int eyeX, int eyeY, int eWidth, int eHeight, uint16_t color) {
  // Use a dynamic radius based on height
  int radius = eHeight / 4;
  tft.fillRoundRect(eyeX, eyeY, eWidth, eHeight, radius, color);
}

// Function to draw the mouth
void drawMouth(Expression expression, uint16_t color, int mOffsetX) {
  // Apply X-axis offset to all mouth coordinates
  int mX = mouthX + mOffsetX;
  int mW = baseMouthWidth; // Mouth width doesn't change

  // ***** FLICKER FIX *****
  // If the color is BLACK, we are erasing.
  // Erase the *entire* bounding box that the mouth could *ever* occupy.
  // This is the simplest, most robust way to prevent artifacts.
  if (color == TFT_BLACK) {
    // This bounding box (mX, mouthY, mW, 25) is calculated to be
    // large enough to cover ALL possible mouth expressions.
    tft.fillRect(mX, mouthY, mW, 25, TFT_BLACK);
    return; // IMPORTANT: Exit after erasing
  }

  // --- DRAW LOGIC ---
  // If color is not BLACK, draw the expression.
  switch (expression) {
    case SMILE:
      // Draw a white filled shape
      tft.fillRoundRect(mX, mouthY, mW, 18, 9, color);
      // Draw a black shape over it, shifted up, to create a crescent
      if (color == TFT_WHITE) {
        tft.fillRoundRect(mX, mouthY - 6, mW, 14, 7, TFT_BLACK);
      }
      break;
    case SAD:
      // Draw a black filled shape first (as a mask)
      tft.fillRoundRect(mX, mouthY, mW, 18, 9, TFT_BLACK);
      // Draw a white shape over it, shifted down, to create a downward crescent
      tft.fillRoundRect(mX, mouthY + 6, mW, 14, 7, color);
      break;
    case KAWAII:
      // Draw a 'w' mouth with 3 circles
      tft.fillCircle(mX + (mW / 4), mouthY + 5, 8, color);
      tft.fillCircle(mX + (mW / 2), mouthY, 8, color);
      tft.fillCircle(mX + (mW * 3 / 4), mouthY + 5, 8, color);
      break;
    case NORMAL:
    default:
      // Draw a simple horizontal rounded rect
      tft.fillRoundRect(mX, mouthY + 4, mW, 6, 3, color);
      break;
  }
}