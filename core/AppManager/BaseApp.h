#ifndef BASE_APP_H
#define BASE_APP_H

#include <Arduino.h>
#include "../DisplayManager/DisplayManager.h"
#include "../TouchInterface/TouchInterface.h"
#include "../SystemCore/SystemCore.h"

// ========================================
// BaseApp - Abstract base class for all remu.ii applications
// Provides common interface and lifecycle management
// ========================================

// App categories for organization
enum AppCategory {
    CATEGORY_SYSTEM,
    CATEGORY_TOOLS,
    CATEGORY_GAMES,
    CATEGORY_MEDIA,
    CATEGORY_COMMUNICATION,
    CATEGORY_OTHER
};

// App metadata structure
struct AppMetadata {
    String name;
    String version;
    String author;
    String description;
    AppCategory category;
    size_t memoryRequirement;
    bool requiresSD;
    bool requiresWiFi;
    bool requiresBLE;
    const uint8_t* icon;  // 16x16 bitmap icon
};

// App lifecycle states
enum AppState {
    APP_UNLOADED,
    APP_LOADING,
    APP_RUNNING,
    APP_PAUSED,
    APP_ERROR,
    APP_EXITING,
    APP_INITIALIZING = APP_LOADING,
    APP_CLEANUP = APP_EXITING
};

// App message types for inter-app communication
enum AppMessageType {
    MSG_NONE = 0,
    MSG_ENTROPY_UPDATE,
    MSG_BATTERY_LOW,
    MSG_BATTERY_CRITICAL,
    MSG_SYSTEM_SHUTDOWN,
    MSG_MEMORY_WARNING,
    MSG_WIFI_CONNECTED,
    MSG_WIFI_DISCONNECTED,
    MSG_BLE_DEVICE_FOUND,
    MSG_SD_CARD_REMOVED,
    MSG_USER_CUSTOM = 100
};

// App message structure
struct AppMessage {
    AppMessageType type;
    void* data;
    size_t dataSize;
    unsigned long timestamp;
};

// Debug logging macro
#ifndef debugLog
#define DBG_LOG(msg) do { Serial.println(msg); } while(0)
#endif

class BaseApp {
protected:
    AppMetadata metadata;
    AppState currentState;
    unsigned long lastUpdateTime;
    bool needsRedraw;
    uint16_t backgroundColor;
    uint16_t foregroundColor;
    bool showBackButton;
    bool showStatusBar;
    unsigned long frameCount;
    
public:
    BaseApp() : currentState(APP_UNLOADED), lastUpdateTime(0), needsRedraw(true),
                backgroundColor(0), foregroundColor(0xFFFF), showBackButton(false),
                showStatusBar(false), frameCount(0) {}
    virtual ~BaseApp() {}
    
    // Pure virtual methods - must be implemented by derived classes
    virtual bool initialize() = 0;
    virtual void update() = 0;
    virtual void render() = 0;
    virtual bool handleTouch(TouchPoint touch) = 0;
    
    // Virtual methods with default implementations
    virtual void cleanup() { shutdown(); }
    virtual void shutdown() {}
    virtual void onPause() { currentState = APP_PAUSED; }
    virtual void onResume() { currentState = APP_RUNNING; }
    virtual void onLowMemory() {}
    virtual void onLowBattery() {}
    virtual const uint8_t* getIcon() const { return nullptr; }
    virtual bool saveState() { return true; }
    virtual bool loadState() { return true; }
    virtual bool handleMessage(AppMessage message) { return false; }
    virtual bool handleMessage(AppMessage message, void* data) { return handleMessage(message); }
    virtual bool handleMessage(int messageType, void* data = nullptr) {
        AppMessage msg = {static_cast<AppMessageType>(messageType), data, 0, millis()};
        return handleMessage(msg);
    }
    virtual uint8_t getSettingsCount() const { return 0; }
    virtual String getSettingName(uint8_t index) const { return ""; }
    virtual void handleSetting(uint8_t index) {}
    virtual void setAppManager(void* manager) { /* Store app manager reference */ }
    
    // Getters
    virtual String getName() const { return metadata.name; }
    size_t getMemoryRequirement() const { return metadata.memoryRequirement; }
    bool getNeedsRedraw() const { return needsRedraw; }
    
    // State management
    void setState(AppState state) { currentState = state; }
    AppState getState() const { return currentState; }
    void setNeedsRedraw(bool redraw = true) { needsRedraw = redraw; }
    
    // Common UI helpers
    void setColors(uint16_t bg, uint16_t fg) { /* default no-op, override if needed */ }
    void setShowBackButton(bool show) { /* default no-op */ }
    void setShowStatusBar(bool show) { /* default no-op */ }
    
    // Utility methods
    bool isRunning() const { return currentState == APP_RUNNING; }
    bool isPaused() const { return currentState == APP_PAUSED; }
    bool hasError() const { return currentState == APP_ERROR; }
    unsigned long getRunTime() const { return millis(); }
    size_t getMemoryUsage() const { return 0; }
    float getFPS() const { return 0.0f; }
    
    // File system helpers
    bool isSDAvailable() const { return true; }
    String getAppDataPath() const { return "/data"; }
    bool createAppDataDir() const { return true; }
    void trackMemoryUsage() {}
    
protected:
    // Helper methods for derived classes
    void setMetadata(const String& name, const String& version, const String& author,
                    const String& description, AppCategory category, size_t memReq) {
        metadata.name = name;
        metadata.version = version;
        metadata.author = author;
        metadata.description = description;
        metadata.category = category;
        metadata.memoryRequirement = memReq;
        metadata.requiresSD = false;
        metadata.requiresWiFi = false;
        metadata.requiresBLE = false;
        metadata.icon = nullptr;
    }
    
    void setRequirements(bool sd, bool wifi, bool ble) {
        metadata.requiresSD = sd;
        metadata.requiresWiFi = wifi;
        metadata.requiresBLE = ble;
    }
    
    void setIcon(const uint8_t* iconData) {
        metadata.icon = iconData;
    }
};

#endif // BASE_APP_H