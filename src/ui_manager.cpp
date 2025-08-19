#include "ui_manager.h"
#include <Arduino_GFX_Library.h>
#include <WiFi.h>

// Global display objects for JC3248W535EN
Arduino_DataBus *bus = nullptr;
Arduino_GFX *gfx = nullptr;

UIManager::UIManager() :
    systemData(nullptr),
    gpsData(nullptr), 
    imuData(nullptr),
    batteryData(nullptr),
    perfStats(nullptr),
    fileTransferPtr(nullptr),
    mainScreen(nullptr),
    currentScreen(SCREEN_SPEEDOMETER),
    updateRequested(true),
    lastUpdate(0),
    lastHeaderUpdate(0),
    lastStatusUpdate(0),
    progressBar(nullptr),
    transferLabel(nullptr),
    loggingCallback(nullptr),
    menuCallback(nullptr),
    systemCallback(nullptr)
{
}

UIManager::~UIManager() {
    // LVGL objects are automatically cleaned up
}

void UIManager::init(SystemData* sysData, GPSData* gpsData, IMUData* imuData, 
                     BatteryData* battData, PerformanceStats* perfData) {
    // Store data pointers
    this->systemData = sysData;
    this->gpsData = gpsData;
    this->imuData = imuData;
    this->batteryData = battData;
    this->perfStats = perfData;
    
    Serial.println("🖥️ Initializing JC3248W535EN display hardware...");
    
    // Initialize the actual display hardware first
    if (!initializeDisplayHardware()) {
        Serial.println("❌ Display hardware initialization failed!");
        return;
    }
    
    Serial.println("🎨 Initializing LVGL with real display...");
    
    // Initialize LVGL
    lv_init();
    
    // Create display buffer with DMA capability
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(BOARD_TFT_WIDTH * 60 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    if (!buf1) {
        Serial.println("❌ Failed to allocate DMA display buffer");
        // Fallback to regular memory
        static lv_color_t buf_fallback[BOARD_TFT_WIDTH * 40];
        lv_disp_draw_buf_init(&draw_buf, buf_fallback, NULL, BOARD_TFT_WIDTH * 40);
    } else {
        lv_disp_draw_buf_init(&draw_buf, buf1, NULL, BOARD_TFT_WIDTH * 60);
    }

    // Display driver setup
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = BOARD_TFT_WIDTH;
    disp_drv.ver_res = BOARD_TFT_HEIGHT;
    disp_drv.flush_cb = lvgl_display_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Touch input driver (placeholder for now)
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read;
    lv_indev_drv_register(&indev_drv);

    Serial.println("✅ LVGL initialized with real display");
    
    // Create a working test screen
    createWorkingTestScreen();
    
    Serial.println("✅ UIManager initialized with working display");
}

bool UIManager::initializeDisplayHardware() {
    try {
        // Initialize the QSPI display bus for JC3248W535EN
        bus = new Arduino_ESP32QSPI(
            TFT_QSPI_CS,    // CS
            TFT_QSPI_SCK,   // SCK
            TFT_QSPI_D0,    // D0 (MOSI)
            TFT_QSPI_D1,    // D1 (MISO)  
            TFT_QSPI_D2,    // D2 (WP)
            TFT_QSPI_D3     // D3 (HOLD)
        );
        
        if (!bus) {
            Serial.println("❌ Failed to create QSPI bus");
            return false;
        }
        
        // Initialize the AXS15231B controller for JC3248W535EN
        gfx = new Arduino_AXS15231B(
            bus,                    // Bus
            GFX_NOT_DEFINED,       // RST pin (handled by controller)
            0,                     // Rotation
            false,                 // IPS
            BOARD_TFT_WIDTH,       // Width
            BOARD_TFT_HEIGHT       // Height
        );
        
        if (!gfx) {
            Serial.println("❌ Failed to create display controller");
            return false;
        }
        
        // Start the display
        if (!gfx->begin()) {
            Serial.println("❌ Display begin() failed");
            return false;
        }
        
        // Enable backlight
        pinMode(BOARD_TFT_BL, OUTPUT);
        digitalWrite(BOARD_TFT_BL, HIGH);
        
        // Test the display with a simple pattern
        gfx->fillScreen(BLACK);
        gfx->setTextColor(WHITE);
        gfx->setTextSize(2);
        gfx->setCursor(10, 10);
        gfx->println("JC3248W535EN");
        gfx->setCursor(10, 40);
        gfx->println("Display Test");
        
        Serial.println("✅ Display hardware initialized successfully");
        return true;
        
    } catch (...) {
        Serial.println("❌ Exception during display initialization");
        return false;
    }
}

// LVGL display flush callback - actually draw to the physical display
void UIManager::lvgl_display_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    if (!gfx) {
        lv_disp_flush_ready(disp);
        return;
    }
    
    try {
        uint32_t w = (area->x2 - area->x1 + 1);
        uint32_t h = (area->y2 - area->y1 + 1);
        
        // Draw the LVGL buffer to the actual display
        gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)&color_p->full, w, h);
        
        lv_disp_flush_ready(disp);
    } catch (...) {
        lv_disp_flush_ready(disp);
    }
}

// Touch read callback (placeholder)
void UIManager::lvgl_touch_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    // For now, no touch input
    data->state = LV_INDEV_STATE_REL;
    data->point.x = 0;
    data->point.y = 0;
}

void UIManager::createWorkingTestScreen() {
    // Create a colorful test screen to verify everything works
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x001122), 0);  // Dark blue background
    
    // Title label
    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, "JC3248W535EN GPS Logger");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(title, 10, 10);
    
    // Status label
    lv_obj_t* status = lv_label_create(scr);
    lv_label_set_text(status, "Display Working!\nLVGL Active\nTouch: TODO");
    lv_obj_set_style_text_color(status, lv_color_hex(0x00FF00), 0);  // Green
    lv_obj_set_style_text_font(status, UI_FONT_SMALL, 0);
    lv_obj_set_pos(status, 10, 60);
    
    // Test button 1
    lv_obj_t* btn1 = lv_btn_create(scr);
    lv_obj_set_size(btn1, 100, 40);
    lv_obj_set_pos(btn1, 10, 150);
    lv_obj_set_style_bg_color(btn1, lv_color_hex(0xFF0000), 0);  // Red
    
    lv_obj_t* btn1_label = lv_label_create(btn1);
    lv_label_set_text(btn1_label, "RED");
    lv_obj_set_style_text_color(btn1_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(btn1_label);
    
    // Test button 2
    lv_obj_t* btn2 = lv_btn_create(scr);
    lv_obj_set_size(btn2, 100, 40);
    lv_obj_set_pos(btn2, 120, 150);
    lv_obj_set_style_bg_color(btn2, lv_color_hex(0x00FF00), 0);  // Green
    
    lv_obj_t* btn2_label = lv_label_create(btn2);
    lv_label_set_text(btn2_label, "GREEN");
    lv_obj_set_style_text_color(btn2_label, lv_color_hex(0x000000), 0);
    lv_obj_center(btn2_label);
    
    // Test button 3
    lv_obj_t* btn3 = lv_btn_create(scr);
    lv_obj_set_size(btn3, 100, 40);
    lv_obj_set_pos(btn3, 10, 200);
    lv_obj_set_style_bg_color(btn3, lv_color_hex(0x0000FF), 0);  // Blue
    
    lv_obj_t* btn3_label = lv_label_create(btn3);
    lv_label_set_text(btn3_label, "BLUE");
    lv_obj_set_style_text_color(btn3_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(btn3_label);
    
    // System info
    lv_obj_t* info = lv_label_create(scr);
    char infoText[128];
    snprintf(infoText, sizeof(infoText), "Resolution: %dx%d\nFree RAM: %lu bytes", 
             BOARD_TFT_WIDTH, BOARD_TFT_HEIGHT, ESP.getFreeHeap());
    lv_label_set_text(info, infoText);
    lv_obj_set_style_text_color(info, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(info, UI_FONT_SMALL, 0);
    lv_obj_set_pos(info, 10, 250);
    
    // Load the screen
    lv_scr_load(scr);
    mainScreen = scr;
    
    Serial.println("✅ Working test screen created with colors and buttons");
}

void UIManager::update() {
    // For now, just handle LVGL timer
    // We can add data updates later once the display is confirmed working
}

void UIManager::requestUpdate() {
    updateRequested = true;
}

// Screen management (simplified for now)
void UIManager::showScreen(ScreenType screen) {
    Serial.printf("Switching to screen %d\n", screen);
}

void UIManager::nextScreen() {
    Serial.println("Next screen requested");
}

void UIManager::previousScreen() {
    Serial.println("Previous screen requested");
}

ScreenType UIManager::getCurrentScreen() const {
    return currentScreen;
}

// Callback setters
void UIManager::setLoggingCallback(void (*callback)()) {
    loggingCallback = callback;
}

void UIManager::setMenuCallback(void (*callback)()) {
    menuCallback = callback;
}

void UIManager::setSystemCallback(void (*callback)()) {
    systemCallback = callback;
}

void UIManager::forceRefresh() {
    updateRequested = true;
}

// File transfer support (placeholder)
void UIManager::updateFileTransferUI() {
    // Placeholder
}

// Utility functions
lv_color_t UIManager::getSpeedColor(float speed) {
    if (speed > 100) return lv_color_hex(0xFF0000);
    if (speed > 60) return lv_color_hex(0xFFAA00);
    if (speed > 30) return lv_color_hex(0x00AA00);
    return lv_color_hex(0x5555FF);
}

lv_color_t UIManager::getPerformanceColor(float value, float good, float excellent) {
    if (value >= excellent) return lv_color_hex(0x00AA00);
    if (value >= good) return lv_color_hex(0xFFAA00);
    return lv_color_hex(0xFF0000);
}

lv_color_t UIManager::getSignalColor(int strength) {
    if (strength > -50) return lv_color_hex(0x00AA00);
    if (strength > -70) return lv_color_hex(0xFFAA00);
    return lv_color_hex(0xFF0000);
}

void UIManager::setBatteryIcon(uint8_t percentage, bool charging, bool connected) {
    // Placeholder
}

void UIManager::setStatusIcon(lv_obj_t* icon, bool connected, const char* symbol) {
    // Placeholder
}

// Static event handlers
void UIManager::buttonEventHandler(lv_event_t* e) {
    Serial.println("Button event triggered");
}

void UIManager::screenEventHandler(lv_event_t* e) {
    Serial.println("Screen event triggered");
}