#ifndef LVGL_TOUCH_MANAGER_H
#define LVGL_TOUCH_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>

class LVGLTouchManager {
private:
    // Hardware pin definitions
    static const uint8_t TOUCH_ADDR = 0x3B;
    static const uint8_t TOUCH_SDA = 4;
    static const uint8_t TOUCH_SCL = 8;
    static const uint32_t TOUCH_I2C_CLOCK = 400000;
    static const uint8_t TOUCH_RST_PIN = 12;
    static const uint8_t TOUCH_INT_PIN = 11;
    static const uint8_t AXS_MAX_TOUCH_NUMBER = 1;
    
    bool isInitialized;
    bool debugEnabled;
    
    // LVGL input device
    lv_indev_drv_t lvgl_indev_drv;
    lv_indev_t* lvgl_indev;
    
    // Touch state
    bool touchPressed;
    uint16_t lastTouchX;
    uint16_t lastTouchY;
    
    // Static callback for LVGL
    static void lvgl_touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
    
    void debugPrint(const String& message);
    void debugPrintValue(const String& label, int value);
    void debugPrintHex(const String& label, uint32_t value);
    
    bool readTouchPoint(uint16_t &x, uint16_t &y);

public:
    LVGLTouchManager(bool enableDebug = true);
    
    bool begin();
    bool isReady() const;
    void reset();
    void scanI2CDevices();
    
    // Manual touch reading (if needed)
    bool getTouchPoint(uint16_t &x, uint16_t &y);
    
    // LVGL access
    lv_indev_t* getInputDevice() { return lvgl_indev; }
};

#endif