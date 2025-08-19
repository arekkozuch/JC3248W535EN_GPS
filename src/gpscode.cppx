///=========================================part1
#include <Arduino.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Preferences.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPowersLib.h>
#include <TouchDrvCSTXXX.hpp>
#include <vector>

#include "ui_manager.h"
#include "data_structures.h"

#include "boardconfig.h"

// Hardware objects
TFT_eSPI tft = TFT_eSPI();
SFE_UBLOX_GNSS myGNSS;
Preferences preferences;
HardwareSerial GNSS_Serial(2);
WiFiUDP udp;
PowersSY6970 PMU;
TouchDrvCSTXXX touch;
UIManager uiManager;

// WiFi Configuration
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const IPAddress remoteIP(172, 16, 2, 158);
const uint16_t remotePort = 9000;


BLECharacteristic* telemetryChar = nullptr;
BLECharacteristic* configChar = nullptr;
BLECharacteristic* fileTransferChar = nullptr;
BLE2902* telemetryDescriptor = nullptr;

// Global data structures
SystemData systemData;
GPSData gpsData;
IMUData imuData;
BatteryData batteryData;
PerformanceStats perfStats;
FileTransferState fileTransfer;

// SD Card and Logging
File logFile;


// Constants
//define debug command
void debugPrint(const char* message) {
    if(debugMode) Serial.print(message);
}
void debugPrintln(const String& message) {
    if(debugMode) Serial.println(message);
}
void debugPrintf(const char* format, ...) {
    if(debugMode) {
        va_list args;
        va_start(args, format);
        Serial.printf(format, args);
        va_end(args);
    }
}

String pendingFilename = "";

void writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(MPU6xxx_ADDRESS);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}

uint8_t readRegister(uint8_t reg) {
    Wire.beginTransmission(MPU6xxx_ADDRESS);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU6xxx_ADDRESS, 1, 1);
    return Wire.read();
}

int16_t readRegister16(uint8_t reg) {
    Wire.beginTransmission(MPU6xxx_ADDRESS);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU6xxx_ADDRESS, 2, 1);
    int16_t value = Wire.read() << 8;
    value |= Wire.read();
    return value;
}

// CRC16 calculation
uint16_t crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}


bool calibrateAccelerometer() {
    debugPrintln("🔧 Calibrating accelerometer...");
    
    const int CALIBRATION_SAMPLES = 100;
    const int CALIBRATION_DELAY = 20; // ms between samples
    
    float accelSumX = 0, accelSumY = 0, accelSumZ = 0;
    float gyroSumX = 0, gyroSumY = 0, gyroSumZ = 0;
    
    // Collect calibration samples
    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        // Read accelerometer
        int16_t accelX = readRegister16(MPU6xxx_ACCEL_XOUT_H);
        int16_t accelY = readRegister16(MPU6xxx_ACCEL_XOUT_H + 2);
        int16_t accelZ = readRegister16(MPU6xxx_ACCEL_XOUT_H + 4);
        
        // Read gyroscope
        int16_t gyroX = readRegister16(MPU6xxx_GYRO_XOUT_H);
        int16_t gyroY = readRegister16(MPU6xxx_GYRO_XOUT_H + 2);
        int16_t gyroZ = readRegister16(MPU6xxx_GYRO_XOUT_H + 4);
        
        // Convert to real units
        float accelX_g = accelX / 16384.0;
        float accelY_g = accelY / 16384.0;
        float accelZ_g = accelZ / 16384.0;
        
        float gyroX_dps = gyroX / 131.0;
        float gyroY_dps = gyroY / 131.0;
        float gyroZ_dps = gyroZ / 131.0;
        
        // Accumulate readings
        accelSumX += accelX_g;
        accelSumY += accelY_g;
        accelSumZ += accelZ_g;
        
        gyroSumX += gyroX_dps;
        gyroSumY += gyroY_dps;
        gyroSumZ += gyroZ_dps;
        
        delay(CALIBRATION_DELAY);
    }
    
    // Calculate offsets
    imuData.accelOffsetX = accelSumX / CALIBRATION_SAMPLES;
    imuData.accelOffsetY = accelSumY / CALIBRATION_SAMPLES;
    imuData.accelOffsetZ = (accelSumZ / CALIBRATION_SAMPLES) - 1.0; // Subtract 1g for Z-axis
    
    imuData.gyroOffsetX = gyroSumX / CALIBRATION_SAMPLES;
    imuData.gyroOffsetY = gyroSumY / CALIBRATION_SAMPLES;
    imuData.gyroOffsetZ = gyroSumZ / CALIBRATION_SAMPLES;
    
    imuData.isCalibrated = true;
    
    debugPrintln("✅ IMU calibration complete!");
    debugPrintf("📊 Accel offsets: X=%.4f, Y=%.4f, Z=%.4f\n", 
                  imuData.accelOffsetX, imuData.accelOffsetY, imuData.accelOffsetZ);
    
    return true;
}

// LVGL display flush callback
void lvgl_display_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1);
    tft.pushColors((uint16_t *)&color_p->full, (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1), true);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

// LVGL touch input callback
void lvgl_touch_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    int16_t x, y;
    uint8_t n = touch.getPoint(&x, &y, 1);
    
    if (n) {
        x = map(x, 2, 477, 0, 480);
        y = map(y, 297, 480, 0, 222);
        x = constrain(x, 0, 479);
        y = constrain(y, 0, 221);
        
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PR;
        
        systemData.lastDisplayActivity = millis();
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void initDisplay() {
    debugPrintln("🖥️ Initializing LVGL display...");

    tft.begin();
    tft.setRotation(1);
    
    lv_init();
    
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(BOARD_TFT_WIDTH * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    if (!buf1) {
        debugPrintln("❌ Failed to allocate display buffer");
        return;
    }
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, BOARD_TFT_WIDTH * 40);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = BOARD_TFT_WIDTH;
    disp_drv.ver_res = BOARD_TFT_HEIGHT;
    disp_drv.flush_cb = lvgl_display_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read;
    lv_indev_drv_register(&indev_drv);

    debugPrintln("✅ LVGL display initialized");
}

bool initTouch() {
    debugPrintln("🖱️ Initializing Touch Controller…");

    touch.setPins(BOARD_TOUCH_RST, BOARD_SENSOR_IRQ);
    touch.begin(Wire, CST226SE_SLAVE_ADDRESS, BOARD_I2C_SDA, BOARD_I2C_SCL);

    touch.setMaxCoordinates(BOARD_TFT_HEIGHT, BOARD_TFT_WIDTH); 
    touch.setSwapXY(true);
    touch.setMirrorXY(false, true);

    touch.setHomeButtonCallback([](void*) {
        debugPrintln("Home key pressed!");
        static uint32_t nextToggle = 0;
        if (millis() > nextToggle) {
            systemData.displayOn = !systemData.displayOn;
            digitalWrite(BOARD_TFT_BL, systemData.displayOn);
            nextToggle = millis() + 200;
        }
    }, nullptr);

    systemData.touchAvailable = true;
    debugPrintln("✅ Touch initialized");
    return true;
}
///=========================================part2
bool initPMU() {
    debugPrintln("🔋 Initializing SY6970 Power Management...");
    
    bool result = PMU.init(Wire, SY6970_SLAVE_ADDRESS);
    
    if (!result) {
        debugPrintln("❌ SY6970 PMU not found");
        systemData.pmuAvailable = false;
        return false;
    }
    
    debugPrintf("✅ SY6970 PMU initialized at address 0x%02X\n", SY6970_SLAVE_ADDRESS);
    
    PMU.setChargeTargetVoltage(4208);
    PMU.setPrechargeCurr(128);
    PMU.setChargerConstantCurr(1024);
    PMU.setInputCurrentLimit(2000);
    PMU.enableADCMeasure();
    PMU.disableStatLed();
    PMU.enableCharge();
    
    systemData.pmuAvailable = true;
    debugPrintln("🔋 PMU configured for optimal battery monitoring");
    
    return true;
}

void updateBatteryData() {
    if (!systemData.pmuAvailable || millis() - batteryData.lastUpdate < 5000) return;
    
    batteryData.lastUpdate = millis();
    
    batteryData.voltage = PMU.getBattVoltage() / 1000.0f;
    batteryData.vbusVoltage = PMU.getVbusVoltage() / 1000.0f;
    batteryData.systemVoltage = PMU.getSystemVoltage() / 1000.0f;
    
    if (batteryData.voltage > 2.5f) {
        batteryData.percentage = constrain(map(batteryData.voltage * 100, 300, 420, 0, 100), 0, 100);
    } else {
        batteryData.percentage = 0;
    }
    
    batteryData.usbConnected = PMU.isVbusIn();
    batteryData.isConnected = (batteryData.voltage > 2.5f);
    
    uint8_t chargeStatus = PMU.chargeStatus();
    switch (chargeStatus) {
        case 0x00: batteryData.chargeStatus = "Not Charging"; break;
        case 0x01: batteryData.chargeStatus = "Pre-charge"; break;
        case 0x02: batteryData.chargeStatus = "Fast Charge"; break;
        case 0x03: batteryData.chargeStatus = "Charge Done"; break;
        default: batteryData.chargeStatus = "Unknown"; break;
    }
    
    batteryData.current = 0.0f;
    
    static uint8_t lastBatteryPercent = 255;
    static bool lastChargingState = false;
    if (abs(batteryData.percentage - lastBatteryPercent) > 5 || 
        batteryData.isCharging != lastChargingState) {
        lastBatteryPercent = batteryData.percentage;
        lastChargingState = batteryData.isCharging;
        uiManager.requestUpdate();
    }
}


void updateBatteryLevel() {
        updateBatteryData();
}

bool initSDCard() {
    debugPrintln("📱 Initializing SD card...");
    
    SPI.end();
    delay(100);
    
    SPI.begin(BOARD_SPI_SCK, BOARD_SPI_MISO, BOARD_SPI_MOSI);
    delay(100);
    
    if (!SD.begin(BOARD_SD_CS, SPI, 4000000)) {
        if (!SD.begin(BOARD_SD_CS, SPI, 1000000)) {
            if (!SD.begin(BOARD_SD_CS, SPI, 400000)) {
                debugPrintln("❌ SD card initialization failed");
                return false;
            }
        }
    }
    
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        debugPrintln("❌ No SD card detected");
        return false;
    }
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    debugPrintf("✅ SD Card initialized, Size: %lluMB\n", cardSize);
    
    File testFile = SD.open("/test.tmp", FILE_WRITE);
    if (testFile) {
        testFile.println("GPS Logger Test - " + String(millis()));
        testFile.close();
        SD.remove("/test.tmp");
        return true;
    }
    
    return false;
}

bool initMPU6050() {
    debugPrintln("🔄 Initializing IMU...");
    
    delay(100);
    
    uint8_t whoami = readRegister(MPU6xxx_WHO_AM_I);
    debugPrintf("📋 WHO_AM_I register: 0x%02X\n", whoami);
    
    switch (whoami) {
        case 0x68:
            debugPrintln("✅ Detected: MPU6050");
            break;
        case 0x70:
            debugPrintln("✅ Detected: MPU6000 or MPU9250");
            break;
        case 0x71:
            debugPrintln("✅ Detected: MPU9250");
            break;
        case 0x73:
            debugPrintln("✅ Detected: MPU9255");
            break;
        default:
            debugPrintf("⚠️ Unknown IMU type: 0x%02X (trying anyway...)\n", whoami);
            break;
    }
    
    writeRegister(MPU6xxx_PWR_MGMT_1, 0x00);
    delay(100);
    debugPrintln("✅ MPU woken up from sleep mode");
    
    writeRegister(MPU6xxx_ACCEL_CONFIG, 0x00);
    debugPrintln("✅ Accelerometer range set to ±2g");
    
    writeRegister(MPU6xxx_GYRO_CONFIG, 0x00);
    debugPrintln("✅ Gyroscope range set to ±250°/s");
    
    int16_t accelX = readRegister16(MPU6xxx_ACCEL_XOUT_H);
    int16_t accelY = readRegister16(MPU6xxx_ACCEL_XOUT_H + 2);
    int16_t accelZ = readRegister16(MPU6xxx_ACCEL_XOUT_H + 4);
    int16_t temp = readRegister16(MPU6xxx_TEMP_OUT_H);
    
    float accelX_g = accelX / 16384.0;
    float accelY_g = accelY / 16384.0;
    float accelZ_g = accelZ / 16384.0;
    
    float temp_c;
    if (whoami == 0x68) {
        temp_c = (temp / 340.0) + 36.53;
    } else {
        temp_c = (temp / 333.87) + 21.0;
    }
    
    debugPrintf("✅ Test read successful:\n");
    debugPrintf("   Accel: %.2f, %.2f, %.2f g\n", accelX_g, accelY_g, accelZ_g);
    debugPrintf("   Temperature: %.1f°C\n", temp_c);
    
    systemData.mpuAvailable = true;
    debugPrintln("✅ IMU configured successfully using direct I2C");
    return true;
}

bool configureGNSS() {
    debugPrintln("🛰️ Configuring GNSS...");
    
    myGNSS.setUART1Output(COM_TYPE_UBX);
    myGNSS.setNavigationFrequency(25);
    myGNSS.setAutoPVT(true);
    myGNSS.setDynamicModel(DYN_MODEL_AUTOMOTIVE);
    
    myGNSS.enableGNSS(true, SFE_UBLOX_GNSS_ID_GPS);
    myGNSS.enableGNSS(true, SFE_UBLOX_GNSS_ID_GALILEO);
    
    debugPrintln("✅ GNSS configured");
    return true;
}

void readMPU6050() {
    if (!systemData.mpuAvailable) return;
    
    int16_t accelX = readRegister16(MPU6xxx_ACCEL_XOUT_H);
    int16_t accelY = readRegister16(MPU6xxx_ACCEL_XOUT_H + 2);
    int16_t accelZ = readRegister16(MPU6xxx_ACCEL_XOUT_H + 4);
    
    int16_t gyroX = readRegister16(MPU6xxx_GYRO_XOUT_H);
    int16_t gyroY = readRegister16(MPU6xxx_GYRO_XOUT_H + 2);
    int16_t gyroZ = readRegister16(MPU6xxx_GYRO_XOUT_H + 4);
    
    int16_t temp = readRegister16(MPU6xxx_TEMP_OUT_H);
    
    // Convert to real units
    float rawAccelX = accelX / 16384.0;
    float rawAccelY = accelY / 16384.0;
    float rawAccelZ = accelZ / 16384.0;
    
    float rawGyroX = gyroX / 131.0;
    float rawGyroY = gyroY / 131.0;
    float rawGyroZ = gyroZ / 131.0;
    
    // Apply calibration if available
    if (imuData.isCalibrated) {
        imuData.accelX = rawAccelX - imuData.accelOffsetX;
        imuData.accelY = rawAccelY - imuData.accelOffsetY;
        imuData.accelZ = rawAccelZ - imuData.accelOffsetZ;
        
        imuData.gyroX = rawGyroX - imuData.gyroOffsetX;
        imuData.gyroY = rawGyroY - imuData.gyroOffsetY;
        imuData.gyroZ = rawGyroZ - imuData.gyroOffsetZ;
    } else {
        // Use raw values if not calibrated
        imuData.accelX = rawAccelX;
        imuData.accelY = rawAccelY;
        imuData.accelZ = rawAccelZ;
        
        imuData.gyroX = rawGyroX;
        imuData.gyroY = rawGyroY;
        imuData.gyroZ = rawGyroZ;
    }
    
    // Temperature calculation
    static uint8_t chipType = 0;
    if (chipType == 0) {
        chipType = readRegister(MPU6xxx_WHO_AM_I);
    }
    
    if (chipType == 0x68) {
        imuData.temperature = (temp / 340.0) + 36.53;
    } else {
        imuData.temperature = (temp / 333.87) + 21.0;
    }
    
    // Calculate magnitude using calibrated values
    imuData.magnitude = sqrt(imuData.accelX * imuData.accelX + 
                            imuData.accelY * imuData.accelY + 
                            imuData.accelZ * imuData.accelZ);
    
    // Motion detection with calibrated values
    if (imuData.magnitude > MOTION_THRESHOLD) {
        if (!imuData.motionDetected) {
            debugPrintf("🏃 Motion detected! Magnitude: %.2fg (calibrated)\n", imuData.magnitude);
            uiManager.requestUpdate();
        }
        imuData.motionDetected = true;
        imuData.lastMotionTime = millis();
    } else {
        if (imuData.motionDetected && (millis() - imuData.lastMotionTime > 2000)) {
            imuData.motionDetected = false;
            debugPrintln("😴 Motion stopped");
            uiManager.requestUpdate();
        }
    }
    
    if (imuData.magnitude > IMPACT_THRESHOLD) {
        debugPrintf("💥 IMPACT DETECTED! Magnitude: %.2fg (calibrated)\n", imuData.magnitude);
        uiManager.requestUpdate();
    }
}
////=========================================part3
bool createLogFile() {
    if (!systemData.sdCardAvailable) return false;
    
    sprintf(currentLogFilename, "/gps_%04d%02d%02d_%02d%02d%02d.bin",
        myGNSS.getYear(), myGNSS.getMonth(), myGNSS.getDay(),
        myGNSS.getHour(), myGNSS.getMinute(), myGNSS.getSecond());
    
    logFile = SD.open(currentLogFilename, FILE_WRITE);
    if (!logFile) {
        debugPrintln("❌ Failed to create log file");
        return false;
    }
    
    debugPrintf("📄 Created: %s\n", currentLogFilename);
    
    const char* header = "GPS_LOG_V1.0\n";
    logFile.write((uint8_t*)header, strlen(header));
    logFile.flush();
    
    return true;
}

void toggleLogging() {
    if (systemData.loggingActive) {
        systemData.loggingActive = false;
        if (logFile) {
            logFile.close();
            debugPrintln("⚪ Logging stopped");
        }
    } else {
        if (systemData.sdCardAvailable && myGNSS.getFixType() >= 2) {
            systemData.loggingActive = true;
            if (createLogFile()) {
                debugPrintln("🔴 Logging started");
            } else {
                systemData.loggingActive = false;
                debugPrintln("❌ Failed to create log file");
            }
        }
    }
    
    uiManager.requestUpdate();
}

// DIRECT File Transfer Functions (called from main loop - safe context)
void sendFileResponse(String response) {
    if (!fileTransferChar) {
        debugPrintln("❌ File transfer characteristic not available");
        return;
    }
    
    // Split large responses into chunks (proven working approach)
    int maxChunkSize = 400; // Conservative size that works
    for (int i = 0; i < response.length(); i += maxChunkSize) {
        String chunk = response.substring(i, min(i + maxChunkSize, (int)response.length()));
        fileTransferChar->setValue(chunk.c_str());
        fileTransferChar->notify();
        delay(50); // Small delay between chunks
    }
}

void listSDFiles() {
    if (!systemData.sdCardAvailable) {
        sendFileResponse("ERROR:NO_SD_CARD");
        return;
    }
    
    debugPrintln("📁 Listing SD card files...");
    String fileList = "FILES:";
    File root = SD.open("/");
    if (!root) {
        sendFileResponse("ERROR:CANT_OPEN_ROOT");
        return;
    }
    
    int fileCount = 0;
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String filename = file.name();
            if (filename.endsWith(".bin") || filename.endsWith(".log") || 
                filename.endsWith(".txt") || filename.endsWith(".csv")) {
                fileList += filename + ":" + String(file.size()) + ";";
                fileCount++;
                debugPrintf("📄 Found: %s (%d bytes)\n", filename.c_str(), file.size());
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
    
    fileList += "COUNT:" + String(fileCount);
    sendFileResponse(fileList);
    debugPrintf("📁 File list sent: %d files found\n", fileCount);
    uiManager.requestUpdate();
}

void startFileTransfer(String filename) {
    if (!systemData.sdCardAvailable) {
        sendFileResponse("ERROR:NO_SD_CARD");
        return;
    }
    
    String fullPath = "/" + filename;
    if (!SD.exists(fullPath.c_str())) {
        sendFileResponse("ERROR:FILE_NOT_FOUND:" + filename);
        debugPrintf("❌ File not found: %s\n", filename.c_str());
        return;
    }
    
    // Close any existing transfer
    if (fileTransfer.active && fileTransfer.transferFile) {
        fileTransfer.transferFile.close();
    }
    
    fileTransfer.transferFile = SD.open(fullPath.c_str(), FILE_READ);
    if (!fileTransfer.transferFile) {
        sendFileResponse("ERROR:CANT_OPEN_FILE:" + filename);
        debugPrintf("❌ Cannot open file: %s\n", filename.c_str());
        return;
    }
    
    fileTransfer.active = true;
    fileTransfer.filename = filename;
    fileTransfer.fileSize = fileTransfer.transferFile.size();
    fileTransfer.bytesSent = 0;
    fileTransfer.lastChunkTime = millis();
    fileTransfer.progressPercent = 0.0f;
    fileTransfer.transferStartTime = millis();
    
    // Send file info
    String response = "START:" + filename + ":" + String(fileTransfer.fileSize);
    sendFileResponse(response);
    
    debugPrintf("📤 Starting transfer: %s (%d bytes)\n", filename.c_str(), fileTransfer.fileSize);
    uiManager.requestUpdate();
}

void processFileTransfer() {
    if (!fileTransfer.active || !fileTransfer.transferFile) return;
    
    unsigned long now = millis();
    if (now - fileTransfer.lastChunkTime < 100) return; // Rate limiting
    
    const int chunkSize = 400; // Conservative chunk size for BLE
    uint8_t buffer[chunkSize];
    
    int bytesRead = fileTransfer.transferFile.read(buffer, chunkSize);
    if (bytesRead > 0) {
        // Convert to hex for reliable BLE transmission (from working code)
        String chunk = "CHUNK:";
        for (int i = 0; i < bytesRead; i++) {
            if (buffer[i] < 16) chunk += "0";
            chunk += String(buffer[i], HEX);
            if (chunk.length() > 800) break; // Prevent overrun
        }
        
        chunk += ":SEQ:" + String(fileTransfer.bytesSent / chunkSize);
        
        sendFileResponse(chunk);
        fileTransfer.bytesSent += bytesRead;
        fileTransfer.lastChunkTime = now;
        
        // Update progress
        fileTransfer.progressPercent = (float)fileTransfer.bytesSent / fileTransfer.fileSize * 100.0f;
        
        // Calculate ETA
        unsigned long elapsed = now - fileTransfer.transferStartTime;
        if (elapsed > 2000 && fileTransfer.bytesSent > 0) {
            float bytesPerMs = (float)fileTransfer.bytesSent / elapsed;
            unsigned long remainingBytes = fileTransfer.fileSize - fileTransfer.bytesSent;
            if (bytesPerMs > 0) {
                fileTransfer.estimatedTimeRemaining = remainingBytes / bytesPerMs;
            }
        }
        
        // Progress indicator
        if (fileTransfer.bytesSent % 2048 == 0) {
            debugPrintf("📤 Transfer progress: %.1f%% (%d/%d bytes)\n", 
                         fileTransfer.progressPercent, fileTransfer.bytesSent, fileTransfer.fileSize);
            uiManager.requestUpdate();
        }
    } else {
        // Transfer complete
        fileTransfer.transferFile.close();
        fileTransfer.active = false;
        
        unsigned long totalTime = now - fileTransfer.transferStartTime;
        
        sendFileResponse("COMPLETE:" + String(fileTransfer.bytesSent) + ":TIME:" + String(totalTime));
        debugPrintf("✅ Transfer complete: %s (%d bytes in %.2fs)\n", 
                     fileTransfer.filename.c_str(), fileTransfer.bytesSent, totalTime / 1000.0f);
        
        fileTransfer.progressPercent = 0.0f;
        fileTransfer.estimatedTimeRemaining = 0;
        uiManager.requestUpdate();
    }
}

void deleteFile(String filename) {
    if (!systemData.sdCardAvailable) {
        sendFileResponse("ERROR:NO_SD_CARD");
        return;
    }
    
    String fullPath = "/" + filename;
    if (!SD.exists(fullPath.c_str())) {
        sendFileResponse("ERROR:FILE_NOT_FOUND:" + filename);
        return;
    }
    
    if (SD.remove(fullPath.c_str())) {
        sendFileResponse("DELETED:" + filename);
        debugPrintf("🗑️ Deleted: %s\n", filename.c_str());
    } else {
        sendFileResponse("ERROR:DELETE_FAILED:" + filename);
        debugPrintf("❌ Failed to delete: %s\n", filename.c_str());
    }
    
    uiManager.requestUpdate();
}

void cancelFileTransfer() {
    if (fileTransfer.active) {
        if (fileTransfer.transferFile) {
            fileTransfer.transferFile.close();
        }
        fileTransfer.active = false;
        sendFileResponse("CANCELLED:" + fileTransfer.filename);
        debugPrintf("⏹️ Transfer cancelled: %s\n", fileTransfer.filename.c_str());
        
        fileTransfer.progressPercent = 0.0f;
        fileTransfer.estimatedTimeRemaining = 0;
        uiManager.requestUpdate();
    }
}

// MINIMAL DEFERRED PROCESSING - Called from main loop (safe stack context)
void processDeferredFileOperations() {
    // Process one operation per loop iteration to prevent blocking
    if (pendingListFiles) {
        pendingListFiles = false;
        debugPrintln("🔄 Processing deferred LIST_FILES");
        listSDFiles();
    } else if (pendingStartTransfer) {
        pendingStartTransfer = false;
        debugPrintf("🔄 Processing deferred START_TRANSFER: %s\n", pendingFilename.c_str());
        startFileTransfer(pendingFilename);
        pendingFilename = "";
    } else if (pendingDeleteFile) {
        pendingDeleteFile = false;
        debugPrintf("🔄 Processing deferred DELETE_FILE: %s\n", pendingFilename.c_str());
        deleteFile(pendingFilename);
        pendingFilename = "";
    } else if (pendingCancelTransfer) {
        pendingCancelTransfer = false;
        debugPrintln("🔄 Processing deferred CANCEL_TRANSFER");
        cancelFileTransfer();
    }
}
//=========================================part4
// SIMPLIFIED BLE Callbacks - Direct approach like working code
// MINIMAL BLE Callbacks - ZERO file system operations to prevent stack overflow
class EnhancedConfigCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string stdValue = pCharacteristic->getValue();
        String value = String(stdValue.c_str());
        
        if (value.length() == 0) return;
        
        debugPrintf("📝 Config command: %s\n", value.c_str());
        
        // Handle simple operations immediately (NO file system access)
        if (value == "START_LOG") {
            if (systemData.sdCardAvailable && myGNSS.getFixType() >= 2) {
                systemData.loggingActive = true;
                // Note: createLogFile() will be called from main loop during next GPS packet
                uiManager.requestUpdate();
                debugPrintln("🔴 Logging started via BLE");
            }
        } else if (value == "STOP_LOG") {
            systemData.loggingActive = false;
            if (logFile) logFile.close();
            uiManager.requestUpdate();
            debugPrintln("⚪ Logging stopped via BLE");
        } 
        // DEFER file operations with simple flags - NO file system access in callback
        else if (value == "LIST_FILES") {
            pendingListFiles = true;
            debugPrintln("📝 Queued LIST_FILES for deferred processing");
        } else if (value.startsWith("DOWNLOAD:")) {
            pendingFilename = value.substring(9);
            pendingStartTransfer = true;
            debugPrintf("📝 Queued START_TRANSFER for: %s\n", pendingFilename.c_str());
        } else if (value.startsWith("DELETE:")) {
            pendingFilename = value.substring(7);
            pendingDeleteFile = true;
            debugPrintf("📝 Queued DELETE_FILE for: %s\n", pendingFilename.c_str());
        } else if (value == "CANCEL_TRANSFER") {
            pendingCancelTransfer = true;
            debugPrintln("📝 Queued CANCEL_TRANSFER");
        } else if (value.startsWith("SET_MTU:")) {
            uint16_t mtu = value.substring(8).toInt();
            if (mtu >= 23 && mtu <= 512) {
                fileTransfer.currentMTU = mtu;
                fileTransfer.mtuNegotiated = true;
                debugPrintf("📡 MTU set to: %d\n", mtu);
            }
        }
        // Callback returns immediately - ZERO file system operations!
    }
};

class EnhancedFileTransferCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string stdValue = pCharacteristic->getValue();
        String value = String(stdValue.c_str());
        
        debugPrintf("📤 File transfer command: %s\n", value.c_str());
        
        // DEFER all file operations with simple flags - NO file system access in callback
        if (value == "LIST") {
            pendingListFiles = true;
            debugPrintln("📤 Queued LIST for deferred processing");
        } else if (value.startsWith("GET:")) {
            pendingFilename = value.substring(4);
            pendingStartTransfer = true;
            debugPrintf("📤 Queued GET for: %s\n", pendingFilename.c_str());
        } else if (value.startsWith("DEL:")) {
            pendingFilename = value.substring(4);
            pendingDeleteFile = true;
            debugPrintf("📤 Queued DEL for: %s\n", pendingFilename.c_str());
        } else if (value == "STOP" || value == "CANCEL") {
            pendingCancelTransfer = true;
            debugPrintln("📤 Queued CANCEL");
        } else if (value == "STATUS") {
            // STATUS is safe - no file system access, just memory reads
            String status = "STATUS:";
            if (fileTransfer.active) {
                status += "ACTIVE:" + fileTransfer.filename + ":" + 
                         String((int)fileTransfer.progressPercent);
            } else {
                status += "IDLE";
            }
            
            // Send response immediately - no file system access
            if (fileTransferChar) {
                fileTransferChar->setValue(status.c_str());
                fileTransferChar->notify();
            }
        }
        // Callback returns immediately - ZERO file system operations!
    }
};

class EnhancedServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        debugPrintln("📱 BLE Client connected");
        fileTransfer.mtuNegotiated = false;
        fileTransfer.currentMTU = 23;
        uiManager.requestUpdate();
    }
    
    void onDisconnect(BLEServer* pServer) {
        debugPrintln("📱 BLE Client disconnected");
        
        // Cancel any active transfer using deferred operation
        if (fileTransfer.active) {
            pendingCancelTransfer = true;
            debugPrintln("📱 Queued transfer cancellation due to disconnect");
        }
        
        BLEDevice::startAdvertising();
        uiManager.requestUpdate();
    }
    
    void onMtuChanged(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) {
        // Handle different ESP32 BLE library versions
        uint16_t mtu = 23; // default fallback
        
        #ifdef ESP_IDF_VERSION_MAJOR
            #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
                if (param->mtu.mtu > 0) {
                    mtu = param->mtu.mtu;
                }
            #else
                if (param->mtu_changed.mtu > 0) {
                    mtu = param->mtu_changed.mtu;
                }
            #endif
        #else
            mtu = 247; // Common negotiated MTU size
        #endif
        
        fileTransfer.currentMTU = mtu;
        fileTransfer.mtuNegotiated = true;
        debugPrintf("📡 MTU negotiated: %d bytes\n", fileTransfer.currentMTU);
    }
};
//=========================================part5
void setup() {
    Serial.begin(115200);
    delay(3000);
    Serial.println("🚀 T-Display-S3-Pro GPS Logger v5.1 Starting...");
    
    // Initialize display first
    pinMode(TFT_POWER, OUTPUT);
    digitalWrite(TFT_POWER, HIGH);
    pinMode(BOARD_TFT_BL, OUTPUT);
    digitalWrite(BOARD_TFT_BL, HIGH);
    
    initDisplay();
    
    // LVGL Splash Label - Booting
    lv_obj_t* splashLabel = lv_label_create(lv_scr_act());
    lv_label_set_text(splashLabel, "Booting...\nInitializing Display");
    lv_obj_align(splashLabel, LV_ALIGN_CENTER, 0, 0);
    lv_timer_handler();
    delay(1000);
    
    // LVGL Splash Label - I2C
    lv_label_set_text(splashLabel, "Initializing I2C Bus");
    lv_timer_handler();
    delay(500);

    // Initialize I2C bus
    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
    debugPrintf("🔍 I2C on SDA:%d, SCL:%d\n", BOARD_I2C_SDA, BOARD_I2C_SCL);

    // LVGL Splash Label - Touch
    lv_label_set_text(splashLabel, "Initializing Touch");
    lv_timer_handler();

    // Initialize peripherals
    initTouch();

    // LVGL Splash Label - IMU
    lv_label_set_text(splashLabel, "Initializing IMU");
    lv_timer_handler();

systemData.mpuAvailable = initMPU6050();

// Auto-calibrate IMU every boot
    if (systemData.mpuAvailable) {
    lv_label_set_text(splashLabel, "Calibrating IMU");
    lv_timer_handler();
    
    calibrateAccelerometer();
}    
    // LVGL Splash Label - PMU
    lv_label_set_text(splashLabel, "Initializing PMU");
    lv_timer_handler();

    systemData.pmuAvailable = initPMU();

    // LVGL Splash Label - SD Card
    lv_label_set_text(splashLabel, "Initializing SD Card");
    lv_timer_handler();

    systemData.sdCardAvailable = initSDCard();
    
    // LVGL Splash Label - GNSS
    lv_label_set_text(splashLabel, "Starting GNSS");
    lv_timer_handler();

    // Initialize GNSS
    debugPrintln("🛰️ Starting GNSS...");
    GNSS_Serial.begin(921600, SERIAL_8N1, GNSS_RX, GNSS_TX);
    if (!myGNSS.begin(GNSS_Serial)) {
        GNSS_Serial.end();
        delay(100);
        GNSS_Serial.begin(115200, SERIAL_8N1, GNSS_RX, GNSS_TX);
        delay(100);
        if (!myGNSS.begin(GNSS_Serial)) {
            debugPrintln("❌ GNSS not detected!");
        } else {
            configureGNSS();
        }
    } else {
        configureGNSS();
    }
if (wifiUDPEnabled){    
    // LVGL Splash Label - WiFi
    lv_label_set_text(splashLabel, "Connecting WiFi");
    lv_timer_handler();

    // Initialize WiFi
    debugPrintln("📡 Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    unsigned long wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 20000) {
        delay(1000);
        debugPrint(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        debugPrintln("\n✅ WiFi connected!");
        debugPrintf("📍 IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        debugPrintln("\n❌ WiFi failed!");
    }
}    
    // LVGL Splash Label - BLE
    lv_label_set_text(splashLabel, "Starting BLE");
    lv_timer_handler();

    // Initialize BLE with minimal callbacks (no file system access)
    debugPrintln("🔵 Initializing BLE...");
    BLEDevice::init("ESP32_GPS_Logger");
    BLEServer* pServer = BLEDevice::createServer();
    pServer->setCallbacks(new EnhancedServerCallbacks());
    
    BLEService* pService = pServer->createService(telemetryServiceUUID);
    
    telemetryChar = pService->createCharacteristic(telemetryCharUUID, BLECharacteristic::PROPERTY_NOTIFY);
    telemetryDescriptor = new BLE2902();
    telemetryChar->addDescriptor(telemetryDescriptor);
    
    configChar = pService->createCharacteristic(configCharUUID, BLECharacteristic::PROPERTY_WRITE);
    configChar->setCallbacks(new EnhancedConfigCallbacks());
    
    // File transfer characteristic
    fileTransferChar = pService->createCharacteristic(
        fileTransferCharUUID,
        BLECharacteristic::PROPERTY_READ | 
        BLECharacteristic::PROPERTY_WRITE | 
        BLECharacteristic::PROPERTY_NOTIFY
    );
    fileTransferChar->setCallbacks(new EnhancedFileTransferCallbacks());
    
    pService->start();
    
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(telemetryServiceUUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();
    
    debugPrintln("✅ BLE with minimal deferred file transfer ready");
    
    // LVGL Splash Label - UI
    lv_label_set_text(splashLabel, "Starting UI");
    lv_timer_handler();

    // Initialize UI Manager with all data references
    uiManager.init(&systemData, &gpsData, &imuData, &batteryData, &perfStats);
    uiManager.setFileTransferData(&fileTransfer);
    uiManager.setLoggingCallback(toggleLogging);
    
    // Remove splash label after UI is ready
    lv_obj_del(splashLabel);

    perfStats.lastResetTime = millis();
    systemData.displayOn = true;
    systemData.lastDisplayActivity = millis();
    
    debugPrintln("🎯 T-Display-S3-Pro GPS Logger Ready!");
    debugPrintln("🖱️ Touch interface with minimal deferred file transfer");
    debugPrintln("📤 BLE commands: LIST_FILES, DOWNLOAD:filename, DELETE:filename");
    debugPrintln("🔒 Zero file system operations in BLE callbacks");
    debugPrintln("⚡ All file operations deferred to main loop for stack safety");
}
//=========================================part6
void loop() {
    static unsigned long lastPacketTime = 0;
    static unsigned long lastDebugTime = 0;
    static unsigned long lastWiFiCheck = 0;
    static unsigned long lastPerfReset = 0;
    
    // Handle LVGL tasks - this is critical for UI responsiveness
    lv_timer_handler();
    uiManager.update();
    
    // CRITICAL: Process deferred file operations (called in main loop - safe stack)
    processDeferredFileOperations();
    
    // Process file transfers (ongoing transfers)
    processFileTransfer();
    
    // Update file transfer UI more frequently during transfer
    if (fileTransfer.active) {
        static unsigned long lastTransferUIUpdate = 0;
        if (millis() - lastTransferUIUpdate > 500) {
            uiManager.requestUpdate();
            lastTransferUIUpdate = millis();
        }
    }
    
    // Update battery data
    updateBatteryLevel();
    
    // Read IMU data
    if (systemData.mpuAvailable) {
        readMPU6050();
    }
    
    // WiFi check every 30s
    if (millis() - lastWiFiCheck > 30000) {
        lastWiFiCheck = millis();
        if (WiFi.status() != WL_CONNECTED) {
            WiFi.disconnect();
            delay(1000);
            WiFi.begin(ssid, password);
            uiManager.requestUpdate();
        }
    }
    
    // Reset performance stats every 5 minutes
    if (millis() - lastPerfReset > 300000) {
        lastPerfReset = millis();
        perfStats.minDelta = 9999;
        perfStats.maxDelta = 0;
        perfStats.droppedPackets = 0;
        perfStats.totalPackets = 0;
    }
    
    // Process GPS data
    if (myGNSS.getPVT()) {
        unsigned long now = millis();
        unsigned long delta = now - lastPacketTime;
        
        // Update GPS data structure
        gpsData.timestamp = myGNSS.getUnixEpoch();
        gpsData.latitude = myGNSS.getLatitude() / 1e7;
        gpsData.longitude = myGNSS.getLongitude() / 1e7;
        gpsData.altitude = myGNSS.getAltitude() / 1000; // mm to m
        gpsData.speed = myGNSS.getGroundSpeed() * 0.0036; // mm/s to km/h
        gpsData.heading = myGNSS.getHeading() / 100000.0; // deg * 1e5 to deg
        gpsData.fixType = myGNSS.getFixType();
        gpsData.satellites = myGNSS.getSIV();
        gpsData.year = myGNSS.getYear();
        gpsData.month = myGNSS.getMonth();
        gpsData.day = myGNSS.getDay();
        gpsData.hour = myGNSS.getHour();
        gpsData.minute = myGNSS.getMinute();
        gpsData.second = myGNSS.getSecond();
        
        // Update performance stats
        perfStats.totalPackets++;
        if (lastPacketTime > 0) {
            if (delta < perfStats.minDelta) perfStats.minDelta = delta;
            if (delta > perfStats.maxDelta) perfStats.maxDelta = delta;
            perfStats.avgDelta = (perfStats.avgDelta + delta) / 2;
        }
        lastPacketTime = now;
        
        // Create GPS packet for transmission
        GPSPacket packet;
        packet.timestamp = gpsData.timestamp;
        packet.latitude = myGNSS.getLatitude();
        packet.longitude = myGNSS.getLongitude();
        packet.altitude = myGNSS.getAltitude();
        packet.speed = myGNSS.getGroundSpeed();
        packet.heading = myGNSS.getHeading();
        packet.fixType = gpsData.fixType;
        packet.satellites = gpsData.satellites;
        
        packet.battery_mv = (uint16_t)(batteryData.voltage * 1000.0f);
        packet.battery_pct = batteryData.percentage;
        
        if (systemData.mpuAvailable) {
            packet.accel_x = (int16_t)(imuData.accelX * 1000);
            packet.accel_y = (int16_t)(imuData.accelY * 1000);
            packet.accel_z = (int16_t)(imuData.accelZ * 1000);
            packet.gyro_x = (int16_t)(imuData.gyroX * 100);
            packet.gyro_y = (int16_t)(imuData.gyroY * 100);
        } else {
            packet.accel_x = packet.accel_y = packet.accel_z = 0;
            packet.gyro_x = packet.gyro_y = 0;
        }
        
        // PMU status byte
        packet.pmu_status = (batteryData.isCharging ? 0x01 : 0x00) |
                           (batteryData.usbConnected ? 0x02 : 0x00) |
                           (batteryData.isConnected ? 0x04 : 0x00);
       
        packet.crc = crc16((uint8_t*)&packet, sizeof(GPSPacket) - 2);
        
        // Send via UDP
        if (WiFi.status() == WL_CONNECTED) {
            udp.beginPacket(remoteIP, remotePort);
            udp.write((uint8_t*)&packet, sizeof(GPSPacket));
            udp.endPacket();
        }
        
        // Send via BLE
        if (telemetryChar && telemetryDescriptor->getNotifications()) {
            telemetryChar->setValue((uint8_t*)&packet, sizeof(GPSPacket));
            telemetryChar->notify();
        }
        
        // Log to SD - create log file if needed (safe in main loop)
        if (systemData.loggingActive && systemData.sdCardAvailable) {
            if (!logFile) {
                createLogFile();
            }
            if (logFile) {
                size_t written = logFile.write((uint8_t*)&packet, sizeof(GPSPacket));
                if (written != sizeof(GPSPacket)) {
                    perfStats.droppedPackets++;
                } else {
                    logFile.flush();
                }
            }
        }
        
        // Update UI if significant changes
        static uint8_t lastFixType = 0;
        static uint8_t lastSats = 0;
        static float lastSpeed = 0;
        
        if (gpsData.fixType != lastFixType || 
            abs((int)gpsData.satellites - (int)lastSats) > 1 ||
            abs(gpsData.speed - lastSpeed) > 1.0f) {
            uiManager.requestUpdate();
            lastFixType = gpsData.fixType;
            lastSats = gpsData.satellites;
            lastSpeed = gpsData.speed;
        }
        
        // Debug output every 10 seconds
        if (now - lastDebugTime >= 10000) {
            lastDebugTime = now;
            
            debugPrintf("📊 GPS: %02d/%02d/%04d %02d:%02d:%02d UTC | ",
                gpsData.day, gpsData.month, gpsData.year,
                gpsData.hour, gpsData.minute, gpsData.second);
            
            debugPrintf("Fix:%d Sats:%d Speed:%.1fkm/h Batt:%.1fV(%d%%)\n",
                gpsData.fixType, gpsData.satellites, gpsData.speed, 
                batteryData.voltage, batteryData.percentage);
            
            debugPrintf("⚡ Perf: Δ=%lums Pkts:%lu Drop:%lu RAM:%d\n",
                delta, perfStats.totalPackets, perfStats.droppedPackets, ESP.getFreeHeap());
            
            // File transfer status
            if (fileTransfer.active) {
                debugPrintf("📤 Transfer: %s %.1f%% (%d/%d bytes)\n",
                    fileTransfer.filename.c_str(), fileTransfer.progressPercent,
                    fileTransfer.bytesSent, fileTransfer.fileSize);
            }
            
            // IMU status
            if (systemData.mpuAvailable) {
                debugPrintf("🔄 IMU: %.1fg %s Temp:%.1f°C\n",
                    imuData.magnitude, 
                    imuData.motionDetected ? "Motion" : "Still",
                    imuData.temperature);
            }
            
            // System status
            debugPrintf("🔗 Status: WiFi:%s BLE:%s SD:%s Log:%s Touch:%s\n",
                WiFi.status() == WL_CONNECTED ? "✅" : "❌",
                telemetryDescriptor->getNotifications() ? "✅" : "❌",
                systemData.sdCardAvailable ? "✅" : "❌",
                systemData.loggingActive ? "✅" : "❌",
                systemData.touchAvailable ? "✅" : "❌");
            
            // Deferred operations status
            if (pendingListFiles || pendingStartTransfer || pendingDeleteFile || pendingCancelTransfer) {
                debugPrintf("⏳ Pending: List:%s Transfer:%s Delete:%s Cancel:%s\n",
                    pendingListFiles ? "YES" : "NO",
                    pendingStartTransfer ? "YES" : "NO", 
                    pendingDeleteFile ? "YES" : "NO",
                    pendingCancelTransfer ? "YES" : "NO");
            }
        }
    }
    
    // Small delay to prevent overwhelming the system
    delay(5);
}
