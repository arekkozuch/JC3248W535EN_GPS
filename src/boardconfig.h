#ifndef BOARDCONFIG_H
#define BOARDCONFIG_H

#define WIFI_SSID "Puchatkova"
#define WIFI_PASSWORD "Internet2@"

// External variable declarations (not definitions)
extern bool debugMode;
extern bool wifiUDPEnabled;

// JC3248W535EN Hardware Pin Definitions
#define BOARD_I2C_SDA       4
#define BOARD_I2C_SCL       8
#define BOARD_SPI_MISO      13
#define BOARD_SPI_MOSI      11
#define BOARD_SPI_SCK       12

// JC3248W535EN Display Configuration
#define BOARD_TFT_WIDTH     320
#define BOARD_TFT_HEIGHT    480
#define BOARD_TFT_BL        1    // Backlight control

// QSPI Display pins (from Arduino_GFX_Library)
#define TFT_QSPI_CS         45
#define TFT_QSPI_SCK        47
#define TFT_QSPI_D0         21   // MOSI
#define TFT_QSPI_D1         48   // MISO  
#define TFT_QSPI_D2         40   // WP
#define TFT_QSPI_D3         39   // HOLD

// Touch Controller (I2C capacitive)
#define TOUCH_ADDR          0x3B
#define TOUCH_I2C_CLOCK     400000
#define BOARD_TOUCH_RST     12
#define BOARD_SENSOR_IRQ    11   // Touch interrupt
#define AXS_MAX_TOUCH_NUMBER 1

// GPS Module (connected to pins 17 & 18 as specified)
#define GNSS_RX             17
#define GNSS_TX             18

// Available GPIO pins for other peripherals
#define BOARD_SD_CS         10   // SD card CS (if available)

// I2C for IMU (can share with touch or use different pins)
#define IMU_I2C_SDA         5    // Available GPIO
#define IMU_I2C_SCL         6    // Available GPIO

// BLE Configuration - External declarations
extern const char* telemetryServiceUUID;
extern const char* telemetryCharUUID;
extern const char* configCharUUID;
extern const char* fileTransferCharUUID;

// System state - External declarations
extern bool sdCardAvailable;
extern bool loggingActive;
extern char currentLogFilename[64];

// IMU Constants
const float MOTION_THRESHOLD = 1.2;
const float IMPACT_THRESHOLD = 2.5;

// MPU6xxx I2C Configuration
#define MPU6xxx_ADDRESS 0x68
#define MPU6xxx_WHO_AM_I 0x75
#define MPU6xxx_PWR_MGMT_1 0x6B
#define MPU6xxx_ACCEL_XOUT_H 0x3B
#define MPU6xxx_GYRO_XOUT_H 0x43
#define MPU6xxx_TEMP_OUT_H 0x41
#define MPU6xxx_ACCEL_CONFIG 0x1C
#define MPU6xxx_GYRO_CONFIG 0x1B

// Performance monitoring - External declarations
extern unsigned long lastPacketTime;
extern unsigned long lastDebugTime;
extern unsigned long lastWiFiCheck;
extern unsigned long lastPerfReset;

// File transfer deferred operations - External declarations
extern volatile bool pendingListFiles;
extern volatile bool pendingStartTransfer;
extern volatile bool pendingDeleteFile;
extern volatile bool pendingCancelTransfer;

// Power Management - JC3248W535EN doesn't have dedicated PMU
#define ADC_BAT             7    // Available ADC pin for battery monitoring

// Additional available GPIO pins on JC3248W535EN
#define GPIO_SPARE_1        9
#define GPIO_SPARE_2        14
#define GPIO_SPARE_3        15
#define GPIO_SPARE_4        16
#define GPIO_SPARE_5        19
#define GPIO_SPARE_6        20

#endif // BOARDCONFIG_H