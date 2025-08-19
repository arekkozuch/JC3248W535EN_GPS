#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>

// System state data
struct SystemData {
    bool displayOn = true;
    bool touchAvailable = false;
    bool mpuAvailable = false;
    bool sdCardAvailable = false;
    bool pmuAvailable = false;
    bool loggingActive = false;
    unsigned long lastDisplayActivity = 0;
    const unsigned long DISPLAY_TIMEOUT = 60000;
    int currentScreen = 0;
    const int MAX_SCREENS = 4;
    uint16_t currentMTU = 23;
};

// GPS data structure
struct GPSData {
    uint32_t timestamp = 0;
    float latitude = 0.0;
    float longitude = 0.0;
    int32_t altitude = 0; // meters
    float speed = 0.0; // km/h
    float heading = 0.0; // degrees
    uint8_t fixType = 0;
    uint8_t satellites = 0;
    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;
};

// IMU data structure
struct IMUData {
    float accelX = 0.0, accelY = 0.0, accelZ = 0.0;
    float gyroX = 0.0, gyroY = 0.0, gyroZ = 0.0;
    float temperature = 0.0;
    float magnitude = 0.0;
    bool motionDetected = false;
    unsigned long lastMotionTime = 0;
    
    // Calibration data
    float accelOffsetX = 0.0;
    float accelOffsetY = 0.0;
    float accelOffsetZ = 0.0;
    float gyroOffsetX = 0.0;
    float gyroOffsetY = 0.0;
    float gyroOffsetZ = 0.0;
    bool isCalibrated = false;
    unsigned long calibrationStartTime = 0;
    int calibrationSamples = 0;
    bool calibrationInProgress = false;
};

// Enhanced battery monitoring with basic ADC monitoring for JC3248W535EN
struct BatteryData {
    float voltage = 0.0;
    float current = 0.0;
    uint8_t percentage = 0;
    bool isCharging = false;
    bool isConnected = false;
    bool usbConnected = false;
    float vbusVoltage = 0.0;
    float systemVoltage = 0.0;
    String chargeStatus = "Unknown";
    unsigned long lastUpdate = 0;
};

// Performance monitoring
struct PerformanceStats {
    unsigned long totalPackets = 0;
    unsigned long droppedPackets = 0;
    unsigned long minDelta = 9999;
    unsigned long maxDelta = 0;
    unsigned long avgDelta = 0;
    unsigned long lastResetTime = 0;
};

// Enhanced File transfer state
struct FileTransferState {
    bool active = false;
    bool listingFiles = false;
    File transferFile;
    String filename = "";
    size_t fileSize = 0;
    size_t bytesSent = 0;
    unsigned long lastChunkTime = 0;
    uint16_t currentMTU = 23;
    bool mtuNegotiated = false;
    float progressPercent = 0.0f;
    unsigned long transferStartTime = 0;
    unsigned long estimatedTimeRemaining = 0;
};

// GPS Packet Structure (42 bytes) for transmission
struct __attribute__((packed)) GPSPacket {
    uint32_t timestamp;      // Unix epoch (4 bytes)
    int32_t latitude;        // deg * 1e7 (4 bytes)
    int32_t longitude;       // deg * 1e7 (4 bytes)
    int32_t altitude;        // mm (4 bytes)
    uint16_t speed;          // mm/s (2 bytes)
    uint32_t heading;        // deg * 1e5 (4 bytes)
    uint8_t fixType;         // 0-5 (1 byte)
    uint8_t satellites;      // count (1 byte)
    uint16_t battery_mv;     // mV (2 bytes)
    uint8_t battery_pct;     // % (1 byte)
    
    int16_t accel_x;         // mg (2 bytes)
    int16_t accel_y;         // mg (2 bytes) 
    int16_t accel_z;         // mg (2 bytes)
    int16_t gyro_x;          // deg/s * 100 (2 bytes)
    int16_t gyro_y;          // deg/s * 100 (2 bytes)
    uint8_t pmu_status;      // PMU status flags (1 byte)
    uint16_t crc;            // CRC16 (2 bytes)
};

// Screen types for the UI
enum ScreenType {
    SCREEN_SPEEDOMETER = 0,
    SCREEN_MOTION = 1,
    SCREEN_SYSTEM = 2,
    SCREEN_PERFORMANCE = 3
};

// Touch zones for portrait mode (320x480)
struct TouchZone {
    uint16_t x, y, w, h;
    String label;
};

// Color definitions for LVGL 8.x (proper format) - DARK THEME
#define UI_COLOR_PRIMARY    lv_color_hex(0x5555FF)  // Blue
#define UI_COLOR_SECONDARY  lv_color_hex(0x55FF55)  // Green  
#define UI_COLOR_ACCENT     lv_color_hex(0xFF5555)  // Red
#define UI_COLOR_WARNING    lv_color_hex(0xFFAA00)  // Orange
#define UI_COLOR_DANGER     lv_color_hex(0xFF0000)  // Red
#define UI_COLOR_SUCCESS    lv_color_hex(0x00AA00)  // Darker Green
#define UI_COLOR_TEXT       lv_color_hex(0xFFFFFF)  // White
#define UI_COLOR_TEXT_MUTED lv_color_hex(0xAAAAAA)  // Gray
#define UI_COLOR_BACKGROUND lv_color_hex(0x000000)  // Black
#define UI_COLOR_SURFACE    lv_color_hex(0x303030)  // Dark Gray
#define UI_COLOR_SURFACE_2  lv_color_hex(0x404040)  // Lighter Gray
#define UI_COLOR_BORDER     lv_color_hex(0x555555)  // Border Gray
#define UI_COLOR_INFO       lv_color_hex(0x0099FF)  // Light Blue
#define UI_COLOR_PURPLE     lv_color_hex(0xAA00FF)  // Purple

// Font sizes - Now using properly enabled LVGL fonts
#define UI_FONT_LARGE       &lv_font_montserrat_28  // Large font
#define UI_FONT_MEDIUM      &lv_font_montserrat_22  // Medium font  
#define UI_FONT_SMALL       &lv_font_montserrat_18  // Small font
#define UI_FONT_EXTRA_LARGE &lv_font_montserrat_48  // Extra large font

// Common UI dimensions for 320x480 portrait display (JC3248W535EN)
#define UI_SCREEN_WIDTH     320
#define UI_SCREEN_HEIGHT    480
#define UI_MARGIN           0
#define UI_HEADER_HEIGHT    40
#define UI_FOOTER_HEIGHT    50
#define UI_BUTTON_HEIGHT    40
#define UI_ICON_SIZE        24

// Status icons using ASCII symbols that display properly
#define ICON_GPS            "G"
#define ICON_WIFI           "W"
#define ICON_BLE            "B"
#define ICON_SD             "S"
#define ICON_BATTERY        "BAT"
#define ICON_MOTION         "MOV"
#define ICON_SPEED          "SPD"
#define ICON_LOCATION       "LOC"
#define ICON_TIME           "TIME"
#define ICON_RECORD         "REC"
#define ICON_STOP           "STOP"
#define ICON_MENU           "MENU"
#define ICON_SYSTEM         "SYS"
#define ICON_PERFORMANCE    "PERF"
#define ICON_PREV           "<"
#define ICON_NEXT           ">"

#endif // DATA_STRUCTURES_H