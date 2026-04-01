#ifndef SEQUENCER_FULL_H
#define SEQUENCER_FULL_H

#include "../../core/AppManager/BaseApp.h"
#include "../../core/FileSystem.h"
#include "../../core/Config/hardware_pins.h"
#include <driver/i2s.h>
#include <ArduinoJson.h>

#define SEQ_STEPS    16
#define SEQ_TRACKS   8
#define SEQ_SAMPLE_RATE 22050
#define SEQ_BUF_SIZE    512
#define SEQ_SAMPLE_LEN  2205   // 100ms at 22050Hz

// Grid layout
#define GRID_X       42
#define GRID_Y       22
#define CELL_W       16
#define CELL_H       16
#define TRACK_LBL_W  40
#define CTRL_Y       160

static const char* TRACK_NAMES[SEQ_TRACKS] = {
    "KICK","SNARE","HIHAT","CLAP","BASS","LEAD","PAD","FX"
};

struct SeqTrack {
    bool steps[SEQ_STEPS];
    uint8_t volume;   // 0-127
    bool muted;
    int16_t sample[SEQ_SAMPLE_LEN];
    uint16_t sampleLen;
};

class SequencerApp : public BaseApp {
private:
    SeqTrack tracks[SEQ_TRACKS];
    uint8_t  bpm;
    uint8_t  swing;       // 0-100, 50=no swing
    bool     playing;
    uint8_t  currentStep;
    unsigned long nextStepMs;
    uint8_t  selTrack;
    uint8_t  selPattern;  // future multi-pattern
    bool     audioReady;
    bool     needsGridRedraw;

    // I2S output buffer
    int16_t  mixBuf[SEQ_BUF_SIZE];

    // Active voice state (one voice per track for simplicity)
    struct Voice {
        bool     active;
        uint16_t pos;
        uint8_t  trackIdx;
    } voices[SEQ_TRACKS];

    // ---- Audio ----
    bool initAudio() {
        i2s_config_t cfg = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
            .sample_rate = SEQ_SAMPLE_RATE,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 4,
            .dma_buf_len = SEQ_BUF_SIZE,
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
        if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) { i2s_driver_uninstall(I2S_NUM_0); return false; }
        return true;
    }

    void shutdownAudio() { i2s_driver_uninstall(I2S_NUM_0); }

    void mixAndOutput() {
        memset(mixBuf, 0, sizeof(mixBuf));
        for (int v = 0; v < SEQ_TRACKS; v++) {
            Voice& voice = voices[v];
            if (!voice.active) continue;
            SeqTrack& t = tracks[voice.trackIdx];
            float vol = t.volume / 127.0f;
            for (int i = 0; i < SEQ_BUF_SIZE / 2 && voice.pos < t.sampleLen; i++, voice.pos++) {
                int32_t s = (int32_t)(t.sample[voice.pos] * vol);
                mixBuf[i * 2]     = (int16_t)constrain(mixBuf[i * 2]     + s, -32768, 32767);
                mixBuf[i * 2 + 1] = (int16_t)constrain(mixBuf[i * 2 + 1] + s, -32768, 32767);
            }
            if (voice.pos >= t.sampleLen) voice.active = false;
        }
        size_t written;
        i2s_write(I2S_NUM_0, mixBuf, sizeof(mixBuf), &written, 10);
    }

    void triggerTrack(uint8_t t) {
        if (tracks[t].muted || !tracks[t].sampleLen) return;
        voices[t] = { true, 0, t };
    }

    // ---- Sample synthesis ----
    void genKick(int16_t* buf, uint16_t len) {
        for (uint16_t i = 0; i < len; i++) {
            float t = (float)i / SEQ_SAMPLE_RATE;
            float freq = 150.0f * expf(-30.0f * t);
            float env  = expf(-8.0f * t);
            buf[i] = (int16_t)(sinf(2 * M_PI * freq * t) * env * 28000);
        }
    }
    void genSnare(int16_t* buf, uint16_t len) {
        for (uint16_t i = 0; i < len; i++) {
            float t = (float)i / SEQ_SAMPLE_RATE;
            float env = expf(-15.0f * t);
            float noise = ((float)esp_random() / UINT32_MAX - 0.5f) * 2.0f;
            float tone  = sinf(2 * M_PI * 200.0f * t);
            buf[i] = (int16_t)((noise * 0.7f + tone * 0.3f) * env * 26000);
        }
    }
    void genHihat(int16_t* buf, uint16_t len) {
        uint16_t hlen = min(len, (uint16_t)(SEQ_SAMPLE_RATE / 20)); // 50ms
        for (uint16_t i = 0; i < hlen; i++) {
            float t = (float)i / SEQ_SAMPLE_RATE;
            float env = expf(-60.0f * t);
            float noise = ((float)esp_random() / UINT32_MAX - 0.5f) * 2.0f;
            buf[i] = (int16_t)(noise * env * 20000);
        }
        for (uint16_t i = hlen; i < len; i++) buf[i] = 0;
    }
    void genClap(int16_t* buf, uint16_t len) {
        for (uint16_t i = 0; i < len; i++) {
            float t = (float)i / SEQ_SAMPLE_RATE;
            float env = expf(-25.0f * t) * (1.0f + 0.5f * sinf(2 * M_PI * 8.0f * t));
            float noise = ((float)esp_random() / UINT32_MAX - 0.5f) * 2.0f;
            buf[i] = (int16_t)(noise * env * 24000);
        }
    }
    void genBass(int16_t* buf, uint16_t len) {
        for (uint16_t i = 0; i < len; i++) {
            float t = (float)i / SEQ_SAMPLE_RATE;
            float env = expf(-5.0f * t);
            buf[i] = (int16_t)(sinf(2 * M_PI * 80.0f * t) * env * 28000);
        }
    }
    void genLead(int16_t* buf, uint16_t len) {
        for (uint16_t i = 0; i < len; i++) {
            float t = (float)i / SEQ_SAMPLE_RATE;
            float env = expf(-4.0f * t);
            float s = sinf(2 * M_PI * 440.0f * t) + 0.5f * sinf(2 * M_PI * 880.0f * t);
            buf[i] = (int16_t)(s / 1.5f * env * 22000);
        }
    }
    void genPad(int16_t* buf, uint16_t len) {
        for (uint16_t i = 0; i < len; i++) {
            float t = (float)i / SEQ_SAMPLE_RATE;
            float env = (t < 0.05f) ? t / 0.05f : expf(-1.5f * (t - 0.05f));
            float s = sinf(2 * M_PI * 220.0f * t) + 0.3f * sinf(2 * M_PI * 330.0f * t);
            buf[i] = (int16_t)(s / 1.3f * env * 18000);
        }
    }
    void genFX(int16_t* buf, uint16_t len) {
        for (uint16_t i = 0; i < len; i++) {
            float t = (float)i / SEQ_SAMPLE_RATE;
            float freq = 800.0f + 600.0f * sinf(2 * M_PI * 3.0f * t);
            float env = expf(-3.0f * t);
            float noise = ((float)esp_random() / UINT32_MAX - 0.5f) * 0.3f;
            buf[i] = (int16_t)((sinf(2 * M_PI * freq * t) + noise) * env * 20000);
        }
    }

    void generateBuiltinSamples() {
        typedef void (SequencerApp::*GenFn)(int16_t*, uint16_t);
        GenFn fns[SEQ_TRACKS] = {
            &SequencerApp::genKick,  &SequencerApp::genSnare,
            &SequencerApp::genHihat, &SequencerApp::genClap,
            &SequencerApp::genBass,  &SequencerApp::genLead,
            &SequencerApp::genPad,   &SequencerApp::genFX
        };
        for (int t = 0; t < SEQ_TRACKS; t++) {
            (this->*fns[t])(tracks[t].sample, SEQ_SAMPLE_LEN);
            tracks[t].sampleLen = SEQ_SAMPLE_LEN;
        }
    }

    // ---- Timing ----
    unsigned long stepDurationMs() {
        unsigned long base = 60000UL / (bpm * 4); // 16th notes
        return base;
    }

    void advanceStep() {
        currentStep = (currentStep + 1) % SEQ_STEPS;
        for (int t = 0; t < SEQ_TRACKS; t++) {
            if (tracks[t].steps[currentStep]) triggerTrack(t);
        }
        needsGridRedraw = true;
    }

    // ---- Persistence ----
    void savePattern() {
        filesystem.ensureDirExists("/data/sequencer");
        DynamicJsonDocument doc(2048);
        doc["bpm"] = bpm;
        doc["swing"] = swing;
        JsonArray arr = doc.createNestedArray("steps");
        for (int t = 0; t < SEQ_TRACKS; t++) {
            String row = "";
            for (int s = 0; s < SEQ_STEPS; s++) row += tracks[t].steps[s] ? "1" : "0";
            arr.add(row);
        }
        String out;
        serializeJson(doc, out);
        filesystem.writeFile("/data/sequencer/pattern.json", out);
    }

    void loadPattern() {
        if (!filesystem.fileExists("/data/sequencer/pattern.json")) return;
        String data = filesystem.readFile("/data/sequencer/pattern.json");
        DynamicJsonDocument doc(2048);
        if (deserializeJson(doc, data) != DeserializationError::Ok) return;
        bpm   = doc["bpm"]   | 120;
        swing = doc["swing"] | 50;
        JsonArray arr = doc["steps"];
        for (int t = 0; t < SEQ_TRACKS && t < (int)arr.size(); t++) {
            String row = arr[t].as<String>();
            for (int s = 0; s < SEQ_STEPS && s < (int)row.length(); s++)
                tracks[t].steps[s] = (row[s] == '1');
        }
    }

    // ---- UI ----
    void drawGrid() {
        // Track labels
        displayManager.setFont(FONT_SMALL);
        for (int t = 0; t < SEQ_TRACKS; t++) {
            uint16_t col = tracks[t].muted ? COLOR_DARK_GRAY :
                           (t == selTrack ? COLOR_RED_GLOW : COLOR_GREEN_PHOS);
            displayManager.drawText(2, GRID_Y + t * CELL_H + 3, TRACK_NAMES[t], col);
        }
        // Step cells
        for (int t = 0; t < SEQ_TRACKS; t++) {
            for (int s = 0; s < SEQ_STEPS; s++) {
                int16_t cx = GRID_X + s * CELL_W;
                int16_t cy = GRID_Y + t * CELL_H;
                uint16_t col;
                if (playing && s == currentStep)
                    col = COLOR_WHITE;
                else if (tracks[t].steps[s])
                    col = (t == selTrack) ? COLOR_RED_GLOW : COLOR_GREEN_PHOS;
                else
                    col = (s % 4 == 0) ? COLOR_DARK_GRAY : 0x1082; // subtle beat markers
                displayManager.drawRetroRect(cx + 1, cy + 1, CELL_W - 2, CELL_H - 2, col, tracks[t].steps[s] || (playing && s == currentStep));
                if (!tracks[t].steps[s] && !(playing && s == currentStep))
                    displayManager.drawRetroRect(cx + 1, cy + 1, CELL_W - 2, CELL_H - 2, col, false);
            }
        }
    }

    void drawControls() {
        displayManager.setFont(FONT_SMALL);
        // Transport
        displayManager.drawButton(2,   CTRL_Y,      50, 20, playing ? "STOP" : "PLAY",
                                  playing ? BUTTON_HIGHLIGHTED : BUTTON_NORMAL, COLOR_MID_GRAY);
        // BPM
        char bpmStr[12];
        snprintf(bpmStr, sizeof(bpmStr), "BPM:%d", bpm);
        displayManager.drawButton(56,  CTRL_Y,      60, 20, bpmStr, BUTTON_NORMAL, COLOR_MID_GRAY);
        // BPM +/-
        displayManager.drawButton(120, CTRL_Y,      24, 20, "-",    BUTTON_NORMAL, COLOR_MID_GRAY);
        displayManager.drawButton(148, CTRL_Y,      24, 20, "+",    BUTTON_NORMAL, COLOR_MID_GRAY);
        // Clear track
        displayManager.drawButton(176, CTRL_Y,      60, 20, "CLR",  BUTTON_NORMAL, COLOR_MID_GRAY);
        // Save
        displayManager.drawButton(240, CTRL_Y,      40, 20, "SAVE", BUTTON_NORMAL, COLOR_MID_GRAY);
        // Exit
        displayManager.drawButton(284, CTRL_Y,      34, 20, "EXIT", BUTTON_NORMAL, COLOR_MID_GRAY);

        // Status bar
        char status[48];
        snprintf(status, sizeof(status), "Step:%02d  Trk:%s  Swing:%d",
                 currentStep + 1, TRACK_NAMES[selTrack], swing);
        displayManager.drawText(2, CTRL_Y + 24, status, COLOR_LIGHT_GRAY);
    }

public:
    SequencerApp() : bpm(120), swing(50), playing(false), currentStep(0),
                     nextStepMs(0), selTrack(0), selPattern(0),
                     audioReady(false), needsGridRedraw(true)
    {
        setMetadata("Sequencer","1.0","remu.ii","8-track beat sequencer",CATEGORY_MEDIA,20480);
        for (int t = 0; t < SEQ_TRACKS; t++) {
            tracks[t].volume = 100;
            tracks[t].muted  = false;
            tracks[t].sampleLen = 0;
            memset(tracks[t].steps, 0, sizeof(tracks[t].steps));
        }
        memset(voices, 0, sizeof(voices));
    }

    bool initialize() override {
        Serial.println("[Sequencer] Initializing...");
        generateBuiltinSamples();
        audioReady = initAudio();
        if (!audioReady) Serial.println("[Sequencer] WARNING: I2S init failed, audio disabled");
        loadPattern();
        currentState = APP_RUNNING;
        needsGridRedraw = true;
        return true;
    }

    void update() override {
        if (currentState != APP_RUNNING) return;
        if (audioReady) mixAndOutput();
        if (playing && millis() >= nextStepMs) {
            advanceStep();
            nextStepMs = millis() + stepDurationMs();
        }
    }

    void render() override {
        if (currentState != APP_RUNNING) return;
        if (needsGridRedraw) {
            displayManager.clearScreen(COLOR_BLACK);
            drawGrid();
            drawControls();
            needsGridRedraw = false;
        } else if (playing) {
            // Only redraw grid during playback for step highlight
            drawGrid();
        }
    }

    bool handleTouch(TouchPoint touch) override {
        if (!touch.isNewPress) return true;

        // Grid area
        if (touch.x >= GRID_X && touch.y >= GRID_Y &&
            touch.y < GRID_Y + SEQ_TRACKS * CELL_H) {
            int s = (touch.x - GRID_X) / CELL_W;
            int t = (touch.y - GRID_Y) / CELL_H;
            if (s >= 0 && s < SEQ_STEPS && t >= 0 && t < SEQ_TRACKS) {
                selTrack = t;
                tracks[t].steps[s] = !tracks[t].steps[s];
                needsGridRedraw = true;
            }
            return true;
        }

        // Track label area — mute toggle
        if (touch.x < GRID_X && touch.y >= GRID_Y &&
            touch.y < GRID_Y + SEQ_TRACKS * CELL_H) {
            int t = (touch.y - GRID_Y) / CELL_H;
            if (t >= 0 && t < SEQ_TRACKS) {
                selTrack = t;
                tracks[t].muted = !tracks[t].muted;
                needsGridRedraw = true;
            }
            return true;
        }

        // Controls row
        if (touch.y >= CTRL_Y && touch.y < CTRL_Y + 22) {
            if (touch.x < 54) {
                // PLAY/STOP
                playing = !playing;
                if (playing) { currentStep = SEQ_STEPS - 1; nextStepMs = millis(); }
                needsGridRedraw = true;
            } else if (touch.x < 118) {
                // BPM display — no action
            } else if (touch.x < 144) {
                if (bpm > 60) { bpm--; needsGridRedraw = true; }
            } else if (touch.x < 174) {
                if (bpm < 200) { bpm++; needsGridRedraw = true; }
            } else if (touch.x < 238) {
                // CLR selected track
                memset(tracks[selTrack].steps, 0, sizeof(tracks[selTrack].steps));
                needsGridRedraw = true;
            } else if (touch.x < 282) {
                savePattern();
            } else {
                savePattern();
                cleanup();
                return false;
            }
        }
        return true;
    }

    void cleanup() override {
        playing = false;
        if (audioReady) { shutdownAudio(); audioReady = false; }
        savePattern();
        currentState = APP_EXITING;
    }

    void onPause() override  { playing = false; }
    void onResume() override { needsGridRedraw = true; }

    String getName() const override { return "Sequencer"; }
    void setAppManager(void* m) override {}
};

#endif // SEQUENCER_FULL_H
