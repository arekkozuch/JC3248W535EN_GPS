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
#include <Arduino_GFX_Library.h>
#include <vector>

#include "ui_manager.h"
#include "data_structures.h"
#include "boardconfig.h"

// Hardware objects (conditionally initialized)
SFE_UBLOX_GNSS myGNSS;
Preferences preferences;
HardwareSerial GNSS_Serial(2);  // Use UART2 for GPS
WiFiUDP udp;
UIManager uiManager;

// Separate I2C for IMU (different from touch I2C)
TwoWire IMU_Wire = TwoWire(1);

// WiFi Configuration
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const IPAddress remoteIP(172, 16, 2, 158);
const uint16_t remotePort = 9000;

// BLE objects
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
String pendingFilename = "";

// Forward declarations
class EnhancedConfigCallbacks;
class EnhancedFileTransferCallbacks;
class EnhancedServerCallbacks;

// Debug functions with peripheral check
void debugPrint(const char* message) {
    if(debugMode && DEBUG_PERIPHERAL_INIT) Serial.print(message);
}

void debugPrintln(const String& message) {
    if(debugMode && DEBUG_PERIPHERAL_INIT) Serial.println(message);
}

void debugPrintf(const char* format, ...) {
    if(debugMode && DEBUG_PERIPHERAL_INIT) {
        va_list args;
        va_start(args, format);
        Serial.printf(format, args);
        va_end(args);
    }
}

void warnMissingHardware(const String& peripheral) {
    if(DEBUG_MISSING_HARDWARE) {
        Serial.printf("⚠️  %s not available - continuing without it\n", peripheral.c_str());
    }
}

// IMU I2C functions using separate I2C bus
void writeRegister(uint8_t reg, uint8_t value) {
    IMU_Wire.beginTransmission(MPU6xxx_ADDRESS);
    IMU_Wire.write(reg);
    IMU_Wire.write(value);
    IMU_Wire.endTransmission();
}

uint8_t readRegister(uint8_t reg) {
    IMU_Wire.beginTransmission(MPU6xxx_ADDRESS);
    IMU_Wire.write(reg);
    IMU_Wire.endTransmission(false);
    IMU_Wire.requestFrom(MPU6xxx_ADDRESS, 1, 1);
    return IMU_Wire.read();
}

int16_t readRegister16(uint8_t reg) {
    IMU_Wire.beginTransmission(MPU6xxx_ADDRESS);
    IMU_Wire.write(reg);
    IMU_Wire.endTransmission(false);
    IMU_Wire.requestFrom(MPU6xxx_ADDRESS, 2, 1);
    int16_t value = IMU_Wire.read() << 8;
    value |= IMU_Wire.read();
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
    const int CALIBRATION_DELAY = 20;
    
    float accelSumX = 0, accelSumY = 0, accelSumZ = 0;
    float gyroSumX = 0, gyroSumY = 0, gyroSumZ = 0;
    
    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        int16_t accelX = readRegister16(MPU6xxx_ACCEL_XOUT_H);
        int16_t accelY = readRegister16(MPU6xxx_ACCEL_XOUT_H + 2);
        int16_t accelZ = readRegister16(MPU6xxx_ACCEL_XOUT_H + 4);
        
        int16_t gyroX = readRegister16(MPU6xxx_GYRO_XOUT_H);
        int16_t gyroY = readRegister16(MPU6xxx_GYRO_XOUT_H + 2);
        int16_t gyroZ = readRegister16(MPU6xxx_GYRO_XOUT_H + 4);
        
        float accelX_g = accelX / 16384.0;
        float accelY_g = accelY / 16384.0;
        float accelZ_g = accelZ / 16384.0;
        
        float gyroX_dps = gyroX / 131.0;
        float gyroY_dps = gyroY / 131.0;
        float gyroZ_dps = gyroZ / 131.0;
        
        accelSumX += accelX_g;
        accelSumY += accelY_g;
        accelSumZ += accelZ_g;
        
        gyroSumX += gyroX_dps;
        gyroSumY += gyroY_dps;
        gyroSumZ += gyroZ_dps;
        
        delay(CALIBRATION_DELAY);
    }
    
    imuData.accelOffsetX = accelSumX / CALIBRATION_SAMPLES;
    imuData.accelOffsetY = accelSumY / CALIBRATION_SAMPLES;
    imuData.accelOffsetZ = (accelSumZ / CALIBRATION_SAMPLES) - 1.0;
    
    imuData.gyroOffsetX = gyroSumX / CALIBRATION_SAMPLES;
    imuData.gyroOffsetY = gyroSumY / CALIBRATION_SAMPLES;
    imuData.gyroOffsetZ = gyroSumZ / CALIBRATION_SAMPLES;
    
    imuData.isCalibrated = true;
    
    debugPrintln("✅ IMU calibration complete!");
    debugPrintf("📊 Accel offsets: X=%.4f, Y=%.4f, Z=%.4f\n", 
                  imuData.accelOffsetX, imuData.accelOffsetY, imuData.accelOffsetZ);
    
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
    
    float rawAccelX = accelX / 16384.0;
    float rawAccelY = accelY / 16384.0;
    float rawAccelZ = accelZ / 16384.0;
    
    float rawGyroX = gyroX / 131.0;
    float rawGyroY = gyroY / 131.0;
    float rawGyroZ = gyroZ / 131.0;
    
    if (imuData.isCalibrated) {
        imuData.accelX = rawAccelX - imuData.accelOffsetX;
        imuData.accelY = rawAccelY - imuData.accelOffsetY;
        imuData.accelZ = rawAccelZ - imuData.accelOffsetZ;
        
        imuData.gyroX = rawGyroX - imuData.gyroOffsetX;
        imuData.gyroY = rawGyroY - imuData.gyroOffsetY;
        imuData.gyroZ = rawGyroZ - imuData.gyroOffsetZ;
    } else {
        imuData.accelX = rawAccelX;
        imuData.accelY = rawAccelY;
        imuData.accelZ = rawAccelZ;
        
        imuData.gyroX = rawGyroX;
        imuData.gyroY = rawGyroY;
        imuData.gyroZ = rawGyroZ;
    }
    
    static uint8_t chipType = 0;
    if (chipType == 0) {
        chipType = readRegister(MPU6xxx_WHO_AM_I);
    }
    
    if (chipType == 0x68) {
        imuData.temperature = (temp / 340.0) + 36.53;
    } else {
        imuData.temperature = (temp / 333.87) + 21.0;
    }
    
    imuData.magnitude = sqrt(imuData.accelX * imuData.accelX + 
                            imuData.accelY * imuData.accelY + 
                            imuData.accelZ * imuData.accelZ);
    
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

// Basic battery monitoring using ADC (JC3248W535EN doesn't have PMU)
void updateBatteryData() {
    static unsigned long lastBatteryUpdate = 0;
    if (millis() - lastBatteryUpdate < 5000) return;
    
    lastBatteryUpdate = millis();
    
    // Read ADC for basic battery monitoring if connected
    uint16_t adcValue = analogRead(ADC_BAT);
    batteryData.voltage = (adcValue / 4095.0) * 3.3 * 2; // Assuming voltage divider
    
    // Simple percentage calculation
    if (batteryData.voltage > 2.5f) {
        batteryData.percentage = constrain(map(batteryData.voltage * 100, 300, 420, 0, 100), 0, 100);
    } else {
        batteryData.percentage = 100; // Assume powered via USB if no battery detected
    }
    
    batteryData.isCharging = false; // JC3248W535EN doesn't have charge detection
    batteryData.usbConnected = true; // Assume USB powered
    batteryData.isConnected = true;
    batteryData.chargeStatus = "USB Powered";
    
    uiManager.requestUpdate();
}

// Robust peripheral initialization functions
bool initGPS() {
    if (!ENABLE_GPS) {
        debugPrintln("🛰️ GPS disabled in configuration");
        return false;
    }
    
    debugPrintln("🛰️ Initializing GPS...");
    unsigned long startTime = millis();
    
    // Try high speed first
    GNSS_Serial.begin(921600, SERIAL_8N1, GNSS_RX, GNSS_TX);
    delay(100);
    
    while (millis() - startTime < GPS_INIT_TIMEOUT_MS) {
        if (myGNSS.begin(GNSS_Serial)) {
            debugPrintln("✅ GPS detected at 921600 baud");
            configureGNSS();
            return true;
        }
        delay(500);
    }
    
    // Try standard speed
    GNSS_Serial.end();
    delay(100);
    GNSS_Serial.begin(115200, SERIAL_8N1, GNSS_RX, GNSS_TX);
    delay(100);
    
    startTime = millis();
    while (millis() - startTime < GPS_INIT_TIMEOUT_MS / 2) {
        if (myGNSS.begin(GNSS_Serial)) {
            debugPrintln("✅ GPS detected at 115200 baud");
            configureGNSS();
            return true;
        }
        delay(500);
    }
    
    warnMissingHardware("GPS");
    return false;
}

bool initIMU() {
    if (!ENABLE_IMU) {
        debugPrintln("📄 IMU disabled in configuration");
        return false;
    }
    
    debugPrintln("📄 Initializing IMU...");
    unsigned long startTime = millis();
    
    // Initialize separate I2C bus for IMU
    IMU_Wire.begin(IMU_I2C_SDA, IMU_I2C_SCL);
    IMU_Wire.setClock(400000);
    delay(100);
    
    while (millis() - startTime < IMU_INIT_TIMEOUT_MS) {
        uint8_t whoami = readRegister(MPU6xxx_WHO_AM_I);
        debugPrintf("🔋 WHO_AM_I register: 0x%02X\n", whoami);
        
        if (whoami == 0x68 || whoami == 0x70 || whoami == 0x71 || whoami == 0x73) {
            debugPrintln("✅ IMU detected");
            
            writeRegister(MPU6xxx_PWR_MGMT_1, 0x00);
            delay(100);
            writeRegister(MPU6xxx_ACCEL_CONFIG, 0x00);
            writeRegister(MPU6xxx_GYRO_CONFIG, 0x00);
            
            // Test read
            int16_t testRead = readRegister16(MPU6xxx_ACCEL_XOUT_H);
            if (testRead != -1 && testRead != 0) {
                debugPrintln("✅ IMU communication verified");
                calibrateAccelerometer();
                return true;
            }
        }
        delay(500);
    }
    
    warnMissingHardware("IMU");
    return false;
}

bool initSDCardRobust() {
    if (!ENABLE_SD_CARD) {
        debugPrintln("📱 SD Card disabled in configuration");
        return false;
    }
    
    debugPrintln("📱 Initializing SD card...");
    
    SPI.end();
    delay(100);
    SPI.begin(BOARD_SPI_SCK, BOARD_SPI_MISO, BOARD_SPI_MOSI);
    delay(100);
    
    // Try different speeds
    uint32_t speeds[] = {4000000, 1000000, 400000};
    
    for (int attempt = 0; attempt < SD_INIT_RETRIES; attempt++) {
        for (int speedIdx = 0; speedIdx < 3; speedIdx++) {
            if (SD.begin(BOARD_SD_CS, SPI, speeds[speedIdx])) {
                uint8_t cardType = SD.cardType();
                if (cardType != CARD_NONE) {
                    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
                    debugPrintf("✅ SD Card: %lluMB at %d Hz\n", cardSize, speeds[speedIdx]);
                    
                    // Test write
                    File testFile = SD.open("/test.tmp", FILE_WRITE);
                    if (testFile) {
                        testFile.println("GPS Logger Test");
                        testFile.close();
                        SD.remove("/test.tmp");
                        return true;
                    }
                }
            }
            delay(500);
        }
        debugPrintf("SD init attempt %d/%d failed\n", attempt + 1, SD_INIT_RETRIES);
    }
    
    warnMissingHardware("SD Card");
    return false;
}

bool initWiFiRobust() {
    if (!ENABLE_WIFI) {
        debugPrintln("📡 WiFi disabled in configuration");
        wifiUDPEnabled = false;
        return false;
    }
    
    debugPrintln("📡 Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT_MS) {
        delay(1000);
        debugPrint(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        debugPrintln("\n✅ WiFi connected!");
        debugPrintf("📍 IP: %s\n", WiFi.localIP().toString().c_str());
        wifiUDPEnabled = true;
        return true;
    } else {
        debugPrintln("\n⚠️ WiFi connection failed - continuing without WiFi");
        wifiUDPEnabled = false;
        return false;
    }
}

// Mock data generation for missing peripherals
void generateMockGPSData() {
    static float mockLat = 52.2297;  // Warsaw coordinates
    static float mockLon = 21.0122;
    static unsigned long lastMockUpdate = 0;
    
    if (millis() - lastMockUpdate > 1000) {  // Update every second
        gpsData.latitude = mockLat + (random(-100, 100) / 100000.0);
        gpsData.longitude = mockLon + (random(-100, 100) / 100000.0);
        gpsData.altitude = 100 + random(-10, 10);
        gpsData.speed = random(0, 60);
        gpsData.heading = random(0, 360);
        gpsData.fixType = 3;  // 3D fix
        gpsData.satellites = random(8, 12);
        gpsData.timestamp = millis() / 1000;
        
        // Set current time (mock)
        unsigned long timeNow = millis() / 1000;
        gpsData.hour = (timeNow / 3600) % 24;
        gpsData.minute = (timeNow / 60) % 60;
        gpsData.second = timeNow % 60;
        gpsData.day = 19;
        gpsData.month = 8;
        gpsData.year = 2025;
        
        lastMockUpdate = millis();
    }
}

void generateMockIMUData() {
    static unsigned long lastMockUpdate = 0;
    
    if (millis() - lastMockUpdate > 100) {  // Update every 100ms
        imuData.accelX = (random(-100, 100) / 100.0);
        imuData.accelY = (random(-100, 100) / 100.0);
        imuData.accelZ = 1.0 + (random(-20, 20) / 100.0);
        imuData.gyroX = (random(-50, 50) / 10.0);
        imuData.gyroY = (random(-50, 50) / 10.0);
        imuData.gyroZ = (random(-50, 50) / 10.0);
        imuData.temperature = 25.0 + (random(-50, 50) / 10.0);
        imuData.magnitude = sqrt(imuData.accelX * imuData.accelX + 
                                imuData.accelY * imuData.accelY + 
                                imuData.accelZ * imuData.accelZ);
        imuData.motionDetected = (imuData.magnitude > MOTION_THRESHOLD);
        lastMockUpdate = millis();
    }
}

bool createLogFile() {
    if (!systemData.sdCardAvailable) return false;
    
    sprintf(currentLogFilename, "/gps_%04d%02d%02d_%02d%02d%02d.bin",
        gpsData.year, gpsData.month, gpsData.day,
        gpsData.hour, gpsData.minute, gpsData.second);
    
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
        if (systemData.sdCardAvailable && (gpsData.fixType >= 2 || !ENABLE_GPS)) {
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

// File transfer functions (same as original)
void sendFileResponse(String response) {
    if (!fileTransferChar) return;
    
    int maxChunkSize = 400;
    for (int i = 0; i < response.length(); i += maxChunkSize) {
        String chunk = response.substring(i, min(i + maxChunkSize, (int)response.length()));
        fileTransferChar->setValue(chunk.c_str());
        fileTransferChar->notify();
        delay(50);
    }
}

void listSDFiles() {
    if (!systemData.sdCardAvailable) {
        sendFileResponse("ERROR:NO_SD_CARD");
        return;
    }
    
    debugPrintln("📂 Listing SD card files...");
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
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
    
    fileList += "COUNT:" + String(fileCount);
    sendFileResponse(fileList);
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
        return;
    }
    
    if (fileTransfer.active && fileTransfer.transferFile) {
        fileTransfer.transferFile.close();
    }
    
    fileTransfer.transferFile = SD.open(fullPath.c_str(), FILE_READ);
    if (!fileTransfer.transferFile) {
        sendFileResponse("ERROR:CANT_OPEN_FILE:" + filename);
        return;
    }
    
    fileTransfer.active = true;
    fileTransfer.filename = filename;
    fileTransfer.fileSize = fileTransfer.transferFile.size();
    fileTransfer.bytesSent = 0;
    fileTransfer.lastChunkTime = millis();
    fileTransfer.progressPercent = 0.0f;
    fileTransfer.transferStartTime = millis();
    
    String response = "START:" + filename + ":" + String(fileTransfer.fileSize);
    sendFileResponse(response);
    
    debugPrintf("📤 Starting transfer: %s (%d bytes)\n", filename.c_str(), fileTransfer.fileSize);
    uiManager.requestUpdate();
}

void processFileTransfer() {
    if (!fileTransfer.active || !fileTransfer.transferFile) return;
    
    unsigned long now = millis();
    if (now - fileTransfer.lastChunkTime < 100) return;
    
    const int chunkSize = 400;
    uint8_t buffer[chunkSize];
    
    int bytesRead = fileTransfer.transferFile.read(buffer, chunkSize);
    if (bytesRead > 0) {
        String chunk = "CHUNK:";
        for (int i = 0; i < bytesRead; i++) {
            if (buffer[i] < 16) chunk += "0";
            chunk += String(buffer[i], HEX);
            if (chunk.length() > 800) break;
        }
        
        chunk += ":SEQ:" + String(fileTransfer.bytesSent / chunkSize);
        
        sendFileResponse(chunk);
        fileTransfer.bytesSent += bytesRead;
        fileTransfer.lastChunkTime = now;
        
        fileTransfer.progressPercent = (float)fileTransfer.bytesSent / fileTransfer.fileSize * 100.0f;
        
        unsigned long elapsed = now - fileTransfer.transferStartTime;
        if (elapsed > 2000 && fileTransfer.bytesSent > 0) {
            float bytesPerMs = (float)fileTransfer.bytesSent / elapsed;
            unsigned long remainingBytes = fileTransfer.fileSize - fileTransfer.bytesSent;
            if (bytesPerMs > 0) {
                fileTransfer.estimatedTimeRemaining = remainingBytes / bytesPerMs;
            }
        }
        
        if (fileTransfer.bytesSent % 2048 == 0) {
            uiManager.requestUpdate();
        }
    } else {
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
        
        fileTransfer.progressPercent = 0.0f;
        fileTransfer.estimatedTimeRemaining = 0;
        uiManager.requestUpdate();
    }
}

void processDeferredFileOperations() {
    if (pendingListFiles) {
        pendingListFiles = false;
        listSDFiles();
    } else if (pendingStartTransfer) {
        pendingStartTransfer = false;
        startFileTransfer(pendingFilename);
        pendingFilename = "";
    } else if (pendingDeleteFile) {
        pendingDeleteFile = false;
        deleteFile(pendingFilename);
        pendingFilename = "";
    } else if (pendingCancelTransfer) {
        pendingCancelTransfer = false;
        cancelFileTransfer();
    }
}

// BLE Callbacks
class EnhancedConfigCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string stdValue = pCharacteristic->getValue();
        String value = String(stdValue.c_str());
        
        if (value.length() == 0) return;
        
        debugPrintf("📝 Config command: %s\n", value.c_str());
        
        if (value == "START_LOG") {
            if (systemData.sdCardAvailable && (gpsData.fixType >= 2 || !ENABLE_GPS)) {
                systemData.loggingActive = true;
                uiManager.requestUpdate();
            }
        } else if (value == "STOP_LOG") {
            systemData.loggingActive = false;
            if (logFile) logFile.close();
            uiManager.requestUpdate();
        } else if (value == "LIST_FILES") {
            pendingListFiles = true;
        } else if (value.startsWith("DOWNLOAD:")) {
            pendingFilename = value.substring(9);
            pendingStartTransfer = true;
        } else if (value.startsWith("DELETE:")) {
            pendingFilename = value.substring(7);
            pendingDeleteFile = true;
        } else if (value == "CANCEL_TRANSFER") {
            pendingCancelTransfer = true;
        }
    }
};

class EnhancedFileTransferCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string stdValue = pCharacteristic->getValue();
        String value = String(stdValue.c_str());
        
        if (value == "LIST") {
            pendingListFiles = true;
        } else if (value.startsWith("GET:")) {
            pendingFilename = value.substring(4);
            pendingStartTransfer = true;
        } else if (value.startsWith("DEL:")) {
            pendingFilename = value.substring(4);
            pendingDeleteFile = true;
        } else if (value == "STOP" || value == "CANCEL") {
            pendingCancelTransfer = true;
        } else if (value == "STATUS") {
            String status = "STATUS:";
            if (fileTransfer.active) {
                status += "ACTIVE:" + fileTransfer.filename + ":" + 
                         String((int)fileTransfer.progressPercent);
            } else {
                status += "IDLE";
            }
            
            if (fileTransferChar) {
                fileTransferChar->setValue(status.c_str());
                fileTransferChar->notify();
            }
        }
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
        
        if (fileTransfer.active) {
            pendingCancelTransfer = true;
        }
        
        BLEDevice::startAdvertising();
        uiManager.requestUpdate();
    }
};

bool initBLERobust() {
    if (!ENABLE_BLE) {
        debugPrintln("🔵 BLE disabled in configuration");
        return false;
    }
    
    try {
        debugPrintln("🔵 Initializing BLE...");
        BLEDevice::init("JC3248_GPS_Logger");
        
        BLEServer* pServer = BLEDevice::createServer();
        pServer->setCallbacks(new EnhancedServerCallbacks());
        
        BLEService* pService = pServer->createService(telemetryServiceUUID);
        
        telemetryChar = pService->createCharacteristic(telemetryCharUUID, BLECharacteristic::PROPERTY_NOTIFY);
        telemetryDescriptor = new BLE2902();
        telemetryChar->addDescriptor(telemetryDescriptor);
        
        configChar = pService->createCharacteristic(configCharUUID, BLECharacteristic::PROPERTY_WRITE);
        configChar->setCallbacks(new EnhancedConfigCallbacks());
        
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
        
        debugPrintln("✅ BLE ready");
        return true;
    } catch (...) {
        warnMissingHardware("BLE");
        return false;
    }
}

void setup() {
    Serial.begin(115200);
    delay(3000);
    Serial.println("🚀 JC3248W535EN GPS Logger v6.1 Starting...");
    Serial.println("🔧 Robust peripheral detection enabled");
    
    // Initialize the UI Manager first (always works)
    uiManager.init(&systemData, &gpsData, &imuData, &batteryData, &perfStats);
    uiManager.setFileTransferData(&fileTransfer);
    uiManager.setLoggingCallback(toggleLogging);
    Serial.println("✅ UI Manager initialized");
    
    // Initialize peripherals with robust detection
    systemData.mpuAvailable = initIMU();
    systemData.sdCardAvailable = initSDCardRobust();
    bool gpsAvailable = initGPS();
    bool wifiAvailable = initWiFiRobust();
    bool bleAvailable = initBLERobust();
    
    // System status summary
    Serial.println("\n📊 System Status Summary:");
    Serial.printf("   🖥️  Display: ✅ Ready\n");
    Serial.printf("   🖱️  Touch:   %s\n", ENABLE_TOUCH ? "✅ Ready" : "❌ Disabled");
    Serial.printf("   🛰️  GPS:     %s\n", gpsAvailable ? "✅ Connected" : "❌ Not found");
    Serial.printf("   📄  IMU:     %s\n", systemData.mpuAvailable ? "✅ Connected" : "❌ Not found");
    Serial.printf("   📱  SD Card: %s\n", systemData.sdCardAvailable ? "✅ Ready" : "❌ Not found");
    Serial.printf("   📡  WiFi:    %s\n", wifiAvailable ? "✅ Connected" : "❌ Disabled/Failed");
    Serial.printf("   🔵  BLE:     %s\n", bleAvailable ? "✅ Ready" : "❌ Failed");
    
    // Set system capabilities
    systemData.touchAvailable = ENABLE_TOUCH;
    systemData.displayOn = true;
    systemData.lastDisplayActivity = millis();
    
    perfStats.lastResetTime = millis();
    
    Serial.println("\n🎯 JC3248W535EN GPS Logger Ready!");
    if (!gpsAvailable) {
        Serial.println("📍 No GPS detected - will use mock data for UI testing");
    }
    if (!systemData.mpuAvailable) {
        Serial.println("🎯 No IMU detected - will use mock data for UI testing");
    }
    Serial.println("🖱️ Touch interface active");
}

void loop() {
    static unsigned long lastPacketTime = 0;
    static unsigned long lastDebugTime = 0;
    static unsigned long lastWiFiCheck = 0;
    static unsigned long lastPerfReset = 0;
    
    // Handle LVGL tasks - this is critical for UI responsiveness
    lv_timer_handler();
    uiManager.update();
    
    // Process deferred file operations
    processDeferredFileOperations();
    
    // Process file transfers
    processFileTransfer();
    
    // Update battery data
    updateBatteryData();
    
    // Read IMU data or generate mock data
    if (systemData.mpuAvailable && ENABLE_IMU) {
        readMPU6050();
    } else {
        generateMockIMUData();  // Provide mock data for UI testing
    }
    
    // WiFi check (if enabled)
    if (ENABLE_WIFI && wifiUDPEnabled && millis() - lastWiFiCheck > 30000) {
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
    
    // Process GPS data or generate mock data
    bool hasGPSData = false;
    if (ENABLE_GPS && myGNSS.getPVT()) {
        hasGPSData = true;
        unsigned long now = millis();
        unsigned long delta = now - lastPacketTime;
        
        // Update GPS data structure
        gpsData.timestamp = myGNSS.getUnixEpoch();
        gpsData.latitude = myGNSS.getLatitude() / 1e7;
        gpsData.longitude = myGNSS.getLongitude() / 1e7;
        gpsData.altitude = myGNSS.getAltitude() / 1000;
        gpsData.speed = myGNSS.getGroundSpeed() * 0.0036;
        gpsData.heading = myGNSS.getHeading() / 100000.0;
        gpsData.fixType = myGNSS.getFixType();
        gpsData.satellites = myGNSS.getSIV();
        gpsData.year = myGNSS.getYear();
        gpsData.month = myGNSS.getMonth();
        gpsData.day = myGNSS.getDay();
        gpsData.hour = myGNSS.getHour();
        gpsData.minute = myGNSS.getMinute();
        gpsData.second = myGNSS.getSecond();
        
        lastPacketTime = now;
    } else if (!ENABLE_GPS) {
        generateMockGPSData();  // Provide mock data for UI testing
        hasGPSData = true;
    }
    
    // Create and transmit data packet (with real or mock data)
    if (hasGPSData || !ENABLE_GPS) {
        unsigned long now = millis();
        unsigned long delta = now - lastPacketTime;
        
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
        packet.latitude = (int32_t)(gpsData.latitude * 1e7);
        packet.longitude = (int32_t)(gpsData.longitude * 1e7);
        packet.altitude = gpsData.altitude * 1000;  // m to mm
        packet.speed = (uint16_t)(gpsData.speed / 0.0036);  // km/h to mm/s
        packet.heading = (uint32_t)(gpsData.heading * 1e5);
        packet.fixType = gpsData.fixType;
        packet.satellites = gpsData.satellites;
        
        packet.battery_mv = (uint16_t)(batteryData.voltage * 1000.0f);
        packet.battery_pct = batteryData.percentage;
        
        if (systemData.mpuAvailable || !ENABLE_IMU) {
            packet.accel_x = (int16_t)(imuData.accelX * 1000);
            packet.accel_y = (int16_t)(imuData.accelY * 1000);
            packet.accel_z = (int16_t)(imuData.accelZ * 1000);
            packet.gyro_x = (int16_t)(imuData.gyroX * 100);
            packet.gyro_y = (int16_t)(imuData.gyroY * 100);
        } else {
            packet.accel_x = packet.accel_y = packet.accel_z = 0;
            packet.gyro_x = packet.gyro_y = 0;
        }
        
        packet.pmu_status = (batteryData.isCharging ? 0x01 : 0x00) |
                           (batteryData.usbConnected ? 0x02 : 0x00) |
                           (batteryData.isConnected ? 0x04 : 0x00);
       
        packet.crc = crc16((uint8_t*)&packet, sizeof(GPSPacket) - 2);
        
        // Send via UDP (if WiFi enabled and connected)
        if (ENABLE_WIFI && wifiUDPEnabled && WiFi.status() == WL_CONNECTED) {
            udp.beginPacket(remoteIP, remotePort);
            udp.write((uint8_t*)&packet, sizeof(GPSPacket));
            udp.endPacket();
        }
        
        // Send via BLE (if enabled and connected)
        if (ENABLE_BLE && telemetryChar && telemetryDescriptor && telemetryDescriptor->getNotifications()) {
            telemetryChar->setValue((uint8_t*)&packet, sizeof(GPSPacket));
            telemetryChar->notify();
        }
        
        // Log to SD (if enabled and available)
        if (ENABLE_SD_CARD && systemData.loggingActive && systemData.sdCardAvailable) {
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
            
            String dataSource = ENABLE_GPS ? "GPS" : "MOCK";
            debugPrintf("📊 %s: %02d/%02d/%04d %02d:%02d:%02d | ",
                dataSource.c_str(), gpsData.day, gpsData.month, gpsData.year,
                gpsData.hour, gpsData.minute, gpsData.second);
            
            debugPrintf("Fix:%d Sats:%d Speed:%.1fkm/h Batt:%.1fV(%d%%)\n",
                gpsData.fixType, gpsData.satellites, gpsData.speed, 
                batteryData.voltage, batteryData.percentage);
            
            debugPrintf("⚡ Perf: Δ=%lums Pkts:%lu Drop:%lu RAM:%d\n",
                delta, perfStats.totalPackets, perfStats.droppedPackets, ESP.getFreeHeap());
            
            // Peripheral status
            debugPrintf("🔗 Active: Display:✅ GPS:%s IMU:%s SD:%s WiFi:%s BLE:%s\n",
                ENABLE_GPS ? "✅" : "🔄",
                systemData.mpuAvailable ? "✅" : "🔄", 
                systemData.sdCardAvailable ? "✅" : "❌",
                (ENABLE_WIFI && WiFi.status() == WL_CONNECTED) ? "✅" : "❌",
                (ENABLE_BLE && telemetryDescriptor && telemetryDescriptor->getNotifications()) ? "✅" : "❌");
        }
    }
    
    // Small delay to prevent overwhelming the system
    delay(5);
}