#include "AppManager.h"
#include "../../apps/CarCloner/CarClonerStub.h"
#include "../../apps/BLEScanner/BLEScannerStub.h"
#include "../../apps/PreqScanner/FreqScannerStub.h"
#include "../../apps/EntropyBeacon/EntropyBeaconStub.h"
#include "../../apps/Sequencer/SequencerStub.h"
#include "../../apps/WifiTools/WiFiToolsStub.h"
#include "../../apps/DigitalPet/DigitalPetStub.h"

// Built-in icon data (16x16 1-bit bitmaps)
const uint8_t AppManager::ICON_SYSTEM[32] = {
    0x00, 0x00, 0x7F, 0xFE, 0x40, 0x02, 0x5F, 0xFA, 0x50, 0x0A, 0x5F, 0xFA,
    0x50, 0x0A, 0x5F, 0xFA, 0x50, 0x0A, 0x5F, 0xFA, 0x50, 0x0A, 0x5F, 0xFA,
    0x40, 0x02, 0x7F, 0xFE, 0x00, 0x00, 0x00, 0x00
};

const uint8_t AppManager::ICON_TOOLS[32] = {
    0x00, 0x00, 0x01, 0x80, 0x03, 0xC0, 0x07, 0xE0, 0x0F, 0xF0, 0x1F, 0xF8,
    0x3F, 0xFC, 0x7F, 0xFE, 0xFF, 0xFF, 0x7F, 0xFE, 0x3F, 0xFC, 0x1F, 0xF8,
    0x0F, 0xF0, 0x07, 0xE0, 0x03, 0xC0, 0x01, 0x80
};

const uint8_t AppManager::ICON_GAMES[32] = {
    0x00, 0x00, 0x3F, 0xFC, 0x60, 0x06, 0xD8, 0x1B, 0xD8, 0x1B, 0xD8, 0x1B,
    0x60, 0x06, 0x7F, 0xFE, 0x7F, 0xFE, 0x60, 0x06, 0x6C, 0x36, 0x6C, 0x36,
    0x6C, 0x36, 0x60, 0x06, 0x3F, 0xFC, 0x00, 0x00
};

const uint8_t AppManager::ICON_DIGITALPET[32] = {
    0x00, 0x00, 0x07, 0xE0, 0x18, 0x18, 0x20, 0x04, 0x47, 0xE2, 0x4C, 0x32,
    0x4C, 0x32, 0x47, 0xE2, 0x40, 0x02, 0x20, 0x04, 0x18, 0x18, 0x07, 0xE0,
    0x01, 0x80, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00
};

const uint8_t AppManager::ICON_MEDIA[32] = {
    0x00, 0x00, 0x00, 0x00, 0x3F, 0xFF, 0x20, 0x01, 0x27, 0x81, 0x24, 0x41,
    0x24, 0x21, 0x24, 0x11, 0x24, 0x09, 0x24, 0x05, 0x24, 0x03, 0x27, 0xFF,
    0x20, 0x01, 0x3F, 0xFF, 0x00, 0x00, 0x00, 0x00
};

const uint8_t AppManager::ICON_COMM[32] = {
    0x00, 0x00, 0x08, 0x00, 0x1C, 0x00, 0x3E, 0x00, 0x7F, 0x00, 0xFF, 0x80,
    0x7F, 0x00, 0x3E, 0x00, 0x1C, 0x00, 0x08, 0x00, 0x08, 0x10, 0x1C, 0x38,
    0x3E, 0x7C, 0x7F, 0xFE, 0xFF, 0xFF, 0x00, 0x00
};

const uint8_t AppManager::ICON_OTHER[32] = {
    0x00, 0x00, 0x18, 0x18, 0x24, 0x24, 0x42, 0x42, 0x81, 0x81, 0x81, 0x81,
    0x42, 0x42, 0x24, 0x24, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x7E, 0x7E, 0x7E,
    0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const uint8_t AppManager::ICON_SEQUENCER[32] = {
    0x00, 0x00, 0xFF, 0xFF, 0x80, 0x01, 0x9F, 0xF9, 0x90, 0x09, 0x9F, 0xF9,
    0x90, 0x09, 0x9F, 0xF9, 0x90, 0x09, 0x9F, 0xF9, 0x90, 0x09, 0x9F, 0xF9,
    0x80, 0x01, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00
};

const uint8_t AppManager::ICON_WIFI[32] = {
    0x00, 0x00, 0x00, 0x00, 0x03, 0xC0, 0x0F, 0xF0, 0x1E, 0x78, 0x38, 0x1C,
    0x70, 0x0E, 0x67, 0xE6, 0x4C, 0x32, 0x58, 0x1A, 0x33, 0xCC, 0x07, 0xE0,
    0x0C, 0x30, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00
};

const uint8_t AppManager::ICON_BLE[32] = {
    0x00, 0x00, 0x01, 0x00, 0x03, 0x80, 0x05, 0x40, 0x09, 0x20, 0x11, 0x10,
    0x21, 0x08, 0x43, 0x04, 0x87, 0x02, 0x8F, 0x02, 0x43, 0x84, 0x21, 0x48,
    0x11, 0x50, 0x09, 0x60, 0x05, 0x40, 0x03, 0x80
};

const uint8_t AppManager::ICON_CAR[32] = {
    0x00, 0x00, 0x00, 0x00, 0x0F, 0xF0, 0x18, 0x18, 0x30, 0x0C, 0x67, 0xE6,
    0x4C, 0x32, 0x4C, 0x32, 0x4C, 0x32, 0x67, 0xE6, 0x30, 0x0C, 0x18, 0x18,
    0x0F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const uint8_t AppManager::ICON_FREQ[32] = {
    0x00, 0x00, 0x08, 0x10, 0x0C, 0x30, 0x0E, 0x70, 0x0F, 0xF0, 0x8F, 0xF1,
    0xCF, 0xF3, 0xEF, 0xF7, 0xFF, 0xFF, 0xEF, 0xF7, 0xCF, 0xF3, 0x8F, 0xF1,
    0x0F, 0xF0, 0x0E, 0x70, 0x0C, 0x30, 0x08, 0x10
};

const uint8_t AppManager::ICON_ENTROPY[32] = {
    0x00, 0x00, 0x81, 0x81, 0x42, 0x42, 0x24, 0x24, 0x18, 0x18, 0x99, 0x99,
    0x42, 0x42, 0xA5, 0xA5, 0x18, 0x18, 0x66, 0x66, 0x99, 0x99, 0x24, 0x24,
    0x42, 0x42, 0x81, 0x81, 0x00, 0x00, 0x00, 0x00
};

AppManager::AppManager() :
    registeredAppCount(0),
    currentApp(nullptr),
    currentAppIndex(-1),
    launcherState(LAUNCHER_MAIN),
    selectedAppIndex(0),
    launcherPage(0),
    totalPages(1),
    showLauncher(true),
    lastUpdateTime(0),
    currentTransition(TRANSITION_NONE),
    transitionProgress(0),
    availableMemory(0),
    memoryLimit(50000) // 50KB default limit
{
    // Initialize app registry
    for (uint8_t i = 0; i < MAX_APPS; i++) {
        appRegistry[i] = {"", "", "", {}, nullptr, false, true, 0};
    }
}

AppManager::~AppManager() {
    shutdown();
}

bool AppManager::initialize() {
    Serial.println("[AppManager] Initializing...");
    
    // SD card is already initialized by FileSystem - do not call SD.begin() again
    if (!filesystem.isReady()) {
        Serial.println("[AppManager] WARNING: SD card not ready - built-in apps only");
    } else {
        Serial.println("[AppManager] SD card available");
    }
    
    // Register built-in apps
    registerBuiltinApps();
    
    // Calculate total pages for launcher
    totalPages = (registeredAppCount + (LAUNCHER_GRID_COLS * LAUNCHER_GRID_ROWS) - 1) / 
                 (LAUNCHER_GRID_COLS * LAUNCHER_GRID_ROWS);
    if (totalPages == 0) totalPages = 1;
    
    // Show launcher initially
    showLauncher = true;
    launcherState = LAUNCHER_MAIN;
    
    Serial.printf("[AppManager] Initialized with %d apps, %d pages\n", registeredAppCount, totalPages);
    
    return true;
}

void AppManager::update() {
    unsigned long currentTime = millis();
    
    // Update current app if running
    if (currentApp && currentApp->isRunning()) {
        currentApp->update();
        
        // Check for memory issues
        if (currentTime - lastUpdateTime > 1000) { // Check every second
            checkMemoryUsage();
            lastUpdateTime = currentTime;
        }
    }
    
    // Update transitions
    if (currentTransition != TRANSITION_NONE) {
        updateTransition();
    }
    
    // Handle pending app launches
    if (!pendingAppName.isEmpty() && currentTransition == TRANSITION_NONE) {
        int8_t appIndex = findAppByName(pendingAppName);
        if (appIndex >= 0) {
            switchToApp(appIndex);
        }
        pendingAppName = "";
    }
}

void AppManager::render() {
    if (showLauncher) {
        drawLauncher();
    } else if (currentApp && currentApp->isRunning()) {
        currentApp->render();
    }
}

void AppManager::shutdown() {
    // Cleanup current app
    if (currentApp) {
        currentApp->cleanup();
        delete currentApp;
        currentApp = nullptr;
    }
    
    // Unload all apps
    for (uint8_t i = 0; i < registeredAppCount; i++) {
        if (appRegistry[i].isLoaded && appRegistry[i].instance) {
            appRegistry[i].instance->cleanup();
            delete appRegistry[i].instance;
            appRegistry[i].instance = nullptr;
            appRegistry[i].isLoaded = false;
        }
    }
    
    Serial.println("[AppManager] Shutdown complete");
}

void AppManager::registerBuiltinApps() {
    Serial.println("[AppManager] Registering built-in apps...");
    
    // DigitalPet
    registerApp("DigitalPet", "DigitalPetApp", "/apps/DigitalPet/");
    
    // Sequencer
    registerApp("Sequencer", "SequencerApp", "/apps/Sequencer/");
    
    // WiFiTools
    registerApp("WiFiTools", "WiFiToolsApp", "/apps/WiFiTools/");
    
    // BLEScanner
    registerApp("BLEScanner", "BLEScannerApp", "/apps/BLEScanner/");
    
    // CarCloner
    registerApp("CarCloner", "CarClonerApp", "/apps/CarCloner/");
    
    // FreqScanner
    registerApp("FreqScanner", "FreqScannerApp", "/apps/FreqScanner/");
    
    // EntropyBeacon
    registerApp("EntropyBeacon", "EntropyBeaconApp", "/apps/EntropyBeacon/");
    
    Serial.printf("[AppManager] Registered %d built-in apps\n", registeredAppCount);
}

bool AppManager::registerApp(String name, String className, String filePath) {
    if (registeredAppCount >= MAX_APPS) {
        Serial.printf("[AppManager] ERROR: Maximum apps (%d) reached\n", MAX_APPS);
        return false;
    }
    
    uint8_t index = registeredAppCount++;
    appRegistry[index].name = name;
    appRegistry[index].className = className;
    appRegistry[index].filePath = filePath;
    appRegistry[index].isEnabled = true;
    appRegistry[index].isLoaded = false;
    appRegistry[index].instance = nullptr;
    appRegistry[index].memoryUsage = 0;
    
    // Set default metadata - will be updated when app is loaded
    appRegistry[index].metadata.name = name;
    appRegistry[index].metadata.version = "1.0";
    appRegistry[index].metadata.author = "remu.ii";
    appRegistry[index].metadata.description = "Built-in application";
    appRegistry[index].metadata.category = CATEGORY_OTHER;
    appRegistry[index].metadata.icon = getDefaultIcon(CATEGORY_OTHER);
    appRegistry[index].metadata.memoryRequirement = 10000; // 10KB default
    
    Serial.printf("[AppManager] Registered: %s\n", name.c_str());
    return true;
}

bool AppManager::launchApp(String appName) {
    int8_t appIndex = findAppByName(appName);
    if (appIndex < 0) {
        Serial.printf("[AppManager] ERROR: App '%s' not found\n", appName.c_str());
        return false;
    }
    
    return launchApp(appIndex);
}

bool AppManager::launchApp(uint8_t appIndex) {
    if (appIndex >= registeredAppCount) {
        Serial.printf("[AppManager] ERROR: Invalid app index %d\n", appIndex);
        return false;
    }
    
    if (!appRegistry[appIndex].isEnabled) {
        Serial.printf("[AppManager] ERROR: App '%s' is disabled\n", 
                     appRegistry[appIndex].name.c_str());
        return false;
    }
    
    // Check memory requirements
    if (!hasEnoughMemoryForApp(appIndex)) {
        Serial.printf("[AppManager] ERROR: Not enough memory for '%s'\n", 
                     appRegistry[appIndex].name.c_str());
        return false;
    }
    
    // Start transition to loading screen
    startTransition(TRANSITION_FADE);
    pendingAppName = appRegistry[appIndex].name;
    
    return true;
}

bool AppManager::switchToApp(uint8_t appIndex) {
    Serial.printf("[AppManager] Switching to app: %s\n", appRegistry[appIndex].name.c_str());
    
    // Exit current app
    if (currentApp) {
        currentApp->onPause();
        currentApp->cleanup();
        // Don't delete here - let AppManager handle cleanup
        currentApp = nullptr;
    }
    
    // Load new app if not already loaded
    if (!appRegistry[appIndex].isLoaded) {
        if (!loadApp(appIndex)) {
            Serial.printf("[AppManager] ERROR: Failed to load app '%s'\n", 
                         appRegistry[appIndex].name.c_str());
            returnToLauncher();
            return false;
        }
    }
    
    // Set as current app
    currentApp = appRegistry[appIndex].instance;
    currentAppIndex = appIndex;
    
    // Initialize app
    if (!currentApp->initialize()) {
        Serial.printf("[AppManager] ERROR: Failed to initialize app '%s'\n", 
                     appRegistry[appIndex].name.c_str());
        currentApp->cleanup();
        currentApp = nullptr;
        appRegistry[appIndex].instance = nullptr;
        appRegistry[appIndex].isLoaded = false;
        returnToLauncher();
        return false;
    }
    
    // Hide launcher
    showLauncher = false;
    
    Serial.printf("[AppManager] Successfully launched: %s\n", appRegistry[appIndex].name.c_str());
    return true;
}

bool AppManager::loadApp(uint8_t appIndex) {
    Serial.printf("[AppManager] Loading app: %s\n", appRegistry[appIndex].name.c_str());
    
    // For built-in apps, create instances directly
    // In a real implementation, this would load from SD card or flash
    BaseApp* appInstance = nullptr;
    
    String appName = appRegistry[appIndex].name;
    
    // Create app instances - all apps are now fully implemented
    if (appName == "DigitalPet") {
        appInstance = new DigitalPetApp();
        Serial.println("[AppManager] DigitalPet app instantiated");
    } else if (appName == "Sequencer") {
        appInstance = new SequencerApp();
        Serial.println("[AppManager] Sequencer app instantiated");
    } else if (appName == "WiFiTools") {
        appInstance = new WiFiToolsApp();
        Serial.println("[AppManager] WiFiTools app instantiated");
    } else if (appName == "BLEScanner") {
        appInstance = new BLEScannerApp();
        Serial.println("[AppManager] BLEScanner app instantiated");
    } else if (appName == "CarCloner") {
        appInstance = new CarClonerApp();
        Serial.println("[AppManager] CarCloner app instantiated");
    } else if (appName == "FreqScanner") {
        appInstance = new FreqScannerApp();
        Serial.println("[AppManager] FreqScanner app instantiated");
    } else if (appName == "EntropyBeacon") {
        appInstance = new EntropyBeaconApp();
        Serial.println("[AppManager] EntropyBeacon app instantiated");
    }
    
    if (!appInstance) {
        Serial.printf("[AppManager] ERROR: Failed to create instance for '%s'\n", appName.c_str());
        return false;
    }
    
    // Set up app
    appInstance->setAppManager(this);
    appRegistry[appIndex].instance = appInstance;
    appRegistry[appIndex].isLoaded = true;
    
    // Update memory usage
    size_t currentHeap = ESP.getFreeHeap();
    appRegistry[appIndex].memoryUsage = (availableMemory > currentHeap) ? (availableMemory - currentHeap) : 0;
    
    return true;
}

void AppManager::exitCurrentApp() {
    if (currentApp) {
        Serial.printf("[AppManager] Exiting app: %s\n", getCurrentAppName().c_str());
        currentApp->onPause();
        // Don't unload immediately - keep in memory for faster restart
    }
    
    returnToLauncher();
}

void AppManager::returnToLauncher() {
    showLauncher = true;
    launcherState = LAUNCHER_MAIN;
    currentApp = nullptr;
    currentAppIndex = -1;
    
    // Clear screen with launcher background
    displayManager.clearScreen(COLOR_BLACK);
}

void AppManager::drawLauncher() {
    switch (launcherState) {
        case LAUNCHER_MAIN:
            drawAppGrid();
            break;
        case LAUNCHER_MENU:
            drawSystemMenu();
            break;
        case LAUNCHER_SETTINGS:
            drawSettingsScreen();
            break;
        case LAUNCHER_INFO:
            drawInfoScreen();
            break;
        case LAUNCHER_LOADING:
            drawLoadingScreen();
            break;
    }
    
    // Always draw status bar
    drawStatusBar();
}

void AppManager::drawAppGrid() {
    displayManager.clearScreen(COLOR_BLACK);
    
    // Draw title
    displayManager.setFont(FONT_LARGE);
    displayManager.drawTextCentered(0, 10, SCREEN_WIDTH, "remu.ii", COLOR_RED_GLOW);
    
    // Draw ASCII border
    displayManager.drawASCIIBorder(10, 40, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 80, COLOR_GREEN_PHOS);
    
    // Calculate grid layout
    uint8_t startIndex = launcherPage * (LAUNCHER_GRID_COLS * LAUNCHER_GRID_ROWS);
    uint8_t endIndex = min(startIndex + (LAUNCHER_GRID_COLS * LAUNCHER_GRID_ROWS), registeredAppCount);
    
    int16_t gridX = 20;
    int16_t gridY = 60;
    int16_t iconSpacing = (SCREEN_WIDTH - 40) / LAUNCHER_GRID_COLS;
    int16_t iconSize = min(iconSpacing - 10, LAUNCHER_ICON_SIZE);
    
    // Draw app icons
    for (uint8_t i = startIndex; i < endIndex; i++) {
        if (!appRegistry[i].isEnabled) continue;
        
        uint8_t gridPos = i - startIndex;
        uint8_t col = gridPos % LAUNCHER_GRID_COLS;
        uint8_t row = gridPos / LAUNCHER_GRID_COLS;
        
        int16_t x = gridX + col * iconSpacing;
        int16_t y = gridY + row * (iconSize + 20);
        
        bool selected = (i == selectedAppIndex);
        drawAppIcon(i, x, y, selected);
    }
    
    // Draw page indicator
    if (totalPages > 1) {
        displayManager.setFont(FONT_SMALL);
        String pageInfo = String(launcherPage + 1) + "/" + String(totalPages);
        displayManager.drawTextCentered(0, SCREEN_HEIGHT - 25, SCREEN_WIDTH, pageInfo, COLOR_LIGHT_GRAY);
    }
}

void AppManager::drawAppIcon(uint8_t appIndex, int16_t x, int16_t y, bool selected) {
    if (appIndex >= registeredAppCount) return;
    
    AppRegistryEntry& app = appRegistry[appIndex];
    uint16_t iconColor = selected ? COLOR_RED_GLOW : COLOR_WHITE;
    uint16_t textColor = selected ? COLOR_RED_GLOW : COLOR_GREEN_PHOS;
    
    // Draw selection highlight
    if (selected) {
        displayManager.drawRetroRect(x - 2, y - 2, 36, 36, COLOR_RED_GLOW, false);
        displayManager.drawGlowEffect(x - 4, y - 4, 40, 40, COLOR_RED_GLOW);
    }
    
    // Draw icon background
    displayManager.drawRetroRect(x, y, 32, 32, COLOR_DARK_GRAY, true);
    
    // Draw icon (16x16 centered in 32x32 area)
    if (app.metadata.icon) {
        displayManager.drawIcon(x + 8, y + 8, app.metadata.icon, iconColor);
    } else {
        // Draw default icon based on category
        const uint8_t* defaultIcon = getDefaultIcon(app.metadata.category);
        displayManager.drawIcon(x + 8, y + 8, defaultIcon, iconColor);
    }
    
    // Draw app name
    displayManager.setFont(FONT_SMALL);
    String displayName = app.name;
    if (displayName.length() > 8) {
        displayName = displayName.substring(0, 7) + "..";
    }
    displayManager.drawTextCentered(x - 10, y + 35, 52, displayName, textColor);
    
    // Draw status indicators
    if (app.isLoaded) {
        displayManager.drawPixel(x + 28, y + 4, COLOR_GREEN_PHOS); // Loaded indicator
    }
    if (!app.isEnabled) {
        displayManager.drawLine(x, y, x + 31, y + 31, COLOR_RED_GLOW); // Disabled X
        displayManager.drawLine(x, y + 31, x + 31, y, COLOR_RED_GLOW);
    }
}

void AppManager::drawSystemMenu() {
    displayManager.clearScreen(COLOR_BLACK);
    
    // Draw menu title
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawTextCentered(0, 10, SCREEN_WIDTH, "SYSTEM MENU", COLOR_PURPLE_GLOW);
    
    // Draw menu options
    String menuItems[] = {
        "Settings",
        "System Info", 
        "Memory Status",
        "Calibrate Touch",
        "Power Off",
        "Back to Apps"
    };
    
    int16_t menuY = 50;
    for (uint8_t i = 0; i < 6; i++) {
        uint16_t color = (i == selectedAppIndex) ? COLOR_RED_GLOW : COLOR_WHITE;
        displayManager.setFont(FONT_MEDIUM);
        displayManager.drawText(30, menuY + i * 25, menuItems[i], color);
        
        if (i == selectedAppIndex) {
            displayManager.drawText(10, menuY + i * 25, ">", COLOR_RED_GLOW);
        }
    }
}

void AppManager::drawStatusBar() {
    // Draw status bar at top
    displayManager.drawRetroRect(0, 0, SCREEN_WIDTH, 20, COLOR_DARK_GRAY, true);
    
    // Battery indicator
    uint8_t batteryLevel = systemCore.getBatteryPercentage();
    displayManager.setFont(FONT_SMALL);
    String batteryText = String(batteryLevel) + "%";
    displayManager.drawText(SCREEN_WIDTH - 30, 5, batteryText, COLOR_GREEN_PHOS);
    
    // Memory indicator
    size_t freeHeap = ESP.getFreeHeap();
    String memText = String(freeHeap / 1024) + "K";
    displayManager.drawText(SCREEN_WIDTH - 80, 5, memText, COLOR_GREEN_PHOS);
    
    // Time indicator (uptime)
    unsigned long uptime = systemCore.getUptimeSeconds();
    String timeText = String(uptime / 60) + ":" + String(uptime % 60);
    displayManager.drawText(10, 5, timeText, COLOR_GREEN_PHOS);
}

bool AppManager::handleTouch(TouchPoint touch) {
    if (showLauncher) {
        handleLauncherTouch(touch);
        return true;
    } else if (currentApp) {
        return currentApp->handleTouch(touch);
    }
    
    return false;
}

void AppManager::handleLauncherTouch(TouchPoint touch) {
    if (!touch.isNewPress) return;
    
    switch (launcherState) {
        case LAUNCHER_MAIN:
            handleAppGridTouch(touch);
            break;
        case LAUNCHER_MENU:
            handleMenuTouch(touch);
            break;
        // Add other launcher states as needed
        default:
            launcherState = LAUNCHER_MAIN;
            break;
    }
}

void AppManager::handleAppGridTouch(TouchPoint touch) {
    // Check for system menu access (top-left corner)
    if (touch.x < 50 && touch.y < 50) {
        launcherState = LAUNCHER_MENU;
        selectedAppIndex = 0;
        return;
    }
    
    // Check for page navigation
    if (totalPages > 1) {
        if (touch.x > SCREEN_WIDTH - 50 && touch.y > SCREEN_HEIGHT - 50) {
            // Next page
            launcherPage = (launcherPage + 1) % totalPages;
            return;
        } else if (touch.x < 50 && touch.y > SCREEN_HEIGHT - 50) {
            // Previous page
            launcherPage = (launcherPage > 0) ? launcherPage - 1 : totalPages - 1;
            return;
        }
    }
    
    // Check for app icon touch
    uint8_t touchedApp = getTouchedAppIndex(touch);
    if (touchedApp < registeredAppCount) {
        selectedAppIndex = touchedApp;
        launchApp(touchedApp);
    }
}

uint8_t AppManager::getTouchedAppIndex(TouchPoint touch) {
    int16_t gridX = 20;
    int16_t gridY = 60;
    int16_t iconSpacing = (SCREEN_WIDTH - 40) / LAUNCHER_GRID_COLS;
    
    if (touch.x < gridX || touch.y < gridY) return 255;
    
    uint8_t col = (touch.x - gridX) / iconSpacing;
    uint8_t row = (touch.y - gridY) / (64 + 20); // Icon size + spacing
    
    if (col >= LAUNCHER_GRID_COLS || row >= LAUNCHER_GRID_ROWS) return 255;
    
    uint8_t index = launcherPage * (LAUNCHER_GRID_COLS * LAUNCHER_GRID_ROWS) + 
                   row * LAUNCHER_GRID_COLS + col;
    
    return (index < registeredAppCount) ? index : 255;
}

const uint8_t* AppManager::getDefaultIcon(AppCategory category) {
    switch (category) {
        case CATEGORY_SYSTEM: return ICON_SYSTEM;
        case CATEGORY_TOOLS: return ICON_TOOLS;
        case CATEGORY_GAMES: return ICON_GAMES;
        case CATEGORY_MEDIA: return ICON_MEDIA;
        case CATEGORY_COMMUNICATION: return ICON_COMM;
        case CATEGORY_OTHER: return ICON_OTHER;
        default: return ICON_OTHER;
    }
}

void AppManager::checkMemoryUsage() {
    availableMemory = ESP.getFreeHeap();
    
    if (availableMemory < 5000) { // Less than 5KB free
        Serial.println("[AppManager] WARNING: Low memory detected");
        handleLowMemory();
    }
}

bool AppManager::hasEnoughMemoryForApp(uint8_t appIndex) {
    if (appIndex >= registeredAppCount) return false;
    
    size_t required = appRegistry[appIndex].metadata.memoryRequirement;
    return (availableMemory >= required + 5000); // 5KB safety margin
}

int8_t AppManager::findAppByName(String name) {
    for (uint8_t i = 0; i < registeredAppCount; i++) {
        if (appRegistry[i].name.equals(name)) {
            return i;
        }
    }
    return -1;
}

String AppManager::getCurrentAppName() const {
    if (currentAppIndex >= 0 && currentAppIndex < registeredAppCount) {
        return appRegistry[currentAppIndex].name;
    }
    return "None";
}

void AppManager::handleLowMemory() {
    // Implementation for low memory handling
    Serial.println("[AppManager] Handling low memory situation");
    
    // Could unload non-critical apps, show warning, etc.
}

void AppManager::printAppRegistry() {
    Serial.println("[AppManager] App Registry:");
    for (uint8_t i = 0; i < registeredAppCount; i++) {
        Serial.printf("  %d: %s (%s) - Loaded: %s, Enabled: %s\n",
                     i, appRegistry[i].name.c_str(), appRegistry[i].className.c_str(),
                     appRegistry[i].isLoaded ? "YES" : "NO",
                     appRegistry[i].isEnabled ? "YES" : "NO");
    }
}

// Placeholder implementations for transition and other methods
void AppManager::startTransition(AppTransition type) {
    currentTransition = type;
    transitionProgress = 0;
}

void AppManager::updateTransition() {
    transitionProgress += 10;
    if (transitionProgress >= 100) {
        finishTransition();
    }
}

void AppManager::finishTransition() {
    currentTransition = TRANSITION_NONE;
    transitionProgress = 0;
}

void AppManager::handleMenuTouch(TouchPoint touch) {
    // Menu touch handling - placeholder
    launcherState = LAUNCHER_MAIN;
}

void AppManager::drawSettingsScreen() {
    displayManager.clearScreen(COLOR_BLACK);
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawTextCentered(0, 100, SCREEN_WIDTH, "Settings Screen", COLOR_WHITE);
}

void AppManager::drawInfoScreen() {
    displayManager.clearScreen(COLOR_BLACK);
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawTextCentered(0, 100, SCREEN_WIDTH, "System Info", COLOR_WHITE);
}

void AppManager::drawLoadingScreen() {
    displayManager.clearScreen(COLOR_BLACK);
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawTextCentered(0, 100, SCREEN_WIDTH, "Loading...", COLOR_GREEN_PHOS);
}