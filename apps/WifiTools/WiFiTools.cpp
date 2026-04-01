#include "WiFiTools.h"

// ========================================
// GLOBAL VARIABLES AND DISCLAIMERS
// ========================================

// Global instance for packet handler callback
WiFiToolsApp* g_wifiToolsInstance = nullptr;

// Legal warning flag
bool WiFiToolsApp::legalWarningShown = false;

// WiFi icon data
const uint8_t WiFiToolsApp::WIFI_ICON[32] = {
    0x00, 0x00, 0x00, 0x00, 0x7F, 0xFE, 0x40, 0x02, 0x5F, 0xFA, 0x50, 0x0A,
    0x5F, 0xFA, 0x50, 0x0A, 0x5F, 0xFA, 0x40, 0x02, 0x7F, 0xFE, 0x00, 0x00,
    0x01, 0x80, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00
};

// ========================================
// CONSTRUCTOR / DESTRUCTOR
// ========================================

WiFiToolsApp::WiFiToolsApp() :
    networkCount(0),
    clientCount(0),
    fakeSSIDCount(0),
    wifiInitialized(false),
    monitorModeActive(false),
    currentPacket(nullptr)
{
    // Set app metadata
    metadata.name = "WiFiTools";
    metadata.version = "1.0";
    metadata.author = "remu.ii";
    metadata.description = "WiFi security analysis tools";
    metadata.category = CATEGORY_TOOLS;
    metadata.memoryRequirement = 20000; // 20KB for network data
    metadata.requiresSD = true;
    metadata.requiresWiFi = true;
    metadata.requiresBLE = false;
    
    // Set colors
    backgroundColor = COLOR_BLACK;
    foregroundColor = COLOR_GREEN_PHOS;
    
    showBackButton = true;
    showStatusBar = true;
    
    // Initialize UI state
    ui.currentMode = WIFI_SCANNER;
    ui.selectedNetwork = 0;
    ui.selectedClient = 0;
    ui.scrollOffset = 0;
    ui.scanActive = false;
    ui.attackActive = false;
    ui.lastScanTime = 0;
    ui.lastUIUpdate = 0;
    ui.packetsSent = 0;
    ui.packetsReceived = 0;
    ui.showDetails = false;
    ui.showChannels = false;
    ui.sortByRSSI = true;
    ui.channelFilter = 0;
    
    // Initialize attack config
    currentAttack.enabled = false;
    currentAttack.targetChannel = 1;
    currentAttack.packetsPerSecond = 10;
    currentAttack.duration = 0;
    currentAttack.startTime = 0;
    
    // Set global instance for callbacks
    g_wifiToolsInstance = this;
}

WiFiToolsApp::~WiFiToolsApp() {
    cleanup();
    g_wifiToolsInstance = nullptr;
}

// ========================================
// MANDATORY BASEAPP METHODS
// ========================================

bool WiFiToolsApp::initialize() {
    debugLog("WiFiTools initializing...");
    
    setState(APP_INITIALIZING);
    
    // Show legal warning first
    if (!legalWarningShown) {
        showLegalWarning();
        legalWarningShown = true;
    }
    
    // Create app data directory
    if (!createAppDataDir()) {
        debugLog("WARNING: Could not create app data directory");
    }
    
    // Initialize WiFi
    if (!initializeWiFi()) {
        debugLog("ERROR: WiFi initialization failed");
        return false;
    }
    
    // Clear data structures
    clearNetworks();
    clearClients();
    
    // Load fake SSID list
    loadFakeSSIDs();
    
    setState(APP_RUNNING);
    debugLog("WiFiTools initialized successfully");
    
    return true;
}

void WiFiToolsApp::update() {
    if (currentState != APP_RUNNING) return;
    
    unsigned long currentTime = millis();
    
    // Update scan results
    if (ui.scanActive && currentTime - ui.lastScanTime >= SCAN_INTERVAL) {
        updateScanResults();
        ui.lastScanTime = currentTime;
    }
    
    // Update attack status
    if (ui.attackActive) {
        // Check if attack duration expired
        if (currentAttack.duration > 0 && 
            currentTime - currentAttack.startTime >= currentAttack.duration) {
            stopAttack();
        }
    }
    
    // Update UI periodically
    if (currentTime - ui.lastUIUpdate >= UI_UPDATE_INTERVAL) {
        // Sort networks if needed
        if (ui.sortByRSSI) {
            sortNetworks();
        }
        ui.lastUIUpdate = currentTime;
    }
    
    frameCount++;
}

void WiFiToolsApp::render() {
    if (currentState != APP_RUNNING) return;
    
    // Clear screen
    displayManager.clearScreen(backgroundColor);
    
    // Draw mode-specific interface
    switch (ui.currentMode) {
        case WIFI_SCANNER:
            drawScannerMode();
            break;
        case WIFI_DEAUTH:
            drawDeauthMode();
            break;
        case WIFI_BEACON_SPAM:
            drawBeaconSpamMode();
            break;
        case WIFI_MONITOR:
            drawMonitorMode();
            break;
        case WIFI_AP_CLONE:
            // Similar to deauth mode for now
            drawDeauthMode();
            break;
        case WIFI_HANDSHAKE:
            drawMonitorMode();
            break;
    }
    
    // Draw common UI elements
    drawCommonUI();
}

bool WiFiToolsApp::handleTouch(TouchPoint touch) {
    // Handle common UI first
    if (handleCommonTouch(touch)) {
        return true;
    }
    
    if (!touch.isNewPress) return false;
    
    // Handle mode-specific touches
    switch (ui.currentMode) {
        case WIFI_SCANNER:
            handleScannerTouch(touch);
            break;
        case WIFI_DEAUTH:
            handleDeauthTouch(touch);
            break;
        case WIFI_BEACON_SPAM:
            handleBeaconTouch(touch);
            break;
        case WIFI_MONITOR:
            handleMonitorTouch(touch);
            break;
        default:
            break;
    }
    
    return true;
}

void WiFiToolsApp::cleanup() {
    // Stop any active attacks
    stopAllAttacks();
    
    // Shutdown WiFi
    shutdownWiFi();
    
    debugLog("WiFiTools cleanup complete");
}

const uint8_t* WiFiToolsApp::getIcon() const {
    return WIFI_ICON;
}

// ========================================
// OPTIONAL BASEAPP METHODS
// ========================================

void WiFiToolsApp::onPause() {
    // Stop active operations when pausing
    stopAllAttacks();
    ui.scanActive = false;
}

void WiFiToolsApp::onResume() {
    // Restart scanning if it was active
    if (ui.currentMode == WIFI_SCANNER) {
        startNetworkScan();
    }
}

String WiFiToolsApp::getSettingName(uint8_t index) const {
    switch (index) {
        case 0: return "Scanner Mode";
        case 1: return "Deauth Mode";
        case 2: return "Beacon Spam";
        case 3: return "Monitor Mode";
        case 4: return "Export Data";
        case 5: return "Clear Data";
        default: return "";
    }
}

void WiFiToolsApp::handleSetting(uint8_t index) {
    switch (index) {
        case 0: // Scanner Mode
            setMode(WIFI_SCANNER);
            break;
        case 1: // Deauth Mode
            setMode(WIFI_DEAUTH);
            break;
        case 2: // Beacon Spam
            setMode(WIFI_BEACON_SPAM);
            break;
        case 3: // Monitor Mode
            setMode(WIFI_MONITOR);
            break;
        case 4: // Export Data
            exportNetworkList("networks.json");
            break;
        case 5: // Clear Data
            clearCapturedData();
            break;
    }
}

// ========================================
// WIFI MANAGEMENT
// ========================================

bool WiFiToolsApp::initializeWiFi() {
    debugLog("Initializing WiFi subsystem...");
    
    // Initialize WiFi in station mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    // Initialize ESP32 WiFi stack
    esp_wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t result = esp_wifi_init(&cfg);
    
    if (result != ESP_OK) {
        debugLog("ESP WiFi init failed: " + String(result));
        return false;
    }
    
    wifiInitialized = true;
    debugLog("WiFi initialized successfully");
    return true;
}

void WiFiToolsApp::shutdownWiFi() {
    if (!wifiInitialized) return;
    
    // Disable monitor mode if active
    disableMonitorMode();
    
    // Stop WiFi
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    
    // Deinitialize ESP WiFi
    esp_wifi_deinit();
    
    wifiInitialized = false;
    debugLog("WiFi shutdown complete");
}

bool WiFiToolsApp::enableMonitorMode() {
    if (!wifiInitialized || monitorModeActive) return false;
    
    debugLog("Enabling monitor mode...");
    
    // Set promiscuous mode
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&packetHandler);
    
    monitorModeActive = true;
    debugLog("Monitor mode enabled");
    return true;
}

void WiFiToolsApp::disableMonitorMode() {
    if (!monitorModeActive) return;
    
    debugLog("Disabling monitor mode...");
    
    // Disable promiscuous mode
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    
    monitorModeActive = false;
    debugLog("Monitor mode disabled");
}

void WiFiToolsApp::setChannel(uint8_t channel) {
    if (!wifiInitialized || channel < 1 || channel > 13) return;
    
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    debugLog("Set WiFi channel: " + String(channel));
}

// ========================================
// SCANNING FUNCTIONALITY
// ========================================

void WiFiToolsApp::startScan() {
    if (!wifiInitialized) return;
    
    debugLog("Starting WiFi scan...");
    
    // Configure scan parameters
    wifi_scan_config_t scanConfig;
    memset(&scanConfig, 0, sizeof(scanConfig));
    scanConfig.ssid = nullptr;
    scanConfig.bssid = nullptr;
    scanConfig.channel = 0; // All channels
    scanConfig.show_hidden = true;
    scanConfig.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scanConfig.scan_time.active.min = 100;
    scanConfig.scan_time.active.max = 300;
    
    // Start asynchronous scan
    esp_wifi_scan_start(&scanConfig, false);
    
    ui.scanActive = true;
    ui.lastScanTime = millis();
}

void WiFiToolsApp::updateScanResults() {
    if (!ui.scanActive) return;
    
    uint16_t networkCount;
    esp_wifi_scan_get_ap_num(&networkCount);
    
    if (networkCount == 0) {
        debugLog("No networks found");
        return;
    }
    
    // Limit to maximum networks we can handle
    networkCount = min(networkCount, (uint16_t)MAX_NETWORKS);
    
    wifi_ap_record_t* apRecords = new wifi_ap_record_t[networkCount];
    esp_wifi_scan_get_ap_records(&networkCount, apRecords);
    
    // Clear existing networks
    this->networkCount = 0;
    
    // Process scan results
    for (uint16_t i = 0; i < networkCount && this->networkCount < MAX_NETWORKS; i++) {
        NetworkInfo& network = networks[this->networkCount];
        
        // Extract network information
        network.ssid = String((char*)apRecords[i].ssid);
        if (network.ssid.length() == 0) {
            network.ssid = "<Hidden>";
            network.hidden = true;
        } else {
            network.hidden = false;
        }
        
        network.bssid = formatBSSID(apRecords[i].bssid);
        network.rssi = apRecords[i].rssi;
        network.channel = apRecords[i].primary;
        network.security = getSecurityType(apRecords[i].authmode);
        network.selected = false;
        network.clientCount = 0; // Will be updated by client scan
        network.lastSeen = millis();
        
        this->networkCount++;
    }
    
    delete[] apRecords;
    
    debugLog("Found " + String(this->networkCount) + " networks");
    
    // Sort networks by RSSI if enabled
    if (ui.sortByRSSI) {
        sortNetworks();
    }
}

SecurityType WiFiToolsApp::getSecurityType(wifi_auth_mode_t authMode) {
    switch (authMode) {
        case WIFI_AUTH_OPEN:
            return SECURITY_OPEN;
        case WIFI_AUTH_WEP:
            return SECURITY_WEP;
        case WIFI_AUTH_WPA_PSK:
            return SECURITY_WPA;
        case WIFI_AUTH_WPA2_PSK:
            return SECURITY_WPA2;
        case WIFI_AUTH_WPA_WPA2_PSK:
            return SECURITY_WPA2;
        case WIFI_AUTH_WPA3_PSK:
            return SECURITY_WPA3;
        default:
            return SECURITY_UNKNOWN;
    }
}

String WiFiToolsApp::formatBSSID(uint8_t* bssid) {
    char bssidStr[18];
    sprintf(bssidStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    return String(bssidStr);
}

// ========================================
// UI RENDERING METHODS
// ========================================

void WiFiToolsApp::drawScannerMode() {
    // Draw header
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawText(5, 5, "WiFi Scanner", COLOR_RED_GLOW);
    
    // Draw scan status
    displayManager.setFont(FONT_SMALL);
    String status = ui.scanActive ? "Scanning..." : "Stopped";
    displayManager.drawText(150, 8, status, ui.scanActive ? COLOR_GREEN_PHOS : COLOR_LIGHT_GRAY);
    
    // Draw network count
    String countText = "Networks: " + String(networkCount);
    displayManager.drawText(220, 8, countText, COLOR_WHITE);
    
    // Draw network list
    drawNetworkList();
    
    // Draw controls
    displayManager.drawButton(5, 200, 50, 16, ui.scanActive ? "Stop" : "Scan");
    displayManager.drawButton(60, 200, 50, 16, "Details");
    displayManager.drawButton(115, 200, 50, 16, "Export");
    
    // Draw mode selector
    displayManager.drawButton(250, 200, 60, 16, "Mode");
}

void WiFiToolsApp::drawNetworkList() {
    if (networkCount == 0) {
        displayManager.setFont(FONT_SMALL);
        displayManager.drawTextCentered(0, 100, SCREEN_WIDTH, "No networks found", COLOR_LIGHT_GRAY);
        displayManager.drawTextCentered(0, 120, SCREEN_WIDTH, "Touch 'Scan' to search", COLOR_LIGHT_GRAY);
        return;
    }
    
    int16_t listY = 25;
    int16_t maxVisible = min(MAX_VISIBLE_NETWORKS, networkCount - ui.scrollOffset);
    
    displayManager.setFont(FONT_SMALL);
    
    for (int16_t i = 0; i < maxVisible; i++) {
        uint8_t networkIndex = ui.scrollOffset + i;
        if (networkIndex >= networkCount) break;
        
        NetworkInfo& network = networks[networkIndex];
        int16_t y = listY + i * NETWORK_LINE_HEIGHT;
        
        // Highlight selected network
        uint16_t textColor = COLOR_WHITE;
        if (networkIndex == ui.selectedNetwork) {
            displayManager.drawRetroRect(0, y - 1, SCREEN_WIDTH, NETWORK_LINE_HEIGHT, 
                                       COLOR_DARK_GRAY, true);
            textColor = COLOR_GREEN_PHOS;
        }
        
        // Network selection indicator
        if (network.selected) {
            displayManager.drawText(2, y, "*", COLOR_RED_GLOW);
        }
        
        // SSID (truncated if too long)
        String ssid = network.ssid;
        if (ssid.length() > 20) {
            ssid = ssid.substring(0, 17) + "...";
        }
        displayManager.drawText(10, y, ssid, textColor);
        
        // Channel
        displayManager.drawText(160, y, String(network.channel), COLOR_LIGHT_GRAY);
        
        // RSSI bar
        String rssiBar = getRSSIBar(network.rssi);
        displayManager.drawText(180, y, rssiBar, COLOR_GREEN_PHOS);
        
        // Security indicator
        String securityStr = getSecurityString(network.security);
        uint16_t securityColor = (network.security == SECURITY_OPEN) ? COLOR_RED_GLOW : COLOR_WHITE;
        displayManager.drawText(220, y, securityStr, securityColor);
        
        // Client count (if known)
        if (network.clientCount > 0) {
            displayManager.drawText(280, y, String(network.clientCount), COLOR_PURPLE_GLOW);
        }
    }
    
    // Draw scrollbar if needed
    if (networkCount > MAX_VISIBLE_NETWORKS) {
        int16_t scrollbarHeight = (MAX_VISIBLE_NETWORKS * NETWORK_LINE_HEIGHT * MAX_VISIBLE_NETWORKS) / networkCount;
        int16_t scrollbarY = listY + (ui.scrollOffset * NETWORK_LINE_HEIGHT * MAX_VISIBLE_NETWORKS) / networkCount;
        
        displayManager.drawScrollbar(SCREEN_WIDTH - 10, listY, 
                                    MAX_VISIBLE_NETWORKS * NETWORK_LINE_HEIGHT,
                                    (ui.scrollOffset * 100) / (networkCount - MAX_VISIBLE_NETWORKS),
                                    (MAX_VISIBLE_NETWORKS * 100) / networkCount);
    }
}

void WiFiToolsApp::drawDeauthMode() {
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawText(5, 5, "Deauth Attack", COLOR_RED_GLOW);
    
    // Warning message
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(5, 25, "WARNING: For authorized testing only!", COLOR_RED_GLOW);
    
    // Target information
    if (ui.selectedNetwork < networkCount) {
        NetworkInfo& target = networks[ui.selectedNetwork];
        displayManager.drawText(5, 45, "Target: " + target.ssid, COLOR_WHITE);
        displayManager.drawText(5, 60, "BSSID: " + target.bssid, COLOR_LIGHT_GRAY);
        displayManager.drawText(5, 75, "Channel: " + String(target.channel), COLOR_LIGHT_GRAY);
    }
    
    // Attack status
    if (ui.attackActive) {
        displayManager.drawText(5, 95, "Attack Status: ACTIVE", COLOR_RED_GLOW);
        displayManager.drawText(5, 110, "Packets Sent: " + String(ui.packetsSent), COLOR_GREEN_PHOS);
        
        // Attack duration
        unsigned long elapsed = (millis() - currentAttack.startTime) / 1000;
        displayManager.drawText(5, 125, "Duration: " + String(elapsed) + "s", COLOR_WHITE);
    } else {
        displayManager.drawText(5, 95, "Attack Status: STOPPED", COLOR_LIGHT_GRAY);
    }
    
    // Controls
    displayManager.drawButton(5, 150, 60, 16, ui.attackActive ? "Stop" : "Start");
    displayManager.drawButton(70, 150, 60, 16, "Select Target");
    displayManager.drawButton(135, 150, 60, 16, "All Clients");
    
    // Legal reminder
    displayManager.setFont(FONT_SMALL);
    displayManager.drawTextCentered(0, 200, SCREEN_WIDTH, "Use responsibly - Educational only", COLOR_LIGHT_GRAY);
}

void WiFiToolsApp::drawBeaconSpamMode() {
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawText(5, 5, "Beacon Spam", COLOR_RED_GLOW);
    
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(5, 25, "Fake SSID Broadcasting", COLOR_WHITE);
    
    // Show loaded fake SSIDs
    displayManager.drawText(5, 45, "Loaded SSIDs: " + String(fakeSSIDCount), COLOR_GREEN_PHOS);
    
    // Attack status
    if (ui.attackActive) {
        displayManager.drawText(5, 65, "Status: Broadcasting", COLOR_RED_GLOW);
        displayManager.drawText(5, 80, "Beacons Sent: " + String(ui.packetsSent), COLOR_GREEN_PHOS);
    } else {
        displayManager.drawText(5, 65, "Status: Stopped", COLOR_LIGHT_GRAY);
    }
    
    // Controls
    displayManager.drawButton(5, 150, 60, 16, ui.attackActive ? "Stop" : "Start");
    displayManager.drawButton(70, 150, 60, 16, "Load SSIDs");
    displayManager.drawButton(135, 150, 60, 16, "Random");
}

void WiFiToolsApp::drawMonitorMode() {
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawText(5, 5, "Packet Monitor", COLOR_RED_GLOW);
    
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(5, 25, "Channel: " + String(currentAttack.targetChannel), COLOR_WHITE);
    
    // Packet statistics
    displayManager.drawText(5, 45, "Packets Received: " + String(ui.packetsReceived), COLOR_GREEN_PHOS);
    displayManager.drawText(5, 60, "Clients Found: " + String(clientCount), COLOR_WHITE);
    
    // Monitor status
    String status = monitorModeActive ? "MONITORING" : "STOPPED";
    uint16_t statusColor = monitorModeActive ? COLOR_GREEN_PHOS : COLOR_LIGHT_GRAY;
    displayManager.drawText(5, 75, "Status: " + status, statusColor);
    
    // Controls
    displayManager.drawButton(5, 150, 60, 16, monitorModeActive ? "Stop" : "Start");
    displayManager.drawButton(70, 150, 60, 16, "Channel+");
    displayManager.drawButton(135, 150, 60, 16, "Export");
}

// ========================================
// TOUCH HANDLING
// ========================================

void WiFiToolsApp::handleScannerTouch(TouchPoint touch) {
    // Scan/Stop button
    if (touchInterface.isPointInRect(touch, 5, 200, 50, 16)) {
        if (ui.scanActive) {
            ui.scanActive = false;
            debugLog("Scan stopped");
        } else {
            startNetworkScan();
        }
        return;
    }
    
    // Mode button
    if (touchInterface.isPointInRect(touch, 250, 200, 60, 16)) {
        // Cycle through modes
        uint8_t nextMode = (ui.currentMode + 1) % 4; // 4 main modes
        setMode((WiFiMode)nextMode);
        return;
    }
    
    // Network list selection
    if (touch.y >= 25 && touch.y < 200) {
        uint8_t selectedIndex = ui.scrollOffset + (touch.y - 25) / NETWORK_LINE_HEIGHT;
        if (selectedIndex < networkCount) {
            ui.selectedNetwork = selectedIndex;
            
            // Double-tap to select/deselect
            static unsigned long lastTapTime = 0;
            static uint8_t lastTappedNetwork = 255;
            
            if (millis() - lastTapTime < 500 && selectedIndex == lastTappedNetwork) {
                toggleNetworkSelection(selectedIndex);
            }
            
            lastTapTime = millis();
            lastTappedNetwork = selectedIndex;
        }
    }
}

void WiFiToolsApp::handleDeauthTouch(TouchPoint touch) {
    // Start/Stop attack button
    if (touchInterface.isPointInRect(touch, 5, 150, 60, 16)) {
        if (ui.attackActive) {
            stopAttack();
        } else {
            if (confirmAttack("Deauthentication")) {
                launchDeauthAttack(ui.selectedNetwork);
            }
        }
        return;
    }
    
    // Select target button
    if (touchInterface.isPointInRect(touch, 70, 150, 60, 16)) {
        setMode(WIFI_SCANNER);
        return;
    }
}

void WiFiToolsApp::handleBeaconTouch(TouchPoint touch) {
    // Start/Stop beacon spam
    if (touchInterface.isPointInRect(touch, 5, 150, 60, 16)) {
        if (ui.attackActive) {
            stopAttack();
        } else {
            if (confirmAttack("Beacon Spam")) {
                launchBeaconSpam();
            }
        }
        return;
    }
}

void WiFiToolsApp::handleMonitorTouch(TouchPoint touch) {
    // Start/Stop monitor
    if (touchInterface.isPointInRect(touch, 5, 150, 60, 16)) {
        if (monitorModeActive) {
            stopPacketMonitor();
        } else {
            startPacketMonitor();
        }
        return;
    }
    
    // Channel+ button
    if (touchInterface.isPointInRect(touch, 70, 150, 60, 16)) {
        currentAttack.targetChannel = (currentAttack.targetChannel % 13) + 1;
        setChannel(currentAttack.targetChannel);
        return;
    }
}

// ========================================
// ATTACK IMPLEMENTATIONS
// ========================================

bool WiFiToolsApp::launchDeauthAttack(uint8_t networkIndex, String clientMAC) {
    if (networkIndex >= networkCount || !wifiInitialized) return false;
    
    NetworkInfo& target = networks[networkIndex];
    
    debugLog("Starting deauth attack on: " + target.ssid);
    
    // Configure attack
    currentAttack.enabled = true;
    currentAttack.targetBSSID = target.bssid;
    currentAttack.targetSSID = target.ssid;
    currentAttack.targetChannel = target.channel;
    currentAttack.startTime = millis();
    
    // Set channel
    setChannel(target.channel);
    
    // Enable monitor mode for packet injection
    enableMonitorMode();
    
    ui.attackActive = true;
    ui.packetsSent = 0;
    
    logActivity("Deauth attack started: " + target.ssid);
    
    return true;
}

bool WiFiToolsApp::launchBeaconSpam(uint8_t channelStart, uint8_t channelEnd) {
    if (fakeSSIDCount == 0) {
        debugLog("No fake SSIDs loaded");
        return false;
    }
    
    debugLog("Starting beacon spam attack");
    
    currentAttack.enabled = true;
    currentAttack.startTime = millis();
    
    ui.attackActive = true;
    ui.packetsSent = 0;
    
    logActivity("Beacon spam attack started");
    
    return true;
}

void WiFiToolsApp::stopAttack() {
    if (!ui.attackActive) return;
    
    debugLog("Stopping attack");
    
    currentAttack.enabled = false;
    ui.attackActive = false;
    
    // Disable monitor mode
    disableMonitorMode();
    
    logActivity("Attack stopped");
}

void WiFiToolsApp::startPacketMonitor(uint8_t channel) {
    if (monitorModeActive) return;
    
    debugLog("Starting packet monitor");
    
    if (channel == 0) {
        channel = currentAttack.targetChannel;
    }
    
    setChannel(channel);
    enableMonitorMode();
    
    ui.packetsReceived = 0;
    
    logActivity("Packet monitor started on channel " + String(channel));
}

void WiFiToolsApp::stopPacketMonitor() {
    if (!monitorModeActive) return;
    
    debugLog("Stopping packet monitor");
    
    disableMonitorMode();
    
    logActivity("Packet monitor stopped");
}

// ========================================
// PACKET HANDLING
// ========================================

void WiFiToolsApp::packetHandler(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!g_wifiToolsInstance) return;
    
    wifi_promiscuous_pkt_t* packet = (wifi_promiscuous_pkt_t*)buf;
    g_wifiToolsInstance->processPacket(packet);
}

void WiFiToolsApp::processPacket(wifi_promiscuous_pkt_t* packet) {
    if (!packet) return;
    
    ui.packetsReceived++;
    
    // Basic packet analysis
    wifi_pkt_rx_ctrl_t* ctrl = &packet->rx_ctrl;
    uint8_t* payload = packet->payload;
    
    // Extract frame control
    uint16_t frameControl = (payload[1] << 8) | payload[0];
    uint8_t frameType = (frameControl & 0x0C) >> 2;
    uint8_t frameSubtype = (frameControl & 0xF0) >> 4;
    
    // Process different frame types
    switch (frameType) {
        case 0: // Management frames
            if (frameSubtype == 4 || frameSubtype == 5) { // Probe request/response
                extractClientInfo(packet);
            }
            break;
        case 1: // Control frames
            break;
        case 2: // Data frames
            extractClientInfo(packet);
            break;
    }
    
    // Log interesting packets
    if (frameType == 0 && (frameSubtype == 12 || frameSubtype == 0)) { // Deauth or assoc
        String packetInfo = "Frame: " + String(frameType) + "." + String(frameSubtype) + 
                           " RSSI: " + String(ctrl->rssi);
        logPacket("MGMT", packetInfo);
    }
}

void WiFiToolsApp::extractClientInfo(wifi_promiscuous_pkt_t* packet) {
    if (clientCount >= MAX_CLIENTS) return;
    
    uint8_t* payload = packet->payload;
    
    // Extract MAC addresses from frame
    String sourceMac = formatMAC(&payload[10]);
    String destMac = formatMAC(&payload[4]);
    String bssid = formatMAC(&payload[16]);
    
    // Check if this is a new client
    bool found = false;
    for (uint8_t i = 0; i < clientCount; i++) {
        if (clients[i].mac == sourceMac) {
            clients[i].lastSeen = millis();
            clients[i].rssi = packet->rx_ctrl.rssi;
            found = true;
            break;
        }
    }
    
    // Add new client
    if (!found) {
        ClientInfo& client = clients[clientCount];
        client.mac = sourceMac;
        client.associatedBSSID = bssid;
        client.rssi = packet->rx_ctrl.rssi;
        client.lastSeen = millis();
        client.isDeauthed = false;
        
        clientCount++;
        debugLog("New client found: " + sourceMac);
    }
}

// ========================================
// UTILITY METHODS
// ========================================

void WiFiToolsApp::setMode(WiFiMode mode) {
    // Stop current operations
    stopAllAttacks();
    
    ui.currentMode = mode;
    
    // Mode-specific initialization
    switch (mode) {
        case WIFI_SCANNER:
            startNetworkScan();
            break;
        case WIFI_MONITOR:
            // Will be started when user presses start
            break;
        default:
            break;
    }
    
    debugLog("Mode changed to: " + String(mode));
}

void WiFiToolsApp::startNetworkScan() {
    if (ui.scanActive) return;
    
    startScan();
    debugLog("Network scan started");
}

void WiFiToolsApp::stopNetworkScan() {
    ui.scanActive = false;
    debugLog("Network scan stopped");
}

void WiFiToolsApp::stopAllAttacks() {
    stopAttack();
    stopPacketMonitor();
}

void WiFiToolsApp::clearNetworks() {
    networkCount = 0;
    ui.selectedNetwork = 0;
    ui.scrollOffset = 0;
}

void WiFiToolsApp::clearClients() {
    clientCount = 0;
    ui.selectedClient = 0;
}

void WiFiToolsApp::sortNetworks() {
    if (networkCount <= 1) return;
    
    // Simple bubble sort by RSSI (stronger signal first)
    for (uint8_t i = 0; i < networkCount - 1; i++) {
        for (uint8_t j = 0; j < networkCount - i - 1; j++) {
            if (networks[j].rssi < networks[j + 1].rssi) {
                // Swap networks
                NetworkInfo temp = networks[j];
                networks[j] = networks[j + 1];
                networks[j + 1] = temp;
            }
        }
    }
}

void WiFiToolsApp::loadFakeSSIDs() {
    // Load fake SSID list from file or use built-in list
    fakeSSIDCount = 0;
    
    // Built-in fake SSIDs for demonstration
    String builtinSSIDs[] = {
        "FREE_WiFi",
        "McDonald's WiFi",
        "Starbucks",
        "Airport_WiFi",
        "Hotel_Guest",
        "Conference_WiFi",
        "Library_Internet",
        "Mall_WiFi"
    };
    
    for (uint8_t i = 0; i < 8 && i < MAX_FAKE_SSIDS; i++) {
        fakeSSIDs[fakeSSIDCount++] = builtinSSIDs[i];
    }
    
    debugLog("Loaded " + String(fakeSSIDCount) + " fake SSIDs");
}

String WiFiToolsApp::getRSSIBar(int32_t rssi) {
    // Convert RSSI to visual bar
    if (rssi >= -30) return "████";
    if (rssi >= -50) return "███░";
    if (rssi >= -70) return "██░░";
    if (rssi >= -90) return "█░░░";
    return "░░░░";
}

String WiFiToolsApp::getSecurityString(SecurityType security) {
    switch (security) {
        case SECURITY_OPEN: return "OPEN";
        case SECURITY_WEP: return "WEP";
        case SECURITY_WPA: return "WPA";
        case SECURITY_WPA2: return "WPA2";
        case SECURITY_WPA3: return "WPA3";
        default: return "UNK";
    }
}

String WiFiToolsApp::formatMAC(uint8_t* mac) {
    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

void WiFiToolsApp::toggleNetworkSelection(uint8_t networkIndex) {
    if (networkIndex >= networkCount) return;
    
    networks[networkIndex].selected = !networks[networkIndex].selected;
    debugLog("Network " + networks[networkIndex].ssid + " " + 
             (networks[networkIndex].selected ? "selected" : "deselected"));
}

bool WiFiToolsApp::confirmAttack(String attackType) {
    // In a real implementation, this would show a confirmation dialog
    // For now, just log the confirmation
    debugLog("Attack confirmed: " + attackType);
    logActivity("User confirmed " + attackType + " attack");
    return true;
}

void WiFiToolsApp::showLegalWarning() {
    // Display legal warning on screen
    displayManager.clearScreen(COLOR_BLACK);
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawTextCentered(0, 20, SCREEN_WIDTH, "LEGAL WARNING", COLOR_RED_GLOW);
    
    displayManager.setFont(FONT_SMALL);
    displayManager.drawTextCentered(0, 50, SCREEN_WIDTH, "This tool is for", COLOR_WHITE);
    displayManager.drawTextCentered(0, 70, SCREEN_WIDTH, "AUTHORIZED TESTING", COLOR_RED_GLOW);
    displayManager.drawTextCentered(0, 90, SCREEN_WIDTH, "and educational", COLOR_WHITE);
    displayManager.drawTextCentered(0, 110, SCREEN_WIDTH, "purposes only.", COLOR_WHITE);
    
    displayManager.drawTextCentered(0, 140, SCREEN_WIDTH, "Unauthorized use is", COLOR_WHITE);
    displayManager.drawTextCentered(0, 160, SCREEN_WIDTH, "ILLEGAL and may", COLOR_RED_GLOW);
    displayManager.drawTextCentered(0, 180, SCREEN_WIDTH, "violate local laws.", COLOR_RED_GLOW);
    
    displayManager.drawTextCentered(0, 210, SCREEN_WIDTH, "Touch to continue", COLOR_LIGHT_GRAY);
    
    // Wait for touch to continue
    while (true) {
        touchInterface.update();
        TouchPoint touch = touchInterface.getCurrentTouch();
        if (touch.isNewPress) break;
        delay(10);
    }
}

void WiFiToolsApp::logActivity(String activity) {
    // Log security testing activities
    debugLog("[ACTIVITY] " + activity);
    
    // In a real implementation, this would write to a log file
    // with timestamps and detailed information for audit purposes
}

bool WiFiToolsApp::exportNetworkList(String filename) {
    String filePath = getAppDataPath() + "/" + filename;
    
    File file = SD.open(filePath, FILE_WRITE);
    if (!file) {
        debugLog("Failed to create export file: " + filePath);
        return false;
    }
    
    // Create JSON document
    DynamicJsonDocument doc(4096);
    
    doc["scan_time"] = millis();
    doc["network_count"] = networkCount;
    
    JsonArray networksArray = doc.createNestedArray("networks");
    for (uint8_t i = 0; i < networkCount; i++) {
        JsonObject networkObj = networksArray.createNestedObject();
        networkObj["ssid"] = networks[i].ssid;
        networkObj["bssid"] = networks[i].bssid;
        networkObj["rssi"] = networks[i].rssi;
        networkObj["channel"] = networks[i].channel;
        networkObj["security"] = getSecurityString(networks[i].security);
        networkObj["hidden"] = networks[i].hidden;
    }
    
    serializeJson(doc, file);
    file.close();
    
    debugLog("Network list exported: " + filename);
    return true;
}

void WiFiToolsApp::clearCapturedData() {
    clearNetworks();
    clearClients();
    ui.packetsSent = 0;
    ui.packetsReceived = 0;
    debugLog("Captured data cleared");
}