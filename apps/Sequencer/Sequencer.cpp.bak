#include "Sequencer.h"

// ========================================
// ICON DATA
// ========================================

const uint8_t SequencerApp::SEQUENCER_ICON[32] = {
    0xFF, 0xFF, 0x80, 0x01, 0x9D, 0xB9, 0x80, 0x01, 0x9D, 0xB9, 0x80, 0x01,
    0x9D, 0xB9, 0x80, 0x01, 0x9D, 0xB9, 0x80, 0x01, 0x9D, 0xB9, 0x80, 0x01,
    0x9D, 0xB9, 0x80, 0x01, 0xFF, 0xFF, 0x00, 0x00
};

// ========================================
// CONSTRUCTOR / DESTRUCTOR
// ========================================

SequencerApp::SequencerApp() :
    loadedSamples(0),
    nextStepTime(0),
    playingStep(0),
    audioInitialized(false)
{
    // Set app metadata
    metadata.name = "Sequencer";
    metadata.version = "1.0";
    metadata.author = "remu.ii";
    metadata.description = "8-track beat sequencer";
    metadata.category = CATEGORY_MEDIA;
    metadata.memoryRequirement = 25000; // 25KB for audio buffers
    metadata.requiresSD = true;
    metadata.requiresBLE = false;
    metadata.requiresWiFi = false;
    
    // Initialize project path
    projectPath = "/apps/Sequencer/projects/";
    
    // Set colors
    backgroundColor = COLOR_BLACK;
    foregroundColor = COLOR_GREEN_PHOS;
    
    showBackButton = true;
    showStatusBar = true;
    
    // Initialize UI state
    ui.mode = MODE_PATTERN;
    ui.selectedTrack = 0;
    ui.selectedPattern = 0;
    ui.currentStep = 0;
    ui.isPlaying = false;
    ui.isRecording = false;
    ui.lastStepTime = 0;
    ui.stepDuration = 500; // 120 BPM default
    ui.showControls = true;
    ui.selectedControl = 0;
}

SequencerApp::~SequencerApp() {
    cleanup();
}

// ========================================
// MANDATORY BASEAPP METHODS
// ========================================

bool SequencerApp::initialize() {
    debugLog("Sequencer initializing...");
    
    setState(APP_INITIALIZING);
    
    // Create app data directory
    if (!createAppDataDir()) {
        debugLog("WARNING: Could not create app data directory");
    }
    
    // Initialize patterns with defaults
    for (uint8_t i = 0; i < MAX_PATTERNS; i++) {
        clearPattern(i);
        patterns[i].name = "Pattern " + String(i + 1);
        patterns[i].bpm = 120;
        patterns[i].swing = 50;
        patterns[i].length = 16;
    }
    
    // Initialize current song
    currentSong.name = "New Song";
    currentSong.stepCount = 1;
    currentSong.currentStep = 0;
    currentSong.steps[0] = {0, 1}; // Start with pattern 0, repeat once
    
    // Setup grid layout
    setupGrid();
    
    // Generate built-in samples
    generateBuiltinSamples();
    
    // Initialize audio system
    if (!initializeAudio()) {
        debugLog("WARNING: Audio initialization failed");
    }
    
    // Load sample library
    loadSampleLibrary();
    
    // Calculate initial step timing
    calculateStepTiming();
    
    setState(APP_RUNNING);
    debugLog("Sequencer initialized successfully");
    
    return true;
}

void SequencerApp::update() {
    if (currentState != APP_RUNNING) return;
    
    unsigned long currentTime = millis();
    
    // Update sequencer playback
    if (ui.isPlaying) {
        updateSequencer();
    }
    
    // Update grid animation
    updateGrid();
    
    frameCount++;
}

void SequencerApp::render() {
    if (currentState != APP_RUNNING) return;
    
    // Clear screen
    displayManager.clearScreen(backgroundColor);
    
    switch (ui.mode) {
        case MODE_PATTERN:
            drawSequencerInterface();
            break;
        case MODE_SONG:
            // Song arrangement view (simplified for now)
            drawSequencerInterface();
            break;
        case MODE_PERFORM:
            drawPerformanceMode();
            break;
        case MODE_SAMPLE:
            drawSampleBrowser();
            break;
    }
    
    // Draw common UI elements
    drawCommonUI();
}

bool SequencerApp::handleTouch(TouchPoint touch) {
    // Handle common UI first
    if (handleCommonTouch(touch)) {
        return true;
    }
    
    if (!touch.isNewPress) return false;
    
    // Check transport controls
    if (touch.y >= SCREEN_HEIGHT - TRANSPORT_HEIGHT) {
        handleTransportTouch(touch);
        return true;
    }
    
    // Check grid area
    GridCell* cell = getTouchedCell(touch);
    if (cell != nullptr) {
        handleCellTouch(cell);
        return true;
    }
    
    // Check control areas
    if (ui.showControls) {
        handleControlTouch(touch);
        return true;
    }
    
    return false;
}

void SequencerApp::cleanup() {
    // Stop playback
    ui.isPlaying = false;
    
    // Save current project
    saveProject("autosave");
    
    debugLog("Sequencer cleanup complete");
}

const uint8_t* SequencerApp::getIcon() const {
    return SEQUENCER_ICON;
}

// ========================================
// OPTIONAL BASEAPP METHODS
// ========================================

void SequencerApp::onPause() {
    // Stop playback when pausing
    if (ui.isPlaying) {
        stopPlayback();
    }
    
    // Save current state
    saveProject("autosave");
}

void SequencerApp::onResume() {
    // Recalculate timing in case system time changed
    calculateStepTiming();
}

bool SequencerApp::saveState() {
    return saveProject("autosave");
}

bool SequencerApp::loadState() {
    return loadProject("autosave");
}

String SequencerApp::getSettingName(uint8_t index) const {
    switch (index) {
        case 0: return "Load Project";
        case 1: return "Save Project";
        case 2: return "Clear Pattern";
        case 3: return "Sample Browser";
        case 4: return "Export Audio";
        default: return "";
    }
}

void SequencerApp::handleSetting(uint8_t index) {
    switch (index) {
        case 0: // Load Project
            // Would show file browser
            debugLog("Load project selected");
            break;
        case 1: // Save Project
            saveProject("user_project");
            break;
        case 2: // Clear Pattern
            clearPattern(ui.selectedPattern);
            break;
        case 3: // Sample Browser
            ui.mode = MODE_SAMPLE;
            break;
        case 4: // Export Audio
            exportPattern(ui.selectedPattern);
            break;
    }
}

// ========================================
// SEQUENCER ENGINE
// ========================================

void SequencerApp::updateSequencer() {
    unsigned long currentTime = millis();
    
    if (currentTime >= nextStepTime) {
        // Play current step
        playStep(ui.currentStep);
        
        // Animate step
        animateStep(ui.currentStep);
        
        // Advance to next step
        ui.currentStep = (ui.currentStep + 1) % getCurrentPattern()->length;
        
        // Calculate next step time with swing
        calculateStepTiming();
        nextStepTime = currentTime + ui.stepDuration;
        
        // Apply swing to odd steps
        if (ui.currentStep % 2 == 1) {
            handleSwing();
        }
        
        ui.lastStepTime = currentTime;
    }
}

void SequencerApp::playStep(uint8_t step) {
    Pattern* pattern = getCurrentPattern();
    
    for (uint8_t track = 0; track < MAX_TRACKS; track++) {
        Track* t = &pattern->tracks[track];
        
        // Skip muted tracks
        if (t->muted) continue;
        
        // Check if any tracks are soloed
        bool hasSolo = false;
        for (uint8_t i = 0; i < MAX_TRACKS; i++) {
            if (pattern->tracks[i].solo) {
                hasSolo = true;
                break;
            }
        }
        
        // If tracks are soloed, only play soloed tracks
        if (hasSolo && !t->solo) continue;
        
        // Check if step is active
        CellState cellState = t->steps[step];
        if (cellState != CELL_OFF) {
            // Calculate velocity based on cell state
            uint8_t velocity = t->volume;
            switch (cellState) {
                case CELL_ON:
                    velocity = t->volume;
                    break;
                case CELL_ACCENT:
                    velocity = min(127, (int)t->volume + 20);
                    break;
                case CELL_GHOST:
                    velocity = max(20, (int)t->volume - 30);
                    break;
                default:
                    break;
            }
            
            triggerSample(track, velocity);
        }
    }
}

void SequencerApp::triggerSample(uint8_t track, uint8_t velocity) {
    Pattern* pattern = getCurrentPattern();
    Track* t = &pattern->tracks[track];
    
    if (!t->sampleLoaded || t->sampleLength == 0) {
        // Generate tone as fallback
        uint16_t frequency = 220 + (track * 55); // Different frequency per track
        generateTone(frequency, 100);
        return;
    }
    
    // Play the loaded sample
    playSample(t->sampleData, t->sampleLength, velocity);
}

void SequencerApp::calculateStepTiming() {
    Pattern* pattern = getCurrentPattern();
    // Convert BPM to milliseconds per 16th note
    ui.stepDuration = (60000 / pattern->bpm) / 4; // 4 steps per beat
}

void SequencerApp::handleSwing() {
    Pattern* pattern = getCurrentPattern();
    if (pattern->swing != 50) {
        // Apply swing by adjusting timing of odd steps
        float swingFactor = (pattern->swing - 50) / 50.0f; // -1 to +1
        ui.stepDuration += (ui.stepDuration * swingFactor * 0.2f); // Max 20% timing adjustment
    }
}

// ========================================
// AUDIO SYSTEM
// ========================================

bool SequencerApp::initializeAudio() {
    // Initialize DAC for audio output
    // Note: ESP32 has built-in DAC on pins 25 and 26
    pinMode(25, OUTPUT); // DAC1
    
    audioInitialized = true;
    debugLog("Audio system initialized");
    return true;
}

bool SequencerApp::loadSample(uint8_t track, String samplePath) {
    if (track >= MAX_TRACKS) return false;
    
    Track* t = &getCurrentPattern()->tracks[track];
    
    if (!SD.exists(samplePath)) {
        debugLog("Sample file not found: " + samplePath);
        return false;
    }
    
    File file = SD.open(samplePath, FILE_READ);
    if (!file) {
        debugLog("Failed to open sample file");
        return false;
    }
    
    // Read sample data (simplified - assumes raw 16-bit audio)
    size_t bytesRead = file.read((uint8_t*)t->sampleData, 
                                 min((size_t)MAX_SAMPLE_LENGTH * 2, file.size()));
    t->sampleLength = bytesRead / 2; // Convert bytes to 16-bit samples
    t->sampleLoaded = true;
    t->samplePath = samplePath;
    
    file.close();
    
    debugLog("Loaded sample for track " + String(track) + ": " + samplePath);
    return true;
}

void SequencerApp::playSample(uint16_t* sampleData, uint16_t length, uint8_t volume) {
    if (!audioInitialized || !sampleData || length == 0) return;
    
    // Simple audio playback via DAC
    // Note: This is a simplified implementation
    // Real implementation would use DMA and interrupts
    for (uint16_t i = 0; i < min(length, (uint16_t)100); i++) {
        uint16_t sample = (sampleData[i] * volume) / 127;
        dacWrite(25, sample >> 8); // Output to DAC (8-bit)
        delayMicroseconds(45); // ~22kHz sample rate
    }
}

void SequencerApp::generateTone(uint16_t frequency, uint16_t duration) {
    if (!audioInitialized) return;
    
    // Generate simple square wave tone
    uint16_t period = 1000000 / frequency; // Period in microseconds
    uint16_t halfPeriod = period / 2;
    uint32_t endTime = micros() + (duration * 1000);
    
    while (micros() < endTime) {
        dacWrite(25, 200); // High
        delayMicroseconds(halfPeriod);
        dacWrite(25, 55);  // Low
        delayMicroseconds(halfPeriod);
    }
}

// ========================================
// PATTERN MANAGEMENT
// ========================================

void SequencerApp::clearPattern(uint8_t patternIndex) {
    if (patternIndex >= MAX_PATTERNS) return;
    
    Pattern* pattern = &patterns[patternIndex];
    
    for (uint8_t track = 0; track < MAX_TRACKS; track++) {
        Track* t = &pattern->tracks[track];
        
        // Clear steps
        for (uint8_t step = 0; step < SEQUENCER_COLS; step++) {
            t->steps[step] = CELL_OFF;
        }
        
        // Reset track settings
        t->name = "Track " + String(track + 1);
        t->volume = 100;
        t->pitch = 0;
        t->pan = 64;
        t->muted = false;
        t->solo = false;
        t->sampleLoaded = false;
        t->sampleLength = 0;
        t->samplePath = "";
    }
    
    pattern->isEmpty = true;
    debugLog("Cleared pattern " + String(patternIndex));
}

void SequencerApp::copyPattern(uint8_t src, uint8_t dest) {
    if (src >= MAX_PATTERNS || dest >= MAX_PATTERNS) return;
    
    patterns[dest] = patterns[src];
    patterns[dest].name = "Copy of " + patterns[src].name;
    
    debugLog("Copied pattern " + String(src) + " to " + String(dest));
}

// ========================================
// GRID INTERFACE
// ========================================

void SequencerApp::setupGrid() {
    // Calculate grid layout
    ui.gridStartX = TRACK_INFO_WIDTH + GRID_MARGIN;
    ui.gridStartY = GRID_MARGIN * 2;
    ui.cellWidth = CELL_SIZE;
    ui.cellHeight = CELL_SIZE;
    
    // Initialize grid cells
    for (uint8_t track = 0; track < MAX_TRACKS; track++) {
        for (uint8_t step = 0; step < SEQUENCER_COLS; step++) {
            GridCell* cell = &ui.grid[track][step];
            cell->x = ui.gridStartX + step * (ui.cellWidth + CELL_SPACING);
            cell->y = ui.gridStartY + track * (ui.cellHeight + CELL_SPACING);
            cell->w = ui.cellWidth;
            cell->h = ui.cellHeight;
            cell->track = track;
            cell->step = step;
            cell->state = CELL_OFF;
            cell->highlighted = false;
        }
    }
}

void SequencerApp::updateGrid() {
    Pattern* pattern = getCurrentPattern();
    
    // Update grid cell states from pattern data
    for (uint8_t track = 0; track < MAX_TRACKS; track++) {
        for (uint8_t step = 0; step < SEQUENCER_COLS; step++) {
            GridCell* cell = &ui.grid[track][step];
            cell->state = pattern->tracks[track].steps[step];
            
            // Highlight current step
            cell->highlighted = (step == ui.currentStep && ui.isPlaying);
        }
    }
}

void SequencerApp::drawGrid() {
    Pattern* pattern = getCurrentPattern();
    
    // Draw track labels
    displayManager.setFont(FONT_SMALL);
    for (uint8_t track = 0; track < MAX_TRACKS; track++) {
        int16_t labelY = ui.gridStartY + track * (ui.cellHeight + CELL_SPACING) + 2;
        String trackLabel = "T" + String(track + 1);
        
        uint16_t labelColor = COLOR_WHITE;
        if (track == ui.selectedTrack) {
            labelColor = COLOR_RED_GLOW;
        }
        if (pattern->tracks[track].muted) {
            labelColor = COLOR_DARK_GRAY;
        }
        if (pattern->tracks[track].solo) {
            labelColor = COLOR_PURPLE_GLOW;
        }
        
        displayManager.drawText(5, labelY, trackLabel, labelColor);
    }
    
    // Draw step numbers
    displayManager.setFont(FONT_SMALL);
    for (uint8_t step = 0; step < SEQUENCER_COLS; step++) {
        if (step % 4 == 0) { // Only show beat numbers
            int16_t stepX = ui.gridStartX + step * (ui.cellWidth + CELL_SPACING);
            String stepLabel = String((step / 4) + 1);
            displayManager.drawText(stepX + 2, ui.gridStartY - 12, stepLabel, COLOR_LIGHT_GRAY);
        }
    }
    
    // Draw grid cells
    for (uint8_t track = 0; track < MAX_TRACKS; track++) {
        for (uint8_t step = 0; step < SEQUENCER_COLS; step++) {
            drawGridCell(track, step);
        }
    }
}

void SequencerApp::drawGridCell(uint8_t track, uint8_t step) {
    GridCell* cell = &ui.grid[track][step];
    
    // Determine cell color based on state
    uint16_t cellColor = COLOR_DARK_GRAY;
    uint16_t borderColor = COLOR_MID_GRAY;
    
    switch (cell->state) {
        case CELL_OFF:
            cellColor = COLOR_DARK_GRAY;
            break;
        case CELL_ON:
            cellColor = COLOR_GREEN_PHOS;
            break;
        case CELL_ACCENT:
            cellColor = COLOR_RED_GLOW;
            break;
        case CELL_GHOST:
            cellColor = COLOR_LIGHT_GRAY;
            break;
        case CELL_PLAYING:
            cellColor = COLOR_WHITE;
            break;
    }
    
    // Highlight current step
    if (cell->highlighted) {
        borderColor = COLOR_PURPLE_GLOW;
        displayManager.drawRetroRect(cell->x - 1, cell->y - 1, cell->w + 2, cell->h + 2, 
                                   borderColor, false);
    }
    
    // Draw cell
    displayManager.drawRetroRect(cell->x, cell->y, cell->w, cell->h, cellColor, true);
    displayManager.drawRetroRect(cell->x, cell->y, cell->w, cell->h, borderColor, false);
    
    // Beat indicators (every 4 steps)
    if (step % 4 == 0) {
        displayManager.drawPixel(cell->x + 1, cell->y + 1, COLOR_WHITE);
    }
}

GridCell* SequencerApp::getTouchedCell(TouchPoint touch) {
    for (uint8_t track = 0; track < MAX_TRACKS; track++) {
        for (uint8_t step = 0; step < SEQUENCER_COLS; step++) {
            GridCell* cell = &ui.grid[track][step];
            if (touchInterface.isPointInRect(touch, cell->x, cell->y, cell->w, cell->h)) {
                return cell;
            }
        }
    }
    return nullptr;
}

void SequencerApp::handleCellTouch(GridCell* cell) {
    if (!cell) return;
    
    // Select track
    ui.selectedTrack = cell->track;
    
    // Toggle step
    toggleStep(cell->track, cell->step);
}

void SequencerApp::animateStep(uint8_t step) {
    // Set playing state for visual feedback
    for (uint8_t track = 0; track < MAX_TRACKS; track++) {
        GridCell* cell = &ui.grid[track][step];
        if (cell->state != CELL_OFF) {
            // Brief animation
            cell->state = CELL_PLAYING;
        }
    }
}

// ========================================
// UI RENDERING
// ========================================

void SequencerApp::drawSequencerInterface() {
    // Draw main grid
    drawGrid();
    
    // Draw transport controls at bottom
    drawTransportControls();
    
    // Draw pattern info
    displayManager.setFont(FONT_SMALL);
    String patternInfo = "Pat:" + String(ui.selectedPattern + 1) + 
                        " BPM:" + String(getCurrentPattern()->bpm);
    displayManager.drawText(5, 5, patternInfo, COLOR_GREEN_PHOS);
    
    // Draw track info
    if (ui.selectedTrack < MAX_TRACKS) {
        Track* track = getCurrentTrack();
        String trackInfo = "Trk:" + String(ui.selectedTrack + 1) + 
                          " Vol:" + String(track->volume);
        displayManager.drawText(150, 5, trackInfo, COLOR_WHITE);
        
        // Show track status
        if (track->muted) {
            displayManager.drawText(250, 5, "MUTE", COLOR_RED_GLOW);
        }
        if (track->solo) {
            displayManager.drawText(280, 5, "SOLO", COLOR_PURPLE_GLOW);
        }
    }
}

void SequencerApp::drawTransportControls() {
    int16_t transportY = SCREEN_HEIGHT - TRANSPORT_HEIGHT + 5;
    
    displayManager.setFont(FONT_SMALL);
    
    // Play/Stop button
    uint16_t playColor = ui.isPlaying ? COLOR_GREEN_PHOS : COLOR_WHITE;
    String playText = ui.isPlaying ? "STOP" : "PLAY";
    displayManager.drawButton(10, transportY, 40, 20, playText, 
                             ui.isPlaying ? BUTTON_PRESSED : BUTTON_NORMAL, playColor);
    
    // Record button
    uint16_t recColor = ui.isRecording ? COLOR_RED_GLOW : COLOR_WHITE;
    displayManager.drawButton(55, transportY, 30, 20, "REC", 
                             ui.isRecording ? BUTTON_PRESSED : BUTTON_NORMAL, recColor);
    
    // Pattern selector
    displayManager.drawButton(90, transportY, 30, 20, String(ui.selectedPattern + 1));
    
    // BPM display
    displayManager.drawText(130, transportY + 5, "BPM:" + String(getCurrentPattern()->bpm), 
                           COLOR_GREEN_PHOS);
    
    // Current step indicator
    if (ui.isPlaying) {
        String stepText = "Step:" + String(ui.currentStep + 1);
        displayManager.drawText(200, transportY + 5, stepText, COLOR_PURPLE_GLOW);
    }
}

void SequencerApp::drawSampleBrowser() {
    displayManager.clearScreen(COLOR_BLACK);
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawTextCentered(0, 20, SCREEN_WIDTH, "Sample Browser", COLOR_RED_GLOW);
    
    displayManager.setFont(FONT_SMALL);
    displayManager.drawTextCentered(0, 200, SCREEN_WIDTH, "Coming soon...", COLOR_LIGHT_GRAY);
}

void SequencerApp::drawPerformanceMode() {
    displayManager.clearScreen(COLOR_BLACK);
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawTextCentered(0, 20, SCREEN_WIDTH, "Performance Mode", COLOR_RED_GLOW);
    
    // Large trigger pads for performance
    int16_t padSize = 60;
    int16_t padSpacing = 10;
    int16_t startX = (SCREEN_WIDTH - (4 * padSize + 3 * padSpacing)) / 2;
    int16_t startY = 60;
    
    for (uint8_t i = 0; i < 8; i++) {
        int16_t col = i % 4;
        int16_t row = i / 4;
        int16_t x = startX + col * (padSize + padSpacing);
        int16_t y = startY + row * (padSize + padSpacing);
        
        uint16_t padColor = (i == ui.selectedTrack) ? COLOR_RED_GLOW : COLOR_MID_GRAY;
        displayManager.drawRetroRect(x, y, padSize, padSize, padColor, true);
        displayManager.drawRetroRect(x, y, padSize, padSize, COLOR_WHITE, false);
        
        displayManager.setFont(FONT_SMALL);
        displayManager.drawTextCentered(x, y + padSize/2 - 4, padSize, String(i + 1), COLOR_BLACK);
    }
}

// ========================================
// CONTROL HANDLING
// ========================================

void SequencerApp::handleTransportTouch(TouchPoint touch) {
    int16_t transportY = SCREEN_HEIGHT - TRANSPORT_HEIGHT + 5;
    
    // Play/Stop button
    if (touchInterface.isPointInRect(touch, 10, transportY, 40, 20)) {
        togglePlayback();
    }
    // Record button
    else if (touchInterface.isPointInRect(touch, 55, transportY, 30, 20)) {
        recordToggle();
    }
    // Pattern selector
    else if (touchInterface.isPointInRect(touch, 90, transportY, 30, 20)) {
        ui.selectedPattern = (ui.selectedPattern + 1) % MAX_PATTERNS;
        debugLog("Selected pattern: " + String(ui.selectedPattern));
    }
}

void SequencerApp::handleControlTouch(TouchPoint touch) {
    // Handle track selection (left side)
    if (touch.x < TRACK_INFO_WIDTH) {
        uint8_t touchedTrack = (touch.y - ui.gridStartY) / (ui.cellHeight + CELL_SPACING);
        if (touchedTrack < MAX_TRACKS) {
            selectTrack(touchedTrack);
        }
    }
}

void SequencerApp::togglePlayback() {
    if (ui.isPlaying) {
        stopPlayback();
    } else {
        ui.isPlaying = true;
        ui.currentStep = 0;
        nextStepTime = millis();
        debugLog("Playback started");
    }
}

void SequencerApp::stopPlayback() {
    ui.isPlaying = false;
    ui.currentStep = 0;
    debugLog("Playback stopped");
}

void SequencerApp::recordToggle() {
    ui.isRecording = !ui.isRecording;
    debugLog("Record mode: " + String(ui.isRecording ? "ON" : "OFF"));
}

// ========================================
// BUILT-IN SAMPLE GENERATION
// ========================================

void SequencerApp::generateBuiltinSamples() {
    debugLog("Generating built-in samples...");
    
    // Generate samples for first 4 tracks
    Track* tracks = getCurrentPattern()->tracks;
    
    // Kick drum - Track 0
    generateKickSample(tracks[0].sampleData, 1024);
    tracks[0].sampleLength = 1024;
    tracks[0].sampleLoaded = true;
    tracks[0].name = "Kick";
    
    // Snare drum - Track 1
    generateSnareSample(tracks[1].sampleData, 1024);
    tracks[1].sampleLength = 1024;
    tracks[1].sampleLoaded = true;
    tracks[1].name = "Snare";
    
    // Hi-hat - Track 2
    generateHihatSample(tracks[2].sampleData, 512);
    tracks[2].sampleLength = 512;
    tracks[2].sampleLoaded = true;
    tracks[2].name = "Hihat";
    
    // Bass - Track 3
    generateBassSample(tracks[3].sampleData, 2048);
    tracks[3].sampleLength = 2048;
    tracks[3].sampleLoaded = true;
    tracks[3].name = "Bass";
    
    debugLog("Built-in samples generated");
}

void SequencerApp::generateKickSample(uint16_t* buffer, uint16_t length) {
    // Generate kick drum sound (low frequency with pitch envelope)
    for (uint16_t i = 0; i < length; i++) {
        float t = (float)i / length;
        float envelope = exp(-t * 8.0f); // Exponential decay
        float frequency = 60.0f * (1.0f - t * 0.8f); // Pitch drop
        float sample = sin(2.0f * PI * frequency * t) * envelope;
        buffer[i] = (uint16_t)((sample + 1.0f) * 32767.5f);
    }
}

void SequencerApp::generateSnareSample(uint16_t* buffer, uint16_t length) {
    // Generate snare drum sound (mix of tone and noise)
    for (uint16_t i = 0; i < length; i++) {
        float t = (float)i / length;
        float envelope = exp(-t * 6.0f);
        
        // Tone component
        float tone = sin(2.0f * PI * 200.0f * t) * 0.3f;
        
        // Noise component
        float noise = ((float)systemCore.getRandomByte() / 127.5f - 1.0f) * 0.7f;
        
        float sample = (tone + noise) * envelope;
        buffer[i] = (uint16_t)((sample + 1.0f) * 32767.5f);
    }
}

void SequencerApp::generateHihatSample(uint16_t* buffer, uint16_t length) {
    // Generate hi-hat sound (filtered noise)
    for (uint16_t i = 0; i < length; i++) {
        float t = (float)i / length;
        float envelope = exp(-t * 12.0f); // Quick decay
        
        // High-frequency noise
        float noise = ((float)systemCore.getRandomByte() / 127.5f - 1.0f);
        
        // Simple high-pass filtering effect
        if (i > 0) {
            noise = noise - buffer[i-1] * 0.5f / 65535.0f;
        }
        
        float sample = noise * envelope;
        buffer[i] = (uint16_t)((sample + 1.0f) * 32767.5f);
    }
}

void SequencerApp::generateBassSample(uint16_t* buffer, uint16_t length) {
    // Generate bass sound (low frequency saw wave)
    for (uint16_t i = 0; i < length; i++) {
        float t = (float)i / length;
        float envelope = exp(-t * 2.0f); // Slow decay
        
        // Saw wave at low frequency
        float frequency = 80.0f;
        float phase = fmod(frequency * t, 1.0f);
        float sample = (2.0f * phase - 1.0f) * envelope;
        
        buffer[i] = (uint16_t)((sample + 1.0f) * 32767.5f);
    }
}

// ========================================
// PUBLIC INTERFACE METHODS
// ========================================

void SequencerApp::selectPattern(uint8_t patternIndex) {
    if (patternIndex < MAX_PATTERNS) {
        ui.selectedPattern = patternIndex;
        debugLog("Selected pattern: " + String(patternIndex));
    }
}

void SequencerApp::selectTrack(uint8_t trackIndex) {
    if (trackIndex < MAX_TRACKS) {
        ui.selectedTrack = trackIndex;
        debugLog("Selected track: " + String(trackIndex));
    }
}

void SequencerApp::toggleStep(uint8_t track, uint8_t step) {
    if (track >= MAX_TRACKS || step >= SEQUENCER_COLS) return;
    
    Pattern* pattern = getCurrentPattern();
    CellState& cellState = pattern->tracks[track].steps[step];
    
    // Cycle through cell states
    switch (cellState) {
        case CELL_OFF:
            cellState = CELL_ON;
            break;
        case CELL_ON:
            cellState = CELL_ACCENT;
            break;
        case CELL_ACCENT:
            cellState = CELL_GHOST;
            break;
        case CELL_GHOST:
            cellState = CELL_OFF;
            break;
        default:
            cellState = CELL_OFF;
            break;
    }
    
    pattern->isEmpty = false;
    
    // Trigger sample for immediate feedback
    if (cellState != CELL_OFF) {
        triggerSample(track, 100);
    }
    
    debugLog("Toggled step " + String(step) + " on track " + String(track));
}

void SequencerApp::setBPM(uint8_t bpm) {
    if (bpm >= 60 && bpm <= 200) {
        getCurrentPattern()->bpm = bpm;
        calculateStepTiming();
        debugLog("BPM set to: " + String(bpm));
    }
}

void SequencerApp::setSwing(uint8_t swing) {
    if (swing <= 100) {
        getCurrentPattern()->swing = swing;
        debugLog("Swing set to: " + String(swing));
    }
}

// ========================================
// FILE I/O SYSTEM
// ========================================

bool SequencerApp::saveProject(String projectName) {
    String filePath = projectPath + projectName + ".json";
    
    File file = SD.open(filePath, FILE_WRITE);
    if (!file) {
        debugLog("Failed to create project file: " + filePath);
        return false;
    }
    
    // Create JSON document
    DynamicJsonDocument doc(8192); // Large buffer for pattern data
    
    // Save patterns (simplified - would save all pattern data)
    JsonArray patternsArray = doc.createNestedArray("patterns");
    for (uint8_t i = 0; i < MAX_PATTERNS; i++) {
        JsonObject patternObj = patternsArray.createNestedObject();
        patternObj["name"] = patterns[i].name;
        patternObj["bpm"] = patterns[i].bpm;
        patternObj["swing"] = patterns[i].swing;
        patternObj["length"] = patterns[i].length;
        
        JsonArray tracksArray = patternObj.createNestedArray("tracks");
        for (uint8_t t = 0; t < MAX_TRACKS; t++) {
            JsonObject trackObj = tracksArray.createNestedObject();
            trackObj["name"] = patterns[i].tracks[t].name;
            trackObj["volume"] = patterns[i].tracks[t].volume;
            trackObj["muted"] = patterns[i].tracks[t].muted;
            trackObj["solo"] = patterns[i].tracks[t].solo;
            
            JsonArray stepsArray = trackObj.createNestedArray("steps");
            for (uint8_t s = 0; s < SEQUENCER_COLS; s++) {
                stepsArray.add((int)patterns[i].tracks[t].steps[s]);
            }
        }
    }
    
    // Save current UI state
    doc["selectedPattern"] = ui.selectedPattern;
    doc["selectedTrack"] = ui.selectedTrack;
    
    // Write to file
    serializeJson(doc, file);
    file.close();
    
    debugLog("Project saved: " + projectName);
    return true;
}

bool SequencerApp::loadProject(String projectName) {
    String filePath = projectPath + projectName + ".json";
    
    if (!SD.exists(filePath)) {
        debugLog("Project file not found: " + filePath);
        return false;
    }
    
    File file = SD.open(filePath, FILE_READ);
    if (!file) {
        debugLog("Failed to open project file");
        return false;
    }
    
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        debugLog("Failed to parse project file");
        return false;
    }
    
    // Load patterns (simplified implementation)
    if (doc.containsKey("patterns")) {
        JsonArray patternsArray = doc["patterns"];
        
        for (uint8_t i = 0; i < min((int)MAX_PATTERNS, (int)patternsArray.size()); i++) {
            JsonObject patternObj = patternsArray[i];
            patterns[i].name = patternObj["name"].as<String>();
            patterns[i].bpm = patternObj["bpm"];
            patterns[i].swing = patternObj["swing"];
            patterns[i].length = patternObj["length"];
        }
    }
    
    // Load UI state
    if (doc.containsKey("selectedPattern")) {
        ui.selectedPattern = doc["selectedPattern"];
    }
    if (doc.containsKey("selectedTrack")) {
        ui.selectedTrack = doc["selectedTrack"];
    }
    
    debugLog("Project loaded: " + projectName);
    return true;
}

bool SequencerApp::loadSampleLibrary() {
    // Scan for sample files in /sounds/ directory
    String sampleDir = "/sounds/";
    
    if (!SD.exists(sampleDir)) {
        debugLog("Sample directory not found, using built-in samples only");
        return false;
    }
    
    // Would scan directory and populate sample list
    debugLog("Sample library loaded");
    return true;
}

bool SequencerApp::exportPattern(uint8_t patternIndex) {
    // Placeholder for audio export functionality
    debugLog("Exporting pattern " + String(patternIndex) + " (not implemented)");
    return true;
}