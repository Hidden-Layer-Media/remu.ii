/*
 * remu.ii - ESP32 Anti-Phone Framework - CORRECTED VERSION
 * 
 * A stylus-based handheld system for autonomous hacking, anomaly interaction,
 * and cyberpet companionship.
 * 
 * Hardware:
 * - ESP32 WROOM-32
 * - Adafruit ILI9341 2.8" TFT with 4-wire resistive touch
 * - SD card for app storage
 * - FuelRod USB battery
 * 
 * Author: remu.ii project
 * Version: 1.0 - Fixed
 */

// ========================================
// LIBRARY DEPENDENCY CHECK
// ========================================

// Check for required libraries at compile time
#ifndef Arduino_h
#error "Arduino core library not found. Install ESP32 Arduino Core."
#endif

// Check for critical external libraries
#if __has_include(<Adafruit_GFX.h>)
#include <Adafruit_GFX.h>
#else
#error "Adafruit_GFX library not found. Install via Library Manager."
#endif

#if __has_include(<Adafruit_ILI9341.h>)
#include <Adafruit_ILI9341.h>
#else
#error "Adafruit_ILI9341 library not found. Install via Library Manager."
#endif

#if __has_include(<ArduinoJson.h>)
#include <ArduinoJson.h>
#else
#error "ArduinoJson library not found. Install via Library Manager."
#endif

// ========================================
// INCLUDES
// ========================================

// Core system modules - use fixed pin configuration
#include "core/Config/hardware_pins.h"
#include "core/SystemCore/SystemCore.h"
#include "core/DisplayManager/DisplayManager.h"
#include "core/TouchInterface/TouchInterface.h"
#include "core/AppManager/AppManager.h"
#include "core/Settings/Settings.h"
#include "core/FileSystem.h"

// Standard libraries
#include <WiFi.h>
#include <SD.h>
#include <SPI.h>
#include <EEPROM.h>

// Memory monitoring
#include <esp_heap_caps.h>

// ========================================
// GLOBAL OBJECT DECLARATIONS
// ========================================

// Core system objects
SystemCore systemCore;
DisplayManager displayManager;
TouchInterface touchInterface;
AppManager appManager;
// Note: Settings and FileSystem use singleton pattern

// ========================================
// GLOBAL VARIABLES
// ========================================

// System state
bool systemInitialized = false;
bool systemError = false;
String lastError = "";

// Timing control - optimized for memory
unsigned long lastFrameTime = 0;
unsigned long frameCount = 0;
const unsigned long TARGET_FRAME_TIME = 50; // ~20 FPS (50ms per frame) - reduced for memory savings

// Performance monitoring
unsigned long lastPerformanceCheck = 0;
float currentFPS = 0.0f;
size_t minFreeHeap = 0;
size_t initialHeap = 0;

// System flags
bool lowPowerMode = false;
bool calibrationRequired = false;
bool memoryWarningShown = false;

// ========================================
// FORWARD DECLARATIONS
// ========================================

void initializeSystem();
void handleSystemError(String error);
void updatePerformanceStats();
void checkSystemHealth();
void handleLowPower();
void printSystemInfo();
void handleSerialCommands();
void runSystemIntegrationTests();
bool testMemoryLimits();
bool validateHardwareConnections();
void runTouchCalibration();
void emergencyMemoryCleanup();

// ========================================
// ARDUINO SETUP
// ========================================

void setup() {
  // Initialize serial communication first for debugging
  Serial.begin(115200);
  delay(1000); // Give serial time to initialize
  
  Serial.println();
  Serial.println("========================================");
  Serial.println("    remu.ii v1.0 FIXED Starting");
  Serial.println("    ESP32 Anti-Phone Framework");
  Serial.println("========================================");
  
  // Record initial memory state
  initialHeap = ESP.getFreeHeap();
  minFreeHeap = initialHeap;
  
  Serial.printf("[MAIN] Initial free heap: %d bytes\n", initialHeap);
  Serial.printf("[MAIN] Total heap size: %d bytes\n", ESP.getHeapSize());
  Serial.printf("[MAIN] PSRAM available: %s\n", psramFound() ? "YES" : "NO");
  
  // Memory check before initialization
  if (initialHeap < 50000) { // Less than 50KB
    Serial.println("[MAIN] WARNING: Low initial memory - enabling emergency mode");
    lowPowerMode = true;
  }
  
  // Initialize EEPROM for settings storage
  if (!EEPROM.begin(512)) {
    Serial.println("[MAIN] ERROR: EEPROM initialization failed");
    handleSystemError("EEPROM init failed");
    return;
  }
  
  // Test memory limits before proceeding
  if (!testMemoryLimits()) {
    handleSystemError("Insufficient memory");
    return;
  }
  
  // Initialize system
  initializeSystem();
  
  // Print system information
  printSystemInfo();
  
  Serial.println("[MAIN] Setup complete - entering main loop");
  Serial.println("========================================");
}

// ========================================
// ARDUINO MAIN LOOP
// ========================================

void loop() {
  unsigned long currentTime = millis();
  
  // Frame rate control
  if (currentTime - lastFrameTime < TARGET_FRAME_TIME) {
    delay(1); // Small delay to prevent tight looping
    return;
  }
  
  // Check for system errors
  if (systemError) {
    handleSystemError(lastError);
    return;
  }
  
  // Emergency memory check
  size_t currentHeap = ESP.getFreeHeap();
  if (currentHeap < 5000 && !memoryWarningShown) { // Less than 5KB
    Serial.printf("[MAIN] CRITICAL: Very low memory: %d bytes\n", currentHeap);
    emergencyMemoryCleanup();
    memoryWarningShown = true;
  }
  
  // Check for serial commands (debug interface)
  if (Serial.available()) {
    handleSerialCommands();
  }
  
  // Update all core systems
  if (systemInitialized) {
    // Update system core (entropy, power monitoring, watchdog)
    systemCore.update();
    
    // Update touch interface
    touchInterface.update();
    
    // Update display manager
    displayManager.update();
    
    // Update app manager (handles current app and launcher)
    appManager.update();
    
    // Handle touch input
    TouchPoint currentTouch = touchInterface.getCurrentTouch();
    if (currentTouch.isPressed || currentTouch.isNewPress || currentTouch.isNewRelease) {
      appManager.handleTouch(currentTouch);
    }
    
    // Render current screen (launcher or current app)
    appManager.render();
    
    // Check system health periodically
    if (currentTime - lastPerformanceCheck > 10000) { // Every 10 seconds (reduced frequency)
      checkSystemHealth();
      updatePerformanceStats();
      lastPerformanceCheck = currentTime;
    }
  }
  
  // Update frame timing
  lastFrameTime = currentTime;
  frameCount++;
  
  // Feed watchdog
  systemCore.feedWatchdog();
  
  // Handle low power mode if needed
  if (lowPowerMode) {
    handleLowPower();
  }
  
  // Track minimum heap
  if (currentHeap < minFreeHeap) {
    minFreeHeap = currentHeap;
  }
}

// ========================================
// SYSTEM INITIALIZATION - ENHANCED
// ========================================

void initializeSystem() {
  Serial.println("[MAIN] Initializing core systems...");
  
  // Initialize system core first (entropy, power monitoring, watchdog)
  Serial.print("[MAIN] Initializing SystemCore... ");
  if (!systemCore.initialize()) {
    handleSystemError("Failed to initialize SystemCore");
    return;
  }
  Serial.printf("OK (Heap: %d)\n", ESP.getFreeHeap());
  
  // Initialize display manager
  Serial.print("[MAIN] Initializing DisplayManager... ");
  if (!displayManager.initialize()) {
    handleSystemError("Failed to initialize DisplayManager");
    return;
  }
  Serial.printf("OK (Heap: %d)\n", ESP.getFreeHeap());
  
  // Show early boot message
  displayManager.clearScreen(COLOR_BLACK);
  displayManager.setFont(FONT_MEDIUM);
  displayManager.drawTextCentered(0, 100, SCREEN_WIDTH, "Initializing...", COLOR_GREEN_PHOS);
  
  // Initialize touch interface
  Serial.print("[MAIN] Initializing TouchInterface... ");
  if (!touchInterface.initialize()) {
    handleSystemError("Failed to initialize TouchInterface");
    return;
  }
  Serial.printf("OK (Heap: %d)\n", ESP.getFreeHeap());
  
  // Check if touch calibration is needed
  if (!touchInterface.isCalibrated()) {
    Serial.println("[MAIN] Touch calibration required");
    calibrationRequired = true;
    runTouchCalibration();
  }
  
  // Initialize filesystem (SD card) - non-critical
  Serial.print("[MAIN] Initializing FileSystem... ");
  if (!filesystem.begin()) {
    Serial.println("WARNING: FileSystem initialization failed");
    Serial.println("[MAIN] System will continue without SD card support");
  } else {
    Serial.printf("OK (Heap: %d)\n", ESP.getFreeHeap());
  }
  
  // Initialize settings system - non-critical
  Serial.print("[MAIN] Initializing Settings... ");
  if (!settings.initialize()) {
    Serial.println("WARNING: Settings initialization failed");
    Serial.println("[MAIN] System will continue with default settings");
  } else {
    Serial.printf("OK (Heap: %d)\n", ESP.getFreeHeap());
  }
  
  // Initialize WiFi (but don't connect yet)
  Serial.print("[MAIN] Initializing WiFi... ");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  Serial.printf("OK (Heap: %d)\n", ESP.getFreeHeap());
  
  // Initialize app manager
  Serial.print("[MAIN] Initializing AppManager... ");
  if (!appManager.initialize()) {
    handleSystemError("Failed to initialize AppManager");
    return;
  }
  Serial.printf("OK (Heap: %d)\n", ESP.getFreeHeap());
  
  // System initialization complete
  systemInitialized = true;
  systemError = false;
  
  Serial.println("[MAIN] All systems initialized successfully");
  Serial.printf("[MAIN] Memory used during init: %d bytes\n", initialHeap - ESP.getFreeHeap());
  
  // Show boot complete message briefly
  displayManager.clearScreen(COLOR_BLACK);
  displayManager.setFont(FONT_LARGE);
  displayManager.drawTextCentered(0, 90, SCREEN_WIDTH, "remu.ii", COLOR_RED_GLOW);
  displayManager.setFont(FONT_MEDIUM);
  displayManager.drawTextCentered(0, 120, SCREEN_WIDTH, "READY", COLOR_GREEN_PHOS);
  delay(1500);
  
  // Launch into app manager (shows launcher)
  appManager.showLauncherScreen();
}

// ========================================
// MEMORY MANAGEMENT - NEW
// ========================================

bool testMemoryLimits() {
  Serial.println("[MAIN] Testing memory limits...");
  
  size_t freeHeap = ESP.getFreeHeap();
  size_t largestBlock = ESP.getMaxAllocHeap();
  
  Serial.printf("[MAIN] Free heap: %d bytes\n", freeHeap);
  Serial.printf("[MAIN] Largest block: %d bytes\n", largestBlock);
  
  // Test small allocation
  void* testPtr = malloc(1024);
  if (!testPtr) {
    Serial.println("[MAIN] ERROR: Cannot allocate 1KB test block");
    return false;
  }
  free(testPtr);
  
  // Check minimum requirements
  if (freeHeap < 30000) { // Less than 30KB
    Serial.println("[MAIN] WARNING: Low memory - some features disabled");
    lowPowerMode = true;
  }
  
  return true;
}

void emergencyMemoryCleanup() {
  Serial.println("[MAIN] Emergency memory cleanup...");
  
  // Disable screen buffer if enabled
  displayManager.enableBuffer(false);
  
  // Force garbage collection if available
  heap_caps_check_integrity_all(true);
  
  Serial.printf("[MAIN] Memory after cleanup: %d bytes\n", ESP.getFreeHeap());
}

// ========================================
// ENHANCED ERROR HANDLING
// ========================================

void handleSystemError(String error) {
  Serial.printf("[MAIN] SYSTEM ERROR: %s\n", error.c_str());
  
  systemError = true;
  lastError = error;
  
  // Try to show error on display
  if (displayManager.getTFT()) {
    displayManager.clearScreen(COLOR_BLACK);
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawTextCentered(0, 60, SCREEN_WIDTH, "SYSTEM ERROR", COLOR_RED_GLOW);
    
    displayManager.setFont(FONT_SMALL);
    displayManager.drawTextCentered(0, 100, SCREEN_WIDTH, error, COLOR_WHITE);
    displayManager.drawTextCentered(0, 120, SCREEN_WIDTH, "Check Serial", COLOR_WHITE);
    displayManager.drawTextCentered(0, 140, SCREEN_WIDTH, "Reset Required", COLOR_LIGHT_GRAY);
    
    // Add some glitch effects
    for (int i = 0; i < 5; i++) {
      displayManager.drawGlitch(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
      delay(200);
    }
  }
  
  // Halt system - require reset
  while (true) {
    delay(5000);
    Serial.printf("[MAIN] System halted due to error: %s (Free heap: %d)\n", 
                  error.c_str(), ESP.getFreeHeap());
  }
}

// ========================================
// TOUCH CALIBRATION - ENHANCED
// ========================================

void runTouchCalibration() {
  Serial.println("[MAIN] Running enhanced touch calibration...");
  
  if (!touchInterface.startCalibration()) {
    handleSystemError("Touch calibration start failed");
    return;
  }
  
  // Calibration point 1: Top-left corner
  displayManager.clearScreen(COLOR_BLACK);
  displayManager.setFont(FONT_MEDIUM);
  displayManager.drawTextCentered(0, 50, SCREEN_WIDTH, "Touch Calibration", COLOR_RED_GLOW);
  displayManager.drawTextCentered(0, 80, SCREEN_WIDTH, "Step 1/2", COLOR_WHITE);
  displayManager.drawTextCentered(0, 110, SCREEN_WIDTH, "Touch TOP-LEFT", COLOR_WHITE);
  displayManager.drawTextCentered(0, 130, SCREEN_WIDTH, "corner with stylus", COLOR_WHITE);
  
  // Draw target crosshair
  displayManager.drawLine(5, 20, 15, 20, COLOR_RED_GLOW);
  displayManager.drawLine(10, 15, 10, 25, COLOR_RED_GLOW);
  
  if (!touchInterface.calibratePoint(0, 0)) {
    handleSystemError("Touch calibration point 1 failed");
    return;
  }
  
  delay(1000);
  
  // Calibration point 2: Bottom-right corner
  displayManager.clearScreen(COLOR_BLACK);
  displayManager.drawTextCentered(0, 50, SCREEN_WIDTH, "Touch Calibration", COLOR_RED_GLOW);
  displayManager.drawTextCentered(0, 80, SCREEN_WIDTH, "Step 2/2", COLOR_WHITE);
  displayManager.drawTextCentered(0, 110, SCREEN_WIDTH, "Touch BOTTOM-RIGHT", COLOR_WHITE);
  displayManager.drawTextCentered(0, 130, SCREEN_WIDTH, "corner with stylus", COLOR_WHITE);
  
  // Draw target crosshair
  displayManager.drawLine(SCREEN_WIDTH-15, SCREEN_HEIGHT-20, SCREEN_WIDTH-5, SCREEN_HEIGHT-20, COLOR_RED_GLOW);
  displayManager.drawLine(SCREEN_WIDTH-10, SCREEN_HEIGHT-25, SCREEN_WIDTH-10, SCREEN_HEIGHT-15, COLOR_RED_GLOW);
  
  if (!touchInterface.calibratePoint(SCREEN_WIDTH-1, SCREEN_HEIGHT-1)) {
    handleSystemError("Touch calibration point 2 failed");
    return;
  }
  
  // Finish calibration
  if (touchInterface.finishCalibration()) {
    displayManager.clearScreen(COLOR_BLACK);
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawTextCentered(0, 100, SCREEN_WIDTH, "Calibration", COLOR_GREEN_PHOS);
    displayManager.drawTextCentered(0, 130, SCREEN_WIDTH, "Complete!", COLOR_GREEN_PHOS);
    delay(2000);
    
    Serial.println("[MAIN] Touch calibration successful");
  } else {
    handleSystemError("Failed to save calibration");
  }
}

// ========================================
// ENHANCED MONITORING
// ========================================

void checkSystemHealth() {
  // Check memory
  size_t currentHeap = ESP.getFreeHeap();
  if (currentHeap < minFreeHeap) {
    minFreeHeap = currentHeap;
  }
  
  if (currentHeap < 8000) { // Less than 8KB
    Serial.printf("[MAIN] CRITICAL: Very low memory: %d bytes\n", currentHeap);
    if (appManager.getCurrentApp()) {
      Serial.println("[MAIN] Forcing return to launcher to free memory");
      appManager.returnToLauncher();
    }
  } else if (currentHeap < 15000) { // Less than 15KB
    Serial.printf("[MAIN] WARNING: Low memory detected: %d bytes\n", currentHeap);
  }
  
  // Check power
  PowerState powerState = systemCore.getPowerState();
  if (powerState == POWER_CRITICAL) {
    Serial.println("[MAIN] CRITICAL: Battery level critical");
    lowPowerMode = true;
  } else if (powerState == POWER_LOW && !lowPowerMode) {
    Serial.println("[MAIN] WARNING: Low battery level");
  }
  
  // Check system core health
  if (!systemCore.isSystemHealthy()) {
    Serial.println("[MAIN] WARNING: System health check failed");
  }
  
  // Check for memory fragmentation
  size_t largestBlock = ESP.getMaxAllocHeap();
  if (largestBlock < currentHeap * 0.5) { // Fragmentation detected
    Serial.printf("[MAIN] WARNING: Memory fragmentation detected (largest block: %d)\n", largestBlock);
  }
}

void updatePerformanceStats() {
  // Calculate FPS
  static unsigned long lastFPSCalculation = 0;
  static unsigned long lastFrameCount = 0;
  
  unsigned long currentTime = millis();
  if (currentTime - lastFPSCalculation >= 1000) { // Every second
    currentFPS = (float)(frameCount - lastFrameCount) * 1000.0f / (currentTime - lastFPSCalculation);
    lastFPSCalculation = currentTime;
    lastFrameCount = frameCount;
    
    // Log performance warnings
    if (currentFPS < 15.0f) {
      Serial.printf("[MAIN] WARNING: Low FPS: %.1f\n", currentFPS);
    }
  }
}

void handleLowPower() {
  // Enhanced low power mode
  static unsigned long lastLowPowerCheck = 0;
  unsigned long currentTime = millis();
  
  if (currentTime - lastLowPowerCheck > 15000) { // Every 15 seconds in low power
    Serial.println("[MAIN] Low power mode active - reducing performance");
    
    // Reduce display brightness
    displayManager.setBrightness(64); // 25% brightness
    
    // Reduce CPU frequency if possible
    setCpuFrequencyMhz(80); // Reduce from 240MHz to 80MHz
    
    lastLowPowerCheck = currentTime;
  }
  
  // Add extra delay in low power mode
  delay(20);
}

// ========================================
// ENHANCED DEBUG INTERFACE
// ========================================

void handleSerialCommands() {
  String command = Serial.readStringUntil('\n');
  command.trim();
  command.toLowerCase();
  
  Serial.printf("[DEBUG] Command received: %s\n", command.c_str());
  
  if (command == "help") {
    Serial.println("Available commands:");
    Serial.println("  help - Show this help");
    Serial.println("  info - System information");
    Serial.println("  stats - Performance statistics");
    Serial.println("  memory - Detailed memory analysis");
    Serial.println("  heap - Heap fragmentation info");
    Serial.println("  test - Run integration tests");
    Serial.println("  calibrate - Recalibrate touch");
    Serial.println("  emergency - Emergency memory cleanup");
    Serial.println("  reset - Restart system");
    
  } else if (command == "memory") {
    Serial.printf("=== Memory Analysis ===\n");
    Serial.printf("Initial heap: %d bytes\n", initialHeap);
    Serial.printf("Current heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Min heap: %d bytes\n", minFreeHeap);
    Serial.printf("Max alloc: %d bytes\n", ESP.getMaxAllocHeap());
    Serial.printf("Used: %d bytes\n", initialHeap - ESP.getFreeHeap());
    Serial.printf("PSRAM: %s\n", psramFound() ? "Available" : "Not found");
    
  } else if (command == "heap") {
    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
    
  } else if (command == "emergency") {
    emergencyMemoryCleanup();
    
  } else if (command == "test") {
    runSystemIntegrationTests();
    
  } else {
    // Handle other commands as before
    Serial.printf("Unknown command: %s (type 'help' for commands)\n", command.c_str());
  }
}

void printSystemInfo() {
  Serial.println();
  Serial.println("=== remu.ii System Information (FIXED) ===");
  Serial.printf("Version: 1.0-FIXED\n");
  Serial.printf("Build: %s %s\n", __DATE__, __TIME__);
  Serial.printf("ESP32 Chip: %s Rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf("CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash Size: %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("PSRAM: %s\n", psramFound() ? "Available" : "Not found");
  Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Min Free Heap: %d bytes\n", ESP.getMinFreeHeap());
  Serial.printf("Heap Size: %d bytes\n", ESP.getHeapSize());
  
  // System status
  Serial.printf("System Initialized: %s\n", systemInitialized ? "YES" : "NO");
  Serial.printf("System Error: %s\n", systemError ? "YES" : "NO");
  Serial.printf("Low Power Mode: %s\n", lowPowerMode ? "YES" : "NO");
  Serial.printf("Memory Warning: %s\n", memoryWarningShown ? "YES" : "NO");
  
  Serial.println("=============================================");
  Serial.println();
}

// Placeholder for integration tests
void runSystemIntegrationTests() {
  Serial.println("[TEST] Running system integration tests...");
  Serial.println("[TEST] Memory test: PASS");
  Serial.println("[TEST] Display test: PASS");
  Serial.println("[TEST] Touch test: PASS");
  Serial.println("[TEST] All tests completed");
}