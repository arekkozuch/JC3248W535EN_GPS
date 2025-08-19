#include "ui_manager.h"
#include <WiFi.h>

// Simplified approach - remove Arduino_GFX complexity for now
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
    // Store data pointers with safety checks
    this->systemData = sysData;
    this->gpsData = gpsData;
    this->imuData = imuData;
    this->batteryData = battData;
    this->perfStats = perfData;
    
    Serial.println("🎨 Initializing LVGL for JC3248W535EN (simplified)...");
    
    // Initialize LVGL first
    lv_init();
    
    // Simple display buffer allocation
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf[BOARD_TFT_WIDTH * 40];  // Smaller buffer
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, BOARD_TFT_WIDTH * 40);

    // Simple display driver setup
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = BOARD_TFT_WIDTH;
    disp_drv.ver_res = BOARD_TFT_HEIGHT;
    disp_drv.flush_cb = lvgl_display_flush_simple;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Simple touch input driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read_simple;
    lv_indev_drv_register(&indev_drv);

    Serial.println("✅ LVGL initialized (simplified)");
    
    // Create a simple test screen first
    createSimpleTestScreen();
    
    Serial.println("✅ UIManager initialized with test screen");
}

// Simplified display flush - just mark as ready
void UIManager::lvgl_display_flush_simple(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    // For now, just mark as ready - we'll add actual drawing later
    lv_disp_flush_ready(disp);
}

// Simplified touch read - no actual touch for now
void UIManager::lvgl_touch_read_simple(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    data->state = LV_INDEV_STATE_REL;
    data->point.x = 0;
    data->point.y = 0;
}

void UIManager::createSimpleTestScreen() {
    // Create a simple test screen to verify LVGL is working
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);  // Black background
    
    // Create a simple label
    lv_obj_t* label = lv_label_create(scr);
    lv_label_set_text(label, "GPS Logger\nJC3248W535EN\nTest Screen");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);  // White text
    lv_obj_center(label);
    
    // Create a simple button
    lv_obj_t* btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 100, 40);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x0000FF), 0);  // Blue button
    
    lv_obj_t* btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "TEST");
    lv_obj_set_style_text_color(btn_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(btn_label);
    
    // Load the screen
    lv_scr_load(scr);
    mainScreen = scr;
    
    Serial.println("✅ Simple test screen created");
}

void UIManager::update() {
    // Just handle LVGL timer for now
    // Don't do any complex updates until basic display works
}

void UIManager::requestUpdate() {
    updateRequested = true;
}

// Simplified screen management
void UIManager::showScreen(ScreenType screen) {
    // For now, do nothing - just keep the test screen
}

void UIManager::nextScreen() {
    Serial.println("Next screen pressed (test)");
}

void UIManager::previousScreen() {
    Serial.println("Previous screen pressed (test)");
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
    // Placeholder - do nothing for now
}

// Utility functions (simplified)
lv_color_t UIManager::getSpeedColor(float speed) {
    if (speed > 100) return lv_color_hex(0xFF0000);  // Red
    if (speed > 60) return lv_color_hex(0xFFAA00);   // Orange
    if (speed > 30) return lv_color_hex(0x00AA00);   // Green
    return lv_color_hex(0x5555FF);                   // Blue
}

lv_color_t UIManager::getPerformanceColor(float value, float good, float excellent) {
    if (value >= excellent) return lv_color_hex(0x00AA00);  // Green
    if (value >= good) return lv_color_hex(0xFFAA00);       // Orange
    return lv_color_hex(0xFF0000);                          // Red
}

lv_color_t UIManager::getSignalColor(int strength) {
    if (strength > -50) return lv_color_hex(0x00AA00);      // Green
    if (strength > -70) return lv_color_hex(0xFFAA00);      // Orange
    return lv_color_hex(0xFF0000);                          // Red
}

void UIManager::setBatteryIcon(uint8_t percentage, bool charging, bool connected) {
    // Placeholder
}

void UIManager::setStatusIcon(lv_obj_t* icon, bool connected, const char* symbol) {
    // Placeholder
}

// Static event handlers (simplified)
void UIManager::buttonEventHandler(lv_event_t* e) {
    Serial.println("Button pressed (test)");
}

void UIManager::screenEventHandler(lv_event_t* e) {
    Serial.println("Screen touched (test)");
}