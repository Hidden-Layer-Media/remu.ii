#ifndef HARDWARE_PINS_H
#define HARDWARE_PINS_H

// ========================================
// remu.ii Hardware Pin Configuration - CORRECTED FOR ADAFRUIT ILI9341
// ESP32 + Adafruit ILI9341 2.8" TFT + XPT2046 Touch Controller
// ========================================

// TFT Display Pins (SPI)
#define TFT_CS     5     // Chip Select - CHANGED from 15
#define TFT_DC     2     // Data/Command
#define TFT_RST    16    // Reset - CHANGED from 4
#define TFT_MOSI   23    // SPI Data Out (Master Out Slave In)
#define TFT_SCLK   18    // SPI Clock
#define TFT_MISO   19    // SPI Data In (Master In Slave Out)

// XPT2046 Touch Controller Pins (SPI-based)
#define TOUCH_CS   15    // Touch Chip Select - CHANGED from analog pins
#define TOUCH_IRQ  22    // Touch Interrupt - CHANGED from analog pins
// Touch shares SPI bus with display (MOSI, MISO, SCLK)

// 4-Wire Resistive Touch Pins
// All read pins MUST be on ADC1 (GPIO32-39) - ADC2 is disabled when WiFi is active
#define TOUCH_XP   32    // X+ (ADC1_CH4 - analog read for Y axis)
#define TOUCH_XM   33    // X- (digital output)
#define TOUCH_YP   34    // Y+ (ADC1_CH6 - analog read for X axis, input-only)
#define TOUCH_YM   27    // Y- (digital output)

// SD Card Pins (shares SPI bus with display and touch)
#define SD_CS      4     // SD Card Chip Select - CHANGED from 5

// Power Management
#define BATTERY_PIN 35   // ADC pin for battery voltage monitoring
#define PWR_LED    17    // Power status LED (GPIO0 is boot pin - DO NOT USE)

// Entropy Sources (floating analog pins) - all on ADC1, no overlap with touch pins
#define ENTROPY_PIN_1  36   // ADC1_CH0 (input-only)
#define ENTROPY_PIN_2  39   // ADC1_CH3 (input-only)
#define ENTROPY_PIN_3  33   // ADC1_CH5 - shares with TOUCH_XM but only sampled when touch is idle

// Audio Output - I2S
#define I2S_BCK_PIN   26    // I2S Bit Clock
#define I2S_WS_PIN    25    // I2S Word Select (LRCLK)
#define I2S_DATA_PIN  14    // I2S Data Output

// DAC output pins (ESP32 has two 8-bit DAC channels)
#define DAC_OUT_LEFT   25   // DAC1 - GPIO25
#define DAC_OUT_RIGHT  26   // DAC2 - GPIO26 (also I2S_BCK when I2S active)

// BLE/RF Pins (for external RF modules if needed)
#define RF_CE_PIN     12    // RF module Chip Enable
#define RF_IRQ_PIN    11    // RF module IRQ

// System Control
#define BUZZER_PIN    21    // Optional piezo buzzer
// DEBUG_LED intentionally removed - GPIO2 is TFT_DC, sharing it corrupts display

// Display Configuration
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define SCREEN_ROTATION 1   // Landscape orientation

// Touch Configuration (for XPT2046, not 4-wire resistive)
#define PRESSURE_THRESHOLD  200   // Minimum pressure for valid touch
#define TOUCH_DEBOUNCE_MS   50    // Debounce delay in milliseconds

// SPI Configuration
#define SPI_FREQUENCY   40000000  // 40 MHz SPI clock
#define TFT_SPI_HOST    VSPI_HOST // Use VSPI for display

// Pin conflict detection (compile-time check)
#if TFT_CS == SD_CS
#error "Pin conflict: TFT_CS and SD_CS cannot use the same GPIO"
#endif

#if TFT_CS == TOUCH_CS  
#error "Pin conflict: TFT_CS and TOUCH_CS cannot use the same GPIO"
#endif

#if SD_CS == TOUCH_CS
#error "Pin conflict: SD_CS and TOUCH_CS cannot use the same GPIO" 
#endif

// Memory optimization settings
#define ENABLE_SCREEN_BUFFER false  // Disable large screen buffer to save memory
#define REDUCED_MEMORY_MODE true    // Enable memory-saving features

#endif // HARDWARE_PINS_H
