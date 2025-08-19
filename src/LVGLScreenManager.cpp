#include "LVGLScreenManager.h"

// Static instance pointer for callbacks
static LVGLScreenManager* instance = nullptr;
static Arduino_Canvas* canvas_instance = nullptr;

LVGLScreenManager::LVGLScreenManager(bool enableDebug) {
    bus = nullptr;
    gfx = nullptr;
    canvas = nullptr;
    lvgl_buf1 = nullptr;
    lvgl_buf2 = nullptr;
    lvgl_display = nullptr;
    isInitialized = false;
    debugEnabled = enableDebug;
    instance = this;
}

LVGLScreenManager::~LVGLScreenManager() {
    if (lvgl_buf1) free(lvgl_buf1);
    if (lvgl_buf2) free(lvgl_buf2);
    if (canvas) delete canvas;
    if (gfx) delete gfx;
    if (bus) delete bus;
}

void LVGLScreenManager::debugPrint(const String& message) {
    if (debugEnabled) {
        Serial.print("[LVGL] ");
        Serial.print(millis());
        Serial.print("ms: ");
        Serial.println(message);
    }
}

void LVGLScreenManager::debugPrintValue(const String& label, int value) {
    if (debugEnabled) {
        Serial.print("[LVGL] ");
        Serial.print(label);
        Serial.print(": ");
        Serial.println(value);
    }
}

void LVGLScreenManager::lvgl_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    if (!canvas_instance) {
        lv_disp_flush_ready(disp_drv);
        return;
    }
    
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    // LVGL uses different coordinate system, so we need to transform
    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint32_t lvgl_x = area->x1 + x;
            uint32_t lvgl_y = area->y1 + y;
            
            // Transform coordinates to match our screen orientation
            int16_t screen_x = 320 - lvgl_y;
            int16_t screen_y = lvgl_x;
            
            // Get color from LVGL buffer
            lv_color_t lvgl_color = color_p[y * w + x];
            uint16_t color = lv_color_to16(lvgl_color);
            
            // Draw pixel if within bounds
            if (screen_x >= 0 && screen_x < 320 && screen_y >= 0 && screen_y < 480) {
                canvas_instance->drawPixel(screen_x, screen_y, color);
            }
        }
    }
    
    canvas_instance->flush();
    lv_disp_flush_ready(disp_drv);
}

void LVGLScreenManager::lvgl_timer_handler(void *param) {
    lv_timer_handler();
}

bool LVGLScreenManager::begin() {
    debugPrint("Initializing LVGL display...");
    
    try {
        // Initialize hardware
        bus = new Arduino_ESP32QSPI(45, 47, 21, 48, 40, 39);
        gfx = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, 320, 480);
        canvas = new Arduino_Canvas(320, 480, gfx, 0, 0, 0);
        canvas_instance = canvas;
        
        if (!canvas->begin()) {
            debugPrint("Display canvas initialization failed!");
            return false;
        }
        
        // Enable backlight
        enableBacklight(true);
        debugPrint("Hardware initialized");
        
        // Initialize LVGL
        lv_init();
        debugPrint("LVGL initialized");
        
        // Allocate LVGL buffers
        lvgl_buf1 = (lv_color_t*)malloc(DISPLAY_BUFFER_SIZE * sizeof(lv_color_t));
        lvgl_buf2 = (lv_color_t*)malloc(DISPLAY_BUFFER_SIZE * sizeof(lv_color_t));
        
        if (!lvgl_buf1 || !lvgl_buf2) {
            debugPrint("Failed to allocate LVGL buffers!");
            return false;
        }
        
        debugPrintValue("Buffer size", DISPLAY_BUFFER_SIZE * sizeof(lv_color_t));
        
        // Initialize display buffer
        lv_disp_draw_buf_init(&lvgl_disp_buf, lvgl_buf1, lvgl_buf2, DISPLAY_BUFFER_SIZE);
        
        // Initialize display driver
        lv_disp_drv_init(&lvgl_disp_drv);
        lvgl_disp_drv.hor_res = 480;  // LVGL coordinate system
        lvgl_disp_drv.ver_res = 320;
        lvgl_disp_drv.flush_cb = lvgl_flush_cb;
        lvgl_disp_drv.draw_buf = &lvgl_disp_buf;
        
        // Register display
        lvgl_display = lv_disp_drv_register(&lvgl_disp_drv);
        if (!lvgl_display) {
            debugPrint("Failed to register LVGL display!");
            return false;
        }
        
        debugPrint("LVGL display registered successfully");
        
        // Clear screen
        canvas->fillScreen(0x0000);
        canvas->flush();
        
        isInitialized = true;
        debugPrint("LVGL initialization completed!");
        debugPrintValue("Free heap", ESP.getFreeHeap());
        
        return true;
        
    } catch (...) {
        debugPrint("Exception during LVGL initialization!");
        isInitialized = false;
        return false;
    }
}

bool LVGLScreenManager::isReady() const {
    return isInitialized;
}

void LVGLScreenManager::update() {
    if (isInitialized) {
        lv_timer_handler();
    }
}

void LVGLScreenManager::enableBacklight(bool enable) {
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, enable ? HIGH : LOW);
}

// Helper functions to create common UI elements
lv_obj_t* LVGLScreenManager::createLabel(lv_obj_t* parent, const char* text, lv_coord_t x, lv_coord_t y) {
    if (!isInitialized) return nullptr;
    
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    return label;
}

lv_obj_t* LVGLScreenManager::createButton(lv_obj_t* parent, const char* text, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
    if (!isInitialized) return nullptr;
    
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    
    return btn;
}

lv_obj_t* LVGLScreenManager::createSlider(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
    if (!isInitialized) return nullptr;
    
    lv_obj_t* slider = lv_slider_create(parent);
    lv_obj_set_pos(slider, x, y);
    lv_obj_set_size(slider, w, h);
    
    return slider;
}

lv_obj_t* LVGLScreenManager::createProgressBar(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
    if (!isInitialized) return nullptr;
    
    lv_obj_t* bar = lv_bar_create(parent);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, h);
    
    return bar;
}