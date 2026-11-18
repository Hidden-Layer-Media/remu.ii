#ifndef DIGITAL_PET_H
#define DIGITAL_PET_H

#include "../../core/AppManager/BaseApp.h"
#include "../../core/SystemCore/SystemCore.h"
#include <ArduinoJson.h>
#include <SD.h>
#include <vector>
#include <deque>

// ========================================
// DigitalPet - Cyberpet companion app for remu.ii
// Features mood tracking, entropy influence, customization, and persistence
// ========================================

// Memory and corruption configuration
#define MAX_MEMORY_ENTRIES 50
#define CORRUPTION_THRESHOLD_LOW 0.3f
#define CORRUPTION_THRESHOLD_HIGH 0.7f
#define ENTROPY_SAMPLE_INTERVAL 1000  // ms between entropy readings

// New pet mood states - more psychological than physical
enum PetMood {
    CALM,        // Low entropy, content state
    RESTLESS,    // Moderate entropy, seeking attention
    OBSESSED,    // Focused on specific behavior pattern
    GLITCHED     // High entropy, corrupted state
};

// Pet personality traits - can have multiple simultaneously
enum PetTrait {
    LOVING,      // Responds well to affection
    AGGRESSIVE,  // Reacts negatively to touch
    NEEDY,       // Requires frequent interaction
    PARANOID     // Suspicious of user actions
};

// Pet archetypes - defines core personality and behavior
enum PetArchetype {
    ORACLE,      // Wise, entropy-reading, offers visions
    PARASITE,    // Clingy, battery-draining, reacts to neglect
    MIRROR       // Mimics user patterns, unsettling behavior
};

// Room decoration based on treatment
enum RoomTheme {
    THEME_LOVING,    // Plants, warm colors
    THEME_GLITCHED,  // Static, corruption artifacts
    THEME_NEEDY,     // Toys, attention-seeking items
    THEME_PARANOID   // Red overlays, "hidden eyes"
};

// Memory entry for storing user interactions
struct PetMemory {
    String action;           // Type of interaction (feed, pet, touch, neglect, etc.)
    unsigned long timestamp; // When it occurred
    float intensity;         // How significant the interaction was (0.0-1.0)
};

// New pet state structure - psychological rather than physical
struct PetState {
    PetMood mood;                    // Current emotional state
    std::vector<PetTrait> traits;    // Active personality traits
    float corruptionLevel;           // 0.0 - 1.0, affects behavior/visuals
    bool isAwake;                    // Sleep/wake cycle
    bool isObservingUser;            // Whether pet is actively watching
    std::deque<PetMemory> memory;    // Interaction history buffer
    uint32_t personalitySeed;        // Unique personality modifier
    
    // Archetype and identity
    PetArchetype archetype;          // Core personality type
    String name;                     // Pet name
    unsigned long birthTime;         // When pet was created
    unsigned long lastUpdate;        // Last state update
    unsigned long totalInteractions; // Lifetime interaction count
    bool isAlive;                    // Still functioning
};

// Touch zones for interaction
struct TouchZone {
    int16_t x, y, w, h;
    String action;
    bool enabled;
};

// Animation frame data
struct AnimationFrame {
    const uint8_t* spriteData;
    uint16_t duration;  // ms to display this frame
};

class DigitalPetApp : public BaseApp {
private:
    // Pet state
    PetState pet;
    String saveFilePath;
    String petTypeFilePath;
    
    // UI state
    bool showStats;
    bool showPetSelection;
    bool showCustomization;  // Missing declaration - used in cpp
    bool firstBoot;
    unsigned long lastEntropyUpdate;
    unsigned long lastMoodUpdate;
    unsigned long lastAnimation;
    uint8_t currentAnimFrame;
    RoomTheme currentRoomTheme;
    unsigned long frameCount;  // Missing declaration - used for glitch effects
    
    // Touch zones
    TouchZone touchZones[8];
    int8_t activeTouchZone;
    
    // Animation data
    AnimationFrame* currentAnimation;
    uint8_t animationFrameCount;
    bool animationLoop;
    
    // Private methods - Memory System
    void recordAction(String action, float intensity = 1.0f);
    void updateMemoryBuffer();
    bool recentNeglect(unsigned long timeWindowMs = 300000); // 5 minutes
    bool wasRecentlyPunished(unsigned long timeWindowMs = 180000); // 3 minutes
    float getMemoryInfluence(String actionType, unsigned long timeWindowMs = 600000); // 10 minutes
    void clearOldMemories();
    
    // Private methods - Mood & Corruption System
    void updateMood();
    void updateCorruption();
    void applyEntropyInfluence();
    float getCurrentEntropy();
    void processCorruptionEffects();
    bool isCorrupted() { return pet.corruptionLevel > CORRUPTION_THRESHOLD_LOW; }
    bool isHighlyCorrupted() { return pet.corruptionLevel > CORRUPTION_THRESHOLD_HIGH; }
    
    // Private methods - Archetype System
    void initializeArchetype(PetArchetype archetype);
    void updateArchetypeBehavior();
    String getArchetypeResponse(String interaction);
    void processOracleBehavior();
    void processParasiteBehavior();
    void processMirrorBehavior();
    
    // Private methods - Pet Selection
    void showPetSelectionScreen();
    bool handlePetSelection(TouchPoint touch);
    void drawArchetypeOption(int16_t x, int16_t y, PetArchetype archetype, bool selected);
    void selectArchetype(PetArchetype archetype);
    bool loadPetType();
    void savePetType();
    
    // Private methods - Interactions (Updated)
    void interactWithPet();
    void feedPet();
    void punishPet();
    void observePet();
    void showPetStats();
    
    // Private methods - Rendering
    void drawPet();
    void drawPetSprite(int16_t x, int16_t y);
    void drawCorruptedSprite(int16_t x, int16_t y);
    void drawMoodIndicator();
    void drawStatsDisplay();
    void drawInteractionButtons();
    void drawReactiveRoom();
    void drawRoomTheme(RoomTheme theme);
    void drawGlitchEffects();
    void drawCorruptionOverlay();
    
    // Private methods - Archetype-specific rendering
    void drawOracleElements();
    void drawParasiteElements();
    void drawMirrorElements();
    void drawArchetypeSprite(int16_t x, int16_t y, PetArchetype archetype, PetMood mood);
    
    // Private methods - Animation
    void updateAnimation();
    void setAnimation(AnimationFrame* frames, uint8_t frameCount, bool loop = true);
    void drawAnimatedSprite(int16_t x, int16_t y);
    
    // Private methods - Touch handling
    void setupTouchZones();
    int8_t getTouchedZone(TouchPoint touch);
    void handleZoneTouch(int8_t zone);
    
    // Private methods - File I/O (Updated)
    bool loadPetData();
    bool savePetData();
    void createDefaultPet(PetArchetype archetype);
    bool validateSaveData(JsonDocument& doc);
    JsonDocument memoryToJson();
    void memoryFromJson(JsonDocument& doc);
    
    // Private methods - Visual Effects
        void drawCorruptedText(String text, int16_t x, int16_t y, uint16_t color);
        void drawStaticNoise(int16_t x, int16_t y, int16_t w, int16_t h);
        void drawEntropyVisualization();
        void debugLog(String message);

        // Animation definitions
    static AnimationFrame idleAnimation[];
    static AnimationFrame happyAnimation[];
    static AnimationFrame sadAnimation[];
    static AnimationFrame eatingAnimation[];
    static AnimationFrame sleepingAnimation[];
    static AnimationFrame playingAnimation[];
    
    // Sprite data (16x16 bitmaps)
    static const uint8_t PET_SPRITE_IDLE[32];
    static const uint8_t PET_SPRITE_HAPPY[32];
    static const uint8_t PET_SPRITE_SAD[32];
    static const uint8_t PET_SPRITE_SLEEPING[32];
    static const uint8_t PET_SPRITE_EATING[32];
    static const uint8_t PET_SPRITE_SICK[32];
    
    // Accessory sprites
    static const uint8_t ACCESSORY_HAT_SPRITE[32];
    static const uint8_t ACCESSORY_GLASSES_SPRITE[32];
    static const uint8_t ACCESSORY_BOWTIE_SPRITE[32];

public:
    DigitalPetApp();
    virtual ~DigitalPetApp();
    
    // Mandatory BaseApp methods
    bool initialize() override;
    void update() override;
    void render() override;
    bool handleTouch(TouchPoint touch) override;
    void cleanup() override;
    String getName() const override { return "DigitalPet"; }
    const uint8_t* getIcon() const override;
    
    // Optional BaseApp methods
    void onPause() override;
    void onResume() override;
    bool saveState() override;
    bool loadState() override;
    bool handleMessage(int messageType, void* data = nullptr) override;
    
    // Settings menu support
    uint8_t getSettingsCount() const override { return 4; }
    String getSettingName(uint8_t index) const override;
    void handleSetting(uint8_t index) override;
    
    // Pet-specific methods (Updated for new system)
    PetMood getCurrentMood() const { return pet.mood; }
    PetArchetype getArchetype() const { return pet.archetype; }
    float getCorruptionLevel() const { return pet.corruptionLevel; }
    String getPetName() const { return pet.name; }
    void setPetName(String name);
    bool isPetAlive() const { return pet.isAlive; }
    unsigned long getPetAge() const;
    size_t getMemoryCount() const { return pet.memory.size(); }
    
    // Memory system access
    bool hasRecentMemory(String actionType, unsigned long timeWindowMs = 300000);
    float getMemoryInfluenceForAction(String actionType);
    void debugPrintMemory();
    
    // Corruption system access
    void increaseCorruption(float amount);
    void decreaseCorruption(float amount);
    bool isCorruptionVisible() const { return pet.corruptionLevel > CORRUPTION_THRESHOLD_LOW; }
    
    // Debug methods (Updated)
    void debugResetPet();
    void debugSetCorruption(float level);
    void debugTriggerMood(PetMood mood);
    void debugAddMemory(String action, float intensity);
    void debugPrintState();
};

#endif // DIGITAL_PET_H