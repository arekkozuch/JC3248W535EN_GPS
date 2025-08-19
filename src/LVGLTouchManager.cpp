#include "LVGLTouchManager.h"

// Static instance pointer for LVGL callback
static LVGLTouchManager* touch_instance = nullptr;

LVGLTouchManager::LVGLTouchManager(bool enableDebug) {
    isInitialized = false;
    debugEnabled = enableDebug;
    lvgl_indev = nullptr;
    touchPressed = false;
    lastTouchX = 0;
    lastTouchY = 0;
    touch_instance = this;
}

void LVGLTouchManager::debugPrint(const String& message) {
    if (debugEnabled) {
        Serial.print("[LVGL_TOUCH] ");
        Serial.print(millis());
        Serial.print("ms: ");
        Serial.println(message);
    }
}

void LVGLTouchManager::debugPrintValue(const String& label, int value) {
    if (debugEnabled) {
        Serial.print("[LVGL_TOUCH] ");
        Serial.print(label);
        Serial.print(": ");
        Serial.println(value);
    }
}

void LVGLTouchManager::debugPrintHex(const String& label, uint32_t value) {
    if (debugEnabled) {
        Serial.print("[LVGL_TOUCH] ");
        Serial.print(label);
        Serial.print(": 0x");
        Serial.println(value, HEX);
    }
}

void LVGLTouchManager::lvgl_touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    if (!touch_instance) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }
    
    uint16_t x, y;
    bool touched = touch_instance->readTouchPoint(x, y);
    
    if (touched) {
        // Transform coordinates from our coordinate system to LVGL coordinate system
        // Our system: 480x320 with (0,0) at top-left
        // LVGL system: 480x320 with (0,0) at top-left
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PR;
        
        touch_instance->touchPressed = true;
        touch_instance->lastTouchX = x;
        touch_instance->lastTouchY = y;
    } else {
        // Use last known position when released
        data->point.x = touch_instance->lastTouchX;
        data->point.y = touch_instance->lastTouchY;
        data->state = LV_INDEV_STATE_REL;
        touch_instance->touchPressed = false;
    }
}

bool LVGLTouchManager::readTouchPoint(uint16_t &x, uint16_t &y) {
    if (!isInitialized) return false;
    
    uint8_t data[AXS_MAX_TOUCH_NUMBER * 6 + 2] = {0};
    
    // Define the read command array
    const uint8_t read_cmd[11] = {
        0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00,
        (uint8_t)((AXS_MAX_TOUCH_NUMBER * 6 + 2) >> 8),
        (uint8_t)((AXS_MAX_TOUCH_NUMBER * 6 + 2) & 0xff),
        0x00, 0x00, 0x00
    };
    
    Wire.beginTransmission(TOUCH_ADDR);
    Wire.write(read_cmd, 11);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    
    if (Wire.requestFrom(TOUCH_ADDR, sizeof(data)) != sizeof(data)) {
        return false;
    }
    
    for (int i = 0; i < sizeof(data); i++) {
        data[i] = Wire.read();
    }
    
    if (data[1] > 0 && data[1] <= AXS_MAX_TOUCH_NUMBER) {
        uint16_t rawX = ((data[2] & 0x0F) << 8) | data[3];
        uint16_t rawY = ((data[4] & 0x0F) << 8) | data[5];
        
        // Filter out invalid readings
        if (rawX == 273 && rawY == 273) return false;
        if (rawX > 4000 || rawY > 4000) return false;
        
        // Apply coordinate mapping for LVGL coordinate system
        // Map to LVGL's 480x320 coordinate system
        y = map(rawX, 0, 320, 320, 0);
        x = rawY;
        
        return true;
    }
    
    return false;
}

bool LVGLTouchManager::begin() {
    debugPrint("Initializing LVGL touch controller...");
    
    // Initialize I2C with correct pins
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(TOUCH_I2C_CLOCK);
    
    debugPrintValue("I2C SDA pin", TOUCH_SDA);
    debugPrintValue("I2C SCL pin", TOUCH_SCL);
    debugPrintValue("I2C clock", TOUCH_I2C_CLOCK);
    
    // Configure touch pins
    pinMode(TOUCH_INT_PIN, INPUT_PULLUP);
    pinMode(TOUCH_RST_PIN, OUTPUT);
    
    debugPrintValue("Touch interrupt pin", TOUCH_INT_PIN);
    debugPrintValue("Touch reset pin", TOUCH_RST_PIN);
    
    // Reset sequence
    reset();
    
    // Test communication
    Wire.beginTransmission(TOUCH_ADDR);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
        debugPrint("Touch controller communication OK");
        
        // Initialize LVGL input device
        lv_indev_drv_init(&lvgl_indev_drv);
        lvgl_indev_drv.type = LV_INDEV_TYPE_POINTER;
        lvgl_indev_drv.read_cb = lvgl_touch_read_cb;
        
        lvgl_indev = lv_indev_drv_register(&lvgl_indev_drv);
        if (!lvgl_indev) {
            debugPrint("Failed to register LVGL input device!");
            return false;
        }
        
        debugPrint("LVGL input device registered successfully");
        isInitialized = true;
        
        if (debugEnabled) {
            scanI2CDevices();
        }
        
        return true;
    } else {
        debugPrint("Touch controller communication FAILED");
        debugPrintValue("I2C error code", error);
        isInitialized = false;
        return false;
    }
}

bool LVGLTouchManager::isReady() const {
    return isInitialized;
}

void LVGLTouchManager::reset() {
    debugPrint("Resetting touch controller...");
    digitalWrite(TOUCH_RST_PIN, LOW);
    delay(200);
    digitalWrite(TOUCH_RST_PIN, HIGH);
    delay(200);
    debugPrint("Touch controller reset completed");
}

void LVGLTouchManager::scanI2CDevices() {
    debugPrint("Scanning I2C bus...");
    int deviceCount = 0;
    
    for (byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        byte error = Wire.endTransmission();
        
        if (error == 0) {
            debugPrintHex("I2C device found at address", address);
            deviceCount++;
        }
    }
    
    debugPrintValue("Total I2C devices found", deviceCount);
}

bool LVGLTouchManager::getTouchPoint(uint16_t &x, uint16_t &y) {
    return readTouchPoint(x, y);
}