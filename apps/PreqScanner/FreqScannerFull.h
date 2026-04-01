#ifndef FREQ_SCANNER_FULL_H
#define FREQ_SCANNER_FULL_H

#include "../../core/AppManager/BaseApp.h"
#include "../../core/FileSystem.h"
#include "../../core/Config/hardware_pins.h"
#include <math.h>
#include <driver/i2s.h>

#define FS_FFT_SIZE     256
#define FS_SAMPLE_RATE  22050
#define FS_BINS         (FS_FFT_SIZE / 2)
#define FS_SPEC_X       0
#define FS_SPEC_Y       20
#define FS_SPEC_W       320
#define FS_SPEC_H       120
#define FS_FALL_Y       142
#define FS_FALL_H       70
#define FS_FALL_LINES   70

enum FSView { FS_VIEW_SPECTRUM, FS_VIEW_WATERFALL, FS_VIEW_DUAL, FS_VIEW_GENERATOR };

class FreqScannerApp : public BaseApp {
private:
    // FFT buffers
    float   real[FS_FFT_SIZE];
    float   imag[FS_FFT_SIZE];
    float   mag[FS_BINS];
    float   smoothed[FS_BINS];
    float   window[FS_FFT_SIZE];

    // Waterfall: ring buffer of lines, each line = FS_SPEC_W pixels (colour)
    uint16_t waterfall[FS_FALL_LINES][FS_SPEC_W];
    uint8_t  wfLine;   // current write line

    // ADC sampling
    uint16_t adcBuf[FS_FFT_SIZE];
    uint16_t adcIdx;
    unsigned long lastSample;
    unsigned long sampleIntervalUs;

    // Signal generator
    bool     genEnabled;
    float    genFreq;
    float    genPhase;
    float    genPhaseInc;
    bool     i2sReady;

    // UI
    FSView   view;
    float    noiseFloor;
    float    peakFreq;
    float    peakMag;
    bool     needsRedraw;
    uint8_t  smoothFactor; // 0-9

    // ---- Hamming window ----
    void buildWindow() {
        for (int i = 0; i < FS_FFT_SIZE; i++)
            window[i] = 0.54f - 0.46f * cosf(2.0f * M_PI * i / (FS_FFT_SIZE - 1));
    }

    // ---- Cooley-Tukey FFT (in-place) ----
    void fft() {
        // Bit-reversal permutation
        int n = FS_FFT_SIZE;
        for (int i = 1, j = 0; i < n; i++) {
            int bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) { float tr = real[i]; real[i] = real[j]; real[j] = tr;
                         float ti = imag[i]; imag[i] = imag[j]; imag[j] = ti; }
        }
        // FFT butterfly
        for (int len = 2; len <= n; len <<= 1) {
            float ang = -2.0f * M_PI / len;
            float wRe = cosf(ang), wIm = sinf(ang);
            for (int i = 0; i < n; i += len) {
                float curRe = 1.0f, curIm = 0.0f;
                for (int j = 0; j < len / 2; j++) {
                    float uRe = real[i+j], uIm = imag[i+j];
                    float vRe = real[i+j+len/2]*curRe - imag[i+j+len/2]*curIm;
                    float vIm = real[i+j+len/2]*curIm + imag[i+j+len/2]*curRe;
                    real[i+j]         = uRe + vRe; imag[i+j]         = uIm + vIm;
                    real[i+j+len/2]   = uRe - vRe; imag[i+j+len/2]   = uIm - vIm;
                    float newRe = curRe*wRe - curIm*wIm;
                    curIm = curRe*wIm + curIm*wRe; curRe = newRe;
                }
            }
        }
    }

    void processFFT() {
        // Load ADC data with window
        for (int i = 0; i < FS_FFT_SIZE; i++) {
            real[i] = ((float)adcBuf[(adcIdx + i) % FS_FFT_SIZE] / 2048.0f - 1.0f) * window[i];
            imag[i] = 0.0f;
        }
        fft();
        // Magnitude in dB
        float alpha = smoothFactor / 10.0f;
        peakMag = -120.0f; peakFreq = 0.0f;
        for (int i = 0; i < FS_BINS; i++) {
            float m = sqrtf(real[i]*real[i] + imag[i]*imag[i]) / FS_FFT_SIZE;
            float db = (m > 1e-10f) ? 20.0f * log10f(m) : -120.0f;
            smoothed[i] = smoothed[i] * alpha + db * (1.0f - alpha);
            mag[i] = smoothed[i];
            if (mag[i] > peakMag) { peakMag = mag[i]; peakFreq = i * (float)FS_SAMPLE_RATE / FS_FFT_SIZE; }
        }
        // Noise floor estimate (median of lower half)
        float sorted[FS_BINS / 2];
        for (int i = 0; i < FS_BINS / 2; i++) sorted[i] = mag[i];
        // Simple insertion sort on small array
        for (int i = 1; i < FS_BINS / 2; i++) {
            float key = sorted[i]; int j = i - 1;
            while (j >= 0 && sorted[j] > key) { sorted[j+1] = sorted[j]; j--; }
            sorted[j+1] = key;
        }
        noiseFloor = sorted[FS_BINS / 4];
    }

    // ---- Waterfall ----
    void updateWaterfall() {
        float minDb = noiseFloor - 10.0f;
        float rangeDb = 60.0f;
        for (int x = 0; x < FS_SPEC_W; x++) {
            int bin = (x * FS_BINS) / FS_SPEC_W;
            float norm = (mag[bin] - minDb) / rangeDb;
            norm = constrain(norm, 0.0f, 1.0f);
            // Map to colour: black -> blue -> green -> yellow -> red
            uint16_t col;
            if (norm < 0.25f) {
                uint8_t b = (uint8_t)(norm * 4 * 31);
                col = b; // blue
            } else if (norm < 0.5f) {
                uint8_t g = (uint8_t)((norm - 0.25f) * 4 * 63);
                col = (g << 5); // green
            } else if (norm < 0.75f) {
                uint8_t r = (uint8_t)((norm - 0.5f) * 4 * 31);
                uint8_t g = 63 - (uint8_t)((norm - 0.5f) * 4 * 63);
                col = (r << 11) | (g << 5);
            } else {
                uint8_t r = 31;
                col = (r << 11);
            }
            waterfall[wfLine][x] = col;
        }
        wfLine = (wfLine + 1) % FS_FALL_LINES;
    }

    // ---- I2S generator ----
    bool initI2S() {
        i2s_config_t cfg = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
            .sample_rate = FS_SAMPLE_RATE,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 4,
            .dma_buf_len = 256,
            .use_apll = false,
            .tx_desc_auto_clear = true
        };
        i2s_pin_config_t pins = {
            .bck_io_num   = I2S_BCK_PIN,
            .ws_io_num    = I2S_WS_PIN,
            .data_out_num = I2S_DATA_PIN,
            .data_in_num  = I2S_PIN_NO_CHANGE
        };
        if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) return false;
        return i2s_set_pin(I2S_NUM_0, &pins) == ESP_OK;
    }

    void outputGeneratorSamples() {
        int16_t buf[256];
        genPhaseInc = 2.0f * M_PI * genFreq / FS_SAMPLE_RATE;
        for (int i = 0; i < 128; i++) {
            float s = sinf(genPhase) * 28000.0f;
            buf[i*2] = buf[i*2+1] = (int16_t)s;
            genPhase += genPhaseInc;
            if (genPhase > 2.0f * M_PI) genPhase -= 2.0f * M_PI;
        }
        size_t written;
        i2s_write(I2S_NUM_0, buf, sizeof(buf), &written, 5);
    }

    // ---- Rendering ----
    void renderSpectrum() {
        float minDb = noiseFloor - 10.0f;
        float rangeDb = 60.0f;
        for (int x = 0; x < FS_SPEC_W; x++) {
            int bin = (x * FS_BINS) / FS_SPEC_W;
            float norm = (mag[bin] - minDb) / rangeDb;
            norm = constrain(norm, 0.0f, 1.0f);
            int16_t barH = (int16_t)(norm * FS_SPEC_H);
            // Clear column
            displayManager.drawLine(x, FS_SPEC_Y, x, FS_SPEC_Y + FS_SPEC_H, COLOR_BLACK);
            if (barH > 0) {
                uint16_t col = (norm > 0.8f) ? COLOR_RED_GLOW :
                               (norm > 0.5f) ? COLOR_YELLOW : COLOR_GREEN_PHOS;
                displayManager.drawLine(x, FS_SPEC_Y + FS_SPEC_H - barH,
                                        x, FS_SPEC_Y + FS_SPEC_H, col);
            }
        }
        // Peak label
        char buf[32];
        snprintf(buf, sizeof(buf), "Peak:%.0fHz %.0fdB", peakFreq, peakMag);
        displayManager.setFont(FONT_SMALL);
        displayManager.drawText(2, FS_SPEC_Y + 2, buf, COLOR_WHITE);
    }

    void renderWaterfall() {
        for (int row = 0; row < FS_FALL_H && row < FS_FALL_LINES; row++) {
            int srcLine = (wfLine - 1 - row + FS_FALL_LINES) % FS_FALL_LINES;
            for (int x = 0; x < FS_SPEC_W; x++) {
                uint16_t col = waterfall[srcLine][x];
                if (col) displayManager.drawPixel(x, FS_FALL_Y + row, col);
            }
        }
    }

    void renderHeader() {
        displayManager.drawRetroRect(0, 0, SCREEN_WIDTH, 20, COLOR_DARK_GRAY, true);
        displayManager.setFont(FONT_SMALL);
        char buf[48];
        snprintf(buf, sizeof(buf), "FreqScan  NF:%.0fdB  Sm:%d",
                 noiseFloor, smoothFactor);
        displayManager.drawText(4, 5, buf, COLOR_GREEN_PHOS);
    }

    void renderFooter() {
        int16_t fy = SCREEN_HEIGHT - 20;
        displayManager.drawRetroRect(0, fy, SCREEN_WIDTH, 20, COLOR_DARK_GRAY, true);
        displayManager.setFont(FONT_SMALL);
        const char* viewNames[] = {"SPEC","FALL","DUAL","GEN"};
        displayManager.drawText(4,   fy + 6, viewNames[view], COLOR_YELLOW);
        displayManager.drawText(60,  fy + 6, "SM-",  COLOR_WHITE);
        displayManager.drawText(90,  fy + 6, "SM+",  COLOR_WHITE);
        if (view == FS_VIEW_GENERATOR) {
            char gBuf[20];
            snprintf(gBuf, sizeof(gBuf), "%.0fHz", genFreq);
            displayManager.drawText(130, fy + 6, gBuf, genEnabled ? COLOR_GREEN_PHOS : COLOR_LIGHT_GRAY);
            displayManager.drawText(200, fy + 6, genEnabled ? "GEN:ON" : "GEN:OFF", COLOR_WHITE);
        }
        displayManager.drawText(280, fy + 6, "EXIT", COLOR_WHITE);
    }

public:
    FreqScannerApp() :
        adcIdx(0), lastSample(0), sampleIntervalUs(1000000UL / FS_SAMPLE_RATE),
        genEnabled(false), genFreq(440.0f), genPhase(0.0f), genPhaseInc(0.0f),
        i2sReady(false), view(FS_VIEW_SPECTRUM), noiseFloor(-80.0f),
        peakFreq(0.0f), peakMag(-120.0f), needsRedraw(true), smoothFactor(7)
    {
        setMetadata("FreqScanner","1.0","remu.ii","FFT spectrum analyzer",CATEGORY_TOOLS,16384);
        memset(adcBuf, 0, sizeof(adcBuf));
        memset(smoothed, 0, sizeof(smoothed));
        memset(waterfall, 0, sizeof(waterfall));
        wfLine = 0;
    }

    bool initialize() override {
        Serial.println("[FreqScanner] Initializing...");
        buildWindow();
        analogReadResolution(12);
        analogSetAttenuation(ADC_11db);
        i2sReady = initI2S();
        if (!i2sReady) Serial.println("[FreqScanner] I2S unavailable, generator disabled");
        currentState = APP_RUNNING;
        return true;
    }

    void update() override {
        if (currentState != APP_RUNNING) return;

        // Sample ADC at target rate
        unsigned long now = micros();
        if (now - lastSample >= sampleIntervalUs) {
            adcBuf[adcIdx] = analogRead(ENTROPY_PIN_1);
            adcIdx = (adcIdx + 1) % FS_FFT_SIZE;
            lastSample = now;
        }

        // Process FFT every full buffer
        static uint16_t sampleCtr = 0;
        if (++sampleCtr >= FS_FFT_SIZE) {
            sampleCtr = 0;
            processFFT();
            if (view == FS_VIEW_WATERFALL || view == FS_VIEW_DUAL) updateWaterfall();
            needsRedraw = true;
        }

        if (genEnabled && i2sReady) outputGeneratorSamples();
    }

    void render() override {
        if (currentState != APP_RUNNING || !needsRedraw) return;
        needsRedraw = false;

        renderHeader();

        switch (view) {
            case FS_VIEW_SPECTRUM:
                renderSpectrum();
                break;
            case FS_VIEW_WATERFALL:
                displayManager.drawRetroRect(0, FS_SPEC_Y, SCREEN_WIDTH, FS_SPEC_H, COLOR_BLACK, true);
                renderWaterfall();
                break;
            case FS_VIEW_DUAL:
                renderSpectrum();
                renderWaterfall();
                break;
            case FS_VIEW_GENERATOR: {
                displayManager.drawRetroRect(0, FS_SPEC_Y, SCREEN_WIDTH, FS_SPEC_H + FS_FALL_H, COLOR_BLACK, true);
                displayManager.setFont(FONT_MEDIUM);
                displayManager.drawTextCentered(0, 80, SCREEN_WIDTH, "Signal Generator", COLOR_GREEN_PHOS);
                char buf[32];
                snprintf(buf, sizeof(buf), "Freq: %.1f Hz", genFreq);
                displayManager.drawTextCentered(0, 110, SCREEN_WIDTH, buf, COLOR_WHITE);
                displayManager.drawTextCentered(0, 130, SCREEN_WIDTH,
                    genEnabled ? "OUTPUT: ON" : "OUTPUT: OFF",
                    genEnabled ? COLOR_GREEN_PHOS : COLOR_LIGHT_GRAY);
                displayManager.setFont(FONT_SMALL);
                displayManager.drawText(10, 160, "Tap left/right of freq to adjust", COLOR_LIGHT_GRAY);
                break;
            }
        }

        renderFooter();
    }

    bool handleTouch(TouchPoint touch) override {
        if (!touch.isNewPress) return true;

        int16_t fy = SCREEN_HEIGHT - 20;

        // Footer
        if (touch.y >= fy) {
            if (touch.x < 55) {
                view = (FSView)((view + 1) % 4);
                needsRedraw = true;
            } else if (touch.x < 80) {
                if (smoothFactor > 0) { smoothFactor--; needsRedraw = true; }
            } else if (touch.x < 110) {
                if (smoothFactor < 9) { smoothFactor++; needsRedraw = true; }
            } else if (view == FS_VIEW_GENERATOR && touch.x < 230) {
                genEnabled = !genEnabled;
                if (!genEnabled && i2sReady) i2s_zero_dma_buffer(I2S_NUM_0);
                needsRedraw = true;
            } else if (touch.x >= 270) {
                cleanup();
                return false;
            }
            return true;
        }

        // Generator frequency adjust
        if (view == FS_VIEW_GENERATOR && touch.y > FS_SPEC_Y && touch.y < FS_FALL_Y) {
            if (touch.x < SCREEN_WIDTH / 2) {
                genFreq = max(20.0f, genFreq - 50.0f);
            } else {
                genFreq = min(10000.0f, genFreq + 50.0f);
            }
            needsRedraw = true;
        }
        return true;
    }

    void cleanup() override {
        genEnabled = false;
        if (i2sReady) { i2s_driver_uninstall(I2S_NUM_0); i2sReady = false; }
        currentState = APP_EXITING;
    }

    void onPause() override  { genEnabled = false; }
    void onResume() override { needsRedraw = true; }

    String getName() const override { return "FreqScanner"; }
    void setAppManager(void* m) override {}
};

#endif // FREQ_SCANNER_FULL_H
