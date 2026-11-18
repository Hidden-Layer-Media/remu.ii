#ifndef SEQUENCER_H
#define SEQUENCER_H

#include "../../core/AppManager/BaseApp.h"
#include "../../core/SystemCore/SystemCore.h"
#include <ArduinoJson.h>
#include <SD.h>

// ========================================
// Sequencer - Beat-making engine for remu.ii
// Model:Samples-inspired interface with glitch/ambient bias
// ========================================

// Grid configuration
#define SEQUENCER_COLS 16
#define SEQUENCER_ROWS 8
#define MAX_TRACKS 8
#define MAX_PATTERNS 16
#define MAX_SAMPLES 32

// Audio configuration
#define SAMPLE_RATE 22050
#define AUDIO_BUFFER_SIZE 512
#define MAX_SAMPLE_LENGTH 44100  // 2 seconds at 22kHz

// Grid cell states
enum CellState {
    CELL_OFF,
    CELL_ON,
    CELL_ACCENT,    // Louder/emphasized hit
    CELL_GHOST,     // Quieter hit
    CELL_PLAYING    // Currently playing (visual feedback)
};

// Sequencer modes
enum SequencerMode {
    MODE_PATTERN,   // Pattern editing
    MODE_SONG,      // Song arrangement
    MODE_PERFORM,   // Live performance
    MODE_SAMPLE     // Sample management
};

// Track data
struct Track {
    String name;
    String samplePath;
    uint8_t volume;        // 0-127
    int8_t pitch;          // -12 to +12 semitones
    uint8_t pan;           // 0-127 (64 = center)
    bool muted;
    bool solo;
    CellState steps[SEQUENCER_COLS];
    uint16_t sampleData[MAX_SAMPLE_LENGTH];
    uint16_t sampleLength;
    bool sampleLoaded;
};

// Pattern data
struct Pattern {
    String name;
    uint8_t bpm;
    uint8_t swing;         // 0-100 (50 = no swing)
    uint8_t length;        // Pattern length in steps (1-16)
    Track tracks[MAX_TRACKS];
    bool isEmpty;
};

// Song arrangement
struct SongStep {
    uint8_t patternIndex;
    uint8_t repeatCount;
};

struct Song {
    String name;
    SongStep steps[64];
    uint8_t stepCount;
    uint8_t currentStep;
};

// Touch grid cell
struct GridCell {
    int16_t x, y, w, h;
    uint8_t track, step;
    CellState state;
    bool highlighted;
};

// UI state
struct SequencerUI {
    SequencerMode mode;
    uint8_t selectedTrack;
    uint8_t selectedPattern;
    uint8_t currentStep;
    bool isPlaying;
    bool isRecording;
    unsigned long lastStepTime;
    unsigned long stepDuration;  // ms per step based on BPM
    
    // Grid display
    GridCell grid[MAX_TRACKS][SEQUENCER_COLS];
    int16_t gridStartX, gridStartY;
    int16_t cellWidth, cellHeight;
    
    // Controls
    bool showControls;
    uint8_t selectedControl;
};

class SequencerApp : public BaseApp {
private:
    // Core data
    Pattern patterns[MAX_PATTERNS];
    Song currentSong;
    SequencerUI ui;
    String projectPath;
    
    // Audio engine
    unsigned long nextStepTime;
    uint8_t playingStep;
    bool audioInitialized;
    
    // Sample management
    String samplePaths[MAX_SAMPLES];
    uint8_t loadedSamples;
    
    // Private methods - Sequencer Engine
    void updateSequencer();
    void playStep(uint8_t step);
    void triggerSample(uint8_t track, uint8_t velocity = 127);
    void calculateStepTiming();
    void handleSwing();
    
    // Private methods - Audio System
    bool initializeAudio();
    bool loadSample(uint8_t track, String samplePath);
    void playSample(uint16_t* sampleData, uint16_t length, uint8_t volume = 127);
    void generateTone(uint16_t frequency, uint16_t duration); // For built-in sounds
    
    // Private methods - Pattern Management
    void clearPattern(uint8_t patternIndex);
    void copyPattern(uint8_t src, uint8_t dest);
    Pattern* getCurrentPattern() { return &patterns[ui.selectedPattern]; }
    Track* getCurrentTrack() { return &patterns[ui.selectedPattern].tracks[ui.selectedTrack]; }
    
    // Private methods - Grid Interface
    void setupGrid();
    void updateGrid();
    void drawGrid();
    void drawGridCell(uint8_t track, uint8_t step);
    GridCell* getTouchedCell(TouchPoint touch);
    void handleCellTouch(GridCell* cell);
    void animateStep(uint8_t step);
    
    // Private methods - UI Rendering
    void drawSequencerInterface();
    void drawTransportControls();
    void drawTrackInfo();
    void drawPatternSelector();
    void drawBPMControl();
    void drawVolumeSliders();
    void drawSampleBrowser();
    void drawPerformanceMode();
    
    // Private methods - Controls
    void handleTransportTouch(TouchPoint touch);
    void handleControlTouch(TouchPoint touch);
    void togglePlayback();
    void stopPlayback();
    void recordToggle();
    
    // Private methods - File I/O
    bool loadProject(String projectName);
    bool saveProject(String projectName);
    bool loadSampleLibrary();
    bool exportPattern(uint8_t patternIndex);
    
    // Private methods - Built-in Samples
    void generateBuiltinSamples();
    void generateKickSample(uint16_t* buffer, uint16_t length);
    void generateSnareSample(uint16_t* buffer, uint16_t length);
    void generateHihatSample(uint16_t* buffer, uint16_t length);
    void generateBassSample(uint16_t* buffer, uint16_t length);
    
    // Private methods - Effects
    void applyDistortion(uint16_t* buffer, uint16_t length, uint8_t amount);
    void applyBitcrush(uint16_t* buffer, uint16_t length, uint8_t bits);
    void applyDelay(uint16_t* buffer, uint16_t length, uint16_t delayTime);
    void applyPitchShift(uint16_t* buffer, uint16_t length, int8_t semitones);
    
    // UI layout constants
    static const int16_t GRID_MARGIN = 10;
    static const int16_t CELL_SIZE = 16;
    static const int16_t CELL_SPACING = 2;
    static const int16_t TRANSPORT_HEIGHT = 30;
    static const int16_t TRACK_INFO_WIDTH = 40;

public:
    SequencerApp();
    virtual ~SequencerApp();
    
    // Mandatory BaseApp methods
    bool initialize() override;
    void update() override;
    void render() override;
    bool handleTouch(TouchPoint touch) override;
    void cleanup() override;
    String getName() const override { return "Sequencer"; }
    const uint8_t* getIcon() const override;
    
    // Optional BaseApp methods
    void onPause() override;
    void onResume() override;
    bool saveState() override;
    bool loadState() override;
    bool handleMessage(AppMessage message) override;
    
    // Settings menu support
    uint8_t getSettingsCount() const override { return 5; }
    String getSettingName(uint8_t index) const override;
    void handleSetting(uint8_t index) override;
    
    // Sequencer-specific methods
    void setBPM(uint8_t bpm);
    uint8_t getBPM() const { return patterns[ui.selectedPattern].bpm; }
    void setSwing(uint8_t swing);
    uint8_t getSwing() const { return patterns[ui.selectedPattern].swing; }
    
    bool isPlaying() const { return ui.isPlaying; }
    bool isRecording() const { return ui.isRecording; }
    uint8_t getCurrentStep() const { return ui.currentStep; }
    uint8_t getSelectedTrack() const { return ui.selectedTrack; }
    uint8_t getSelectedPattern() const { return ui.selectedPattern; }
    
    // Pattern operations
    void selectPattern(uint8_t patternIndex);
    void selectTrack(uint8_t trackIndex);
    void toggleStep(uint8_t track, uint8_t step);
    void setStepVelocity(uint8_t track, uint8_t step, uint8_t velocity);
    void clearTrack(uint8_t trackIndex);
    void muteTrack(uint8_t trackIndex, bool mute);
    void soloTrack(uint8_t trackIndex, bool solo);
    
    // Sample operations
    bool loadSampleForTrack(uint8_t trackIndex, String samplePath);
    void assignBuiltinSample(uint8_t trackIndex, String sampleType);
    
    // Debug methods
    void debugPrintPattern();
    void debugTriggerStep(uint8_t step);
    void debugGenerateRandomPattern();
    
    // Icon data
    static const uint8_t SEQUENCER_ICON[32];
};

#endif // SEQUENCER_H