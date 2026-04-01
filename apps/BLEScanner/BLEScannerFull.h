#ifndef BLE_SCANNER_FULL_H
#define BLE_SCANNER_FULL_H

#include "../../core/AppManager/BaseApp.h"
#include "../../core/FileSystem.h"
#include "../../core/Config.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ---- Constants ----
#define BLE_MAX_DEVICES     32
#define BLE_DEVICE_TIMEOUT  30000
#define BLE_SCAN_DURATION   5
#define BLE_LIST_ITEM_H     22
#define BLE_LIST_MAX_VIS    8
#define BLE_HEADER_H        20
#define BLE_FOOTER_H        22

struct BLEEntry {
    char mac[18];
    char name[32];
    char label[24];
    int8_t rssi;
    unsigned long firstSeen;
    unsigned long lastSeen;
    uint32_t scanCount;
    bool isMacRandom;
    bool hasAnomaly;
};

enum BLEView { BLE_VIEW_LIST, BLE_VIEW_DETAIL, BLE_VIEW_STATS };

class BLEScannerApp : public BaseApp {
private:
    BLEScan*  pScan;
    bool      bleReady;
    bool      scanning;
    unsigned long lastScan;

    BLEEntry  devices[BLE_MAX_DEVICES];
    uint8_t   deviceCount;
    uint8_t   selectedIdx;
    int8_t    scrollOffset;

    BLEView   view;
    uint32_t  totalScans;
    uint32_t  anomalyCount;

    // ---- helpers ----
    int findDevice(const char* mac) {
        for (int i = 0; i < deviceCount; i++)
            if (strcmp(devices[i].mac, mac) == 0) return i;
        return -1;
    }

    void addOrUpdate(const char* mac, const char* name, int8_t rssi) {
        int idx = findDevice(mac);
        unsigned long now = millis();
        if (idx < 0) {
            if (deviceCount >= BLE_MAX_DEVICES) return;
            idx = deviceCount++;
            strncpy(devices[idx].mac, mac, 17); devices[idx].mac[17] = 0;
            devices[idx].firstSeen = now;
            devices[idx].scanCount = 0;
            devices[idx].label[0] = 0;
            devices[idx].hasAnomaly = false;
            // Detect randomised MAC (locally administered bit)
            devices[idx].isMacRandom = (mac[1] == '2' || mac[1] == '6' ||
                                        mac[1] == 'A' || mac[1] == 'a' ||
                                        mac[1] == 'E' || mac[1] == 'e');
            if (devices[idx].isMacRandom) { devices[idx].hasAnomaly = true; anomalyCount++; }
        }
        strncpy(devices[idx].name, (name && name[0]) ? name : "Unknown", 31);
        devices[idx].name[31] = 0;
        devices[idx].rssi = rssi;
        devices[idx].lastSeen = now;
        devices[idx].scanCount++;
    }

    void sortByRSSI() {
        for (int i = 0; i < deviceCount - 1; i++)
            for (int j = i + 1; j < deviceCount; j++)
                if (devices[j].rssi > devices[i].rssi) {
                    BLEEntry tmp = devices[i]; devices[i] = devices[j]; devices[j] = tmp;
                }
    }

    void pruneOld() {
        unsigned long now = millis();
        for (int i = 0; i < deviceCount; ) {
            if (now - devices[i].lastSeen > BLE_DEVICE_TIMEOUT) {
                devices[i] = devices[--deviceCount];
            } else i++;
        }
    }

    uint16_t rssiColor(int8_t rssi) {
        if (rssi > -50) return COLOR_GREEN;
        if (rssi > -70) return COLOR_YELLOW;
        return COLOR_RED;
    }

    void drawHeader() {
        displayManager.drawRetroRect(0, 0, SCREEN_WIDTH, BLE_HEADER_H, COLOR_DARK_GRAY, true);
        displayManager.setFont(FONT_SMALL);
        char buf[40];
        snprintf(buf, sizeof(buf), "BLE  Dev:%d  Anom:%lu", deviceCount, (unsigned long)anomalyCount);
        displayManager.drawText(4, 5, buf, COLOR_GREEN_PHOS);
        displayManager.drawText(SCREEN_WIDTH - 50, 5, scanning ? "SCAN..." : "IDLE", 
                                scanning ? COLOR_YELLOW : COLOR_LIGHT_GRAY);
    }

    void drawFooter() {
        int16_t fy = SCREEN_HEIGHT - BLE_FOOTER_H;
        displayManager.drawRetroRect(0, fy, SCREEN_WIDTH, BLE_FOOTER_H, COLOR_DARK_GRAY, true);
        displayManager.setFont(FONT_SMALL);
        displayManager.drawText(4,   fy + 6, scanning ? "STOP" : "SCAN", COLOR_WHITE);
        displayManager.drawText(70,  fy + 6, "DETAIL", COLOR_WHITE);
        displayManager.drawText(150, fy + 6, "STATS",  COLOR_WHITE);
        displayManager.drawText(230, fy + 6, "EXIT",   COLOR_WHITE);
    }

    void drawList() {
        int16_t y = BLE_HEADER_H + 2;
        int maxVis = (SCREEN_HEIGHT - BLE_HEADER_H - BLE_FOOTER_H - 4) / BLE_LIST_ITEM_H;
        for (int i = 0; i < maxVis && (i + scrollOffset) < deviceCount; i++) {
            int idx = i + scrollOffset;
            bool sel = (idx == selectedIdx);
            uint16_t bg = sel ? COLOR_MID_GRAY : COLOR_BLACK;
            displayManager.drawRetroRect(0, y, SCREEN_WIDTH, BLE_LIST_ITEM_H - 1, bg, true);

            // RSSI bar
            int barW = map(constrain(devices[idx].rssi, -100, -20), -100, -20, 2, 20);
            displayManager.drawRetroRect(2, y + 4, barW, BLE_LIST_ITEM_H - 8,
                                         rssiColor(devices[idx].rssi), true);

            // Name + MAC
            char line[48];
            snprintf(line, sizeof(line), "%-16s %s", devices[idx].name, devices[idx].mac);
            displayManager.drawText(26, y + 4, line, sel ? COLOR_WHITE : COLOR_GREEN_PHOS);

            // Anomaly marker
            if (devices[idx].hasAnomaly)
                displayManager.drawText(SCREEN_WIDTH - 14, y + 4, "!", COLOR_RED_GLOW);

            y += BLE_LIST_ITEM_H;
        }
    }

    void drawDetail() {
        if (selectedIdx >= deviceCount) { view = BLE_VIEW_LIST; return; }
        BLEEntry& d = devices[selectedIdx];
        displayManager.setFont(FONT_MEDIUM);
        displayManager.drawText(5, 25, d.name, COLOR_GREEN_PHOS);
        displayManager.setFont(FONT_SMALL);
        char buf[48];
        snprintf(buf, sizeof(buf), "MAC: %s", d.mac);
        displayManager.drawText(5, 50, buf, COLOR_WHITE);
        snprintf(buf, sizeof(buf), "RSSI: %d dBm", d.rssi);
        displayManager.drawText(5, 65, buf, rssiColor(d.rssi));
        snprintf(buf, sizeof(buf), "Scans: %lu", (unsigned long)d.scanCount);
        displayManager.drawText(5, 80, buf, COLOR_WHITE);
        snprintf(buf, sizeof(buf), "Age: %lus", (millis() - d.firstSeen) / 1000);
        displayManager.drawText(5, 95, buf, COLOR_WHITE);
        displayManager.drawText(5, 110, d.isMacRandom ? "MAC: RANDOMISED" : "MAC: Static",
                                d.isMacRandom ? COLOR_RED_GLOW : COLOR_GREEN_PHOS);
        if (d.label[0])
            displayManager.drawText(5, 125, String("Label: ") + d.label, COLOR_YELLOW);
    }

    void drawStats() {
        displayManager.setFont(FONT_MEDIUM);
        displayManager.drawText(5, 25, "BLE Statistics", COLOR_GREEN_PHOS);
        displayManager.setFont(FONT_SMALL);
        char buf[48];
        snprintf(buf, sizeof(buf), "Total devices: %d", deviceCount);
        displayManager.drawText(5, 50, buf, COLOR_WHITE);
        snprintf(buf, sizeof(buf), "Total scans: %lu", (unsigned long)totalScans);
        displayManager.drawText(5, 65, buf, COLOR_WHITE);
        snprintf(buf, sizeof(buf), "Anomalies: %lu", (unsigned long)anomalyCount);
        displayManager.drawText(5, 80, buf, anomalyCount ? COLOR_RED_GLOW : COLOR_GREEN_PHOS);
        // RSSI histogram
        displayManager.drawText(5, 100, "Signal strength:", COLOR_LIGHT_GRAY);
        int strong = 0, mid = 0, weak = 0;
        for (int i = 0; i < deviceCount; i++) {
            if (devices[i].rssi > -50) strong++;
            else if (devices[i].rssi > -70) mid++;
            else weak++;
        }
        snprintf(buf, sizeof(buf), "Strong:%d  Mid:%d  Weak:%d", strong, mid, weak);
        displayManager.drawText(5, 115, buf, COLOR_WHITE);
    }

    void saveLog() {
        filesystem.ensureDirExists("/logs");
        String entry = "[" + String(millis()) + "] Scan #" + String(totalScans) +
                       " found " + String(deviceCount) + " devices\n";
        for (int i = 0; i < deviceCount; i++) {
            entry += "  " + String(devices[i].mac) + " " +
                     String(devices[i].name) + " " +
                     String(devices[i].rssi) + "dBm\n";
        }
        filesystem.appendFile("/logs/ble_scan.log", entry);
    }

public:
    BLEScannerApp() :
        pScan(nullptr), bleReady(false), scanning(false), lastScan(0),
        deviceCount(0), selectedIdx(0), scrollOffset(0),
        view(BLE_VIEW_LIST), totalScans(0), anomalyCount(0)
    {
        setMetadata("BLEScanner","1.0","remu.ii","Bluetooth LE scanner",CATEGORY_TOOLS,12288);
        memset(devices, 0, sizeof(devices));
    }

    bool initialize() override {
        Serial.println("[BLEScanner] Initializing BLE...");
        BLEDevice::init("remu.ii");
        pScan = BLEDevice::getScan();
        pScan->setActiveScan(true);
        pScan->setInterval(100);
        pScan->setWindow(99);
        bleReady = true;
        currentState = APP_RUNNING;
        filesystem.ensureDirExists("/logs");
        Serial.println("[BLEScanner] Ready");
        return true;
    }

    void update() override {
        if (!bleReady || currentState != APP_RUNNING) return;

        // Auto-scan every 10s when idle
        if (!scanning && millis() - lastScan > 10000) {
            scanning = true;
            lastScan = millis();
            BLEScanResults results = pScan->start(BLE_SCAN_DURATION, false);
            totalScans++;
            for (int i = 0; i < results.getCount(); i++) {
                BLEAdvertisedDevice dev = results.getDevice(i);
                addOrUpdate(dev.getAddress().toString().c_str(),
                            dev.haveName() ? dev.getName().c_str() : "",
                            dev.getRSSI());
            }
            pScan->clearResults();
            sortByRSSI();
            pruneOld();
            saveLog();
            scanning = false;
        }
    }

    void render() override {
        if (currentState != APP_RUNNING) return;
        displayManager.clearScreen(COLOR_BLACK);
        drawHeader();
        switch (view) {
            case BLE_VIEW_LIST:   drawList();   break;
            case BLE_VIEW_DETAIL: drawDetail(); break;
            case BLE_VIEW_STATS:  drawStats();  break;
        }
        drawFooter();
    }

    bool handleTouch(TouchPoint touch) override {
        if (!touch.isNewPress) return true;

        int16_t fy = SCREEN_HEIGHT - BLE_FOOTER_H;

        // Footer buttons
        if (touch.y >= fy) {
            if (touch.x < 65) {
                // SCAN / STOP — trigger immediate scan
                if (!scanning) lastScan = 0;
            } else if (touch.x < 145) {
                view = (view == BLE_VIEW_DETAIL) ? BLE_VIEW_LIST : BLE_VIEW_DETAIL;
            } else if (touch.x < 225) {
                view = (view == BLE_VIEW_STATS) ? BLE_VIEW_LIST : BLE_VIEW_STATS;
            } else {
                cleanup();
                return false;
            }
            return true;
        }

        // List area — select device
        if (view == BLE_VIEW_LIST && touch.y > BLE_HEADER_H) {
            int row = (touch.y - BLE_HEADER_H - 2) / BLE_LIST_ITEM_H;
            int idx = row + scrollOffset;
            if (idx >= 0 && idx < deviceCount) {
                selectedIdx = idx;
                view = BLE_VIEW_DETAIL;
            }
        } else if (view == BLE_VIEW_DETAIL) {
            view = BLE_VIEW_LIST;
        }
        return true;
    }

    void cleanup() override {
        if (pScan && scanning) pScan->stop();
        if (bleReady) BLEDevice::deinit(true);
        bleReady = false;
        currentState = APP_EXITING;
    }

    void onPause() override { if (pScan && scanning) pScan->stop(); scanning = false; }
    void onResume() override { lastScan = 0; }

    String getName() const override { return "BLEScanner"; }
    void setAppManager(void* m) override {}
};

#endif // BLE_SCANNER_FULL_H
