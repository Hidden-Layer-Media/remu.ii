#ifndef CAR_CLONER_FULL_H
#define CAR_CLONER_FULL_H

#include "../../core/AppManager/BaseApp.h"
#include "../../core/FileSystem.h"
#include "../../core/Config/hardware_pins.h"
#include <ArduinoJson.h>

#define CC_MAX_SIGNALS   16
#define CC_MAX_SAMPLES   4096
#define CC_SAMPLE_RATE   1000000UL  // 1 MHz ADC sampling
#define CC_CAPTURE_MS    500        // 500ms capture window
#define CC_ITEM_H        26
#define CC_MAX_VIS       7

enum CCView {
    CC_VIEW_WARNING,
    CC_VIEW_MAIN,
    CC_VIEW_CAPTURE,
    CC_VIEW_LIBRARY,
    CC_VIEW_REPLAY,
    CC_VIEW_ANALYSIS
};

struct CCSignal {
    char     name[24];
    float    freqMHz;
    uint16_t samples[CC_MAX_SAMPLES];
    uint16_t sampleCount;
    uint32_t captureTime;
    int8_t   rssi;
    uint32_t pulseCount;
    uint32_t avgPulseUs;
    bool     savedToSD;
    char     filePath[48];
};

class CarClonerApp : public BaseApp {
private:
    CCSignal signals[CC_MAX_SIGNALS];
    uint8_t  signalCount;
    int8_t   selectedIdx;
    int8_t   scrollOffset;

    CCView   view;
    bool     capturing;
    bool     replaying;
    unsigned long captureStart;
    uint16_t captureIdx;

    float    currentFreq;
    uint8_t  power;          // 0-255
    bool     warningAccepted;
    unsigned long warningShown;

    String   statusMsg;
    unsigned long statusExpiry;

    static constexpr float FREQ_PRESETS[] = { 315.0f, 433.92f, 868.0f, 915.0f };
    static constexpr const char* FREQ_NAMES[] = { "315MHz", "433MHz", "868MHz", "915MHz" };
    uint8_t  freqPresetIdx;

    // ---- Capture ----
    void startCapture() {
        if (signalCount >= CC_MAX_SIGNALS) { showStatus("Library full!"); return; }
        capturing = true;
        captureStart = millis();
        captureIdx = 0;
        CCSignal& sig = signals[signalCount];
        memset(&sig, 0, sizeof(sig));
        snprintf(sig.name, sizeof(sig.name), "Signal_%d", signalCount + 1);
        sig.freqMHz = currentFreq;
        sig.captureTime = millis();
        showStatus("Capturing...");
    }

    void updateCapture() {
        if (!capturing) return;
        // Sample ADC as fast as possible into buffer
        if (captureIdx < CC_MAX_SAMPLES) {
            signals[signalCount].samples[captureIdx++] = analogRead(ENTROPY_PIN_1);
        }
        if (millis() - captureStart >= CC_CAPTURE_MS || captureIdx >= CC_MAX_SAMPLES) {
            finalizeCapture();
        }
    }

    void finalizeCapture() {
        capturing = false;
        CCSignal& sig = signals[signalCount];
        sig.sampleCount = captureIdx;
        sig.rssi = -60 + (int8_t)(analogRead(ENTROPY_PIN_2) / 68); // rough RSSI estimate
        analyzeSignal(sig);
        signalCount++;
        saveSignalToSD(signalCount - 1);
        showStatus("Captured! " + String(sig.sampleCount) + " samples");
        view = CC_VIEW_LIBRARY;
    }

    void analyzeSignal(CCSignal& sig) {
        if (sig.sampleCount < 4) return;
        // Detect pulses (threshold crossings)
        uint16_t threshold = 2048;
        bool lastHigh = sig.samples[0] > threshold;
        uint32_t pulses = 0;
        uint32_t totalHighUs = 0;
        uint16_t runLen = 0;
        for (uint16_t i = 1; i < sig.sampleCount; i++) {
            bool high = sig.samples[i] > threshold;
            if (high != lastHigh) {
                pulses++;
                if (lastHigh) totalHighUs += runLen;
                runLen = 0;
                lastHigh = high;
            }
            runLen++;
        }
        sig.pulseCount = pulses;
        sig.avgPulseUs = (pulses > 0) ? (totalHighUs / max(1UL, (unsigned long)(pulses / 2))) : 0;
    }

    // ---- Replay ----
    void startReplay(int idx) {
        if (idx < 0 || idx >= signalCount) return;
        replaying = true;
        showStatus("Replaying " + String(signals[idx].name));
        CCSignal& sig = signals[idx];
        // Output via DAC
        for (uint16_t i = 0; i < sig.sampleCount; i++) {
            dacWrite(DAC_OUT_LEFT, sig.samples[i] >> 4); // 12-bit to 8-bit
            delayMicroseconds(1); // ~1MHz output rate
        }
        dacWrite(DAC_OUT_LEFT, 0);
        replaying = false;
        showStatus("Replay complete");
    }

    // ---- SD Card ----
    void saveSignalToSD(int idx) {
        if (idx < 0 || idx >= signalCount) return;
        filesystem.ensureDirExists("/data/carcloner");
        CCSignal& sig = signals[idx];
        snprintf(sig.filePath, sizeof(sig.filePath), "/data/carcloner/%s.bin", sig.name);

        // Save header as JSON + raw samples as binary
        DynamicJsonDocument doc(512);
        doc["name"]    = sig.name;
        doc["freq"]    = sig.freqMHz;
        doc["count"]   = sig.sampleCount;
        doc["pulses"]  = sig.pulseCount;
        doc["avgPulse"]= sig.avgPulseUs;
        doc["rssi"]    = sig.rssi;
        String hdr;
        serializeJson(doc, hdr);

        char hdrPath[52];
        snprintf(hdrPath, sizeof(hdrPath), "/data/carcloner/%s.json", sig.name);
        filesystem.writeFile(hdrPath, hdr);
        filesystem.writeBinaryFile(sig.filePath, (uint8_t*)sig.samples, sig.sampleCount * 2);
        sig.savedToSD = true;
    }

    void loadLibraryFromSD() {
        if (!filesystem.isReady()) return;
        std::vector<String> files = filesystem.listFiles("/data/carcloner");
        for (const String& f : files) {
            if (!f.endsWith(".json") || signalCount >= CC_MAX_SIGNALS) continue;
            String path = "/data/carcloner/" + f;
            String data = filesystem.readFile(path);
            DynamicJsonDocument doc(512);
            if (deserializeJson(doc, data) != DeserializationError::Ok) continue;
            CCSignal& sig = signals[signalCount];
            strncpy(sig.name, doc["name"] | "Unknown", 23);
            sig.freqMHz    = doc["freq"]     | 433.92f;
            sig.sampleCount= doc["count"]    | 0;
            sig.pulseCount = doc["pulses"]   | 0;
            sig.avgPulseUs = doc["avgPulse"] | 0;
            sig.rssi       = doc["rssi"]     | -70;
            sig.savedToSD  = true;
            snprintf(sig.filePath, sizeof(sig.filePath), "/data/carcloner/%s.bin", sig.name);
            // Load samples
            if (sig.sampleCount > 0 && sig.sampleCount <= CC_MAX_SAMPLES) {
                filesystem.readBinaryFile(sig.filePath, (uint8_t*)sig.samples, sig.sampleCount * 2);
            }
            signalCount++;
        }
    }

    // ---- Status ----
    void showStatus(const String& msg, uint32_t ms = 3000) {
        statusMsg = msg;
        statusExpiry = millis() + ms;
    }

    // ---- UI ----
    void drawHeader(const char* title) {
        displayManager.drawRetroRect(0, 0, SCREEN_WIDTH, 20, COLOR_DARK_GRAY, true);
        displayManager.setFont(FONT_SMALL);
        displayManager.drawText(4, 5, title, COLOR_RED_GLOW);
        char freqBuf[20];
        snprintf(freqBuf, sizeof(freqBuf), "%.2fMHz", currentFreq);
        displayManager.drawText(SCREEN_WIDTH - 80, 5, freqBuf, COLOR_GREEN_PHOS);
    }

    void drawFooter() {
        int16_t fy = SCREEN_HEIGHT - 20;
        displayManager.drawRetroRect(0, fy, SCREEN_WIDTH, 20, COLOR_DARK_GRAY, true);
        displayManager.setFont(FONT_SMALL);
        if (millis() < statusExpiry) {
            displayManager.drawText(4, fy + 6, statusMsg, COLOR_YELLOW);
        } else {
            displayManager.drawText(4,   fy + 6, "CAPT", COLOR_WHITE);
            displayManager.drawText(60,  fy + 6, "LIB",  COLOR_WHITE);
            displayManager.drawText(110, fy + 6, "FREQ", COLOR_WHITE);
            displayManager.drawText(170, fy + 6, "PWR",  COLOR_WHITE);
            displayManager.drawText(280, fy + 6, "EXIT", COLOR_WHITE);
        }
    }

    void renderWarning() {
        displayManager.clearScreen(COLOR_BLACK);
        displayManager.setFont(FONT_MEDIUM);
        displayManager.drawTextCentered(0, 20, SCREEN_WIDTH, "! LEGAL WARNING !", COLOR_RED_GLOW);
        displayManager.setFont(FONT_SMALL);
        const char* lines[] = {
            "This tool is for AUTHORIZED",
            "security research ONLY.",
            "",
            "RF transmission may be illegal",
            "without proper licensing.",
            "",
            "Only use on devices you OWN",
            "or have written permission.",
            "",
            "Tap ACCEPT to continue."
        };
        for (int i = 0; i < 10; i++)
            displayManager.drawText(10, 45 + i * 14, lines[i], i == 9 ? COLOR_GREEN_PHOS : COLOR_WHITE);
    }

    void renderMain() {
        displayManager.clearScreen(COLOR_BLACK);
        drawHeader("CarCloner RF");
        displayManager.setFont(FONT_MEDIUM);
        displayManager.drawButton(20,  40, 130, 36, "CAPTURE",  BUTTON_NORMAL, COLOR_MID_GRAY);
        displayManager.drawButton(170, 40, 130, 36, "LIBRARY",  BUTTON_NORMAL, COLOR_MID_GRAY);
        displayManager.drawButton(20,  90, 130, 36, "REPLAY",   BUTTON_NORMAL, COLOR_MID_GRAY);
        displayManager.drawButton(170, 90, 130, 36, "ANALYSIS", BUTTON_NORMAL, COLOR_MID_GRAY);
        displayManager.setFont(FONT_SMALL);
        char buf[48];
        snprintf(buf, sizeof(buf), "Signals: %d  Freq: %.2fMHz  Pwr: %d",
                 signalCount, currentFreq, power);
        displayManager.drawText(10, 145, buf, COLOR_LIGHT_GRAY);
        drawFooter();
    }

    void renderCapture() {
        displayManager.clearScreen(COLOR_BLACK);
        drawHeader("Capture RF Signal");
        displayManager.setFont(FONT_MEDIUM);
        if (capturing) {
            uint32_t elapsed = millis() - captureStart;
            char buf[32];
            snprintf(buf, sizeof(buf), "Capturing... %lums", elapsed);
            displayManager.drawTextCentered(0, 80, SCREEN_WIDTH, buf, COLOR_YELLOW);
            // Progress bar
            displayManager.drawProgressBar(20, 110, 280, 16,
                (uint8_t)(elapsed * 100 / CC_CAPTURE_MS));
            snprintf(buf, sizeof(buf), "Samples: %d", captureIdx);
            displayManager.drawTextCentered(0, 140, SCREEN_WIDTH, buf, COLOR_WHITE);
        } else {
            displayManager.drawTextCentered(0, 80, SCREEN_WIDTH, "Ready to capture", COLOR_GREEN_PHOS);
            char buf[32];
            snprintf(buf, sizeof(buf), "Freq: %.2f MHz", currentFreq);
            displayManager.drawTextCentered(0, 110, SCREEN_WIDTH, buf, COLOR_WHITE);
            displayManager.drawButton(95, 145, 130, 32, "START CAPTURE", BUTTON_NORMAL, COLOR_MID_GRAY);
        }
        drawFooter();
    }

    void renderLibrary() {
        displayManager.clearScreen(COLOR_BLACK);
        drawHeader("Signal Library");
        if (signalCount == 0) {
            displayManager.setFont(FONT_MEDIUM);
            displayManager.drawTextCentered(0, 100, SCREEN_WIDTH, "No signals captured", COLOR_LIGHT_GRAY);
        } else {
            displayManager.setFont(FONT_SMALL);
            int maxVis = (SCREEN_HEIGHT - 40 - 20) / CC_ITEM_H;
            for (int i = 0; i < maxVis && (i + scrollOffset) < signalCount; i++) {
                int idx = i + scrollOffset;
                bool sel = (idx == selectedIdx);
                int16_t y = 22 + i * CC_ITEM_H;
                displayManager.drawRetroRect(0, y, SCREEN_WIDTH, CC_ITEM_H - 1,
                                             sel ? COLOR_MID_GRAY : COLOR_BLACK, true);
                char line[48];
                snprintf(line, sizeof(line), "%-14s %.2fMHz %ddBm %lup",
                         signals[idx].name, signals[idx].freqMHz,
                         signals[idx].rssi, (unsigned long)signals[idx].pulseCount);
                displayManager.drawText(4, y + 6, line, sel ? COLOR_WHITE : COLOR_GREEN_PHOS);
                if (signals[idx].savedToSD)
                    displayManager.drawText(SCREEN_WIDTH - 14, y + 6, "S", COLOR_BLUE_CYBER);
            }
        }
        drawFooter();
    }

    void renderReplay() {
        displayManager.clearScreen(COLOR_BLACK);
        drawHeader("Replay Signal");
        if (selectedIdx < 0 || selectedIdx >= signalCount) {
            displayManager.setFont(FONT_MEDIUM);
            displayManager.drawTextCentered(0, 100, SCREEN_WIDTH, "Select signal in Library", COLOR_LIGHT_GRAY);
        } else {
            CCSignal& sig = signals[selectedIdx];
            displayManager.setFont(FONT_MEDIUM);
            displayManager.drawText(10, 30, sig.name, COLOR_GREEN_PHOS);
            displayManager.setFont(FONT_SMALL);
            char buf[48];
            snprintf(buf, sizeof(buf), "Freq: %.2f MHz  RSSI: %d dBm", sig.freqMHz, sig.rssi);
            displayManager.drawText(10, 55, buf, COLOR_WHITE);
            snprintf(buf, sizeof(buf), "Samples: %d  Pulses: %lu", sig.sampleCount, (unsigned long)sig.pulseCount);
            displayManager.drawText(10, 70, buf, COLOR_WHITE);
            // Waveform preview
            displayManager.drawRetroRect(10, 90, 300, 50, COLOR_DARK_GRAY, false);
            if (sig.sampleCount > 0) {
                for (int x = 0; x < 300; x++) {
                    int sIdx = (x * sig.sampleCount) / 300;
                    int16_t y = 90 + 50 - (int16_t)(sig.samples[sIdx] * 50 / 4095);
                    y = constrain(y, 90, 139);
                    displayManager.drawPixel(10 + x, y, COLOR_GREEN_PHOS);
                }
            }
            displayManager.drawButton(95, 150, 130, 32, replaying ? "REPLAYING..." : "REPLAY NOW",
                                      replaying ? BUTTON_HIGHLIGHTED : BUTTON_NORMAL, COLOR_MID_GRAY);
        }
        drawFooter();
    }

    void renderAnalysis() {
        displayManager.clearScreen(COLOR_BLACK);
        drawHeader("Signal Analysis");
        if (selectedIdx < 0 || selectedIdx >= signalCount) {
            displayManager.setFont(FONT_MEDIUM);
            displayManager.drawTextCentered(0, 100, SCREEN_WIDTH, "Select signal in Library", COLOR_LIGHT_GRAY);
        } else {
            CCSignal& sig = signals[selectedIdx];
            displayManager.setFont(FONT_MEDIUM);
            displayManager.drawText(10, 25, sig.name, COLOR_GREEN_PHOS);
            displayManager.setFont(FONT_SMALL);
            char buf[48];
            snprintf(buf, sizeof(buf), "Frequency:  %.2f MHz", sig.freqMHz);
            displayManager.drawText(10, 48, buf, COLOR_WHITE);
            snprintf(buf, sizeof(buf), "Samples:    %d", sig.sampleCount);
            displayManager.drawText(10, 62, buf, COLOR_WHITE);
            snprintf(buf, sizeof(buf), "Pulses:     %lu", (unsigned long)sig.pulseCount);
            displayManager.drawText(10, 76, buf, COLOR_WHITE);
            snprintf(buf, sizeof(buf), "Avg pulse:  %lu us", (unsigned long)sig.avgPulseUs);
            displayManager.drawText(10, 90, buf, COLOR_WHITE);
            snprintf(buf, sizeof(buf), "RSSI:       %d dBm", sig.rssi);
            displayManager.drawText(10, 104, buf, COLOR_WHITE);
            // Modulation guess
            const char* mod = "Unknown";
            if (sig.avgPulseUs > 0 && sig.avgPulseUs < 500)  mod = "OOK/ASK (short)";
            else if (sig.avgPulseUs < 2000) mod = "OOK/ASK (std)";
            else if (sig.avgPulseUs < 5000) mod = "PWM";
            snprintf(buf, sizeof(buf), "Modulation: %s", mod);
            displayManager.drawText(10, 118, buf, COLOR_YELLOW);
            snprintf(buf, sizeof(buf), "Saved: %s", sig.savedToSD ? "YES" : "NO");
            displayManager.drawText(10, 132, buf, sig.savedToSD ? COLOR_GREEN_PHOS : COLOR_LIGHT_GRAY);
        }
        drawFooter();
    }

public:
    CarClonerApp() :
        signalCount(0), selectedIdx(-1), scrollOffset(0),
        view(CC_VIEW_WARNING), capturing(false), replaying(false),
        captureStart(0), captureIdx(0),
        currentFreq(433.92f), power(50),
        warningAccepted(false), warningShown(0), statusExpiry(0),
        freqPresetIdx(1)
    {
        setMetadata("CarCloner","1.0","remu.ii","RF signal capture & replay",CATEGORY_TOOLS,16384);
        memset(signals, 0, sizeof(signals));
    }

    bool initialize() override {
        Serial.println("[CarCloner] Initializing...");
        analogReadResolution(12);
        filesystem.ensureDirExists("/data/carcloner");
        filesystem.ensureDirExists("/logs");
        loadLibraryFromSD();
        warningShown = millis();
        currentState = APP_RUNNING;
        return true;
    }

    void update() override {
        if (currentState != APP_RUNNING) return;
        if (capturing) updateCapture();
    }

    void render() override {
        if (currentState != APP_RUNNING) return;
        switch (view) {
            case CC_VIEW_WARNING:  renderWarning();  break;
            case CC_VIEW_MAIN:     renderMain();     break;
            case CC_VIEW_CAPTURE:  renderCapture();  break;
            case CC_VIEW_LIBRARY:  renderLibrary();  break;
            case CC_VIEW_REPLAY:   renderReplay();   break;
            case CC_VIEW_ANALYSIS: renderAnalysis(); break;
        }
    }

    bool handleTouch(TouchPoint touch) override {
        if (!touch.isNewPress) return true;

        // Legal warning
        if (view == CC_VIEW_WARNING) {
            if (millis() - warningShown >= 3000) {
                warningAccepted = true;
                view = CC_VIEW_MAIN;
            }
            return true;
        }

        int16_t fy = SCREEN_HEIGHT - 20;

        // Footer
        if (touch.y >= fy) {
            if (touch.x < 55) {
                view = CC_VIEW_CAPTURE;
            } else if (touch.x < 105) {
                view = CC_VIEW_LIBRARY;
            } else if (touch.x < 165) {
                // Cycle frequency preset
                const float presets[] = { 315.0f, 433.92f, 868.0f, 915.0f };
                freqPresetIdx = (freqPresetIdx + 1) % 4;
                currentFreq = presets[freqPresetIdx];
                showStatus("Freq: " + String(currentFreq) + " MHz");
            } else if (touch.x < 225) {
                power = (power >= 200) ? 50 : power + 50;
                showStatus("Power: " + String(power));
            } else if (touch.x >= 270) {
                cleanup();
                return false;
            }
            return true;
        }

        // View-specific touch
        switch (view) {
            case CC_VIEW_MAIN:
                if (touch.y >= 40 && touch.y < 76) {
                    if (touch.x < 160) view = CC_VIEW_CAPTURE;
                    else               view = CC_VIEW_LIBRARY;
                } else if (touch.y >= 90 && touch.y < 126) {
                    if (touch.x < 160) view = CC_VIEW_REPLAY;
                    else               view = CC_VIEW_ANALYSIS;
                }
                break;

            case CC_VIEW_CAPTURE:
                if (!capturing && touch.y >= 145 && touch.y < 177) startCapture();
                break;

            case CC_VIEW_LIBRARY:
                if (touch.y > 22 && touch.y < fy) {
                    int row = (touch.y - 22) / CC_ITEM_H;
                    int idx = row + scrollOffset;
                    if (idx >= 0 && idx < signalCount) {
                        selectedIdx = idx;
                        view = CC_VIEW_ANALYSIS;
                    }
                }
                break;

            case CC_VIEW_REPLAY:
                if (!replaying && touch.y >= 150 && touch.y < 182 && selectedIdx >= 0)
                    startReplay(selectedIdx);
                break;

            case CC_VIEW_ANALYSIS:
                view = CC_VIEW_LIBRARY;
                break;

            default: break;
        }
        return true;
    }

    void cleanup() override {
        capturing = false;
        replaying = false;
        dacWrite(DAC_OUT_LEFT, 0);
        currentState = APP_EXITING;
    }

    void onPause() override  { capturing = false; }
    void onResume() override {}

    String getName() const override { return "CarCloner"; }
    void setAppManager(void* m) override {}
};

#endif // CAR_CLONER_FULL_H
