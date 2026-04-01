#include "DigitalPet.h"
#include "../../core/DisplayManager/DisplayManager.h"
#include "../../core/TouchInterface/TouchInterface.h"
#include "../../core/SystemCore/SystemCore.h"

DigitalPetApp::DigitalPetApp() : 
    petX(SCREEN_WIDTH/2),
    petY(SCREEN_HEIGHT/2),
    petEnergy(100),
    petHappiness(100),
    petAge(0),
    lastUpdate(0),
    currentState(PET_IDLE),
    animationFrame(0),
    feedButtonPressed(false),
    playButtonPressed(false)
{
    // Set metadata
    metadata.name = "DigitalPet";
    metadata.version = "1.0";
    metadata.author = "remu.ii";
    metadata.description = "Virtual pet companion";
    metadata.category = CATEGORY_GAMES;
    metadata.memoryRequirement = 8192; // 8KB
    metadata.requiresSD = false;
    metadata.requiresWiFi = false;
    metadata.requiresBLE = false;
}

DigitalPetApp::~DigitalPetApp() {
    cleanup();
}

bool DigitalPetApp::initialize() {
    Serial.println("[DigitalPet] Initializing...");
    
    setState(APP_INITIALIZING);
    
    // Initialize pet position to center of screen
    petX = SCREEN_WIDTH / 2;
    petY = SCREEN_HEIGHT / 2;
    
    // Reset pet stats
    petEnergy = 100;
    petHappiness = 100;
    petAge = 0;
    lastUpdate = millis();
    currentState = PET_IDLE;
    animationFrame = 0;
    
    setState(APP_RUNNING);
    Serial.println("[DigitalPet] Initialized successfully");
    
    return true;
}

void DigitalPetApp::update() {
    if (getState() != APP_RUNNING) return;
    
    unsigned long currentTime = millis();
    
    // Update pet stats every 5 seconds
    if (currentTime - lastUpdate > 5000) {
        updatePetStats();
        lastUpdate = currentTime;
    }
    
    // Update animation
    if (currentTime % 500 < 250) { // Change frame every 500ms
        animationFrame = (animationFrame + 1) % 4;
    }
    
    // Simple AI behavior
    updatePetBehavior();
    
    // Update frame counter
    frameCount++;
}

void DigitalPetApp::render() {
    if (getState() != APP_RUNNING) return;
    
    // Clear screen
    displayManager.clearScreen(COLOR_BLACK);
    
    // Draw title bar
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawTextCentered(0, 5, SCREEN_WIDTH, "Digital Pet", COLOR_GREEN_PHOS);
    
    // Draw pet stats
    displayManager.setFont(FONT_SMALL);
    String energyText = "Energy: " + String(petEnergy) + "%";
    String happyText = "Happy: " + String(petHappiness) + "%";
    String ageText = "Age: " + String(petAge) + " days";
    
    displayManager.drawText(5, 25, energyText, petEnergy > 20 ? COLOR_WHITE : COLOR_RED_GLOW);
    displayManager.drawText(5, 40, happyText, petHappiness > 20 ? COLOR_WHITE : COLOR_RED_GLOW);
    displayManager.drawText(5, 55, ageText, COLOR_WHITE);
    
    // Draw pet
    drawPet();
    
    // Draw action buttons
    drawActionButtons();
    
    // Draw pet state
    drawPetState();
}

bool DigitalPetApp::handleTouch(TouchPoint touch) {
    if (getState() != APP_RUNNING) return false;
    
    if (!touch.isNewPress) return false;
    
    // Check back button (top-left corner)
    if (touch.x < 50 && touch.y < 30) {
        exitApp();
        return true;
    }
    
    // Check feed button (bottom-left)
    if (touch.x < 80 && touch.y > SCREEN_HEIGHT - 40) {
        feedPet();
        feedButtonPressed = true;
        return true;
    }
    
    // Check play button (bottom-right)
    if (touch.x > SCREEN_WIDTH - 80 && touch.y > SCREEN_HEIGHT - 40) {
        playWithPet();
        playButtonPressed = true;
        return true;
    }
    
    // Check pet interaction (touch pet directly)
    if (abs(touch.x - petX) < 30 && abs(touch.y - petY) < 30) {
        petPet();
        return true;
    }
    
    return false;
}

void DigitalPetApp::cleanup() {
    Serial.println("[DigitalPet] Cleaning up...");
    setState(APP_CLEANUP);
}

String DigitalPetApp::getName() const {
    return "DigitalPet";
}

const uint8_t* DigitalPetApp::getIcon() const {
    // Simple 16x16 pet icon
    static const uint8_t petIcon[32] = {
        0x00, 0x00, 0x07, 0xE0, 0x18, 0x18, 0x20, 0x04,
        0x47, 0xE2, 0x4C, 0x32, 0x4C, 0x32, 0x47, 0xE2,
        0x40, 0x02, 0x20, 0x04, 0x18, 0x18, 0x07, 0xE0,
        0x01, 0x80, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00
    };
    return petIcon;
}

void DigitalPetApp::updatePetStats() {
    // Decrease stats over time
    if (petEnergy > 0) petEnergy--;
    if (petHappiness > 0) petHappiness--;
    
    // Age the pet
    petAge++;
    
    // Check for critical states
    if (petEnergy <= 0) {
        currentState = PET_SLEEPING;
    } else if (petHappiness <= 20) {
        currentState = PET_SAD;
    } else if (petEnergy <= 20) {
        currentState = PET_HUNGRY;
    } else {
        currentState = PET_IDLE;
    }
}

void DigitalPetApp::updatePetBehavior() {
    static unsigned long lastMove = 0;
    unsigned long currentTime = millis();
    
    // Random movement every 3 seconds
    if (currentTime - lastMove > 3000 && currentState == PET_IDLE) {
        int moveX = (systemCore.getRandomByte() % 20) - 10;
        int moveY = (systemCore.getRandomByte() % 20) - 10;
        
        petX = constrain(petX + moveX, 30, SCREEN_WIDTH - 30);
        petY = constrain(petY + moveY, 80, SCREEN_HEIGHT - 60);
        
        lastMove = currentTime;
    }
}

void DigitalPetApp::drawPet() {
    uint16_t petColor = COLOR_WHITE;
    
    // Change color based on state
    switch (currentState) {
        case PET_HAPPY:
            petColor = COLOR_GREEN_PHOS;
            break;
        case PET_SAD:
            petColor = COLOR_BLUE_CYBER;
            break;
        case PET_HUNGRY:
            petColor = COLOR_YELLOW;
            break;
        case PET_SLEEPING:
            petColor = COLOR_DARK_GRAY;
            break;
        default:
            petColor = COLOR_WHITE;
            break;
    }
    
    // Draw simple pet (circle with eyes)
    displayManager.drawRetroCircle(petX, petY, 20, petColor, true);
    
    // Draw eyes (unless sleeping)
    if (currentState != PET_SLEEPING) {
        displayManager.drawPixel(petX - 8, petY - 5, COLOR_BLACK);
        displayManager.drawPixel(petX + 8, petY - 5, COLOR_BLACK);
    }
    
    // Draw mouth based on happiness
    if (petHappiness > 50) {
        // Happy mouth (smile)
        displayManager.drawLine(petX - 6, petY + 5, petX + 6, petY + 5, COLOR_BLACK);
        displayManager.drawPixel(petX - 4, petY + 7, COLOR_BLACK);
        displayManager.drawPixel(petX + 4, petY + 7, COLOR_BLACK);
    } else if (petHappiness < 20) {
        // Sad mouth (frown)
        displayManager.drawLine(petX - 6, petY + 8, petX + 6, petY + 8, COLOR_BLACK);
        displayManager.drawPixel(petX - 4, petY + 6, COLOR_BLACK);
        displayManager.drawPixel(petX + 4, petY + 6, COLOR_BLACK);
    } else {
        // Neutral mouth
        displayManager.drawLine(petX - 4, petY + 6, petX + 4, petY + 6, COLOR_BLACK);
    }
    
    // Draw animation effect (sparkles when happy)
    if (currentState == PET_HAPPY && animationFrame % 2 == 0) {
        displayManager.drawPixel(petX - 25, petY - 15, COLOR_YELLOW);
        displayManager.drawPixel(petX + 25, petY - 15, COLOR_YELLOW);
        displayManager.drawPixel(petX, petY - 30, COLOR_YELLOW);
    }
}

void DigitalPetApp::drawActionButtons() {
    // Feed button
    uint16_t feedColor = feedButtonPressed ? COLOR_GREEN_PHOS : COLOR_MID_GRAY;
    displayManager.drawButton(5, SCREEN_HEIGHT - 35, 70, 30, "Feed", 
                             feedButtonPressed ? BUTTON_PRESSED : BUTTON_NORMAL, feedColor);
    
    // Play button
    uint16_t playColor = playButtonPressed ? COLOR_PURPLE_GLOW : COLOR_MID_GRAY;
    displayManager.drawButton(SCREEN_WIDTH - 75, SCREEN_HEIGHT - 35, 70, 30, "Play",
                             playButtonPressed ? BUTTON_PRESSED : BUTTON_NORMAL, playColor);
    
    // Reset button states
    feedButtonPressed = false;
    playButtonPressed = false;
}

void DigitalPetApp::drawPetState() {
    displayManager.setFont(FONT_SMALL);
    String stateText = "";
    uint16_t stateColor = COLOR_WHITE;
    
    switch (currentState) {
        case PET_IDLE:
            stateText = "Idle";
            stateColor = COLOR_WHITE;
            break;
        case PET_HAPPY:
            stateText = "Happy!";
            stateColor = COLOR_GREEN_PHOS;
            break;
        case PET_SAD:
            stateText = "Sad...";
            stateColor = COLOR_BLUE_CYBER;
            break;
        case PET_HUNGRY:
            stateText = "Hungry!";
            stateColor = COLOR_YELLOW;
            break;
        case PET_SLEEPING:
            stateText = "Sleeping Zzz";
            stateColor = COLOR_DARK_GRAY;
            break;
    }
    
    displayManager.drawTextCentered(0, SCREEN_HEIGHT - 55, SCREEN_WIDTH, stateText, stateColor);
}

void DigitalPetApp::feedPet() {
    if (petEnergy < 100) {
        petEnergy = min(100, petEnergy + 20);
        petHappiness = min(100, petHappiness + 10);
        currentState = PET_HAPPY;
        
        Serial.println("[DigitalPet] Pet fed - energy restored!");
    }
}

void DigitalPetApp::playWithPet() {
    if (petEnergy >= 10) {
        petEnergy -= 10;
        petHappiness = min(100, petHappiness + 25);
        currentState = PET_HAPPY;
        
        Serial.println("[DigitalPet] Playing with pet - happiness increased!");
    }
}

void DigitalPetApp::petPet() {
    petHappiness = min(100, petHappiness + 5);
    if (petHappiness > 70) {
        currentState = PET_HAPPY;
    }
    
    Serial.println("[DigitalPet] Pet petted - happiness slightly increased!");
}