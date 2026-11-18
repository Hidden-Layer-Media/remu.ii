#include "FreqScanner.h"
#include <math.h>

// ========================================
// FreqScanner Implementation
// Advanced spectrum analysis and signal processing
// ========================================

// Icon data for FreqScanner (16x16 pixels, 1-bit per pixel)
const uint8_t freq_scanner_icon[32] = {
    0x00, 0x00, 0x3F, 0xFC, 0x40, 0x02, 0x80, 0x01, 0x90, 0x09, 0xA8, 0x15,
    0xAC, 0x35, 0xAE, 0x75, 0xAE, 0x75, 0xAC, 0x35, 0xA8, 0x15, 0x90, 0x09,
    0x80, 0x01, 0x40, 0x02, 0x3F, 0xFC, 0x00, 0x00
};

FreqScanner::FreqScanner() {
    // Initialize basic app metadata
    metadata.name = "FreqScanner";
    metadata.version = "1.0.0";
    metadata.author = "remu.ii";
    metadata.description = "Spectrum analyzer with FFT processing";
    metadata.category = CATEGORY_TOOLS;
    metadata.icon = freq_scanner_icon;
    metadata.memoryRequirement = 65536; // 64KB for FFT buffers
    metadata.requiresSD = true;
    metadata.requiresWiFi = false;
    metadata.requiresBLE = false;
    
    // Initialize state
    isProcessing = false;
    needsRedraw = true;
    lastFFTTime = 0;
    lastDisplayUpdate = 0;
    adcSampleTimer = 0;
    noiseFloor = -80.0;
    averagingBuffer = nullptr;
    averagingCount = 0;
    
    // Initialize colors
    colorBackground = COLOR_BLACK;
    colorGrid = COLOR_DARK_GRAY;
    colorSpectrum = COLOR_GREEN_PHOS;
    colorWaterfall = COLOR_BLUE_CYBER;
    colorPeaks = COLOR_RED_GLOW;
    colorMarkers = COLOR_YELLOW;
    colorText = COLOR_WHITE;
    
    // Initialize file paths
    configFilePath = FREQ_SCANNER_CONFIG;
    recordingsPath = RECORDINGS_DIR;
    settingsPath = FREQ_SCANNER_DATA_DIR;
}

FreqScanner::~FreqScanner() {
    cleanup();
}

bool FreqScanner::initialize() {
    debugLog("FreqScanner: Initializing");
    
    setState(APP_INITIALIZING);
    
    // Initialize directories
    if (!filesystem.ensureDirExists(FREQ_SCANNER_DATA_DIR)) {
        debugLog("FreqScanner: Failed to create data directory");
        setState(APP_ERROR);
        return false;
    }
    
    if (!filesystem.ensureDirExists(RECORDINGS_DIR)) {
        debugLog("FreqScanner: Failed to create recordings directory");
        setState(APP_ERROR);
        return false;
    }
    
    // Load configuration
    loadConfiguration();
    
    // Initialize FFT processor
    if (!initializeFFT()) {
        debugLog("FreqScanner: FFT initialization failed");
        setState(APP_ERROR);
        return false;
    }
    
    // Initialize waterfall display
    if (!initializeWaterfall()) {
        debugLog("FreqScanner: Waterfall initialization failed");
        setState(APP_ERROR);
        return false;
    }
    
    // Initialize signal generator
    if (!initializeGenerator()) {
        debugLog("FreqScanner: Generator initialization failed");
        setState(APP_ERROR);
        return false;
    }
    
    // Initialize averaging buffer
    averagingBuffer = new float[config.fftSize / 2];
    if (!averagingBuffer) {
        debugLog("FreqScanner: Failed to allocate averaging buffer");
        setState(APP_ERROR);
        return false;
    }
    
    // Clear averaging buffer
    for (uint16_t i = 0; i < config.fftSize / 2; i++) {
        averagingBuffer[i] = -120.0;
    }
    
    // Set initial view
    uiState.currentView = config.defaultView;
    
    // Initialize timing
    lastFFTTime = millis();
    lastDisplayUpdate = millis();
    
    setState(APP_RUNNING);
    debugLog("FreqScanner: Initialization complete");
    return true;
}

void FreqScanner::update() {
    if (currentState != APP_RUNNING) return;
    
    unsigned long currentTime = millis();
    
    // Process FFT at regular intervals
    if (currentTime - lastFFTTime >= (1000 / 30)) { // 30 FPS processing
        if (processFFT()) {
            lastFFTTime = currentTime;
            needsRedraw = true;
        }
    }
    
    // Update signal generator
    if (signalGenerator.isEnabled) {
        updateGenerator();
    }
    
    // Update waterfall display
    if (uiState.currentView == VIEW_WATERFALL || uiState.currentView == VIEW_DUAL) {
        updateWaterfall();
    }
    
    // Update statistics
    updateStatistics();
    
    // Check for auto-recording triggers
    if (config.autoRecord && !signalRecording.isRecording) {
        // Auto-record if signal exceeds threshold
        for (const auto& peak : detectedPeaks) {
            if (peak.magnitude > config.peakThreshold + 20) {
                startRecording(generateRecordingFilename());
                break;
            }
        }
    }
    
    // Update frame count
    frameCount++;
}

void FreqScanner::render() {
    if (currentState != APP_RUNNING || !needsRedraw) return;
    
    unsigned long currentTime = millis();
    if (currentTime - lastDisplayUpdate < 33) return; // Limit to 30 FPS
    
    // Clear screen
    displayManager.clearScreen(colorBackground);
    
    // Render based on current view mode
    switch (uiState.currentView) {
        case VIEW_SPECTRUM:
            renderSpectrum();
            break;
        case VIEW_WATERFALL:
            renderWaterfall();
            break;
        case VIEW_DUAL:
            renderDualView();
            break;
        case VIEW_RECORDING:
            renderRecordingInterface();
            break;
        case VIEW_GENERATOR:
            renderGeneratorInterface();
            break;
        case VIEW_SETTINGS:
            renderSettingsPanel();
            break;
    }
    
    // Always render status bar
    renderStatusBar();
    
    needsRedraw = false;
    lastDisplayUpdate = currentTime;
}

bool FreqScanner::handleTouch(TouchPoint touch) {
    if (currentState != APP_RUNNING) return false;
    
    uiState.lastTouch = touch;
    uiState.lastTouchTime = millis();
    
    if (!touch.isPressed) return false;
    
    // Identify touch zone
    TouchZone zone = identifyTouchZone(touch);
    
    switch (zone) {
        case ZONE_BACK_BUTTON:
            exitApp();
            return true;
            
        case ZONE_VIEW_TOGGLE:
            // Cycle through view modes
            switch (uiState.currentView) {
                case VIEW_SPECTRUM: uiState.currentView = VIEW_WATERFALL; break;
                case VIEW_WATERFALL: uiState.currentView = VIEW_DUAL; break;
                case VIEW_DUAL: uiState.currentView = VIEW_SPECTRUM; break;
                default: uiState.currentView = VIEW_SPECTRUM; break;
            }
            needsRedraw = true;
            return true;
            
        case ZONE_RECORD_BUTTON:
            toggleRecording();
            return true;
            
        case ZONE_GENERATOR_BUTTON:
            toggleGenerator();
            return true;
            
        case ZONE_SETTINGS_BUTTON:
            uiState.currentView = VIEW_SETTINGS;
            needsRedraw = true;
            return true;
            
        case ZONE_SPECTRUM_AREA:
            handleSpectrumTouch(touch);
            return true;
            
        case ZONE_WATERFALL_AREA:
            handleWaterfallTouch(touch);
            return true;
            
        case ZONE_CONTROL_PANEL:
            handleControlPanelTouch(touch);
            return true;
            
        default:
            break;
    }
    
    return false;
}

void FreqScanner::cleanup() {
    debugLog("FreqScanner: Cleaning up");
    
    // Stop any active recording
    if (signalRecording.isRecording) {
        stopRecording();
    }
    
    // Shutdown components
    shutdownFFT();
    shutdownWaterfall();
    shutdownGenerator();
    
    // Free memory
    if (averagingBuffer) {
        delete[] averagingBuffer;
        averagingBuffer = nullptr;
    }
    
    // Save state
    saveConfiguration();
    
    setState(APP_CLEANUP);
}

String FreqScanner::getName() const {
    return "FreqScanner";
}

const uint8_t* FreqScanner::getIcon() const {
    return freq_scanner_icon;
}

// ===== FFT PROCESSING IMPLEMENTATION =====

bool FreqScanner::initializeFFT() {
    debugLog("FreqScanner: Initializing FFT processor");
    
    // Allocate FFT buffers
    fftProcessor.inputBuffer = new float[config.fftSize];
    fftProcessor.windowBuffer = new float[config.fftSize];
    fftProcessor.fftBuffer = new std::complex<float>[config.fftSize];
    fftProcessor.magnitudeSpectrum = new float[config.fftSize / 2];
    fftProcessor.phaseSpectrum = new float[config.fftSize / 2];
    fftProcessor.smoothedSpectrum = new float[config.fftSize / 2];
    
    if (!fftProcessor.inputBuffer || !fftProcessor.windowBuffer || 
        !fftProcessor.fftBuffer || !fftProcessor.magnitudeSpectrum ||
        !fftProcessor.phaseSpectrum || !fftProcessor.smoothedSpectrum) {
        debugLog("FreqScanner: FFT buffer allocation failed");
        return false;
    }
    
    // Initialize buffers
    for (uint16_t i = 0; i < config.fftSize; i++) {
        fftProcessor.inputBuffer[i] = 0.0;
        fftProcessor.windowBuffer[i] = 0.0;
        fftProcessor.fftBuffer[i] = std::complex<float>(0.0, 0.0);
    }
    
    for (uint16_t i = 0; i < config.fftSize / 2; i++) {
        fftProcessor.magnitudeSpectrum[i] = -120.0;
        fftProcessor.phaseSpectrum[i] = 0.0;
        fftProcessor.smoothedSpectrum[i] = -120.0;
    }
    
    // Set FFT parameters
    fftProcessor.size = config.fftSize;
    fftProcessor.sampleRate = config.sampleRate;
    fftProcessor.windowType = config.windowType;
    fftProcessor.binWidth = (float)config.sampleRate / config.fftSize;
    
    // Generate window function
    generateWindow(config.windowType);
    
    fftProcessor.isInitialized = true;
    debugLog("FreqScanner: FFT processor initialized");
    return true;
}

void FreqScanner::shutdownFFT() {
    if (!fftProcessor.isInitialized) return;
    
    debugLog("FreqScanner: Shutting down FFT processor");
    
    // Free FFT buffers
    if (fftProcessor.inputBuffer) {
        delete[] fftProcessor.inputBuffer;
        fftProcessor.inputBuffer = nullptr;
    }
    
    if (fftProcessor.windowBuffer) {
        delete[] fftProcessor.windowBuffer;
        fftProcessor.windowBuffer = nullptr;
    }
    
    if (fftProcessor.fftBuffer) {
        delete[] fftProcessor.fftBuffer;
        fftProcessor.fftBuffer = nullptr;
    }
    
    if (fftProcessor.magnitudeSpectrum) {
        delete[] fftProcessor.magnitudeSpectrum;
        fftProcessor.magnitudeSpectrum = nullptr;
    }
    
    if (fftProcessor.phaseSpectrum) {
        delete[] fftProcessor.phaseSpectrum;
        fftProcessor.phaseSpectrum = nullptr;
    }
    
    if (fftProcessor.smoothedSpectrum) {
        delete[] fftProcessor.smoothedSpectrum;
        fftProcessor.smoothedSpectrum = nullptr;
    }
    
    fftProcessor.isInitialized = false;
}

bool FreqScanner::processFFT() {
    if (!fftProcessor.isInitialized || isProcessing) return false;
    
    isProcessing = true;
    unsigned long startTime = micros();
    
    // Sample ADC data
    sampleADC();
    
    // Apply window function
    applyWindow();
    
    // Compute FFT
    computeFFT();
    
    // Compute magnitude and phase spectra
    computeMagnitudeSpectrum();
    computePhaseSpectrum();
    
    // Apply smoothing
    smoothSpectrum();
    
    // Detect peaks
    if (config.enablePeakDetection) {
        detectPeaks();
    }
    
    // Estimate noise floor
    estimateNoiseFloor();
    
    // Update statistics
    unsigned long processingTime = micros() - startTime;
    stats.totalProcessingTime += processingTime / 1000; // Convert to milliseconds
    stats.fftProcessedCount++;
    
    isProcessing = false;
    return true;
}

void FreqScanner::sampleADC() {
    // Sample analog input (using entropy pin as signal source)
    for (uint16_t i = 0; i < fftProcessor.size; i++) {
        // Read ADC value and convert to voltage
        uint16_t adcValue = analogRead(ENTROPY_PIN_1);
        float voltage = (adcValue / 4095.0) * 3.3; // ESP32 12-bit ADC, 3.3V reference
        
        // Convert to centered signal (-1.65 to +1.65V)
        fftProcessor.inputBuffer[i] = voltage - 1.65;
        
        // Add small delay for consistent sampling rate
        delayMicroseconds(1000000 / config.sampleRate);
    }
}

void FreqScanner::applyWindow() {
    for (uint16_t i = 0; i < fftProcessor.size; i++) {
        fftProcessor.inputBuffer[i] *= fftProcessor.windowBuffer[i];
    }
}

void FreqScanner::computeFFT() {
    // Copy windowed input to complex buffer
    for (uint16_t i = 0; i < fftProcessor.size; i++) {
        fftProcessor.fftBuffer[i] = std::complex<float>(fftProcessor.inputBuffer[i], 0.0);
    }
    
    // Bit-reversal reordering
    for (uint16_t i = 0; i < fftProcessor.size; i++) {
        uint16_t j = 0;
        uint16_t temp = i;
        uint16_t bits = log2(fftProcessor.size);
        
        for (uint16_t k = 0; k < bits; k++) {
            j = (j << 1) | (temp & 1);
            temp >>= 1;
        }
        
        if (i < j) {
            std::swap(fftProcessor.fftBuffer[i], fftProcessor.fftBuffer[j]);
        }
    }
    
    // Cooley-Tukey FFT algorithm
    for (uint16_t length = 2; length <= fftProcessor.size; length <<= 1) {
        float angle = -2.0 * M_PI / length;
        std::complex<float> wlen(cos(angle), sin(angle));
        
        for (uint16_t i = 0; i < fftProcessor.size; i += length) {
            std::complex<float> w(1.0, 0.0);
            
            for (uint16_t j = 0; j < length / 2; j++) {
                std::complex<float> u = fftProcessor.fftBuffer[i + j];
                std::complex<float> v = fftProcessor.fftBuffer[i + j + length / 2] * w;
                
                fftProcessor.fftBuffer[i + j] = u + v;
                fftProcessor.fftBuffer[i + j + length / 2] = u - v;
                
                w *= wlen;
            }
        }
    }
}

void FreqScanner::computeMagnitudeSpectrum() {
    for (uint16_t i = 0; i < fftProcessor.size / 2; i++) {
        float real = fftProcessor.fftBuffer[i].real();
        float imag = fftProcessor.fftBuffer[i].imag();
        float magnitude = sqrt(real * real + imag * imag);
        
        // Convert to dB
        if (magnitude > 0.0) {
            fftProcessor.magnitudeSpectrum[i] = 20.0 * log10(magnitude);
        } else {
            fftProcessor.magnitudeSpectrum[i] = -120.0; // Minimum dB value
        }
    }
}

void FreqScanner::computePhaseSpectrum() {
    for (uint16_t i = 0; i < fftProcessor.size / 2; i++) {
        float real = fftProcessor.fftBuffer[i].real();
        float imag = fftProcessor.fftBuffer[i].imag();
        fftProcessor.phaseSpectrum[i] = atan2(imag, real);
    }
}

void FreqScanner::smoothSpectrum() {
    if (!config.enableAveraging) {
        // Copy magnitude spectrum to smoothed spectrum
        for (uint16_t i = 0; i < fftProcessor.size / 2; i++) {
            fftProcessor.smoothedSpectrum[i] = fftProcessor.magnitudeSpectrum[i];
        }
        return;
    }
    
    // Apply exponential moving average
    float alpha = config.smoothingFactor;
    for (uint16_t i = 0; i < fftProcessor.size / 2; i++) {
        fftProcessor.smoothedSpectrum[i] = alpha * fftProcessor.magnitudeSpectrum[i] + 
                                         (1.0 - alpha) * fftProcessor.smoothedSpectrum[i];
    }
}

void FreqScanner::estimateNoiseFloor() {
    // Calculate noise floor as median of lower 25% of spectrum
    std::vector<float> sortedMagnitudes;
    
    for (uint16_t i = 1; i < fftProcessor.size / 2 - 1; i++) { // Skip DC and Nyquist
        sortedMagnitudes.push_back(fftProcessor.smoothedSpectrum[i]);
    }
    
    std::sort(sortedMagnitudes.begin(), sortedMagnitudes.end());
    
    uint16_t medianIndex = sortedMagnitudes.size() / 4; // 25th percentile
    noiseFloor = sortedMagnitudes[medianIndex];
    
    // Update statistics
    stats.averageNoiseFloor = 0.9 * stats.averageNoiseFloor + 0.1 * noiseFloor;
}

// ===== WINDOW FUNCTION IMPLEMENTATION =====

void FreqScanner::generateWindow(WindowType type) {
    switch (type) {
        case WINDOW_RECTANGULAR:
            for (uint16_t i = 0; i < fftProcessor.size; i++) {
                fftProcessor.windowBuffer[i] = 1.0;
            }
            break;
            
        case WINDOW_HAMMING:
            for (uint16_t i = 0; i < fftProcessor.size; i++) {
                fftProcessor.windowBuffer[i] = hammingWindow(i, fftProcessor.size);
            }
            break;
            
        case WINDOW_BLACKMAN:
            for (uint16_t i = 0; i < fftProcessor.size; i++) {
                fftProcessor.windowBuffer[i] = blackmanWindow(i, fftProcessor.size);
            }
            break;
            
        case WINDOW_HANNING:
            for (uint16_t i = 0; i < fftProcessor.size; i++) {
                fftProcessor.windowBuffer[i] = hanningWindow(i, fftProcessor.size);
            }
            break;
            
        case WINDOW_KAISER:
            for (uint16_t i = 0; i < fftProcessor.size; i++) {
                fftProcessor.windowBuffer[i] = kaiserWindow(i, fftProcessor.size, 8.6);
            }
            break;
    }
}

float FreqScanner::hammingWindow(int n, int N) {
    return 0.54 - 0.46 * cos(2.0 * M_PI * n / (N - 1));
}

float FreqScanner::blackmanWindow(int n, int N) {
    return 0.42 - 0.5 * cos(2.0 * M_PI * n / (N - 1)) + 
           0.08 * cos(4.0 * M_PI * n / (N - 1));
}

float FreqScanner::hanningWindow(int n, int N) {
    return 0.5 - 0.5 * cos(2.0 * M_PI * n / (N - 1));
}

float FreqScanner::kaiserWindow(int n, int N, float beta) {
    float arg = beta * sqrt(1.0 - pow(2.0 * n / (N - 1) - 1.0, 2));
    // Simplified Bessel function approximation
    float i0_beta = 1.0 + pow(arg / 2.0, 2) + pow(arg / 4.0, 4) / 9.0;
    float i0_beta_0 = 1.0 + pow(beta / 2.0, 2) + pow(beta / 4.0, 4) / 9.0;
    return i0_beta / i0_beta_0;
}

// ===== PEAK DETECTION IMPLEMENTATION =====

void FreqScanner::detectPeaks() {
    detectedPeaks.clear();
    
    // Find local maxima above threshold
    for (uint16_t i = 2; i < fftProcessor.size / 2 - 2; i++) {
        if (isPeak(i) && fftProcessor.smoothedSpectrum[i] > config.peakThreshold) {
            SpectralPeak peak;
            peak.binIndex = i;
            peak.frequency = interpolatePeakFrequency(i);
            peak.magnitude = fftProcessor.smoothedSpectrum[i];
            peak.phase = fftProcessor.phaseSpectrum[i];
            peak.isValid = true;
            peak.timestamp = millis();
            
            detectedPeaks.push_back(peak);
        }
    }
    
    // Sort peaks by magnitude and keep only the strongest ones
    sortPeaksByMagnitude();
    
    if (detectedPeaks.size() > config.maxPeaks) {
        detectedPeaks.resize(config.maxPeaks);
    }
    
    // Update statistics
    stats.peaksDetected += detectedPeaks.size();
    
    // Update peak signal level
    if (!detectedPeaks.empty()) {
        float maxMagnitude = detectedPeaks[0].magnitude;
        if (maxMagnitude > stats.peakSignalLevel) {
            stats.peakSignalLevel = maxMagnitude;
        }
    }
}

bool FreqScanner::isPeak(uint16_t binIndex) {
    float current = fftProcessor.smoothedSpectrum[binIndex];
    float prev1 = fftProcessor.smoothedSpectrum[binIndex - 1];
    float prev2 = fftProcessor.smoothedSpectrum[binIndex - 2];
    float next1 = fftProcessor.smoothedSpectrum[binIndex + 1];
    float next2 = fftProcessor.smoothedSpectrum[binIndex + 2];
    
    // Peak must be higher than surrounding bins and significantly above noise floor
    return (current > prev1 && current > prev2 && 
            current > next1 && current > next2 &&
            current > noiseFloor + 6.0); // 6dB above noise floor
}

float FreqScanner::interpolatePeakFrequency(uint16_t binIndex) {
    // Parabolic interpolation for more accurate frequency estimation
    float y1 = fftProcessor.smoothedSpectrum[binIndex - 1];
    float y2 = fftProcessor.smoothedSpectrum[binIndex];
    float y3 = fftProcessor.smoothedSpectrum[binIndex + 1];
    
    float a = (y1 - 2*y2 + y3) / 2.0;
    float b = (y3 - y1) / 2.0;
    
    float peakOffset = 0.0;
    if (a != 0.0) {
        peakOffset = -b / (2.0 * a);
    }
    
    float interpolatedBin = binIndex + peakOffset;
    return interpolatedBin * fftProcessor.binWidth;
}

void FreqScanner::sortPeaksByMagnitude() {
    std::sort(detectedPeaks.begin(), detectedPeaks.end(),
              [](const SpectralPeak& a, const SpectralPeak& b) {
                  return a.magnitude > b.magnitude;
              });
}

// ===== WATERFALL DISPLAY IMPLEMENTATION =====

bool FreqScanner::initializeWaterfall() {
    debugLog("FreqScanner: Initializing waterfall display");
    
    // Allocate waterfall history buffer
    waterfallDisplay.historyBuffer = new uint16_t*[waterfallDisplay.historyDepth];
    if (!waterfallDisplay.historyBuffer) {
        debugLog("FreqScanner: Failed to allocate waterfall history buffer");
        return false;
    }
    
    for (uint16_t i = 0; i < waterfallDisplay.historyDepth; i++) {
        waterfallDisplay.historyBuffer[i] = new uint16_t[waterfallDisplay.width];
        if (!waterfallDisplay.historyBuffer[i]) {
            debugLog("FreqScanner: Failed to allocate waterfall line buffer");
            return false;
        }
        
        // Initialize with minimum intensity
        for (uint16_t j = 0; j < waterfallDisplay.width; j++) {
            waterfallDisplay.historyBuffer[i][j] = 0;
        }
    }
    
    // Allocate color palette
    waterfallDisplay.colorPalette = new uint16_t[waterfallDisplay.paletteSize];
    if (!waterfallDisplay.colorPalette) {
        debugLog("FreqScanner: Failed to allocate color palette");
        return false;
    }
    
    // Generate color palette
    generateColorPalette();
    
    debugLog("FreqScanner: Waterfall display initialized");
    return true;
}

void FreqScanner::shutdownWaterfall() {
    debugLog("FreqScanner: Shutting down waterfall display");
    
    // Free waterfall history buffer
    if (waterfallDisplay.historyBuffer) {
        for (uint16_t i = 0; i < waterfallDisplay.historyDepth; i++) {
            if (waterfallDisplay.historyBuffer[i]) {
                delete[] waterfallDisplay.historyBuffer[i];
            }
        }
        delete[] waterfallDisplay.historyBuffer;
        waterfallDisplay.historyBuffer = nullptr;
    }
    
    // Free color palette
    if (waterfallDisplay.colorPalette) {
        delete[] waterfallDisplay.colorPalette;
        waterfallDisplay.colorPalette = nullptr;
    }
}

void FreqScanner::updateWaterfall() {
    if (!waterfallDisplay.historyBuffer || !fftProcessor.isInitialized) return;
    
    // Map spectrum data to waterfall pixels
    uint16_t* currentLine = waterfallDisplay.historyBuffer[waterfallDisplay.currentLine];
    
    for (uint16_t x = 0; x < waterfallDisplay.width; x++) {
        // Map pixel to frequency bin
        uint16_t binIndex = (x * fftProcessor.size / 2) / waterfallDisplay.width;
        if (binIndex >= fftProcessor.size / 2) binIndex = fftProcessor.size / 2 - 1;
        
        // Get magnitude and convert to color index
        float magnitude = fftProcessor.smoothedSpectrum[binIndex];
        currentLine[x] = intensityToColor(magnitude);
    }
    
    // Advance to next line
    waterfallDisplay.currentLine = (waterfallDisplay.currentLine + 1) % waterfallDisplay.historyDepth;
}

void FreqScanner::generateColorPalette() {
    // Generate blue-to-red intensity color palette
    for (uint8_t i = 0; i < waterfallDisplay.paletteSize; i++) {
        float intensity = (float)i / (waterfallDisplay.paletteSize - 1);
        
        uint8_t r, g, b;
        
        if (intensity < 0.25) {
            // Black to blue
            r = 0;
            g = 0;
            b = (uint8_t)(intensity * 4 * 255);
        } else if (intensity < 0.5) {
            // Blue to cyan
            r = 0;
            g = (uint8_t)((intensity - 0.25) * 4 * 255);
            b = 255;
        } else if (intensity < 0.75) {
            // Cyan to yellow
            r = (uint8_t)((intensity - 0.5) * 4 * 255);
            g = 255;
            b = (uint8_t)(255 - (intensity - 0.5) * 4 * 255);
        } else {
            // Yellow to red
            r = 255;
            g = (uint8_t)(255 - (intensity - 0.75) * 4 * 255);
            b = 0;
        }
        
        // Convert to RGB565
        waterfallDisplay.colorPalette[i] = displayManager.rgb565(r, g, b);
    }
}

uint16_t FreqScanner::intensityToColor(float intensity) {
    // Map intensity range to palette index
    float normalizedIntensity = (intensity - waterfallDisplay.intensityMin) / 
                               (waterfallDisplay.intensityMax - waterfallDisplay.intensityMin);
    
    // Clamp to valid range
    if (normalizedIntensity < 0.0) normalizedIntensity = 0.0;
    if (normalizedIntensity > 1.0) normalizedIntensity = 1.0;
    
    uint8_t paletteIndex = (uint8_t)(normalizedIntensity * (waterfallDisplay.paletteSize - 1));
    return waterfallDisplay.colorPalette[paletteIndex];
}

// ===== SIGNAL GENERATOR IMPLEMENTATION =====

bool FreqScanner::initializeGenerator() {
    debugLog("FreqScanner: Initializing signal generator");
    
    // Configure DAC pin
    signalGenerator.dacPin = DAC_OUT_LEFT;
    
    // Calculate phase increment
    signalGenerator.phaseIncrement = 2.0 * M_PI * signalGenerator.frequency / signalGenerator.sampleRate;
    
    debugLog("FreqScanner: Signal generator initialized");
    return true;
}

void FreqScanner::shutdownGenerator() {
    debugLog("FreqScanner: Shutting down signal generator");
    
    // Stop generation
    signalGenerator.isEnabled = false;
    
    // Set DAC to zero
    if (signalGenerator.useDac) {
        dacWrite(signalGenerator.dacPin, 0);
    }
}

void FreqScanner::updateGenerator() {
    if (!signalGenerator.isEnabled) return;
    
    // Generate sample
    float sample = generateSample();
    
    // Apply modulation
    applyModulation(sample);
    
    // Apply amplitude scaling
    sample *= signalGenerator.amplitude;
    
    // Output to DAC
    outputToDAC(sample);
    
    // Update phase
    signalGenerator.phase += signalGenerator.phaseIncrement;
    if (signalGenerator.phase >= 2.0 * M_PI) {
        signalGenerator.phase -= 2.0 * M_PI;
    }
}

float FreqScanner::generateSample() {
    switch (signalGenerator.waveform) {
        case SignalGenerator::WAVE_SINE:
            return generateSineWave();
            
        case SignalGenerator::WAVE_SQUARE:
            return generateSquareWave();
            
        case SignalGenerator::WAVE_TRIANGLE:
            return generateTriangleWave();
            
        case SignalGenerator::WAVE_SAWTOOTH:
            return generateSawtoothWave();
            
        case SignalGenerator::WAVE_NOISE:
            return generateNoise();
            
        case SignalGenerator::WAVE_SWEEP:
            return generateSweep();
            
        default:
            return 0.0;
    }
}

float FreqScanner::generateSineWave() {
    return sin(signalGenerator.phase);
}

float FreqScanner::generateSquareWave() {
    return (signalGenerator.phase < M_PI) ? 1.0 : -1.0;
}

float FreqScanner::generateTriangleWave() {
    if (signalGenerator.phase < M_PI) {
        return -1.0 + 2.0 * signalGenerator.phase / M_PI;
    } else {
        return 3.0 - 2.0 * signalGenerator.phase / M_PI;
    }
}

float FreqScanner::generateSawtoothWave() {
    return -1.0 + signalGenerator.phase / M_PI;
}

float FreqScanner::generateNoise() {
    return 2.0 * ((float)random(0, 32767) / 32767.0) - 1.0;
}

float FreqScanner::generateSweep() {
    // Linear frequency sweep
    unsigned long currentTime = millis();
    float sweepProgress = fmod((float)(currentTime) / 1000.0, signalGenerator.sweepDuration) / 
                         signalGenerator.sweepDuration;
    
    float currentFreq = signalGenerator.sweepStartFreq + 
                       sweepProgress * (signalGenerator.sweepEndFreq - signalGenerator.sweepStartFreq);
    
    float instantPhaseIncrement = 2.0 * M_PI * currentFreq / signalGenerator.sampleRate;
    return sin(signalGenerator.phase);
}

void FreqScanner::applyModulation(float& sample) {
    if (signalGenerator.modulation == SignalGenerator::MOD_NONE) return;
    
    float modPhase = 2.0 * M_PI * signalGenerator.modFrequency * millis() / 1000.0;
    float modSignal = sin(modPhase);
    
    switch (signalGenerator.modulation) {
        case SignalGenerator::MOD_AM:
            sample *= (1.0 + signalGenerator.modDepth * modSignal);
            break;
            
        case SignalGenerator::MOD_FM:
            // Frequency modulation affects phase increment
            signalGenerator.phaseIncrement = 2.0 * M_PI * 
                (signalGenerator.frequency + signalGenerator.modDepth * signalGenerator.frequency * modSignal) /
                signalGenerator.sampleRate;
            break;
            
        default:
            break;
    }
}

void FreqScanner::outputToDAC(float sample) {
    if (signalGenerator.useDac) {
        // Convert to DAC range (0-255 for 8-bit DAC)
        uint8_t dacValue = (uint8_t)((sample + 1.0) * 127.5);
        dacWrite(signalGenerator.dacPin, dacValue);
    }
}

// ===== TOUCH HANDLING IMPLEMENTATION =====

TouchZone FreqScanner::identifyTouchZone(TouchPoint touch) {
    // Back button (top-left)
    if (touch.x < 40 && touch.y < 20) {
        return ZONE_BACK_BUTTON;
    }
    
    // Control buttons (top-right)
    if (touch.y < 20) {
        if (touch.x > 280) return ZONE_SETTINGS_BUTTON;
        if (touch.x > 240) return ZONE_GENERATOR_BUTTON;
        if (touch.x > 200) return ZONE_RECORD_BUTTON;
        if (touch.x > 160) return ZONE_VIEW_TOGGLE;
    }
    
    // Spectrum area
    if (touch.y >= SPECTRUM_AREA_Y && touch.y < SPECTRUM_AREA_Y + SPECTRUM_AREA_H) {
        return ZONE_SPECTRUM_AREA;
    }
    
    // Waterfall area
    if (touch.y >= WATERFALL_AREA_Y && touch.y < WATERFALL_AREA_Y + WATERFALL_AREA_H) {
        return ZONE_WATERFALL_AREA;
    }
    
    // Control panel area
    if (touch.y >= 220) {
        return ZONE_CONTROL_PANEL;
    }
    
    return ZONE_NONE;
}

void FreqScanner::handleSpectrumTouch(TouchPoint touch) {
    // Convert touch coordinates to frequency
    float frequency = pixelToFrequency(touch.x);
    
    // Find nearest peak
    selectPeakNearTouch(touch);
    
    // Update measurement cursor
    if (uiState.measurementMode) {
        updateMeasurementCursor(touch);
    }
    
    // Add frequency marker on double-tap
    // (This would need gesture detection from TouchInterface)
    
    needsRedraw = true;
}

void FreqScanner::selectPeakNearTouch(TouchPoint touch) {
    float touchFreq = pixelToFrequency(touch.x);
    float minDistance = fftProcessor.binWidth * 5; // 5 bins tolerance
    int nearestPeak = -1;
    
    for (size_t i = 0; i < detectedPeaks.size(); i++) {
        float distance = abs(detectedPeaks[i].frequency - touchFreq);
        if (distance < minDistance) {
            minDistance = distance;
            nearestPeak = i;
        }
    }
    
    uiState.selectedPeak = nearestPeak;
}

// ===== RENDERING IMPLEMENTATION =====

void FreqScanner::renderSpectrum() {
    if (!fftProcessor.isInitialized) return;
    
    // Draw spectrum background
    displayManager.drawRetroRect(SPECTRUM_AREA_X, SPECTRUM_AREA_Y, 
                                SPECTRUM_AREA_W, SPECTRUM_AREA_H, 
                                colorBackground, true);
    
    // Draw grid
    if (uiState.showGrid) {
        renderGrid();
    }
    
    // Draw frequency axis
    renderFrequencyAxis();
    
    // Draw amplitude axis
    renderAmplitudeAxis();
    
    // Draw spectrum data
    for (uint16_t x = 1; x < SPECTRUM_AREA_W - 1; x++) {
        uint16_t binIndex = (x * fftProcessor.size / 2) / SPECTRUM_AREA_W;
        if (binIndex >= fftProcessor.size / 2) continue;
        
        float magnitude = fftProcessor.smoothedSpectrum[binIndex];
        drawSpectrumLine(x, magnitude);
    }
    
    // Draw peaks
    if (config.enablePeakDetection) {
        renderPeaks();
    }
    
    // Draw markers
    if (uiState.showMarkers) {
        renderMarkers();
    }
    
    // Draw measurement cursor
    if (uiState.measurementMode) {
        renderMeasurementCursor();
    }
}

void FreqScanner::renderGrid() {
    // Vertical grid lines (frequency)
    for (uint16_t x = SPECTRUM_AREA_X; x < SPECTRUM_AREA_X + SPECTRUM_AREA_W; x += GRID_SPACING) {
        displayManager.drawRetroLine(x, SPECTRUM_AREA_Y, x, SPECTRUM_AREA_Y + SPECTRUM_AREA_H, colorGrid);
    }
    
    // Horizontal grid lines (amplitude)
    for (uint16_t y = SPECTRUM_AREA_Y; y < SPECTRUM_AREA_Y + SPECTRUM_AREA_H; y += GRID_SPACING) {
        displayManager.drawRetroLine(SPECTRUM_AREA_X, y, SPECTRUM_AREA_X + SPECTRUM_AREA_W, y, colorGrid);
    }
}

void FreqScanner::renderFrequencyAxis() {
    // Draw frequency labels
    uint16_t freqMin = getFrequencyRangeMin();
    uint16_t freqMax = getFrequencyRangeMax();
    
    for (uint16_t i = 0; i <= 4; i++) {
        uint16_t x = SPECTRUM_AREA_X + (i * SPECTRUM_AREA_W) / 4;
        float frequency = freqMin + (i * (freqMax - freqMin)) / 4;
        
        String freqLabel = formatFrequency(frequency);
        displayManager.setFont(FONT_SMALL);
        displayManager.drawText(x - 15, SPECTRUM_AREA_Y + SPECTRUM_AREA_H + 5, freqLabel, colorText);
    }
}

void FreqScanner::renderAmplitudeAxis() {
    // Draw amplitude labels
    for (uint16_t i = 0; i <= 4; i++) {
        uint16_t y = SPECTRUM_AREA_Y + (i * SPECTRUM_AREA_H) / 4;
        float amplitude = -20.0 - (i * 20.0); // -20dB to -100dB range
        
        String ampLabel = formatAmplitude(amplitude);
        displayManager.setFont(FONT_SMALL);
        displayManager.drawText(5, y - 4, ampLabel, colorText);
    }
}

void FreqScanner::drawSpectrumLine(uint16_t x, float magnitude) {
    uint16_t y = amplitudeToPixel(magnitude);
    
    // Draw line from bottom to magnitude
    displayManager.drawRetroLine(x, SPECTRUM_AREA_Y + SPECTRUM_AREA_H, x, y, colorSpectrum);
}

void FreqScanner::renderPeaks() {
    for (const auto& peak : detectedPeaks) {
        drawPeakMarker(peak);
    }
}

void FreqScanner::drawPeakMarker(const SpectralPeak& peak) {
    uint16_t x = frequencyToPixel(peak.frequency);
    uint16_t y = amplitudeToPixel(peak.magnitude);
    
    // Draw peak symbol
    displayManager.drawRetroCircle(x, y, PEAK_MARKER_SIZE / 2, colorPeaks, false);
    
    // Draw frequency label if enabled
    if (uiState.showPeakLabels) {
        String freqLabel = formatFrequency(peak.frequency);
        displayManager.setFont(FONT_SMALL);
        displayManager.drawText(x - 15, y - 15, freqLabel, colorPeaks);
    }
}

// ===== UTILITY IMPLEMENTATION =====

String FreqScanner::formatFrequency(float frequency) {
    if (frequency >= 1000000) {
        return String(frequency / 1000000, 1) + "MHz";
    } else if (frequency >= 1000) {
        return String(frequency / 1000, 1) + "kHz";
    } else {
        return String(frequency, 0) + "Hz";
    }
}

String FreqScanner::formatAmplitude(float amplitude) {
    return String(amplitude, 0) + "dB";
}

uint16_t FreqScanner::frequencyToPixel(float frequency) {
    uint16_t freqMin = getFrequencyRangeMin();
    uint16_t freqMax = getFrequencyRangeMax();
    
    float normalizedFreq = (frequency - freqMin) / (freqMax - freqMin);
    return SPECTRUM_AREA_X + (uint16_t)(normalizedFreq * SPECTRUM_AREA_W);
}

uint16_t FreqScanner::amplitudeToPixel(float amplitude) {
    // Map -100dB to -20dB range to pixel coordinates
    float normalizedAmp = (amplitude + 100.0) / 80.0; // -100 to -20 dB range
    if (normalizedAmp < 0.0) normalizedAmp = 0.0;
    if (normalizedAmp > 1.0) normalizedAmp = 1.0;
    
    return SPECTRUM_AREA_Y + SPECTRUM_AREA_H - (uint16_t)(normalizedAmp * SPECTRUM_AREA_H);
}

float FreqScanner::pixelToFrequency(uint16_t pixel) {
    uint16_t freqMin = getFrequencyRangeMin();
    uint16_t freqMax = getFrequencyRangeMax();
    
    float normalizedPixel = (float)(pixel - SPECTRUM_AREA_X) / SPECTRUM_AREA_W;
    return freqMin + normalizedPixel * (freqMax - freqMin);
}

uint16_t FreqScanner::getFrequencyRangeMin() {
    switch (config.freqRange) {
        case RANGE_AUDIO_LOW: return 20;
        case RANGE_AUDIO_MID: return 200;
        case RANGE_AUDIO_FULL: return 20;
        case RANGE_RF_LOW: return 1000000;
        case RANGE_RF_HIGH: return 30000000;
        case RANGE_CUSTOM: return config.customFreqMin;
        default: return 20;
    }
}

uint16_t FreqScanner::getFrequencyRangeMax() {
    switch (config.freqRange) {
        case RANGE_AUDIO_LOW: return 2000;
        case RANGE_AUDIO_MID: return 8000;
        case RANGE_AUDIO_FULL: return 20000;
        case RANGE_RF_LOW: return 30000000;
        case RANGE_RF_HIGH: return 300000000;
        case RANGE_CUSTOM: return config.customFreqMax;
        default: return 20000;
    }
}

// ===== PLACEHOLDER IMPLEMENTATIONS =====
// (Additional methods would be implemented similarly)

void FreqScanner::renderWaterfall() {
    // Waterfall rendering implementation
    needsRedraw = true;
}

void FreqScanner::renderDualView() {
    // Dual view rendering implementation  
    needsRedraw = true;
}

void FreqScanner::renderRecordingInterface() {
    // Recording interface implementation
    needsRedraw = true;
}

void FreqScanner::renderGeneratorInterface() {
    // Generator interface implementation
    needsRedraw = true;
}

void FreqScanner::renderSettingsPanel() {
    // Settings panel implementation
    needsRedraw = true;
}

void FreqScanner::renderStatusBar() {
    // Status bar implementation
    String status = "FFT: " + String(config.fftSize) + " | " + 
                   formatFrequency(config.sampleRate / 2) + " | " +
                   String(stats.fftProcessedCount) + " processed";
    
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(5, 5, status, colorText);
}

void FreqScanner::renderMarkers() {
    for (uint8_t i = 0; i < 2; i++) {
        if (markers[i].isEnabled) {
            drawFrequencyMarker(markers[i]);
        }  
    }
}

void FreqScanner::renderMeasurementCursor() {
    // Measurement cursor implementation
}

void FreqScanner::drawFrequencyMarker(const FrequencyMarker& marker) {
    uint16_t x = frequencyToPixel(marker.frequency);
    
    // Draw vertical line
    displayManager.drawRetroLine(x, SPECTRUM_AREA_Y, x, SPECTRUM_AREA_Y + SPECTRUM_AREA_H, marker.color);
    
    // Draw label
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(x + 2, SPECTRUM_AREA_Y + 10, marker.label, marker.color);
}

// Additional placeholder methods for configuration, recording, etc.
void FreqScanner::loadConfiguration() { /* Implementation */ }
void FreqScanner::saveConfiguration() { /* Implementation */ }
void FreqScanner::resetConfiguration() { /* Implementation */ }
void FreqScanner::applyConfiguration() { /* Implementation */ }
void FreqScanner::updateStatistics() { /* Implementation */ }
void FreqScanner::resetStatistics() { /* Implementation */ }

bool FreqScanner::startRecording(const String& filename) { return false; }
void FreqScanner::stopRecording() { /* Implementation */ }
void FreqScanner::toggleRecording() { /* Implementation */ }
void FreqScanner::toggleGenerator() { /* Implementation */ }

void FreqScanner::handleWaterfallTouch(TouchPoint touch) { /* Implementation */ }
void FreqScanner::handleControlPanelTouch(TouchPoint touch) { /* Implementation */ }
void FreqScanner::updateMeasurementCursor(TouchPoint touch) { /* Implementation */ }

String FreqScanner::getSettingName(uint8_t index) const { return "Setting " + String(index); }
void FreqScanner::handleSetting(uint8_t index) { /* Implementation */ }

// BaseApp overrides
void FreqScanner::onPause() { /* Implementation */ }
void FreqScanner::onResume() { /* Implementation */ }
bool FreqScanner::saveState() { return true; }
bool FreqScanner::loadState() { return true; }
bool FreqScanner::handleMessage(AppMessage message, void* data) { return false; }

String FreqScanner::generateRecordingFilename() { return "/recordings/rec_" + String(millis()) + ".dat"; }