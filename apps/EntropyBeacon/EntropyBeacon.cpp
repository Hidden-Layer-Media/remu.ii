#include "EntropyBeacon.h"
#include "../../core/SystemCore/SystemCore.h"
#include "../../core/DisplayManager/DisplayManager.h"
#include "../../core/Config/hardware_pins.h"
#include <math.h>

const uint8_t EntropyBeaconApp::ENTROPY_ICON[32] = {
    0x00,0x00,0x18,0x18,0x3C,0x3C,0x7E,0x7E,
    0xFF,0xFF,0x7E,0x7E,0x3C,0x3C,0x18,0x18,
    0x81,0x81,0xC3,0xC3,0x66,0x66,0x3C,0x3C,
    0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00
};

EntropyBeaconApp::EntropyBeaconApp() :
    bufferIndex(0), lastSampleTime(0), sampleInterval(1000), dacEnabled(false),
    recordingEnabled(false)
{
    setMetadata("EntropyBeacon","1.0","remu.ii","Real-time entropy visualizer",CATEGORY_OTHER,12288);
    memset(entropyBuffer, 0, sizeof(entropyBuffer));
    memset(spectrumData, 0, sizeof(spectrumData));
    viz = { VIZ_OSCILLOSCOPE, RATE_1KHZ, 1.0f, COLOR_GREEN_PHOS };
    anomalyDetector = { 0.5f, 0.1f, 2.5f, 0 };
}

EntropyBeaconApp::~EntropyBeaconApp() { cleanup(); }

bool EntropyBeaconApp::initialize() {
    Serial.println("[EntropyBeacon] Initializing...");
    currentState = APP_RUNNING;
    calculateSampleInterval();
    filesystem.ensureDirExists("/data/entropybeacon");
    loadConfiguration();
    return true;
}

void EntropyBeaconApp::update() {
    if (currentState != APP_RUNNING) return;
    unsigned long now = micros();
    if (now - lastSampleTime >= sampleInterval) {
        sampleEntropy();
        lastSampleTime = now;
    }
    if (dacEnabled) updateDACOutput();
}

void EntropyBeaconApp::render() {
    if (currentState != APP_RUNNING) return;
    displayManager.clearScreen(COLOR_BLACK);

    // Header
    displayManager.setFont(FONT_MEDIUM);
    displayManager.drawText(5, 2, "Entropy Beacon", COLOR_RED_GLOW);
    displayManager.setFont(FONT_SMALL);
    const char* modeStr = (viz.mode == VIZ_OSCILLOSCOPE) ? "OSC" : "SPEC";
    displayManager.drawText(200, 5, modeStr, COLOR_GREEN_PHOS);
    char rateStr[12];
    snprintf(rateStr, sizeof(rateStr), "%dHz", viz.sampleRate);
    displayManager.drawText(240, 5, rateStr, COLOR_WHITE);

    // Stats row
    char statBuf[40];
    snprintf(statBuf, sizeof(statBuf), "Buf:%d  Anom:%lu", getBufferSize(), anomalyDetector.anomalyCount);
    displayManager.drawText(5, 20, statBuf, COLOR_LIGHT_GRAY);

    // Graph border
    displayManager.drawRetroRect(GRAPH_X - 1, GRAPH_Y - 1, GRAPH_WIDTH + 2, GRAPH_HEIGHT + 2, COLOR_DARK_GRAY, false);

    if (viz.mode == VIZ_OSCILLOSCOPE) drawOscilloscope();
    else drawSpectrum();

    drawControls();
}

bool EntropyBeaconApp::handleTouch(TouchPoint touch) {
    if (!touch.isNewPress) return true;

    // Bottom control row
    if (touch.y > 210) {
        if (touch.x < 80) {
            viz.mode = (viz.mode == VIZ_OSCILLOSCOPE) ? VIZ_SPECTRUM : VIZ_OSCILLOSCOPE;
        } else if (touch.x < 160) {
            // Cycle sample rate
            const SampleRate rates[] = { RATE_100HZ, RATE_500HZ, RATE_1KHZ, RATE_2KHZ, RATE_5KHZ, RATE_8KHZ };
            for (int i = 0; i < 6; i++) {
                if (viz.sampleRate == rates[i]) {
                    viz.sampleRate = rates[(i + 1) % 6];
                    break;
                }
            }
            calculateSampleInterval();
        } else if (touch.x < 240) {
            dacEnabled = !dacEnabled;
        } else {
            // Exit
            cleanup();
            return false;
        }
        return true;
    }

    // Graph area tap — reset anomaly counter
    if (touch.x >= GRAPH_X && touch.x < GRAPH_X + GRAPH_WIDTH &&
        touch.y >= GRAPH_Y && touch.y < GRAPH_Y + GRAPH_HEIGHT) {
        anomalyDetector.anomalyCount = 0;
    }
    return true;
}

void EntropyBeaconApp::cleanup() {
    if (recordingEnabled) stopRecording();
    dacWrite(DAC_OUT_LEFT, 0);
    saveConfiguration();
    currentState = APP_EXITING;
}

const uint8_t* EntropyBeaconApp::getIcon() const { return ENTROPY_ICON; }

// ---- Sampling ----

void EntropyBeaconApp::sampleEntropy() {
    uint16_t s1 = analogRead(ENTROPY_PIN_1);
    uint16_t s2 = analogRead(ENTROPY_PIN_2);
    uint32_t pool = systemCore.getEntropyPool();
    uint16_t val = (s1 ^ (s2 << 4) ^ (uint16_t)(pool & 0xFFF)) & 0xFFF;

    entropyBuffer[bufferIndex] = val;
    bufferIndex = (bufferIndex + 1) % ENTROPY_BUFFER_SIZE;

    float norm = val / 4095.0f;
    bool anomaly = detectAnomaly(norm);
    updateStatistics(norm);

    if (recordingEnabled) writeDataToSD(val, norm, anomaly);
}

bool EntropyBeaconApp::detectAnomaly(float value) {
    float stddev = sqrtf(anomalyDetector.variance);
    if (stddev < 0.001f) return false;
    bool isAnom = fabsf(value - anomalyDetector.mean) > anomalyDetector.threshold * stddev;
    if (isAnom) anomalyDetector.anomalyCount++;
    return isAnom;
}

void EntropyBeaconApp::updateStatistics(float value) {
    float alpha = 0.02f;
    float delta = value - anomalyDetector.mean;
    anomalyDetector.mean += alpha * delta;
    anomalyDetector.variance += alpha * (delta * delta - anomalyDetector.variance);
}

void EntropyBeaconApp::calculateSampleInterval() {
    sampleInterval = 1000000UL / viz.sampleRate;
}

// ---- Analysis ----

void EntropyBeaconApp::performSimpleFFT() {
    uint16_t n = SPECTRUM_BINS * 2;
    for (uint16_t k = 0; k < SPECTRUM_BINS; k++) {
        float re = 0, im = 0;
        for (uint16_t t = 0; t < n; t++) {
            uint16_t idx = (bufferIndex + t) % ENTROPY_BUFFER_SIZE;
            float sample = (entropyBuffer[idx] / 4095.0f) - 0.5f;
            float angle = 2.0f * M_PI * k * t / n;
            re += sample * cosf(angle);
            im -= sample * sinf(angle);
        }
        spectrumData[k] = sqrtf(re * re + im * im) / n;
    }
}

// ---- Visualization ----

void EntropyBeaconApp::drawOscilloscope() {
    uint16_t count = getBufferSize();
    if (count < 2) return;

    // Mean line
    int16_t meanY = GRAPH_Y + GRAPH_HEIGHT - (int16_t)(anomalyDetector.mean * GRAPH_HEIGHT);
    meanY = constrain(meanY, GRAPH_Y, GRAPH_Y + GRAPH_HEIGHT - 1);
    displayManager.drawLine(GRAPH_X, meanY, GRAPH_X + GRAPH_WIDTH, meanY, COLOR_DARK_GRAY);

    // Waveform
    for (int16_t x = 0; x < GRAPH_WIDTH - 1; x++) {
        uint16_t i1 = (bufferIndex + ENTROPY_BUFFER_SIZE - GRAPH_WIDTH + x) % ENTROPY_BUFFER_SIZE;
        uint16_t i2 = (i1 + 1) % ENTROPY_BUFFER_SIZE;
        float v1 = entropyBuffer[i1] / 4095.0f;
        float v2 = entropyBuffer[i2] / 4095.0f;
        int16_t y1 = GRAPH_Y + GRAPH_HEIGHT - (int16_t)(v1 * GRAPH_HEIGHT);
        int16_t y2 = GRAPH_Y + GRAPH_HEIGHT - (int16_t)(v2 * GRAPH_HEIGHT);
        y1 = constrain(y1, GRAPH_Y, GRAPH_Y + GRAPH_HEIGHT - 1);
        y2 = constrain(y2, GRAPH_Y, GRAPH_Y + GRAPH_HEIGHT - 1);
        displayManager.drawLine(GRAPH_X + x, y1, GRAPH_X + x + 1, y2, viz.traceColor);
    }

    // Current value
    char buf[20];
    snprintf(buf, sizeof(buf), "Val:%.3f", getCurrentEntropy());
    displayManager.setFont(FONT_SMALL);
    displayManager.drawText(GRAPH_X + 2, GRAPH_Y + 2, buf, COLOR_WHITE);
}

void EntropyBeaconApp::drawSpectrum() {
    performSimpleFFT();

    // Normalise
    float maxMag = 0.001f;
    for (uint16_t i = 0; i < SPECTRUM_BINS; i++) if (spectrumData[i] > maxMag) maxMag = spectrumData[i];

    int16_t barW = GRAPH_WIDTH / SPECTRUM_BINS;
    for (uint16_t i = 0; i < SPECTRUM_BINS; i++) {
        int16_t barH = (int16_t)((spectrumData[i] / maxMag) * GRAPH_HEIGHT);
        barH = constrain(barH, 1, GRAPH_HEIGHT);
        int16_t bx = GRAPH_X + i * barW;
        int16_t by = GRAPH_Y + GRAPH_HEIGHT - barH;
        uint16_t col = (i < SPECTRUM_BINS / 3) ? COLOR_RED_GLOW :
                       (i < 2 * SPECTRUM_BINS / 3) ? COLOR_GREEN_PHOS : COLOR_BLUE_CYBER;
        displayManager.drawRetroRect(bx, by, barW - 1, barH, col, true);
    }
}

void EntropyBeaconApp::drawControls() {
    const int16_t y = 215;
    displayManager.setFont(FONT_SMALL);
    displayManager.drawButton(2,   y, 74, 20, "MODE",  BUTTON_NORMAL, COLOR_MID_GRAY);
    displayManager.drawButton(80,  y, 74, 20, "RATE",  BUTTON_NORMAL, COLOR_MID_GRAY);
    displayManager.drawButton(158, y, 74, 20, dacEnabled ? "DAC:ON" : "DAC:OFF",
                              dacEnabled ? BUTTON_HIGHLIGHTED : BUTTON_NORMAL, COLOR_MID_GRAY);
    displayManager.drawButton(236, y, 74, 20, "EXIT",  BUTTON_NORMAL, COLOR_MID_GRAY);
}

// ---- DAC ----

void EntropyBeaconApp::updateDACOutput() {
    float val = getCurrentEntropy();
    dacWrite(DAC_OUT_LEFT, (uint8_t)(val * 255.0f));
}

// ---- SD Card ----

void EntropyBeaconApp::writeDataToSD(uint16_t value, float normalized, bool isAnomaly) {
    if (!recordingFile) return;
    char line[48];
    snprintf(line, sizeof(line), "%lu,%u,%.4f,%d\n", millis(), value, normalized, isAnomaly ? 1 : 0);
    recordingFile.print(line);
    static uint16_t flushCtr = 0;
    if (++flushCtr >= 20) { recordingFile.flush(); flushCtr = 0; }
}

void EntropyBeaconApp::logEventToSD(String eventType, float value) {
    File f = SD.open("/logs/entropy.log", FILE_APPEND);
    if (!f) return;
    char line[64];
    snprintf(line, sizeof(line), "%lu [%s] %.4f\n", millis(), eventType.c_str(), value);
    f.print(line);
    f.close();
}

bool EntropyBeaconApp::startRecording() {
    filesystem.ensureDirExists("/data/entropybeacon");
    char fname[48];
    snprintf(fname, sizeof(fname), "/data/entropybeacon/rec_%lu.csv", millis());
    recordingFile = SD.open(fname, FILE_WRITE);
    if (!recordingFile) return false;
    recordingFile.println("timestamp,value,normalized,anomaly");
    recordingEnabled = true;
    recordingPath = fname;
    return true;
}

void EntropyBeaconApp::stopRecording() {
    if (recordingFile) { recordingFile.flush(); recordingFile.close(); }
    recordingEnabled = false;
}

void EntropyBeaconApp::loadConfiguration() {
    if (!filesystem.fileExists("/data/entropybeacon/config.txt")) return;
    String data = filesystem.readFile("/data/entropybeacon/config.txt");
    int rateIdx = data.indexOf("rate=");
    if (rateIdx >= 0) {
        int rate = data.substring(rateIdx + 5).toInt();
        if (rate > 0) { viz.sampleRate = (SampleRate)rate; calculateSampleInterval(); }
    }
}

void EntropyBeaconApp::saveConfiguration() {
    filesystem.ensureDirExists("/data/entropybeacon");
    String cfg = "rate=" + String(viz.sampleRate) + "\n";
    filesystem.writeFile("/data/entropybeacon/config.txt", cfg);
}

// ---- Utility ----

uint16_t EntropyBeaconApp::getBufferSize() const {
    return min((uint16_t)ENTROPY_BUFFER_SIZE, (uint16_t)(bufferIndex == 0 ? ENTROPY_BUFFER_SIZE : bufferIndex));
}

float EntropyBeaconApp::getCurrentEntropy() const {
    if (bufferIndex == 0) return 0.0f;
    uint16_t last = (bufferIndex - 1 + ENTROPY_BUFFER_SIZE) % ENTROPY_BUFFER_SIZE;
    return entropyBuffer[last] / 4095.0f;
}
