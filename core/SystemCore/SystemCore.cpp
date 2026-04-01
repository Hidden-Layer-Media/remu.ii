#include "SystemCore.h"

SystemCore::SystemCore() :
    errorSystem(),
    currentState(SYSTEM_BOOT),
    currentPowerState(POWER_GOOD),
    bootTime(0),
    lastEntropyUpdate(0),
    lastPowerCheck(0),
    entropyIndex(0),
    entropyPool(0),
    entropyBuffer{},
    batteryVoltage(3.7f),
    batteryPercentage(50),
    isCharging(false)
{
    for (int i = 0; i < ENTROPY_BUFFER_SIZE; i++) {
        entropyBuffer[i] = 0;
    }
}

SystemCore::~SystemCore() {
    disableWatchdog();
}

bool SystemCore::initialize() {
    Serial.println("[SystemCore] Initializing...");
    
    bootTime = millis();
    currentState = SYSTEM_BOOT;
    
    // Initialize entropy pins as analog inputs
    pinMode(ENTROPY_PIN_1, INPUT);
    pinMode(ENTROPY_PIN_2, INPUT);
    pinMode(ENTROPY_PIN_3, INPUT);
    
    // Initialize battery monitoring pin
    pinMode(BATTERY_PIN, INPUT);
    
    // Initialize power LED
    pinMode(PWR_LED, OUTPUT);
    digitalWrite(PWR_LED, HIGH); // Power on indicator
    
    // Initialize watchdog
    initializeWatchdog();
    
    // Seed initial entropy
    entropyPool = esp_random();
    for (int i = 0; i < 10; i++) {
        updateEntropy();
        delay(1);
    }
    
    // Initial power check
    updatePower();
    
    currentState = SYSTEM_RUNNING;
    Serial.println("[SystemCore] Initialization complete");
    Serial.printf("[SystemCore] Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("[SystemCore] Initial entropy pool: 0x%08X\n", entropyPool);
    
    // Verify entropy pool
    if (entropyPool == 0) {
        Serial.println("[SystemCore] Warning: Entropy pool not properly initialized");
        logError(ERROR_ENTROPY, "Entropy pool not properly initialized");
    }
    
    return true;
}

void SystemCore::logError(ErrorCodes code, const char* message) {
    errorSystem.logError(code, message);
}

ErrorCodes SystemCore::getLastError() const {
    return errorSystem.getLastError();
}

void SystemCore::update() {
    unsigned long currentTime = millis();
    
    // Update entropy at regular intervals
    if (currentTime - lastEntropyUpdate >= ENTROPY_SAMPLE_INTERVAL) {
        updateEntropy();
        lastEntropyUpdate = currentTime;
    }
    
    // Update power monitoring
    if (currentTime - lastPowerCheck >= POWER_CHECK_INTERVAL) {
        updatePower();
        lastPowerCheck = currentTime;
    }
    
    // Feed watchdog
    feedWatchdog();
    
    // Handle low power states
    if (currentPowerState == POWER_CRITICAL) {
        // Reduce system performance in critical power state
        delay(10); // Small delay to reduce CPU usage
    }
}

void SystemCore::shutdown() {
    Serial.println("[SystemCore] Shutting down...");
    currentState = SYSTEM_SHUTDOWN;
    
    // Turn off power LED
    digitalWrite(PWR_LED, LOW);
    
    // Disable watchdog
    disableWatchdog();
    
    Serial.println("[SystemCore] Shutdown complete");
}

void SystemCore::initializeWatchdog() {
    esp_task_wdt_init(WATCHDOG_TIMEOUT, true);
    esp_task_wdt_add(NULL); // Add current task to watchdog
    Serial.printf("[SystemCore] Watchdog initialized (%d seconds timeout)\n", WATCHDOG_TIMEOUT);
}

void SystemCore::updateEntropy() {
    // Sample from multiple entropy sources
    uint32_t newEntropy = 0;
    
    // Read from floating analog pins
    newEntropy ^= analogRead(ENTROPY_PIN_1);
    newEntropy <<= 4;
    newEntropy ^= analogRead(ENTROPY_PIN_2);
    newEntropy <<= 4;
    newEntropy ^= analogRead(ENTROPY_PIN_3);
    newEntropy <<= 4;
    
    // Add timing jitter
    newEntropy ^= (micros() & 0xFFFF);
    
    // Add hardware random if available
    newEntropy ^= esp_random();
    
    // Mix into entropy pool
    mixEntropy(newEntropy);
    
    // Store in circular buffer
    entropyBuffer[entropyIndex] = (uint8_t)(newEntropy & 0xFF);
    entropyIndex = (entropyIndex + 1) % ENTROPY_BUFFER_SIZE;
}

void SystemCore::updateEntropyFromPin(uint8_t pin) {
    uint16_t reading = analogRead(pin);
    mixEntropy(reading);
}

void SystemCore::mixEntropy(uint32_t newEntropy) {
    // Simple entropy mixing function
    entropyPool ^= newEntropy;
    entropyPool = (entropyPool << 1) | (entropyPool >> 31); // Rotate left
    entropyPool ^= millis(); // Add timing component
}

void SystemCore::updatePower() {
    batteryVoltage = readBatteryVoltage();
    batteryPercentage = calculateBatteryPercentage(batteryVoltage);
    
    // Determine power state
    if (batteryPercentage > 75) {
        currentPowerState = POWER_FULL;
    } else if (batteryPercentage > 25) {
        currentPowerState = POWER_GOOD;
    } else if (batteryPercentage > 10) {
        currentPowerState = POWER_LOW;
    } else {
        currentPowerState = POWER_CRITICAL;
    }
    
    // Simple charging detection (voltage rising)
    static float lastVoltage = batteryVoltage;
    isCharging = (batteryVoltage > lastVoltage + 0.1f);
    lastVoltage = batteryVoltage;
}

float SystemCore::readBatteryVoltage() {
    // Read ADC value (0-4095 for 12-bit ADC)
    int adcValue = analogRead(BATTERY_PIN);
    
    // Convert to voltage (assuming 3.3V reference and voltage divider)
    // Adjust these values based on your voltage divider circuit
    float voltage = (adcValue / 4095.0f) * 3.3f * 2.0f; // *2 for voltage divider
    
    return voltage;
}

uint8_t SystemCore::calculateBatteryPercentage(float voltage) {
    // LiPo battery voltage curve (approximate)
    // 4.2V = 100%, 3.7V = 50%, 3.0V = 0%
    if (voltage >= 4.2f) return 100;
    if (voltage <= 3.0f) return 0;
    
    // Linear approximation
    return (uint8_t)((voltage - 3.0f) / 1.2f * 100.0f);
}

bool SystemCore::isSystemHealthy() const {
    return (currentState == SYSTEM_RUNNING && 
            currentPowerState != POWER_CRITICAL &&
            ESP.getFreeHeap() > 10000); // At least 10KB free heap
}

// Entropy generation methods
uint32_t SystemCore::getRandomSeed() {
    updateEntropy();
    return entropyPool;
}

uint8_t SystemCore::getRandomByte() {
    updateEntropy();
    // entropyIndex was incremented after the write, so read the slot just written
    uint8_t idx = (entropyIndex == 0) ? ENTROPY_BUFFER_SIZE - 1 : entropyIndex - 1;
    return entropyBuffer[idx];
}

uint16_t SystemCore::getRandomWord() {
    return (getRandomByte() << 8) | getRandomByte();
}

uint32_t SystemCore::getRandomDWord() {
    return ((uint32_t)getRandomWord() << 16) | getRandomWord();
}

void SystemCore::getRandomBytes(uint8_t* buffer, size_t length) {
    for (size_t i = 0; i < length; i++) {
        buffer[i] = getRandomByte();
    }
}

// System information methods
unsigned long SystemCore::getUptime() const {
    return millis() - bootTime;
}

unsigned long SystemCore::getUptimeSeconds() const {
    return getUptime() / 1000;
}

size_t SystemCore::getFreeHeap() const {
    return ESP.getFreeHeap();
}

size_t SystemCore::getMinFreeHeap() const {
    return ESP.getMinFreeHeap();
}

float SystemCore::getCPUTemperature() const {
    // ESP32 internal temperature sensor (approximate)
    return ((float)ESP.getChipRevision() * 10.0f) + 25.0f; // Placeholder
}

// Watchdog management
void SystemCore::feedWatchdog() {
    esp_task_wdt_reset();
}

void SystemCore::enableWatchdog() {
    esp_task_wdt_init(WATCHDOG_TIMEOUT, true);
    esp_task_wdt_add(NULL);
}

void SystemCore::disableWatchdog() {
    esp_task_wdt_delete(NULL);
}

// System utilities
void SystemCore::resetSystem() {
    Serial.println("[SystemCore] System reset requested");
    ESP.restart();
}

void SystemCore::enterDeepSleep(uint64_t sleepTimeMs) {
    Serial.printf("[SystemCore] Entering deep sleep for %llu ms\n", sleepTimeMs);
    esp_deep_sleep(sleepTimeMs * 1000); // Convert to microseconds
}

String SystemCore::getSystemInfo() const {
    String info = "=== remu.ii System Information ===\n";
    info += "Uptime: " + String(getUptimeSeconds()) + " seconds\n";
    info += "Free Heap: " + String(getFreeHeap()) + " bytes\n";
    info += "Min Free Heap: " + String(getMinFreeHeap()) + " bytes\n";
    info += "Battery: " + String(batteryPercentage) + "% (" + String(batteryVoltage, 2) + "V)\n";
    info += "Power State: " + String(currentPowerState) + "\n";
    info += "System State: " + String(currentState) + "\n";
    info += "Entropy Pool: 0x" + String(entropyPool, HEX) + "\n";
    return info;
}

void SystemCore::dumpSystemStats() const {
    Serial.println(getSystemInfo());
}