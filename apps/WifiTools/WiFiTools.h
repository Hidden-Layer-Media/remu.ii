#ifndef WIFI_TOOLS_H
#define WIFI_TOOLS_H

#include "../../core/AppManager/BaseApp.h"
#include "../../core/SystemCore/SystemCore.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>

// Debug logging macro
#ifndef debugLog
#define debugLog(msg) Serial.println(msg)
#endif

// ========================================
// WiFiTools - Network hacking utilities for remu.ii
// SSID scanning, deauth attacks, fake AP creation, packet monitoring
// ========================================

// Maximum networks to track
#define MAX_NETWORKS 50
#define MAX_FAKE_SSIDS 20
#define MAX_CLIENTS 20

// WiFi Tool modes
enum WiFiMode {
    WIFI_SCANNER,       // Network scanning
    WIFI_DEAUTH,        // Deauthentication attacks
    WIFI_BEACON_SPAM,   // Fake SSID beacon spam
    WIFI_MONITOR,       // Packet monitoring
    WIFI_AP_CLONE,      // Evil twin AP
    WIFI_HANDSHAKE      // Handshake capture
};

// Network security types
enum SecurityType {
    SECURITY_OPEN,
    SECURITY_WEP,
    SECURITY_WPA,
    SECURITY_WPA2,
    SECURITY_WPA3,
    SECURITY_UNKNOWN
};

// Network information structure
struct NetworkInfo {
    String ssid;
    String bssid;
    int32_t rssi;
    uint8_t channel;
    SecurityType security;
    bool hidden;
    bool selected;
    uint8_t clientCount;
    unsigned long lastSeen;
};

// Client device information
struct ClientInfo {
    String mac;
    String associatedBSSID;
    int32_t rssi;
    unsigned long lastSeen;
    bool isDeauthed;
};

// Attack configuration
struct AttackConfig {
    bool enabled;
    uint8_t targetChannel;
    String targetBSSID;
    String targetSSID;
    uint16_t packetsPerSecond;
    unsigned long duration; // 0 = infinite
    unsigned long startTime;
};

// UI state for WiFi tools
struct WiFiToolsUI {
    WiFiMode currentMode;
    uint8_t selectedNetwork;
    uint8_t selectedClient;
    uint8_t scrollOffset;
    bool scanActive;
    bool attackActive;
    unsigned long lastScanTime;
    unsigned long lastUIUpdate;
    uint16_t packetsSent;
    uint16_t packetsReceived;
    
    // Display settings
    bool showDetails;
    bool showChannels;
    bool sortByRSSI;
    uint8_t channelFilter; // 0 = all channels
};

class WiFiToolsApp : public BaseApp {
private:
    // Network data
    NetworkInfo networks[MAX_NETWORKS];
    ClientInfo clients[MAX_CLIENTS];
    String fakeSSIDs[MAX_FAKE_SSIDS];
    uint8_t networkCount;
    uint8_t clientCount;
    uint8_t fakeSSIDCount;
    
    // Attack state
    AttackConfig currentAttack;
    
    // UI state
    WiFiToolsUI ui;
    
    // WiFi management
    bool wifiInitialized;
    bool monitorModeActive;
    wifi_promiscuous_pkt_t* currentPacket;
    
    // Private methods - WiFi Management
    bool initializeWiFi();
    void shutdownWiFi();
    bool enableMonitorMode();
    void disableMonitorMode();
    void setChannel(uint8_t channel);
    
    // Private methods - Scanning
    void startScan();
    void updateScanResults();
    void processScanResult(int networkIndex);
    void scanForClients();
    SecurityType getSecurityType(wifi_auth_mode_t authMode);
    String formatBSSID(uint8_t* bssid);
    
    // Private methods - Attacks
    bool startDeauthAttack(String targetBSSID, String clientMAC = "");
    void stopAttack();
    void sendDeauthPacket(uint8_t* targetMAC, uint8_t* sourceMAC, uint8_t* bssid);
    bool startBeaconSpam();
    void sendFakeBeacon(String ssid, uint8_t channel);
    bool startEvilTwin(String targetSSID);
    
    // Private methods - Packet Analysis
    static void packetHandler(void* buf, wifi_promiscuous_pkt_type_t type);
    void processPacket(wifi_promiscuous_pkt_t* packet);
    void extractClientInfo(wifi_promiscuous_pkt_t* packet);
    void logPacket(String packetType, String details);
    
    // Private methods - UI Rendering
    void drawScannerMode();
    void drawDeauthMode();
    void drawBeaconSpamMode();
    void drawMonitorMode();
    void drawEvilTwinMode();
    void drawHandshakeMode();
    void drawNetworkList();
    void drawNetworkDetails(uint8_t networkIndex);
    void drawClientList();
    void drawAttackStatus();
    void drawChannelGraph();
    void drawPacketStats();
    void drawCommonUI();
    
    // Private methods - Controls
    void handleScannerTouch(TouchPoint touch);
    void handleDeauthTouch(TouchPoint touch);
    void handleBeaconTouch(TouchPoint touch);
    void handleMonitorTouch(TouchPoint touch);
    void handleEvilTwinTouch(TouchPoint touch);
    void handleHandshakeTouch(TouchPoint touch);
    bool handleCommonTouch(TouchPoint touch);
    void toggleNetworkSelection(uint8_t networkIndex);
    void selectTarget(uint8_t networkIndex);
    
    // Private methods - Data Management
    void clearNetworks();
    void clearClients();
    void sortNetworks();
    void filterByChannel(uint8_t channel);
    void loadFakeSSIDs();
    void saveCapturedData();
    
    // Private methods - Security/Legal
    bool confirmAttack(String attackType);
    void showLegalWarning();
    void logActivity(String activity);
    
    // Utility methods
    String getRSSIBar(int32_t rssi);
    String getSecurityString(SecurityType security);
    String formatMAC(uint8_t* mac);
    uint16_t calculateChecksum(uint8_t* data, uint16_t length);
    bool isValidMAC(String mac);
    
    // Constants for UI layout
    static const int16_t NETWORK_LINE_HEIGHT = 16;
    static const int16_t MAX_VISIBLE_NETWORKS = 12;
    static const uint16_t SCAN_INTERVAL = 3000; // 3 seconds
    static const uint16_t UI_UPDATE_INTERVAL = 500; // 0.5 seconds

public:
    WiFiToolsApp();
    virtual ~WiFiToolsApp();
    
    // Mandatory BaseApp methods
    bool initialize() override;
    void update() override;
    void render() override;
    bool handleTouch(TouchPoint touch) override;
    void cleanup() override;
    String getName() const override { return "WiFiTools"; }
    const uint8_t* getIcon() const override;
    
    // Optional BaseApp methods
    void onPause() override;
    void onResume() override;
    bool saveState() override;
    bool loadState() override;
    bool handleMessage(AppMessage message) override;
    
    // Settings menu support
    uint8_t getSettingsCount() const override { return 8; }
    String getSettingName(uint8_t index) const override;
    void handleSetting(uint8_t index) override;
    
    // WiFi-specific methods
    void setMode(WiFiMode mode);
    WiFiMode getMode() const { return ui.currentMode; }
    
    bool isScanActive() const { return ui.scanActive; }
    bool isAttackActive() const { return ui.attackActive; }
    uint8_t getNetworkCount() const { return networkCount; }
    uint8_t getClientCount() const { return clientCount; }
    
    // Network operations
    void startNetworkScan();
    void stopNetworkScan();
    NetworkInfo getNetwork(uint8_t index) const;
    void selectNetwork(uint8_t index);
    void deselectAllNetworks();
    
    // Attack operations
    bool launchDeauthAttack(uint8_t networkIndex, String clientMAC = "");
    bool launchBeaconSpam(uint8_t channelStart = 1, uint8_t channelEnd = 13);
    bool launchEvilTwin(uint8_t networkIndex);
    void stopAllAttacks();
    
    // Monitor operations
    void startPacketMonitor(uint8_t channel = 0); // 0 = all channels
    void stopPacketMonitor();
    void captureHandshakes(bool enable);
    
    // Data export
    bool exportNetworkList(String filename);
    bool exportCapturedPackets(String filename);
    void clearCapturedData();
    
    // Debug methods
    void debugPrintNetworks();
    void debugPrintClients();
    void debugSendTestPacket();
    void debugInjectFrame(String frameType);
    
    // Icon data
    static const uint8_t WIFI_ICON[32];
    
    // Legal disclaimer flag
    static bool legalWarningShown;
};

// Global packet handler reference
extern WiFiToolsApp* g_wifiToolsInstance;

#endif // WIFI_TOOLS_H