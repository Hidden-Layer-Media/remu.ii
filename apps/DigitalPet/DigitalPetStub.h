#ifndef DIGITAL_PET_STUB_H
#define DIGITAL_PET_STUB_H

#include "../../core/AppManager/BaseApp.h"
#include "../../core/FileSystem.h"
#include <ArduinoJson.h>

struct SimplePetState {
    int mood = 50;        // 0-100
    int hunger = 30;      // 0-100
    int happiness = 70;   // 0-100
    unsigned long lastFed = 0;
    unsigned long lastPet = 0;
    String name = "Pet";
};

class DigitalPetApp : public BaseApp {
private:
    SimplePetState pet;
    String saveFile = "/data/pet_save.json";
    unsigned long lastUpdate = 0;
    int selectedAction = 0;
    
    void loadPetData() {
        if (filesystem.fileExists(saveFile)) {
            String data = filesystem.readFile(saveFile);
            if (data.length() > 0) {
                DynamicJsonDocument doc(512);
                if (deserializeJson(doc, data) == DeserializationError::Ok) {
                    pet.mood = doc["mood"] | 50;
                    pet.hunger = doc["hunger"] | 30;
                    pet.happiness = doc["happiness"] | 70;
                    pet.lastFed = doc["lastFed"] | 0;
                    pet.lastPet = doc["lastPet"] | 0;
                    pet.name = doc["name"] | "Pet";
                }
            }
        }
    }
    
    void savePetData() {
        DynamicJsonDocument doc(512);
        doc["mood"] = pet.mood;
        doc["hunger"] = pet.hunger;
        doc["happiness"] = pet.happiness;
        doc["lastFed"] = pet.lastFed;
        doc["lastPet"] = pet.lastPet;
        doc["name"] = pet.name;
        
        String output;
        serializeJson(doc, output);
        filesystem.ensureDirExists("/data");
        filesystem.writeFile(saveFile, output);
    }
    
    void updatePet() {
        unsigned long now = millis();
        if (now - lastUpdate > 5000) { // Update every 5 seconds
            // Hunger increases over time
            if (now - pet.lastFed > 30000) { // 30 seconds
                pet.hunger = min(100, pet.hunger + 1);
            }
            
            // Mood affected by hunger and happiness
            if (pet.hunger > 80) {
                pet.mood = max(0, pet.mood - 2);
            } else if (pet.hunger < 20) {
                pet.mood = min(100, pet.mood + 1);
            }
            
            lastUpdate = now;
            savePetData();
        }
    }
    
public:
    DigitalPetApp() {
        setMetadata("DigitalPet", "1.0", "remu.ii", "Digital pet with SD storage", CATEGORY_GAMES, 8192);
    }
    
    bool initialize() override {
        Serial.println("[DigitalPet] Initializing with SD storage...");
        loadPetData();
        return true;
    }
    
    void update() override {
        updatePet();
    }
    
    void render() override {
        displayManager.clearScreen(COLOR_BLACK);
        
        // Title
        displayManager.setFont(FONT_LARGE);
        displayManager.drawTextCentered(0, 20, SCREEN_WIDTH, pet.name, COLOR_GREEN_PHOS);
        
        // Pet sprite (simple)
        int16_t petX = SCREEN_WIDTH/2 - 16;
        int16_t petY = 60;
        displayManager.drawRetroRect(petX, petY, 32, 32, COLOR_WHITE, false);
        
        // Eyes based on mood
        uint16_t eyeColor = (pet.mood > 50) ? COLOR_GREEN_PHOS : COLOR_RED_GLOW;
        displayManager.drawPixel(petX + 8, petY + 10, eyeColor);
        displayManager.drawPixel(petX + 24, petY + 10, eyeColor);
        
        // Mouth based on happiness
        if (pet.happiness > 60) {
            displayManager.drawLine(petX + 12, petY + 20, petX + 20, petY + 20, COLOR_WHITE);
        }
        
        // Stats
        displayManager.setFont(FONT_SMALL);
        displayManager.drawText(20, 120, "Mood: " + String(pet.mood), COLOR_WHITE);
        displayManager.drawText(20, 140, "Hunger: " + String(pet.hunger), COLOR_WHITE);
        displayManager.drawText(20, 160, "Happy: " + String(pet.happiness), COLOR_WHITE);
        
        // Action buttons - evenly spaced across 320px
        const char* actions[] = {"Feed", "Pet", "Play", "Exit"};
        for (int i = 0; i < 4; i++) {
            uint16_t color = (i == selectedAction) ? COLOR_RED_GLOW : COLOR_WHITE;
            displayManager.drawText(20 + i * 72, 200, actions[i], color);
        }
    }
    
    bool handleTouch(TouchPoint touch) override {
        if (!touch.isNewPress) return true;
        
        if (touch.y > 190) {
            int action = touch.x / 80;
            if (action > 3) action = 3;
            selectedAction = action;
            unsigned long now = millis();
            switch (action) {
                case 0: // Feed
                    pet.hunger = max(0, pet.hunger - 20);
                    pet.lastFed = now;
                    pet.happiness = min(100, pet.happiness + 5);
                    break;
                case 1: // Pet
                    pet.happiness = min(100, pet.happiness + 10);
                    pet.mood = min(100, pet.mood + 5);
                    pet.lastPet = now;
                    break;
                case 2: // Play
                    pet.happiness = min(100, pet.happiness + 15);
                    pet.hunger = min(100, pet.hunger + 5);
                    break;
                case 3: // Exit
                    savePetData();
                    return false;
            }
            savePetData();
        }
        return true;
    }
    
    String getName() const override { return "DigitalPet"; }
    
    void cleanup() override {
        savePetData();
    }
    
    void setAppManager(void* manager) override {
        // Store reference if needed
    }
};

#endif