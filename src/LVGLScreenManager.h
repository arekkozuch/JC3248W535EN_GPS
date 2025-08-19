#ifndef LVGL_SCREEN_MANAGER_H
#define LVGL_SCREEN_MANAGER_H

#include <Arduino.h>
#include <lvgl.h>
#include <Arduino_GFX_Library.h>

class LVGLScreenManager {
private:
    // Hardware pin definitions
    static const uint8_t GFX_BL = 1;
    
    Arduino_DataBus *bus;
    Arduino_GFX *gfx;
    Arduino_Canvas *canvas;
    
    bool isInitialized;
    bool debugEnabled;
    
    // LVGL display buffer
    static const size_t DISPLAY_BUFFER_SIZE = 320 * 40; // 40 lines buffer
    lv_color_t *lvgl_buf1;
    lv_color_t *lvgl_buf2;
    lv_disp_draw_buf_t lvgl_disp_buf;
    lv_disp_drv_t lvgl_disp_drv;
    lv_disp_t *lvgl_display;
    
    // Static callback functions for LVGL
    static void lvgl_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
    static void lvgl_timer_handler(void *param);
    
    void debugPrint(const String& message);
    void debugPrintValue(const String& label, int value);

public:
    LVGLScreenManager(bool enableDebug = true);
    ~LVGLScreenManager();
    
    bool begin();
    bool isReady() const;
    void update(); // Call this in loop()
    
    // Utility functions
    void enableBacklight(bool enable = true);
    
    // Screen dimensions
    uint16_t width() const { return 480; }
    uint16_t height() const { return 320; }
    
    // LVGL access
    lv_disp_t* getDisplay() { return lvgl_display; }
    
    // Helper function to create basic UI elements
    lv_obj_t* createLabel(lv_obj_t* parent, const char* text, lv_coord_t x, lv_coord_t y);
    lv_obj_t* createButton(lv_obj_t* parent, const char* text, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h);
    lv_obj_t* createSlider(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h);
    lv_obj_t* createProgressBar(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h);
};

#endif