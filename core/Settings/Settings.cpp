#include "Settings.h"

// Static instance for singleton
Settings* Settings::instance = nullptr;

// Predefined setting keys
const String Settings::AUDIO_ENABLED = "audio.enabled";
const String Settings::AUDIO_VOLUME = "audio.volume";
const String Settings::AUDIO_SAMPLE_RATE = "audio.sample_rate";
const String Settings::AUDIO_OUTPUT_MODE = "audio.output_mode";

const String Settings::DISPLAY_BRIGHTNESS = "display.brightness";
const String Settings::DISPLAY_TIMEOUT = "display.timeout";
const String Settings::DISPLAY_THEME = "display.theme";
const String Settings::DISPLAY_ORIENTATION = "display.orientation";
const String Settings::DISPLAY_ANIMATIONS = "display.animations";

const String Settings::SYSTEM_PET_CHOICE = "system.pet_choice";
const String Settings::SYSTEM_LANGUAGE = "system.language";
const String Settings::SYSTEM_TIMEZONE = "system.timezone";
const String Settings::SYSTEM_AUTO_SAVE = "system.auto_save";
const String Settings::SYSTEM_DEBUG_MODE = "system.debug_mode";

const String Settings::INTERFACE_TOUCH_SENSITIVITY = "interface.touch_sensitivity";
const String Settings::INTERFACE_HAPTIC_FEEDBACK = "interface.haptic_feedback";
const String Settings::INTERFACE_BUTTON_SOUNDS = "interface.button_sounds";
const String Settings::INTERFACE_DOUBLE_TAP_SPEED = "interface.double_tap_speed";

const String Settings::SECURITY_AUTO_LOCK = "security.auto_lock";
const String Settings::SECURITY_LOCK_TIMEOUT = "security.lock_timeout";
const String Settings::SECURITY_REQUIRE_PIN = "security.require_pin";
const String Settings::SECURITY_HIDE_SENSITIVE = "security.hide_sensitive";

const String Settings::PERFORMANCE_FRAME_RATE = "performance.frame_rate";
const String Settings::PERFORMANCE_MEMORY_MONITOR = "performance.memory_monitor";
const String Settings::PERFORMANCE_BATTERY_SAVER = "performance.battery_saver";
const String Settings::PERFORMANCE_CPU_FREQUENCY = "performance.cpu_frequency";

const String Settings::DEBUG_LOG_LEVEL = "debug.log_level";
const String Settings::DEBUG_SERIAL_OUTPUT = "debug.serial_output";
const String Settings::DEBUG_SHOW_FPS = "debug.show_fps";
const String Settings::DEBUG_MEMORY_INFO = "debug.memory_info";

Settings::Settings() :
    settings(nullptr),
    settingCount(0),
    maxSettings(20),
    configPath("/settings/config.json"),
    backupPath("/settings/config.bak"),
    changeCallback(nullptr)
{
    settings = new Setting[maxSettings];
}

Settings& Settings::getInstance() {
    if (!instance) {
        instance = new Settings();
    }
    return *instance;
}

void Settings::cleanup() {
    if (instance) {
        delete[] instance->settings;
        delete instance;
        instance = nullptr;
    }
}

bool Settings::initialize() {
    Serial.println("[Settings] Initializing settings system...");
    
    // Ensure settings directory exists
    if (!filesystem.ensureDirExists("/settings")) {
        Serial.println("[Settings] WARNING: Could not create settings directory");
    }
    
    // Initialize default settings
    initializeDefaultSettings();
    
    // Try to load existing settings
    if (!loadSettings()) {
        Serial.println("[Settings] No existing settings found, using defaults");
        saveSettings(); // Save defaults
    }
    
    Serial.printf("[Settings] Initialized with %d settings\\n", settingCount);
    return true;
}

void Settings::initializeDefaultSettings() {
    // Audio settings
    registerSetting({
        AUDIO_ENABLED, "Audio Enabled", "Enable/disable audio output",
        SETTING_BOOL, CATEGORY_AUDIO,
        true, 0, 0.0f, "", 0x0000,
        0, 1, nullptr, 0,
        true, 0, 0.0f, "", 0x0000,
        false, false, true
    });
    
    registerSetting({
        AUDIO_VOLUME, "Volume", "Audio output volume (0-127)",
        SETTING_INT, CATEGORY_AUDIO,
        false, 80, 0.0f, "", 0x0000,
        0, 127, nullptr, 0,
        false, 80, 0.0f, "", 0x0000,
        false, false, true
    });
    
    // Display settings
    registerSetting({
        DISPLAY_BRIGHTNESS, "Brightness", "Screen brightness (0-255)",
        SETTING_INT, CATEGORY_DISPLAY,
        false, 200, 0.0f, "", 0x0000,
        0, 255, nullptr, 0,
        false, 200, 0.0f, "", 0x0000,
        false, false, true
    });
    
    registerSetting({
        DISPLAY_TIMEOUT, "Screen Timeout", "Auto-dim timeout in seconds",
        SETTING_INT, CATEGORY_DISPLAY,
        false, 30, 0.0f, "", 0x0000,
        5, 300, nullptr, 0,
        false, 30, 0.0f, "", 0x0000,
        false, false, true
    });
    
    registerSetting({
        DISPLAY_ANIMATIONS, "Animations", "Enable UI animations",
        SETTING_BOOL, CATEGORY_DISPLAY,
        true, 0, 0.0f, "", 0x0000,
        0, 1, nullptr, 0,
        true, 0, 0.0f, "", 0x0000,
        false, false, true
    });
    
    // System settings
    registerSetting({
        SYSTEM_AUTO_SAVE, "Auto Save", "Automatically save app states",
        SETTING_BOOL, CATEGORY_SYSTEM,
        true, 0, 0.0f, "", 0x0000,
        0, 1, nullptr, 0,
        true, 0, 0.0f, "", 0x0000,
        false, false, true
    });
    
    registerSetting({
        SYSTEM_DEBUG_MODE, "Debug Mode", "Enable debug output",
        SETTING_BOOL, CATEGORY_SYSTEM,
        false, 0, 0.0f, "", 0x0000,
        0, 1, nullptr, 0,
        false, 0, 0.0f, "", 0x0000,
        true, false, true
    });
    
    // Interface settings
    registerSetting({
        INTERFACE_TOUCH_SENSITIVITY, "Touch Sensitivity", "Touch pressure threshold",
        SETTING_INT, CATEGORY_INTERFACE,
        false, 200, 0.0f, "", 0x0000,
        50, 500, nullptr, 0,
        false, 200, 0.0f, "", 0x0000,
        false, false, true
    });
    
    registerSetting({
        INTERFACE_BUTTON_SOUNDS, "Button Sounds", "Play sounds on button press",
        SETTING_BOOL, CATEGORY_INTERFACE,
        true, 0, 0.0f, "", 0x0000,
        0, 1, nullptr, 0,
        true, 0, 0.0f, "", 0x0000,
        false, false, true
    });
    
    // Performance settings
    registerSetting({
        PERFORMANCE_FRAME_RATE, "Target FPS", "Target frame rate (15-60)",
        SETTING_INT, CATEGORY_PERFORMANCE,
        false, 30, 0.0f, "", 0x0000,
        15, 60, nullptr, 0,
        false, 30, 0.0f, "", 0x0000,
        true, false, true
    });
    
    registerSetting({
        PERFORMANCE_BATTERY_SAVER, "Battery Saver", "Enable battery saving mode",
        SETTING_BOOL, CATEGORY_PERFORMANCE,
        false, 0, 0.0f, "", 0x0000,
        0, 1, nullptr, 0,
        false, 0, 0.0f, "", 0x0000,
        false, false, true
    });
    
    // Debug settings
    registerSetting({
        DEBUG_SERIAL_OUTPUT, "Serial Debug", "Enable serial debug output",
        SETTING_BOOL, CATEGORY_DEBUG,
        true, 0, 0.0f, "", 0x0000,
        0, 1, nullptr, 0,
        true, 0, 0.0f, "", 0x0000,
        false, false, true
    });
    
    registerSetting({
        DEBUG_SHOW_FPS, "Show FPS", "Display FPS counter",
        SETTING_BOOL, CATEGORY_DEBUG,
        false, 0, 0.0f, "", 0x0000,
        0, 1, nullptr, 0,
        false, 0, 0.0f, "", 0x0000,
        false, false, true
    });
}

bool Settings::loadSettings() {
    if (!filesystem.fileExists(configPath)) {
        return false;
    }
    
    String jsonStr = filesystem.readFile(configPath);
    if (jsonStr.isEmpty()) {
        return false;
    }
    
    return loadFromJson(jsonStr);
}

bool Settings::saveSettings() {
    String jsonStr = saveToJson();
    if (jsonStr.isEmpty()) {
        return false;
    }
    
    return filesystem.writeFile(configPath, jsonStr);
}

bool Settings::loadFromJson(const String& jsonStr) {
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, jsonStr);
    
    if (error) {
        Serial.printf("[Settings] JSON parse error: %s\\n", error.c_str());
        return false;
    }
    
    JsonObject root = doc.as<JsonObject>();
    
    for (JsonPair kv : root) {
        String key = kv.key().c_str();
        Setting* setting = findSetting(key);
        
        if (setting) {
            switch (setting->type) {
                case SETTING_BOOL:
                    setting->boolValue = kv.value().as<bool>();
                    break;
                case SETTING_INT:
                    setting->intValue = kv.value().as<int>();
                    break;
                case SETTING_FLOAT:
                    setting->floatValue = kv.value().as<float>();
                    break;
                case SETTING_STRING:
                    setting->stringValue = kv.value().as<String>();
                    break;
                case SETTING_COLOR:
                    setting->colorValue = kv.value().as<uint16_t>();
                    break;
                case SETTING_ENUM:
                    setting->intValue = kv.value().as<int>();
                    break;
            }
        }
    }
    
    return true;
}

String Settings::saveToJson() {
    DynamicJsonDocument doc(4096);
    
    for (uint8_t i = 0; i < settingCount; i++) {
        Setting& setting = settings[i];
        
        switch (setting.type) {
            case SETTING_BOOL:
                doc[setting.key] = setting.boolValue;
                break;
            case SETTING_INT:
            case SETTING_ENUM:
                doc[setting.key] = setting.intValue;
                break;
            case SETTING_FLOAT:
                doc[setting.key] = setting.floatValue;
                break;
            case SETTING_STRING:
                doc[setting.key] = setting.stringValue;
                break;
            case SETTING_COLOR:
                doc[setting.key] = setting.colorValue;
                break;
        }
    }
    
    String jsonStr;
    serializeJson(doc, jsonStr);
    return jsonStr;
}

bool Settings::registerSetting(const Setting& setting) {
    if (settingCount >= maxSettings) {
        Serial.println("[Settings] ERROR: Maximum settings reached");
        return false;
    }
    
    if (!isValidKey(setting.key)) {
        Serial.printf("[Settings] ERROR: Invalid setting key: %s\\n", setting.key.c_str());
        return false;
    }
    
    // Check if setting already exists
    if (findSetting(setting.key)) {
        Serial.printf("[Settings] WARNING: Setting already exists: %s\\n", setting.key.c_str());
        return false;
    }
    
    settings[settingCount] = setting;
    settingCount++;
    
    return true;
}

Setting* Settings::findSetting(const String& key) {
    for (uint8_t i = 0; i < settingCount; i++) {
        if (settings[i].key.equals(key)) {
            return &settings[i];
        }
    }
    return nullptr;
}

// Value getters
bool Settings::getBool(const String& key, bool defaultValue) {
    Setting* setting = findSetting(key);
    if (setting && setting->type == SETTING_BOOL) {
        return setting->boolValue;
    }
    return defaultValue;
}

int Settings::getInt(const String& key, int defaultValue) {
    Setting* setting = findSetting(key);
    if (setting && (setting->type == SETTING_INT || setting->type == SETTING_ENUM)) {
        return setting->intValue;
    }
    return defaultValue;
}

float Settings::getFloat(const String& key, float defaultValue) {
    Setting* setting = findSetting(key);
    if (setting && setting->type == SETTING_FLOAT) {
        return setting->floatValue;
    }
    return defaultValue;
}

String Settings::getString(const String& key, const String& defaultValue) {
    Setting* setting = findSetting(key);
    if (setting && setting->type == SETTING_STRING) {
        return setting->stringValue;
    }
    return defaultValue;
}

uint16_t Settings::getColor(const String& key, uint16_t defaultValue) {
    Setting* setting = findSetting(key);
    if (setting && setting->type == SETTING_COLOR) {
        return setting->colorValue;
    }
    return defaultValue;
}

// Value setters
bool Settings::setBool(const String& key, bool value) {
    Setting* setting = findSetting(key);
    if (setting && setting->type == SETTING_BOOL && !setting->isReadOnly) {
        setting->boolValue = value;
        notifyChange(key, *setting);
        return true;
    }
    return false;
}

bool Settings::setInt(const String& key, int value) {
    Setting* setting = findSetting(key);
    if (setting && (setting->type == SETTING_INT || setting->type == SETTING_ENUM) && !setting->isReadOnly) {
        // Apply constraints
        if (value < setting->minValue) value = setting->minValue;
        if (value > setting->maxValue) value = setting->maxValue;
        
        setting->intValue = value;
        notifyChange(key, *setting);
        return true;
    }
    return false;
}

bool Settings::setFloat(const String& key, float value) {
    Setting* setting = findSetting(key);
    if (setting && setting->type == SETTING_FLOAT && !setting->isReadOnly) {
        setting->floatValue = value;
        notifyChange(key, *setting);
        return true;
    }
    return false;
}

bool Settings::setString(const String& key, const String& value) {
    Setting* setting = findSetting(key);
    if (setting && setting->type == SETTING_STRING && !setting->isReadOnly) {
        setting->stringValue = value;
        notifyChange(key, *setting);
        return true;
    }
    return false;
}

bool Settings::setColor(const String& key, uint16_t value) {
    Setting* setting = findSetting(key);
    if (setting && setting->type == SETTING_COLOR && !setting->isReadOnly) {
        setting->colorValue = value;
        notifyChange(key, *setting);
        return true;
    }
    return false;
}

bool Settings::exists(const String& key) {
    return findSetting(key) != nullptr;
}

bool Settings::isValidKey(const String& key) {
    if (key.length() == 0 || key.length() > 32) {
        return false;
    }
    
    // Key should contain only alphanumeric characters, dots, and underscores
    for (int i = 0; i < key.length(); i++) {
        char c = key[i];
        if (!isalnum(c) && c != '.' && c != '_') {
            return false;
        }
    }
    
    return true;
}

void Settings::notifyChange(const String& key, const Setting& setting) {
    if (changeCallback) {
        changeCallback(key, setting);
    }
}

void Settings::setChangeCallback(SettingsChangeCallback callback) {
    changeCallback = callback;
}

bool Settings::resetToDefaults() {
    for (uint8_t i = 0; i < settingCount; i++) {
        Setting& setting = settings[i];
        
        switch (setting.type) {
            case SETTING_BOOL:
                setting.boolValue = setting.defaultBool;
                break;
            case SETTING_INT:
            case SETTING_ENUM:
                setting.intValue = setting.defaultInt;
                break;
            case SETTING_FLOAT:
                setting.floatValue = setting.defaultFloat;
                break;
            case SETTING_STRING:
                setting.stringValue = setting.defaultString;
                break;
            case SETTING_COLOR:
                setting.colorValue = setting.defaultColor;
                break;
        }
    }
    
    return saveSettings();
}

String Settings::getCategoryName(SettingCategory category) {
    switch (category) {
        case CATEGORY_AUDIO: return "Audio";
        case CATEGORY_DISPLAY: return "Display";
        case CATEGORY_SYSTEM: return "System";
        case CATEGORY_INTERFACE: return "Interface";
        case CATEGORY_SECURITY: return "Security";
        case CATEGORY_PERFORMANCE: return "Performance";
        case CATEGORY_DEBUG: return "Debug";
        default: return "Unknown";
    }
}

void Settings::printSettings() {
    Serial.println("[Settings] Current Settings:");
    for (uint8_t i = 0; i < settingCount; i++) {
        Setting& setting = settings[i];
        Serial.printf("  %s = ", setting.key.c_str());
        
        switch (setting.type) {
            case SETTING_BOOL:
                Serial.println(setting.boolValue ? "true" : "false");
                break;
            case SETTING_INT:
            case SETTING_ENUM:
                Serial.println(setting.intValue);
                break;
            case SETTING_FLOAT:
                Serial.println(setting.floatValue);
                break;
            case SETTING_STRING:
                Serial.println(setting.stringValue);
                break;
            case SETTING_COLOR:
                Serial.printf("0x%04X\\n", setting.colorValue);
                break;
        }
    }
}

bool Settings::validateSetting(const String& key, const Setting& setting) { return isValidKey(key); }
bool Settings::createBackup() { return filesystem.copyFile(configPath, backupPath); }
bool Settings::restoreBackup() { return filesystem.copyFile(backupPath, configPath) && loadSettings(); }
bool Settings::unregisterSetting(const String& key) {
    for (uint8_t i = 0; i < settingCount; i++) {
        if (settings[i].key.equals(key)) {
            for (uint8_t j = i; j < settingCount - 1; j++) settings[j] = settings[j+1];
            settingCount--;
            return true;
        }
    }
    return false;
}
int Settings::getEnumIndex(const String& key, int defaultIndex) { return getInt(key, defaultIndex); }
String Settings::getEnumValue(const String& key, const String& defaultValue) {
    Setting* s = findSetting(key);
    if (s && s->type == SETTING_ENUM && s->enumOptions && s->intValue < s->enumCount)
        return s->enumOptions[s->intValue];
    return defaultValue;
}
bool Settings::setEnumIndex(const String& key, int index) { return setInt(key, index); }
bool Settings::setEnumValue(const String& key, const String& value) {
    Setting* s = findSetting(key);
    if (!s || s->type != SETTING_ENUM) return false;
    for (uint8_t i = 0; i < s->enumCount; i++) {
        if (s->enumOptions[i].equals(value)) return setInt(key, i);
    }
    return false;
}
SettingType Settings::getType(const String& key) {
    Setting* s = findSetting(key);
    return s ? s->type : SETTING_BOOL;
}
SettingCategory Settings::getCategory(const String& key) {
    Setting* s = findSetting(key);
    return s ? s->category : CATEGORY_SYSTEM;
}
String Settings::getName(const String& key) {
    Setting* s = findSetting(key);
    return s ? s->name : "";
}
String Settings::getDescription(const String& key) {
    Setting* s = findSetting(key);
    return s ? s->description : "";
}
bool Settings::needsRestart(const String& key) {
    Setting* s = findSetting(key);
    return s ? s->needsRestart : false;
}
bool Settings::isReadOnly(const String& key) {
    Setting* s = findSetting(key);
    return s ? s->isReadOnly : false;
}
uint8_t Settings::getSettingsInCategory(SettingCategory category, String* keys, uint8_t maxKeys) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < settingCount && count < maxKeys; i++) {
        if (settings[i].category == category) keys[count++] = settings[i].key;
    }
    return count;
}
uint8_t Settings::getAllSettings(String* keys, uint8_t maxKeys) {
    uint8_t count = min(settingCount, maxKeys);
    for (uint8_t i = 0; i < count; i++) keys[i] = settings[i].key;
    return count;
}
String Settings::getSettingsInfo() {
    return "Settings: " + String(settingCount) + "/" + String(maxSettings);
}
