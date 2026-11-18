#ifndef CAR_CLONER_H
#define CAR_CLONER_H

#include "../../core/AppManager/BaseApp.h"
#include "../../core/FileSystem.h"
#include "../../core/Config.h"
#include "../../core/Config/hardware_pins.h"
#include <SPI.h>
#include <vector>
#include <map>

// ========================================
// CarCloner - Automotive RF Security Research Tool
// RF signal capture, analysis, and replay for educational purposes
// ========================================

// Maximum limits
#define MAX_CAPTURED_SIGNALS    32
#define MAX_SIGNAL_SAMPLES      8192
#define MAX_SIGNAL_NAME_LENGTH  32
#define MAX_FREQUENCY_PRESETS   16
#define MAX_PROTOCOL_TYPES      8

// RF frequency ranges (MHz)
#define RF_FREQ_315MHZ          315.0
#define RF_FREQ_433MHZ          433.92
#define RF_FREQ_868MHZ          868.0
#define RF_FREQ_915MHZ          915.0

// Signal modulation types
enum CarCloner_ModulationType {
    MOD_ASK,        // Amplitude Shift Keying
    MOD_FSK,        // Frequency Shift Keying  
    MOD_PSK,        // Phase Shift Keying
    MOD_OOK,        // On-Off Keying
    MOD_PWM,        // Pulse Width Modulation
    MOD_MANCHESTER, // Manchester encoding
    MOD_UNKNOWN
};

// Capture modes
enum CarCloner_CaptureMode {
    CAPTURE_SINGLE,     // Single shot capture
    CAPTURE_CONTINUOUS, // Continuous recording
    CAPTURE_TRIGGERED,  // Triggered on signal detection
    CAPTURE_TIMED       // Timed duration capture
};

// Application view states
enum CarCloner_ViewState {
    VIEW_MAIN_MENU,
    VIEW_CAPTURE,
    VIEW_SIGNAL_LIBRARY,
    VIEW_REPLAY,
    VIEW_ANALYSIS,
    VIEW_SETTINGS,
    VIEW_LEGAL_WARNING
};

// Touch interaction zones
enum CarCloner_TouchZone {
    ZONE_NONE,
    ZONE_CAPTURE_BUTTON,
    ZONE_REPLAY_BUTTON,
    ZONE_LIBRARY_BUTTON,
    ZONE_ANALYSIS_BUTTON,
    ZONE_SETTINGS_BUTTON,
    ZONE_BACK_BUTTON,
    ZONE_SIGNAL_LIST,
    ZONE_FREQUENCY_SELECTOR,
    ZONE_POWER_SLIDER,
    ZONE_RECORD_TOGGLE
};

// RF signal sample structure
struct CarCloner_RFSample {
    uint16_t amplitude;     // Signal amplitude (12-bit ADC)
    uint16_t timing;        // Sample timing (microseconds)
    bool digitalLevel;      // Digital interpretation
    
    CarCloner_RFSample() : amplitude(0), timing(0), digitalLevel(false) {}
    CarCloner_RFSample(uint16_t amp, uint16_t time, bool level) : 
        amplitude(amp), timing(time), digitalLevel(level) {}
};

// Complete RF signal structure
struct CarCloner_RFSignal {
    char name[MAX_SIGNAL_NAME_LENGTH];
    float frequency;                    // Carrier frequency in MHz
    CarCloner_ModulationType modulation;          // Modulation type
    uint32_t sampleRate;               // Samples per second
    uint32_t duration;                 // Signal duration in microseconds
    uint16_t sampleCount;              // Number of samples
    CarCloner_RFSample samples[MAX_SIGNAL_SAMPLES];
    
    // Metadata
    unsigned long captureTime;         // When signal was captured
    int8_t captureRSSI;               // Signal strength during capture
    uint32_t pulseCount;              // Number of detected pulses
    uint32_t avgPulseWidth;           // Average pulse width (us)
    uint32_t avgGapWidth;             // Average gap width (us)
    float entropyScore;               // Signal entropy measure
    bool isAnalyzed;                  // Analysis completion flag
    
    // File storage info
    String filePath;                  // SD card file path
    bool isSavedToSD;                // File save status
    
    CarCloner_RFSignal() {
        strcpy(name, "Untitled");
        frequency = RF_FREQ_433MHZ;
        modulation = MOD_UNKNOWN;
        sampleRate = 1000000; // 1 MHz default
        duration = 0;
        sampleCount = 0;
        captureTime = 0;
        captureRSSI = -100;
        pulseCount = 0;
        avgPulseWidth = 0;
        avgGapWidth = 0;
        entropyScore = 0.0;
        isAnalyzed = false;
        isSavedToSD = false;
    }
};

// Protocol detection results
struct CarCloner_ProtocolInfo {
    String protocolName;
    float confidence;       // 0.0 - 1.0 confidence level
    String description;
    std::map<String, String> parameters;
    
    CarCloner_ProtocolInfo() : confidence(0.0) {}
};

// RF hardware configuration
struct CarCloner_RFConfig {
    float frequency;        // Current frequency in MHz
    uint8_t power;         // Transmission power (0-255)
    uint32_t sampleRate;   // ADC sample rate
    uint16_t sensitivity;  // Receiver sensitivity threshold
    bool autoGain;         // Automatic gain control
    CarCloner_CaptureMode captureMode;
    uint32_t captureTimeout; // Timeout for triggered mode (ms)
    
    CarCloner_RFConfig() {
        frequency = RF_FREQ_433MHZ;
        power = 128;  // Medium power
        sampleRate = 1000000; // 1 MHz
        sensitivity = 200;
        autoGain = true;
        captureMode = CAPTURE_SINGLE;
        captureTimeout = 5000; // 5 seconds
    }
};

// Capture session state
struct CarCloner_CaptureSession {
    bool isActive;
    unsigned long startTime;
    unsigned long duration;
    uint32_t samplesCollected;
    float signalStrength;
    bool triggerDetected;
    String statusMessage;
    
    CarCloner_CaptureSession() : isActive(false), startTime(0), duration(0),
        samplesCollected(0), signalStrength(-100.0), triggerDetected(false) {}
};

// Replay session state 
struct CarCloner_ReplaySession {
    bool isActive;
    int selectedSignal;
    uint8_t repeatCount;
    uint8_t remainingRepeats;
    unsigned long lastTransmission;
    uint32_t transmissionInterval; // ms between repeats
    String statusMessage;
    
    CarCloner_ReplaySession() : isActive(false), selectedSignal(-1), repeatCount(1),
        remainingRepeats(0), lastTransmission(0), transmissionInterval(1000) {}
};

// UI state management
struct CarCloner_UIState {
    CarCloner_ViewState currentView;
    CarCloner_ViewState previousView;
    int selectedSignalIndex;
    int scrollOffset;
    bool showLegalWarning;
    bool warningAccepted;
    unsigned long lastUIUpdate;
    TouchPoint lastTouch;
    String statusMessage;
    String alertMessage;
    unsigned long alertTimeout;
    
    CarCloner_UIState() : currentView(VIEW_LEGAL_WARNING), previousView(VIEW_MAIN_MENU),
        selectedSignalIndex(-1), scrollOffset(0), showLegalWarning(true),
        warningAccepted(false), lastUIUpdate(0), alertTimeout(0) {}
};

class CarCloner : public BaseApp {
private:
    // Core RF hardware
    bool rfInitialized;
    CarCloner_RFConfig rfConfig;
    
    // Signal storage
    std::vector<CarCloner_RFSignal> capturedSignals;
    CarCloner_RFSignal currentSignal;
    uint32_t signalCount;
    
    // Session management
    CarCloner_CaptureSession captureState;
    CarCloner_ReplaySession replayState;
    
    // UI and interaction
    CarCloner_UIState uiState;
    
    // File paths
    String dataDirectory;
    String signalDirectory;
    String configFilePath;
    String logFilePath;
    
    // Frequency presets
    float frequencyPresets[MAX_FREQUENCY_PRESETS];
    String frequencyNames[MAX_FREQUENCY_PRESETS];
    uint8_t presetCount;
    
    // Analysis data
    std::vector<CarCloner_ProtocolInfo> detectedProtocols;
    
    // ===== RF HARDWARE METHODS =====
    bool initializeRFHardware();
    void shutdownRFHardware();
    bool setPowerLevel(uint8_t power);
    void setupADCForCapture();
    void setupDACForTransmission();
    bool calibrateRFHardware();
    float getCurrentRSSI();
    
    // ===== SIGNAL CAPTURE METHODS =====
    bool startCapture();
    void stopCapture();
    void updateCapture();
    bool captureRFSample(CarCloner_RFSample& sample);
    bool detectSignalTrigger();
    void processAnalogSample(uint16_t adcValue, uint32_t timestamp);
    void finalizeCapture();
    bool validateCapturedSignal();
    
    // ===== SIGNAL REPLAY METHODS =====
    bool startReplay(int signalIndex);
    void stopReplay();
    void updateReplay();
    bool transmitRFSample(const CarCloner_RFSample& sample);
    bool transmitSignal(const CarCloner_RFSignal& signal);
    void setupTransmissionTiming();
    
    // ===== SIGNAL ANALYSIS METHODS =====
    void analyzeSignal(CarCloner_RFSignal& signal);
    void detectPulses(CarCloner_RFSignal& signal);
    void calculateTiming(CarCloner_RFSignal& signal);
    float calculateEntropy(const CarCloner_RFSignal& signal);
    CarCloner_ModulationType identifyModulation(const CarCloner_RFSignal& signal);
    void detectProtocols(CarCloner_RFSignal& signal);
    CarCloner_ProtocolInfo analyzeFixedCodeProtocol(const CarCloner_RFSignal& signal);
    CarCloner_ProtocolInfo analyzeRollingCodeProtocol(const CarCloner_RFSignal& signal);
    CarCloner_ProtocolInfo analyzePWMProtocol(const CarCloner_RFSignal& signal);
    
    // ===== FILE SYSTEM METHODS =====
    bool initializeFSStructure();
    bool saveSignal(CarCloner_RFSignal& signal);
    bool loadSignal(const String& filename, CarCloner_RFSignal& signal);
    void loadSignalLibrary();
    void saveConfiguration();
    void loadConfiguration();
    String generateSignalFilename(const CarCloner_RFSignal& signal);
    
    // ===== UI RENDERING METHODS =====
    void renderLegalWarning();
    void renderMainMenu();
    void renderCaptureView();
    void renderSignalLibrary();
    void renderReplayView();
    void renderAnalysisView();
    void renderSettingsView();
    void renderStatusBar();
    void renderFrequencyDisplay();
    void renderSignalStrength();
    void renderProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, float progress);
    
    // ===== UI HELPER METHODS =====
    void drawSignalEntry(int16_t y, const CarCloner_RFSignal& signal, bool selected);
    void drawFrequencySelector();
    void drawPowerSlider();
    void drawCaptureControls();
    void drawReplayControls();
    void drawSignalWaveform(int16_t x, int16_t y, int16_t w, int16_t h, const CarCloner_RFSignal& signal);
    void drawAnalysisResults();
    uint16_t getSignalColor(const CarCloner_RFSignal& signal);
    String formatFrequency(float frequency);
    String formatDuration(uint32_t microseconds);
    String formatFileSize(size_t bytes);
    
    // ===== TOUCH HANDLING METHODS =====
    CarCloner_TouchZone identifyTouchZone(TouchPoint touch);
    void handleMainMenuTouch(TouchPoint touch);
    void handleCaptureTouch(TouchPoint touch);
    void handleLibraryTouch(TouchPoint touch);
    void handleReplayTouch(TouchPoint touch);
    void handleAnalysisTouch(TouchPoint touch);
    void handleSettingsTouch(TouchPoint touch);
    void handleLegalWarningTouch(TouchPoint touch);
    
    // ===== SAFETY AND LEGAL METHODS =====
    void showLegalDisclaimer();
    bool confirmTransmission();
    void logActivity(const String& activity);
    bool checkTransmissionLegality(float frequency, uint8_t power);
    void displaySafetyWarning(const String& warning);
    void enforceTransmissionLimits();
    
    // ===== UTILITY METHODS =====
    void initializeFrequencyPresets();
    void cleanupOldSignals();
    bool isValidFrequency(float frequency);
    String getModulationString(CarCloner_ModulationType mod);
    void updateStatusMessage(const String& message);
    void showAlert(const String& message, uint32_t duration = 3000);
    void debugPrintSignal(const CarCloner_RFSignal& signal);
    
    // ===== ERROR HANDLING =====
    void handleRFError(const String& error);
    void handleFileSystemError(const String& error);
    void handleMemoryError();
    
public:
    CarCloner();
    virtual ~CarCloner();
    
    // ===== BaseApp INTERFACE IMPLEMENTATION =====
    bool initialize() override;
    void update() override;
    void render() override;
    bool handleTouch(TouchPoint touch) override;
    void cleanup() override;
    String getName() const override;
    const uint8_t* getIcon() const override;
    
    // ===== BaseApp OPTIONAL OVERRIDES =====
    void onPause() override;
    void onResume() override;
    bool saveState() override;
    bool loadState() override;
    bool handleMessage(AppMessage message, void* data = nullptr) override;
    
    // ===== PUBLIC INTERFACE =====
    bool isCapturing() const { return captureState.isActive; }
    bool isReplaying() const { return replayState.isActive; }
    uint32_t getSignalCount() const { return signalCount; }
    float getCurrentFrequency() const { return rfConfig.frequency; }
    
    // ===== SIGNAL MANAGEMENT =====
    bool captureSignal(const String& name = "");
    bool replaySignal(int index, uint8_t repeatCount = 1);
    bool deleteSignal(int index);
    CarCloner_RFSignal getSignal(int index) const;
    void exportSignalData(int index, const String& format);
    
    // ===== CONFIGURATION =====
    void setFrequency(float freq) { rfConfig.frequency = freq; }
    void setPower(uint8_t power) { rfConfig.power = power; }
    void setCaptureMode(CarCloner_CaptureMode mode) { rfConfig.captureMode = mode; }
    CarCloner_RFConfig getConfiguration() const { return rfConfig; }
    
    // ===== SETTINGS INTERFACE =====
    uint8_t getSettingsCount() const override { return 7; }
    String getSettingName(uint8_t index) const override;
    void handleSetting(uint8_t index) override;
    
    // ===== DEBUG METHODS =====
    void runRFTest();
    void printRFStatus();
    void printSignalLibrary();
};

// Constants for UI layout
#define SIGNAL_LIST_ITEM_HEIGHT     28
#define SIGNAL_LIST_MAX_VISIBLE     7
#define HEADER_HEIGHT              24
#define STATUS_BAR_HEIGHT          20
#define BUTTON_WIDTH               80
#define MARGIN                     8
#define WAVEFORM_HEIGHT            60

// File paths and directories
#define CAR_CLONER_DATA_DIR        "/data/carcloner"
#define CAR_CLONER_SIGNALS_DIR     "/data/carcloner/signals"
#define CAR_CLONER_CONFIG_FILE     "/settings/carcloner.cfg"
#define CAR_CLONER_LOG_FILE        "/logs/carcloner.log"

// Legal and safety constants
#define LEGAL_WARNING_TIMEOUT      10000  // 10 seconds minimum display
#define MAX_TRANSMISSION_TIME      30000  // 30 seconds max continuous transmission
#define POWER_LIMIT_DEFAULT        50     // Conservative power limit (0-255)

// Icon data (16x16 pixels, 1-bit per pixel)
extern const uint8_t car_cloner_icon[32];

#endif // CAR_CLONER_H