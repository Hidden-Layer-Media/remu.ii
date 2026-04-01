#include "TouchInterface.h"
#include "../SystemCore/SystemCore.h"
#include "../DisplayManager/DisplayManager.h"
#include <EEPROM.h>

// EEPROM addresses for calibration data
#define EEPROM_CALIBRATION_ADDR 100
#define CALIBRATION_MAGIC_NUMBER 0xCAFE

TouchInterface::TouchInterface() :
    lastReadTime(0),
    lastPressTime(0),
    lastReleaseTime(0),
    gestureStartTime(0),
    touchActive(false),
    gestureActive(false),
    tapCount(0)
{
    // Initialize touch point
    currentTouch = {0, 0, 0, 0, 0, false, false, false, false, 0};
    lastTouch = currentTouch;
    
    // Initialize gesture
    currentGesture = {GESTURE_NONE, {0}, {0}, {0}, 0, 0, 0, 0.0f};
    
    // Initialize calibration with default values
    calibration = {
        200, 3800,    // xMin, xMax (typical 4-wire resistive values)
        300, 3700,    // yMin, yMax
        0, 0,         // offsetX, offsetY
        1.0f, 1.0f,   // scaleX, scaleY
        false         // isCalibrated
    };
}

TouchInterface::~TouchInterface() {
    shutdown();
}

bool TouchInterface::initialize() {
    Serial.println("[TouchInterface] Initializing 4-wire resistive touch...");
    
    // TOUCH_YP (GPIO34) is input-only on ESP32 - never set as OUTPUT
    // XP, XM, YM are bidirectional GPIOs
    pinMode(TOUCH_XP, INPUT);
    pinMode(TOUCH_XM, OUTPUT);
    pinMode(TOUCH_YM, OUTPUT);
    // TOUCH_YP: input-only, no pinMode needed
    
    digitalWrite(TOUCH_XM, LOW);
    digitalWrite(TOUCH_YM, LOW);
    
    loadCalibration();
    
    Serial.println("[TouchInterface] Touch interface initialized");
    Serial.printf("[TouchInterface] Calibrated: %s\n", calibration.isCalibrated ? "YES" : "NO");
    
    return true;
}

void TouchInterface::update() {
    unsigned long currentTime = millis();
    
    // Sample touch at regular intervals
    if (currentTime - lastReadTime >= 10) { // 100Hz sampling
        sampleTouch();
        processTouch();
        detectGestures();
        lastReadTime = currentTime;
    }
}

void TouchInterface::shutdown() {
    pinMode(TOUCH_XP, INPUT);
    pinMode(TOUCH_XM, INPUT);
    pinMode(TOUCH_YM, INPUT);
    // TOUCH_YP (GPIO34) is always input-only
    Serial.println("[TouchInterface] Touch interface shutdown");
}

uint16_t TouchInterface::readTouchX() {
    // Drive XP high, XM low; read on TOUCH_YP (input-only GPIO34)
    pinMode(TOUCH_XP, OUTPUT);
    pinMode(TOUCH_XM, OUTPUT);
    // TOUCH_YP is input-only - leave as-is
    pinMode(TOUCH_YM, INPUT);
    
    digitalWrite(TOUCH_XP, HIGH);
    digitalWrite(TOUCH_XM, LOW);
    
    delayMicroseconds(20);
    return analogRead(TOUCH_YP);
}

uint16_t TouchInterface::readTouchY() {
    // Drive YM low, XP high; read on TOUCH_XM (bidirectional)
    // Cannot drive TOUCH_YP (GPIO34 input-only), so drive from XP side
    pinMode(TOUCH_XP, OUTPUT);
    pinMode(TOUCH_YM, OUTPUT);
    pinMode(TOUCH_XM, INPUT);
    // TOUCH_YP stays as input
    
    digitalWrite(TOUCH_XP, HIGH);
    digitalWrite(TOUCH_YM, LOW);
    
    delayMicroseconds(20);
    return analogRead(TOUCH_XM);
}

uint16_t TouchInterface::readTouchPressure() {
    // Standard Z1/Z2 resistive touch pressure measurement
    // Set XP=LOW, YM=HIGH, read Z1 on XM and Z2 on YP
    pinMode(TOUCH_XP, OUTPUT);
    pinMode(TOUCH_YM, OUTPUT);
    pinMode(TOUCH_XM, INPUT);
    pinMode(TOUCH_YP, INPUT);
    
    digitalWrite(TOUCH_XP, LOW);
    digitalWrite(TOUCH_YM, HIGH);
    
    delayMicroseconds(20);
    
    uint16_t z1 = analogRead(TOUCH_XM); // rises with pressure
    uint16_t z2 = analogRead(TOUCH_YP); // falls with pressure
    
    // Pressure proportional to z1/(z2-z1); clamp to 0-1023
    uint16_t pressure = 0;
    if (z2 > z1) {
        pressure = (uint16_t)constrain((z1 * 1023UL) / (z2 - z1 + 1), 0, 1023);
    }
    
    return pressure;
}

bool TouchInterface::isTouchPressed() {
    uint16_t pressure = readTouchPressure();
    return (pressure > PRESSURE_THRESHOLD);
}

void TouchInterface::sampleTouch() {
    TouchPoint samples[TOUCH_SAMPLES];
    uint8_t validSamples = 0;
    
    // Take multiple samples for noise reduction
    for (uint8_t i = 0; i < TOUCH_SAMPLES; i++) {
        if (isTouchPressed()) {
            samples[validSamples].rawX = readTouchX();
            samples[validSamples].rawY = readTouchY();
            samples[validSamples].pressure = readTouchPressure();
            samples[validSamples].isPressed = true;
            samples[validSamples].timestamp = millis();
            validSamples++;
        }
        delayMicroseconds(100); // Small delay between samples
    }
    
    // Store previous state
    lastTouch = currentTouch;
    currentTouch.wasPressed = currentTouch.isPressed;
    
    if (validSamples >= TOUCH_SAMPLES / 2) {
        // Average the valid samples
        currentTouch = averageReadings(samples, validSamples);
        currentTouch.isPressed = true;
        
        // Apply calibration
        applyCalibration(currentTouch);
    } else {
        // No valid touch detected
        currentTouch.isPressed = false;
        currentTouch.pressure = 0;
        currentTouch.timestamp = millis();
    }
    
    // Detect touch state changes
    currentTouch.isNewPress = (!currentTouch.wasPressed && currentTouch.isPressed);
    currentTouch.isNewRelease = (currentTouch.wasPressed && !currentTouch.isPressed);
}

TouchPoint TouchInterface::averageReadings(TouchPoint* samples, uint8_t count) {
    if (count == 0) return {0, 0, 0, 0, 0, false, false, false, false, 0};
    
    uint32_t sumX = 0, sumY = 0, sumPressure = 0;
    
    for (uint8_t i = 0; i < count; i++) {
        sumX += samples[i].rawX;
        sumY += samples[i].rawY;
        sumPressure += samples[i].pressure;
    }
    
    TouchPoint averaged;
    averaged.rawX = sumX / count;
    averaged.rawY = sumY / count;
    averaged.pressure = sumPressure / count;
    averaged.timestamp = millis();
    
    return averaged;
}

void TouchInterface::processTouch() {
    unsigned long currentTime = millis();
    
    // Handle debouncing
    if (currentTouch.isNewPress) {
        if (currentTime - lastReleaseTime < DEBOUNCE_DELAY) {
            // Too soon after release, ignore this press
            currentTouch.isPressed = false;
            currentTouch.isNewPress = false;
            return;
        }
        lastPressTime = currentTime;
        touchActive = true;
    }
    
    if (currentTouch.isNewRelease) {
        if (currentTime - lastPressTime < DEBOUNCE_DELAY) {
            // Too soon after press, ignore this release
            currentTouch.isPressed = true;
            currentTouch.isNewRelease = false;
            return;
        }
        lastReleaseTime = currentTime;
        touchActive = false;
    }
}

void TouchInterface::detectGestures() {
    unsigned long currentTime = millis();
    
    if (currentTouch.isNewPress) {
        // Start new gesture
        gestureStartTime = currentTime;
        currentGesture.startPoint = currentTouch;
        currentGesture.currentPoint = currentTouch;
        currentGesture.type = GESTURE_NONE;
        gestureActive = true;
        
        // Check for double tap
        if (currentTime - lastReleaseTime < DOUBLE_TAP_TIME) {
            tapCount++;
            if (tapCount >= 2) {
                currentGesture.type = GESTURE_DOUBLE_TAP;
                tapCount = 0;
            }
        } else {
            tapCount = 1;
        }
    }
    
    if (touchActive && gestureActive) {
        currentGesture.currentPoint = currentTouch;
        currentGesture.duration = currentTime - gestureStartTime;
        
        // Calculate movement
        currentGesture.deltaX = currentTouch.x - currentGesture.startPoint.x;
        currentGesture.deltaY = currentTouch.y - currentGesture.startPoint.y;
        
        float distance = calculateDistance(currentGesture.startPoint, currentTouch);
        
        // Detect gesture types
        if (currentGesture.duration > LONG_PRESS_TIME && distance < DRAG_THRESHOLD) {
            currentGesture.type = GESTURE_LONG_PRESS;
        } else if (distance > DRAG_THRESHOLD) {
            if (currentGesture.type == GESTURE_NONE || currentGesture.type == GESTURE_DRAG_START) {
                currentGesture.type = GESTURE_DRAG_START;
            } else {
                currentGesture.type = GESTURE_DRAG_MOVE;
            }
        }
    }
    
    if (currentTouch.isNewRelease && gestureActive) {
        currentGesture.endPoint = currentTouch;
        currentGesture.duration = currentTime - gestureStartTime;
        
        float distance = calculateDistance(currentGesture.startPoint, currentGesture.endPoint);
        currentGesture.velocity = calculateVelocity(currentGesture.startPoint, currentGesture.endPoint, currentGesture.duration);
        
        // Finalize gesture type
        if (currentGesture.type == GESTURE_DRAG_START || currentGesture.type == GESTURE_DRAG_MOVE) {
            currentGesture.type = GESTURE_DRAG_END;
        } else if (distance > SWIPE_THRESHOLD && currentGesture.velocity > SWIPE_MIN_VELOCITY) {
            currentGesture.type = detectSwipeDirection(currentGesture.deltaX, currentGesture.deltaY);
        } else if (currentGesture.type == GESTURE_NONE && tapCount == 1) {
            currentGesture.type = GESTURE_TAP;
        }
        
        gestureActive = false;
    }
}

int16_t TouchInterface::mapX(uint16_t rawX) {
    if (!calibration.isCalibrated) {
        // Default mapping without calibration
        return map(rawX, calibration.xMin, calibration.xMax, 0, SCREEN_WIDTH);
    }
    
    float x = (rawX - calibration.xMin) * calibration.scaleX + calibration.offsetX;
    return constrain((int16_t)x, 0, SCREEN_WIDTH - 1);
}

int16_t TouchInterface::mapY(uint16_t rawY) {
    if (!calibration.isCalibrated) {
        // Default mapping without calibration
        return map(rawY, calibration.yMin, calibration.yMax, 0, SCREEN_HEIGHT);
    }
    
    float y = (rawY - calibration.yMin) * calibration.scaleY + calibration.offsetY;
    return constrain((int16_t)y, 0, SCREEN_HEIGHT - 1);
}

void TouchInterface::applyCalibration(TouchPoint& point) {
    point.x = mapX(point.rawX);
    point.y = mapY(point.rawY);
}

float TouchInterface::calculateVelocity(TouchPoint start, TouchPoint end, unsigned long timeMs) {
    if (timeMs == 0) return 0.0f;
    
    float distance = calculateDistance(start, end);
    return distance / (timeMs / 1000.0f); // pixels per second
}

float TouchInterface::calculateDistance(TouchPoint p1, TouchPoint p2) {
    int16_t dx = p2.x - p1.x;
    int16_t dy = p2.y - p1.y;
    return sqrt(dx * dx + dy * dy);
}

TouchGesture TouchInterface::detectSwipeDirection(int16_t deltaX, int16_t deltaY) {
    if (abs(deltaX) > abs(deltaY)) {
        return (deltaX > 0) ? GESTURE_SWIPE_RIGHT : GESTURE_SWIPE_LEFT;
    } else {
        return (deltaY > 0) ? GESTURE_SWIPE_DOWN : GESTURE_SWIPE_UP;
    }
}

TouchPoint TouchInterface::getCurrentTouch() {
    return currentTouch;
}

bool TouchInterface::hasNewGesture() const {
    return (currentGesture.type != GESTURE_NONE && !gestureActive);
}

void TouchInterface::clearGesture() {
    currentGesture.type = GESTURE_NONE;
}

bool TouchInterface::startCalibration() {
    Serial.println("[TouchInterface] Starting calibration...");
    calibration.isCalibrated = false;
    return true;
}

bool TouchInterface::calibratePoint(int16_t screenX, int16_t screenY) {
    // Wait for touch
    Serial.printf("[TouchInterface] Touch calibration point at (%d, %d)\n", screenX, screenY);
    
    unsigned long startTime = millis();
    while (millis() - startTime < 10000) { // 10 second timeout
        update();
        if (currentTouch.isNewPress) {
            // Store calibration point
            if (screenX == 0 && screenY == 0) {
                // Top-left corner
                calibration.xMin = currentTouch.rawX;
                calibration.yMin = currentTouch.rawY;
            } else if (screenX == SCREEN_WIDTH - 1 && screenY == SCREEN_HEIGHT - 1) {
                // Bottom-right corner
                calibration.xMax = currentTouch.rawX;
                calibration.yMax = currentTouch.rawY;
            }
            
            Serial.printf("[TouchInterface] Calibration point recorded: raw(%d, %d)\n", 
                         currentTouch.rawX, currentTouch.rawY);
            return true;
        }
        delay(10);
    }
    
    Serial.println("[TouchInterface] Calibration timeout");
    return false;
}

bool TouchInterface::finishCalibration() {
    // Calculate scaling factors
    calibration.scaleX = (float)SCREEN_WIDTH / (calibration.xMax - calibration.xMin);
    calibration.scaleY = (float)SCREEN_HEIGHT / (calibration.yMax - calibration.yMin);
    calibration.offsetX = 0;
    calibration.offsetY = 0;
    calibration.isCalibrated = true;
    
    // Save calibration
    saveCalibration();
    
    Serial.println("[TouchInterface] Calibration complete");
    printCalibrationInfo();
    
    return true;
}

void TouchInterface::loadCalibration() {
    uint16_t magic;
    EEPROM.get(EEPROM_CALIBRATION_ADDR, magic);
    
    if (magic == CALIBRATION_MAGIC_NUMBER) {
        EEPROM.get(EEPROM_CALIBRATION_ADDR + 2, calibration);
        Serial.println("[TouchInterface] Calibration loaded from EEPROM");
    } else {
        Serial.println("[TouchInterface] No valid calibration found, using defaults");
    }
}

void TouchInterface::saveCalibration() {
    uint16_t magic = CALIBRATION_MAGIC_NUMBER;
    EEPROM.put(EEPROM_CALIBRATION_ADDR, magic);
    EEPROM.put(EEPROM_CALIBRATION_ADDR + 2, calibration);
    EEPROM.commit();
    Serial.println("[TouchInterface] Calibration saved to EEPROM");
}

void TouchInterface::resetCalibration() {
    calibration.isCalibrated = false;
    calibration.xMin = 200;
    calibration.xMax = 3800;
    calibration.yMin = 300;
    calibration.yMax = 3700;
    calibration.scaleX = calibration.scaleY = 1.0f;
    calibration.offsetX = calibration.offsetY = 0;
    
    Serial.println("[TouchInterface] Calibration reset to defaults");
}

bool TouchInterface::isPointInRect(TouchPoint point, int16_t x, int16_t y, int16_t w, int16_t h) {
    return (point.x >= x && point.x < x + w && point.y >= y && point.y < y + h);
}

bool TouchInterface::isPointInCircle(TouchPoint point, int16_t centerX, int16_t centerY, int16_t radius) {
    int16_t dx = point.x - centerX;
    int16_t dy = point.y - centerY;
    return (dx * dx + dy * dy <= radius * radius);
}

bool TouchInterface::isTouchInButton(int16_t x, int16_t y, int16_t w, int16_t h) {
    return (currentTouch.isNewPress && isPointInRect(currentTouch, x, y, w, h));
}

uint8_t TouchInterface::getTouchGridPosition(uint8_t gridCols, uint8_t gridRows) {
    if (!currentTouch.isPressed) return 255;
    
    uint8_t col = (currentTouch.x * gridCols) / SCREEN_WIDTH;
    uint8_t row = (currentTouch.y * gridRows) / SCREEN_HEIGHT;
    
    if (col >= gridCols) col = gridCols - 1;
    if (row >= gridRows) row = gridRows - 1;
    
    return row * gridCols + col;
}

void TouchInterface::setPressureThreshold(uint16_t threshold) {
    // Update the global threshold - would need to modify hardware_pins.h
    Serial.printf("[TouchInterface] Pressure threshold set to %d\n", threshold);
}

uint16_t TouchInterface::getPressureThreshold() const {
    return PRESSURE_THRESHOLD;
}

void TouchInterface::printTouchInfo() {
    Serial.printf("[TouchInterface] Touch: (%d,%d) Raw: (%d,%d) Pressure: %d Pressed: %s\n",
                 currentTouch.x, currentTouch.y, currentTouch.rawX, currentTouch.rawY, 
                 currentTouch.pressure, currentTouch.isPressed ? "YES" : "NO");
}

void TouchInterface::printCalibrationInfo() {
    Serial.println("[TouchInterface] Calibration Data:");
    Serial.printf("  X Range: %d - %d\n", calibration.xMin, calibration.xMax);
    Serial.printf("  Y Range: %d - %d\n", calibration.yMin, calibration.yMax);
    Serial.printf("  Scale: %.3f, %.3f\n", calibration.scaleX, calibration.scaleY);
    Serial.printf("  Offset: %d, %d\n", calibration.offsetX, calibration.offsetY);
    Serial.printf("  Calibrated: %s\n", calibration.isCalibrated ? "YES" : "NO");
}

String TouchInterface::getTouchStatusString() {
    String status = "Touch: ";
    if (currentTouch.isPressed) {
        status += "(" + String(currentTouch.x) + "," + String(currentTouch.y) + ") ";
        status += "P:" + String(currentTouch.pressure);
    } else {
        status += "Not pressed";
    }
    return status;
}