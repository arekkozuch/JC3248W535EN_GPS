#define WIFI_SSID "Puchatkova"
#define WIFI_PASSWORD "Internet2@"
#define boardtype ""//T-Display-S3-Pro, JC3248W535EN


bool debugMode = false;

// T-Display-S3-Pro Hardware Pin Definitions
if (boardtype == "T-Display-S3-Pro") {
#define BOARD_I2C_SDA       5
#define BOARD_I2C_SCL       6
#define BOARD_SPI_MISO      8
#define BOARD_SPI_MOSI      17
#define BOARD_SPI_SCK       18
#define BOARD_TFT_CS        39
#define BOARD_TFT_RST       47
#define BOARD_TFT_DC        9
#define BOARD_TFT_BL        48
#define BOARD_SD_CS         14
#define BOARD_SENSOR_IRQ    21
#define BOARD_TOUCH_RST     13
#define TFT_POWER           15
#define ADC_BAT             4
#define PMU_IRQ             21
#define BOARD_TFT_WIDTH     480
#define BOARD_TFT_HEIGHT    222
#define GNSS_RX             43
#define GNSS_TX             44
}
if (boardtype == "JC3248W535EN") {
#define GFX_BL 1
#define TOUCH_ADDR 0x3B
#define TOUCH_SDA 4
#define TOUCH_SCL 8
#define TOUCH_I2C_CLOCK 400000
#define TOUCH_RST_PIN 12
#define TOUCH_INT_PIN 11
#define AXS_MAX_TOUCH_NUMBER 1

}


// BLE Configuration
const char* telemetryServiceUUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const char* telemetryCharUUID    = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
const char* configCharUUID       = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
const char* fileTransferCharUUID = "6e400005-b5a3-f393-e0a9-e50e24dcca9e";

bool sdCardAvailable = false;
bool loggingActive = false;
char currentLogFilename[64] = "";
bool wifiUDPEnabled = false;

// Constants
const float MOTION_THRESHOLD = 1.2;
const float IMPACT_THRESHOLD = 2.5;

// MPU6xxx Direct I2C Functions
#define MPU6xxx_ADDRESS 0x68
#define MPU6xxx_WHO_AM_I 0x75
#define MPU6xxx_PWR_MGMT_1 0x6B
#define MPU6xxx_ACCEL_XOUT_H 0x3B
#define MPU6xxx_GYRO_XOUT_H 0x43
#define MPU6xxx_TEMP_OUT_H 0x41
#define MPU6xxx_ACCEL_CONFIG 0x1C
#define MPU6xxx_GYRO_CONFIG 0x1B

static unsigned long lastPacketTime = 0;
static unsigned long lastDebugTime = 0;
static unsigned long lastWiFiCheck = 0;
static unsigned long lastPerfReset = 0;

volatile bool pendingListFiles = false;
volatile bool pendingStartTransfer = false;
volatile bool pendingDeleteFile = false;
volatile bool pendingCancelTransfer = false;

