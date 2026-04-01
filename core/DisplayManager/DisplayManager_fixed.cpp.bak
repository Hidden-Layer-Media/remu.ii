#include "DisplayManager.h"
#include "../SystemCore/SystemCore.h"

// Global instance
DisplayManager displayManager;

DisplayManager::DisplayManager() : 
    tft(nullptr),
    initialized(false),
    brightness(255),
    currentFont(FONT_MEDIUM),
    screenBuffer(nullptr),
    bufferEnabled(false),
    backgroundColor(COLOR_BLACK),
    foregroundColor(COLOR_WHITE)
{
}

DisplayManager::~DisplayManager() {
    shutdown();
}

bool DisplayManager::initialize() {
    Serial.println("[DisplayManager] Initializing ILI9341 display...");
    
    // Initialize TFT display
    tft = new Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
    
    if (!tft) {
        Serial.println("[DisplayManager] ERROR: Failed to create TFT instance");
        return false;
    }
    
    // Initialize SPI and display
    tft->begin();
    tft->setRotation(SCREEN_ROTATION);
    tft->fillScreen(COLOR_BLACK);
    
    // Set initial font
    setFont(FONT_MEDIUM);
    
    // Display boot logo (memory-optimized version)
    drawBootLogoOptimized();
    
    initialized = true;
    Serial.println("[DisplayManager] Display initialized successfully");
    Serial.printf("[DisplayManager] Resolution: %dx%d\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    Serial.printf("[DisplayManager] Free heap after init: %d bytes\n", ESP.getFreeHeap());
    
    return true;
}

void DisplayManager::update() {
    // Memory monitoring
    static unsigned long lastMemCheck = 0;
    if (millis() - lastMemCheck > 5000) { // Check every 5 seconds
        size_t freeHeap = ESP.getFreeHeap();
        if (freeHeap < 10000) { // Less than 10KB
            Serial.printf("[DisplayManager] WARNING: Low memory: %d bytes\n", freeHeap);
        }
        lastMemCheck = millis();
    }
}

void DisplayManager::shutdown() {
    if (tft) {
        tft->fillScreen(COLOR_BLACK);
        delete tft;
        tft = nullptr;
    }
    
    if (screenBuffer) {
        free(screenBuffer);
        screenBuffer = nullptr;
    }
    
    initialized = false;
    Serial.println("[DisplayManager] Display shutdown complete");
}

void DisplayManager::clearScreen(uint16_t color) {
    if (!initialized || !tft) return;
    tft->fillScreen(color);
    backgroundColor = color;
}

void DisplayManager::setBrightness(uint8_t level) {
    brightness = level;
    // Note: ILI9341 doesn't have built-in brightness control
    // This would require PWM on the backlight pin or external circuit
    Serial.printf("[DisplayManager] Brightness set to %d (hardware implementation needed)\n", level);
}

void DisplayManager::setRotation(uint8_t rotation) {
    if (!initialized || !tft) return;
    tft->setRotation(rotation);
}

uint16_t DisplayManager::rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void DisplayManager::setFont(uint8_t font) {
    if (!initialized || !tft) return;
    
    currentFont = font;
    switch (font) {
        case FONT_SMALL:
            tft->setTextSize(1);
            break;
        case FONT_MEDIUM:
            tft->setTextSize(2);
            break;
        case FONT_LARGE:
            tft->setTextSize(3);
            break;
        default:
            tft->setTextSize(2);
            break;
    }
}

void DisplayManager::drawText(int16_t x, int16_t y, String text, uint16_t color) {
    if (!initialized || !tft) return;
    
    tft->setCursor(x, y);
    tft->setTextColor(color);
    tft->print(text);
}

void DisplayManager::drawTextCentered(int16_t x, int16_t y, int16_t w, String text, uint16_t color) {
    if (!initialized || !tft) return;
    
    int16_t textWidth = getTextWidth(text);
    int16_t centeredX = x + (w - textWidth) / 2;
    drawText(centeredX, y, text, color);
}

int16_t DisplayManager::getTextWidth(String text) {
    if (!initialized || !tft) return 0;
    
    int16_t x1, y1;
    uint16_t w, h;
    tft->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return w;
}

int16_t DisplayManager::getTextHeight() {
    if (!initialized || !tft) return 0;
    
    switch (currentFont) {
        case FONT_SMALL: return 8;
        case FONT_MEDIUM: return 16;
        case FONT_LARGE: return 24;
        default: return 16;
    }
}

// Memory-optimized boot logo (no large string arrays)
void DisplayManager::drawBootLogoOptimized() {
    if (!initialized || !tft) return;
    
    clearScreen(COLOR_BLACK);
    
    // Simple text-based logo instead of ASCII art
    setFont(FONT_LARGE);
    drawTextCentered(0, 80, SCREEN_WIDTH, "remu.ii", COLOR_RED_GLOW);
    
    setFont(FONT_MEDIUM);
    drawTextCentered(0, 110, SCREEN_WIDTH, "v1.0", COLOR_GREEN_PHOS);
    
    setFont(FONT_SMALL);
    drawTextCentered(0, 140, SCREEN_WIDTH, "ESP32 Anti-Phone", COLOR_WHITE);
    drawTextCentered(0, 155, SCREEN_WIDTH, "Framework", COLOR_WHITE);
    
    // Simple loading animation
    for (int i = 0; i <= 100; i += 10) {
        drawProgressBar(50, 180, 220, 12, i, COLOR_GREEN_PHOS, COLOR_DARK_GRAY);
        delay(100);
    }
    
    delay(1000);
    clearScreen(COLOR_BLACK);
}

void DisplayManager::drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t progress, uint16_t fillColor, uint16_t bgColor) {
    if (!initialized || !tft) return;
    
    // Background
    tft->fillRect(x, y, w, h, bgColor);
    
    // Border
    tft->drawRect(x, y, w, h, COLOR_DARK_GRAY);
    
    // Progress fill
    int16_t fillWidth = (w - 4) * progress / 100;
    if (fillWidth > 0) {
        tft->fillRect(x + 2, y + 2, fillWidth, h - 4, fillColor);
    }
}

void DisplayManager::drawButton(int16_t x, int16_t y, int16_t w, int16_t h, String text, ButtonState state, uint16_t color) {
    if (!initialized || !tft) return;
    
    // Draw button background
    tft->fillRect(x, y, w, h, color);
    
    // Draw simple border
    uint16_t borderColor = (state == BUTTON_PRESSED) ? COLOR_DARK_GRAY : COLOR_WHITE;
    tft->drawRect(x, y, w, h, borderColor);
    
    // Draw button text
    uint16_t textColor = COLOR_WHITE;
    if (state == BUTTON_DISABLED) {
        textColor = COLOR_LIGHT_GRAY;
    } else if (state == BUTTON_HIGHLIGHTED) {
        textColor = COLOR_RED_GLOW;
    }
    
    setFont(FONT_MEDIUM);
    drawTextCentered(x, y + (h - getTextHeight()) / 2, w, text, textColor);
}

void DisplayManager::drawRetroRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color, bool filled) {
    if (!initialized || !tft) return;
    
    if (filled) {
        tft->fillRect(x, y, w, h, color);
    } else {
        tft->drawRect(x, y, w, h, color);
    }
}

void DisplayManager::drawRetroLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    if (!initialized || !tft) return;
    tft->drawLine(x0, y0, x1, y1, color);
}

void DisplayManager::drawGlitch(int16_t x, int16_t y, int16_t w, int16_t h) {
    if (!initialized || !tft) return;
    
    // Simple glitch effect using random lines
    for (int i = 0; i < 5; i++) {
        int16_t glitchY = y + (systemCore.getRandomByte() % h);
        int16_t glitchW = systemCore.getRandomByte() % (w/2);
        uint16_t glitchColor = (systemCore.getRandomByte() % 2) ? COLOR_RED_GLOW : COLOR_PURPLE_GLOW;
        
        tft->drawFastHLine(x, glitchY, glitchW, glitchColor);
    }
}

void DisplayManager::drawGlowEffect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!initialized || !tft) return;
    
    // Simple glow effect - draw multiple borders
    for (int i = 0; i < 2; i++) {
        tft->drawRect(x - i, y - i, w + 2 * i, h + 2 * i, color);
    }
}

void DisplayManager::drawIcon(int16_t x, int16_t y, const uint8_t* iconData, uint16_t color) {
    if (!initialized || !tft || !iconData) return;
    
    // Draw 16x16 1-bit bitmap
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            int byteIndex = (row * 2) + (col / 8);
            int bitIndex = 7 - (col % 8);
            
            if (iconData[byteIndex] & (1 << bitIndex)) {
                tft->drawPixel(x + col, y + row, color);
            }
        }
    }
}

void DisplayManager::drawASCIIBorder(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!initialized || !tft) return;
    
    setFont(FONT_SMALL);
    
    // Simplified ASCII border
    drawText(x, y, "+", color);
    drawText(x + w - 6, y, "+", color);
    drawText(x, y + h - 8, "+", color);
    drawText(x + w - 6, y + h - 8, "+", color);
    
    // Draw horizontal lines
    for (int i = 6; i < w - 6; i += 6) {
        drawText(x + i, y, "-", color);
        drawText(x + i, y + h - 8, "-", color);
    }
    
    // Draw vertical lines
    for (int i = 8; i < h - 8; i += 8) {
        drawText(x, y + i, "|", color);
        drawText(x + w - 6, y + i, "|", color);
    }
}

// Memory-safe buffer management
void DisplayManager::enableBuffer(bool enable) {
    if (enable && !screenBuffer) {
        // Only allocate a small line buffer instead of full screen
        const size_t lineBufferSize = SCREEN_WIDTH * 2; // One line of 16-bit pixels
        screenBuffer = (uint16_t*)malloc(lineBufferSize);
        if (screenBuffer) {
            bufferEnabled = true;
            Serial.printf("[DisplayManager] Line buffer enabled (%d bytes)\n", lineBufferSize);
        } else {
            Serial.println("[DisplayManager] ERROR: Failed to allocate line buffer");
        }
    } else if (!enable && screenBuffer) {
        free(screenBuffer);
        screenBuffer = nullptr;
        bufferEnabled = false;
        Serial.println("[DisplayManager] Line buffer disabled");
    }
}

void DisplayManager::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (!initialized || !tft) return;
    tft->drawPixel(x, y, color);
}

void DisplayManager::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    if (!initialized || !tft) return;
    tft->drawLine(x0, y0, x1, y1, color);
}