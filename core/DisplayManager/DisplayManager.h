#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include "../Config/hardware_pins.h"

// ========================================
// DisplayManager - Retro UI display system for remu.ii
// ILI9341 management with darkweb hacker aesthetic
// ========================================

// Color palette - Darkweb hacker theme
#define COLOR_BLACK         0x0000  // #000000 - Background
#define COLOR_WHITE         0xFFFF  // #FFFFFF - Contrast text
#define COLOR_RED_GLOW      0xF800  // #FF0040 - Primary highlight
#define COLOR_PURPLE_GLOW   0x8010  // #8000FF - Secondary highlight  
#define COLOR_GREEN_PHOS    0x07E0  // #00FF41 - Phosphor green accents
#define COLOR_DARK_GRAY     0x2104  // #202020 - UI borders
#define COLOR_MID_GRAY      0x4208  // #404040 - Button faces
#define COLOR_LIGHT_GRAY    0x8410  // #808080 - Disabled elements
#define COLOR_BLUE_CYBER    0x001F  // #0000FF - Info accents
#define COLOR_YELLOW        0xFFE0  // #FFFF00 - Warning/attention color
#define COLOR_CYAN_GLOW     0x07FF  // #00FFFF - Cyan accent
#define COLOR_ORANGE_GLOW   0xFD20  // #FF6600 - Orange accent
#define COLOR_VERY_DARK_GRAY 0x0841 // #101010 - Near-black grid lines

// UI element dimensions
#define BUTTON_HEIGHT       24
#define TITLE_BAR_HEIGHT    20
#define SCROLL_BAR_WIDTH    12
#define BORDER_WIDTH        2
#define ICON_SIZE           16

// Font configuration
#define FONT_SMALL          1
#define FONT_MEDIUM         2
#define FONT_LARGE          3
#define FONT_TINY           1  // Alias for FONT_SMALL (no smaller built-in font)

// Button states
enum ButtonState {
    BUTTON_NORMAL,
    BUTTON_PRESSED,
    BUTTON_DISABLED,
    BUTTON_HIGHLIGHTED
};

// Window types
enum WindowType {
    WINDOW_NORMAL,
    WINDOW_DIALOG,
    WINDOW_TERMINAL,
    WINDOW_POPUP
};

// UI element structures
struct Button {
    int16_t x, y, w, h;
    String text;
    ButtonState state;
    uint16_t color;
    bool visible;
};

struct Window {
    int16_t x, y, w, h;
    String title;
    WindowType type;
    uint16_t borderColor;
    uint16_t fillColor;
    bool hasTitleBar;
    bool visible;
};

struct ProgressBar {
    int16_t x, y, w, h;
    uint8_t progress;  // 0-100
    uint16_t fillColor;
    uint16_t bgColor;
    bool showText;
};

class DisplayManager {
private:
    Adafruit_ILI9341* tft;
    Adafruit_ILI9341 tftInstance;  // static instance avoids heap allocation
    bool initialized;
    uint8_t brightness;
    uint8_t currentFont;
    
    // Screen buffer management
    uint16_t* screenBuffer;
    bool bufferEnabled;
    
    // UI state
    uint16_t backgroundColor;
    uint16_t foregroundColor;
    
    // Private drawing methods
    void drawBorder3D(int16_t x, int16_t y, int16_t w, int16_t h, bool inset);
    void drawPixelPattern(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t pattern);
    void drawGlowEffect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    
public:
    DisplayManager();
    ~DisplayManager();
    
    // Core initialization and management
    bool initialize();
    void update();
    void shutdown();
    
    // Display control
    void clearScreen(uint16_t color = COLOR_BLACK);
    void setBrightness(uint8_t level);
    uint8_t getBrightness() const { return brightness; }
    void setRotation(uint8_t rotation);
    
    // Color management
    void setBackgroundColor(uint16_t color) { backgroundColor = color; }
    void setForegroundColor(uint16_t color) { foregroundColor = color; }
    uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
    
    // Font and text rendering
    void setFont(uint8_t font);
    void drawText(int16_t x, int16_t y, String text, uint16_t color = COLOR_WHITE);
    void drawTextCentered(int16_t x, int16_t y, int16_t w, String text, uint16_t color = COLOR_WHITE);
    void drawTerminalText(int16_t x, int16_t y, String text, uint16_t color = COLOR_GREEN_PHOS);
    int16_t getTextWidth(String text);
    int16_t getTextHeight();
    
    // Retro UI primitives
    void drawButton(Button& button);
    void drawButton(int16_t x, int16_t y, int16_t w, int16_t h, String text, 
                   ButtonState state = BUTTON_NORMAL, uint16_t color = COLOR_MID_GRAY);
    
    void drawWindow(Window& window);
    void drawWindow(int16_t x, int16_t y, int16_t w, int16_t h, String title = "",
                   WindowType type = WINDOW_NORMAL);
    
    void drawProgressBar(ProgressBar& progressBar);
    void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t progress,
                        uint16_t fillColor = COLOR_GREEN_PHOS, uint16_t bgColor = COLOR_DARK_GRAY);
    
    // Advanced UI elements
    void drawScrollbar(int16_t x, int16_t y, int16_t h, uint8_t position, uint8_t size);
    void drawCheckbox(int16_t x, int16_t y, bool checked, String label = "");
    void drawRadioButton(int16_t x, int16_t y, bool selected, String label = "");
    void drawSlider(int16_t x, int16_t y, int16_t w, uint8_t value, uint8_t min = 0, uint8_t max = 100);
    
    // Icon and sprite rendering
    void drawIcon(int16_t x, int16_t y, const uint8_t* iconData, uint16_t color = COLOR_WHITE);
    void drawSprite(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* spriteData);
    void drawBitmap(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t* bitmap, uint16_t color);
    
    // Special effects
    void drawGlitch(int16_t x, int16_t y, int16_t w, int16_t h);
    void drawScanlines(int16_t x, int16_t y, int16_t w, int16_t h);
    void drawNoise(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t intensity);
    void drawMatrixRain(int16_t x, int16_t y, int16_t w, int16_t h);
    
    // Geometric primitives with retro styling
    void drawRetroLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    void drawRetroRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color, bool filled = false);
    void drawRetroCircle(int16_t x, int16_t y, int16_t r, uint16_t color, bool filled = false);
    
    // ASCII art and terminal styling
    void drawASCIIBorder(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color = COLOR_GREEN_PHOS);
    void drawTerminalCursor(int16_t x, int16_t y, bool blink = true);
    void drawHexDump(int16_t x, int16_t y, const uint8_t* data, size_t length, size_t offset = 0);
    
    // Screen buffer operations
    void enableBuffer(bool enable);
    void swapBuffers();
    void copyToBuffer();
    void copyFromBuffer();
    
    // Utility functions
    void drawTestPattern();
    void drawBootLogo();
    void drawBootLogoOptimized(); // Memory-optimized version
    void drawSystemStats(int16_t x, int16_t y);
    void screenshot(); // Debug function
    
    // Basic drawing primitives
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    
    // Raw Adafruit pass-throughs (for legacy app code that calls display methods directly)
    void fillScreen(uint16_t color) { if (tft) tft->fillScreen(color); }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { if (tft) tft->fillRect(x, y, w, h, color); }
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { if (tft) tft->drawRect(x, y, w, h, color); }
    void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) { if (tft) tft->drawTriangle(x0, y0, x1, y1, x2, y2, color); }
    void setCursor(int16_t x, int16_t y) { if (tft) tft->setCursor(x, y); }
    void setTextColor(uint16_t color) { if (tft) tft->setTextColor(color); }
    void setTextColor(uint16_t fg, uint16_t bg) { if (tft) tft->setTextColor(fg, bg); }
    void setTextSize(uint8_t size) { if (tft) tft->setTextSize(size); }
    void print(const String& s) { if (tft) tft->print(s); }
    void print(const char* s) { if (tft) tft->print(s); }
    void print(int n) { if (tft) tft->print(n); }
    void println(const String& s) { if (tft) tft->println(s); }
    
    // Direct TFT access (use carefully)
    Adafruit_ILI9341* getTFT() { return tft; }
    
    // Singleton-style accessor for legacy app code
    static DisplayManager& getInstance() { return displayManager; }
    
    // Screen dimensions
    int16_t getWidth() const { return SCREEN_WIDTH; }
    int16_t getHeight() const { return SCREEN_HEIGHT; }
};

// Global display manager instance
extern DisplayManager displayManager;

#endif // DISPLAY_MANAGER_H