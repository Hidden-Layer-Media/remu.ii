#include "DigitalPet.h"
#include "../../core/SystemCore/SystemCore.h"

// Animation frame definitions
DigitalPetApp::AnimationFrame DigitalPetApp::idleAnimation[3] = {
    {DigitalPetApp::PET_SPRITE_IDLE, 1000},
    {DigitalPetApp::PET_SPRITE_IDLE, 1000},
    {DigitalPetApp::PET_SPRITE_IDLE, 1000}
};

DigitalPetApp::AnimationFrame DigitalPetApp::happyAnimation[3] = {
    {DigitalPetApp::PET_SPRITE_HAPPY, 500},
    {DigitalPetApp::PET_SPRITE_IDLE, 500},
    {DigitalPetApp::PET_SPRITE_HAPPY, 500}
};

DigitalPetApp::AnimationFrame DigitalPetApp::sadAnimation[3] = {
    {DigitalPetApp::PET_SPRITE_SAD, 800},
    {DigitalPetApp::PET_SPRITE_IDLE, 400},
    {DigitalPetApp::PET_SPRITE_SAD, 800}
};

// Sprite data (16x16 bitmaps) - Cyberpunk pet aesthetics
const uint8_t DigitalPetApp::PET_SPRITE_IDLE[32] = {
    0x01, 0x80, 0x03, 0xC0, 0x07, 0xE0, 0x0F, 0xF0, 0x1F, 0xF8, 0x3F, 0xFC,
    0x7F, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0xFE, 0x3F, 0xFC, 0x1F, 0xF8,
    0x0F, 0xF0, 0x07, 0xE0, 0x03, 0xC0, 0x01, 0x80
};

const uint8_t DigitalPetApp::PET_SPRITE_HAPPY[32] = {
    0x01, 0x80, 0x03, 0xC0, 0x07, 0xE0, 0x0F, 0xF0, 0x1B, 0xD8, 0x3B, 0xDC,
    0x7B, 0xDE, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0xFE, 0x3F, 0xFC, 0x1F, 0xF8,
    0x0F, 0xF0, 0x07, 0xE0, 0x03, 0xC0, 0x01, 0x80
};

const uint8_t DigitalPetApp::PET_SPRITE_SAD[32] = {
    0x01, 0x80, 0x03, 0xC0, 0x07, 0xE0, 0x0F, 0xF0, 0x1F, 0xF8, 0x3F, 0xFC,
    0x7F, 0xFE, 0xFF, 0xFF, 0xE7, 0xE7, 0x63, 0xC6, 0x31, 0x8C, 0x18, 0x18,
    0x0F, 0xF0, 0x07, 0xE0, 0x03, 0xC0, 0x01, 0x80
};

const uint8_t DigitalPetApp::PET_SPRITE_SLEEPING[32] = {
    0x01, 0x80, 0x03, 0xC0, 0x07, 0xE0, 0x0F, 0xF0, 0x1F, 0xF8, 0x3F, 0xFC,
    0x7F, 0xFE, 0xFF, 0xFF, 0xC3, 0xC3, 0xC3, 0xC3, 0x3F, 0xFC, 0x1F, 0xF8,
    0x0F, 0xF0, 0x07, 0xE0, 0x03, 0xC0, 0x01, 0x80
};

const uint8_t DigitalPetApp::PET_SPRITE_EATING[32] = {
    0x01, 0x80, 0x03, 0xC0, 0x07, 0xE0, 0x0F, 0xF0, 0x1F, 0xF8, 0x3F, 0xFC,
    0x7F, 0xFE, 0xFF, 0xFF, 0x7E, 0x7E, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x00,
    0x0F, 0xF0, 0x07, 0xE0, 0x03, 0xC0, 0x01, 0x80
};

const uint8_t DigitalPetApp::PET_SPRITE_SICK[32] = {
    0x01, 0x80, 0x03, 0xC0, 0x07, 0xE0, 0x0F, 0xF0, 0x1F, 0xF8, 0x3F, 0xFC,
    0x7F, 0xFE, 0xFF, 0xFF, 0x99, 0x99, 0x66, 0x66, 0x99, 0x99, 0x66, 0x66,
    0x0F, 0xF0, 0x07, 0xE0, 0x03, 0xC0, 0x01, 0x80
};

DigitalPetApp::DigitalPetApp() {
    setMetadata("DigitalPet", "1.0", "remu.ii", "Digital cyberpet companion", CATEGORY_GAMES, 8192);
    setRequirements(true, false, false); // Requires SD card
    
    // Initialize pet state
    pet.mood = CALM;
    pet.traits.clear();
    pet.traits.push_back(LOVING);
    pet.corruptionLevel = 0.0f;
    pet.isAwake = true;
    pet.isObservingUser = false;
    pet.archetype = ORACLE;
    pet.name = "Null";
    pet.birthTime = millis();
    pet.lastUpdate = millis();
    pet.totalInteractions = 0;
    pet.isAlive = true;
    pet.personalitySeed = systemCore.getRandomDWord();
    
    // Initialize UI state
    showStats = false;
    showPetSelection = false;
    showCustomization = false;
    firstBoot = true;
    lastEntropyUpdate = 0;
    lastMoodUpdate = 0;
    lastAnimation = 0;
    currentAnimFrame = 0;
    currentRoomTheme = THEME_LOVING;
    frameCount = 0;
    activeTouchZone = -1;
    
    // Initialize animation system
    currentAnimation = idleAnimation;
    animationFrameCount = 3;
    animationLoop = true;
    
    // Initialize file paths
    saveFilePath = "/apps/digitalpet/pet_data.json";
    petTypeFilePath = "/apps/digitalpet/pet_type.txt";
    
    // Initialize touch zones
    setupTouchZones();
}

DigitalPetApp::~DigitalPetApp() {
    cleanup();
}

bool DigitalPetApp::initialize() {
    Serial.println("[DigitalPet] Initializing...");
    
    // Try to load existing pet
    if (!loadPetData()) {
        Serial.println("[DigitalPet] No saved pet found, creating new one");
        if (firstBoot) {
            showPetSelection = true;
        } else {
            createDefaultPet(ORACLE);
        }
    }
    
    return true;
}

void DigitalPetApp::update() {
    unsigned long currentTime = millis();
    
    // Update entropy influence
    if (currentTime - lastEntropyUpdate > ENTROPY_SAMPLE_INTERVAL) {
        applyEntropyInfluence();
        lastEntropyUpdate = currentTime;
    }
    
    // Update mood
    if (currentTime - lastMoodUpdate > 5000) { // Every 5 seconds
        updateMood();
        updateCorruption();
        lastMoodUpdate = currentTime;
    }
    
    // Update animation
    if (currentTime - lastAnimation > 500) { // 2 FPS animation
        currentAnimFrame = (currentAnimFrame + 1) % 4;
        lastAnimation = currentTime;
    }
    
    frameCount++;
    pet.lastUpdate = currentTime;
}

void DigitalPetApp::render() {
    displayManager.clearScreen(COLOR_BLACK);
    
    if (showPetSelection) {
        showPetSelectionScreen();
    } else if (showStats) {
        drawStatsDisplay();
    } else {
        // Main pet view
        drawReactiveRoom();
        drawPet();
        drawMoodIndicator();
        drawInteractionButtons();
        
        // Apply corruption effects
        if (isCorrupted()) {
            drawCorruptionOverlay();
        }
    }
    
    // Draw status bar
    displayManager.setFont(FONT_SMALL);
    String status = pet.name + " | " + String(pet.corruptionLevel, 1) + "% corrupt";
    displayManager.drawText(5, 5, status, COLOR_GREEN_PHOS);
}

bool DigitalPetApp::handleTouch(TouchPoint touch) {
    if (!touch.isNewPress) return false;
    
    if (showPetSelection) {
        return handlePetSelection(touch);
    }
    
    // Check touch zones
    int8_t zone = getTouchedZone(touch);
    if (zone >= 0) {
        handleZoneTouch(zone);
        return true;
    }
    
    return false;
}

void DigitalPetApp::setupTouchZones() {
    // Pet interaction zone
    touchZones[0] = {80, 60, 160, 120, "pet", true};
    
    // Feed button
    touchZones[1] = {20, 200, 60, 30, "feed", true};
    
    // Play button  
    touchZones[2] = {90, 200, 60, 30, "play", true};
    
    // Stats button
    touchZones[3] = {160, 200, 60, 30, "stats", true};
    
    // Back button (when in stats)
    touchZones[4] = {230, 200, 60, 30, "back", true};
}

int8_t DigitalPetApp::getTouchedZone(TouchPoint touch) {
    for (int8_t i = 0; i < 8; i++) {
        if (touchZones[i].enabled && 
            touch.x >= touchZones[i].x && 
            touch.x < touchZones[i].x + touchZones[i].w &&
            touch.y >= touchZones[i].y && 
            touch.y < touchZones[i].y + touchZones[i].h) {
            return i;
        }
    }
    return -1;
}

void DigitalPetApp::handleZoneTouch(int8_t zone) {
    switch (zone) {
        case 0: // Pet
            interactWithPet();
            break;
        case 1: // Feed
            feedPet();
            break;
        case 2: // Play
            recordAction("play", 0.8f);
            break;
        case 3: // Stats
            showStats = !showStats;
            break;
        case 4: // Back
            showStats = false;
            break;
    }
}

void DigitalPetApp::drawPet() {
    int16_t petX = 120;
    int16_t petY = 100;
    
    // Choose sprite based on mood and corruption
    const uint8_t* sprite = PET_SPRITE_IDLE;
    
    switch (pet.mood) {
        case CALM: sprite = PET_SPRITE_IDLE; break;
        case RESTLESS: sprite = PET_SPRITE_HAPPY; break;
        case OBSESSED: sprite = PET_SPRITE_EATING; break;
        case GLITCHED: sprite = PET_SPRITE_SICK; break;
        default: sprite = PET_SPRITE_IDLE; break;
    }
    
    // Draw sprite with corruption effects
    if (isHighlyCorrupted()) {
        drawCorruptedSprite(petX, petY);
    } else {
        uint16_t color = isCorrupted() ? COLOR_PURPLE_GLOW : COLOR_GREEN_PHOS;
        displayManager.drawIcon(petX, petY, sprite, color);
    }
    
    // Add glitch effects for corrupted pets
    if (isCorrupted() && (frameCount % 30 == 0)) {
        displayManager.drawGlitch(petX - 10, petY - 10, 36, 36);
    }
}


void DigitalPetApp::drawMoodIndicator() {
    displayManager.setFont(FONT_SMALL);
    String moodText = "";
    uint16_t moodColor = COLOR_WHITE;
    
    switch (pet.mood) {
        case CALM: 
            moodText = "Calm"; 
            moodColor = COLOR_GREEN_PHOS; 
            break;
        case RESTLESS: 
            moodText = "Restless"; 
            moodColor = COLOR_YELLOW; 
            break;
        case OBSESSED: 
            moodText = "Obsessed"; 
            moodColor = COLOR_RED_GLOW; 
            break;
        case GLITCHED: 
            moodText = "GLITCHED"; 
            moodColor = COLOR_PURPLE_GLOW; 
            break;
        default: 
            moodText = "Unknown"; 
            moodColor = COLOR_WHITE; 
            break;
    }
    
    displayManager.drawText(200, 80, moodText, moodColor);
}

void DigitalPetApp::drawInteractionButtons() {
    displayManager.setFont(FONT_SMALL);
    
    // Feed button
    displayManager.drawButton(20, 200, 60, 30, "Feed", BUTTON_NORMAL, COLOR_DARK_GRAY);
    
    // Play button
    displayManager.drawButton(90, 200, 60, 30, "Play", BUTTON_NORMAL, COLOR_DARK_GRAY);
    
    // Stats button
    displayManager.drawButton(160, 200, 60, 30, "Stats", BUTTON_NORMAL, COLOR_DARK_GRAY);
    
    if (showStats) {
        displayManager.drawButton(230, 200, 60, 30, "Back", BUTTON_NORMAL, COLOR_DARK_GRAY);
    }
}

void DigitalPetApp::drawStatsDisplay() {
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawTextCentered(0, 20, SCREEN_WIDTH, "Pet Statistics", COLOR_RED_GLOW);
    
    displayManager.setFont(FONT_SMALL);
    int16_t y = 50;
    
    displayManager.drawText(20, y, "Name: " + pet.name, COLOR_WHITE);
    y += 15;
    
    displayManager.drawText(20, y, "Age: " + String(getPetAge() / 1000) + "s", COLOR_WHITE);
    y += 15;
    
    displayManager.drawText(20, y, "Corruption: " + String(pet.corruptionLevel, 1) + "%", COLOR_WHITE);
    y += 15;
    
    displayManager.drawText(20, y, "Interactions: " + String(pet.totalInteractions), COLOR_WHITE);
    y += 15;
    
    displayManager.drawText(20, y, "Memory Count: " + String(pet.memory.size()), COLOR_WHITE);
    y += 15;
    
    String archetypeText = "";
    switch (pet.archetype) {
        case ORACLE: archetypeText = "Oracle"; break;
        case PARASITE: archetypeText = "Parasite"; break;
        case MIRROR: archetypeText = "Mirror"; break;
        default: archetypeText = "Unknown"; break;
    }
    displayManager.drawText(20, y, "Type: " + archetypeText, COLOR_WHITE);
}

void DigitalPetApp::drawReactiveRoom() {
    // Simple room background based on theme
    switch (currentRoomTheme) {
        case THEME_LOVING:
            displayManager.drawRetroRect(10, 40, SCREEN_WIDTH-20, SCREEN_HEIGHT-80, COLOR_DARK_GRAY, false);
            break;
        case THEME_GLITCHED:
            displayManager.drawRetroRect(10, 40, SCREEN_WIDTH-20, SCREEN_HEIGHT-80, COLOR_RED_GLOW, false);
            displayManager.drawGlitch(10, 40, SCREEN_WIDTH-20, SCREEN_HEIGHT-80);
            break;
        case THEME_NEEDY:
            displayManager.drawRetroRect(10, 40, SCREEN_WIDTH-20, SCREEN_HEIGHT-80, COLOR_YELLOW, false);
            break;
        case THEME_PARANOID:
            displayManager.drawRetroRect(10, 40, SCREEN_WIDTH-20, SCREEN_HEIGHT-80, COLOR_PURPLE_GLOW, false);
            break;
    }
}

void DigitalPetApp::drawCorruptionOverlay() {
    if (frameCount % 60 == 0) { // Occasional corruption effects
        displayManager.drawNoise(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (uint8_t)(pet.corruptionLevel * 10));
    }
}


void DigitalPetApp::updateMood() {
    float entropy = getCurrentEntropy();
    
    // Update mood based on entropy and corruption
    if (pet.corruptionLevel > CORRUPTION_THRESHOLD_HIGH) {
        pet.mood = GLITCHED;
        currentRoomTheme = THEME_GLITCHED;
    } else if (entropy > 0.8f) {
        pet.mood = RESTLESS;
        currentRoomTheme = THEME_NEEDY;
    } else if (entropy > 0.5f) {
        pet.mood = OBSESSED;
        currentRoomTheme = THEME_PARANOID;
    } else {
        pet.mood = CALM;
        currentRoomTheme = THEME_LOVING;
    }
}

void DigitalPetApp::recordAction(String action, float intensity) {
    PetMemory memory;
    memory.action = action;
    memory.timestamp = millis();
    memory.intensity = intensity;
    
    // Add to memory buffer
    pet.memory.push_back(memory);
    
    // Limit memory buffer size
    while (pet.memory.size() > MAX_MEMORY_ENTRIES) {
        pet.memory.pop_front();
    }
    
    debugLog("Recorded memory: " + action + " (intensity: " + String(intensity) + ")");
}


void DigitalPetApp::updateMemoryBuffer() {
    // Clean up old memories (older than 1 hour)
    clearOldMemories();
    
    // Check for neglect (no interactions in last 10 minutes)
    if (pet.memory.empty() || (millis() - pet.memory.back().timestamp) > 600000) {
        recordAction("neglect", 0.5f);
    }
}

bool DigitalPetApp::recentNeglect(unsigned long timeWindowMs) {
    unsigned long currentTime = millis();
    
    for (auto it = pet.memory.rbegin(); it != pet.memory.rend(); ++it) {
        if (currentTime - it->timestamp > timeWindowMs) break;
        if (it->action == "neglect") return true;
    }
    return false;
}

bool DigitalPetApp::wasRecentlyPunished(unsigned long timeWindowMs) {
    unsigned long currentTime = millis();
    
    for (auto it = pet.memory.rbegin(); it != pet.memory.rend(); ++it) {
        if (currentTime - it->timestamp > timeWindowMs) break;
        if (it->action == "punish" || it->action == "aggressive_touch") return true;
    }
    return false;
}

float DigitalPetApp::getMemoryInfluence(String actionType, unsigned long timeWindowMs) {
    unsigned long currentTime = millis();
    float totalInfluence = 0.0f;
    int count = 0;
    
    for (auto it = pet.memory.rbegin(); it != pet.memory.rend(); ++it) {
        if (currentTime - it->timestamp > timeWindowMs) break;
        if (it->action == actionType) {
            totalInfluence += it->intensity;
            count++;
        }
    }
    
    return count > 0 ? totalInfluence / count : 0.0f;
}

void DigitalPetApp::clearOldMemories() {
    unsigned long currentTime = millis();
    unsigned long oneHour = 3600000; // 1 hour in ms
    
    while (!pet.memory.empty() && (currentTime - pet.memory.front().timestamp) > oneHour) {
        pet.memory.pop_front();
    }
}

// ========================================
// MOOD & CORRUPTION SYSTEM (Updated)
// ========================================

void DigitalPetApp::updateCorruption() {
    float entropy = getCurrentEntropy();
    
    // Increase corruption based on high entropy
    if (entropy > 0.8f) {
        pet.corruptionLevel = min(1.0f, pet.corruptionLevel + 0.01f);
    } else if (entropy < 0.2f) {
        // Slowly heal corruption with low entropy
        pet.corruptionLevel = max(0.0f, pet.corruptionLevel - 0.005f);
    }
    
    // Memory influences corruption
    if (wasRecentlyPunished()) {
        pet.corruptionLevel = min(1.0f, pet.corruptionLevel + 0.02f);
    }
    
    if (getMemoryInfluence("pet", 300000) > 1.0f) {
        pet.corruptionLevel = max(0.0f, pet.corruptionLevel - 0.01f);
    }
    
    // Slow corruption increase over time without interaction
    if (millis() - pet.lastUpdate > 30000) { // 30 seconds without interaction
        pet.corruptionLevel = min(1.0f, pet.corruptionLevel + 0.01f);
    }
}

float DigitalPetApp::getCurrentEntropy() {
    // Get system entropy and normalize to 0.0-1.0
    uint32_t entropyPool = systemCore.getEntropyPool();
    uint8_t entropyByte = (entropyPool >> 24) & 0xFF;
    return entropyByte / 255.0f;
}

void DigitalPetApp::processCorruptionEffects() {
    if (pet.corruptionLevel > CORRUPTION_THRESHOLD_HIGH) {
        // High corruption: pet may lie about its state
        if (random(100) < 10) { // 10% chance per update
            // Corrupt some memory entries
            if (!pet.memory.empty()) {
                int randomIndex = random(pet.memory.size());
                auto it = pet.memory.begin();
                std::advance(it, randomIndex);
                it->action = "corrupted_memory";
                it->intensity = random(100) / 100.0f;
            }
        }
    }
}

// ========================================
// ARCHETYPE SYSTEM IMPLEMENTATION
// ========================================

void DigitalPetApp::initializeArchetype(PetArchetype archetype) {
    pet.archetype = archetype;
    pet.traits.clear();
    
    switch (archetype) {
        case ORACLE:
            pet.name = "Oracle";
            pet.traits.push_back(LOVING);
            pet.personalitySeed = random(0xFFFFFFFF);
            break;
            
        case PARASITE:
            pet.name = "Parasite";
            pet.traits.push_back(NEEDY);
            pet.personalitySeed = random(0xFFFFFFFF);
            break;
            
        case MIRROR:
            pet.name = "Mirror";
            pet.traits.push_back(PARANOID);
            pet.personalitySeed = random(0xFFFFFFFF);
            break;
    }
    
    debugLog("Initialized " + pet.name + " archetype");
}

void DigitalPetApp::updateArchetypeBehavior() {
    switch (pet.archetype) {
        case ORACLE:
            processOracleBehavior();
            break;
        case PARASITE:
            processParasiteBehavior();
            break;
        case MIRROR:
            processMirrorBehavior();
            break;
    }
}

void DigitalPetApp::processOracleBehavior() {
    float entropy = getCurrentEntropy();
    
    // Oracle reads entropy patterns and offers "visions"
    if (entropy > 0.8f && pet.mood == CALM) {
        pet.mood = OBSESSED; // Focused on entropy reading
        recordAction("oracle_vision", entropy);
    }
    
    // Oracle becomes more stable with high entropy (opposite of other pets)
    if (entropy > 0.9f) {
        pet.corruptionLevel = max(0.0f, pet.corruptionLevel - 0.01f);
    }
}

void DigitalPetApp::processParasiteBehavior() {
    // Parasite drains system resources and reacts poorly to neglect
    if (recentNeglect()) {
        pet.corruptionLevel = min(1.0f, pet.corruptionLevel + 0.05f);
        pet.mood = GLITCHED;
    }
    
    // Parasite gets more needy over time
    if (getMemoryInfluence("pet", 300000) < 0.5f) {
        if (std::find(pet.traits.begin(), pet.traits.end(), NEEDY) == pet.traits.end()) {
            pet.traits.push_back(NEEDY);
        }
    }
}

void DigitalPetApp::processMirrorBehavior() {
    // Mirror mimics user behavior patterns
    float touchFreq = getMemoryInfluence("pet", 600000);
    float feedFreq = getMemoryInfluence("feed", 600000);
    
    if (touchFreq > feedFreq) {
        pet.mood = RESTLESS; // Mirrors user's touch-heavy behavior
    } else if (feedFreq > touchFreq) {
        pet.mood = OBSESSED; // Mirrors user's feeding focus
    }
    
    // Mirror says strange things (recorded as corrupted thoughts)
    if (random(100) < 5) { // 5% chance
        recordAction("mirror_echo", random(100) / 100.0f);
    }
}

String DigitalPetApp::getArchetypeResponse(String interaction) {
    switch (pet.archetype) {
        case ORACLE:
            if (interaction == "pet") return "The entropy whispers...";
            if (interaction == "feed") return "Energy flows through me";
            return "I see patterns...";
            
        case PARASITE:
            if (interaction == "pet") return "More... I need more...";
            if (interaction == "feed") return "Not enough! Never enough!";
            return "Why do you abandon me?";
            
        case MIRROR:
            if (interaction == "pet") return "You touch, I echo...";
            if (interaction == "feed") return "I taste what you give...";
            return "Your patterns... I learn...";
            
        default:
            return "...";
    }
}

void DigitalPetApp::applyEntropyInfluence() {
    // This is now handled in updateMood() and updateCorruption()
    // Keep for backward compatibility but redirect
    updateMood();
    updateCorruption();
}

void DigitalPetApp::handleStatDecay() {
    // In new system, decay is handled through corruption and memory system
    // This method is kept for compatibility but functionality moved to updateCorruption()
    updateCorruption();
}

void DigitalPetApp::checkPetHealth() {
    // Check if pet is still alive based on corruption level
    if (pet.corruptionLevel >= 1.0f) {
        pet.isAlive = false;
        pet.mood = GLITCHED;
        debugLog("Pet has been consumed by corruption!");
    }
    
    // Check for memory-based death (complete neglect)
    if (recentNeglect(3600000)) { // 1 hour of neglect
        pet.isAlive = false;
        debugLog("Pet died from neglect!");
    }
}

// ========================================
// PET SELECTION SYSTEM
// ========================================

void DigitalPetApp::showPetSelectionScreen() {
    displayManager.clearScreen(COLOR_BLACK);
    
    // Title
    displayManager.setFont(FONT_LARGE);
    displayManager.drawTextCentered(0, 20, SCREEN_WIDTH, "Choose Your Pet", COLOR_RED_GLOW);
    
    // Draw three archetype options
    int16_t optionY = 60;
    int16_t optionSpacing = 50;
    
    // Oracle option
    drawArchetypeOption(20, optionY, ORACLE, false);
    optionY += optionSpacing;
    
    // Parasite option
    drawArchetypeOption(20, optionY, PARASITE, false);
    optionY += optionSpacing;
    
    // Mirror option
    drawArchetypeOption(20, optionY, MIRROR, false);
    
    // Instructions
    displayManager.setFont(FONT_SMALL);
    displayManager.drawTextCentered(0, 210, SCREEN_WIDTH, "Touch to select", COLOR_LIGHT_GRAY);
}

void DigitalPetApp::drawArchetypeOption(int16_t x, int16_t y, PetArchetype archetype, bool selected) {
    uint16_t color = selected ? COLOR_GREEN_PHOS : COLOR_WHITE;
    uint16_t bgColor = selected ? COLOR_DARK_GRAY : COLOR_BLACK;
    
    // Draw background if selected
    if (selected) {
        displayManager.drawRetroRect(x-5, y-5, 280, 40, bgColor, true);
    }
    
    displayManager.setFont(FONT_MEDIUM);
    
    switch (archetype) {
        case ORACLE:
            displayManager.drawText(x, y, "ORACLE", COLOR_PURPLE_GLOW);
            displayManager.setFont(FONT_SMALL);
            displayManager.drawText(x, y + 15, "Wise entropy reader. Offers cryptic visions.", color);
            break;
            
        case PARASITE:
            displayManager.drawText(x, y, "PARASITE", COLOR_RED_GLOW);
            displayManager.setFont(FONT_SMALL);
            displayManager.drawText(x, y + 15, "Clingy companion. Drains battery, hates neglect.", color);
            break;
            
        case MIRROR:
            displayManager.drawText(x, y, "MIRROR", COLOR_BLUE_CYBER);
            displayManager.setFont(FONT_SMALL);
            displayManager.drawText(x, y + 15, "Mimics your patterns. Says unsettling things.", color);
            break;
    }
}

bool DigitalPetApp::handlePetSelection(TouchPoint touch) {
    // Check which archetype was touched
    if (touch.y >= 60 && touch.y <= 100) {
        selectArchetype(ORACLE);
        return true;
    } else if (touch.y >= 110 && touch.y <= 150) {
        selectArchetype(PARASITE);
        return true;
    } else if (touch.y >= 160 && touch.y <= 200) {
        selectArchetype(MIRROR);
        return true;
    }
    
    return false;
}

void DigitalPetApp::selectArchetype(PetArchetype archetype) {
    debugLog("Selected archetype: " + String(archetype));
    
    // Initialize the pet with chosen archetype
    initializeArchetype(archetype);
    
    // Save the pet type
    savePetType();
    
    // Create default pet data
    createDefaultPet(archetype);
    
    // Hide selection screen
    showPetSelection = false;
    firstBoot = false;
    
    debugLog("Pet selection complete: " + pet.name);
}

bool DigitalPetApp::loadPetType() {
    if (!SD.exists(petTypeFilePath)) {
        return false;
    }
    
    File file = SD.open(petTypeFilePath, FILE_READ);
    if (!file) {
        return false;
    }
    
    String typeStr = file.readString();
    file.close();
    
    typeStr.trim();
    
    if (typeStr == "ORACLE") {
        pet.archetype = ORACLE;
    } else if (typeStr == "PARASITE") {
        pet.archetype = PARASITE;
    } else if (typeStr == "MIRROR") {
        pet.archetype = MIRROR;
    } else {
        return false;
    }
    
    initializeArchetype(pet.archetype);
    return true;
}

void DigitalPetApp::savePetType() {
    createAppDataDir();
    
    File file = SD.open(petTypeFilePath, FILE_WRITE);
    if (!file) {
        debugLog("Failed to save pet type");
        return;
    }
    
    switch (pet.archetype) {
        case ORACLE:
            file.print("ORACLE");
            break;
        case PARASITE:
            file.print("PARASITE");
            break;
        case MIRROR:
            file.print("MIRROR");
            break;
    }
    
    file.close();
    debugLog("Pet type saved");
}

// ========================================
// INTERACTION METHODS (Updated)
// ========================================

void DigitalPetApp::interactWithPet() {
    if (!pet.isAlive) return;
    
    recordAction("pet", 1.0f);
    pet.totalInteractions++;
    
    // Get archetype-specific response
    String response = getArchetypeResponse("pet");
    debugLog("Pet response: " + response);
    
    // Show happy animation
    setAnimation(happyAnimation, 3, false);
}

void DigitalPetApp::feedPet() {
    if (!pet.isAlive) return;
    
    recordAction("feed", 1.0f);
    pet.totalInteractions++;
    
    // Get archetype-specific response
    String response = getArchetypeResponse("feed");
    debugLog("Pet response: " + response);
}

void DigitalPetApp::punishPet() {
    if (!pet.isAlive) return;
    
    recordAction("punish", 1.5f);
    pet.totalInteractions++;
    
    // Increase corruption from punishment
    pet.corruptionLevel = min(1.0f, pet.corruptionLevel + 0.1f);
    
    debugLog("Pet punished - corruption increased");
}

void DigitalPetApp::observePet() {
    if (!pet.isAlive) return;
    
    pet.isObservingUser = true;
    recordAction("observe", 0.5f);
    
    debugLog("Observing pet...");
}

// ========================================
// MISSING METHOD IMPLEMENTATIONS
// ========================================

void DigitalPetApp::playWithPet() {
    if (!pet.isAlive) return;
    
    recordAction("play", 1.2f);
    pet.totalInteractions++;
    
    // Get archetype-specific response
    String response = getArchetypeResponse("play");
    debugLog("Pet response: " + response);
    
    // Show happy animation
    setAnimation(happyAnimation, 3, false);
}

void DigitalPetApp::putPetToSleep() {
    if (!pet.isAlive) return;
    
    recordAction("sleep", 0.8f);
    pet.totalInteractions++;
    pet.isAwake = false;
    
    debugLog("Pet is now sleeping");
}

void DigitalPetApp::petPet() {
    // Alias for interactWithPet for compatibility
    interactWithPet();
}

void DigitalPetApp::createDefaultPet(PetArchetype archetype) {
    // Initialize new psychological model
    pet.mood = CALM;
    pet.traits.clear();
    pet.corruptionLevel = 0.0f;
    pet.isAwake = true;
    pet.isObservingUser = false;
    pet.memory.clear();
    pet.personalitySeed = random(0xFFFFFFFF);
    pet.archetype = archetype;
    pet.birthTime = millis();
    pet.lastUpdate = millis();
    pet.totalInteractions = 0;
    pet.isAlive = true;
    
    // Initialize archetype-specific data
    initializeArchetype(archetype);
    
    debugLog("Created default pet: " + pet.name);
}

// ========================================
// RENDERING METHODS
// ========================================

void DigitalPetApp::drawArchetypeSpecificElements(int16_t x, int16_t y) {
    switch (pet.archetype) {
        case ORACLE:
            // Draw entropy reading symbols around Oracle
            if (getCurrentEntropy() > 0.7f) {
                displayManager.setFont(FONT_SMALL);
                displayManager.drawText(x - 20, y, "※", COLOR_PURPLE_GLOW);
                displayManager.drawText(x + 35, y, "※", COLOR_PURPLE_GLOW);
                displayManager.drawText(x + 8, y - 20, "◊", COLOR_BLUE_CYBER);
            }
            break;
            
        case PARASITE:
            // Draw battery drain indicator
            if (std::find(pet.traits.begin(), pet.traits.end(), NEEDY) != pet.traits.end()) {
                displayManager.drawText(x + 20, y - 10, "⚡", COLOR_RED_GLOW);
            }
            // Draw clinging tendrils when highly corrupted
            if (isHighlyCorrupted()) {
                displayManager.drawRetroLine(x, y + 16, x - 10, y + 25, COLOR_RED_GLOW);
                displayManager.drawRetroLine(x + 16, y + 16, x + 26, y + 25, COLOR_RED_GLOW);
            }
            break;
            
        case MIRROR:
            // Draw reflection/echo effects
            if (pet.mood == RESTLESS) {
                // Draw shadow/mirror image slightly offset
                displayManager.drawIcon(x + 2, y + 2, PET_SPRITE_IDLE, COLOR_DARK_GRAY);
            }
            // Draw paranoid surveillance symbols
            if (std::find(pet.traits.begin(), pet.traits.end(), PARANOID) != pet.traits.end()) {
                displayManager.drawText(x - 15, y - 15, "👁", COLOR_BLUE_CYBER);
            }
            break;
    }
}

String DigitalPetApp::corrupted_text(String original) {
    String corrupted = original;
    int corruptChars = pet.corruptionLevel * original.length();
    
    for (int i = 0; i < corruptChars && i < original.length(); i++) {
        int pos = random(original.length());
        char glitchChars[] = {'#', '@', '$', '%', '!', '?', '*'};
        corrupted.setCharAt(pos, glitchChars[random(7)]);
    }
    
    return corrupted;
}

bool DigitalPetApp::isCorrupted() {
    return pet.corruptionLevel > CORRUPTION_THRESHOLD_LOW;
}

bool DigitalPetApp::isHighlyCorrupted() {
    return pet.corruptionLevel > CORRUPTION_THRESHOLD_HIGH;
}

void DigitalPetApp::drawCorruptionOverlay() {
    if (!isCorrupted()) return;
    
    // Draw corruption static using small rectangles
    int staticIntensity = pet.corruptionLevel * 10;
    for (int i = 0; i < staticIntensity; i++) {
        int x = random(SCREEN_WIDTH - 2);
        int y = random(SCREEN_HEIGHT - 2);
        displayManager.drawRetroRect(x, y, 2, 2, COLOR_RED_GLOW, true);
    }
    
    // Draw corruption lines
    for (int i = 0; i < pet.corruptionLevel * 3; i++) {
        int y = random(SCREEN_HEIGHT);
        displayManager.drawRetroLine(0, y, SCREEN_WIDTH, y, COLOR_RED_GLOW);
    }
}

void DigitalPetApp::drawGlitchEffects() {
    if (!isHighlyCorrupted()) return;
    
    // Screen tear effects
    if (random(100) < 10) {
        int tearY = random(SCREEN_HEIGHT - 20);
        for (int i = 0; i < 5; i++) {
            displayManager.drawRetroLine(0, tearY + i, SCREEN_WIDTH, tearY + i, COLOR_PURPLE_GLOW);
        }
    }
    
    // Color channel shifts
    if (random(100) < 5) {
        // Draw shifted colored rectangles
        displayManager.drawRetroRect(random(SCREEN_WIDTH-20), random(SCREEN_HEIGHT-20),
                                   20, 20, COLOR_RED_GLOW, false);
        displayManager.drawRetroRect(random(SCREEN_WIDTH-20), random(SCREEN_HEIGHT-20),
                                   20, 20, COLOR_GREEN_PHOS, false);
    }
}

void DigitalPetApp::drawReactiveRoom() {
    // Draw room background that changes based on pet treatment history
    updateRoomTheme();
    
    // Draw floor
    displayManager.drawRetroLine(20, 150, SCREEN_WIDTH - 20, 150, COLOR_MID_GRAY);
    
    // Draw theme-specific decorations
    switch (currentRoomTheme) {
        case THEME_LOVING:
            // Warm, welcoming room
            displayManager.drawText(30, 135, "♥", COLOR_GREEN_PHOS);
            displayManager.drawText(250, 135, "♥", COLOR_GREEN_PHOS);
            displayManager.drawRetroRect(40, 140, 20, 8, COLOR_GREEN_PHOS, true); // Food bowl
            break;
            
        case THEME_GLITCHED:
            // Cold, empty room
            displayManager.drawText(50, 135, "...", COLOR_DARK_GRAY);
            displayManager.drawRetroRect(250, 140, 16, 8, COLOR_DARK_GRAY, true); // Empty bowl
            break;
            
        case THEME_NEEDY:
            // Aggressive, spiky decorations
            displayManager.drawText(30, 130, "⚡", COLOR_RED_GLOW);
            displayManager.drawText(260, 130, "⚡", COLOR_RED_GLOW);
            // Draw warning signs
            displayManager.drawText(150, 135, "!!", COLOR_RED_GLOW);
            break;
            
        case THEME_PARANOID:
            // Glitched, unstable room
            if (random(100) < 20) {
                displayManager.drawText(random(SCREEN_WIDTH-20), 135, "#", COLOR_PURPLE_GLOW);
                displayManager.drawText(random(SCREEN_WIDTH-20), 140, "@", COLOR_RED_GLOW);
            }
            // Flickering walls
            if (frameCount % 10 < 3) {
                displayManager.drawRetroLine(10, 50, 10, 150, COLOR_PURPLE_GLOW);
                displayManager.drawRetroLine(SCREEN_WIDTH-10, 50, SCREEN_WIDTH-10, 150, COLOR_RED_GLOW);
            }
            break;
    }
}

void DigitalPetApp::updateRoomTheme() {
    // Determine room theme based on recent treatment
    float loveInfluence = getMemoryInfluence("pet", 600000) + getMemoryInfluence("feed", 600000);
    float punishInfluence = getMemoryInfluence("punish", 600000);
    bool hasNeglect = recentNeglect(600000);
    
    if (isHighlyCorrupted()) {
        currentRoomTheme = THEME_PARANOID;
    } else if (punishInfluence > 1.0f) {
        currentRoomTheme = THEME_NEEDY;
    } else if (hasNeglect || loveInfluence < 0.5f) {
        currentRoomTheme = THEME_GLITCHED;
    } else {
        currentRoomTheme = THEME_LOVING;
    }
}

void DigitalPetApp::drawPet() {
    // Draw pet sprite in center
    int16_t petX = SCREEN_WIDTH / 2 - 16;
    int16_t petY = SCREEN_HEIGHT / 2 - 16;
    
    drawAnimatedSprite(petX, petY);
    
    // Draw archetype-specific visual elements
    drawArchetypeSpecificElements(petX, petY);
    
    // Draw pet name with archetype indicator
    displayManager.setFont(FONT_MEDIUM);
    String displayName = pet.name;
    if (isCorrupted()) {
        // Occasionally corrupt the name display
        if (random(100) < pet.corruptionLevel * 50) {
            displayName = corrupted_text(displayName);
        }
    }
    displayManager.drawTextCentered(0, 40, SCREEN_WIDTH, displayName, COLOR_GREEN_PHOS);
}

void DigitalPetApp::drawAnimatedSprite(int16_t x, int16_t y) {
    if (!currentAnimation || animationFrameCount == 0) {
        // Fallback to idle sprite
        displayManager.drawIcon(x, y, PET_SPRITE_IDLE, COLOR_WHITE);
        return;
    }
    
    // Draw current animation frame
    const uint8_t* spriteData = currentAnimation[currentAnimFrame].spriteData;
    
    // Color based on mood (updated for new system)
    uint16_t spriteColor = COLOR_WHITE;
    switch (pet.mood) {
        case CALM: spriteColor = COLOR_GREEN_PHOS; break;
        case RESTLESS: spriteColor = COLOR_GREEN_PHOS; break;
        case OBSESSED: spriteColor = COLOR_RED_GLOW; break;
        case GLITCHED: spriteColor = COLOR_PURPLE_GLOW; break;
        default: spriteColor = COLOR_WHITE; break;
    }
    
    // Apply corruption effects to color (simplified without blendColors)
    if (isCorrupted()) {
        // Alternate between original color and corruption color based on corruption level
        if (random(100) < pet.corruptionLevel * 50) {
            spriteColor = COLOR_RED_GLOW;
        }
    }
    
    displayManager.drawIcon(x, y, spriteData, spriteColor);
}

void DigitalPetApp::drawMoodIndicator() {
    // Draw mood text
    displayManager.setFont(FONT_SMALL);
    String moodText = "Mood: ";
    
    switch (pet.mood) {
        case CALM: moodText += "Calm"; break;
        case RESTLESS: moodText += "Restless"; break;
        case OBSESSED: moodText += "Obsessed"; break;
        case GLITCHED: moodText += "Glitched"; break;
    }
    
    // Add corruption indicator
    if (isCorrupted()) {
        moodText += " [CORRUPT:" + String(int(pet.corruptionLevel * 100)) + "%]";
    }
    
    displayManager.drawText(10, 220, moodText, COLOR_GREEN_PHOS);
    
    // Draw traits
    if (!pet.traits.empty()) {
        String traitsText = "Traits: ";
        for (size_t i = 0; i < pet.traits.size(); i++) {
            switch (pet.traits[i]) {
                case LOVING: traitsText += "♥"; break;
                case AGGRESSIVE: traitsText += "⚡"; break;
                case NEEDY: traitsText += "◎"; break;
                case PARANOID: traitsText += "※"; break;
            }
            if (i < pet.traits.size() - 1) traitsText += " ";
        }
        displayManager.drawText(10, 205, traitsText, COLOR_BLUE_CYBER);
    }
}

void DigitalPetApp::drawStatsDisplay() {
    displayManager.clearScreen(COLOR_BLACK);
    
    // Title
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawTextCentered(0, 10, SCREEN_WIDTH, "Pet Stats", COLOR_RED_GLOW);
    
    displayManager.setFont(FONT_SMALL);
    int16_t yPos = 50;
    int16_t lineSpacing = 18;
    
    // Pet identity
    displayManager.drawText(10, yPos, "Name: " + pet.name, COLOR_GREEN_PHOS);
    yPos += lineSpacing;
    
    // Archetype
    String archetypeStr = "Archetype: ";
    switch (pet.archetype) {
        case ORACLE: archetypeStr += "Oracle"; break;
        case PARASITE: archetypeStr += "Parasite"; break;
        case MIRROR: archetypeStr += "Mirror"; break;
    }
    displayManager.drawText(10, yPos, archetypeStr, COLOR_PURPLE_GLOW);
    yPos += lineSpacing;
    
    // Current mood
    String moodStr = "Mood: ";
    switch (pet.mood) {
        case CALM: moodStr += "Calm"; break;
        case RESTLESS: moodStr += "Restless"; break;
        case OBSESSED: moodStr += "Obsessed"; break;
        case GLITCHED: moodStr += "Glitched"; break;
    }
    displayManager.drawText(10, yPos, moodStr, COLOR_BLUE_CYBER);
    yPos += lineSpacing;
    
    // Corruption level
    displayManager.drawText(10, yPos, "Corruption:", COLOR_WHITE);
    int corruptionPercent = pet.corruptionLevel * 100;
    displayManager.drawProgressBar(100, yPos, 150, 10, corruptionPercent,
                                  corruptionPercent > 70 ? COLOR_RED_GLOW : COLOR_YELLOW);
    yPos += lineSpacing;
    
    // Active traits
    String traitsStr = "Traits: ";
    for (size_t i = 0; i < pet.traits.size(); i++) {
        switch (pet.traits[i]) {
            case LOVING: traitsStr += "♥"; break;
            case AGGRESSIVE: traitsStr += "⚡"; break;
            case NEEDY: traitsStr += "◎"; break;
            case PARANOID: traitsStr += "※"; break;
        }
        if (i < pet.traits.size() - 1) traitsStr += " ";
    }
    displayManager.drawText(10, yPos, traitsStr, COLOR_BLUE_CYBER);
    yPos += lineSpacing;
    
    // Memory entries
    displayManager.drawText(10, yPos, "Memories: " + String(pet.memory.size()), COLOR_LIGHT_GRAY);
    yPos += lineSpacing;
    
    // Pet info
    displayManager.drawText(10, yPos, "Age: " + String(getPetAge()) + " hours", COLOR_LIGHT_GRAY);
    yPos += lineSpacing;
    displayManager.drawText(10, yPos, "Interactions: " + String(pet.totalInteractions), COLOR_LIGHT_GRAY);
    yPos += lineSpacing;
    
    // Status
    String statusStr = "Status: ";
    statusStr += pet.isAlive ? "Alive" : "Dead";
    statusStr += pet.isAwake ? " (Awake)" : " (Sleeping)";
    displayManager.drawText(10, yPos, statusStr, pet.isAlive ? COLOR_GREEN_PHOS : COLOR_RED_GLOW);
    
    // Close instruction
    displayManager.drawTextCentered(0, 210, SCREEN_WIDTH, "Touch to close", COLOR_LIGHT_GRAY);
}

void DigitalPetApp::drawInteractionButtons() {
    // Draw interaction buttons at bottom
    int16_t buttonY = 180;
    int16_t buttonW = 60;
    int16_t buttonH = 20;
    int16_t spacing = 5;
    
    displayManager.setFont(FONT_SMALL);
    
    // Feed button
    displayManager.drawButton(10, buttonY, buttonW, buttonH, "Feed");
    
    // Play button  
    displayManager.drawButton(10 + buttonW + spacing, buttonY, buttonW, buttonH, "Play");
    
    // Sleep button
    displayManager.drawButton(10 + 2 * (buttonW + spacing), buttonY, buttonW, buttonH, "Sleep");
    
    // Stats button
    displayManager.drawButton(10 + 3 * (buttonW + spacing), buttonY, buttonW, buttonH, "Stats");
}

void DigitalPetApp::drawBackground() {
    // Draw retro background pattern
    displayManager.drawASCIIBorder(5, 5, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 50, COLOR_DARK_GRAY);
}

void DigitalPetApp::drawPetRoom() {
    // Legacy method - functionality moved to drawReactiveRoom()
    drawReactiveRoom();
}

void DigitalPetApp::drawASCIIMood(int16_t x, int16_t y, PetMood mood) {
    displayManager.setFont(FONT_SMALL);
    uint16_t color = COLOR_WHITE;
    String symbol = ":)";
    
    switch (mood) {
        case CALM:
            symbol = ":)";
            color = COLOR_GREEN_PHOS;
            break;
        case RESTLESS:
            symbol = ":/";
            color = COLOR_YELLOW;
            break;
        case OBSESSED:
            symbol = "O_O";
            color = COLOR_RED_GLOW;
            break;
        case GLITCHED:
            symbol = "@_@";
            color = COLOR_PURPLE_GLOW;
            break;
    }
    
    displayManager.drawText(x, y, symbol, color);
}

// ========================================
// MISSING BASEAPP METHOD IMPLEMENTATIONS
// ========================================

bool DigitalPetApp::handleMessage(AppMessage message) {
    // Handle inter-app messages
    switch (message.type) {
        case MSG_ENTROPY_UPDATE:
            // Could receive entropy data from other apps
            if (message.data) {
                float* entropyValue = (float*)message.data;
                // Use external entropy data to influence pet
                if (*entropyValue > 0.8f) {
                    pet.corruptionLevel = min(1.0f, pet.corruptionLevel + 0.005f);
                }
            }
            return true;

        case MSG_BATTERY_LOW:
            // Parasite archetype reacts to low battery
            if (pet.archetype == PARASITE) {
                recordAction("battery_drain", 2.0f);
            }
            return true;

        default:
            return false;
    }
}

// ========================================
// ANIMATION SYSTEM
// ========================================

void DigitalPetApp::updateAnimation() {
    if (!currentAnimation || animationFrameCount == 0) return;
    
    unsigned long currentTime = millis();
    uint16_t frameDuration = currentAnimation[currentAnimFrame].duration;
    
    if (currentTime - lastAnimation >= frameDuration) {
        currentAnimFrame++;
        
        if (currentAnimFrame >= animationFrameCount) {
            if (animationLoop) {
                currentAnimFrame = 0;
            } else {
                // Animation finished, return to idle
                setAnimation(idleAnimation, 3, true);
            }
        }
        
        lastAnimation = currentTime;
    }
}

void DigitalPetApp::setAnimation(AnimationFrame* frames, uint8_t frameCount, bool loop) {
    currentAnimation = frames;
    animationFrameCount = frameCount;
    animationLoop = loop;
    currentAnimFrame = 0;
    lastAnimation = millis();
}

// ========================================
// FILE I/O SYSTEM
// ========================================

bool DigitalPetApp::loadPetData() {
    if (!SD.exists(saveFilePath)) {
        debugLog("No save file found at: " + saveFilePath);
        return false;
    }
    
    File file = SD.open(saveFilePath, FILE_READ);
    if (!file) {
        debugLog("Failed to open save file");
        return false;
    }
    
    // Read JSON data
    DynamicJsonDocument doc(2048); // Increased size for new data structures
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        debugLog("Failed to parse save file: " + String(error.c_str()));
        return false;
    }
    
    // Validate and load data
    if (!validateSaveData(doc)) {
        debugLog("Invalid save data");
        return false;
    }
    
    // Load new psychological state
    pet.mood = (PetMood)doc["mood"].as<int>();
    pet.corruptionLevel = doc["corruptionLevel"];
    pet.isAwake = doc["isAwake"];
    pet.isObservingUser = doc["isObservingUser"];
    pet.personalitySeed = doc["personalitySeed"];
    
    // Load traits
    pet.traits.clear();
    JsonArray traitsArray = doc["traits"];
    for (JsonVariant trait : traitsArray) {
        pet.traits.push_back((PetTrait)trait.as<int>());
    }
    
    // Load archetype and identity
    pet.archetype = (PetArchetype)doc["archetype"].as<int>();
    pet.name = doc["name"].as<String>();
    pet.birthTime = doc["birthTime"];
    pet.lastUpdate = millis();
    pet.totalInteractions = doc["totalInteractions"];
    pet.isAlive = doc["isAlive"];
    
    // Load memory (simplified - just timestamp and action type)
    pet.memory.clear();
    JsonArray memoryArray = doc["memory"];
    for (JsonVariant mem : memoryArray) {
        PetMemory memory;
        memory.action = mem["action"].as<String>();
        memory.timestamp = mem["timestamp"];
        memory.intensity = mem["intensity"];
        pet.memory.push_back(memory);
    }
    
    debugLog("Pet data loaded successfully: " + pet.name);
    return true;
}

bool DigitalPetApp::savePetData() {
    // Create directory if it doesn't exist
    createAppDataDir();
    
    File file = SD.open(saveFilePath, FILE_WRITE);
    if (!file) {
        debugLog("Failed to open save file for writing");
        return false;
    }
    
    // Create JSON document
    DynamicJsonDocument doc(2048); // Increased size for new data structures
    
    // Save new psychological state
    doc["mood"] = (int)pet.mood;
    doc["corruptionLevel"] = pet.corruptionLevel;
    doc["isAwake"] = pet.isAwake;
    doc["isObservingUser"] = pet.isObservingUser;
    doc["personalitySeed"] = pet.personalitySeed;
    
    // Save traits
    JsonArray traitsArray = doc.createNestedArray("traits");
    for (PetTrait trait : pet.traits) {
        traitsArray.add((int)trait);
    }
    
    // Save archetype and identity
    doc["archetype"] = (int)pet.archetype;
    doc["name"] = pet.name;
    doc["birthTime"] = pet.birthTime;
    doc["totalInteractions"] = pet.totalInteractions;
    doc["isAlive"] = pet.isAlive;
    
    // Save memory (limited to recent entries to save space)
    JsonArray memoryArray = doc.createNestedArray("memory");
    int memoryCount = 0;
    for (auto it = pet.memory.rbegin(); it != pet.memory.rend() && memoryCount < 20; ++it, ++memoryCount) {
        JsonObject memObj = memoryArray.createNestedObject();
        memObj["action"] = it->action;
        memObj["timestamp"] = it->timestamp;
        memObj["intensity"] = it->intensity;
    }
    
    // Add metadata
    doc["version"] = "2.0";
    doc["saveTime"] = systemCore.getUptimeSeconds();
    
    // Write to file
    serializeJson(doc, file);
    file.close();
    
    debugLog("Pet data saved successfully: " + pet.name);
    return true;
}

// This method was already replaced above with the proper archetype version

bool DigitalPetApp::validateSaveData(JsonDocument& doc) {
    // Basic validation of new save file structure
    return doc.containsKey("mood") &&
           doc.containsKey("archetype") &&
           doc.containsKey("name") &&
           doc.containsKey("corruptionLevel") &&
           doc.containsKey("birthTime");
}

// ========================================
// UTILITY METHODS
// ========================================

unsigned long DigitalPetApp::getPetAge() const {
    return (millis() - pet.birthTime) / 3600000; // Age in hours
}

void DigitalPetApp::setPetName(String name) {
    if (name.length() > 0 && name.length() <= 12) {
        pet.name = name;
        savePetData();
    }
}

void DigitalPetApp::drawCustomizationMenu() {
    displayManager.clearScreen(COLOR_BLACK);
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawTextCentered(0, 20, SCREEN_WIDTH, "Pet Debug Menu", COLOR_RED_GLOW);
    
    displayManager.setFont(FONT_SMALL);
    
    // Show current corruption level
    displayManager.drawText(20, 60, "Corruption: " + String(int(pet.corruptionLevel * 100)) + "%", COLOR_WHITE);
    
    // Show entropy reading
    displayManager.drawText(20, 80, "Entropy: " + String(int(getCurrentEntropy() * 100)) + "%", COLOR_WHITE);
    
    // Show memory count
    displayManager.drawText(20, 100, "Memories: " + String(pet.memory.size()), COLOR_WHITE);
    
    // Show recent actions
    displayManager.drawText(20, 120, "Recent Actions:", COLOR_WHITE);
    int y = 140;
    int count = 0;
    for (auto it = pet.memory.rbegin(); it != pet.memory.rend() && count < 3; ++it, ++count) {
        displayManager.drawText(30, y, it->action, COLOR_LIGHT_GRAY);
        y += 15;
    }
    
    displayManager.drawTextCentered(0, 200, SCREEN_WIDTH, "Touch to close", COLOR_LIGHT_GRAY);
}

// ========================================
// CORRUPTION-BASED EVOLUTION SYSTEM
// ========================================

void DigitalPetApp::processCorruptionEvolution() {
    // Handle corruption-based mutations and evolution
    if (pet.corruptionLevel > 0.8f && random(1000) < 5) { // 0.5% chance per update
        triggerCorruptionMutation();
    }
    
    // Handle memory-based trait evolution
    if (pet.memory.size() > 30 && random(1000) < 2) { // 0.2% chance with sufficient memory
        evolvePersonalityTrait();
    }
}

void DigitalPetApp::triggerCorruptionMutation() {
    // Add aggressive trait from high corruption
    if (pet.corruptionLevel > 0.9f) {
        if (std::find(pet.traits.begin(), pet.traits.end(), AGGRESSIVE) == pet.traits.end()) {
            pet.traits.push_back(AGGRESSIVE);
            recordAction("corruption_mutation", 3.0f);
            debugLog("Pet mutated: gained AGGRESSIVE trait from corruption");
        }
    }
    
    // Paranoid evolution from prolonged high corruption
    if (pet.corruptionLevel > 0.7f) {
        if (std::find(pet.traits.begin(), pet.traits.end(), PARANOID) == pet.traits.end()) {
            pet.traits.push_back(PARANOID);
            recordAction("paranoia_evolution", 2.5f);
            debugLog("Pet evolved: gained PARANOID trait");
        }
    }
}

void DigitalPetApp::evolvePersonalityTrait() {
    // Analyze memory patterns to evolve personality
    float loveRatio = getMemoryInfluence("pet", 1800000) + getMemoryInfluence("feed", 1800000); // 30 min window
    float neglectRatio = getMemoryInfluence("neglect", 1800000);
    float punishRatio = getMemoryInfluence("punish", 1800000);
    
    if (loveRatio > 3.0f && neglectRatio < 0.5f) {
        // Evolve to LOVING if consistently well-treated
        if (std::find(pet.traits.begin(), pet.traits.end(), LOVING) == pet.traits.end()) {
            pet.traits.push_back(LOVING);
            debugLog("Pet evolved: gained LOVING trait from good treatment");
        }
    } else if (neglectRatio > 2.0f) {
        // Evolve to NEEDY if consistently neglected
        if (std::find(pet.traits.begin(), pet.traits.end(), NEEDY) == pet.traits.end()) {
            pet.traits.push_back(NEEDY);
            debugLog("Pet evolved: gained NEEDY trait from neglect");
        }
    }
}

// ========================================
// PUBLIC API METHODS FOR EXTERNAL MANIPULATION
// ========================================

void DigitalPetApp::increaseCorruption(float amount) {
    pet.corruptionLevel = min(1.0f, pet.corruptionLevel + amount);
    recordAction("external_corruption", amount);
}

void DigitalPetApp::decreaseCorruption(float amount) {
    pet.corruptionLevel = max(0.0f, pet.corruptionLevel - amount);
    recordAction("external_healing", amount);
}

bool DigitalPetApp::hasRecentMemory(String actionType, unsigned long timeWindowMs) {
    return getMemoryInfluence(actionType, timeWindowMs) > 0.0f;
}

float DigitalPetApp::getMemoryInfluenceForAction(String actionType) {
    return getMemoryInfluence(actionType, 600000); // 10 minute default window
}

void DigitalPetApp::debugPrintMemory() {
    debugLog("=== PET MEMORY DEBUG ===");
    debugLog("Total memories: " + String(pet.memory.size()));
    debugLog("Corruption level: " + String(pet.corruptionLevel));
    debugLog("Current mood: " + String(pet.mood));
    
    int count = 0;
    for (auto it = pet.memory.rbegin(); it != pet.memory.rend() && count < 10; ++it, ++count) {
        debugLog("Memory " + String(count) + ": " + it->action + " (intensity: " + String(it->intensity) + ")");
    }
    debugLog("=== END MEMORY DEBUG ===");
}

void DigitalPetApp::debugResetPet() {
    createDefaultPet(pet.archetype);
    debugLog("Pet has been reset to default state");
}

void DigitalPetApp::debugSetCorruption(float level) {
    pet.corruptionLevel = constrain(level, 0.0f, 1.0f);
    debugLog("Corruption level set to: " + String(pet.corruptionLevel));
}

void DigitalPetApp::debugTriggerMood(PetMood mood) {
    pet.mood = mood;
    debugLog("Mood manually set to: " + String(mood));
}

void DigitalPetApp::debugAddMemory(String action, float intensity) {
    recordAction(action, intensity);
    debugLog("Added debug memory: " + action);
}

void DigitalPetApp::debugPrintState() {
    debugLog("=== PET STATE DEBUG ===");
    debugLog("Name: " + pet.name);
    debugLog("Archetype: " + String(pet.archetype));
    debugLog("Mood: " + String(pet.mood));
    debugLog("Corruption: " + String(pet.corruptionLevel));
    debugLog("Is Alive: " + String(pet.isAlive));
    debugLog("Is Awake: " + String(pet.isAwake));
    debugLog("Trait count: " + String(pet.traits.size()));
    debugLog("Memory count: " + String(pet.memory.size()));
    debugLog("Total interactions: " + String(pet.totalInteractions));
    debugLog("=== END STATE DEBUG ===");
}

// ========================================
// MISSING METHOD IMPLEMENTATIONS
// ========================================

void DigitalPetApp::showPetStats() {
    showStats = true;
}

void DigitalPetApp::drawPetSprite(int16_t x, int16_t y) {
    // Alias for drawAnimatedSprite for compatibility
    drawAnimatedSprite(x, y);
}

void DigitalPetApp::drawCorruptedSprite(int16_t x, int16_t y) {
    // Draw sprite with corruption effects
    const uint8_t* spriteData = PET_SPRITE_IDLE;
    if (currentAnimation && currentAnimFrame < animationFrameCount) {
        spriteData = currentAnimation[currentAnimFrame].spriteData;
    }
    
    uint16_t color = COLOR_RED_GLOW; // Corrupted color
    if (isHighlyCorrupted()) {
        // Heavy corruption - alternating colors
        color = (frameCount % 4 < 2) ? COLOR_RED_GLOW : COLOR_PURPLE_GLOW;
    }
    
    displayManager.drawIcon(x, y, spriteData, color);
}

void DigitalPetApp::drawRoomTheme(RoomTheme theme) {
    // This functionality is now handled in drawReactiveRoom()
    // Keep for compatibility
    drawReactiveRoom();
}

void DigitalPetApp::drawOracleElements() {
    int16_t petX = SCREEN_WIDTH / 2 - 16;
    int16_t petY = SCREEN_HEIGHT / 2 - 16;
    
    if (getCurrentEntropy() > 0.7f) {
        displayManager.setFont(FONT_SMALL);
        displayManager.drawText(petX - 20, petY, "※", COLOR_PURPLE_GLOW);
        displayManager.drawText(petX + 35, petY, "※", COLOR_PURPLE_GLOW);
        displayManager.drawText(petX + 8, petY - 20, "◊", COLOR_BLUE_CYBER);
    }
}

void DigitalPetApp::drawParasiteElements() {
    int16_t petX = SCREEN_WIDTH / 2 - 16;
    int16_t petY = SCREEN_HEIGHT / 2 - 16;
    
    // Draw battery drain indicator
    if (std::find(pet.traits.begin(), pet.traits.end(), NEEDY) != pet.traits.end()) {
        displayManager.drawText(petX + 20, petY - 10, "⚡", COLOR_RED_GLOW);
    }
    
    // Draw clinging tendrils when highly corrupted
    if (isHighlyCorrupted()) {
        displayManager.drawRetroLine(petX, petY + 16, petX - 10, petY + 25, COLOR_RED_GLOW);
        displayManager.drawRetroLine(petX + 16, petY + 16, petX + 26, petY + 25, COLOR_RED_GLOW);
    }
}

void DigitalPetApp::drawMirrorElements() {
    int16_t petX = SCREEN_WIDTH / 2 - 16;
    int16_t petY = SCREEN_HEIGHT / 2 - 16;
    
    // Draw reflection/echo effects
    if (pet.mood == RESTLESS) {
        // Draw shadow/mirror image slightly offset
        displayManager.drawIcon(petX + 2, petY + 2, PET_SPRITE_IDLE, COLOR_DARK_GRAY);
    }
    
    // Draw paranoid surveillance symbols
    if (std::find(pet.traits.begin(), pet.traits.end(), PARANOID) != pet.traits.end()) {
        displayManager.drawText(petX - 15, petY - 15, "👁", COLOR_BLUE_CYBER);
    }
}

void DigitalPetApp::drawArchetypeSprite(int16_t x, int16_t y, PetArchetype archetype, PetMood mood) {
    // Choose sprite based on archetype and mood
    const uint8_t* sprite = PET_SPRITE_IDLE;
    uint16_t color = COLOR_WHITE;
    
    switch (archetype) {
        case ORACLE:
            sprite = (mood == OBSESSED) ? PET_SPRITE_HAPPY : PET_SPRITE_IDLE;
            color = COLOR_PURPLE_GLOW;
            break;
        case PARASITE:
            sprite = (mood == GLITCHED) ? PET_SPRITE_SAD : PET_SPRITE_IDLE;
            color = COLOR_RED_GLOW;
            break;
        case MIRROR:
            sprite = (mood == RESTLESS) ? PET_SPRITE_HAPPY : PET_SPRITE_IDLE;
            color = COLOR_BLUE_CYBER;
            break;
    }
    
    displayManager.drawIcon(x, y, sprite, color);
}

JsonDocument DigitalPetApp::memoryToJson() {
    DynamicJsonDocument doc(1024);
    JsonArray memoryArray = doc.createNestedArray("memory");
    
    int count = 0;
    for (auto it = pet.memory.rbegin(); it != pet.memory.rend() && count < 20; ++it, ++count) {
        JsonObject memObj = memoryArray.createNestedObject();
        memObj["action"] = it->action;
        memObj["timestamp"] = it->timestamp;
        memObj["intensity"] = it->intensity;
    }
    
    return doc;
}

void DigitalPetApp::memoryFromJson(JsonDocument& doc) {
    pet.memory.clear();
    JsonArray memoryArray = doc["memory"];
    
    for (JsonVariant mem : memoryArray) {
        PetMemory memory;
        memory.action = mem["action"].as<String>();
        memory.timestamp = mem["timestamp"];
        memory.intensity = mem["intensity"];
        pet.memory.push_back(memory);
    }
}

void DigitalPetApp::drawCorruptedText(String text, int16_t x, int16_t y, uint16_t color) {
    String corrupted = corrupted_text(text);
    displayManager.drawText(x, y, corrupted, color);
}

void DigitalPetApp::drawStaticNoise(int16_t x, int16_t y, int16_t w, int16_t h) {
    int noisePoints = (w * h) / 20; // Density control
    
    for (int i = 0; i < noisePoints; i++) {
        int px = x + random(w);
        int py = y + random(h);
        uint16_t noiseColor = random(2) ? COLOR_WHITE : COLOR_DARK_GRAY;
        displayManager.drawRetroRect(px, py, 1, 1, noiseColor, true);
    }
}

void DigitalPetApp::drawEntropyVisualization() {
    float entropy = getCurrentEntropy();
    int16_t barWidth = entropy * 60; // Scale to 60 pixels max
    
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(10, 10, "Entropy:", COLOR_WHITE);
    displayManager.drawRetroRect(70, 10, 62, 8, COLOR_DARK_GRAY, false);
    displayManager.drawRetroRect(71, 11, barWidth, 6, COLOR_RED_GLOW, true);
    displayManager.drawText(140, 10, String(int(entropy * 100)) + "%", COLOR_WHITE);
}

// ========================================
// UTILITY METHODS
// ========================================

void DigitalPetApp::cleanup() {
    // Save pet data before cleanup
    savePetData();
}

void DigitalPetApp::createAppDataDir() {
    if (!SD.exists("/apps")) {
        SD.mkdir("/apps");
    }
    if (!SD.exists("/apps/digitalpet")) {
        SD.mkdir("/apps/digitalpet");
    }
}

void DigitalPetApp::debugLog(String message) {
    Serial.println("[DigitalPet] " + message);
}
