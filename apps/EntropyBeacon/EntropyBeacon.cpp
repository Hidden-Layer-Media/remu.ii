#include "EntropyBeacon.h"
#include "../../core/SystemCore/SystemCore.h"
#include "../../core/Config/hardware_pins.h"
#include <math.h>

// Use pin definitions from hardware_pins.h
// ENTROPY_PIN_1 = 36, ENTROPY_PIN_2 = 39 (ADC1, always available)
#define DAC_OUT_PIN 26  // Use GPIO26 (DAC1) - I2S_WS_PIN is only active when I2S is running

// ========================================
// ICON DATA
// ========================================

const uint8_t EntropyBeaconApp::ENTROPY_ICON[32] = {
    0x00, 0x00, 0x18, 0x18, 0x3C, 0x3C, 0x7E, 0x7E, 0xFF, 0xFF, 0x7E, 0x7E,
    0x3C, 0x3C, 0x18, 0x18, 0x81, 0x81, 0xC3, 0xC3, 0x66, 0x66, 0x3C, 0x3C,
    0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// ========================================
// CONSTRUCTOR / DESTRUCTOR
// ========================================

EntropyBeaconApp::EntropyBeaconApp() :
    bufferIndex(0),
    lastSampleTime(0),
    sampleInterval(1000), // 1ms default (1kHz)
    dacEnabled(false)
{
    setMetadata("EntropyBeacon", "1.0", "remu.ii", "Real-time entropy visualization", CATEGORY_TOOLS, 20000);
    setRequirements(false, false, false);
    
    backgroundColor = COLOR_BLACK;
    foregroundColor = COLOR_GREEN_PHOS;
    
    showBackButton = true;
    showStatusBar = true;
    
    // Initialize visualization settings
    viz.mode = VIZ_OSCILLOSCOPE;
    viz.sampleRate = RATE_1KHZ;
    viz.amplitudeScale = 1.0f;
    viz.traceColor = COLOR_GREEN_PHOS;
    
    // Clear buffers
    memset(entropyBuffer, 0, sizeof(entropyBuffer));
    memset(spectrumData, 0, sizeof(spectrumData));
    
    // Initialize anomaly detector
    anomalyDetector.mean = 0.5f;
    anomalyDetector.variance = 0.1f;
    anomalyDetector.threshold = 2.0f;
    anomalyDetector.anomalyCount = 0;
}

EntropyBeaconApp::~EntropyBeaconApp() {
    cleanup();
}

// ========================================
// MANDATORY BASEAPP METHODS
// ========================================

bool EntropyBeaconApp::initialize() {
    debugLog("EntropyBeacon initializing...");
    
    // Create app data directory on SD card
    String appDir = "/sd/apps/entropybeacon";
    if (!SD.exists("/sd/apps")) {
        SD.mkdir("/sd/apps");
    }
    SD.mkdir(appDir.c_str());
    
    // Initialize recording paths
    recordingPath = "/sd/apps/entropybeacon/entropy_data.csv";
    configPath = "/sd/apps/entropybeacon/config.json";
    logPath = "/sd/apps/entropybeacon/system.log";
    
    // Initialize DAC
    pinMode(DAC_OUT_PIN, OUTPUT);
    dacWrite(DAC_OUT_PIN, 0);
    
    // Calculate sample interval
    calculateSampleInterval();
    
    // Load saved configuration
    loadConfiguration();
    
    debugLog("EntropyBeacon initialized successfully");
    return true;
}

void EntropyBeaconApp::update() {
    if (currentState != APP_RUNNING) return;
    
    unsigned long currentTime = micros();
    
    // Sample entropy at specified rate
    if (currentTime - lastSampleTime >= sampleInterval) {
        sampleEntropy();
        lastSampleTime = currentTime;
    }
    
    // Update DAC output if enabled
    if (dacEnabled) {
        updateDACOutput();
    }
}

void EntropyBeaconApp::render() {
    if (currentState != APP_RUNNING) return;
    
    // Clear screen
    displayManager.clearScreen(backgroundColor);
    
    // Draw title
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawText(5, 5, "Entropy Beacon", COLOR_RED_GLOW);
    
    // Draw mode indicator
    String modeNames[] = {"OSC", "SPEC"};
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(150, 8, modeNames[viz.mode], COLOR_GREEN_PHOS);
    
    // Draw sample rate
    displayManager.drawText(220, 8, String(viz.sampleRate) + "Hz", COLOR_WHITE);
    
    // Draw visualization based on current mode
    switch (viz.mode) {
        case VIZ_OSCILLOSCOPE:
            drawOscilloscope();
            break;
        case VIZ_SPECTRUM:
            drawSpectrum();
            break;
    }
    
    // Draw controls
    drawControls();
    
    // Draw status info
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(5, 25, "Buffer: " + String(getBufferSize()) + "/" + String(ENTROPY_BUFFER_SIZE), COLOR_LIGHT_GRAY);
    
    if (getBufferSize() > 0) {
        float currentEntropy = getCurrentEntropy();
        displayManager.drawText(120, 25, "Val: " + String(currentEntropy, 3), COLOR_WHITE);
    }
    
    displayManager.drawText(220, 25, "Anom: " + String(anomalyDetector.anomalyCount), 
                           anomalyDetector.anomalyCount > 0 ? COLOR_RED_GLOW : COLOR_LIGHT_GRAY);
}

bool EntropyBeaconApp::handleTouch(TouchPoint touch) {
    if (!touch.isNewPress) return false;
    
    // Mode switching buttons (top row)
    if (touch.y < 30) {
        if (touch.x < 80) {
            viz.mode = VIZ_OSCILLOSCOPE;
        } else if (touch.x < 160) {
            viz.mode = VIZ_SPECTRUM;
        }
        return true;
    }
    
    // Settings area (bottom)
    if (touch.y > 210) {
        if (touch.x < 80) {
            // Decrease sample rate
            if (viz.sampleRate > RATE_100HZ) {
                viz.sampleRate = (SampleRate)(viz.sampleRate / 2);
                calculateSampleInterval();
            }
        } else if (touch.x < 160) {
            // Increase sample rate
            if (viz.sampleRate < RATE_8KHZ) {
                viz.sampleRate = (SampleRate)(viz.sampleRate * 2);
                calculateSampleInterval();
            }
        } else if (touch.x < 240) {
            // Toggle DAC
            dacEnabled = !dacEnabled;
        }
        return true;
    }
    
    return true;
}

void EntropyBeaconApp::cleanup() {
    // Turn off DAC
    dacWrite(DAC_OUT_PIN, 0);
    debugLog("EntropyBeacon cleanup complete");
}

const uint8_t* EntropyBeaconApp::getIcon() const {
    return ENTROPY_ICON;
}

// ========================================
// SAMPLING AND DATA PROCESSING
// ========================================

void EntropyBeaconApp::sampleEntropy() {
    // Read from multiple entropy sources
    uint16_t source1 = analogRead(ENTROPY_PIN_1);
    uint16_t source2 = analogRead(ENTROPY_PIN_2);
    uint16_t systemEntropy = systemCore.getEntropyPool() & 0xFFF;
    
    // Combine sources
    uint16_t entropyValue = source1 ^ (source2 << 4) ^ systemEntropy;
    float normalized = (float)entropyValue / 4095.0f;
    
    // Store in circular buffer
    entropyBuffer[bufferIndex] = entropyValue;
    bufferIndex = (bufferIndex + 1) % ENTROPY_BUFFER_SIZE;
    
    // Simple anomaly detection
    bool isAnomaly = detectAnomaly(normalized);
    
    // Update statistics
    updateStatistics(normalized);
    
    // Write to SD card if recording
    if (viz.recordingEnabled) {
        writeDataToSD(entropyValue, normalized, isAnomaly);
    }
    
    // Log significant events to SD card
    static uint32_t sampleCount = 0;
    sampleCount++;
    if (isAnomaly || sampleCount % 1000 == 0) {
        logEventToSD(isAnomaly ? "ANOMALY" : "CHECKPOINT", normalized);
    }
}

bool EntropyBeaconApp::detectAnomaly(float value) {
    float deviation = abs(value - anomalyDetector.mean);
    float stdDev = sqrt(anomalyDetector.variance);
    
    bool isAnomaly = deviation > (anomalyDetector.threshold * stdDev);
    if (isAnomaly) {
        anomalyDetector.anomalyCount++;
        debugLog("Anomaly detected: " + String(value, 4));
    }
    return isAnomaly;
}

void EntropyBeaconApp::updateStatistics(float value) {
    // Update running mean and variance
    float alpha = 0.01f; // Learning rate
    
    float delta = value - anomalyDetector.mean;
    anomalyDetector.mean += alpha * delta;
    anomalyDetector.variance += alpha * (delta * delta - anomalyDetector.variance);
}

void EntropyBeaconApp::calculateSampleInterval() {
    // Convert sample rate to microsecond interval
    sampleInterval = 1000000 / viz.sampleRate;
    debugLog("Sample interval set to: " + String(sampleInterval) + " us");
}

// ========================================
// VISUALIZATION METHODS
// ========================================

void EntropyBeaconApp::drawOscilloscope() {
    if (getBufferSize() < 2) return;
    
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(GRAPH_X, GRAPH_Y - 15, "Entropy Oscilloscope", COLOR_GREEN_PHOS);
    
    // Draw waveform
    for (int16_t x = 0; x < GRAPH_WIDTH - 1; x++) {
        uint16_t sampleIndex1 = (bufferIndex - GRAPH_WIDTH + x + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
        uint16_t sampleIndex2 = (bufferIndex - GRAPH_WIDTH + x + 1 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
        
        float value1 = (float)entropyBuffer[sampleIndex1] / 4095.0f;
        float value2 = (float)entropyBuffer[sampleIndex2] / 4095.0f;
        
        int16_t y1 = GRAPH_Y + GRAPH_HEIGHT - (int16_t)(value1 * GRAPH_HEIGHT);
        int16_t y2 = GRAPH_Y + GRAPH_HEIGHT - (int16_t)(value2 * GRAPH_HEIGHT);
        
        // Clamp to graph area
        y1 = max(GRAPH_Y, min(GRAPH_Y + GRAPH_HEIGHT - 1, y1));
        y2 = max(GRAPH_Y, min(GRAPH_Y + GRAPH_HEIGHT - 1, y2));
        
        displayManager.drawLine(GRAPH_X + x, y1, GRAPH_X + x + 1, y2, viz.traceColor);
    }
    
    // Draw statistics
    displayManager.setFont(FONT_TINY);
    displayManager.drawText(GRAPH_X + GRAPH_WIDTH - 80, GRAPH_Y - 8, 
                           "Mean: " + String(anomalyDetector.mean, 3), COLOR_LIGHT_GRAY);
}

void EntropyBeaconApp::drawSpectrum() {
    // Simple spectrum analyzer
    performSimpleFFT();
    
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(GRAPH_X, GRAPH_Y - 15, "Frequency Spectrum", COLOR_CYAN_GLOW);
    
// ========================================
// OPTIONAL BASEAPP METHODS
// ========================================

void EntropyBeaconApp::onPause() {
    // Stop recording when pausing
    if (viz.recordingEnabled) {
        stopRecording();
    }
}

void EntropyBeaconApp::onResume() {
    // Recalculate sample interval in case system timing changed
    calculateSampleInterval();
}

String EntropyBeaconApp::getSettingName(uint8_t index) const {
    switch (index) {
        case 0: return "Oscilloscope";
        case 1: return "Spectrum";
        case 2: return "Waterfall";
        case 3: return "Anomaly View";
        case 4: return "Start Recording";
        case 5: return "Export Data";
        default: return "";
    }
}

void EntropyBeaconApp::handleSetting(uint8_t index) {
    switch (index) {
        case 0: // Oscilloscope
            setVisualizationMode(VIZ_OSCILLOSCOPE);
            break;
        case 1: // Spectrum
            setVisualizationMode(VIZ_SPECTRUM);
            break;
        case 2: // Waterfall
            setVisualizationMode(VIZ_WATERFALL);
            break;
        case 3: // Anomaly View
            setVisualizationMode(VIZ_ANOMALY);
            break;
        case 4: // Start Recording
            if (viz.recordingEnabled) {
                stopDataRecording();
            } else {
                startDataRecording();
            }
            break;
        case 5: // Export Data
            exportData("entropy_export.json", "json");
            break;
    }
}

// ========================================
// SAMPLING AND DATA PROCESSING
// ========================================

void EntropyBeaconApp::sampleEntropy() {
    // Create new data point
    EntropyPoint point;
    point.timestamp = millis();
    point.anomaly = false;
    
    // Generate entropy using selected algorithm
    uint32_t entropyValue = 0;
    
    switch (generators.activeGenerator) {
        case ENTROPY_ADC_NOISE:
            // Traditional ADC noise sampling
            {
                uint16_t sample1 = readEntropySource(ENTROPY_PIN_1);
                uint16_t sample2 = readEntropySource(ENTROPY_PIN_2);
                uint16_t sample3 = readEntropySource(ENTROPY_PIN_3);
                entropyValue = sample1 ^ (sample2 << 4) ^ (sample3 << 8);
                point.source = ENTROPY_ADC_NOISE;
            }
            break;
            
        case ENTROPY_LCG:
            entropyValue = generateLCG();
            point.source = ENTROPY_LCG;
            break;
            
        case ENTROPY_MERSENNE:
            entropyValue = generateMersenneTwister();
            point.source = ENTROPY_MERSENNE;
            break;
            
        case ENTROPY_LOGISTIC_MAP:
            entropyValue = (uint32_t)(generateLogisticMap() * 0xFFFFFFFF);
            point.source = ENTROPY_LOGISTIC_MAP;
            break;
            
        case ENTROPY_HENON_MAP:
            {
                float x, y;
                generateHenonMap(&x, &y);
                entropyValue = (uint32_t)((x + 2.0f) * 0x7FFFFFFF); // Scale and shift
                point.source = ENTROPY_HENON_MAP;
            }
            break;
            
        case ENTROPY_LORENZ:
            {
                float x, y, z;
                generateLorenzSystem(&x, &y, &z);
                entropyValue = (uint32_t)((x + 50.0f) * 0x1FFFFFF); // Scale Lorenz attractor
                point.source = ENTROPY_LORENZ;
            }
            break;
            
        case ENTROPY_LFSR:
            entropyValue = generateLFSR();
            point.source = ENTROPY_LFSR;
            break;
            
        case ENTROPY_CHAOS_COMBINED:
        default:
            entropyValue = generateChaoticCombined();
            point.source = ENTROPY_CHAOS_COMBINED;
            break;
    }
    
    // Convert to 12-bit range and normalize
    point.value = entropyValue & 0xFFF;
    point.normalized = (float)point.value / 4095.0f;
    
    // Calculate Shannon entropy for this sample
    uint16_t recentSamples[32];
    uint16_t sampleCount = min(32, getBufferSize());
    for (uint16_t i = 0; i < sampleCount; i++) {
        uint16_t idx = (bufferIndex - i - 1 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
        recentSamples[i] = entropyBuffer[idx].value;
    }
    if (sampleCount > 0) {
        recentSamples[0] = point.value; // Include current sample
        point.shannonEntropy = calculateShannonEntropy(recentSamples, sampleCount);
    }
    
    // Estimate algorithmic complexity
    if (sampleCount >= 8) {
        point.complexity = estimateKolmogorovComplexity(recentSamples, sampleCount);
    }
    
    // Process the point for anomaly detection
    processEntropyPoint(point);
    
    // Store in circular buffer
    entropyBuffer[bufferIndex] = point;
    bufferIndex = (bufferIndex + 1) % ENTROPY_BUFFER_SIZE;
    
    if (!bufferFull && bufferIndex == 0) {
        bufferFull = true;
    }
    
    // Update histogram
    updateHistogram(point.value);
    
    // Update advanced analysis
    updateAdvancedAnalysis();
    
    // Write to recording file if active
    if (viz.recordingEnabled) {
        writeDataPoint(point);
        viz.samplesRecorded++;
    }
}

void EntropyBeaconApp::updateAdvancedAnalysis() {
    if (getBufferSize() < 16) return;
    
    // Get recent data for analysis
    uint16_t recentData[64];
    uint16_t analysisSize = min(64, getBufferSize());
    
    for (uint16_t i = 0; i < analysisSize; i++) {
        uint16_t idx = (bufferIndex - i - 1 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
        recentData[i] = entropyBuffer[idx].value;
    }
    
    // Update comprehensive analysis
    analysis.shannonEntropy = calculateShannonEntropy(recentData, analysisSize);
    analysis.conditionalEntropy = calculateConditionalEntropy(recentData, analysisSize);
    analysis.compressionRatio = calculateCompressionRatio((uint8_t*)recentData, analysisSize * 2);
    analysis.algorithmicComplexity = estimateKolmogorovComplexity(recentData, analysisSize);
    analysis.chiSquareValue = performChiSquareTest(recentData, analysisSize);
    
    // Calculate serial correlations
    calculateSerialCorrelation(recentData, analysisSize);
    
    // Update spectral analysis
    analysis.spectralEntropy = calculateSpectralEntropy();
    
    // Find dominant frequency
    float maxMagnitude = 0.0f;
    for (uint16_t i = 1; i < FFT_SIZE/2; i++) {
        if (spectrumData[i].magnitude > maxMagnitude) {
            maxMagnitude = spectrumData[i].magnitude;
            analysis.dominantFrequency = spectrumData[i].frequency;
        }
    }
    
    // Calculate spectral flatness (Wiener entropy)
    float geometricMean = 1.0f;
    float arithmeticMean = 0.0f;
    uint16_t validBins = 0;
    
    for (uint16_t i = 1; i < FFT_SIZE/2; i++) {
        if (spectrumData[i].magnitude > 0.001f) {
            geometricMean *= powf(spectrumData[i].magnitude, 1.0f / (FFT_SIZE/2 - 1));
            arithmeticMean += spectrumData[i].magnitude;
            validBins++;
        }
    }
    
    if (validBins > 0) {
        arithmeticMean /= validBins;
        analysis.spectralFlatness = (arithmeticMean > 0.0f) ? geometricMean / arithmeticMean : 0.0f;
    }
    
    // Calculate Lyapunov exponent from recent trajectory
    float trajectory[64];
    for (uint16_t i = 0; i < min(64, analysisSize); i++) {
        trajectory[i] = (float)recentData[i] / 4095.0f;
    }
    analysis.lyapunovExponent = calculateLyapunovExponent(trajectory, min(64, analysisSize));
    
    // Calculate fractal dimension
    analysis.fractalDimension = calculateFractalDimension(recentData, analysisSize);
    
    // Update pattern analysis
    if (analysisSize >= 8) {
        // Simple pattern detection - look for repetitions
        uint16_t patterns = 0;
        for (uint16_t i = 0; i < analysisSize - 4; i++) {
            for (uint16_t j = i + 4; j < analysisSize - 4; j++) {
                bool match = true;
                for (uint16_t k = 0; k < 4; k++) {
                    if (recentData[i + k] != recentData[j + k]) {
                        match = false;
                        break;
                    }
                }
                if (match) patterns++;
            }
        }
        analysis.patternRepeats = patterns;
        analysis.predictability = (float)patterns / (analysisSize - 8);
    }
    
    analysis.compressionEfficiency = 1.0f - analysis.compressionRatio;
}

void EntropyBeaconApp::processEntropyPoint(EntropyPoint& point) {
    // Advanced anomaly detection
    detectAnomalies(point);
    
    // Statistical anomaly detection
    point.anomaly = point.anomaly || isAnomaly(point.normalized);
    
    // Mahalanobis distance anomaly detection
    if (getBufferSize() > 1) {
        uint16_t prevIdx = (bufferIndex - 1 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
        float prevValue = entropyBuffer[prevIdx].normalized;
        float mahalanobis = calculateMahalanobisDistance(point.normalized, prevValue);
        point.anomaly = point.anomaly || (mahalanobis > anomalyDetector.mahalanobisThreshold);
    }
    
    // Pattern-based anomaly detection
    point.anomaly = point.anomaly || detectPatternAnomalies(point.value);
    
    // Temporal anomaly detection
    point.anomaly = point.anomaly || detectTemporalAnomalies(point.timestamp);
    
    // Update clustering for ML-based anomaly detection
    if (getBufferSize() > 0) {
        uint16_t prevIdx = (bufferIndex - 1 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
        float prevValue = entropyBuffer[prevIdx].normalized;
        updateClustering(point.normalized, prevValue);
        point.anomaly = point.anomaly || isClusterAnomaly(point.normalized, prevValue);
    }
    
    // Update running statistics
    updateAnomalyStats(point.normalized);
    
    // Log significant anomalies
    if (point.anomaly) {
        logAnomaly(point);
    }
}

void EntropyBeaconApp::calculateSampleInterval() {
    // Convert sample rate to microsecond interval
    sampleInterval = 1000000 / viz.sampleRate;
    
    // Clamp to reasonable limits
    sampleInterval = max(MIN_SAMPLE_INTERVAL, min(MAX_SAMPLE_INTERVAL, sampleInterval));
    
    debugLog("Sample interval set to: " + String(sampleInterval) + " us");
}

uint16_t EntropyBeaconApp::readEntropySource(uint8_t source) {
    // Read from analog pin with some processing
    uint16_t reading = analogRead(source);
    
    // Add system entropy for more randomness
    reading ^= (systemCore.getEntropyPool() & 0xFFF);
    
    return reading;
}

// ========================================
// VISUALIZATION METHODS
// ========================================

void EntropyBeaconApp::drawOscilloscope() {
    if (getBufferSize() < 2) return;
    
    // Enhanced oscilloscope with multiple traces and advanced features
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(GRAPH_X, GRAPH_Y - 15, "Advanced Entropy Oscilloscope", COLOR_GREEN_PHOS);
    
    // Draw grid if enabled
    if (viz.showGrid) {
        drawGrid();
    }
    
    // Draw trigger line
    drawTriggerLine();
    
    // Draw statistical overlay
    int16_t meanY = GRAPH_Y + GRAPH_HEIGHT - (int16_t)((anomalyDetector.mean * GRAPH_HEIGHT));
    int16_t stdDevY1 = GRAPH_Y + GRAPH_HEIGHT - (int16_t)(((anomalyDetector.mean + getStandardDeviation()) * GRAPH_HEIGHT));
    int16_t stdDevY2 = GRAPH_Y + GRAPH_HEIGHT - (int16_t)(((anomalyDetector.mean - getStandardDeviation()) * GRAPH_HEIGHT));
    
    displayManager.drawLine(GRAPH_X, meanY, GRAPH_X + GRAPH_WIDTH, meanY, COLOR_BLUE_CYBER);
    displayManager.drawLine(GRAPH_X, stdDevY1, GRAPH_X + GRAPH_WIDTH, stdDevY1, COLOR_PURPLE_GLOW);
    displayManager.drawLine(GRAPH_X, stdDevY2, GRAPH_X + GRAPH_WIDTH, stdDevY2, COLOR_PURPLE_GLOW);
    
    // Calculate display parameters
    uint16_t samplesPerPixel = max(1, getBufferSize() / GRAPH_WIDTH);
    
    // Multiple trace rendering
    for (uint8_t trace = 0; trace < 3 && trace < viz.activeTraces; trace++) {
        if (!(viz.activeTraces & (1 << trace))) continue;
        
        uint16_t traceColor = viz.traceColors[trace];
        int16_t traceOffset = trace * 10; // Offset for multiple traces
        
        for (int16_t x = 0; x < GRAPH_WIDTH - 1; x++) {
            uint16_t sampleIndex1 = (bufferIndex - getBufferSize() + x * samplesPerPixel + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
            uint16_t sampleIndex2 = (bufferIndex - getBufferSize() + (x + 1) * samplesPerPixel + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
            
            if (!bufferFull && (sampleIndex1 >= bufferIndex || sampleIndex2 >= bufferIndex)) {
                continue;
            }
            
            EntropyPoint& point1 = entropyBuffer[sampleIndex1];
            EntropyPoint& point2 = entropyBuffer[sampleIndex2];
            
            // Different traces show different aspects
            float value1, value2;
            switch (trace) {
                case 0: // Raw entropy values
                    value1 = point1.normalized;
                    value2 = point2.normalized;
                    break;
                case 1: // Shannon entropy overlay
                    value1 = point1.shannonEntropy / 8.0f; // Normalize to 0-1
                    value2 = point2.shannonEntropy / 8.0f;
                    traceColor = COLOR_CYAN_GLOW;
                    break;
                case 2: // Complexity overlay
                    value1 = point1.complexity / 10.0f; // Normalize to 0-1
                    value2 = point2.complexity / 10.0f;
                    traceColor = COLOR_ORANGE_GLOW;
                    break;
            }
            
            // Convert values to screen coordinates
            int16_t y1 = GRAPH_Y + GRAPH_HEIGHT - (int16_t)(value1 * GRAPH_HEIGHT) + traceOffset;
            int16_t y2 = GRAPH_Y + GRAPH_HEIGHT - (int16_t)(value2 * GRAPH_HEIGHT) + traceOffset;
            
            // Clamp to graph area
            y1 = max(GRAPH_Y, min(GRAPH_Y + GRAPH_HEIGHT - 1, y1));
            y2 = max(GRAPH_Y, min(GRAPH_Y + GRAPH_HEIGHT - 1, y2));
            
            // Anomaly highlighting
            if (point1.anomaly || point2.anomaly) {
                traceColor = COLOR_RED_GLOW;
            }
            
            // Persistence effect
            uint8_t alpha = 255 - (viz.persistence * x) / GRAPH_WIDTH;
            if (alpha > 128) {
                displayManager.drawLine(GRAPH_X + x, y1, GRAPH_X + x + 1, y2, traceColor);
            }
            
            // Special markers for different entropy sources
            if (trace == 0 && x % 20 == 0) {
                uint16_t markerColor = COLOR_WHITE;
                switch (point1.source) {
                    case ENTROPY_LOGISTIC_MAP:
                        markerColor = COLOR_ORANGE_GLOW;
                        break;
                    case ENTROPY_HENON_MAP:
                        markerColor = COLOR_PURPLE_GLOW;
                        break;
                    case ENTROPY_LORENZ:
                        markerColor = COLOR_BLUE_CYBER;
                        break;
                    case ENTROPY_MERSENNE:
                        markerColor = COLOR_CYAN_GLOW;
                        break;
                    case ENTROPY_CHAOS_COMBINED:
                        markerColor = COLOR_WHITE;
                        break;
                }
                displayManager.drawPixel(GRAPH_X + x, y1 - 1, markerColor);
            }
        }
    }
    
    // Draw anomaly event markers along bottom
    int16_t markerY = GRAPH_Y + GRAPH_HEIGHT + 2;
    for (int16_t x = 0; x < GRAPH_WIDTH; x++) {
        uint16_t sampleIndex = (bufferIndex - getBufferSize() + x * samplesPerPixel + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
        if (!bufferFull && sampleIndex >= bufferIndex) continue;
        
        EntropyPoint& point = entropyBuffer[sampleIndex];
        if (point.anomaly) {
            displayManager.drawLine(GRAPH_X + x, markerY, GRAPH_X + x, markerY + 3, COLOR_RED_GLOW);
        }
    }
    
    // Real-time statistics display
    displayManager.setFont(FONT_TINY);
    if (getBufferSize() > 0) {
        EntropyPoint& current = entropyBuffer[(bufferIndex - 1 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE];
        
        String stats = "H=" + String(current.shannonEntropy, 1) +
                      " K=" + String(current.complexity, 1) +
                      " λ=" + String(analysis.lyapunovExponent, 2);
        displayManager.drawText(GRAPH_X + GRAPH_WIDTH - 100, GRAPH_Y - 8, stats, COLOR_LIGHT_GRAY);
        
        // Current generator indicator
        String genText = "Gen: ";
        switch (current.source) {
            case ENTROPY_ADC_NOISE: genText += "ADC"; break;
            case ENTROPY_LCG: genText += "LCG"; break;
            case ENTROPY_MERSENNE: genText += "MT19937"; break;
            case ENTROPY_LOGISTIC_MAP: genText += "Logistic"; break;
            case ENTROPY_HENON_MAP: genText += "Hénon"; break;
            case ENTROPY_LORENZ: genText += "Lorenz"; break;
            case ENTROPY_LFSR: genText += "LFSR"; break;
            case ENTROPY_CHAOS_COMBINED: genText += "Chaos∞"; break;
        }
        displayManager.drawText(GRAPH_X, GRAPH_Y + GRAPH_HEIGHT + 8, genText, COLOR_LIGHT_GRAY);
    }
    
    // Trace legend
    displayManager.setFont(FONT_TINY);
    int16_t legendX = GRAPH_X + GRAPH_WIDTH - 60;
    int16_t legendY = GRAPH_Y + 10;
    
    if (viz.activeTraces & 0x01) {
        displayManager.drawText(legendX, legendY, "Raw", viz.traceColors[0]);
        legendY += 8;
    }
    if (viz.activeTraces & 0x02) {
        displayManager.drawText(legendX, legendY, "H(x)", COLOR_CYAN_GLOW);
        legendY += 8;
    }
    if (viz.activeTraces & 0x04) {
        displayManager.drawText(legendX, legendY, "K(x)", COLOR_ORANGE_GLOW);
    }
}

void EntropyBeaconApp::drawSpectrum() {
    // Perform FFT on recent data
    performFFT();
    
    // Draw frequency bars
    int16_t barWidth = GRAPH_WIDTH / viz.spectrumBars;
    int16_t barSpacing = max(1, barWidth / 4);
    
    for (uint8_t i = 0; i < viz.spectrumBars; i++) {
        if (i >= FFT_SIZE/2) break;
        
        float magnitude = spectrumData[i].magnitude * viz.spectrumGain;
        int16_t barHeight = (int16_t)(magnitude * GRAPH_HEIGHT);
        barHeight = min(barHeight, GRAPH_HEIGHT);
        
        int16_t barX = GRAPH_X + i * (barWidth + barSpacing);
        int16_t barY = GRAPH_Y + GRAPH_HEIGHT - barHeight;
        
        // Color based on frequency
        uint16_t barColor = COLOR_GREEN_PHOS;
        if (i < viz.spectrumBars / 3) {
            barColor = COLOR_RED_GLOW; // Low frequencies
        } else if (i > 2 * viz.spectrumBars / 3) {
            barColor = COLOR_BLUE_CYBER; // High frequencies
        }
        
        displayManager.drawRetroRect(barX, barY, barWidth - barSpacing, barHeight, barColor, true);
    }
    
    // Draw frequency labels
    displayManager.setFont(FONT_SMALL);
    for (uint8_t i = 0; i < 4; i++) {
        int16_t labelX = GRAPH_X + i * GRAPH_WIDTH / 3;
        float frequency = (float)(i * viz.sampleRate) / 6.0f; // Rough approximation
        displayManager.drawText(labelX, GRAPH_Y + GRAPH_HEIGHT + 5, 
                               formatFrequency(frequency), COLOR_LIGHT_GRAY);
    }
}

void EntropyBeaconApp::drawWaterfall() {
    // Shift waterfall data up
    for (int16_t y = 0; y < WATERFALL_HEIGHT - 1; y++) {
        for (int16_t x = 0; x < GRAPH_WIDTH; x++) {
            waterfallData[y][x] = waterfallData[y + 1][x];
        }
    }
    
    // Add new line from current spectrum
    performFFT();
    for (int16_t x = 0; x < GRAPH_WIDTH; x++) {
        uint8_t spectrumIndex = (x * (FFT_SIZE/2)) / GRAPH_WIDTH;
        float magnitude = spectrumData[spectrumIndex].magnitude;
        waterfallData[WATERFALL_HEIGHT - 1][x] = (uint8_t)(magnitude * 255);
    }
    
    // Draw waterfall
    for (int16_t y = 0; y < min(WATERFALL_HEIGHT, GRAPH_HEIGHT); y++) {
        for (int16_t x = 0; x < GRAPH_WIDTH; x++) {
            uint8_t intensity = waterfallData[y][x];
            
            // Convert intensity to color
            uint16_t pixelColor = COLOR_BLACK;
            if (intensity > 200) {
                pixelColor = COLOR_WHITE;
            } else if (intensity > 150) {
                pixelColor = COLOR_RED_GLOW;
            } else if (intensity > 100) {
                pixelColor = COLOR_PURPLE_GLOW;
            } else if (intensity > 50) {
                pixelColor = COLOR_GREEN_PHOS;
            } else if (intensity > 25) {
                pixelColor = COLOR_DARK_GRAY;
            }
            
            if (pixelColor != COLOR_BLACK) {
                displayManager.drawPixel(GRAPH_X + x, GRAPH_Y + y, pixelColor);
            }
        }
    }
}

void EntropyBeaconApp::drawScatterPlot() {
    if (getBufferSize() < 2) return;
    
    // Enhanced phase space plot with multiple analysis modes
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(GRAPH_X, GRAPH_Y - 15, "Phase Space Analysis", COLOR_CYAN_GLOW);
    
    // Draw axes
    int16_t centerX = GRAPH_X + GRAPH_WIDTH / 2;
    int16_t centerY = GRAPH_Y + GRAPH_HEIGHT / 2;
    
    displayManager.drawLine(GRAPH_X, centerY, GRAPH_X + GRAPH_WIDTH, centerY, COLOR_DARK_GRAY);
    displayManager.drawLine(centerX, GRAPH_Y, centerX, GRAPH_Y + GRAPH_HEIGHT, COLOR_DARK_GRAY);
    
    // Draw grid for reference
    for (int i = 1; i < 4; i++) {
        int16_t gridX = GRAPH_X + (i * GRAPH_WIDTH) / 4;
        int16_t gridY = GRAPH_Y + (i * GRAPH_HEIGHT) / 4;
        displayManager.drawLine(gridX, GRAPH_Y, gridX, GRAPH_Y + GRAPH_HEIGHT, COLOR_VERY_DARK_GRAY);
        displayManager.drawLine(GRAPH_X, gridY, GRAPH_X + GRAPH_WIDTH, gridY, COLOR_VERY_DARK_GRAY);
    }
    
    // Multi-dimensional phase space plotting
    uint16_t plotPoints = min(200, getBufferSize() - 2);
    
    for (uint16_t i = 0; i < plotPoints; i++) {
        uint16_t idx1 = (bufferIndex - plotPoints + i + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
        uint16_t idx2 = (bufferIndex - plotPoints + i + 1 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
        uint16_t idx3 = (bufferIndex - plotPoints + i + 2 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
        
        if (!bufferFull && (idx1 >= bufferIndex || idx2 >= bufferIndex || idx3 >= bufferIndex)) {
            continue;
        }
        
        EntropyPoint& point1 = entropyBuffer[idx1];
        EntropyPoint& point2 = entropyBuffer[idx2];
        EntropyPoint& point3 = entropyBuffer[idx3];
        
        // Phase space coordinates (Takens embedding)
        float x_coord, y_coord;
        
        // Different embedding dimensions based on entropy source
        switch (point1.source) {
            case ENTROPY_HENON_MAP:
            case ENTROPY_LORENZ:
                // For chaotic sources, use proper phase space coordinates
                x_coord = point1.normalized;
                y_coord = point2.normalized;
                break;
                
            case ENTROPY_LOGISTIC_MAP:
                // For 1D chaotic map, use delay embedding
                x_coord = point1.normalized;
                y_coord = point2.normalized - point1.normalized; // Derivative approximation
                break;
                
            default:
                // Standard lag plot for other sources
                x_coord = point1.normalized;
                y_coord = point2.normalized;
                break;
        }
        
        // Map to screen coordinates
        int16_t screenX = GRAPH_X + (int16_t)(x_coord * GRAPH_WIDTH);
        int16_t screenY = GRAPH_Y + GRAPH_HEIGHT - (int16_t)(y_coord * GRAPH_HEIGHT);
        
        // Clamp to graph area
        screenX = max(GRAPH_X, min(GRAPH_X + GRAPH_WIDTH - 1, screenX));
        screenY = max(GRAPH_Y, min(GRAPH_Y + GRAPH_HEIGHT - 1, screenY));
        
        // Color coding based on point characteristics
        uint16_t pointColor = COLOR_GREEN_PHOS;
        
        if (point1.anomaly) {
            pointColor = COLOR_RED_GLOW;
        } else if (point1.complexity > 4.0f) {
            pointColor = COLOR_PURPLE_GLOW;
        } else if (point1.shannonEntropy > 6.0f) {
            pointColor = COLOR_BLUE_CYBER;
        } else {
            // Color based on entropy source
            switch (point1.source) {
                case ENTROPY_MERSENNE:
                    pointColor = COLOR_CYAN_GLOW;
                    break;
                case ENTROPY_LOGISTIC_MAP:
                    pointColor = COLOR_ORANGE_GLOW;
                    break;
                case ENTROPY_HENON_MAP:
                    pointColor = COLOR_PURPLE_GLOW;
                    break;
                case ENTROPY_LORENZ:
                    pointColor = COLOR_BLUE_CYBER;
                    break;
                case ENTROPY_CHAOS_COMBINED:
                    pointColor = COLOR_WHITE;
                    break;
                default:
                    pointColor = COLOR_GREEN_PHOS;
                    break;
            }
        }
        
        // Draw point with trajectory if it's a chaotic source
        if (point1.source == ENTROPY_HENON_MAP || point1.source == ENTROPY_LORENZ ||
            point1.source == ENTROPY_LOGISTIC_MAP) {
            
            // Draw trajectory line to previous point
            if (i > 0) {
                uint16_t prevIdx = (bufferIndex - plotPoints + i - 1 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
                EntropyPoint& prevPoint = entropyBuffer[prevIdx];
                
                float prev_x = prevPoint.normalized;
                float prev_y = (i > 1) ? entropyBuffer[(bufferIndex - plotPoints + i + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE].normalized : prevPoint.normalized;
                
                int16_t prevScreenX = GRAPH_X + (int16_t)(prev_x * GRAPH_WIDTH);
                int16_t prevScreenY = GRAPH_Y + GRAPH_HEIGHT - (int16_t)(prev_y * GRAPH_HEIGHT);
                
                prevScreenX = max(GRAPH_X, min(GRAPH_X + GRAPH_WIDTH - 1, prevScreenX));
                prevScreenY = max(GRAPH_Y, min(GRAPH_Y + GRAPH_HEIGHT - 1, prevScreenY));
                
                // Faded trajectory line
                uint16_t trailColor = (pointColor == COLOR_WHITE) ? COLOR_LIGHT_GRAY : COLOR_DARK_GRAY;
                displayManager.drawLine(prevScreenX, prevScreenY, screenX, screenY, trailColor);
            }
        }
        
        // Draw the actual point
        displayManager.drawPixel(screenX, screenY, pointColor);
        
        // Highlight recent points with larger markers
        if (i >= plotPoints - 10) {
            displayManager.drawRetroCircle(screenX, screenY, 1, pointColor, false);
        }
    }
    
    // Draw attractor information
    displayManager.setFont(FONT_TINY);
    if (getBufferSize() > 0) {
        EntropyPoint& current = entropyBuffer[(bufferIndex - 1 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE];
        String sourceText = "Source: ";
        switch (current.source) {
            case ENTROPY_ADC_NOISE: sourceText += "ADC"; break;
            case ENTROPY_LCG: sourceText += "LCG"; break;
            case ENTROPY_MERSENNE: sourceText += "MT"; break;
            case ENTROPY_LOGISTIC_MAP: sourceText += "Logistic"; break;
            case ENTROPY_HENON_MAP: sourceText += "Hénon"; break;
            case ENTROPY_LORENZ: sourceText += "Lorenz"; break;
            case ENTROPY_LFSR: sourceText += "LFSR"; break;
            case ENTROPY_CHAOS_COMBINED: sourceText += "Combined"; break;
        }
        
        displayManager.drawText(GRAPH_X, GRAPH_Y + GRAPH_HEIGHT + 5, sourceText, COLOR_LIGHT_GRAY);
        displayManager.drawText(GRAPH_X + 80, GRAPH_Y + GRAPH_HEIGHT + 5,
                               "Lyapunov: " + String(analysis.lyapunovExponent, 3), COLOR_LIGHT_GRAY);
    }
}

void EntropyBeaconApp::drawHistogram() {
    // Find maximum bin count for scaling
    uint16_t maxCount = 0;
    for (uint16_t i = 0; i < 256; i++) {
        maxCount = max(maxCount, histogramBins[i]);
    }
    
    if (maxCount == 0) return;
    
    // Draw histogram bars
    int16_t barWidth = GRAPH_WIDTH / 256;
    if (barWidth < 1) barWidth = 1;
    
    for (uint16_t i = 0; i < 256; i++) {
        int16_t barHeight = (histogramBins[i] * GRAPH_HEIGHT) / maxCount;
        int16_t barX = GRAPH_X + (i * GRAPH_WIDTH) / 256;
        int16_t barY = GRAPH_Y + GRAPH_HEIGHT - barHeight;
        
        if (barHeight > 0) {
            displayManager.drawRetroRect(barX, barY, barWidth, barHeight, COLOR_GREEN_PHOS, true);
        }
    }
    
    // Draw distribution statistics
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(GRAPH_X, GRAPH_Y - 15, 
                           "Mean: " + String(anomalyDetector.mean, 3), COLOR_WHITE);
    displayManager.drawText(GRAPH_X + 100, GRAPH_Y - 15,
                           "StdDev: " + String(getStandardDeviation(), 3), COLOR_WHITE);
}

void EntropyBeaconApp::drawAnomalyView() {
    // Draw comprehensive anomaly analysis
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawText(GRAPH_X, GRAPH_Y - 20, "Advanced Anomaly Analysis", COLOR_RED_GLOW);
    
    displayManager.setFont(FONT_SMALL);
    int16_t yPos = GRAPH_Y;
    int16_t lineHeight = 12;
    
    // Statistical anomaly detection
    displayManager.drawText(GRAPH_X, yPos, "Statistical Anomalies: " + String(anomalyDetector.anomalyCount), COLOR_WHITE);
    yPos += lineHeight;
    
    displayManager.drawText(GRAPH_X, yPos, "Threshold: " + String(anomalyDetector.threshold, 1) + "σ", COLOR_LIGHT_GRAY);
    yPos += lineHeight;
    
    // Pattern anomalies
    displayManager.drawText(GRAPH_X, yPos, "Pattern Repeats: " + String(anomalyDetector.repeatedPatterns), COLOR_PURPLE_GLOW);
    yPos += lineHeight;
    
    // Temporal anomalies
    displayManager.drawText(GRAPH_X, yPos, "Timing Anomalies: " + String(anomalyDetector.timingAnomalies), COLOR_BLUE_CYBER);
    yPos += lineHeight;
    
    // Mahalanobis distance
    if (getBufferSize() > 1) {
        uint16_t prevIdx = (bufferIndex - 1 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
        uint16_t currentIdx = (bufferIndex - 2 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
        float mahalanobis = calculateMahalanobisDistance(
            entropyBuffer[prevIdx].normalized,
            entropyBuffer[currentIdx].normalized
        );
        displayManager.drawText(GRAPH_X, yPos, "Mahalanobis: " + String(mahalanobis, 2), COLOR_ORANGE_GLOW);
        yPos += lineHeight;
    }
    
    // Current entropy quality metrics
    if (getBufferSize() > 0) {
        EntropyPoint& current = entropyBuffer[(bufferIndex - 1 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE];
        displayManager.drawText(GRAPH_X, yPos, "Shannon H: " + String(current.shannonEntropy, 2), COLOR_GREEN_PHOS);
        yPos += lineHeight;
        
        displayManager.drawText(GRAPH_X, yPos, "Complexity: " + String(current.complexity, 2), COLOR_CYAN_GLOW);
        yPos += lineHeight;
    }
    
    // Real-time anomaly indicator with severity levels
    float currentValue = getCurrentEntropy();
    bool isCurrentAnomaly = false;
    uint16_t indicatorColor = COLOR_GREEN_PHOS;
    String statusText = "NORMAL";
    
    if (getBufferSize() > 0) {
        EntropyPoint& current = entropyBuffer[(bufferIndex - 1 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE];
        isCurrentAnomaly = current.anomaly;
        
        if (isCurrentAnomaly) {
            // Determine anomaly severity
            float deviation = abs(currentValue - anomalyDetector.mean) / getStandardDeviation();
            if (deviation > 5.0f) {
                indicatorColor = COLOR_RED_GLOW;
                statusText = "CRITICAL";
            } else if (deviation > 3.0f) {
                indicatorColor = COLOR_ORANGE_GLOW;
                statusText = "HIGH";
            } else {
                indicatorColor = COLOR_PURPLE_GLOW;
                statusText = "MODERATE";
            }
        }
    }
    
    int16_t indicatorY = GRAPH_Y + 95;
    displayManager.drawRetroRect(GRAPH_X, indicatorY, 120, 18, indicatorColor, true);
    displayManager.drawTextCentered(GRAPH_X, indicatorY + 4, 120, statusText, COLOR_BLACK);
    displayManager.setFont(FONT_TINY);
    displayManager.drawTextCentered(GRAPH_X, indicatorY + 12, 120, String(currentValue, 3), COLOR_BLACK);
    
    // Enhanced anomaly timeline with different anomaly types
    int16_t timelineY = GRAPH_Y + 120;
    displayManager.drawLine(GRAPH_X, timelineY, GRAPH_X + GRAPH_WIDTH, timelineY, COLOR_DARK_GRAY);
    
    // Draw time markers
    for (int i = 0; i <= 4; i++) {
        int16_t markerX = GRAPH_X + (i * GRAPH_WIDTH) / 4;
        displayManager.drawLine(markerX, timelineY - 2, markerX, timelineY + 2, COLOR_LIGHT_GRAY);
        if (i == 0) {
            displayManager.setFont(FONT_TINY);
            displayManager.drawText(markerX - 5, timelineY + 5, "Now", COLOR_LIGHT_GRAY);
        } else if (i == 4) {
            displayManager.drawText(markerX - 10, timelineY + 5, "60s", COLOR_LIGHT_GRAY);
        }
    }
    
    // Mark recent anomalies with type indicators
    unsigned long currentTime = millis();
    for (uint16_t i = 0; i < getBufferSize(); i++) {
        EntropyPoint& point = entropyBuffer[i];
        if (point.anomaly && (currentTime - point.timestamp) < 60000) {
            float timeRatio = (float)(currentTime - point.timestamp) / 60000.0f;
            int16_t timelineX = GRAPH_X + (int16_t)(timeRatio * GRAPH_WIDTH);
            
            // Different markers for different anomaly types
            uint16_t markerColor = COLOR_RED_GLOW;
            int16_t markerHeight = 8;
            
            // Determine anomaly type based on characteristics
            if (point.complexity > 5.0f) {
                markerColor = COLOR_PURPLE_GLOW; // High complexity
                markerHeight = 6;
            } else if (point.shannonEntropy > 6.0f) {
                markerColor = COLOR_BLUE_CYBER; // High entropy
                markerHeight = 10;
            } else if (point.source == ENTROPY_CHAOS_COMBINED) {
                markerColor = COLOR_ORANGE_GLOW; // Chaotic source
                markerHeight = 7;
            }
            
            displayManager.drawLine(timelineX, timelineY - markerHeight, timelineX, timelineY + markerHeight, markerColor);
            displayManager.drawRetroCircle(timelineX, timelineY, 2, markerColor, true);
        }
    }
    
    // Draw clustering visualization
    if (anomalyDetector.clustering.initialized) {
        int16_t clusterX = GRAPH_X + GRAPH_WIDTH - 60;
        int16_t clusterY = GRAPH_Y + 20;
        
        displayManager.setFont(FONT_TINY);
        displayManager.drawText(clusterX, clusterY - 8, "Clusters", COLOR_LIGHT_GRAY);
        
        // Draw cluster centers
        for (uint8_t i = 0; i < anomalyDetector.clustering.activeCluster; i++) {
            int16_t centerX = clusterX + (int16_t)(anomalyDetector.clustering.centroids[i][0] * 30);
            int16_t centerY = clusterY + (int16_t)(anomalyDetector.clustering.centroids[i][1] * 30);
            
            uint16_t clusterColor = (i == 0) ? COLOR_GREEN_PHOS :
                                   (i == 1) ? COLOR_BLUE_CYBER :
                                   (i == 2) ? COLOR_PURPLE_GLOW : COLOR_ORANGE_GLOW;
            
            displayManager.drawRetroCircle(centerX, centerY, (int16_t)(anomalyDetector.clustering.clusterRadii[i] * 5), clusterColor, false);
            displayManager.drawRetroCircle(centerX, centerY, 1, clusterColor, true);
        }
    }
}

void EntropyBeaconApp::drawGrid() {
    // Draw horizontal grid lines
    for (int16_t i = 1; i < 4; i++) {
        int16_t y = GRAPH_Y + (i * GRAPH_HEIGHT) / 4;
        displayManager.drawLine(GRAPH_X, y, GRAPH_X + GRAPH_WIDTH, y, COLOR_DARK_GRAY);
    }
    
    // Draw vertical grid lines
    for (int16_t i = 1; i < 4; i++) {
        int16_t x = GRAPH_X + (i * GRAPH_WIDTH) / 4;
        displayManager.drawLine(x, GRAPH_Y, x, GRAPH_Y + GRAPH_HEIGHT, COLOR_DARK_GRAY);
    }
}

void EntropyBeaconApp::drawTriggerLine() {
    int16_t triggerY = GRAPH_Y + GRAPH_HEIGHT - (viz.triggerLevel * GRAPH_HEIGHT) / 255;
    displayManager.drawLine(GRAPH_X, triggerY, GRAPH_X + GRAPH_WIDTH, triggerY, COLOR_PURPLE_GLOW);
}

// ========================================
// ANALYSIS METHODS
// ========================================

void EntropyBeaconApp::performFFT() {
    // Simplified FFT implementation
    // In a real implementation, you'd use a proper FFT library
    
    uint16_t dataSize = min(FFT_SIZE, getBufferSize());
    if (dataSize < 8) return;
    
    // Copy data to working buffer
    float realData[FFT_SIZE];
    float imagData[FFT_SIZE];
    
    for (uint16_t i = 0; i < dataSize; i++) {
        uint16_t bufferIdx = (bufferIndex + i) % ENTROPY_BUFFER_SIZE;
        realData[i] = entropyBuffer[bufferIdx].normalized - 0.5f; // Center around zero
        imagData[i] = 0.0f;
    }
    
    // Zero pad if necessary
    for (uint16_t i = dataSize; i < FFT_SIZE; i++) {
        realData[i] = 0.0f;
        imagData[i] = 0.0f;
    }
    
    // Simple magnitude calculation (not a real FFT)
    for (uint16_t i = 0; i < FFT_SIZE/2; i++) {
        float sum = 0.0f;
        float frequency = (float)i * viz.sampleRate / FFT_SIZE;
        
        // Calculate correlation with sine wave at this frequency
        for (uint16_t j = 0; j < dataSize; j++) {
            float phase = 2.0f * PI * frequency * j / viz.sampleRate;
            sum += realData[j] * sin(phase);
        }
        
        spectrumData[i].frequency = frequency;
        spectrumData[i].magnitude = abs(sum) / dataSize;
        spectrumData[i].phase = 0.0f;
    }
    
    normalizeSpectrum();
}

void EntropyBeaconApp::normalizeSpectrum() {
    // Find maximum magnitude
    float maxMagnitude = 0.0f;
    for (uint16_t i = 0; i < FFT_SIZE/2; i++) {
        maxMagnitude = max(maxMagnitude, spectrumData[i].magnitude);
    }
    
    // Normalize to 0-1 range
    if (maxMagnitude > 0.0f) {
        for (uint16_t i = 0; i < FFT_SIZE/2; i++) {
            spectrumData[i].magnitude /= maxMagnitude;
        }
    }
}

void EntropyBeaconApp::initializeAnomalyDetector() {
    anomalyDetector.mean = 0.5f;
    anomalyDetector.variance = 0.1f;
    anomalyDetector.threshold = ANOMALY_THRESHOLD_DEFAULT;
    anomalyDetector.windowSize = 100;
    anomalyDetector.enabled = true;
    anomalyDetector.anomalyCount = 0;
}

void EntropyBeaconApp::detectAnomalies(EntropyPoint& point) {
    if (!anomalyDetector.enabled) return;
    
    point.anomaly = isAnomaly(point.normalized);
    
    if (point.anomaly) {
        anomalyDetector.anomalyCount++;
        logAnomaly(point);
    }
}

void EntropyBeaconApp::updateAnomalyStats(float value) {
    // Update running mean and variance using exponential moving average
    float alpha = 0.01f; // Learning rate
    
    float delta = value - anomalyDetector.mean;
    anomalyDetector.mean += alpha * delta;
    anomalyDetector.variance += alpha * (delta * delta - anomalyDetector.variance);
}

bool EntropyBeaconApp::isAnomaly(float value) {
    float standardDev = getStandardDeviation();
    float deviation = abs(value - anomalyDetector.mean);
    
    return deviation > (anomalyDetector.threshold * standardDev);
}

void EntropyBeaconApp::logAnomaly(EntropyPoint& point) {
    debugLog("ANOMALY detected: value=" + String(point.normalized, 4) + 
             " at time=" + String(point.timestamp));
}

void EntropyBeaconApp::updateHistogram(uint16_t value) {
    uint8_t binIndex = value >> 4; // Convert 12-bit to 8-bit for histogram
    if (binIndex < 256) {
        histogramBins[binIndex]++;
        
        // Prevent overflow by scaling down periodically
        if (histogramBins[binIndex] > 30000) {
            for (uint16_t i = 0; i < 256; i++) {
                histogramBins[i] /= 2;
            }
        }
    }
}

// ========================================
// DAC OUTPUT METHODS
// ========================================

void EntropyBeaconApp::updateDACOutput() {
    if (viz.dacMode == DAC_OFF || getBufferSize() == 0) return;
    
    // Get current entropy value
    EntropyPoint& currentPoint = entropyBuffer[(bufferIndex - 1 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE];
    uint16_t outputValue = 0;
    
    switch (viz.dacMode) {
        case DAC_RAW:
            // Raw entropy output with full dynamic range
            outputValue = currentPoint.value >> 4; // Convert 12-bit to 8-bit
            break;
            
        case DAC_FILTERED:
            // Multi-stage filtering for clean output
            {
                float filtered = applyFilter(currentPoint.normalized, 0); // Low-pass
                filtered = applyFilter(filtered, 1); // High-pass for DC removal
                filtered = applyFilter(filtered, 2); // Band-pass for audio range
                outputValue = (uint8_t)(constrain(filtered * 255.0f, 0, 255));
            }
            break;
            
        case DAC_TONE:
            // Advanced audio synthesis based on entropy characteristics
            generateAdvancedDACWaveform();
            return;
            
        case DAC_MODULATED:
            // Multiple modulation schemes
            generateModulatedOutput(currentPoint);
            return;
            
        case DAC_PULSE:
            // Sophisticated pulse generation with entropy-based timing
            generateEntropyPulseOutput(currentPoint);
            return;
            
        default:
            return;
    }
    
    // Output to both DAC channels if available
    outputToDAC(outputValue);
}

void EntropyBeaconApp::generateAdvancedDACWaveform() {
    static unsigned long lastUpdate = 0;
    static float phase = 0.0f;
    static float amplitude = 1.0f;
    static uint8_t waveformType = 0;
    
    unsigned long currentTime = micros();
    if (currentTime - lastUpdate < 125) return; // 8kHz update rate
    
    if (getBufferSize() > 0) {
        EntropyPoint& currentPoint = entropyBuffer[(bufferIndex - 1 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE];
        
        // Dynamic frequency based on Shannon entropy
        float baseFreq = 200.0f + (currentPoint.shannonEntropy * 800.0f); // 200Hz - 1kHz
        
        // Modulate frequency with complexity
        float frequency = baseFreq * (1.0f + 0.3f * currentPoint.complexity);
        
        // Dynamic amplitude based on anomaly detection
        amplitude = currentPoint.anomaly ? 0.8f : 0.5f;
        
        // Waveform selection based on entropy source
        waveformType = (uint8_t)currentPoint.source % 5;
        
        // Calculate phase increment
        float sampleRate = 8000.0f;
        float phaseIncrement = 2.0f * PI * frequency / sampleRate;
        phase += phaseIncrement;
        if (phase > 2.0f * PI) phase -= 2.0f * PI;
        
        float waveformValue = 0.0f;
        
        switch (waveformType) {
            case 0: // Sine wave
                waveformValue = amplitude * sinf(phase);
                break;
                
            case 1: // Square wave with entropy-based duty cycle
                {
                    float dutyCycle = 0.3f + 0.4f * currentPoint.normalized;
                    waveformValue = (phase < dutyCycle * 2.0f * PI) ? amplitude : -amplitude;
                }
                break;
                
            case 2: // Sawtooth wave
                waveformValue = amplitude * (2.0f * (phase / (2.0f * PI)) - 1.0f);
                break;
                
            case 3: // Triangle wave
                {
                    float normalizedPhase = phase / (2.0f * PI);
                    waveformValue = amplitude * (normalizedPhase < 0.5f ?
                        4.0f * normalizedPhase - 1.0f : 3.0f - 4.0f * normalizedPhase);
                }
                break;
                
            case 4: // Chaos-based waveform
                {
                    // Use logistic map output directly
                    float logisticValue = generateLogisticMap();
                    waveformValue = amplitude * (2.0f * logisticValue - 1.0f);
                }
                break;
        }
        
        // Add entropy-based noise for true randomness
        float noiseLevel = 0.1f * currentPoint.normalized;
        float noise = noiseLevel * (2.0f * ((float)random(1000) / 1000.0f) - 1.0f);
        waveformValue += noise;
        
        // Convert to DAC range and output
        uint8_t dacValue = (uint8_t)((waveformValue + 1.0f) * 127.5f);
        outputToDAC(dacValue);
    }
    
    lastUpdate = currentTime;
}

void EntropyBeaconApp::generateModulatedOutput(EntropyPoint& point) {
    static unsigned long lastUpdate = 0;
    static float carrierPhase = 0.0f;
    static float modulatorPhase = 0.0f;
    static uint8_t modulationType = 0;
    
    unsigned long currentTime = micros();
    if (currentTime - lastUpdate < 125) return; // 8kHz update rate
    
    float sampleRate = 8000.0f;
    
    // Carrier frequency based on entropy value
    float carrierFreq = 440.0f + (point.normalized * 1000.0f); // 440Hz - 1440Hz
    
    // Modulator frequency based on complexity
    float modFreq = 5.0f + (point.complexity * 45.0f); // 5Hz - 50Hz modulation
    
    // Update phases
    carrierPhase += 2.0f * PI * carrierFreq / sampleRate;
    modulatorPhase += 2.0f * PI * modFreq / sampleRate;
    
    if (carrierPhase > 2.0f * PI) carrierPhase -= 2.0f * PI;
    if (modulatorPhase > 2.0f * PI) modulatorPhase -= 2.0f * PI;
    
    // Modulation type cycles based on anomaly detection
    if (point.anomaly) {
        modulationType = (modulationType + 1) % 4;
    }
    
    float outputValue = 0.0f;
    float modulator = sinf(modulatorPhase);
    float carrier = sinf(carrierPhase);
    
    switch (modulationType) {
        case 0: // Amplitude Modulation (AM)
            {
                float modDepth = 0.5f + 0.4f * point.shannonEntropy / 8.0f; // Depth based on entropy
                outputValue = carrier * (1.0f + modDepth * modulator);
            }
            break;
            
        case 1: // Frequency Modulation (FM)
            {
                float deviation = 100.0f + 200.0f * point.normalized; // Frequency deviation
                float fmPhase = carrierPhase + (deviation / carrierFreq) * modulator;
                outputValue = sinf(fmPhase);
            }
            break;
            
        case 2: // Phase Modulation (PM)
            {
                float phaseDeviation = PI * point.normalized; // Phase deviation
                outputValue = sinf(carrierPhase + phaseDeviation * modulator);
            }
            break;
            
        case 3: // Ring Modulation
            outputValue = carrier * modulator;
            break;
    }
    
    // Add entropy-based distortion for chaos
    if (point.source == ENTROPY_CHAOS_COMBINED) {
        float distortion = 0.2f * point.complexity;
        outputValue = outputValue + distortion * powf(outputValue, 3.0f);
    }
    
    // Clamp and convert to DAC range
    outputValue = constrain(outputValue, -1.0f, 1.0f);
    uint8_t dacValue = (uint8_t)((outputValue + 1.0f) * 127.5f);
    
    outputToDAC(dacValue);
    lastUpdate = currentTime;
}

void EntropyBeaconApp::generateEntropyPulseOutput(EntropyPoint& point) {
    static unsigned long lastPulse = 0;
    static bool pulseState = false;
    static float pulseWidth = 0.5f;
    static float pulseInterval = 1000.0f; // microseconds
    
    unsigned long currentTime = micros();
    
    // Dynamic pulse parameters based on entropy characteristics
    float baseInterval = 500.0f + point.normalized * 2000.0f; // 0.5ms - 2.5ms
    float complexityModulation = 1.0f + 0.5f * point.complexity;
    pulseInterval = baseInterval * complexityModulation;
    
    // Pulse width modulation based on Shannon entropy
    pulseWidth = 0.1f + 0.8f * (point.shannonEntropy / 8.0f); // 10% - 90% duty cycle
    
    // Anomaly detection affects pulse pattern
    if (point.anomaly) {
        pulseInterval *= 0.5f; // Double the pulse rate for anomalies
        pulseWidth = 0.95f; // Nearly full width for anomaly indication
    }
    
    // Generate pulse train
    if (currentTime - lastPulse >= pulseInterval) {
        pulseState = true;
        lastPulse = currentTime;
    }
    
    float pulseWidthTime = pulseInterval * pulseWidth;
    if (pulseState && (currentTime - lastPulse >= pulseWidthTime)) {
        pulseState = false;
    }
    
    // Multi-level pulse output based on entropy source
    uint8_t pulseAmplitude = 0;
    if (pulseState) {
        switch (point.source) {
            case ENTROPY_ADC_NOISE:
                pulseAmplitude = 255; // Full amplitude
                break;
            case ENTROPY_LCG:
                pulseAmplitude = 200; // 78% amplitude
                break;
            case ENTROPY_MERSENNE:
                pulseAmplitude = 180; // 70% amplitude
                break;
            case ENTROPY_LOGISTIC_MAP:
                pulseAmplitude = 160; // 63% amplitude
                break;
            case ENTROPY_HENON_MAP:
                pulseAmplitude = 140; // 55% amplitude
                break;
            case ENTROPY_LORENZ:
                pulseAmplitude = 120; // 47% amplitude
                break;
            case ENTROPY_LFSR:
                pulseAmplitude = 100; // 39% amplitude
                break;
            case ENTROPY_CHAOS_COMBINED:
            default:
                // Variable amplitude based on current entropy value
                pulseAmplitude = (uint8_t)(50 + point.normalized * 205);
                break;
        }
    }
    
    outputToDAC(pulseAmplitude);
}

void EntropyBeaconApp::generateDACWaveform() {
    // Generate audio tone based on current entropy
    static unsigned long lastToneUpdate = 0;
    static float phase = 0.0f;
    
    if (millis() - lastToneUpdate < 1) return; // 1ms update rate
    
    if (getBufferSize() > 0) {
        EntropyPoint& currentPoint = entropyBuffer[(bufferIndex - 1 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE];
        
        // Map entropy to frequency (100Hz - 2kHz)
        float frequency = 100.0f + (currentPoint.normalized * 1900.0f);
        
        // Generate sine wave
        float sampleRate = 8000.0f; // 8kHz DAC update rate
        phase += 2.0f * PI * frequency / sampleRate;
        if (phase > 2.0f * PI) {
            phase -= 2.0f * PI;
        }
        
        uint8_t outputValue = (sin(phase) + 1.0f) * 127.5f;
        outputToDAC(outputValue);
    }
    
    lastToneUpdate = millis();
}

    static float highPassState = 0.0f;
    static float bandPassState1 = 0.0f;
    static float bandPassState2 = 0.0f;
    static float lastInput = 0.0f;
    
    switch (filterType) {
        case 0: // Low-pass filter (cutoff ~800Hz at 8kHz sample rate)
            {
                float alpha = 0.2f; // Cutoff frequency parameter
                lowPassState = alpha * input + (1.0f - alpha) * lowPassState;
                return lowPassState;
            }
            
        case 1: // High-pass filter (cutoff ~200Hz at 8kHz sample rate)
            {
                float alpha = 0.95f; // High-pass parameter
                highPassState = alpha * (highPassState + input - lastInput);
                lastInput = input;
                return highPassState;
            }
            
        case 2: // Band-pass filter (200Hz - 800Hz)
            {
                // Two-stage cascaded filter
                float alpha1 = 0.95f; // High-pass stage
                bandPassState1 = alpha1 * (bandPassState1 + input - lastInput);
                
                float alpha2 = 0.2f; // Low-pass stage
                bandPassState2 = alpha2 * bandPassState1 + (1.0f - alpha2) * bandPassState2;
                
                lastInput = input;
                return bandPassState2;
            }
            
        case 3: // Notch filter (removes 60Hz hum)
            {
                // Simple notch at 60Hz
                static float delay1 = 0.0f, delay2 = 0.0f;
                float output = input - delay2;
                delay2 = delay1;
                delay1 = input;
                return output * 0.5f;
            }
            
        default:
            return input;
    }
}

void EntropyBeaconApp::outputToDAC(uint16_t value) {
    uint8_t dacValue = (uint8_t)min(255, (int)value);
    dacWrite(DAC_OUT_PIN, dacValue);
}

// ========================================
// UI AND CONTROL METHODS
// ========================================

void EntropyBeaconApp::drawInterface() {
    // Draw title
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawText(5, 5, "Entropy Beacon", COLOR_RED_GLOW);
    
    // Draw mode indicator
    String modeNames[] = {"OSC", "SPEC", "FALL", "SCAT", "HIST", "ANOM"};
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(150, 8, modeNames[viz.mode], COLOR_GREEN_PHOS);
    
    // Draw sample rate
    displayManager.drawText(200, 8, String(viz.sampleRate) + "Hz", COLOR_WHITE);
}

void EntropyBeaconApp::drawControls() {
    // Mode buttons
    int16_t buttonY = 220;
    displayManager.drawButton(5, buttonY, 30, 16, "Mode");
    displayManager.drawButton(40, buttonY, 30, 16, "Rate");
    displayManager.drawButton(75, buttonY, 30, 16, "DAC");
    
    // Recording indicator
    if (viz.recordingEnabled) {
        displayManager.drawButton(110, buttonY, 40, 16, "REC", BUTTON_PRESSED, COLOR_RED_GLOW);
    } else {
        displayManager.drawButton(110, buttonY, 40, 16, "Rec");
    }
    
    // Export button
    displayManager.drawButton(155, buttonY, 40, 16, "Export");
}

void EntropyBeaconApp::drawStatusBar() {
    displayManager.setFont(FONT_SMALL);
    
    // Buffer status
    String bufferStatus = "Buf: " + String(getBufferSize()) + "/" + String(ENTROPY_BUFFER_SIZE);
    displayManager.drawText(5, 25, bufferStatus, COLOR_LIGHT_GRAY);
    
    // Current entropy value
    if (getBufferSize() > 0) {
        float currentEntropy = getCurrentEntropy();
        displayManager.drawText(100, 25, "Val: " + String(currentEntropy, 3), COLOR_WHITE);
    }
    
    // Anomaly count
    displayManager.drawText(200, 25, "Anom: " + String(anomalyDetector.anomalyCount), 
                           anomalyDetector.anomalyCount > 0 ? COLOR_RED_GLOW : COLOR_LIGHT_GRAY);
}


void EntropyBeaconApp::handleControlTouch(TouchPoint touch) {
    // Enhanced touch handling with advanced entropy controls
    for (uint8_t i = 0; i < 8; i++) {
        InteractionZone& zone = touchZones[i];
        if (zone.enabled && touchInterface.isPointInRect(touch, zone.x, zone.y, zone.w, zone.h)) {
            
            if (zone.function == "mode") {
                // Cycle through visualization modes
                viz.mode = (VisualizationMode)((viz.mode + 1) % 6);
                debugLog("Visualization mode: " + String(viz.mode));
                
            } else if (zone.function == "generator") {
                // Cycle through entropy generators
                EntropyGeneratorType generators[] = {
                    ENTROPY_ADC_NOISE, ENTROPY_LCG, ENTROPY_MERSENNE,
                    ENTROPY_LOGISTIC_MAP, ENTROPY_HENON_MAP, ENTROPY_LORENZ,
                    ENTROPY_LFSR, ENTROPY_CHAOS_COMBINED
                };
                
                for (uint8_t g = 0; g < 8; g++) {
                    if (generators.activeGenerator == generators[g]) {
                        generators.activeGenerator = generators[(g + 1) % 8];
                        break;
                    }
                }
                
                // Reseed generators when switching
                seedGenerators(millis() ^ analogRead(ENTROPY_PIN_1));
                debugLog("Entropy generator: " + String(generators.activeGenerator));
                
            } else if (zone.function == "rate") {
                // Cycle through sample rates with extended range
                SampleRate rates[] = {RATE_100HZ, RATE_500HZ, RATE_1KHZ, RATE_2KHZ, RATE_5KHZ, RATE_10KHZ};
                for (uint8_t r = 0; r < 6; r++) {
                    if (viz.sampleRate == rates[r]) {
                        viz.sampleRate = rates[(r + 1) % 6];
                        break;
                    }
                }
                calculateSampleInterval();
                debugLog("Sample rate: " + String(viz.sampleRate) + "Hz");
                
            } else if (zone.function == "dac") {
                // Cycle through DAC modes
                viz.dacMode = (DACMode)((viz.dacMode + 1) % 6);
                debugLog("DAC mode: " + String(viz.dacMode));
                
            } else if (zone.function == "anomaly") {
                // Adjust anomaly detection sensitivity
                float thresholds[] = {1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f};
                for (uint8_t t = 0; t < 7; t++) {
                    if (abs(anomalyDetector.threshold - thresholds[t]) < 0.1f) {
                        anomalyDetector.threshold = thresholds[(t + 1) % 7];
                        break;
                    }
                }
                debugLog("Anomaly threshold: " + String(anomalyDetector.threshold, 1) + "σ");
                
            } else if (zone.function == "traces") {
                // Toggle trace visibility
                viz.activeTraces = ((viz.activeTraces + 1) % 8) | 0x01; // Always keep trace 0 active
                debugLog("Active traces: " + String(viz.activeTraces, BIN));
                
            } else if (zone.function == "record") {
                // Toggle recording with enhanced options
                if (viz.recordingEnabled) {
                    stopDataRecording();
                } else {
                    String filename = "entropy_" + String(generators.activeGenerator) +
                                     "_" + String(viz.sampleRate) + "Hz_" + String(millis()) + ".csv";
                    startDataRecording(filename);
                }
                
            } else if (zone.function == "export") {
                // Export with comprehensive analysis
                String timestamp = String(millis());
                exportAdvancedAnalysis("analysis_" + timestamp + ".json");
                
            } else if (zone.function == "graph") {
                // Graph area touch for parameter adjustment
                handleGraphTouch(touch);
            }
            
            return;
        }
    }
    
    // Long press detection for advanced functions
    static unsigned long pressStartTime = 0;
    static bool longPressHandled = false;
    
    if (touch.isNewPress) {
        pressStartTime = millis();
        longPressHandled = false;
    } else if (touch.isPressed && !longPressHandled && (millis() - pressStartTime > 1000)) {
        // Long press detected - open advanced settings
        handleLongPress(touch);
        longPressHandled = true;
    }
}

void EntropyBeaconApp::handleGraphTouch(TouchPoint touch) {
    // Interactive graph parameter adjustment
    int16_t relativeX = touch.x - GRAPH_X;
    int16_t relativeY = touch.y - GRAPH_Y;
    
    if (relativeX < 0 || relativeX >= GRAPH_WIDTH || relativeY < 0 || relativeY >= GRAPH_HEIGHT) {
        return;
    }
    
    switch (viz.mode) {
        case VIZ_OSCILLOSCOPE:
            {
                // Adjust trigger level and time scale
                if (relativeY < GRAPH_HEIGHT / 2) {
                    // Upper half - adjust trigger level
                    viz.triggerLevel = (uint8_t)((relativeY * 255) / (GRAPH_HEIGHT / 2));
                    debugLog("Trigger level: " + String(viz.triggerLevel));
                } else {
                    // Lower half - adjust time scale
                    viz.timeScale = 0.1f + (relativeX * 2.0f) / GRAPH_WIDTH;
                    debugLog("Time scale: " + String(viz.timeScale, 2));
                }
            }
            break;
            
        case VIZ_SPECTRUM:
            {
                // Adjust spectrum gain and frequency range
                viz.spectrumGain = 0.1f + (relativeY * 5.0f) / GRAPH_HEIGHT;
                debugLog("Spectrum gain: " + String(viz.spectrumGain, 2));
            }
            break;
            
        case VIZ_ANOMALY:
            {
                // Adjust detection parameters
                float newThreshold = 1.0f + (relativeY * 4.0f) / GRAPH_HEIGHT;
                setAnomalyThreshold(newThreshold);
                debugLog("Anomaly threshold: " + String(newThreshold, 2));
            }
            break;
            
        default:
            // For other modes, adjust general amplitude scaling
            viz.amplitudeScale = 0.1f + (relativeY * 3.0f) / GRAPH_HEIGHT;
            debugLog("Amplitude scale: " + String(viz.amplitudeScale, 2));
            break;
    }
}

void EntropyBeaconApp::handleLongPress(TouchPoint touch) {
    // Advanced functions accessed via long press
    debugLog("Long press detected - Advanced mode");
    
    // Cycle through advanced generator parameters
    switch (generators.activeGenerator) {
        case ENTROPY_LOGISTIC_MAP:
            // Adjust growth parameter for different chaotic regimes
            generators.logistic.r += 0.1f;
            if (generators.logistic.r > 4.0f) generators.logistic.r = 3.0f;
            debugLog("Logistic r = " + String(generators.logistic.r, 2));
            break;
            
        case ENTROPY_HENON_MAP:
            // Adjust Hénon parameters
            generators.henon.a += 0.1f;
            if (generators.henon.a > 1.8f) generators.henon.a = 1.0f;
            debugLog("Hénon a = " + String(generators.henon.a, 2));
            break;
            
        case ENTROPY_LORENZ:
            // Adjust Lorenz system parameters
            generators.lorenz.rho += 2.0f;
            if (generators.lorenz.rho > 40.0f) generators.lorenz.rho = 20.0f;
            debugLog("Lorenz ρ = " + String(generators.lorenz.rho, 1));
            break;
            
        case ENTROPY_LCG:
            // Switch LCG parameters to different well-known values
            if (generators.lcg.a == 1664525) {
                generators.lcg.a = 214013;      // Microsoft Visual C++
                generators.lcg.c = 2531011;
            } else {
                generators.lcg.a = 1664525;     // Numerical Recipes
                generators.lcg.c = 1013904223;
            }
            debugLog("LCG parameters switched");
            break;
            
        case ENTROPY_CHAOS_COMBINED:
            // Toggle multiple source usage
            generators.useMultipleSources = !generators.useMultipleSources;
            debugLog("Multi-source: " + String(generators.useMultipleSources ? "ON" : "OFF"));
            break;
            
        default:
            // For other generators, reset to initial conditions
            seedGenerators(random(0xFFFFFFFF));
            debugLog("Generator reseeded");
            break;
    }
}

void EntropyBeaconApp::setupTouchZones() {
    // Enhanced touch zones with additional controls
    touchZones[0] = {5, 220, 25, 16, "mode", true};        // Visualization mode
    touchZones[1] = {35, 220, 30, 16, "generator", true};   // Entropy generator
    touchZones[2] = {70, 220, 25, 16, "rate", true};        // Sample rate
    touchZones[3] = {100, 220, 25, 16, "dac", true};        // DAC mode
    touchZones[4] = {130, 220, 30, 16, "anomaly", true};    // Anomaly sensitivity
    touchZones[5] = {165, 220, 25, 16, "traces", true};     // Trace visibility
    touchZones[6] = {195, 220, 25, 16, "record", true};     // Recording
    touchZones[7] = {225, 220, 30, 16, "export", true};     // Export analysis
    
    // Graph area for interactive parameter adjustment
    touchZones[8] = {GRAPH_X, GRAPH_Y, GRAPH_WIDTH, GRAPH_HEIGHT, "graph", true};
}

bool EntropyBeaconApp::exportAdvancedAnalysis(String filename) {
    // Export comprehensive entropy analysis
    String fullPath = getAppDataPath() + "/" + filename;
    
    filesystem.writeFile(fullPath, "");
    File exportFile = SD.open(fullPath, FILE_WRITE);
    if (!exportFile) {
        debugLog("Failed to create analysis export: " + fullPath);
        return false;
    }
    
    // Create comprehensive JSON analysis export
    DynamicJsonDocument doc(16384); // Larger buffer for comprehensive analysis
    
    // Metadata
    doc["export_timestamp"] = millis();
    doc["export_type"] = "comprehensive_entropy_analysis";
    doc["version"] = "2.0";
    
    // Current configuration
    JsonObject config = doc.createNestedObject("configuration");
    config["active_generator"] = generators.activeGenerator;
    config["sample_rate"] = viz.sampleRate;
    config["buffer_size"] = getBufferSize();
    config["visualization_mode"] = viz.mode;
    config["dac_mode"] = viz.dacMode;
    config["anomaly_threshold"] = anomalyDetector.threshold;
    
    // Mathematical analysis results
    JsonObject mathAnalysis = doc.createNestedObject("mathematical_analysis");
    mathAnalysis["shannon_entropy"] = analysis.shannonEntropy;
    mathAnalysis["conditional_entropy"] = analysis.conditionalEntropy;
    mathAnalysis["mutual_information"] = analysis.mutualInformation;
    mathAnalysis["kolmogorov_complexity"] = analysis.algorithmicComplexity;
    mathAnalysis["compression_ratio"] = analysis.compressionRatio;
    mathAnalysis["chi_square_value"] = analysis.chiSquareValue;
    mathAnalysis["spectral_entropy"] = analysis.spectralEntropy;
    mathAnalysis["lyapunov_exponent"] = analysis.lyapunovExponent;
    mathAnalysis["fractal_dimension"] = analysis.fractalDimension;
    mathAnalysis["dominant_frequency"] = analysis.dominantFrequency;
    mathAnalysis["spectral_flatness"] = analysis.spectralFlatness;
    
    // Serial correlation array
    JsonArray correlations = mathAnalysis.createNestedArray("serial_correlations");
    for (int i = 0; i < 10; i++) {
        correlations.add(analysis.serialCorrelation[i]);
    }
    
    // Anomaly detection statistics
    JsonObject anomalies = doc.createNestedObject("anomaly_detection");
    anomalies["total_anomalies"] = anomalyDetector.anomalyCount;
    anomalies["pattern_repeats"] = anomalyDetector.repeatedPatterns;
    anomalies["timing_anomalies"] = anomalyDetector.timingAnomalies;
    anomalies["statistical_mean"] = anomalyDetector.mean;
    anomalies["statistical_variance"] = anomalyDetector.variance;
    anomalies["mahalanobis_threshold"] = anomalyDetector.mahalanobisThreshold;
    
    // Clustering information
    if (anomalyDetector.clustering.initialized) {
        JsonObject clustering = anomalies.createNestedObject("clustering");
        clustering["active_clusters"] = anomalyDetector.clustering.activeCluster;
        
        JsonArray centroids = clustering.createNestedArray("centroids");
        JsonArray radii = clustering.createNestedArray("radii");
        
        for (uint8_t i = 0; i < anomalyDetector.clustering.activeCluster; i++) {
            JsonObject centroid = centroids.createNestedObject();
            centroid["x"] = anomalyDetector.clustering.centroids[i][0];
            centroid["y"] = anomalyDetector.clustering.centroids[i][1];
            radii.add(anomalyDetector.clustering.clusterRadii[i]);
        }
    }
    
    // Generator-specific parameters
    JsonObject genParams = doc.createNestedObject("generator_parameters");
    genParams["lcg_a"] = generators.lcg.a;
    genParams["lcg_c"] = generators.lcg.c;
    genParams["lcg_m"] = generators.lcg.m;
    genParams["logistic_r"] = generators.logistic.r;
    genParams["henon_a"] = generators.henon.a;
    genParams["henon_b"] = generators.henon.b;
    genParams["lorenz_sigma"] = generators.lorenz.sigma;
    genParams["lorenz_rho"] = generators.lorenz.rho;
    genParams["lorenz_beta"] = generators.lorenz.beta;
    genParams["use_multiple_sources"] = generators.useMultipleSources;
    
    // Recent data samples (last 100 points)
    JsonArray recentData = doc.createNestedArray("recent_samples");
    uint16_t sampleCount = min(100, getBufferSize());
    
    for (uint16_t i = 0; i < sampleCount; i++) {
        uint16_t idx = (bufferIndex - sampleCount + i + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
        JsonObject sample = recentData.createNestedObject();
        
        EntropyPoint& point = entropyBuffer[idx];
        sample["timestamp"] = point.timestamp;
        sample["value"] = point.value;
        sample["normalized"] = point.normalized;
        sample["shannon_entropy"] = point.shannonEntropy;
        sample["complexity"] = point.complexity;
        sample["source"] = point.source;
        sample["anomaly"] = point.anomaly;
    }
    
    // Write JSON to file
    serializeJsonPretty(doc, exportFile);
    exportFile.close();
    
    debugLog("Advanced analysis exported: " + filename);
    return true;
}

// ========================================
// UTILITY METHODS
// ========================================

uint16_t EntropyBeaconApp::mapToGraph(uint16_t value, int16_t graphHeight) {
    return (value * graphHeight) / 4095;
}

float EntropyBeaconApp::getCurrentEntropy() const {
    if (getBufferSize() == 0) return 0.0f;
    
    uint16_t lastIndex = (bufferIndex - 1 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
    return entropyBuffer[lastIndex].normalized;
}

float EntropyBeaconApp::getStandardDeviation() const {
    return sqrt(anomalyDetector.variance);
}

String EntropyBeaconApp::formatFrequency(float frequency) {
    if (frequency >= 1000.0f) {
        return String(frequency / 1000.0f, 1) + "k";
    } else {
        return String((int)frequency);
    }
}

void EntropyBeaconApp::setVisualizationMode(VisualizationMode mode) {
    viz.mode = mode;
    
    // Mode-specific initialization
    switch (mode) {
        case VIZ_SPECTRUM:
            performFFT();
            break;
        case VIZ_HISTOGRAM:
            // Clear histogram for fresh start
            memset(histogramBins, 0, sizeof(histogramBins));
            break;
        default:
            break;
    }
    
    debugLog("Visualization mode set to: " + String(mode));
}

bool EntropyBeaconApp::startDataRecording(String filename) {
    if (viz.recordingEnabled) return false;
    
    if (filename.isEmpty()) {
        filename = "entropy_" + String(millis()) + ".csv";
    }
    
    String fullPath = getAppDataPath() + "/" + filename;
    recordingFile = SD.open(fullPath, FILE_WRITE);
    
    if (!recordingFile) {
        debugLog("Failed to create recording file: " + fullPath);
        return false;
    }
    
    // Write CSV header
    recordingFile.println("timestamp,value,normalized,anomaly");
    
    viz.recordingEnabled = true;
    viz.recordStartTime = millis();
    viz.samplesRecorded = 0;
    
    debugLog("Recording started: " + filename);
    return true;
}

bool EntropyBeaconApp::stopDataRecording() {
    if (!viz.recordingEnabled) return false;
    
    recordingFile.close();
    viz.recordingEnabled = false;
    
    debugLog("Recording stopped. Samples recorded: " + String(viz.samplesRecorded));
    return true;
}

void EntropyBeaconApp::writeDataPoint(EntropyPoint& point) {
    if (!viz.recordingEnabled || !recordingFile) return;
    
    // Enhanced CSV format with comprehensive data
    recordingFile.print(point.timestamp);
    recordingFile.print(",");
    recordingFile.print(point.value);
    recordingFile.print(",");
    recordingFile.print(point.normalized, 6);
    recordingFile.print(",");
    recordingFile.print(point.shannonEntropy, 4);
    recordingFile.print(",");
    recordingFile.print(point.complexity, 4);
    recordingFile.print(",");
    recordingFile.print(point.source);
    recordingFile.print(",");
    recordingFile.print(point.anomaly ? "1" : "0");
    recordingFile.print(",");
    recordingFile.print(anomalyDetector.mean, 6);
    recordingFile.print(",");
    recordingFile.print(getStandardDeviation(), 6);
    recordingFile.print(",");
    recordingFile.print(analysis.lyapunovExponent, 6);
    recordingFile.print(",");
    recordingFile.println(analysis.fractalDimension, 6);
    
    // Flush periodically for data integrity
    if (viz.samplesRecorded % 50 == 0) {
        recordingFile.flush();
    }
    
    // Also log to system entropy log
    logEntropyEvent(point);
}

void EntropyBeaconApp::logEntropyEvent(EntropyPoint& point) {
    // Maintain a detailed system log of entropy events
    static File entropyLog;
    static unsigned long lastLogTime = 0;
    static uint32_t logSequence = 0;
    
    // Open log file if not already open (or reopen periodically)
    if (!entropyLog || (millis() - lastLogTime > 300000)) { // Reopen every 5 minutes
        if (entropyLog) entropyLog.close();
        
        String logPath = getAppDataPath() + "/entropy_system.log";
        entropyLog = SD.open(logPath, FILE_APPEND);
        
        if (entropyLog) {
            // Write log header if this is a new session
            if (lastLogTime == 0) {
                entropyLog.println("# EntropyBeacon System Log - Session Start: " + String(millis()));
                entropyLog.println("# Format: seq,timestamp,level,generator,value,entropy,complexity,anomaly,message");
            }
            lastLogTime = millis();
        }
    }
    
    if (!entropyLog) return;
    
    // Log significant events
    bool shouldLog = false;
    String logLevel = "INFO";
    String message = "";
    
    if (point.anomaly) {
        shouldLog = true;
        logLevel = "WARN";
        message = "Anomaly detected: deviation=" + String(abs(point.normalized - anomalyDetector.mean) / getStandardDeviation(), 2) + "σ";
    } else if (point.complexity > 8.0f) {
        shouldLog = true;
        logLevel = "INFO";
        message = "High complexity sample detected";
    } else if (point.shannonEntropy > 7.5f) {
        shouldLog = true;
        logLevel = "INFO";
        message = "High entropy sample detected";
    } else if (logSequence % 1000 == 0) {
        shouldLog = true;
        logLevel = "DEBUG";
        message = "Periodic status checkpoint";
    }
    
    if (shouldLog) {
        entropyLog.print(logSequence++);
        entropyLog.print(",");
        entropyLog.print(point.timestamp);
        entropyLog.print(",");
        entropyLog.print(logLevel);
        entropyLog.print(",");
        entropyLog.print(point.source);
        entropyLog.print(",");
        entropyLog.print(point.value);
        entropyLog.print(",");
        entropyLog.print(point.shannonEntropy, 3);
        entropyLog.print(",");
        entropyLog.print(point.complexity, 3);
        entropyLog.print(",");
        entropyLog.print(point.anomaly ? "1" : "0");
        entropyLog.print(",");
        entropyLog.println(message);
        
        // Flush important events immediately
        if (logLevel == "WARN" || logLevel == "ERROR") {
            entropyLog.flush();
        }
    }
}

void EntropyBeaconApp::logSystemEvent(String level, String event, String details) {
    // Log system-level events (configuration changes, errors, etc.)
    static File systemLog;
    static unsigned long lastSystemLogTime = 0;
    
    if (!systemLog || (millis() - lastSystemLogTime > 600000)) { // Reopen every 10 minutes
        if (systemLog) systemLog.close();
        
        String logPath = getAppDataPath() + "/system_events.log";
        systemLog = SD.open(logPath, FILE_APPEND);
        lastSystemLogTime = millis();
    }
    
    if (systemLog) {
        systemLog.print(millis());
        systemLog.print(" [");
        systemLog.print(level);
        systemLog.print("] ");
        systemLog.print(event);
        if (details.length() > 0) {
            systemLog.print(" - ");
            systemLog.print(details);
        }
        systemLog.println();
        
        // Flush errors and warnings immediately
        if (level == "ERROR" || level == "WARN") {
            systemLog.flush();
        }
    }
}

void EntropyBeaconApp::logPerformanceMetrics() {
    // Log performance and quality metrics periodically
    static unsigned long lastPerfLog = 0;
    static File perfLog;
    
    if (millis() - lastPerfLog < 60000) return; // Log every minute
    
    if (!perfLog) {
        String logPath = getAppDataPath() + "/performance_metrics.csv";
        bool isNewFile = !filesystem.fileExists(logPath);
        perfLog = SD.open(logPath, FILE_APPEND);
        
        if (perfLog && isNewFile) {
            // Write CSV header for new file
            perfLog.println("timestamp,frame_count,fps,memory_usage,buffer_fill,total_anomalies,active_generator,sample_rate,shannon_avg,complexity_avg,lyapunov,fractal_dim");
        }
    }
    
    if (perfLog) {
        // Calculate average metrics over recent samples
        float avgShannon = 0.0f;
        float avgComplexity = 0.0f;
        uint16_t recentSamples = min(100, getBufferSize());
        
        for (uint16_t i = 0; i < recentSamples; i++) {
            uint16_t idx = (bufferIndex - i - 1 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
            avgShannon += entropyBuffer[idx].shannonEntropy;
            avgComplexity += entropyBuffer[idx].complexity;
        }
        
        if (recentSamples > 0) {
            avgShannon /= recentSamples;
            avgComplexity /= recentSamples;
        }
        
        // Calculate current FPS
        float currentFPS = getFPS();
        
        // Log comprehensive performance data
        perfLog.print(millis());
        perfLog.print(",");
        perfLog.print(getFrameCount());
        perfLog.print(",");
        perfLog.print(currentFPS, 2);
        perfLog.print(",");
        perfLog.print(getMemoryUsage());
        perfLog.print(",");
        perfLog.print((float)getBufferSize() / ENTROPY_BUFFER_SIZE, 3);
        perfLog.print(",");
        perfLog.print(anomalyDetector.anomalyCount);
        perfLog.print(",");
        perfLog.print(generators.activeGenerator);
        perfLog.print(",");
        perfLog.print(viz.sampleRate);
        perfLog.print(",");
        perfLog.print(avgShannon, 3);
        perfLog.print(",");
        perfLog.print(avgComplexity, 3);
        perfLog.print(",");
        perfLog.print(analysis.lyapunovExponent, 4);
        perfLog.print(",");
        perfLog.println(analysis.fractalDimension, 4);
        
        perfLog.flush(); // Ensure data is written
        lastPerfLog = millis();
    }
}

void EntropyBeaconApp::logConfigurationChange(String parameter, String oldValue, String newValue) {
    // Log configuration changes for audit trail
    logSystemEvent("INFO", "Configuration Change",
                   parameter + ": " + oldValue + " -> " + newValue);
    
    // Also write to dedicated config log
    static File configLog;
    
    if (!configLog) {
        String logPath = getAppDataPath() + "/configuration_changes.log";
        configLog = SD.open(logPath, FILE_APPEND);
    }
    
    if (configLog) {
        configLog.print(millis());
        configLog.print(" ");
        configLog.print(parameter);
        configLog.print(": ");
        configLog.print(oldValue);
        configLog.print(" -> ");
        configLog.println(newValue);
        configLog.flush();
    }
}

bool EntropyBeaconApp::createPeriodicBackup() {
    // Create periodic backups of important data
    static unsigned long lastBackup = 0;
    
    if (millis() - lastBackup < 1800000) return true; // Backup every 30 minutes
    
    String timestamp = String(millis());
    String backupDir = getAppDataPath() + "/backups";
    
    // Ensure backup directory exists
    if (!filesystem.ensureDirExists(backupDir)) {
        logSystemEvent("ERROR", "Backup Failed", "Could not create backup directory");
        return false;
    }
    
    // Backup current configuration
    String configBackup = backupDir + "/config_" + timestamp + ".json";
    if (saveConfigurationToFile(configBackup)) {
        logSystemEvent("INFO", "Configuration Backup", "Saved to " + configBackup);
    }
    
    // Backup recent entropy data
    String dataBackup = backupDir + "/entropy_" + timestamp + ".csv";
    if (exportRecentData(dataBackup, 1000)) { // Last 1000 samples
        logSystemEvent("INFO", "Data Backup", "Saved to " + dataBackup);
    }
    
    // Cleanup old backups (keep only last 10)
    cleanupOldBackups(backupDir);
    
    lastBackup = millis();
    return true;
}

bool EntropyBeaconApp::saveConfigurationToFile(String filename) {
    // Save current configuration to JSON file
    DynamicJsonDocument config(2048);
    
    config["timestamp"] = millis();
    config["active_generator"] = generators.activeGenerator;
    config["sample_rate"] = viz.sampleRate;
    config["visualization_mode"] = viz.mode;
    config["dac_mode"] = viz.dacMode;
    config["anomaly_threshold"] = anomalyDetector.threshold;
    config["trigger_level"] = viz.triggerLevel;
    config["time_scale"] = viz.timeScale;
    config["amplitude_scale"] = viz.amplitudeScale;
    config["spectrum_gain"] = viz.spectrumGain;
    config["active_traces"] = viz.activeTraces;
    config["show_grid"] = viz.showGrid;
    config["persistence"] = viz.persistence;
    
    // Generator-specific parameters
    JsonObject genConfig = config.createNestedObject("generator_params");
    genConfig["lcg_a"] = generators.lcg.a;
    genConfig["lcg_c"] = generators.lcg.c;
    genConfig["logistic_r"] = generators.logistic.r;
    genConfig["henon_a"] = generators.henon.a;
    genConfig["henon_b"] = generators.henon.b;
    genConfig["lorenz_rho"] = generators.lorenz.rho;
    genConfig["use_multiple_sources"] = generators.useMultipleSources;
    
    filesystem.writeFile(filename, "");
    File configFile = SD.open(filename, FILE_WRITE);
    if (configFile) {
        serializeJsonPretty(config, configFile);
        configFile.close();
        return true;
    }
    
    return false;
}

bool EntropyBeaconApp::exportRecentData(String filename, uint16_t sampleCount) {
    // Export recent entropy data in CSV format
    filesystem.writeFile(filename, "");
    File dataFile = SD.open(filename, FILE_WRITE);
    if (!dataFile) return false;
    
    // Write enhanced CSV header
    dataFile.println("timestamp,value,normalized,shannon_entropy,complexity,source,anomaly,mean,stddev,lyapunov,fractal_dim");
    
    uint16_t actualCount = min(sampleCount, getBufferSize());
    
    for (uint16_t i = 0; i < actualCount; i++) {
        uint16_t idx = (bufferIndex - actualCount + i + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
        EntropyPoint& point = entropyBuffer[idx];
        
        dataFile.print(point.timestamp);
        dataFile.print(",");
        dataFile.print(point.value);
        dataFile.print(",");
        dataFile.print(point.normalized, 6);
        dataFile.print(",");
        dataFile.print(point.shannonEntropy, 4);
        dataFile.print(",");
        dataFile.print(point.complexity, 4);
        dataFile.print(",");
        dataFile.print(point.source);
        dataFile.print(",");
        dataFile.print(point.anomaly ? "1" : "0");
        dataFile.print(",");
        dataFile.print(anomalyDetector.mean, 6);
        dataFile.print(",");
        dataFile.print(getStandardDeviation(), 6);
        dataFile.print(",");
        dataFile.print(analysis.lyapunovExponent, 6);
        dataFile.print(",");
        dataFile.println(analysis.fractalDimension, 6);
    }
    
    dataFile.close();
    return true;
}

void EntropyBeaconApp::cleanupOldBackups(String backupDir) {
    // Keep only the 10 most recent backup files
    std::vector<String> backupFiles = filesystem.listFiles(backupDir);
    
    if (backupFiles.size() <= 10) return;
    
    // Sort files by modification time (approximate based on filename timestamp)
    // Remove oldest files
    for (size_t i = 0; i < backupFiles.size() - 10; i++) {
        String fullPath = backupDir + "/" + backupFiles[i];
        if (filesystem.deleteFile(fullPath)) {
            logSystemEvent("INFO", "Backup Cleanup", "Deleted old backup: " + backupFiles[i]);
        }
    }
}

bool EntropyBeaconApp::exportData(String filename, String format) {
    String fullPath = getAppDataPath() + "/" + filename;
    
    File exportFile = SD.open(fullPath, FILE_WRITE);
    if (!exportFile) {
        debugLog("Failed to create export file: " + fullPath);
        return false;
    }
    
    if (format == "json") {
        // Export as JSON
        DynamicJsonDocument doc(8192);
        
        doc["export_time"] = millis();
        doc["sample_rate"] = viz.sampleRate;
        doc["buffer_size"] = getBufferSize();
        doc["anomaly_count"] = anomalyDetector.anomalyCount;
        doc["statistics"]["mean"] = anomalyDetector.mean;
        doc["statistics"]["variance"] = anomalyDetector.variance;
        doc["statistics"]["std_deviation"] = getStandardDeviation();
        
        JsonArray dataArray = doc.createNestedArray("data");
        for (uint16_t i = 0; i < getBufferSize(); i++) {
            JsonObject pointObj = dataArray.createNestedObject();
            EntropyPoint& point = entropyBuffer[i];
            pointObj["timestamp"] = point.timestamp;
            pointObj["value"] = point.value;
            pointObj["normalized"] = point.normalized;
            pointObj["anomaly"] = point.anomaly;
        }
        
        serializeJson(doc, exportFile);
    }
    
    exportFile.close();
    debugLog("Data exported: " + filename);
    return true;
}

EntropyPoint EntropyBeaconApp::getDataPoint(uint16_t index) const {
    if (index >= getBufferSize()) {
        return {0, 0.0f, 0, false}; // Return empty point
    }
    
    uint16_t bufferIdx = (bufferIndex + index) % ENTROPY_BUFFER_SIZE;
    return entropyBuffer[bufferIdx];
}

void EntropyBeaconApp::calibrateBaseline() {
    debugLog("Calibrating baseline...");
    
    // Reset statistics
    resetStatistics();
    
    // Collect baseline samples
    unsigned long startTime = millis();
    while (millis() - startTime < 5000) { // 5 second calibration
        update();
        delay(1);
    }
    
    debugLog("Baseline calibration complete");
    debugLog("Mean: " + String(anomalyDetector.mean, 4));
    debugLog("StdDev: " + String(getStandardDeviation(), 4));
}

// ========================================
// ADVANCED ENTROPY GENERATION ALGORITHMS
// ========================================

void EntropyBeaconApp::initializeEntropyGenerators() {
    // Initialize Linear Congruential Generator (Numerical Recipes parameters)
    generators.lcg.a = 1664525;      // Multiplier
    generators.lcg.c = 1013904223;   // Increment
    generators.lcg.m = 0xFFFFFFFF;   // Modulus (2^32)
    generators.lcg.seed = millis() ^ analogRead(ENTROPY_PIN_1);
    
    // Initialize Mersenne Twister
    generators.mersenne.initialized = false;
    generators.mersenne.index = 0;
    
    // Initialize Logistic Map (chaotic regime)
    generators.logistic.r = 3.9f;   // Growth parameter for chaos
    generators.logistic.x = 0.5f;   // Initial condition
    
    // Initialize Hénon Map
    generators.henon.a = 1.4f;      // Standard parameters
    generators.henon.b = 0.3f;
    generators.henon.x = 0.1f;      // Initial conditions
    generators.henon.y = 0.1f;
    
    // Initialize Lorenz System
    generators.lorenz.sigma = 10.0f;  // Standard parameters
    generators.lorenz.rho = 28.0f;
    generators.lorenz.beta = 8.0f/3.0f;
    generators.lorenz.x = 1.0f;     // Initial conditions
    generators.lorenz.y = 1.0f;
    generators.lorenz.z = 1.0f;
    generators.lorenz.dt = 0.01f;   // Time step
    
    // Initialize Linear Feedback Shift Register
    generators.lfsr.state = 0xACE1u; // Non-zero initial state
    generators.lfsr.taps = 0xB400u;  // Polynomial: x^16 + x^14 + x^13 + x^11 + 1
    
    // Set default active generator
    generators.activeGenerator = ENTROPY_CHAOS_COMBINED;
    generators.useMultipleSources = true;
    
    // Initialize mixing weights
    for (int i = 0; i < 8; i++) {
        generators.mixingWeight[i] = 1.0f / 8.0f; // Equal weights initially
    }
    
    debugLog("Advanced entropy generators initialized");
}

uint32_t EntropyBeaconApp::generateLCG() {
    generators.lcg.seed = (generators.lcg.a * generators.lcg.seed + generators.lcg.c) % generators.lcg.m;
    return generators.lcg.seed;
}

uint32_t EntropyBeaconApp::generateMersenneTwister() {
    if (!generators.mersenne.initialized) {
        // Initialize generator state
        generators.mersenne.mt[0] = generators.lcg.seed;
        for (int i = 1; i < 624; i++) {
            generators.mersenne.mt[i] = 1812433253 * (generators.mersenne.mt[i-1] ^ (generators.mersenne.mt[i-1] >> 30)) + i;
        }
        generators.mersenne.initialized = true;
        generators.mersenne.index = 0;
    }
    
    if (generators.mersenne.index >= 624) {
        // Generate new batch of 624 values
        for (int i = 0; i < 624; i++) {
            uint32_t y = (generators.mersenne.mt[i] & 0x80000000) + (generators.mersenne.mt[(i + 1) % 624] & 0x7fffffff);
            generators.mersenne.mt[i] = generators.mersenne.mt[(i + 397) % 624] ^ (y >> 1);
            if (y % 2 != 0) {
                generators.mersenne.mt[i] = generators.mersenne.mt[i] ^ 0x9908b0df;
            }
        }
        generators.mersenne.index = 0;
    }
    
    uint32_t y = generators.mersenne.mt[generators.mersenne.index];
    y = y ^ (y >> 11);
    y = y ^ ((y << 7) & 0x9d2c5680);
    y = y ^ ((y << 15) & 0xefc60000);
    y = y ^ (y >> 18);
    
    generators.mersenne.index++;
    return y;
}

float EntropyBeaconApp::generateLogisticMap() {
    // Chaotic logistic map: x_{n+1} = r * x_n * (1 - x_n)
    generators.logistic.x = generators.logistic.r * generators.logistic.x * (1.0f - generators.logistic.x);
    
    // Ensure x stays in [0,1] range
    if (generators.logistic.x < 0.0f) generators.logistic.x = 0.0f;
    if (generators.logistic.x > 1.0f) generators.logistic.x = 1.0f;
    
    return generators.logistic.x;
}

void EntropyBeaconApp::generateHenonMap(float* x, float* y) {
    // Hénon map: x_{n+1} = 1 - a*x_n^2 + y_n, y_{n+1} = b*x_n
    float newX = 1.0f - generators.henon.a * generators.henon.x * generators.henon.x + generators.henon.y;
    float newY = generators.henon.b * generators.henon.x;
    
    generators.henon.x = newX;
    generators.henon.y = newY;
    
    *x = newX;
    *y = newY;
}

void EntropyBeaconApp::generateLorenzSystem(float* x, float* y, float* z) {
    // Lorenz system differential equations (Runge-Kutta integration)
    float dx = generators.lorenz.sigma * (generators.lorenz.y - generators.lorenz.x);
    float dy = generators.lorenz.x * (generators.lorenz.rho - generators.lorenz.z) - generators.lorenz.y;
    float dz = generators.lorenz.x * generators.lorenz.y - generators.lorenz.beta * generators.lorenz.z;
    
    generators.lorenz.x += dx * generators.lorenz.dt;
    generators.lorenz.y += dy * generators.lorenz.dt;
    generators.lorenz.z += dz * generators.lorenz.dt;
    
    *x = generators.lorenz.x;
    *y = generators.lorenz.y;
    *z = generators.lorenz.z;
}

uint32_t EntropyBeaconApp::generateLFSR() {
    // Linear Feedback Shift Register with Galois configuration
    uint32_t lsb = generators.lfsr.state & 1;
    generators.lfsr.state >>= 1;
    if (lsb) {
        generators.lfsr.state ^= generators.lfsr.taps;
    }
    return generators.lfsr.state;
}

uint32_t EntropyBeaconApp::generateChaoticCombined() {
    // Combine multiple chaotic generators for enhanced entropy
    uint32_t lcg = generateLCG();
    uint32_t mt = generateMersenneTwister();
    uint32_t lfsr = generateLFSR();
    
    float logistic = generateLogisticMap();
    float henonX, henonY;
    generateHenonMap(&henonX, &henonY);
    
    float lorenzX, lorenzY, lorenzZ;
    generateLorenzSystem(&lorenzX, &lorenzY, &lorenzZ);
    
    // Convert floating point values to integers
    uint32_t logisticInt = (uint32_t)(logistic * 0xFFFFFFFF);
    uint32_t henonInt = (uint32_t)((henonX + 2.0f) * 0x3FFFFFFF); // Shift and scale
    uint32_t lorenzInt = (uint32_t)((lorenzX + 50.0f) * 0x1FFFFFF); // Scale Lorenz attractor
    
    // XOR all sources together
    return lcg ^ mt ^ lfsr ^ logisticInt ^ henonInt ^ lorenzInt;
}

uint32_t EntropyBeaconApp::mixEntropySources(uint32_t* sources, uint8_t count) {
    if (count == 0) return 0;
    
    uint32_t mixed = 0;
    float totalWeight = 0.0f;
    
    for (uint8_t i = 0; i < count && i < 8; i++) {
        totalWeight += generators.mixingWeight[i];
    }
    
    if (totalWeight > 0.0f) {
        for (uint8_t i = 0; i < count && i < 8; i++) {
            float normalizedWeight = generators.mixingWeight[i] / totalWeight;
            mixed ^= (uint32_t)(sources[i] * normalizedWeight);
        }
    } else {
        // Fallback to simple XOR
        for (uint8_t i = 0; i < count; i++) {
            mixed ^= sources[i];
        }
    }
    
    return mixed;
}

void EntropyBeaconApp::seedGenerators(uint32_t seed) {
    generators.lcg.seed = seed;
    generators.mersenne.initialized = false;
    generators.logistic.x = (float)(seed % 1000) / 1000.0f;
    generators.henon.x = (float)((seed >> 8) % 100) / 100.0f;
    generators.henon.y = (float)((seed >> 16) % 100) / 100.0f;
    generators.lorenz.x = (float)((seed >> 4) % 50) - 25.0f;
    generators.lorenz.y = (float)((seed >> 12) % 50) - 25.0f;
    generators.lorenz.z = (float)((seed >> 20) % 50);
    generators.lfsr.state = seed | 1; // Ensure non-zero
    
    debugLog("Entropy generators reseeded with: " + String(seed, HEX));
}

// ========================================
// MATHEMATICAL ENTROPY ANALYSIS
// ========================================

float EntropyBeaconApp::calculateShannonEntropy(uint16_t* data, uint16_t length) {
    if (length == 0) return 0.0f;
    
    // Count frequency of each value (8-bit binning)
    uint16_t freq[256] = {0};
    for (uint16_t i = 0; i < length; i++) {
        uint8_t bin = data[i] >> 4; // Convert 12-bit to 8-bit
        freq[bin]++;
    }
    
    // Calculate Shannon entropy: H(X) = -Σ p(x) log₂ p(x)
    float entropy = 0.0f;
    for (uint16_t i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            float probability = (float)freq[i] / length;
            entropy -= probability * log2f(probability);
        }
    }
    
    return entropy;
}

float EntropyBeaconApp::calculateConditionalEntropy(uint16_t* data, uint16_t length) {
    if (length < 2) return 0.0f;
    
    // H(X|Y) = H(X,Y) - H(Y)
    // Simplified version using adjacent pairs
    uint16_t jointFreq[16][16] = {0};
    uint16_t marginalFreq[16] = {0};
    uint16_t validPairs = 0;
    
    for (uint16_t i = 0; i < length - 1; i++) {
        uint8_t x = (data[i] >> 8) & 0x0F;     // High 4 bits
        uint8_t y = (data[i+1] >> 8) & 0x0F;  // High 4 bits of next sample
        jointFreq[x][y]++;
        marginalFreq[y]++;
        validPairs++;
    }
    
    if (validPairs == 0) return 0.0f;
    
    // Calculate joint entropy H(X,Y)
    float jointEntropy = 0.0f;
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            if (jointFreq[x][y] > 0) {
                float prob = (float)jointFreq[x][y] / validPairs;
                jointEntropy -= prob * log2f(prob);
            }
        }
    }
    
    // Calculate marginal entropy H(Y)
    float marginalEntropy = 0.0f;
    for (int y = 0; y < 16; y++) {
        if (marginalFreq[y] > 0) {
            float prob = (float)marginalFreq[y] / validPairs;
            marginalEntropy -= prob * log2f(prob);
        }
    }
    
    return jointEntropy - marginalEntropy;
}

float EntropyBeaconApp::calculateMutualInformation(uint16_t* dataX, uint16_t* dataY, uint16_t length) {
    if (length == 0) return 0.0f;
    
    // I(X;Y) = H(X) + H(Y) - H(X,Y)
    float entropyX = calculateShannonEntropy(dataX, length);
    float entropyY = calculateShannonEntropy(dataY, length);
    
    // Calculate joint entropy
    uint16_t jointData[ENTROPY_BUFFER_SIZE];
    for (uint16_t i = 0; i < length; i++) {
        jointData[i] = (dataX[i] >> 1) ^ (dataY[i] >> 1); // Combine for joint distribution
    }
    float jointEntropy = calculateShannonEntropy(jointData, length);
    
    return entropyX + entropyY - jointEntropy;
}

float EntropyBeaconApp::estimateKolmogorovComplexity(uint16_t* data, uint16_t length) {
    if (length == 0) return 0.0f;
    
    // Estimate using compression ratio (approximation of Kolmogorov complexity)
    uint8_t compressedData[ENTROPY_BUFFER_SIZE / 4];
    uint16_t compressedSize = 0;
    
    // Simple run-length encoding
    uint16_t runLength = 1;
    uint16_t currentValue = data[0];
    
    for (uint16_t i = 1; i < length && compressedSize < sizeof(compressedData) - 4; i++) {
        if (data[i] == currentValue && runLength < 255) {
            runLength++;
        } else {
            // Store run
            compressedData[compressedSize++] = (uint8_t)(currentValue >> 4);
            compressedData[compressedSize++] = (uint8_t)runLength;
            currentValue = data[i];
            runLength = 1;
        }
    }
    
    // Store final run
    if (compressedSize < sizeof(compressedData) - 2) {
        compressedData[compressedSize++] = (uint8_t)(currentValue >> 4);
        compressedData[compressedSize++] = (uint8_t)runLength;
    }
    
    float compressionRatio = (float)compressedSize / (length * 2); // 2 bytes per uint16_t
    return -log2f(compressionRatio); // Higher complexity = lower compression ratio
}

float EntropyBeaconApp::calculateCompressionRatio(uint8_t* data, uint16_t length) {
    // Implement simple LZ77-style compression estimate
    uint16_t matches = 0;
    uint16_t windowSize = min(256, length / 4);
    
    for (uint16_t i = windowSize; i < length - 4; i++) {
        // Look for matches in sliding window
        for (uint16_t j = max(0, (int)i - windowSize); j < i; j++) {
            uint16_t matchLength = 0;
            while (j + matchLength < i &&
                   i + matchLength < length &&
                   data[j + matchLength] == data[i + matchLength] &&
                   matchLength < 32) {
                matchLength++;
            }
            if (matchLength >= 3) {
                matches += matchLength;
                i += matchLength - 1; // Skip matched portion
                break;
            }
        }
    }
    
    return 1.0f - ((float)matches / length);
}

float EntropyBeaconApp::performChiSquareTest(uint16_t* data, uint16_t length) {
    if (length == 0) return 0.0f;
    
    // Chi-square test for uniformity (8-bit bins)
    uint16_t freq[256] = {0};
    for (uint16_t i = 0; i < length; i++) {
        uint8_t bin = data[i] >> 4; // Convert to 8-bit
        freq[bin]++;
    }
    
    float expected = (float)length / 256.0f;
    float chiSquare = 0.0f;
    
    for (uint16_t i = 0; i < 256; i++) {
        float diff = freq[i] - expected;
        chiSquare += (diff * diff) / expected;
    }
    
    return chiSquare;
}

void EntropyBeaconApp::calculateSerialCorrelation(uint16_t* data, uint16_t length) {
    // Calculate autocorrelation for various lags
    for (int lag = 1; lag <= 10 && lag < length; lag++) {
        float sum1 = 0.0f, sum2 = 0.0f, sum12 = 0.0f;
        float sum1sq = 0.0f, sum2sq = 0.0f;
        uint16_t validSamples = length - lag;
        
        for (uint16_t i = 0; i < validSamples; i++) {
            float x1 = (float)data[i];
            float x2 = (float)data[i + lag];
            
            sum1 += x1;
            sum2 += x2;
            sum12 += x1 * x2;
            sum1sq += x1 * x1;
            sum2sq += x2 * x2;
        }
        
        float mean1 = sum1 / validSamples;
        float mean2 = sum2 / validSamples;
        
        float numerator = sum12 - validSamples * mean1 * mean2;
        float denominator = sqrt((sum1sq - validSamples * mean1 * mean1) *
                                (sum2sq - validSamples * mean2 * mean2));
        
        analysis.serialCorrelation[lag-1] = (denominator > 0.0f) ? numerator / denominator : 0.0f;
    }
}

float EntropyBeaconApp::calculateSpectralEntropy() {
    // Calculate entropy in frequency domain
    float totalPower = 0.0f;
    for (uint16_t i = 0; i < FFT_SIZE/2; i++) {
        totalPower += spectrumData[i].magnitude * spectrumData[i].magnitude;
    }
    
    if (totalPower == 0.0f) return 0.0f;
    
    float spectralEntropy = 0.0f;
    for (uint16_t i = 0; i < FFT_SIZE/2; i++) {
        float power = spectrumData[i].magnitude * spectrumData[i].magnitude;
        if (power > 0.0f) {
            float probability = power / totalPower;
            spectralEntropy -= probability * log2f(probability);
        }
    }
    
    return spectralEntropy;
}

float EntropyBeaconApp::calculateLyapunovExponent(float* trajectory, uint16_t length) {
    if (length < 10) return 0.0f;
    
    // Simplified Lyapunov exponent calculation
    float sumLogDiv = 0.0f;
    uint16_t validPoints = 0;
    float epsilon = 0.001f; // Small perturbation
    
    for (uint16_t i = 1; i < length - 1; i++) {
        float dx = trajectory[i+1] - trajectory[i];
        float dy = trajectory[i] - trajectory[i-1];
        
        if (abs(dy) > epsilon) {
            float divergence = abs(dx / dy);
            if (divergence > 1.0f) {
                sumLogDiv += logf(divergence);
                validPoints++;
            }
        }
    }
    
    return (validPoints > 0) ? sumLogDiv / validPoints : 0.0f;
}

float EntropyBeaconApp::calculateFractalDimension(uint16_t* data, uint16_t length) {
    if (length < 4) return 0.0f;
    
    // Box-counting method approximation
    uint16_t scales[] = {2, 4, 8, 16, 32};
    float logScales[5], logCounts[5];
    uint8_t validScales = 0;
    
    for (uint8_t s = 0; s < 5 && scales[s] < length; s++) {
        uint16_t scale = scales[s];
        uint16_t boxes = (length + scale - 1) / scale;
        uint16_t filledBoxes = 0;
        
        for (uint16_t box = 0; box < boxes; box++) {
            uint16_t start = box * scale;
            uint16_t end = min(start + scale, length);
            
            uint16_t minVal = data[start];
            uint16_t maxVal = data[start];
            
            for (uint16_t i = start; i < end; i++) {
                minVal = min(minVal, data[i]);
                maxVal = max(maxVal, data[i]);
            }
            
            if (maxVal > minVal) filledBoxes++;
        }
        
        if (filledBoxes > 0) {
            logScales[validScales] = logf(1.0f / scale);
            logCounts[validScales] = logf(filledBoxes);
            validScales++;
        }
    }
    
    if (validScales < 2) return 1.0f;
    
    // Linear regression to find slope (fractal dimension)
    float sumX = 0.0f, sumY = 0.0f, sumXY = 0.0f, sumXX = 0.0f;
    
    for (uint8_t i = 0; i < validScales; i++) {
        sumX += logScales[i];
        sumY += logCounts[i];
        sumXY += logScales[i] * logCounts[i];
        sumXX += logScales[i] * logScales[i];
    }
    
    float denominator = validScales * sumXX - sumX * sumX;
    if (abs(denominator) < 0.001f) return 1.0f;
    
    float slope = (validScales * sumXY - sumX * sumY) / denominator;
    return abs(slope); // Fractal dimension
}

// ========================================
// ADVANCED ANOMALY DETECTION
// ========================================

void EntropyBeaconApp::initializeAdvancedAnomalyDetection() {
    // Initialize Mahalanobis distance parameters
    anomalyDetector.mahalanobisThreshold = 6.0f; // Chi-square threshold for 2D
    for (int i = 0; i < 4; i++) {
        anomalyDetector.covarianceMatrix[i] = (i % 3 == 0) ? 1.0f : 0.0f; // Identity matrix
    }
    
    // Initialize pattern detection
    memset(anomalyDetector.patternBuffer, 0, sizeof(anomalyDetector.patternBuffer));
    anomalyDetector.patternIndex = 0;
    anomalyDetector.repeatedPatterns = 0;
    
    // Initialize temporal anomaly detection
    anomalyDetector.expectedInterval = 1000; // 1ms default
    anomalyDetector.intervalVariance = 0.1f;
    anomalyDetector.timingAnomalies = 0;
    
    // Initialize cross-correlation
    anomalyDetector.crossCorrelationThreshold = 0.8f;
    anomalyDetector.maxCrossCorrelation = 0.0f;
    
    // Initialize clustering
    anomalyDetector.clustering.initialized = false;
    anomalyDetector.clustering.activeCluster = 0;
    
    debugLog("Advanced anomaly detection initialized");
}

float EntropyBeaconApp::calculateMahalanobisDistance(float x, float y) {
    // Simplified 2D Mahalanobis distance
    float meanX = anomalyDetector.mean;
    float meanY = anomalyDetector.mean; // Assume same mean for simplicity
    
    float dx = x - meanX;
    float dy = y - meanY;
    
    // Using diagonal covariance matrix approximation
    float varX = anomalyDetector.variance;
    float varY = anomalyDetector.variance;
    
    if (varX <= 0.0f) varX = 0.01f;
    if (varY <= 0.0f) varY = 0.01f;
    
    float distance = (dx * dx) / varX + (dy * dy) / varY;
    return sqrt(distance);
}

bool EntropyBeaconApp::detectPatternAnomalies(uint16_t value) {
    // Add to pattern buffer
    uint8_t patternValue = value >> 8; // Use high byte
    anomalyDetector.patternBuffer[anomalyDetector.patternIndex] = patternValue;
    anomalyDetector.patternIndex = (anomalyDetector.patternIndex + 1) % 32;
    
    // Look for repeated patterns
    uint8_t patternLength = 4; // Look for 4-byte patterns
    if (anomalyDetector.patternIndex >= patternLength * 2) {
        bool patternMatch = true;
        
        for (uint8_t i = 0; i < patternLength; i++) {
            uint8_t idx1 = (anomalyDetector.patternIndex - patternLength + i) % 32;
            uint8_t idx2 = (anomalyDetector.patternIndex - 2 * patternLength + i) % 32;
            
            if (anomalyDetector.patternBuffer[idx1] != anomalyDetector.patternBuffer[idx2]) {
                patternMatch = false;
                break;
            }
        }
        
        if (patternMatch) {
            anomalyDetector.repeatedPatterns++;
            return true; // Pattern anomaly detected
        }
    }
    
    return false;
}

bool EntropyBeaconApp::detectTemporalAnomalies(unsigned long timestamp) {
    static unsigned long lastTimestamp = 0;
    
    if (lastTimestamp > 0) {
        unsigned long interval = timestamp - lastTimestamp;
        long deviation = (long)interval - (long)anomalyDetector.expectedInterval;
        
        float normalizedDev = abs(deviation) / (float)anomalyDetector.expectedInterval;
        
        if (normalizedDev > 0.5f) { // 50% deviation threshold
            anomalyDetector.timingAnomalies++;
            lastTimestamp = timestamp;
            return true;
        }
        
        // Update expected interval (exponential moving average)
        float alpha = 0.1f;
        anomalyDetector.expectedInterval = (unsigned long)(alpha * interval + (1.0f - alpha) * anomalyDetector.expectedInterval);
    }
    
    lastTimestamp = timestamp;
    return false;
}

float EntropyBeaconApp::calculateCrossCorrelation(uint16_t* data1, uint16_t* data2, uint16_t length) {
    if (length < 2) return 0.0f;
    
    float sum1 = 0.0f, sum2 = 0.0f, sum12 = 0.0f;
    float sum1sq = 0.0f, sum2sq = 0.0f;
    
    for (uint16_t i = 0; i < length; i++) {
        float x1 = (float)data1[i];
        float x2 = (float)data2[i];
        
        sum1 += x1;
        sum2 += x2;
        sum12 += x1 * x2;
        sum1sq += x1 * x1;
        sum2sq += x2 * x2;
    }
    
    float mean1 = sum1 / length;
    float mean2 = sum2 / length;
    
    float numerator = sum12 - length * mean1 * mean2;
    float denominator = sqrt((sum1sq - length * mean1 * mean1) *
                           (sum2sq - length * mean2 * mean2));
    
    return (denominator > 0.0f) ? numerator / denominator : 0.0f;
}

void EntropyBeaconApp::updateClustering(float x, float y) {
    if (!anomalyDetector.clustering.initialized) {
        // Initialize first cluster
        anomalyDetector.clustering.centroids[0][0] = x;
        anomalyDetector.clustering.centroids[0][1] = y;
        anomalyDetector.clustering.clusterRadii[0] = 1.0f;
        anomalyDetector.clustering.initialized = true;
        anomalyDetector.clustering.activeCluster = 1;
        return;
    }
    
    // Find nearest cluster
    float minDistance = FLT_MAX;
    uint8_t nearestCluster = 0;
    
    for (uint8_t i = 0; i < anomalyDetector.clustering.activeCluster; i++) {
        float dx = x - anomalyDetector.clustering.centroids[i][0];
        float dy = y - anomalyDetector.clustering.centroids[i][1];
        float distance = sqrt(dx * dx + dy * dy);
        
        if (distance < minDistance) {
            minDistance = distance;
            nearestCluster = i;
        }
    }
    
    // Update cluster or create new one
    if (minDistance < anomalyDetector.clustering.clusterRadii[nearestCluster] * 2.0f) {
        // Update existing cluster (moving average)
        float alpha = 0.1f;
        anomalyDetector.clustering.centroids[nearestCluster][0] =
            alpha * x + (1.0f - alpha) * anomalyDetector.clustering.centroids[nearestCluster][0];
        anomalyDetector.clustering.centroids[nearestCluster][1] =
            alpha * y + (1.0f - alpha) * anomalyDetector.clustering.centroids[nearestCluster][1];
        
        // Update radius
        anomalyDetector.clustering.clusterRadii[nearestCluster] =
            alpha * minDistance + (1.0f - alpha) * anomalyDetector.clustering.clusterRadii[nearestCluster];
    } else if (anomalyDetector.clustering.activeCluster < 4) {
        // Create new cluster
        uint8_t newCluster = anomalyDetector.clustering.activeCluster++;
        anomalyDetector.clustering.centroids[newCluster][0] = x;
        anomalyDetector.clustering.centroids[newCluster][1] = y;
        anomalyDetector.clustering.clusterRadii[newCluster] = minDistance;
    }
}

bool EntropyBeaconApp::isClusterAnomaly(float x, float y) {
    if (!anomalyDetector.clustering.initialized) return false;
    
    // Check if point is far from all clusters
    for (uint8_t i = 0; i < anomalyDetector.clustering.activeCluster; i++) {
        float dx = x - anomalyDetector.clustering.centroids[i][0];
        float dy = y - anomalyDetector.clustering.centroids[i][1];
        float distance = sqrt(dx * dx + dy * dy);
        
        if (distance < anomalyDetector.clustering.clusterRadii[i] * 3.0f) {
            return false; // Within cluster bounds
        }
    }
    
    return true; // Outside all clusters - anomaly
}

void EntropyBeaconApp::resetStatistics() {
    initializeAnomalyDetector();
    initializeAdvancedAnomalyDetection();
    memset(histogramBins, 0, sizeof(histogramBins));
    viz.samplesRecorded = 0;
    
    // Reset analysis structure
    memset(&analysis, 0, sizeof(analysis));
    
    debugLog("Statistics reset");
}

// ========================================
// SD CARD STORAGE METHODS
// ========================================

void EntropyBeaconApp::writeDataToSD(uint16_t value, float normalized, bool isAnomaly) {
    if (!recordingFile) return;
    
    // Write CSV data: timestamp,raw_value,normalized,anomaly
    recordingFile.print(millis());
    recordingFile.print(",");
    recordingFile.print(value);
    recordingFile.print(",");
    recordingFile.print(normalized, 6);
    recordingFile.print(",");
    recordingFile.println(isAnomaly ? "1" : "0");
    
    // Flush every 10 samples for data integrity
    static uint16_t flushCounter = 0;
    if (++flushCounter >= 10) {
        recordingFile.flush();
        flushCounter = 0;
    }
}

void EntropyBeaconApp::logEventToSD(String eventType, float value) {
    File logFile = SD.open(logPath.c_str(), FILE_APPEND);
    if (logFile) {
        logFile.print(millis());
        logFile.print(" [");
        logFile.print(eventType);
        logFile.print("] Value: ");
        logFile.print(value, 4);
        logFile.print(" Mean: ");
        logFile.print(anomalyDetector.mean, 4);
        logFile.print(" Anomalies: ");
        logFile.println(anomalyDetector.anomalyCount);
        logFile.close();
    }
}

bool EntropyBeaconApp::startRecording() {
    if (recordingFile) {
        recordingFile.close();
    }
    
    // Create unique filename with timestamp
    String filename = "/sd/apps/entropybeacon/entropy_" + String(millis()) + ".csv";
    recordingFile = SD.open(filename.c_str(), FILE_WRITE);
    
    if (recordingFile) {
        // Write CSV header
        recordingFile.println("timestamp,raw_value,normalized,anomaly");
        debugLog("Recording started: " + filename);
        return true;
    }
    
    debugLog("Failed to start recording");
    return false;
}

void EntropyBeaconApp::stopRecording() {
    if (recordingFile) {
        recordingFile.close();
        debugLog("Recording stopped");
    }
}

void EntropyBeaconApp::loadConfiguration() {
    File configFile = SD.open(configPath.c_str(), FILE_READ);
    if (configFile) {
        // Simple configuration loading
        String config = configFile.readString();
        configFile.close();
        
        // Parse basic settings (simple key=value format)
        if (config.indexOf("sampleRate=") >= 0) {
            int start = config.indexOf("sampleRate=") + 11;
            int end = config.indexOf("\n", start);
            if (end > start) {
                int rate = config.substring(start, end).toInt();
                if (rate >= RATE_100HZ && rate <= RATE_8KHZ) {
                    viz.sampleRate = (SampleRate)rate;
                    calculateSampleInterval();
                }
            }
        }
        
        debugLog("Configuration loaded from SD card");
    }
}

void EntropyBeaconApp::saveConfiguration() {
    File configFile = SD.open(configPath.c_str(), FILE_WRITE);
    if (configFile) {
        configFile.println("# EntropyBeacon Configuration");
        configFile.println("sampleRate=" + String(viz.sampleRate));
        configFile.println("threshold=" + String(anomalyDetector.threshold, 2));
        configFile.println("dacEnabled=" + String(dacEnabled ? "1" : "0"));
        configFile.close();
        debugLog("Configuration saved to SD card");
    }
}