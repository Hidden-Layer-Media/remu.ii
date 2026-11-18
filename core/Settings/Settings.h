#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../FileSystem.h"
#include "../Config.h"

// ========================================
// Settings Management System for remu.ii
// Persistent storage and management of system-wide configuration
// ========================================

// Setting types for validation and UI rendering
enum SettingType {
    SETTING_BOOL,
    SETTING_INT,
    SETTING_FLOAT,
    SETTING_STRING,
    SETTING_ENUM,
    SETTING_COLOR
};

// Setting categories for organization
enum SettingCategory {
    SC_AUDIO,
    SC_DISPLAY,
    SC_SYSTEM,
    SC_INTERFACE,
    SC_SECURITY,
    SC_PERFORMANCE,
    SC_DEBUG
};

// Individual setting definition
struct Setting {
    String key;                 // Unique setting identifier
    String name;                // Display name
    String description;         // Help text
    SettingType type;          // Data type
    SettingCategory category;   // Category for organization
    
    // Value storage (union would be more memory efficient, but this is clearer)
    bool boolValue;
    int intValue;
    float floatValue;
    String stringValue;
    uint16_t colorValue;
    
    // Constraints
    int minValue;
    int maxValue;
    String* enumOptions;        // Array of enum option strings
    uint8_t enumCount;          // Number of enum options
    
    // Default value
    bool defaultBool;
    int defaultInt;
    float defaultFloat;
    String defaultString;
    uint16_t defaultColor;
    
    // Flags
    bool needsRestart;          // Setting requires system restart
    bool isReadOnly;            // Setting cannot be modified
    bool isVisible;             // Setting visible in UI
};

// Settings change callback function type
typedef void (*SettingsChangeCallback)(const String& key, const Setting& setting);

class Settings {
private:
    static Settings* instance;
    
    // Settings storage
    Setting* _settings;
    uint8_t settingCount;
    uint8_t maxSettings;
    
    // File paths
    String configPath;
    String backupPath;
    
    // Change callbacks
    SettingsChangeCallback changeCallback;
    
    // Private constructor for singleton
    Settings();
    
    // Private methods
    void initializeDefaultSettings();
    bool validateSetting(const String& key, const Setting& setting);
    Setting* findSetting(const String& key);
    bool loadFromJson(const String& jsonStr);
    String saveToJson();
    void notifyChange(const String& key, const Setting& setting);
    
public:
    // Singleton access
    static Settings& getInstance();
    static void cleanup();
    
    // Initialization and persistence
    bool initialize();
    bool loadSettings();
    bool saveSettings();
    bool resetToDefaults();
    bool createBackup();
    bool restoreBackup();
    
    // Setting registration (for apps to add custom settings)
    bool registerSetting(const Setting& setting);
    bool unregisterSetting(const String& key);
    
    // Value getters
    bool getBool(const String& key, bool defaultValue = false);
    int getInt(const String& key, int defaultValue = 0);
    float getFloat(const String& key, float defaultValue = 0.0);
    String getString(const String& key, const String& defaultValue = "");
    uint16_t getColor(const String& key, uint16_t defaultValue = 0x0000);
    
    // Value setters
    bool setBool(const String& key, bool value);
    bool setInt(const String& key, int value);
    bool setFloat(const String& key, float value);
    bool setString(const String& key, const String& value);
    bool setColor(const String& key, uint16_t value);
    
    // Enum helpers
    int getEnumIndex(const String& key, int defaultIndex = 0);
    String getEnumValue(const String& key, const String& defaultValue = "");
    bool setEnumIndex(const String& key, int index);
    bool setEnumValue(const String& key, const String& value);
    
    // Setting queries
    bool exists(const String& key);
    SettingType getType(const String& key);
    SettingCategory getCategory(const String& key);
    String getName(const String& key);
    String getDescription(const String& key);
    bool needsRestart(const String& key);
    bool isReadOnly(const String& key);
    
    // Category queries
    uint8_t getSettingsInCategory(SettingCategory category, String* keys, uint8_t maxKeys);
    String getCategoryName(SettingCategory category);
    
    // All settings
    uint8_t getAllSettings(String* keys, uint8_t maxKeys);
    uint8_t getSettingCount() const { return settingCount; }
    
    // Change notification
    void setChangeCallback(SettingsChangeCallback callback);
    
    // Utility methods
    bool isValidKey(const String& key);
    String getSettingsInfo();
    void printSettings();
    
    // Predefined setting keys (constants for type safety)
    static const String AUDIO_ENABLED;
    static const String AUDIO_VOLUME;
    static const String AUDIO_SAMPLE_RATE;
    static const String AUDIO_OUTPUT_MODE;
    
    static const String DISPLAY_BRIGHTNESS;
    static const String DISPLAY_TIMEOUT;
    static const String DISPLAY_THEME;
    static const String DISPLAY_ORIENTATION;
    static const String DISPLAY_ANIMATIONS;
    
    static const String SYSTEM_PET_CHOICE;
    static const String SYSTEM_LANGUAGE;
    static const String SYSTEM_TIMEZONE;
    static const String SYSTEM_AUTO_SAVE;
    static const String SYSTEM_DEBUG_MODE;
    
    static const String INTERFACE_TOUCH_SENSITIVITY;
    static const String INTERFACE_HAPTIC_FEEDBACK;
    static const String INTERFACE_BUTTON_SOUNDS;
    static const String INTERFACE_DOUBLE_TAP_SPEED;
    
    static const String SECURITY_AUTO_LOCK;
    static const String SECURITY_LOCK_TIMEOUT;
    static const String SECURITY_REQUIRE_PIN;
    static const String SECURITY_HIDE_SENSITIVE;
    
    static const String PERFORMANCE_FRAME_RATE;
    static const String PERFORMANCE_MEMORY_MONITOR;
    static const String PERFORMANCE_BATTERY_SAVER;
    static const String PERFORMANCE_CPU_FREQUENCY;
    
    static const String DEBUG_LOG_LEVEL;
    static const String DEBUG_SERIAL_OUTPUT;
    static const String DEBUG_SHOW_FPS;
    static const String DEBUG_MEMORY_INFO;
};

// Global settings instance macro for convenience
#define settings Settings::getInstance()

// Setting helper macros
#define SETTINGS_BOOL(key, value) settings.setBool(key, value)
#define SETTINGS_INT(key, value) settings.setInt(key, value)
#define SETTINGS_STRING(key, value) settings.setString(key, value)

#define GET_BOOL(key, default) settings.getBool(key, default)
#define GET_INT(key, default) settings.getInt(key, default)
#define GET_STRING(key, default) settings.getString(key, default)

#endif // SETTINGS_H