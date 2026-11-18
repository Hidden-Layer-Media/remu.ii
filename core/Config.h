#ifndef CONFIG_H
#define CONFIG_H

// ========================================
// remu.ii System Configuration
// Global constants and configuration values
// ========================================

// ========================================
// HARDWARE CONFIGURATION
// ========================================

// Display Configuration
#define SCREEN_WIDTH        320
#define SCREEN_HEIGHT       240
#define SCREEN_ROTATION     1       // Landscape orientation
#define DEFAULT_DISPLAY_BRIGHTNESS  200     // 0-255, default brightness
#define BACKLIGHT_TIMEOUT   30000   // Auto-dim timeout (ms)

// Touch Interface Configuration
#define TOUCH_PRESSURE_MIN      200     // Minimum pressure for valid touch
#define TOUCH_DEBOUNCE_MS       50      // Touch debounce delay
#define TOUCH_HOLD_TIME_MS      500     // Long press detection time
#define TOUCH_DOUBLE_TAP_MS     300     // Double tap detection window
#define TOUCH_CALIBRATION_POINTS 5      // Number of calibration points

// ========================================
// MEMORY MANAGEMENT
// ========================================

// Application Limits
#define MAX_APPS                16      // Maximum concurrent apps
#define MAX_APP_MEMORY          32768   // Max memory per app (32KB)
#define SYSTEM_MEMORY_RESERVE   16384   // Reserved system memory (16KB)
#define LOW_MEMORY_THRESHOLD    8192    // Low memory warning (8KB)
#define CRITICAL_MEMORY_THRESHOLD 4096  // Critical memory level (4KB)

// Buffer Sizes
#define DISPLAY_BUFFER_SIZE     1024    // Display rendering buffer
#define TOUCH_BUFFER_SIZE       64      // Touch event buffer
#define FILE_BUFFER_SIZE        512     // File I/O buffer
#define STRING_BUFFER_SIZE      256     // General string operations

// ========================================
// AUDIO SYSTEM CONFIGURATION
// ========================================

// Audio Sample Rates and Quality
#define AUDIO_SAMPLE_RATE_44K   44100   // CD quality
#define AUDIO_SAMPLE_RATE_22K   22050   // Standard quality (default)
#define AUDIO_SAMPLE_RATE_11K   11025   // Low quality
#define AUDIO_DEFAULT_SAMPLE_RATE AUDIO_SAMPLE_RATE_22K

// Audio Buffers
#define AUDIO_BUFFER_SIZE       512     // Main audio buffer
#define AUDIO_DMA_BUFFER_COUNT  2       // Number of DMA buffers
#define AUDIO_MAX_CHANNELS      2       // Stereo output
#define AUDIO_BIT_DEPTH         16      // 16-bit audio

// Audio Processing
#define AUDIO_VOLUME_MAX        127     // Maximum volume level
#define AUDIO_VOLUME_DEFAULT    80      // Default volume level
#define AUDIO_FADE_STEPS        32      // Steps for volume fading
#define MAX_SIMULTANEOUS_SAMPLES 4      // Max concurrent audio samples

// DAC Configuration
#define DAC_RESOLUTION          8       // 8-bit DAC resolution
#define PWM_FREQUENCY           22050   // PWM frequency for audio
#define PWM_RESOLUTION          8       // PWM resolution bits

// ========================================
// FILE SYSTEM CONFIGURATION
// ========================================

// Path Limits
#define MAX_FILENAME_LENGTH     64      // Maximum filename length
#define MAX_PATH_LENGTH         256     // Maximum full path length
#define MAX_DIRECTORY_DEPTH     8       // Maximum directory nesting

// Standard Directories
#define APPS_DIR                "/apps"
#define DATA_DIR                "/data"
#define SAMPLES_DIR             "/samples"
#define SETTINGS_DIR            "/settings"
#define TEMP_DIR                "/temp"
#define LOGS_DIR                "/logs"

// File Extensions
#define CONFIG_FILE_EXT         ".cfg"
#define LOG_FILE_EXT            ".log"
#define SAMPLE_FILE_EXT         ".wav"
#define PROJECT_FILE_EXT        ".proj"

// File Limits
#define MAX_OPEN_FILES          8       // Maximum simultaneous open files
#define MAX_LOG_FILE_SIZE       65536   // 64KB max log file
#define MAX_CONFIG_FILE_SIZE    4096    // 4KB max config file

// ========================================
// WIRELESS COMMUNICATION
// ========================================

// BLE Scanner Configuration
#define BLE_SCAN_DURATION_SEC   10      // Default scan duration
#define BLE_SCAN_INTERVAL       0x50    // BLE scan interval (50ms)
#define BLE_SCAN_WINDOW         0x30    // BLE scan window (30ms)
#define BLE_RSSI_THRESHOLD      -70     // Minimum RSSI to report (dBm)
#define BLE_MAX_DEVICES         32      // Maximum devices to track
#define BLE_DEVICE_TIMEOUT      30000   // Device timeout (ms)
#define BLE_NAME_MAX_LENGTH     32      // Maximum BLE device name length

// WiFi Configuration
#define WIFI_SCAN_TIMEOUT       15000   // WiFi scan timeout (ms)
#define WIFI_CONNECT_TIMEOUT    20000   // WiFi connection timeout (ms)
#define WIFI_MAX_NETWORKS       32      // Maximum networks to store
#define WIFI_SSID_MAX_LENGTH    32      // Maximum SSID length
#define WIFI_PASSWORD_MAX_LENGTH 64     // Maximum password length
#define WIFI_RSSI_THRESHOLD     -80     // Minimum RSSI to display (dBm)

// RF Frequency Configuration
#define RF_FREQ_MIN_MHZ         315     // Minimum RF frequency (MHz)
#define RF_FREQ_MAX_MHZ         928     // Maximum RF frequency (MHz)
#define RF_FREQ_STEP_KHZ        25      // Frequency step size (kHz)
#define RF_SCAN_DWELL_MS        100     // Time per frequency (ms)
#define RF_SIGNAL_THRESHOLD     -90     // Signal detection threshold (dBm)

// ========================================
// APPLICATION FRAMEWORK
// ========================================

// App Manager Configuration
#define LAUNCHER_GRID_COLS      4       // Launcher grid columns
#define LAUNCHER_GRID_ROWS      4       // Launcher grid rows
#define LAUNCHER_ICON_SIZE      64      // App icon size (pixels)
#define LAUNCHER_ICON_SPACING   80      // Icon spacing (pixels)
#define APP_TRANSITION_SPEED    8       // Transition animation speed
#define APP_LOAD_TIMEOUT        5000    // App loading timeout (ms)

// UI Configuration
#define UI_FONT_SIZE_SMALL      1       // Small font multiplier
#define UI_FONT_SIZE_MEDIUM     2       // Medium font multiplier
#define UI_FONT_SIZE_LARGE      3       // Large font multiplier
#define UI_MARGIN_SMALL         4       // Small UI margin
#define UI_MARGIN_MEDIUM        8       // Medium UI margin
#define UI_MARGIN_LARGE         16      // Large UI margin

// Color Scheme (16-bit RGB565)
#define COLOR_BLACK             0x0000
#define COLOR_WHITE             0xFFFF
#define COLOR_RED               0xF800
#define COLOR_GREEN             0x07E0
#define COLOR_BLUE              0x001F
#define COLOR_YELLOW            0xFFE0
#define COLOR_CYAN              0x07FF
#define COLOR_MAGENTA           0xF81F
#define COLOR_GRAY_LIGHT        0xC618
#define COLOR_GRAY_DARK         0x4208
#define COLOR_BACKGROUND        0x0000  // Black background
#define COLOR_FOREGROUND        0xFFFF  // White foreground
#define COLOR_ACCENT            0x07E0  // Green accent
#define COLOR_WARNING           0xFFE0  // Yellow warning
#define COLOR_ERROR             0xF800  // Red error

// ========================================
// SEQUENCER CONFIGURATION
// ========================================

// Grid Configuration
#define SEQUENCER_COLS          16      // Number of steps
#define SEQUENCER_ROWS          8       // Number of tracks
#define MAX_SEQUENCER_TRACKS    8       // Maximum tracks
#define MAX_SEQUENCER_PATTERNS  16      // Maximum patterns
#define MAX_SAMPLES_PER_TRACK   32      // Maximum samples per track

// Timing Configuration
#define MIN_BPM                 60      // Minimum BPM
#define MAX_BPM                 200     // Maximum BPM
#define DEFAULT_BPM             120     // Default BPM
#define BPM_STEP                5       // BPM adjustment step
#define SWING_MIN               0       // Minimum swing (%)
#define SWING_MAX               100     // Maximum swing (%)
#define DEFAULT_SWING           50      // Default swing (no swing)

// Sample Configuration
#define MAX_SAMPLE_LENGTH_SEC   4       // Maximum sample length (seconds)
#define SAMPLE_FADE_MS          10      // Sample fade in/out time
#define SAMPLE_CACHE_SIZE       4       // Number of cached samples

// ========================================
// SYSTEM MONITORING
// ========================================

// Battery Monitoring
#define BATTERY_VOLTAGE_MIN     3.0     // Minimum battery voltage
#define BATTERY_VOLTAGE_MAX     4.2     // Maximum battery voltage
#define BATTERY_LOW_THRESHOLD   3.3     // Low battery warning
#define BATTERY_CRITICAL_THRESHOLD 3.1  // Critical battery level
#define BATTERY_CHECK_INTERVAL  30000   // Battery check interval (ms)

// Performance Monitoring
#define FPS_TARGET              30      // Target FPS
#define FRAME_TIME_MS           33      // Target frame time (33ms = 30fps)
#define PERFORMANCE_LOG_INTERVAL 10000  // Performance logging interval
#define MAX_FRAME_TIME_MS       100     // Maximum acceptable frame time

// System Limits
#define MAX_ERROR_COUNT         10      // Maximum errors before restart
#define WATCHDOG_TIMEOUT_SEC    30      // Watchdog timeout
#define SYSTEM_TICK_MS          10      // System tick interval
#define IDLE_TIMEOUT_SEC        300     // System idle timeout (5 minutes)

// ========================================
// ENTROPY AND RANDOMNESS
// ========================================

// Entropy Sources
#define ENTROPY_POOL_SIZE       256     // Entropy pool size (bytes)
#define ENTROPY_REFRESH_MS      1000    // Entropy refresh interval
#define ADC_ENTROPY_SAMPLES     16      // ADC samples for entropy
#define ENTROPY_QUALITY_MIN     80      // Minimum entropy quality (%)

// Random Beacon Configuration
#define BEACON_INTERVAL_MS      10000   // Beacon broadcast interval
#define BEACON_DATA_SIZE        32      // Beacon data size (bytes)
#define BEACON_TX_POWER         0       // Transmission power (0dBm)

// ========================================
// DEBUG AND LOGGING
// ========================================

// Debug Levels
#define LOG_LEVEL_NONE          0       // No logging
#define LOG_LEVEL_ERROR         1       // Errors only
#define LOG_LEVEL_WARN          2       // Warnings and errors
#define LOG_LEVEL_INFO          3       // Info, warnings, errors
#define LOG_LEVEL_DEBUG         4       // All messages
#define DEFAULT_LOG_LEVEL       LOG_LEVEL_INFO

// Debug Configuration
#define SERIAL_BAUD_RATE        115200  // Serial port baud rate
#define DEBUG_BUFFER_SIZE       512     // Debug message buffer
#define LOG_ROTATION_SIZE       32768   // Log file rotation size
#define MAX_LOG_FILES           3       // Maximum log files to keep

// ========================================
// VERSION INFORMATION
// ========================================

#define FIRMWARE_VERSION_MAJOR  1
#define FIRMWARE_VERSION_MINOR  0
#define FIRMWARE_VERSION_PATCH  0
#define FIRMWARE_VERSION_STRING "1.0.0"
#define HARDWARE_VERSION        "remu.ii-v1"
#define BUILD_DATE              __DATE__
#define BUILD_TIME              __TIME__

#endif // CONFIG_H