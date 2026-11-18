#ifndef FREQ_SCANNER_H
#define FREQ_SCANNER_H

#include "../../core/AppManager/BaseApp.h"
#include "../../core/FileSystem.h"
#include "../../core/Config.h"
#include "../../core/Config/hardware_pins.h"
#include <vector>
#include <complex>

// ========================================
// FreqScanner - Digital Signal Processing and Spectrum Analysis
// Provides real-time FFT processing, waterfall displays, and signal generation
// ========================================

// FFT Configuration constants
#define FFT_SIZE_128     128
#define FFT_SIZE_256     256
#define FFT_SIZE_512     512
#define FFT_SIZE_1024    1024
#define FFT_MAX_SIZE     FFT_SIZE_1024

// Sampling rates (Hz)
#define SAMPLE_RATE_8K   8000
#define SAMPLE_RATE_16K  16000
#define SAMPLE_RATE_22K  22050
#define SAMPLE_RATE_44K  44100
#define DEFAULT_SAMPLE_RATE SAMPLE_RATE_22K

// Window function types
enum FreqScanner_WindowType {
    WINDOW_RECTANGULAR,
    WINDOW_HAMMING,
    WINDOW_BLACKMAN,
    WINDOW_HANNING,
    WINDOW_KAISER
};

// Frequency range presets
enum FrequencyRange {
    RANGE_AUDIO_LOW,      // 20Hz - 2kHz
    RANGE_AUDIO_MID,      // 200Hz - 8kHz
    RANGE_AUDIO_FULL,     // 20Hz - 20kHz
    RANGE_RF_LOW,         // 1MHz - 30MHz
    RANGE_RF_HIGH,        // 30MHz - 300MHz
    RANGE_CUSTOM
};

// Display view modes
enum FreqScanner_ViewMode {
    FREQSCANNER_VIEW_SPECTRUM,        // Real-time spectrum display
    FREQSCANNER_VIEW_WATERFALL,       // Time-frequency waterfall
    FREQSCANNER_VIEW_DUAL,            // Spectrum + waterfall split
    FREQSCANNER_VIEW_RECORDING,       // Recording interface
    FREQSCANNER_VIEW_GENERATOR,       // Signal generator
    FREQSCANNER_VIEW_SETTINGS         // Configuration panel
};

// Touch interaction zones
enum FreqScanner_TouchZone {
    FREQSCANNER_ZONE_NONE,
    FREQSCANNER_ZONE_SPECTRUM_AREA,
    FREQSCANNER_ZONE_WATERFALL_AREA,
    FREQSCANNER_ZONE_FREQUENCY_AXIS,
    FREQSCANNER_ZONE_AMPLITUDE_AXIS,
    FREQSCANNER_ZONE_CONTROL_PANEL,
    FREQSCANNER_ZONE_BACK_BUTTON,
    FREQSCANNER_ZONE_VIEW_TOGGLE,
    FREQSCANNER_ZONE_RANGE_BUTTON,
    FREQSCANNER_ZONE_RECORD_BUTTON,
    FREQSCANNER_ZONE_GENERATOR_BUTTON,
    FREQSCANNER_ZONE_SETTINGS_BUTTON,
    FREQSCANNER_ZONE_ZOOM_IN,
    FREQSCANNER_ZONE_ZOOM_OUT,
    FREQSCANNER_ZONE_MARKER_1,
    FREQSCANNER_ZONE_MARKER_2
};

// Peak detection structure
struct SpectralPeak {
    float frequency;      // Peak frequency in Hz
    float magnitude;      // Peak magnitude in dB
    float phase;          // Peak phase in radians
    uint16_t binIndex;    // FFT bin index
    bool isValid;         // Peak validity flag
    unsigned long timestamp; // When peak was detected
    
    SpectralPeak() : frequency(0), magnitude(-120), phase(0), 
                    binIndex(0), isValid(false), timestamp(0) {}
};

// FFT processing structure
struct FFTProcessor {
    uint16_t size;                    // Current FFT size
    uint32_t sampleRate;              // Sampling rate in Hz
    FreqScanner_WindowType windowType;            // Window function type
    float* inputBuffer;               // Time domain input
    float* windowBuffer;              // Window function coefficients
    std::complex<float>* fftBuffer;   // Complex FFT buffer
    float* magnitudeSpectrum;         // Magnitude spectrum (dB)
    float* phaseSpectrum;             // Phase spectrum (radians)
    float* smoothedSpectrum;          // Smoothed magnitude spectrum
    float binWidth;                   // Frequency resolution (Hz/bin)
    bool isInitialized;               // Initialization status
    
    FFTProcessor() : size(FFT_SIZE_512), sampleRate(DEFAULT_SAMPLE_RATE),
                    windowType(WINDOW_HAMMING), inputBuffer(nullptr),
                    windowBuffer(nullptr), fftBuffer(nullptr),
                    magnitudeSpectrum(nullptr), phaseSpectrum(nullptr),
                    smoothedSpectrum(nullptr), binWidth(0), isInitialized(false) {}
};

// Waterfall display structure
struct WaterfallDisplay {
    uint16_t width;                   // Display width in pixels
    uint16_t height;                  // Display height in pixels
    uint16_t historyDepth;            // Number of time slices to store
    uint16_t** historyBuffer;         // 2D buffer for waterfall data
    uint16_t currentLine;             // Current line being written
    float intensityMin;               // Minimum intensity (dB)
    float intensityMax;               // Maximum intensity (dB)
    uint16_t* colorPalette;           // Color palette for intensity mapping
    uint8_t paletteSize;              // Number of colors in palette
    bool scrollEnabled;               // Enable scrolling display
    float timePerLine;                // Time resolution per line (seconds)
    
    WaterfallDisplay() : width(320), height(120), historyDepth(120),
                        historyBuffer(nullptr), currentLine(0),
                        intensityMin(-100), intensityMax(-20),
                        colorPalette(nullptr), paletteSize(64),
                        scrollEnabled(true), timePerLine(0.1) {}
};

// Signal recording structure
struct SignalRecording {
    String filename;                  // Recording filename
    bool isRecording;                 // Recording status
    bool saveTimeData;                // Save time domain data
    bool saveFreqData;                // Save frequency domain data
    uint32_t maxDuration;             // Maximum recording duration (ms)
    uint32_t recordedSamples;         // Number of samples recorded
    float* timeBuffer;                // Time domain buffer
    float* freqBuffer;                // Frequency domain buffer
    unsigned long startTime;          // Recording start timestamp
    unsigned long lastSaveTime;       // Last file save timestamp
    File dataFile;                    // SD card file handle
    String metadata;                  // Recording metadata (JSON)
    
    SignalRecording() : isRecording(false), saveTimeData(true),
                       saveFreqData(true), maxDuration(60000),
                       recordedSamples(0), timeBuffer(nullptr),
                       freqBuffer(nullptr), startTime(0), lastSaveTime(0) {}
};

// Signal generator structure
struct SignalGenerator {
    enum WaveformType {
        WAVE_SINE,
        WAVE_SQUARE,
        WAVE_TRIANGLE,
        WAVE_SAWTOOTH,
        WAVE_NOISE,
        WAVE_SWEEP,
        WAVE_CUSTOM
    };
    
    enum ModulationType {
        MOD_NONE,
        MOD_AM,           // Amplitude modulation
        MOD_FM,           // Frequency modulation
        MOD_PWM           // Pulse width modulation
    };
    
    WaveformType waveform;            // Current waveform type
    ModulationType modulation;        // Modulation type
    float frequency;                  // Base frequency (Hz)
    float amplitude;                  // Output amplitude (0.0-1.0)
    float phase;                      // Current phase (radians)
    float phaseIncrement;             // Phase increment per sample
    float modFrequency;               // Modulation frequency (Hz)
    float modDepth;                   // Modulation depth (0.0-1.0)
    float sweepStartFreq;             // Sweep start frequency
    float sweepEndFreq;               // Sweep end frequency
    float sweepDuration;              // Sweep duration (seconds)
    bool isEnabled;                   // Generator enable flag
    bool useDac;                      // Use DAC vs PWM output
    uint8_t dacPin;                   // DAC output pin
    uint32_t sampleRate;              // Generation sample rate
    float* customWaveform;            // Custom waveform buffer
    uint16_t customWaveformSize;      // Custom waveform length
    
    SignalGenerator() : waveform(WAVE_SINE), modulation(MOD_NONE),
                       frequency(1000), amplitude(0.5), phase(0),
                       phaseIncrement(0), modFrequency(10), modDepth(0.1),
                       sweepStartFreq(100), sweepEndFreq(2000), sweepDuration(1.0),
                       isEnabled(false), useDac(true), dacPin(DAC_OUT_LEFT),
                       sampleRate(DEFAULT_SAMPLE_RATE), customWaveform(nullptr),
                       customWaveformSize(0) {}
};

// Frequency marker structure
struct FrequencyMarker {
    float frequency;                  // Marker frequency (Hz)
    float magnitude;                  // Magnitude at marker frequency (dB)
    uint16_t color;                   // Marker display color
    bool isEnabled;                   // Marker visibility
    bool isDragging;                  // User is dragging marker
    String label;                     // Marker label text
    
    FrequencyMarker() : frequency(1000), magnitude(-60), color(COLOR_YELLOW),
                       isEnabled(false), isDragging(false), label("") {}
};

// Configuration structure
struct FreqScannerConfig {
    uint16_t fftSize;                 // FFT size
    uint32_t sampleRate;              // Sampling rate
    FreqScanner_WindowType windowType;            // Window function
    FrequencyRange freqRange;         // Frequency range preset
    float customFreqMin;              // Custom range minimum (Hz)
    float customFreqMax;              // Custom range maximum (Hz)
    float smoothingFactor;            // Spectrum smoothing (0.0-1.0)
    float peakThreshold;              // Peak detection threshold (dB)
    uint8_t maxPeaks;                 // Maximum peaks to detect
    bool enablePeakDetection;         // Enable automatic peak detection
    bool enableAveraging;             // Enable spectrum averaging
    uint8_t averagingCount;           // Number of spectra to average
    FreqScanner_ViewMode defaultView;             // Default view mode
    bool autoRecord;                  // Auto-record interesting signals
    String dataDirectory;             // Data storage directory
    
    FreqScannerConfig() : fftSize(FFT_SIZE_512), sampleRate(DEFAULT_SAMPLE_RATE),
                         windowType(WINDOW_HAMMING), freqRange(RANGE_AUDIO_FULL),
                         customFreqMin(20), customFreqMax(20000), smoothingFactor(0.7),
                         peakThreshold(-40), maxPeaks(10), enablePeakDetection(true),
                         enableAveraging(true), averagingCount(4), defaultView(FREQSCANNER_VIEW_SPECTRUM),
                         autoRecord(false), dataDirectory("/data/freqscanner") {}
};

// Statistics structure
struct FreqScannerStats {
    unsigned long totalProcessingTime; // Total FFT processing time (ms)
    uint32_t fftProcessedCount;       // Number of FFTs processed
    uint32_t peaksDetected;           // Total peaks detected
    uint32_t recordingsSaved;         // Number of recordings saved
    float averageNoiseFloor;          // Average noise floor (dB)
    float peakSignalLevel;            // Highest signal detected (dB)
    unsigned long lastResetTime;      // Last statistics reset
    
    FreqScannerStats() : totalProcessingTime(0), fftProcessedCount(0),
                        peaksDetected(0), recordingsSaved(0), averageNoiseFloor(-80),
                        peakSignalLevel(-120), lastResetTime(millis()) {}
};

// UI state structure
struct FreqScanner_UIState {
    FreqScanner_ViewMode currentView;             // Current display view
    TouchPoint lastTouch;             // Last touch coordinates
    unsigned long lastTouchTime;      // Last touch timestamp
    int selectedPeak;                 // Selected peak index (-1 = none)
    bool showGrid;                    // Display frequency/amplitude grid
    bool showMarkers;                 // Display frequency markers
    bool showPeakLabels;              // Display peak frequency labels
    float zoomLevel;                  // Display zoom level
    float panOffsetHz;                // Frequency pan offset
    uint16_t cursorX, cursorY;        // Measurement cursor position
    bool measurementMode;             // Measurement cursor active
    
    FreqScanner_UIState() : currentView(FREQSCANNER_VIEW_SPECTRUM), lastTouchTime(0), selectedPeak(-1),
               showGrid(true), showMarkers(true), showPeakLabels(true),
               zoomLevel(1.0), panOffsetHz(0), cursorX(0), cursorY(0),
               measurementMode(false) {}
};

class FreqScanner : public BaseApp {
private:
    // Core DSP components
    FFTProcessor fftProcessor;
    WaterfallDisplay waterfallDisplay;
    SignalRecording signalRecording;
    SignalGenerator signalGenerator;
    
    // Detection and analysis
    std::vector<SpectralPeak> detectedPeaks;
    FrequencyMarker markers[2];       // Two frequency markers
    float noiseFloor;                 // Current noise floor estimate
    float* averagingBuffer;           // Spectrum averaging buffer
    uint8_t averagingCount;           // Current averaging count
    
    // Configuration and statistics
    FreqScannerConfig config;
    FreqScannerStats stats;
    FreqScanner_UIState uiState;
    
    // Timing and control
    unsigned long lastFFTTime;        // Last FFT processing time
    unsigned long lastDisplayUpdate;  // Last display update time
    unsigned long adcSampleTimer;     // ADC sampling timer
    bool isProcessing;                // FFT processing active
    bool needsRedraw;                 // Display needs update
    
    // Color scheme
    uint16_t colorBackground;
    uint16_t colorGrid;
    uint16_t colorSpectrum;
    uint16_t colorWaterfall;
    uint16_t colorPeaks;
    uint16_t colorMarkers;
    uint16_t colorText;
    
    // File paths
    String configFilePath;
    String recordingsPath;
    String settingsPath;
    
    // ===== CORE FFT PROCESSING METHODS =====
    bool initializeFFT();
    void shutdownFFT();
    bool processFFT();
    void sampleADC();
    void applyWindow();
    void computeFFT();
    void computeMagnitudeSpectrum();
    void computePhaseSpectrum();
    void smoothSpectrum();
    void estimateNoiseFloor();
    
    // ===== WINDOW FUNCTION METHODS =====
    void generateWindow(FreqScanner_WindowType type);
    float hammingWindow(int n, int N);
    float blackmanWindow(int n, int N);
    float hanningWindow(int n, int N);
    float kaiserWindow(int n, int N, float beta);
    
    // ===== PEAK DETECTION METHODS =====
    void detectPeaks();
    bool isPeak(uint16_t binIndex);
    float interpolatePeakFrequency(uint16_t binIndex);
    void sortPeaksByMagnitude();
    void updatePeakHistory();
    
    // ===== WATERFALL DISPLAY METHODS =====
    bool initializeWaterfall();
    void shutdownWaterfall();
    void updateWaterfall();
    void generateColorPalette();
    uint16_t intensityToColor(float intensity);
    void scrollWaterfallHistory();
    
    // ===== SIGNAL RECORDING METHODS =====
    bool startRecording(const String& filename);
    void stopRecording();
    void recordCurrentSpectrum();
    void recordTimeDomainData();
    void recordFrequencyDomainData();
    void saveRecordingMetadata();
    String generateRecordingFilename();
    
    // ===== SIGNAL GENERATION METHODS =====
    bool initializeGenerator();
    void shutdownGenerator();
    void updateGenerator();
    float generateSample();
    float generateSineWave();
    float generateSquareWave();
    float generateTriangleWave();
    float generateSawtoothWave();
    float generateNoise();
    float generateSweep();
    void applyModulation(float& sample);
    void outputToDAC(float sample);
    
    // ===== DISPLAY RENDERING METHODS =====
    void renderSpectrum();
    void renderWaterfall();
    void renderDualView();
    void renderRecordingInterface();
    void renderGeneratorInterface();
    void renderSettingsPanel();
    void renderFrequencyAxis();
    void renderAmplitudeAxis();
    void renderGrid();
    void renderPeaks();
    void renderMarkers();
    void renderStatusBar();
    void renderMeasurementCursor();
    
    // ===== UI HELPER METHODS =====
    void drawSpectrumLine(uint16_t x, float magnitude);
    void drawWaterfallLine(uint16_t y, const float* spectrum);
    void drawPeakMarker(const SpectralPeak& peak);
    void drawFrequencyMarker(const FrequencyMarker& marker);
    void drawFrequencyLabel(float frequency, uint16_t x, uint16_t y);
    void drawAmplitudeLabel(float amplitude, uint16_t x, uint16_t y);
    uint16_t frequencyToPixel(float frequency);
    uint16_t amplitudeToPixel(float amplitude);
    float pixelToFrequency(uint16_t pixel);
    float pixelToAmplitude(uint16_t pixel);
    
    // ===== TOUCH HANDLING METHODS =====
    FreqScanner_TouchZone identifyTouchZone(TouchPoint touch);
    void handleSpectrumTouch(TouchPoint touch);
    void handleWaterfallTouch(TouchPoint touch);
    void handleControlPanelTouch(TouchPoint touch);
    void handleMarkerDrag(TouchPoint touch);
    void handleZoomGesture(TouchPoint touch);
    void handlePanGesture(TouchPoint touch);
    void updateMeasurementCursor(TouchPoint touch);
    void selectPeakNearTouch(TouchPoint touch);
    
    // ===== CONFIGURATION METHODS =====
    void loadConfiguration();
    void saveConfiguration();
    void resetConfiguration();
    void applyConfiguration();
    void updateFrequencyRange();
    void updateFFTSize();
    void updateSampleRate();
    void updateWindowType();
    
    // ===== UTILITY METHODS =====
    String formatFrequency(float frequency);
    String formatAmplitude(float amplitude);
    String formatTime(unsigned long timestamp);
    float dbToLinear(float db);
    float linearToDb(float linear);
    void updateStatistics();
    void resetStatistics();
    bool validateFrequencyRange(float min, float max);
    uint16_t getFrequencyRangeMin();
    uint16_t getFrequencyRangeMax();

public:
    FreqScanner();
    virtual ~FreqScanner();
    
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
    void toggleRecording();
    void toggleGenerator();
    void setFrequencyRange(FrequencyRange range);
    void setFFTSize(uint16_t size);
    void setSampleRate(uint32_t rate);
    void setWindowType(WindowType type);
    void addFrequencyMarker(float frequency);
    void removeFrequencyMarker(uint8_t index);
    SpectralPeak* getPeakAt(float frequency);
    float getMagnitudeAt(float frequency);
    
    // ===== SETTINGS INTERFACE =====
    uint8_t getSettingsCount() const override { return 12; }
    String getSettingName(uint8_t index) const override;
    void handleSetting(uint8_t index) override;
};

// Constants for UI layout
#define SPECTRUM_AREA_X         0
#define SPECTRUM_AREA_Y         20
#define SPECTRUM_AREA_W         320
#define SPECTRUM_AREA_H         120
#define WATERFALL_AREA_X        0
#define WATERFALL_AREA_Y        140
#define WATERFALL_AREA_W        320
#define WATERFALL_AREA_H        80
#define CONTROL_PANEL_H         20
#define FREQUENCY_AXIS_H        20
#define AMPLITUDE_AXIS_W        40
#define GRID_SPACING            20
#define MARKER_WIDTH            2
#define PEAK_MARKER_SIZE        8

// File paths
#define FREQ_SCANNER_DATA_DIR   "/data/freqscanner"
#define FREQ_SCANNER_CONFIG     "/settings/freqscanner.cfg"
#define RECORDINGS_DIR          "/data/freqscanner/recordings"
#define FREQSCANNER_SAMPLES_DIR             "/data/freqscanner/samples"

// Icon data (16x16 pixels, 1-bit per pixel)
extern const uint8_t freq_scanner_icon[32];

#endif // FREQ_SCANNER_H