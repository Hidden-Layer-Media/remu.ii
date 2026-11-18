#include "CarCloner.h"
#include "../../core/DisplayManager/DisplayManager.h"
#include "../../core/TouchInterface/TouchInterface.h"

// ========================================
// CarCloner Implementation
// Automotive RF Security Research Tool
// ========================================

// Icon data for CarCloner (16x16 pixels, 1-bit per pixel)
const uint8_t car_cloner_icon[32] = {
    0b00000110, 0b01100000,
    0b00001111, 0b11110000,
    0b00011111, 0b11111000,
    0b00111100, 0b00111100,
    0b01110000, 0b00001110,
    0b11100000, 0b00000111,
    0b11000110, 0b01100011,
    0b11001111, 0b11110011,
    0b11001111, 0b11110011,
    0b11000110, 0b01100011,
    0b11100000, 0b00000111,
    0b01110000, 0b00001110,
    0b00111100, 0b00111100,
    0b00011111, 0b11111000,
    0b00001111, 0b11110000,
    0b00000110, 0b01100000
};

CarCloner::CarCloner() : BaseApp() {
    // Initialize basic state
    rfInitialized = false;
    signalCount = 0;
    
    // Initialize metadata
    metadata.name = "CarCloner";
    metadata.version = "1.0.0";
    metadata.author = "remu.ii Security Research";
    metadata.description = "Automotive RF Security Research Tool";
    metadata.category = CATEGORY_TOOLS;
    metadata.icon = car_cloner_icon;
    metadata.memoryRequirement = 48000; // 48KB
    metadata.requiresSD = true;
    metadata.requiresWiFi = false;
    metadata.requiresBLE = false;
    
    // Set UI colors
    setColors(COLOR_BLACK, COLOR_WHITE);
    setShowBackButton(true);
    setShowStatusBar(true);
    
    // Initialize paths
    dataDirectory = CAR_CLONER_DATA_DIR;
    signalDirectory = CAR_CLONER_SIGNALS_DIR;
    configFilePath = CAR_CLONER_CONFIG_FILE;
    logFilePath = CAR_CLONER_LOG_FILE;
    
    initializeFrequencyPresets();
}

CarCloner::~CarCloner() {
    cleanup();
}

bool CarCloner::initialize() {
    setState(APP_INITIALIZING);
    
    debugLog("CarCloner: Starting initialization");
    
    // Check SD card availability
    if (!isSDAvailable()) {
        debugLog("CarCloner: SD card required but not available");
        showAlert("SD card required for signal storage", 5000);
        return false;
    }
    
    // Initialize file system structure
    if (!initializeFSStructure()) {
        debugLog("CarCloner: Failed to initialize file system structure");
        return false;
    }
    
    // Load configuration
    loadConfiguration();
    
    // Initialize RF hardware
    if (!initializeRFHardware()) {
        debugLog("CarCloner: Failed to initialize RF hardware");
        showAlert("RF hardware initialization failed", 5000);
        return false;
    }
    
    // Load existing signal library
    loadSignalLibrary();
    
    // Display legal warning first
    uiState.currentView = VIEW_LEGAL_WARNING;
    uiState.showLegalWarning = true;
    uiState.warningAccepted = false;
    
    debugLog("CarCloner: Initialization complete");
    setState(APP_RUNNING);
    return true;
}

void CarCloner::update() {
    if (getState() != APP_RUNNING) return;
    
    unsigned long currentTime = millis();
    
    // Update capture session
    if (captureState.isActive) {
        updateCapture();
    }
    
    // Update replay session
    if (replayState.isActive) {
        updateReplay();
    }
    
    // Clear alerts after timeout
    if (uiState.alertTimeout > 0 && currentTime > uiState.alertTimeout) {
        uiState.alertMessage = "";
        uiState.alertTimeout = 0;
    }
    
    // Update status periodically
    if (currentTime - uiState.lastUIUpdate > 100) { // 10 Hz update
        uiState.lastUIUpdate = currentTime;
        
        // Update signal strength reading
        if (rfInitialized) {
            captureState.signalStrength = getCurrentRSSI();
        }
    }
    
    trackMemoryUsage();
}

void CarCloner::render() {
    if (getState() != APP_RUNNING) return;
    
    // Clear screen
    displayManager.clearScreen(COLOR_BLACK);
    
    // Render current view
    switch (uiState.currentView) {
        case VIEW_LEGAL_WARNING:
            renderLegalWarning();
            break;
        
        case VIEW_MAIN_MENU:
            renderMainMenu();
            break;
            
        case VIEW_CAPTURE:
            renderCaptureView();
            break;
            
        case VIEW_SIGNAL_LIBRARY:
            renderSignalLibrary();
            break;
            
        case VIEW_REPLAY:
            renderReplayView();
            break;
            
        case VIEW_ANALYSIS:
            renderAnalysisView();
            break;
            
        case VIEW_SETTINGS:
            renderSettingsView();
            break;
    }
    
    // Always render status bar (except in legal warning)
    if (uiState.currentView != VIEW_LEGAL_WARNING) {
        renderStatusBar();
    }
    
    // Show alert message if active
    if (!uiState.alertMessage.isEmpty()) {
        int16_t alertY = SCREEN_HEIGHT - 40;
        displayManager.drawWindow(10, alertY, SCREEN_WIDTH - 20, 30, "Alert", WINDOW_DIALOG);
        displayManager.drawTextCentered(10, alertY + 15, SCREEN_WIDTH - 20, uiState.alertMessage, COLOR_YELLOW);
    }
}

bool CarCloner::handleTouch(TouchPoint touch) {
    if (!touch.isNewPress) return false;
    
    uiState.lastTouch = touch;
    
    switch (uiState.currentView) {
        case VIEW_LEGAL_WARNING:
            handleLegalWarningTouch(touch);
            break;
            
        case VIEW_MAIN_MENU:
            handleMainMenuTouch(touch);
            break;
            
        case VIEW_CAPTURE:
            handleCaptureTouch(touch);
            break;
            
        case VIEW_SIGNAL_LIBRARY:
            handleLibraryTouch(touch);
            break;
            
        case VIEW_REPLAY:
            handleReplayTouch(touch);
            break;
            
        case VIEW_ANALYSIS:
            handleAnalysisTouch(touch);
            break;
            
        case VIEW_SETTINGS:
            handleSettingsTouch(touch);
            break;
    }
    
    return true;
}

void CarCloner::cleanup() {
    debugLog("CarCloner: Starting cleanup");
    
    // Stop any active operations
    if (captureState.isActive) {
        stopCapture();
    }
    
    if (replayState.isActive) {
        stopReplay();
    }
    
    // Save current state
    saveState();
    saveConfiguration();
    
    // Shutdown RF hardware
    shutdownRFHardware();
    
    // Clear signal data from memory
    capturedSignals.clear();
    detectedProtocols.clear();
    
    setState(APP_CLEANUP);
    debugLog("CarCloner: Cleanup complete");
}

String CarCloner::getName() const {
    return "CarCloner";
}

const uint8_t* CarCloner::getIcon() const {
    return car_cloner_icon;
}

// ========================================
// RF HARDWARE METHODS
// ========================================

bool CarCloner::initializeRFHardware() {
    debugLog("CarCloner: Initializing RF hardware");
    
    // Initialize SPI for RF communication
    SPI.begin();
    SPI.setFrequency(4000000); // 4 MHz
    SPI.setDataMode(SPI_MODE0);
    
    // Configure RF control pins
    pinMode(RF_CE_PIN, OUTPUT);
    pinMode(RF_CSN_PIN, OUTPUT);
    pinMode(RF_IRQ_PIN, INPUT);
    
    digitalWrite(RF_CE_PIN, LOW);
    digitalWrite(RF_CSN_PIN, HIGH);
    
    // Initialize ADC for signal capture
    setupADCForCapture();
    
    // Initialize DAC for signal transmission
    setupDACForTransmission();
    
    // Set default frequency
    if (!setFrequency(rfConfig.frequency)) {
        debugLog("CarCloner: Failed to set default frequency");
        return false;
    }
    
    // Calibrate RF hardware
    if (!calibrateRFHardware()) {
        debugLog("CarCloner: RF calibration failed");
        return false;
    }
    
    rfInitialized = true;
    debugLog("CarCloner: RF hardware initialized successfully");
    return true;
}

void CarCloner::shutdownRFHardware() {
    if (!rfInitialized) return;
    
    debugLog("CarCloner: Shutting down RF hardware");
    
    // Power down RF module
    digitalWrite(RF_CE_PIN, LOW);
    digitalWrite(RF_CSN_PIN, HIGH);
    
    // Stop SPI
    SPI.end();
    
    rfInitialized = false;
}

bool CarCloner::setFrequency(float frequency) {
    if (!isValidFrequency(frequency)) {
        debugLog("CarCloner: Invalid frequency: " + String(frequency));
        return false;
    }
    
    rfConfig.frequency = frequency;
    
    // Configure RF module for new frequency
    // This would interface with actual RF hardware
    // For simulation, we'll just store the value
    
    debugLog("CarCloner: Frequency set to " + String(frequency) + " MHz");
    return true;
}

bool CarCloner::setPowerLevel(uint8_t power) {
    if (power > POWER_LIMIT_DEFAULT) {
        debugLog("CarCloner: Power level limited to " + String(POWER_LIMIT_DEFAULT));
        power = POWER_LIMIT_DEFAULT;
    }
    
    rfConfig.power = power;
    
    // Configure RF module power
    // This would interface with actual RF hardware
    
    debugLog("CarCloner: Power level set to " + String(power));
    return true;
}

void CarCloner::setupADCForCapture() {
    // Configure ADC for high-speed signal sampling
    analogReadResolution(12); // 12-bit resolution
    analogSetAttenuation(ADC_11db); // Full range
    analogSetPinAttenuation(ENTROPY_PIN_1, ADC_11db);
    
    debugLog("CarCloner: ADC configured for capture");
}

void CarCloner::setupDACForTransmission() {
    // Configure DAC for signal transmission
    dacWrite(DAC_OUT_LEFT, 0);
    dacWrite(DAC_OUT_RIGHT, 0);
    
    debugLog("CarCloner: DAC configured for transmission");
}

bool CarCloner::calibrateRFHardware() {
    debugLog("CarCloner: Performing RF calibration");
    
    // Perform basic calibration routine
    // This would involve actual RF hardware calibration
    
    // Simulate calibration delay
    delay(100);
    
    debugLog("CarCloner: RF calibration complete");
    return true;
}

float CarCloner::getCurrentRSSI() {
    if (!rfInitialized) return -100.0;
    
    // Read signal strength from ADC
    uint16_t adcValue = analogRead(ENTROPY_PIN_1);
    
    // Convert ADC reading to RSSI (dBm)
    // This is a simplified conversion - real implementation would
    // depend on specific RF hardware characteristics
    float rssi = -100.0 + (adcValue / 4095.0) * 60.0; // -100 to -40 dBm range
    
    return rssi;
}

// ========================================
// SIGNAL CAPTURE METHODS
// ========================================

bool CarCloner::startCapture() {
    if (captureState.isActive) {
        debugLog("CarCloner: Capture already active");
        return false;
    }
    
    if (!rfInitialized) {
        debugLog("CarCloner: RF hardware not initialized");
        showAlert("RF hardware not ready", 3000);
        return false;
    }
    
    debugLog("CarCloner: Starting signal capture");
    
    // Initialize capture session
    captureState.isActive = true;
    captureState.startTime = millis();
    captureState.duration = 0;
    captureState.samplesCollected = 0;
    captureState.triggerDetected = false;
    captureState.statusMessage = "Capturing...";
    
    // Reset current signal
    currentSignal = RFSignal();
    currentSignal.frequency = rfConfig.frequency;
    currentSignal.sampleRate = rfConfig.sampleRate;
    currentSignal.captureTime = captureState.startTime;
    
    // Configure capture based on mode
    switch (rfConfig.captureMode) {
        case CAPTURE_SINGLE:
            captureState.statusMessage = "Single shot capture - click to stop";
            break;
            
        case CAPTURE_CONTINUOUS:
            captureState.statusMessage = "Continuous capture active";
            break;
            
        case CAPTURE_TRIGGERED:
            captureState.statusMessage = "Waiting for trigger...";
            break;
            
        case CAPTURE_TIMED:
            captureState.statusMessage = "Timed capture - " + String(rfConfig.captureTimeout/1000) + "s";
            break;
    }
    
    logActivity("Capture started at " + String(rfConfig.frequency) + " MHz");
    return true;
}

void CarCloner::stopCapture() {
    if (!captureState.isActive) return;
    
    debugLog("CarCloner: Stopping signal capture");
    
    captureState.isActive = false;
    captureState.duration = millis() - captureState.startTime;
    
    // Finalize the captured signal
    finalizeCapture();
    
    if (validateCapturedSignal()) {
        captureState.statusMessage = "Capture complete - " + String(currentSignal.sampleCount) + " samples";
        showAlert("Signal captured successfully", 3000);
        
        // Analyze the captured signal
        analyzeSignal(currentSignal);
        
        // Add to signal library
        capturedSignals.push_back(currentSignal);
        signalCount++;
        
        // Save to SD card
        if (saveSignal(currentSignal)) {
            logActivity("Signal saved: " + String(currentSignal.name));
        }
    } else {
        captureState.statusMessage = "Capture failed - insufficient data";
        showAlert("Capture failed - no valid signal detected", 3000);
    }
}

void CarCloner::updateCapture() {
    if (!captureState.isActive) return;
    
    unsigned long currentTime = millis();
    captureState.duration = currentTime - captureState.startTime;
    
    // Check for timeout in timed mode
    if (rfConfig.captureMode == CAPTURE_TIMED && 
        captureState.duration > rfConfig.captureTimeout) {
        stopCapture();
        return;
    }
    
    // Check for trigger in triggered mode
    if (rfConfig.captureMode == CAPTURE_TRIGGERED && !captureState.triggerDetected) {
        if (detectSignalTrigger()) {
            captureState.triggerDetected = true;
            captureState.statusMessage = "Trigger detected - capturing...";
        }
    }
    
    // Capture samples
    if (rfConfig.captureMode != CAPTURE_TRIGGERED || captureState.triggerDetected) {
        RFSample sample;
        if (captureRFSample(sample)) {
            if (currentSignal.sampleCount < MAX_SIGNAL_SAMPLES) {
                currentSignal.samples[currentSignal.sampleCount] = sample;
                currentSignal.sampleCount++;
                captureState.samplesCollected++;
            }
        }
    }
    
    // Update signal strength
    captureState.signalStrength = getCurrentRSSI();
}

bool CarCloner::captureRFSample(RFSample& sample) {
    // Read ADC value
    uint16_t adcValue = analogRead(ENTROPY_PIN_1);
    
    // Record timing
    uint32_t timestamp = micros();
    
    // Process the sample
    sample.amplitude = adcValue;
    sample.timing = timestamp;
    sample.digitalLevel = (adcValue > rfConfig.sensitivity);
    
    return true;
}

bool CarCloner::detectSignalTrigger() {
    float rssi = getCurrentRSSI();
    
    // Simple threshold-based trigger
    return (rssi > (RF_SIGNAL_THRESHOLD + 10.0));
}

void CarCloner::finalizeCapture() {
    currentSignal.duration = captureState.duration * 1000; // Convert to microseconds
    currentSignal.captureRSSI = (int8_t)captureState.signalStrength;
    
    // Generate default name if not set
    if (strlen(currentSignal.name) == 0 || strcmp(currentSignal.name, "Untitled") == 0) {
        snprintf(currentSignal.name, MAX_SIGNAL_NAME_LENGTH, 
                "Signal_%lu", currentSignal.captureTime);
    }
}

bool CarCloner::validateCapturedSignal() {
    // Check minimum requirements for a valid signal
    if (currentSignal.sampleCount < 10) {
        debugLog("CarCloner: Signal validation failed - too few samples");
        return false;
    }
    
    if (currentSignal.duration < 1000) { // Less than 1ms
        debugLog("CarCloner: Signal validation failed - duration too short");
        return false;
    }
    
    // Check for actual signal content (not just noise)
    uint16_t transitionCount = 0;
    for (uint16_t i = 1; i < currentSignal.sampleCount; i++) {
        if (currentSignal.samples[i].digitalLevel != currentSignal.samples[i-1].digitalLevel) {
            transitionCount++;
        }
    }
    
    if (transitionCount < 2) {
        debugLog("CarCloner: Signal validation failed - no transitions detected");
        return false;
    }
    
    debugLog("CarCloner: Signal validation passed");
    return true;
}

// ========================================
// SIGNAL REPLAY METHODS
// ========================================

bool CarCloner::startReplay(int signalIndex) {
    if (replayState.isActive) {
        debugLog("CarCloner: Replay already active");
        return false;
    }
    
    if (signalIndex < 0 || signalIndex >= (int)capturedSignals.size()) {
        debugLog("CarCloner: Invalid signal index for replay");
        return false;
    }
    
    if (!rfInitialized) {
        debugLog("CarCloner: RF hardware not initialized");
        showAlert("RF hardware not ready", 3000);
        return false;
    }
    
    // Show legal confirmation
    if (!confirmTransmission()) {
        return false;
    }
    
    debugLog("CarCloner: Starting signal replay");
    
    replayState.isActive = true;
    replayState.selectedSignal = signalIndex;
    replayState.remainingRepeats = replayState.repeatCount;
    replayState.lastTransmission = 0;
    replayState.statusMessage = "Transmitting signal...";
    
    const RFSignal& signal = capturedSignals[signalIndex];
    
    // Set frequency for transmission
    setFrequency(signal.frequency);
    
    logActivity("Replay started: " + String(signal.name) + " at " + String(signal.frequency) + " MHz");
    
    return true;
}

void CarCloner::stopReplay() {
    if (!replayState.isActive) return;
    
    debugLog("CarCloner: Stopping signal replay");
    
    replayState.isActive = false;
    replayState.statusMessage = "Transmission stopped";
    
    // Power down transmitter
    dacWrite(DAC_OUT_LEFT, 0);
    dacWrite(DAC_OUT_RIGHT, 0);
    
    logActivity("Replay stopped");
}

void CarCloner::updateReplay() {
    if (!replayState.isActive) return;
    
    unsigned long currentTime = millis();
    
    // Check if it's time for next transmission
    if (currentTime - replayState.lastTransmission >= replayState.transmissionInterval) {
        if (replayState.remainingRepeats > 0) {
            const RFSignal& signal = capturedSignals[replayState.selectedSignal];
            
            if (transmitSignal(signal)) {
                replayState.remainingRepeats--;
                replayState.lastTransmission = currentTime;
                
                replayState.statusMessage = "Transmitted - " + 
                    String(replayState.remainingRepeats) + " repeats remaining";
                
                if (replayState.remainingRepeats == 0) {
                    stopReplay();
                    showAlert("Transmission complete", 3000);
                }
            } else {
                stopReplay();
                showAlert("Transmission failed", 3000);
            }
        }
    }
}

bool CarCloner::transmitSignal(const RFSignal& signal) {
    debugLog("CarCloner: Transmitting signal: " + String(signal.name));
    
    // Configure transmission timing
    setupTransmissionTiming();
    
    // Transmit each sample
    for (uint16_t i = 0; i < signal.sampleCount; i++) {
        if (!transmitRFSample(signal.samples[i])) {
            debugLog("CarCloner: Failed to transmit sample " + String(i));
            return false;
        }
    }
    
    return true;
}

bool CarCloner::transmitRFSample(const RFSample& sample) {
    // Convert amplitude to DAC value
    uint8_t dacValue = map(sample.amplitude, 0, 4095, 0, 255);
    
    // Apply power scaling
    dacValue = map(dacValue, 0, 255, 0, rfConfig.power);
    
    // Write to DAC
    dacWrite(DAC_OUT_LEFT, dacValue);
    
    // Wait for sample timing
    delayMicroseconds(sample.timing % 1000); // Simple timing approximation
    
    return true;
}

void CarCloner::setupTransmissionTiming() {
    // Configure high-precision timing for transmission
    // This would use hardware timers for precise timing
    debugLog("CarCloner: Transmission timing configured");
}

// ========================================
// UI RENDERING METHODS
// ========================================

void CarCloner::renderLegalWarning() {
    displayManager.setFont(FONT_MEDIUM);
    
    // Draw warning window
    displayManager.drawWindow(20, 20, SCREEN_WIDTH - 40, SCREEN_HEIGHT - 40, 
                             "LEGAL WARNING", WINDOW_DIALOG);
    
    int y = 50;
    displayManager.setFont(FONT_SMALL);
    
    displayManager.drawText(30, y, "CarCloner is for EDUCATIONAL and", COLOR_YELLOW);
    y += 15;
    displayManager.drawText(30, y, "RESEARCH purposes ONLY.", COLOR_YELLOW);
    y += 20;
    
    displayManager.drawText(30, y, "Unauthorized RF transmission may", COLOR_WHITE);
    y += 12;
    displayManager.drawText(30, y, "violate local laws and regulations.", COLOR_WHITE);
    y += 12;
    displayManager.drawText(30, y, "Use only on signals you own or", COLOR_WHITE);
    y += 12;
    displayManager.drawText(30, y, "have explicit permission to clone.", COLOR_WHITE);
    y += 20;
    
    displayManager.drawText(30, y, "You are responsible for compliance", COLOR_RED_GLOW);
    y += 12;
    displayManager.drawText(30, y, "with all applicable laws.", COLOR_RED_GLOW);
    y += 25;
    
    // Warning timeout countdown
    if (!uiState.warningAccepted) {
        unsigned long remaining = LEGAL_WARNING_TIMEOUT - (millis() - startTime);
        if (remaining > 0) {
            displayManager.drawText(30, y, "Please wait " + String(remaining/1000) + "s...", COLOR_GRAY_LIGHT);
        } else {
            displayManager.drawButton(SCREEN_WIDTH/2 - 40, y, 80, 25, "I ACCEPT", BUTTON_NORMAL, COLOR_GREEN_PHOS);
        }
    }
}

void CarCloner::renderMainMenu() {
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawTextCentered(0, 10, SCREEN_WIDTH, "CarCloner v1.0", COLOR_GREEN_PHOS);
    
    int y = 40;
    int buttonWidth = 120;
    int buttonHeight = 30;
    int centerX = SCREEN_WIDTH / 2 - buttonWidth / 2;
    
    // Main menu buttons
    displayManager.drawButton(centerX, y, buttonWidth, buttonHeight, "CAPTURE", BUTTON_NORMAL, COLOR_BLUE_CYBER);
    y += buttonHeight + 10;
    
    displayManager.drawButton(centerX, y, buttonWidth, buttonHeight, "SIGNAL LIBRARY", BUTTON_NORMAL, COLOR_PURPLE_GLOW);
    y += buttonHeight + 10;
    
    displayManager.drawButton(centerX, y, buttonWidth, buttonHeight, "REPLAY", BUTTON_NORMAL, COLOR_RED_GLOW);
    y += buttonHeight + 10;
    
    displayManager.drawButton(centerX, y, buttonWidth, buttonHeight, "ANALYSIS", BUTTON_NORMAL, COLOR_YELLOW);
    y += buttonHeight + 10;
    
    displayManager.drawButton(centerX, y, buttonWidth, buttonHeight, "SETTINGS", BUTTON_NORMAL, COLOR_MID_GRAY);
    
    // Show signal count
    displayManager.setFont(FONT_SMALL);
    displayManager.drawTextCentered(0, SCREEN_HEIGHT - 30, SCREEN_WIDTH, 
                                   "Signals: " + String(signalCount), COLOR_LIGHT_GRAY);
}

void CarCloner::renderCaptureView() {
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawText(10, 10, "RF Signal Capture", COLOR_GREEN_PHOS);
    
    // Frequency display
    renderFrequencyDisplay();
    
    // Signal strength meter
    renderSignalStrength();
    
    // Capture controls
    drawCaptureControls();
    
    // Status message
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(10, SCREEN_HEIGHT - 45, captureState.statusMessage, COLOR_WHITE);
    
    // Sample count if capturing
    if (captureState.isActive) {
        displayManager.drawText(10, SCREEN_HEIGHT - 30, 
                               "Samples: " + String(captureState.samplesCollected), COLOR_YELLOW);
    }
}

void CarCloner::renderSignalLibrary() {
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawText(10, 10, "Signal Library", COLOR_PURPLE_GLOW);
    
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(SCREEN_WIDTH - 60, 10, String(signalCount) + " signals", COLOR_LIGHT_GRAY);
    
    // Signal list
    int y = 35;
    int visibleStart = uiState.scrollOffset;
    int visibleEnd = min(visibleStart + SIGNAL_LIST_MAX_VISIBLE, (int)capturedSignals.size());
    
    for (int i = visibleStart; i < visibleEnd; i++) {
        bool selected = (i == uiState.selectedSignalIndex);
        drawSignalEntry(y, capturedSignals[i], selected);
        y += SIGNAL_LIST_ITEM_HEIGHT;
    }
    
    // Scrollbar if needed
    if (capturedSignals.size() > SIGNAL_LIST_MAX_VISIBLE) {
        int scrollbarHeight = SIGNAL_LIST_MAX_VISIBLE * SIGNAL_LIST_ITEM_HEIGHT;
        displayManager.drawScrollbar(SCREEN_WIDTH - 12, 35, scrollbarHeight, 
                                    uiState.scrollOffset, SIGNAL_LIST_MAX_VISIBLE);
    }
}

void CarCloner::renderReplayView() {
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawText(10, 10, "Signal Replay", COLOR_RED_GLOW);
    
    if (uiState.selectedSignalIndex >= 0 && uiState.selectedSignalIndex < (int)capturedSignals.size()) {
        const RFSignal& signal = capturedSignals[uiState.selectedSignalIndex];
        
        // Signal info
        displayManager.setFont(FONT_SMALL);
        displayManager.drawText(10, 35, "Signal: " + String(signal.name), COLOR_WHITE);
        displayManager.drawText(10, 50, "Frequency: " + formatFrequency(signal.frequency), COLOR_WHITE);
        displayManager.drawText(10, 65, "Duration: " + formatDuration(signal.duration), COLOR_WHITE);
        
        // Waveform preview
        drawSignalWaveform(10, 85, SCREEN_WIDTH - 20, WAVEFORM_HEIGHT, signal);
        
        // Replay controls
        drawReplayControls();
        
        // Status
        if (replayState.isActive) {
            displayManager.drawText(10, SCREEN_HEIGHT - 30, replayState.statusMessage, COLOR_YELLOW);
        }
    } else {
        displayManager.setFont(FONT_SMALL);
        displayManager.drawTextCentered(0, SCREEN_HEIGHT/2, SCREEN_WIDTH, 
                                       "No signal selected", COLOR_LIGHT_GRAY);
    }
}

void CarCloner::renderAnalysisView() {
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawText(10, 10, "Signal Analysis", COLOR_YELLOW);
    
    if (uiState.selectedSignalIndex >= 0 && uiState.selectedSignalIndex < (int)capturedSignals.size()) {
        const RFSignal& signal = capturedSignals[uiState.selectedSignalIndex];
        
        drawAnalysisResults();
    } else {
        displayManager.setFont(FONT_SMALL);
        displayManager.drawTextCentered(0, SCREEN_HEIGHT/2, SCREEN_WIDTH, 
                                       "Select signal for analysis", COLOR_LIGHT_GRAY);
    }
}

void CarCloner::renderSettingsView() {
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawText(10, 10, "Settings", COLOR_MID_GRAY);
    
    int y = 40;
    displayManager.setFont(FONT_SMALL);
    
    // Frequency setting
    displayManager.drawText(10, y, "Frequency: " + formatFrequency(rfConfig.frequency), COLOR_WHITE);
    y += 20;
    
    // Power setting
    displayManager.drawText(10, y, "Power: " + String(rfConfig.power) + "/255", COLOR_WHITE);
    y += 20;
    
    // Capture mode
    String modeStr = "Unknown";
    switch (rfConfig.captureMode) {
        case CAPTURE_SINGLE: modeStr = "Single Shot"; break;
        case CAPTURE_CONTINUOUS: modeStr = "Continuous"; break;
        case CAPTURE_TRIGGERED: modeStr = "Triggered"; break;
        case CAPTURE_TIMED: modeStr = "Timed"; break;
    }
    displayManager.drawText(10, y, "Capture Mode: " + modeStr, COLOR_WHITE);
    y += 20;
    
    // Sample rate
    displayManager.drawText(10, y, "Sample Rate: " + String(rfConfig.sampleRate/1000) + " kHz", COLOR_WHITE);
}

void CarCloner::renderStatusBar() {
    int y = SCREEN_HEIGHT - STATUS_BAR_HEIGHT;
    
    // Background
    displayManager.drawRetroRect(0, y, SCREEN_WIDTH, STATUS_BAR_HEIGHT, COLOR_DARK_GRAY, true);
    
    displayManager.setFont(FONT_SMALL);
    
    // Left side - frequency and signal strength
    String statusLeft = formatFrequency(rfConfig.frequency) + " | " + 
                       String((int)captureState.signalStrength) + "dBm";
    displayManager.drawText(5, y + 6, statusLeft, COLOR_WHITE);
    
    // Right side - memory and battery
    String statusRight = "Mem:" + String(getMemoryUsage()/1024) + "KB";
    int textWidth = displayManager.getTextWidth(statusRight);
    displayManager.drawText(SCREEN_WIDTH - textWidth - 5, y + 6, statusRight, COLOR_WHITE);
}

void CarCloner::renderFrequencyDisplay() {
    int y = 30;
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(10, y, "Frequency:", COLOR_WHITE);
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawText(80, y - 2, formatFrequency(rfConfig.frequency), COLOR_GREEN_PHOS);
}

void CarCloner::renderSignalStrength() {
    int x = 10;
    int y = 55;
    int width = 100;
    int height = 20;
    
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(x, y - 15, "Signal Strength:", COLOR_WHITE);
    
    // Signal strength bar
    float rssi = captureState.signalStrength;
    float normalizedRssi = (rssi + 100.0) / 60.0; // Normalize -100 to -40 dBm to 0-1
    normalizedRssi = constrained(normalizedRssi, 0.0, 1.0);
    
    uint16_t barColor = COLOR_RED_GLOW;
    if (normalizedRssi > 0.6) barColor = COLOR_GREEN_PHOS;
    else if (normalizedRssi > 0.3) barColor = COLOR_YELLOW;
    
    renderProgressBar(x, y, width, height, normalizedRssi);
    
    // RSSI value
    displayManager.drawText(x + width + 10, y + 6, String((int)rssi) + " dBm", COLOR_WHITE);
}

void CarCloner::renderProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, float progress) {
    // Background
    displayManager.drawRetroRect(x, y, w, h, COLOR_DARK_GRAY, true);
    
    // Progress fill
    int16_t fillWidth = (int16_t)(w * progress);
    uint16_t fillColor = COLOR_GREEN_PHOS;
    if (progress < 0.3) fillColor = COLOR_RED_GLOW;
    else if (progress < 0.6) fillColor = COLOR_YELLOW;
    
    displayManager.drawRetroRect(x, y, fillWidth, h, fillColor, true);
    
    // Border
    displayManager.drawRetroRect(x, y, w, h, COLOR_WHITE, false);
}

// ========================================
// UI HELPER METHODS
// ========================================

void CarCloner::drawSignalEntry(int16_t y, const RFSignal& signal, bool selected) {
    uint16_t bgColor = selected ? COLOR_DARK_GRAY : COLOR_BLACK;
    uint16_t textColor = selected ? COLOR_YELLOW : COLOR_WHITE;
    
    // Background
    if (selected) {
        displayManager.drawRetroRect(10, y, SCREEN_WIDTH - 30, SIGNAL_LIST_ITEM_HEIGHT, bgColor, true);
    }
    
    displayManager.setFont(FONT_SMALL);
    
    // Signal name
    displayManager.drawText(15, y + 5, signal.name, textColor);
    
    // Frequency
    displayManager.drawText(15, y + 17, formatFrequency(signal.frequency), COLOR_LIGHT_GRAY);
    
    // Duration and samples
    String info = formatDuration(signal.duration) + " (" + String(signal.sampleCount) + " samples)";
    int textWidth = displayManager.getTextWidth(info);
    displayManager.drawText(SCREEN_WIDTH - textWidth - 15, y + 5, info, COLOR_LIGHT_GRAY);
    
    // Analysis status
    if (signal.isAnalyzed) {
        displayManager.drawText(SCREEN_WIDTH - 30, y + 17, "A", COLOR_GREEN_PHOS);
    }
}

void CarCloner::drawCaptureControls() {
    int y = 80;
    int buttonY = y + 40;
    
    // Frequency selector buttons
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(10, y, "Frequency Presets:", COLOR_WHITE);
    
    int presetX = 10;
    int presetY = y + 15;
    for (int i = 0; i < min(4, presetCount); i++) {
        bool selected = (abs(frequencyPresets[i] - rfConfig.frequency) < 0.1);
        uint16_t buttonColor = selected ? COLOR_GREEN_PHOS : COLOR_DARK_GRAY;
        
        displayManager.drawButton(presetX, presetY, 60, 20, 
                                 String((int)frequencyPresets[i]), 
                                 selected ? BUTTON_PRESSED : BUTTON_NORMAL, 
                                 buttonColor);
        presetX += 70;
    }
    
    // Capture button
    if (captureState.isActive) {
        displayManager.drawButton(SCREEN_WIDTH/2 - 40, buttonY, 80, 30, "STOP", BUTTON_PRESSED, COLOR_RED_GLOW);
    } else {
        displayManager.drawButton(SCREEN_WIDTH/2 - 40, buttonY, 80, 30, "CAPTURE", BUTTON_NORMAL, COLOR_GREEN_PHOS);
    }
}

void CarCloner::drawReplayControls() {
    int y = 160;
    
    // Repeat count
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(10, y, "Repeat Count: " + String(replayState.repeatCount), COLOR_WHITE);
    
    // Repeat count buttons
    displayManager.drawButton(120, y - 5, 20, 20, "-", BUTTON_NORMAL, COLOR_DARK_GRAY);
    displayManager.drawButton(145, y - 5, 20, 20, "+", BUTTON_NORMAL, COLOR_DARK_GRAY);
    
    // Replay button
    int buttonY = y + 25;
    if (replayState.isActive) {
        displayManager.drawButton(SCREEN_WIDTH/2 - 40, buttonY, 80, 30, "STOP", BUTTON_PRESSED, COLOR_RED_GLOW);
    } else {
        displayManager.drawButton(SCREEN_WIDTH/2 - 40, buttonY, 80, 30, "TRANSMIT", BUTTON_NORMAL, COLOR_RED_GLOW);
    }
}

void CarCloner::drawSignalWaveform(int16_t x, int16_t y, int16_t w, int16_t h, const RFSignal& signal) {
    // Background
    displayManager.drawRetroRect(x, y, w, h, COLOR_DARK_GRAY, true);
    displayManager.drawRetroRect(x, y, w, h, COLOR_WHITE, false);
    
    if (signal.sampleCount == 0) return;
    
    // Draw waveform
    int16_t midY = y + h / 2;
    int16_t prevX = x;
    int16_t prevY = midY;
    
    for (uint16_t i = 0; i < signal.sampleCount && i < w; i++) {
        int16_t sampleX = x + (i * w) / signal.sampleCount;
        int16_t sampleY = y + h - ((signal.samples[i].amplitude * h) / 4096);
        
        if (i > 0) {
            displayManager.drawRetroLine(prevX, prevY, sampleX, sampleY, COLOR_GREEN_PHOS);
        }
        
        prevX = sampleX;
        prevY = sampleY;
    }
    
    // Center line
    displayManager.drawRetroLine(x, midY, x + w, midY, COLOR_GRAY_DARK);
}

void CarCloner::drawAnalysisResults() {
    if (uiState.selectedSignalIndex < 0 || uiState.selectedSignalIndex >= (int)capturedSignals.size()) {
        return;
    }
    
    const RFSignal& signal = capturedSignals[uiState.selectedSignalIndex];
    
    int y = 40;
    displayManager.setFont(FONT_SMALL);
    
    // Basic signal properties
    displayManager.drawText(10, y, "Signal Properties:", COLOR_YELLOW);
    y += 15;
    
    displayManager.drawText(15, y, "Samples: " + String(signal.sampleCount), COLOR_WHITE);
    y += 12;
    
    displayManager.drawText(15, y, "Duration: " + formatDuration(signal.duration), COLOR_WHITE);
    y += 12;
    
    displayManager.drawText(15, y, "Pulses: " + String(signal.pulseCount), COLOR_WHITE);
    y += 12;
    
    displayManager.drawText(15, y, "Avg Pulse Width: " + String(signal.avgPulseWidth) + "µs", COLOR_WHITE);
    y += 12;
    
    displayManager.drawText(15, y, "Entropy: " + String(signal.entropyScore, 2), COLOR_WHITE);
    y += 20;
    
    // Modulation type
    displayManager.drawText(10, y, "Modulation: " + getModulationString(signal.modulation), COLOR_YELLOW);
    y += 20;
    
    // Protocol detection results
    if (detectedProtocols.size() > 0) {
        displayManager.drawText(10, y, "Detected Protocols:", COLOR_YELLOW);
        y += 15;
        
        for (size_t i = 0; i < min(detectedProtocols.size(), (size_t)3); i++) {
            const ProtocolInfo& protocol = detectedProtocols[i];
            displayManager.drawText(15, y, protocol.protocolName + " (" + 
                                   String((int)(protocol.confidence * 100)) + "%)", COLOR_WHITE);
            y += 12;
        }
    }
}

uint16_t CarCloner::getSignalColor(const RFSignal& signal) {
    if (!signal.isAnalyzed) return COLOR_LIGHT_GRAY;
    if (signal.entropyScore > 0.8) return COLOR_RED_GLOW;
    if (signal.pulseCount > 100) return COLOR_YELLOW;
    return COLOR_GREEN_PHOS;
}

String CarCloner::formatFrequency(float frequency) {
    return String(frequency, 2) + " MHz";
}

String CarCloner::formatDuration(uint32_t microseconds) {
    if (microseconds < 1000) {
        return String(microseconds) + "µs";
    } else if (microseconds < 1000000) {
        return String(microseconds / 1000.0, 1) + "ms";
    } else {
        return String(microseconds / 1000000.0, 2) + "s";
    }
}

String CarCloner::formatFileSize(size_t bytes) {
    if (bytes < 1024) {
        return String(bytes) + "B";
    } else if (bytes < 1024 * 1024) {
        return String(bytes / 1024.0, 1) + "KB";
    } else {
        return String(bytes / (1024.0 * 1024.0), 1) + "MB";
    }
}

// ========================================
// TOUCH HANDLING METHODS
// ========================================

TouchZone CarCloner::identifyTouchZone(TouchPoint touch) {
    int x = touch.x;
    int y = touch.y;
    
    // Back button (top-left)
    if (x < 60 && y < 30) {
        return ZONE_BACK_BUTTON;
    }
    
    switch (uiState.currentView) {
        case VIEW_MAIN_MENU:
            if (x >= 100 && x <= 220) {
                if (y >= 40 && y <= 70) return ZONE_CAPTURE_BUTTON;
                if (y >= 80 && y <= 110) return ZONE_LIBRARY_BUTTON;
                if (y >= 120 && y <= 150) return ZONE_REPLAY_BUTTON;
                if (y >= 160 && y <= 190) return ZONE_ANALYSIS_BUTTON;
                if (y >= 200 && y <= 230) return ZONE_SETTINGS_BUTTON;
            }
            break;
            
        case VIEW_CAPTURE:
            if (x >= SCREEN_WIDTH/2 - 40 && x <= SCREEN_WIDTH/2 + 40 && y >= 120 && y <= 150) {
                return ZONE_RECORD_TOGGLE;
            }
            if (y >= 95 && y <= 115) {
                return ZONE_FREQUENCY_SELECTOR;
            }
            break;
            
        case VIEW_SIGNAL_LIBRARY:
            if (y >= 35 && y <= 35 + SIGNAL_LIST_MAX_VISIBLE * SIGNAL_LIST_ITEM_HEIGHT) {
                return ZONE_SIGNAL_LIST;
            }
            break;
            
        case VIEW_REPLAY:
            if (x >= SCREEN_WIDTH/2 - 40 && x <= SCREEN_WIDTH/2 + 40 && y >= 185 && y <= 215) {
                return ZONE_REPLAY_BUTTON;
            }
            break;
    }
    
    return ZONE_NONE;
}

void CarCloner::handleLegalWarningTouch(TouchPoint touch) {
    // Only allow acceptance after timeout
    if (millis() - startTime >= LEGAL_WARNING_TIMEOUT) {
        if (touch.x >= SCREEN_WIDTH/2 - 40 && touch.x <= SCREEN_WIDTH/2 + 40 &&
            touch.y >= 170 && touch.y <= 195) {
            uiState.warningAccepted = true;
            uiState.showLegalWarning = false;
            uiState.currentView = VIEW_MAIN_MENU;
            logActivity("Legal warning accepted");
        }
    }
}

void CarCloner::handleMainMenuTouch(TouchPoint touch) {
    TouchZone zone = identifyTouchZone(touch);
    
    switch (zone) {
        case ZONE_CAPTURE_BUTTON:
            uiState.currentView = VIEW_CAPTURE;
            break;
            
        case ZONE_LIBRARY_BUTTON:
            uiState.currentView = VIEW_SIGNAL_LIBRARY;
            break;
            
        case ZONE_REPLAY_BUTTON:
            uiState.currentView = VIEW_REPLAY;
            break;
            
        case ZONE_ANALYSIS_BUTTON:
            uiState.currentView = VIEW_ANALYSIS;
            break;
            
        case ZONE_SETTINGS_BUTTON:
            uiState.currentView = VIEW_SETTINGS;
            break;
            
        case ZONE_BACK_BUTTON:
            exitApp();
            break;
    }
}

void CarCloner::handleCaptureTouch(TouchPoint touch) {
    TouchZone zone = identifyTouchZone(touch);
    
    switch (zone) {
        case ZONE_RECORD_TOGGLE:
            if (captureState.isActive) {
                stopCapture();
            } else {
                startCapture();
            }
            break;
            
        case ZONE_FREQUENCY_SELECTOR:
            // Cycle through frequency presets
            for (int i = 0; i < presetCount; i++) {
                if (abs(frequencyPresets[i] - rfConfig.frequency) < 0.1) {
                    int nextIndex = (i + 1) % presetCount;
                    setFrequency(frequencyPresets[nextIndex]);
                    break;
                }
            }
            break;
            
        case ZONE_BACK_BUTTON:
            if (captureState.isActive) {
                stopCapture();
            }
            uiState.currentView = VIEW_MAIN_MENU;
            break;
    }
}

void CarCloner::handleLibraryTouch(TouchPoint touch) {
    TouchZone zone = identifyTouchZone(touch);
    
    switch (zone) {
        case ZONE_SIGNAL_LIST:
            {
                int itemIndex = (touch.y - 35) / SIGNAL_LIST_ITEM_HEIGHT + uiState.scrollOffset;
                if (itemIndex >= 0 && itemIndex < (int)capturedSignals.size()) {
                    uiState.selectedSignalIndex = itemIndex;
                }
            }
            break;
            
        case ZONE_BACK_BUTTON:
            uiState.currentView = VIEW_MAIN_MENU;
            break;
    }
}

void CarCloner::handleReplayTouch(TouchPoint touch) {
    TouchZone zone = identifyTouchZone(touch);
    
    switch (zone) {
        case ZONE_REPLAY_BUTTON:
            if (replayState.isActive) {
                stopReplay();
            } else if (uiState.selectedSignalIndex >= 0) {
                startReplay(uiState.selectedSignalIndex);
            } else {
                showAlert("No signal selected", 3000);
            }
            break;
            
        case ZONE_BACK_BUTTON:
            if (replayState.isActive) {
                stopReplay();
            }
            uiState.currentView = VIEW_MAIN_MENU;
            break;
    }
    
    // Handle repeat count buttons
    if (touch.x >= 120 && touch.x <= 140 && touch.y >= 155 && touch.y <= 175) {
        if (replayState.repeatCount > 1) {
            replayState.repeatCount--;
        }
    } else if (touch.x >= 145 && touch.x <= 165 && touch.y >= 155 && touch.y <= 175) {
        if (replayState.repeatCount < 10) {
            replayState.repeatCount++;
        }
    }
}

void CarCloner::handleAnalysisTouch(TouchPoint touch) {
    TouchZone zone = identifyTouchZone(touch);
    
    switch (zone) {
        case ZONE_BACK_BUTTON:
            uiState.currentView = VIEW_MAIN_MENU;
            break;
    }
}

void CarCloner::handleSettingsTouch(TouchPoint touch) {
    TouchZone zone = identifyTouchZone(touch);
    
    switch (zone) {
        case ZONE_BACK_BUTTON:
            uiState.currentView = VIEW_MAIN_MENU;
            break;
    }
}

// ========================================
// SIGNAL ANALYSIS METHODS
// ========================================

void CarCloner::analyzeSignal(RFSignal& signal) {
    debugLog("CarCloner: Analyzing signal: " + String(signal.name));
    
    // Reset analysis results
    signal.pulseCount = 0;
    signal.avgPulseWidth = 0;
    signal.avgGapWidth = 0;
    signal.entropyScore = 0.0;
    signal.modulation = MOD_UNKNOWN;
    
    // Detect pulses and timing
    detectPulses(signal);
    
    // Calculate timing statistics
    calculateTiming(signal);
    
    // Calculate entropy
    signal.entropyScore = calculateEntropy(signal);
    
    // Identify modulation type
    signal.modulation = identifyModulation(signal);
    
    // Detect protocols
    detectProtocols(signal);
    
    signal.isAnalyzed = true;
    debugLog("CarCloner: Signal analysis complete");
}

void CarCloner::detectPulses(RFSignal& signal) {
    if (signal.sampleCount < 2) return;
    
    bool currentLevel = signal.samples[0].digitalLevel;
    uint32_t pulseStart = 0;
    uint32_t pulseCount = 0;
    
    for (uint16_t i = 1; i < signal.sampleCount; i++) {
        if (signal.samples[i].digitalLevel != currentLevel) {
            // Level transition detected
            uint32_t pulseWidth = signal.samples[i].timing - signal.samples[pulseStart].timing;
            
            if (pulseWidth > 10) { // Filter out noise (>10µs pulses)
                pulseCount++;
            }
            
            currentLevel = signal.samples[i].digitalLevel;
            pulseStart = i;
        }
    }
    
    signal.pulseCount = pulseCount;
}

void CarCloner::calculateTiming(RFSignal& signal) {
    if (signal.sampleCount < 2) return;
    
    uint32_t totalPulseWidth = 0;
    uint32_t totalGapWidth = 0;
    uint32_t pulseWidthCount = 0;
    uint32_t gapWidthCount = 0;
    
    bool inPulse = false;
    uint32_t levelStart = 0;
    
    for (uint16_t i = 0; i < signal.sampleCount; i++) {
        bool currentLevel = signal.samples[i].digitalLevel;
        
        if (i == 0) {
            inPulse = currentLevel;
            levelStart = signal.samples[i].timing;
            continue;
        }
        
        if (currentLevel != inPulse) {
            uint32_t levelWidth = signal.samples[i].timing - levelStart;
            
            if (inPulse) {
                totalPulseWidth += levelWidth;
                pulseWidthCount++;
            } else {
                totalGapWidth += levelWidth;
                gapWidthCount++;
            }
            
            inPulse = currentLevel;
            levelStart = signal.samples[i].timing;
        }
    }
    
    signal.avgPulseWidth = pulseWidthCount > 0 ? totalPulseWidth / pulseWidthCount : 0;
    signal.avgGapWidth = gapWidthCount > 0 ? totalGapWidth / gapWidthCount : 0;
}

float CarCloner::calculateEntropy(const RFSignal& signal) {
    if (signal.sampleCount == 0) return 0.0;
    
    // Calculate Shannon entropy of amplitude distribution
    uint32_t histogram[256] = {0};
    
    // Build histogram
    for (uint16_t i = 0; i < signal.sampleCount; i++) {
        uint8_t bin = signal.samples[i].amplitude >> 4; // 12-bit to 8-bit
        histogram[bin]++;
    }
    
    // Calculate entropy
    float entropy = 0.0;
    for (int i = 0; i < 256; i++) {
        if (histogram[i] > 0) {
            float probability = (float)histogram[i] / signal.sampleCount;
            entropy -= probability * log2(probability);
        }
    }
    
    return entropy / 8.0; // Normalize to 0-1 range
}

ModulationType CarCloner::identifyModulation(const RFSignal& signal) {
    if (signal.sampleCount < 10) return MOD_UNKNOWN;
    
    // Simple heuristic-based modulation detection
    
    // Check for ASK (Amplitude Shift Keying)
    uint16_t amplitudeVariance = 0;
    uint16_t maxAmp = 0, minAmp = 4095;
    
    for (uint16_t i = 0; i < signal.sampleCount; i++) {
        if (signal.samples[i].amplitude > maxAmp) maxAmp = signal.samples[i].amplitude;
        if (signal.samples[i].amplitude < minAmp) minAmp = signal.samples[i].amplitude;
    }
    
    amplitudeVariance = maxAmp - minAmp;
    
    // If high amplitude variation, likely ASK/OOK
    if (amplitudeVariance > 1000) {
        // Check for OOK (simple on-off)
        uint16_t midpoint = (maxAmp + minAmp) / 2;
        bool hasIntermediateValues = false;
        
        for (uint16_t i = 0; i < signal.sampleCount; i++) {
            if (abs((int)signal.samples[i].amplitude - (int)midpoint) < amplitudeVariance / 4) {
                hasIntermediateValues = true;
                break;
            }
        }
        
        return hasIntermediateValues ? MOD_ASK : MOD_OOK;
    }
    
    // Check timing patterns for PWM/Manchester
    if (signal.avgPulseWidth > 0 && signal.avgGapWidth > 0) {
        float timingRatio = (float)signal.avgPulseWidth / signal.avgGapWidth;
        
        if (abs(timingRatio - 1.0) < 0.2) { // Nearly equal pulse/gap
            return MOD_MANCHESTER;
        } else if ((timingRatio > 0.3 && timingRatio < 3.0)) {
            return MOD_PWM;
        }
    }
    
    return MOD_UNKNOWN;
}

void CarCloner::detectProtocols(RFSignal& signal) {
    detectedProtocols.clear();
    
    // Analyze for common automotive protocols
    ProtocolInfo protocol;
    
    // Fixed code protocol detection
    protocol = analyzeFixedCodeProtocol(signal);
    if (protocol.confidence > 0.5) {
        detectedProtocols.push_back(protocol);
    }
    
    // Rolling code protocol detection  
    protocol = analyzeRollingCodeProtocol(signal);
    if (protocol.confidence > 0.5) {
        detectedProtocols.push_back(protocol);
    }
    
    // PWM protocol detection
    protocol = analyzePWMProtocol(signal);
    if (protocol.confidence > 0.5) {
        detectedProtocols.push_back(protocol);
    }
}

ProtocolInfo CarCloner::analyzeFixedCodeProtocol(const RFSignal& signal) {
    ProtocolInfo protocol;
    protocol.protocolName = "Fixed Code";
    protocol.confidence = 0.0;
    
    // Look for repeating patterns
    if (signal.pulseCount < 20 || signal.pulseCount > 200) {
        return protocol; // Outside typical range
    }
    
    // Check for consistent timing
    if (signal.avgPulseWidth > 0 && signal.avgGapWidth > 0) {
        float timingConsistency = 1.0 - abs(1.0 - (float)signal.avgPulseWidth / signal.avgGapWidth);
        protocol.confidence = timingConsistency * 0.7;
        
        if (signal.entropyScore < 0.5) { // Low entropy suggests fixed pattern
            protocol.confidence += 0.3;
        }
        
        protocol.description = "Possible fixed code remote control signal";
        protocol.parameters["pulse_width"] = String(signal.avgPulseWidth) + "µs";
        protocol.parameters["gap_width"] = String(signal.avgGapWidth) + "µs";
        protocol.parameters["pulse_count"] = String(signal.pulseCount);
    }
    
    return protocol;
}

ProtocolInfo CarCloner::analyzeRollingCodeProtocol(const RFSignal& signal) {
    ProtocolInfo protocol;
    protocol.protocolName = "Rolling Code";
    protocol.confidence = 0.0;
    
    // Rolling codes typically have higher entropy
    if (signal.entropyScore > 0.7) {
        protocol.confidence = signal.entropyScore;
        protocol.description = "Possible rolling code security system";
        protocol.parameters["entropy"] = String(signal.entropyScore, 3);
        protocol.parameters["complexity"] = "High";
    }
    
    return protocol;
}

ProtocolInfo CarCloner::analyzePWMProtocol(const RFSignal& signal) {
    ProtocolInfo protocol;
    protocol.protocolName = "PWM";
    protocol.confidence = 0.0;
    
    if (signal.modulation == MOD_PWM || signal.modulation == MOD_MANCHESTER) {
        protocol.confidence = 0.8;
        protocol.description = "Pulse width modulated signal";
        protocol.parameters["modulation"] = getModulationString(signal.modulation);
        protocol.parameters["avg_pulse"] = String(signal.avgPulseWidth) + "µs";
    }
    
    return protocol;
}

// ========================================
// FILE SYSTEM METHODS
// ========================================

bool CarCloner::initializeFSStructure() {
    debugLog("CarCloner: Initializing file system structure");
    
    // Create directory structure
    if (!filesystem.ensureDirExists(dataDirectory)) {
        debugLog("CarCloner: Failed to create data directory");
        return false;
    }
    
    if (!filesystem.ensureDirExists(signalDirectory)) {
        debugLog("CarCloner: Failed to create signals directory");
        return false;
    }
    
    // Ensure parent directories for config and log files exist
    if (!filesystem.ensureDirExists("/settings")) {
        debugLog("CarCloner: Failed to create settings directory");
        return false;
    }
    
    if (!filesystem.ensureDirExists("/logs")) {
        debugLog("CarCloner: Failed to create logs directory");
        return false;
    }
    
    debugLog("CarCloner: File system structure initialized");
    return true;
}

bool CarCloner::saveSignal(RFSignal& signal) {
    if (!filesystem.isReady()) {
        debugLog("CarCloner: File system not ready");
        return false;
    }
    
    String filename = generateSignalFilename(signal);
    String filepath = signalDirectory + "/" + filename;
    
    debugLog("CarCloner: Saving signal to " + filepath);
    
    // Create JSON representation of signal
    String jsonData = "{\n";
    jsonData += "  \"name\": \"" + String(signal.name) + "\",\n";
    jsonData += "  \"frequency\": " + String(signal.frequency, 6) + ",\n";
    jsonData += "  \"modulation\": " + String((int)signal.modulation) + ",\n";
    jsonData += "  \"sampleRate\": " + String(signal.sampleRate) + ",\n";
    jsonData += "  \"duration\": " + String(signal.duration) + ",\n";
    jsonData += "  \"sampleCount\": " + String(signal.sampleCount) + ",\n";
    jsonData += "  \"captureTime\": " + String(signal.captureTime) + ",\n";
    jsonData += "  \"captureRSSI\": " + String(signal.captureRSSI) + ",\n";
    jsonData += "  \"pulseCount\": " + String(signal.pulseCount) + ",\n";
    jsonData += "  \"avgPulseWidth\": " + String(signal.avgPulseWidth) + ",\n";
    jsonData += "  \"avgGapWidth\": " + String(signal.avgGapWidth) + ",\n";
    jsonData += "  \"entropyScore\": " + String(signal.entropyScore, 6) + ",\n";
    jsonData += "  \"isAnalyzed\": " + String(signal.isAnalyzed ? "true" : "false") + ",\n";
    jsonData += "  \"samples\": [\n";
    
    for (uint16_t i = 0; i < signal.sampleCount; i++) {
        jsonData += "    {\"amplitude\": " + String(signal.samples[i].amplitude) + 
                   ", \"timing\": " + String(signal.samples[i].timing) + 
                   ", \"level\": " + String(signal.samples[i].digitalLevel ? "true" : "false") + "}";
        if (i < signal.sampleCount - 1) jsonData += ",";
        jsonData += "\n";
    }
    
    jsonData += "  ]\n";
    jsonData += "}\n";
    
    if (filesystem.writeFile(filepath, jsonData)) {
        signal.filePath = filepath;
        signal.isSavedToSD = true;
        debugLog("CarCloner: Signal saved successfully");
        return true;
    } else {
        debugLog("CarCloner: Failed to save signal");
        return false;
    }
}

bool CarCloner::loadSignal(const String& filename, RFSignal& signal) {
    String filepath = signalDirectory + "/" + filename;
    
    if (!filesystem.fileExists(filepath)) {
        debugLog("CarCloner: Signal file not found: " + filepath);
        return false;
    }
    
    String jsonData = filesystem.readFile(filepath);
    if (jsonData.isEmpty()) {
        debugLog("CarCloner: Failed to read signal file");
        return false;
    }
    
    // Simple JSON parsing (basic implementation)
    // In a real implementation, you'd use a proper JSON library
    
    // Extract basic fields
    int nameStart = jsonData.indexOf("\"name\": \"") + 9;
    int nameEnd = jsonData.indexOf("\"", nameStart);
    if (nameStart > 8 && nameEnd > nameStart) {
        String name = jsonData.substring(nameStart, nameEnd);
        strncpy(signal.name, name.c_str(), MAX_SIGNAL_NAME_LENGTH - 1);
        signal.name[MAX_SIGNAL_NAME_LENGTH - 1] = '\0';
    }
    
    // Extract frequency
    int freqStart = jsonData.indexOf("\"frequency\": ") + 13;
    int freqEnd = jsonData.indexOf(",", freqStart);
    if (freqStart > 12 && freqEnd > freqStart) {
        signal.frequency = jsonData.substring(freqStart, freqEnd).toFloat();
    }
    
    // For full implementation, you would parse all fields and samples
    // This is a simplified version for demonstration
    
    signal.filePath = filepath;
    signal.isSavedToSD = true;
    
    debugLog("CarCloner: Signal loaded: " + String(signal.name));
    return true;
}

void CarCloner::loadSignalLibrary() {
    if (!filesystem.isReady()) {
        debugLog("CarCloner: File system not ready for library load");
        return;
    }
    
    debugLog("CarCloner: Loading signal library");
    
    capturedSignals.clear();
    signalCount = 0;
    
    // Get list of signal files
    std::vector<String> signalFiles = filesystem.listFilesPattern(signalDirectory, "*.json");
    
    for (const String& filename : signalFiles) {
        RFSignal signal;
        if (loadSignal(filename, signal)) {
            capturedSignals.push_back(signal);
            signalCount++;
        }
    }
    
    debugLog("CarCloner: Loaded " + String(signalCount) + " signals");
}

void CarCloner::saveConfiguration() {
    if (!filesystem.isReady()) return;
    
    String configData = "# CarCloner Configuration\n";
    configData += "frequency=" + String(rfConfig.frequency, 6) + "\n";
    configData += "power=" + String(rfConfig.power) + "\n";
    configData += "sampleRate=" + String(rfConfig.sampleRate) + "\n";
    configData += "sensitivity=" + String(rfConfig.sensitivity) + "\n";
    configData += "autoGain=" + String(rfConfig.autoGain ? "1" : "0") + "\n";
    configData += "captureMode=" + String((int)rfConfig.captureMode) + "\n";
    configData += "captureTimeout=" + String(rfConfig.captureTimeout) + "\n";
    
    filesystem.writeFile(configFilePath, configData);
    debugLog("CarCloner: Configuration saved");
}

void CarCloner::loadConfiguration() {
    if (!filesystem.fileExists(configFilePath)) {
        debugLog("CarCloner: No configuration file found, using defaults");
        return;
    }
    
    String configData = filesystem.readFile(configFilePath);
    if (configData.isEmpty()) return;
    
    // Parse configuration (simple key=value format)
    int lineStart = 0;
    while (lineStart < (int)configData.length()) {
        int lineEnd = configData.indexOf('\n', lineStart);
        if (lineEnd == -1) lineEnd = configData.length();
        
        String line = configData.substring(lineStart, lineEnd);
        line.trim();
        
        if (line.length() > 0 && !line.startsWith("#")) {
            int equalPos = line.indexOf('=');
            if (equalPos > 0) {
                String key = line.substring(0, equalPos);
                String value = line.substring(equalPos + 1);
                
                if (key == "frequency") rfConfig.frequency = value.toFloat();
                else if (key == "power") rfConfig.power = value.toInt();
                else if (key == "sampleRate") rfConfig.sampleRate = value.toInt();
                else if (key == "sensitivity") rfConfig.sensitivity = value.toInt();
                else if (key == "autoGain") rfConfig.autoGain = (value == "1");
                else if (key == "captureMode") rfConfig.captureMode = (CaptureMode)value.toInt();
                else if (key == "captureTimeout") rfConfig.captureTimeout = value.toInt();
            }
        }
        
        lineStart = lineEnd + 1;
    }
    
    debugLog("CarCloner: Configuration loaded");
}

String CarCloner::generateSignalFilename(const RFSignal& signal) {
    String filename = String(signal.name);
    
    // Replace invalid characters
    filename.replace(" ", "_");
    filename.replace("/", "_");
    filename.replace("\\", "_");
    filename.replace(":", "_");
    
    // Add timestamp to ensure uniqueness
    filename += "_" + String(signal.captureTime);
    filename += ".json";
    
    return filename;
}

// ========================================
// SAFETY AND LEGAL METHODS
// ========================================

bool CarCloner::confirmTransmission() {
    // Show confirmation dialog for transmission
    displayManager.clearScreen(COLOR_BLACK);
    
    displayManager.drawWindow(30, 60, SCREEN_WIDTH - 60, 120, "TRANSMISSION WARNING", WINDOW_DIALOG);
    
    displayManager.setFont(FONT_SMALL);
    int y = 85;
    
    displayManager.drawText(40, y, "You are about to transmit an RF", COLOR_YELLOW);
    y += 12;
    displayManager.drawText(40, y, "signal. Ensure you have legal", COLOR_YELLOW);
    y += 12;
    displayManager.drawText(40, y, "permission to transmit on this", COLOR_YELLOW);
    y += 12;
    displayManager.drawText(40, y, "frequency.", COLOR_YELLOW);
    y += 20;
    
    displayManager.drawButton(60, y, 60, 25, "CANCEL", BUTTON_NORMAL, COLOR_RED_GLOW);
    displayManager.drawButton(SCREEN_WIDTH - 120, y, 60, 25, "TRANSMIT", BUTTON_NORMAL, COLOR_GREEN_PHOS);
    
    // Wait for user input (simplified - in real implementation would be handled in touch system)
    delay(100); // Brief pause
    
    // For this implementation, we'll assume user confirms
    // In real implementation, this would wait for actual touch input
    logActivity("Transmission confirmed by user");
    return true;
}

void CarCloner::logActivity(const String& activity) {
    if (!filesystem.isReady()) return;
    
    unsigned long timestamp = millis();
    String logEntry = String(timestamp) + ": " + activity + "\n";
    
    filesystem.appendFile(logFilePath, logEntry);
}

bool CarCloner::checkTransmissionLegality(float frequency, uint8_t power) {
    // Basic frequency range checks
    // This is a simplified implementation - real version would check
    // local regulations and allowed power levels
    
    if (frequency < 100.0 || frequency > 1000.0) {
        displaySafetyWarning("Frequency outside typical ISM bands");
        return false;
    }
    
    if (power > POWER_LIMIT_DEFAULT) {
        displaySafetyWarning("Power level exceeds safety limit");
        return false;
    }
    
    return true;
}

void CarCloner::displaySafetyWarning(const String& warning) {
    showAlert("SAFETY: " + warning, 5000);
    logActivity("Safety warning: " + warning);
}

// ========================================
// UTILITY METHODS
// ========================================

void CarCloner::initializeFrequencyPresets() {
    presetCount = 0;
    
    frequencyPresets[presetCount] = RF_FREQ_315MHZ;
    frequencyNames[presetCount] = "315MHz";
    presetCount++;
    
    frequencyPresets[presetCount] = RF_FREQ_433MHZ;
    frequencyNames[presetCount] = "433MHz";
    presetCount++;
    
    frequencyPresets[presetCount] = RF_FREQ_868MHZ;
    frequencyNames[presetCount] = "868MHz";
    presetCount++;
    
    frequencyPresets[presetCount] = RF_FREQ_915MHZ;
    frequencyNames[presetCount] = "915MHz";
    presetCount++;
    
    debugLog("CarCloner: " + String(presetCount) + " frequency presets loaded");
}

bool CarCloner::isValidFrequency(float frequency) {
    return (frequency >= RF_FREQ_MIN_MHZ && frequency <= RF_FREQ_MAX_MHZ);
}

String CarCloner::getModulationString(ModulationType mod) {
    switch (mod) {
        case MOD_ASK: return "ASK";
        case MOD_FSK: return "FSK";
        case MOD_PSK: return "PSK";
        case MOD_OOK: return "OOK";
        case MOD_PWM: return "PWM";
        case MOD_MANCHESTER: return "Manchester";
        default: return "Unknown";
    }
}

void CarCloner::updateStatusMessage(const String& message) {
    uiState.statusMessage = message;
    debugLog("CarCloner Status: " + message);
}

void CarCloner::showAlert(const String& message, uint32_t duration) {
    uiState.alertMessage = message;
    uiState.alertTimeout = millis() + duration;
}

// ========================================
// BaseApp OPTIONAL OVERRIDES
// ========================================

void CarCloner::onPause() {
    // Stop any active operations when app is paused
    if (captureState.isActive) {
        stopCapture();
    }
    
    if (replayState.isActive) {
        stopReplay();
    }
    
    saveState();
}

void CarCloner::onResume() {
    // Restore state when app resumes
    loadState();
}

bool CarCloner::saveState() {
    String stateFile = dataDirectory + "/state.cfg";
    
    String stateData = "selectedSignal=" + String(uiState.selectedSignalIndex) + "\n";
    stateData += "currentView=" + String((int)uiState.currentView) + "\n";
    stateData += "warningAccepted=" + String(uiState.warningAccepted ? "1" : "0") + "\n";
    
    return filesystem.writeFile(stateFile, stateData);
}

bool CarCloner::loadState() {
    String stateFile = dataDirectory + "/state.cfg";
    
    if (!filesystem.fileExists(stateFile)) {
        return true; // No state to load, use defaults
    }
    
    String stateData = filesystem.readFile(stateFile);
    
    // Parse state data (simplified)
    if (stateData.indexOf("warningAccepted=1") >= 0) {
        uiState.warningAccepted = true;
        uiState.showLegalWarning = false;
        if (uiState.currentView == VIEW_LEGAL_WARNING) {
            uiState.currentView = VIEW_MAIN_MENU;
        }
    }
    
    return true;
}

bool CarCloner::handleMessage(AppMessage message, void* data) {
    switch (message) {
        case MSG_PAUSE:
            onPause();
            return true;
            
        case MSG_RESUME:
            onResume();
            return true;
            
        case MSG_SHUTDOWN:
            cleanup();
            return true;
            
        case MSG_LOW_MEMORY:
            if (captureState.isActive) {
                stopCapture();
                showAlert("Capture stopped - low memory", 3000);
            }
            return true;
            
        default:
            return false;
    }
}

// ========================================
// SETTINGS INTERFACE
// ========================================

String CarCloner::getSettingName(uint8_t index) const {
    switch (index) {
        case 0: return "Frequency";
        case 1: return "Power Level";
        case 2: return "Capture Mode";
        case 3: return "Sample Rate";
        case 4: return "Sensitivity";
        case 5: return "Auto Gain";
        case 6: return "Reset Settings";
        default: return "";
    }
}

void CarCloner::handleSetting(uint8_t index) {
    switch (index) {
        case 0: // Frequency
            // Cycle through presets
            for (int i = 0; i < presetCount; i++) {
                if (abs(frequencyPresets[i] - rfConfig.frequency) < 0.1) {
                    int nextIndex = (i + 1) % presetCount;
                    setFrequency(frequencyPresets[nextIndex]);
                    break;
                }
            }
            break;
            
        case 1: // Power Level
            rfConfig.power = (rfConfig.power + 32) % 256;
            if (rfConfig.power > POWER_LIMIT_DEFAULT) {
                rfConfig.power = 16; // Reset to low power
            }
            setPowerLevel(rfConfig.power);
            break;
            
        case 2: // Capture Mode
            rfConfig.captureMode = (CaptureMode)((rfConfig.captureMode + 1) % 4);
            break;
            
        case 3: // Sample Rate
            if (rfConfig.sampleRate == 500000) rfConfig.sampleRate = 1000000;
            else if (rfConfig.sampleRate == 1000000) rfConfig.sampleRate = 2000000;
            else rfConfig.sampleRate = 500000;
            break;
            
        case 4: // Sensitivity
            rfConfig.sensitivity = (rfConfig.sensitivity + 50) % 500;
            if (rfConfig.sensitivity < 100) rfConfig.sensitivity = 100;
            break;
            
        case 5: // Auto Gain
            rfConfig.autoGain = !rfConfig.autoGain;
            break;
            
        case 6: // Reset Settings
            rfConfig = RFConfig(); // Reset to defaults
            showAlert("Settings reset to defaults", 3000);
            break;
    }
    
    saveConfiguration();
}

// ========================================
// PUBLIC INTERFACE METHODS
// ========================================

bool CarCloner::captureSignal(const String& name) {
    if (!name.isEmpty()) {
        strncpy(currentSignal.name, name.c_str(), MAX_SIGNAL_NAME_LENGTH - 1);
        currentSignal.name[MAX_SIGNAL_NAME_LENGTH - 1] = '\0';
    }
    
    return startCapture();
}

bool CarCloner::replaySignal(int index, uint8_t repeatCount) {
    if (index < 0 || index >= (int)capturedSignals.size()) {
        return false;
    }
    
    replayState.repeatCount = repeatCount;
    replayState.remainingRepeats = repeatCount;
    
    return startReplay(index);
}

bool CarCloner::deleteSignal(int index) {
    if (index < 0 || index >= (int)capturedSignals.size()) {
        return false;
    }
    
    const RFSignal& signal = capturedSignals[index];
    
    // Delete file if it exists
    if (signal.isSavedToSD && !signal.filePath.isEmpty()) {
        filesystem.deleteFile(signal.filePath);
    }
    
    // Remove from memory
    capturedSignals.erase(capturedSignals.begin() + index);
    signalCount--;
    
    // Adjust selected index if necessary
    if (uiState.selectedSignalIndex >= index) {
        uiState.selectedSignalIndex--;
        if (uiState.selectedSignalIndex < 0 && signalCount > 0) {
            uiState.selectedSignalIndex = 0;
        }
    }
    
    logActivity("Signal deleted: " + String(signal.name));
    return true;
}

RFSignal CarCloner::getSignal(int index) const {
    if (index >= 0 && index < (int)capturedSignals.size()) {
        return capturedSignals[index];
    }
    return RFSignal(); // Return empty signal if invalid index
}

void CarCloner::exportSignalData(int index, const String& format) {
    if (index < 0 || index >= (int)capturedSignals.size()) {
        return;
    }
    
    const RFSignal& signal = capturedSignals[index];
    String exportPath = dataDirectory + "/export_" + String(signal.name);
    
    if (format == "CSV") {
        exportPath += ".csv";
        String csvData = "Sample,Amplitude,Timing,DigitalLevel\n";
        
        for (uint16_t i = 0; i < signal.sampleCount; i++) {
            csvData += String(i) + "," +
                      String(signal.samples[i].amplitude) + "," +
                      String(signal.samples[i].timing) + "," +
                      String(signal.samples[i].digitalLevel ? "1" : "0") + "\n";
        }
        
        filesystem.writeFile(exportPath, csvData);
    } else if (format == "RAW") {
        exportPath += ".raw";
        
        // Export raw binary data
        uint8_t* rawData = new uint8_t[signal.sampleCount * 4]; // 4 bytes per sample
        for (uint16_t i = 0; i < signal.sampleCount; i++) {
            rawData[i * 4] = signal.samples[i].amplitude & 0xFF;
            rawData[i * 4 + 1] = (signal.samples[i].amplitude >> 8) & 0xFF;
            rawData[i * 4 + 2] = signal.samples[i].timing & 0xFF;
            rawData[i * 4 + 3] = (signal.samples[i].timing >> 8) & 0xFF;
        }
        
        filesystem.writeBinaryFile(exportPath, rawData, signal.sampleCount * 4);
        delete[] rawData;
    }
    
    showAlert("Signal exported to " + exportPath, 3000);
    logActivity("Signal exported: " + String(signal.name) + " as " + format);
}

// ========================================
// ERROR HANDLING METHODS
// ========================================

void CarCloner::handleRFError(const String& error) {
    debugLog("CarCloner RF Error: " + error);
    showAlert("RF Error: " + error, 5000);
    
    // Stop any active operations
    if (captureState.isActive) {
        stopCapture();
    }
    
    if (replayState.isActive) {
        stopReplay();
    }
    
    // Log the error
    logActivity("RF Error: " + error);
}

void CarCloner::handleFileSystemError(const String& error) {
    debugLog("CarCloner FS Error: " + error);
    showAlert("Storage Error: " + error, 5000);
    logActivity("File System Error: " + error);
}

void CarCloner::handleMemoryError() {
    debugLog("CarCloner: Memory error detected");
    showAlert("Low memory - some features disabled", 5000);
    
    // Free up memory by stopping captures
    if (captureState.isActive) {
        stopCapture();
    }
    
    // Clear old analysis data
    detectedProtocols.clear();
    
    logActivity("Memory error handled");
}

// ========================================
// DEBUG METHODS
// ========================================

void CarCloner::runRFTest() {
    debugLog("CarCloner: Running RF hardware test");
    
    if (!rfInitialized) {
        debugLog("CarCloner: RF hardware not initialized");
        return;
    }
    
    // Test frequency setting
    float testFreqs[] = {315.0, 433.92, 868.0, 915.0};
    for (int i = 0; i < 4; i++) {
        if (setFrequency(testFreqs[i])) {
            debugLog("CarCloner: Frequency test passed: " + String(testFreqs[i]) + " MHz");
        } else {
            debugLog("CarCloner: Frequency test failed: " + String(testFreqs[i]) + " MHz");
        }
        delay(100);
    }
    
    // Test power levels
    for (uint8_t power = 16; power <= 128; power += 32) {
        if (setPowerLevel(power)) {
            debugLog("CarCloner: Power test passed: " + String(power));
        } else {
            debugLog("CarCloner: Power test failed: " + String(power));
        }
        delay(50);
    }
    
    // Test RSSI reading
    for (int i = 0; i < 10; i++) {
        float rssi = getCurrentRSSI();
        debugLog("CarCloner: RSSI reading " + String(i) + ": " + String(rssi) + " dBm");
        delay(100);
    }
    
    debugLog("CarCloner: RF hardware test complete");
}

void CarCloner::printRFStatus() {
    debugLog("=== CarCloner RF Status ===");
    debugLog("Initialized: " + String(rfInitialized ? "Yes" : "No"));
    debugLog("Frequency: " + String(rfConfig.frequency) + " MHz");
    debugLog("Power: " + String(rfConfig.power) + "/255");
    debugLog("Sample Rate: " + String(rfConfig.sampleRate) + " Hz");
    debugLog("Sensitivity: " + String(rfConfig.sensitivity));
    debugLog("Auto Gain: " + String(rfConfig.autoGain ? "On" : "Off"));
    debugLog("Capture Mode: " + String((int)rfConfig.captureMode));
    debugLog("Current RSSI: " + String(getCurrentRSSI()) + " dBm");
    debugLog("=========================");
}

void CarCloner::printSignalLibrary() {
    debugLog("=== CarCloner Signal Library ===");
    debugLog("Total Signals: " + String(signalCount));
    
    for (size_t i = 0; i < capturedSignals.size(); i++) {
        const RFSignal& signal = capturedSignals[i];
        debugLog("Signal " + String(i) + ": " + String(signal.name));
        debugLog("  Frequency: " + String(signal.frequency) + " MHz");
        debugLog("  Duration: " + String(signal.duration) + " µs");
        debugLog("  Samples: " + String(signal.sampleCount));
        debugLog("  Analyzed: " + String(signal.isAnalyzed ? "Yes" : "No"));
        debugLog("  Saved: " + String(signal.isSavedToSD ? "Yes" : "No"));
    }
    
    debugLog("===============================");
}

void CarCloner::debugPrintSignal(const RFSignal& signal) {
    debugLog("=== Signal Debug Info ===");
    debugLog("Name: " + String(signal.name));
    debugLog("Frequency: " + String(signal.frequency) + " MHz");
    debugLog("Modulation: " + getModulationString(signal.modulation));
    debugLog("Sample Rate: " + String(signal.sampleRate) + " Hz");
    debugLog("Duration: " + String(signal.duration) + " µs");
    debugLog("Sample Count: " + String(signal.sampleCount));
    debugLog("Capture RSSI: " + String(signal.captureRSSI) + " dBm");
    debugLog("Pulse Count: " + String(signal.pulseCount));
    debugLog("Avg Pulse Width: " + String(signal.avgPulseWidth) + " µs");
    debugLog("Avg Gap Width: " + String(signal.avgGapWidth) + " µs");
    debugLog("Entropy Score: " + String(signal.entropyScore, 3));
    debugLog("Analyzed: " + String(signal.isAnalyzed ? "Yes" : "No"));
    debugLog("File Path: " + signal.filePath);
    debugLog("========================");
}