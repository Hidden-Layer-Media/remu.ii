#ifndef BLE_SCANNER_H
#define BLE_SCANNER_H

#include "../../core/AppManager/BaseApp.h"
#include "../../core/FileSystem.h"
#include "../../core/Config.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <vector>
#include <map>

// ========================================
// BLEScanner - Advanced BLE device scanning and analysis
// Provides comprehensive BLE monitoring with anomaly detection
// ========================================

// Anomaly detection types
enum AnomalyType {
    ANOMALY_NONE = 0,
    ANOMALY_NEW_DEVICE = 1,
    ANOMALY_RSSI_SUDDEN_CHANGE = 2,
    ANOMALY_RSSI_OUTLIER = 4,
    ANOMALY_MAC_RANDOMIZED = 8,
    ANOMALY_TIMING_IRREGULAR = 16,
    ANOMALY_ENTROPY_HIGH = 32,
    ANOMALY_ENTROPY_LOW = 64,
    ANOMALY_SIGNAL_SPOOFING = 128,
    ANOMALY_RAPID_APPEARING = 256
};

// Device status flags
enum DeviceStatus {
    DEVICE_ACTIVE = 1,
    DEVICE_TIMEOUT = 2,
    DEVICE_LABELED = 4,
    DEVICE_SUSPICIOUS = 8,
    DEVICE_NEW = 16,
    DEVICE_HIDDEN = 32
};

// UI View modes
enum BLEScanner_ViewMode {
    BLESCANNER_VIEW_DEVICE_LIST,
    BLESCANNER_VIEW_DEVICE_DETAILS,
    BLESCANNER_VIEW_ANOMALY_ALERTS,
    BLESCANNER_VIEW_STATISTICS,
    BLESCANNER_VIEW_LABELING,
    BLESCANNER_VIEW_LOGS
};

// Touch interaction zones
enum BLEScanner_TouchZone {
    BLESCANNER_ZONE_NONE,
    BLESCANNER_ZONE_DEVICE_LIST,
    BLESCANNER_ZONE_BACK_BUTTON,
    BLESCANNER_ZONE_SCAN_TOGGLE,
    BLESCANNER_ZONE_VIEW_TOGGLE,
    BLESCANNER_ZONE_DEVICE_ENTRY,
    BLESCANNER_ZONE_LABEL_BUTTON,
    BLESCANNER_ZONE_LOG_BUTTON,
    BLESCANNER_ZONE_ALERT_DISMISS
};

// RSSI history for anomaly detection
struct RSSIHistory {
    std::vector<int8_t> values;
    float mean;
    float variance;
    float standardDeviation;
    int8_t min;
    int8_t max;
    unsigned long lastUpdated;
    
    void addValue(int8_t rssi) {
        values.push_back(rssi);
        if (values.size() > 20) { // Keep last 20 readings
            values.erase(values.begin());
        }
        updateStatistics();
    }
    
    void updateStatistics();
    bool isOutlier(int8_t rssi) const;
};

// BLE device information with extended tracking
struct BLEDeviceInfo {
    String macAddress;
    String deviceName;
    String label;
    int8_t rssi;
    RSSIHistory rssiHistory;
    unsigned long firstSeen;
    unsigned long lastSeen;
    unsigned long lastUpdate;
    uint32_t scanCount;
    uint32_t statusFlags;
    uint32_t anomalies;
    float entropyScore;
    bool isMacRandomized;
    std::vector<unsigned long> appearanceTimes;
    
    // Constructor
    BLEDeviceInfo() : 
        rssi(-100), firstSeen(0), lastSeen(0), lastUpdate(0),
        scanCount(0), statusFlags(DEVICE_NEW), anomalies(ANOMALY_NONE),
        entropyScore(0.0), isMacRandomized(false) {}
    
    // Helper methods
    bool isActive() const { return (millis() - lastSeen) < BLE_DEVICE_TIMEOUT; }
    bool isLabeled() const { return !label.isEmpty(); }
    bool hasAnomalies() const { return anomalies != ANOMALY_NONE; }
    String getStatusString() const;
    String getAnomalyString() const;
};

// Anomaly event logging
struct AnomalyEvent {
    unsigned long timestamp;
    String macAddress;
    AnomalyType type;
    String description;
    float severity; // 0.0 - 1.0
    String details;
    
    AnomalyEvent(const String& mac, AnomalyType t, const String& desc, float sev) :
        timestamp(millis()), macAddress(mac), type(t), 
        description(desc), severity(sev) {}
};

// Statistics tracking
struct ScanStatistics {
    unsigned long totalScanTime;
    uint32_t totalDevicesFound;
    uint32_t uniqueDevicesFound;
    uint32_t labeledDevices;
    uint32_t anomaliesDetected;
    uint32_t newDevicesToday;
    float averageRSSI;
    float entropyMean;
    unsigned long lastReset;
    
    ScanStatistics() : totalScanTime(0), totalDevicesFound(0), 
        uniqueDevicesFound(0), labeledDevices(0), anomaliesDetected(0),
        newDevicesToday(0), averageRSSI(-70.0), entropyMean(0.5),
        lastReset(millis()) {}
};

// UI state management
struct BLEScanner_UIState {
    BLEScanner_ViewMode currentView;
    int selectedDevice;
    int scrollOffset;
    bool scanningActive;
    bool showAnomalyAlert;
    String alertMessage;
    unsigned long lastUIUpdate;
    TouchPoint lastTouch;
    unsigned long lastTouchTime;
    
    BLEScanner_UIState() : currentView(BLESCANNER_VIEW_DEVICE_LIST), selectedDevice(-1),
        scrollOffset(0), scanningActive(false), showAnomalyAlert(false),
        lastUIUpdate(0), lastTouchTime(0) {}
};

// Configuration settings
struct ScanConfig {
    uint32_t scanDuration;
    uint32_t scanInterval;
    int8_t rssiThreshold;
    bool enableAnomalyDetection;
    bool autoLabelKnownDevices;
    bool logToSD;
    float anomalySensitivity;
    uint32_t deviceTimeout;
    
    ScanConfig() : scanDuration(BLE_SCAN_DURATION_SEC * 1000),
        scanInterval(BLE_SCAN_INTERVAL), rssiThreshold(BLE_RSSI_THRESHOLD),
        enableAnomalyDetection(true), autoLabelKnownDevices(true),
        logToSD(true), anomalySensitivity(0.7), deviceTimeout(BLE_DEVICE_TIMEOUT) {}
};

class BLEScanner : public BaseApp {
private:
    // BLE scanning components
    BLEScan* pBLEScan;
    bool bleInitialized;
    bool scanning;
    unsigned long lastScanTime;
    unsigned long scanStartTime;
    
    // Device tracking
    std::map<String, BLEDeviceInfo> devices;
    std::vector<String> deviceOrder; // For consistent UI ordering
    uint32_t deviceCount;
    
    // Anomaly detection
    std::vector<AnomalyEvent> anomalyEvents;
    float entropyPool[256]; // For entropy calculation
    uint32_t entropyIndex;
    unsigned long lastAnomalyCheck;
    
    // Statistics and logging
    ScanStatistics stats;
    String logFilePath;
    String labelFilePath;
    String configFilePath;
    unsigned long lastLogWrite;
    
    // UI state
    BLEScanner_UIState uiState;
    ScanConfig config;
    
    // Colors for UI
    uint16_t colorNormal;
    uint16_t colorLabeled;
    uint16_t colorAnomaly;
    uint16_t colorNew;
    uint16_t colorBackground;
    uint16_t colorText;
    
    // ===== CORE BLE METHODS =====
    bool initializeBLE();
    void startScan();
    void stopScan();
    void processScanResults();
    void updateDeviceInfo(BLEAdvertisedDevice advertisedDevice);
    
    // ===== ANOMALY DETECTION METHODS =====
    void performAnomalyDetection();
    void analyzeRSSIAnomalies(BLEDeviceInfo& device);
    void analyzeMACRandomization(BLEDeviceInfo& device);
    void analyzeTimingAnomalies(BLEDeviceInfo& device);
    void analyzeEntropyPattern(BLEDeviceInfo& device);
    float calculateEntropy(const std::vector<uint8_t>& data);
    float calculateMACEntropy(const String& macAddress);
    void detectSignalSpoofing();
    void addAnomalyEvent(const String& macAddress, AnomalyType type, 
                        const String& description, float severity);
    
    // ===== DEVICE LABELING METHODS =====
    void loadDeviceLabels();
    void saveDeviceLabels();
    void labelDevice(const String& macAddress, const String& label);
    void removeLabelFromDevice(const String& macAddress);
    String generateAutoLabel(const BLEDeviceInfo& device);
    
    // ===== DATA LOGGING METHODS =====
    void initializeLogging();
    void logScanEvent(const BLEDeviceInfo& device, const String& event);
    void logAnomalyEvent(const AnomalyEvent& event);
    void exportLogData(const String& format); // JSON or CSV
    void rotateLogs();
    String formatLogEntry(const BLEDeviceInfo& device, const String& event);
    
    // ===== STATISTICS METHODS =====
    void updateStatistics();
    void resetStatistics();
    String generateStatsReport();
    
    // ===== UI RENDERING METHODS =====
    void renderDeviceList();
    void renderDeviceDetails();
    void renderAnomalyAlerts();
    void renderStatistics();
    void renderLabelingInterface();
    void renderLogView();
    void renderHeader();
    void renderStatusBar();
    void renderScrollbar();
    
    // ===== UI HELPER METHODS =====
    void drawDeviceEntry(int y, const BLEDeviceInfo& device, bool selected);
    void drawAnomalyIcon(int x, int y, uint32_t anomalies);
    void drawSignalStrengthIcon(int x, int y, int8_t rssi);
    void drawLabelIcon(int x, int y, bool labeled);
    uint16_t getDeviceColor(const BLEDeviceInfo& device);
    String formatRSSI(int8_t rssi);
    String formatTime(unsigned long timestamp);
    String formatDuration(unsigned long duration);
    
    // ===== TOUCH HANDLING METHODS =====
    BLEScanner_TouchZone identifyTouchZone(TouchPoint touch);
    void handleDeviceListTouch(TouchPoint touch);
    void handleDeviceDetailsTouch(TouchPoint touch);
    void handleLabelingTouch(TouchPoint touch);
    void showLabelingDialog(int deviceIndex);
    void showDeviceContextMenu(int deviceIndex);
    
    // ===== CONFIGURATION METHODS =====
    void loadConfiguration();
    void saveConfiguration();
    void resetConfiguration();
    
    // ===== UTILITY METHODS =====
    void cleanupOldDevices();
    void sortDevicesByRSSI();
    void sortDevicesByTime();
    bool isValidMACAddress(const String& mac);
    String sanitizeDeviceName(const String& name);
    int findDeviceIndex(const String& macAddress);
    
public:
    BLEScanner();
    virtual ~BLEScanner();
    
    // ===== BaseApp INTERFACE IMPLEMENTATION =====
    bool initialize() override;
    void update() override;
    void render() override;
    bool handleTouch(TouchPoint touch) override;
    void cleanup() override;
    String getName() const override;
    const uint8_t* getIcon() const override;
    
    // ===== BaseApp OPTIONAL OVERRIDES =====
    void onPause() override;
    void onResume() override;
    bool saveState() override;
    bool loadState() override;
    bool handleMessage(AppMessage message, void* data = nullptr) override;
    
    // ===== PUBLIC INTERFACE =====
    void toggleScanning();
    void clearDeviceList();
    void exportDeviceData();
    uint32_t getDeviceCount() const { return deviceCount; }
    uint32_t getAnomalyCount() const { return anomalyEvents.size(); }
    ScanStatistics getStatistics() const { return stats; }
    
    // ===== SETTINGS INTERFACE =====
    uint8_t getSettingsCount() const override { return 8; }
    String getSettingName(uint8_t index) const override;
    void handleSetting(uint8_t index) override;
};

// BLE scan callback class
class BLEScanCallback : public BLEAdvertisedDeviceCallbacks {
private:
    BLEScanner* scanner;
    
public:
    BLEScanCallback(BLEScanner* s) : scanner(s) {}
    void onResult(BLEAdvertisedDevice advertisedDevice) override;
};

// Constants for UI layout
#define DEVICE_LIST_ITEM_HEIGHT     24
#define DEVICE_LIST_MAX_VISIBLE     8
#define BLESCANNER_HEADER_HEIGHT              20
#define BLESCANNER_STATUS_BAR_HEIGHT          16
#define BLESCANNER_SCROLL_BAR_WIDTH           8
#define BLESCANNER_ICON_SIZE                  12
#define BLESCANNER_MARGIN                     4

// File paths
#define BLE_SCANNER_DATA_DIR       "/data/blescanner"
#define BLE_DEVICE_LABELS_FILE     "/data/blescanner/labels.json"
#define BLE_SCAN_LOG_FILE          "/logs/ble_scan.log"
#define BLE_ANOMALY_LOG_FILE       "/logs/ble_anomalies.log"
#define BLE_CONFIG_FILE            "/settings/blescanner.cfg"

// Icon data (16x16 pixels, 1-bit per pixel)
extern const uint8_t ble_scanner_icon[32];

#endif // BLE_SCANNER_H