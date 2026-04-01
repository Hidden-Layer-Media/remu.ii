#ifndef ENTROPY_BEACON_H
#define ENTROPY_BEACON_H

#include "../../core/AppManager/BaseApp.h"
#include "../../core/SystemCore/SystemCore.h"
#include <SD.h>

// ========================================
// EntropyBeacon - Real-time entropy visualization for remu.ii
// Oscilloscope-style display, spectrum analysis, DAC output, anomaly detection
// ========================================

// Visualization modes
enum VisualizationMode {
    VIZ_OSCILLOSCOPE,   // Time domain waveform
    VIZ_SPECTRUM        // Frequency domain analysis
};

// Sample rates
enum SampleRate {
    RATE_100HZ = 100,
    RATE_500HZ = 500,
    RATE_1KHZ = 1000,
    RATE_2KHZ = 2000,
    RATE_5KHZ = 5000,
    RATE_8KHZ = 8000
};

// Buffer sizes
#define ENTROPY_BUFFER_SIZE 256
#define SPECTRUM_BINS 32

// Display configuration
#define GRAPH_WIDTH 280
#define GRAPH_HEIGHT 140
#define GRAPH_X 20
#define GRAPH_Y 40

// Simple anomaly detector
struct AnomalyDetector {
    float mean;            // Running mean
    float variance;        // Running variance
    float threshold;       // Anomaly threshold (stddev multiplier)
    uint32_t anomalyCount; // Total anomalies detected
};

// Visualization state
struct EntropyVisualization {
    VisualizationMode mode;
    SampleRate sampleRate;
    float amplitudeScale;  // Amplitude scaling
    uint16_t traceColor;   // Trace color
};

class EntropyBeaconApp : public BaseApp {
private:
    // Data buffers
    uint16_t entropyBuffer[ENTROPY_BUFFER_SIZE];
    float spectrumData[SPECTRUM_BINS];
    
    // Buffer management
    uint16_t bufferIndex;
    
    // Sampling control
    unsigned long lastSampleTime;
    unsigned long sampleInterval; // Microseconds between samples
    
    // Visualization state
    EntropyVisualization viz;
    AnomalyDetector anomalyDetector;
    
    // DAC output
    bool dacEnabled;
    
    // Recording to SD card
    bool recordingEnabled;
    File recordingFile;
    String recordingPath;
    String configPath;
    String logPath;
    
    // Private methods - Sampling
    void sampleEntropy();
    bool detectAnomaly(float value);
    void updateStatistics(float value);
    void calculateSampleInterval();
    
    // Private methods - Analysis
    void performSimpleFFT();
    
    // Private methods - Visualization
    void drawOscilloscope();
    void drawSpectrum();
    void drawControls();
    
    // Private methods - DAC Output
    void updateDACOutput();
    
    // Private methods - SD Card Storage
    void writeDataToSD(uint16_t value, float normalized, bool isAnomaly);
    void logEventToSD(String eventType, float value);
    bool startRecording();
    void stopRecording();
    void loadConfiguration();
    void saveConfiguration();
    
    // Utility methods
    uint16_t getBufferSize() const;
    float getCurrentEntropy() const;

public:
    EntropyBeaconApp();
    virtual ~EntropyBeaconApp();
    
    // Mandatory BaseApp methods
    bool initialize() override;
    void update() override;
    void render() override;
    bool handleTouch(TouchPoint touch) override;
    void cleanup() override;
    String getName() const override { return "EntropyBeacon"; }
    const uint8_t* getIcon() const override;
    
    // Icon data
    static const uint8_t ENTROPY_ICON[32];
    
    // Utility helpers used internally
    String getAppDataPath() const { return "/data/entropybeacon"; }
    float getFPS() const { return 20.0f; }
    uint32_t getFrameCount() const { return 0; }
    size_t getMemoryUsage() const { return ESP.getFreeHeap(); }
};

#endif // ENTROPY_BEACON_H