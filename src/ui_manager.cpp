#include "ui_manager.h"
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <WiFi.h>

// Global display objects for JC3248W535EN
Arduino_DataBus *bus;
Arduino_GFX *gfx;

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
    this->systemData = sysData;
    this->gpsData = gpsData;
    this->imuData = imuData;
    this->batteryData = battData;
    this->perfStats = perfData;
    
    // Initialize display hardware
    initializeHardware();
    
    // Initialize LVGL
    initializeLVGL();
    
    // Create the main UI layout
    createMainLayout();
    
    // Set initial screen
    showScreen(SCREEN_SPEEDOMETER);
    
    Serial.println("✅ UIManager initialized for JC3248W535EN");
}

bool UIManager::initializeHardware() {
    Serial.println("🖥️ Initializing JC3248W535EN display...");
    
    // Initialize Arduino_GFX for the AXS15231B controller
    bus = new Arduino_ESP32QSPI(TFT_QSPI_CS, TFT_QSPI_SCK, TFT_QSPI_D0, TFT_QSPI_D1, TFT_QSPI_D2, TFT_QSPI_D3);
    gfx = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, BOARD_TFT_WIDTH, BOARD_TFT_HEIGHT);
    
    if (!gfx->begin()) {
        Serial.println("❌ Display initialization failed!");
        return false;
    }
    
    // Enable backlight
    pinMode(BOARD_TFT_BL, OUTPUT);
    digitalWrite(BOARD_TFT_BL, HIGH);
    
    // Initialize touch controller
    initializeTouch();
    
    Serial.println("✅ JC3248W535EN hardware initialized");
    return true;
}

bool UIManager::initializeTouch() {
    Serial.println("🖱️ Initializing capacitive touch...");
    
    // Initialize I2C for touch
    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
    Wire.setClock(TOUCH_I2C_CLOCK);
    
    // Configure touch pins
    pinMode(BOARD_SENSOR_IRQ, INPUT_PULLUP);
    pinMode(BOARD_TOUCH_RST, OUTPUT);
    
    // Reset touch controller
    digitalWrite(BOARD_TOUCH_RST, LOW);
    delay(200);
    digitalWrite(BOARD_TOUCH_RST, HIGH);
    delay(200);
    
    // Test communication
    Wire.beginTransmission(TOUCH_ADDR);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
        Serial.println("✅ Touch controller initialized");
        return true;
    } else {
        Serial.println("❌ Touch controller initialization failed");
        return false;
    }
}

void UIManager::initializeLVGL() {
    Serial.println("🎨 Initializing LVGL for JC3248W535EN...");
    
    lv_init();
    
    // Create display buffer - using smaller buffer for 320x480 display
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(BOARD_TFT_WIDTH * 60 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    if (!buf1) {
        Serial.println("❌ Failed to allocate display buffer");
        return;
    }
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, BOARD_TFT_WIDTH * 60);

    // Display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = BOARD_TFT_WIDTH;
    disp_drv.ver_res = BOARD_TFT_HEIGHT;
    disp_drv.flush_cb = lvgl_display_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Touch input driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read;
    lv_indev_drv_register(&indev_drv);

    Serial.println("✅ LVGL initialized for JC3248W535EN");
}

// LVGL display flush callback for Arduino_GFX
void UIManager::lvgl_display_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)&color_p->full, w, h);
    
    lv_disp_flush_ready(disp);
}

// LVGL touch input callback for JC3248W535EN
void UIManager::lvgl_touch_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    uint16_t x, y;
    
    // Get instance from user data
    UIManager* instance = (UIManager*)indev_driver->user_data;
    if (!instance) {
        // Fallback - try to read touch directly
        static UIManager* staticInstance = nullptr;
        if (!staticInstance) {
            // This is a fallback, normally we'd set user_data properly
            data->state = LV_INDEV_STATE_REL;
            return;
        }
        instance = staticInstance;
    }
    
    if (instance->readTouchPoint(x, y)) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PR;
        
        if (instance->systemData) {
            instance->systemData->lastDisplayActivity = millis();
        }
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

bool UIManager::readTouchPoint(uint16_t &x, uint16_t &y) {
    uint8_t data[AXS_MAX_TOUCH_NUMBER * 6 + 2] = {0};
    
    // Touch read command (from working sample)
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
        
        // Filter invalid readings
        if (rawX == 273 && rawY == 273) return false;
        if (rawX > 4000 || rawY > 4000) return false;
        
        // Apply coordinate mapping for 320x480 portrait display
        x = map(rawX, 0, 320, 0, BOARD_TFT_WIDTH);
        y = map(rawY, 0, 480, 0, BOARD_TFT_HEIGHT);
        
        // Ensure coordinates are within bounds
        x = constrain(x, 0, BOARD_TFT_WIDTH - 1);
        y = constrain(y, 0, BOARD_TFT_HEIGHT - 1);
        
        return true;
    }
    
    return false;
}

void UIManager::createMainLayout() {
    // Create main screen
    mainScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(mainScreen, UI_COLOR_BACKGROUND, 0);
    lv_obj_set_style_pad_all(mainScreen, 0, 0);
    lv_scr_load(mainScreen);
    
    // Create layout panels adapted for 320x480 portrait
    createHeader();
    createFooter();
    createStatusBar();
    createScreens();
    
    Serial.println("✅ Main UI layout created for JC3248W535EN");
}

void UIManager::createHeader() {
    headerPanel = lv_obj_create(mainScreen);
    lv_obj_set_size(headerPanel, BOARD_TFT_WIDTH, UI_HEADER_HEIGHT);
    lv_obj_set_pos(headerPanel, 0, 0);
    lv_obj_set_style_bg_color(headerPanel, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(headerPanel, 1, 0);
    lv_obj_set_style_border_color(headerPanel, UI_COLOR_BORDER, 0);
    lv_obj_set_style_pad_all(headerPanel, 5, 0);
    lv_obj_set_style_radius(headerPanel, 0, 0);
    
    // Title label (left side) - adapted for narrower screen
    titleLabel = lv_label_create(headerPanel);
    lv_label_set_text(titleLabel, "GPS LOG");
    lv_obj_set_style_text_color(titleLabel, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(titleLabel, UI_FONT_SMALL, 0);  // Smaller font for narrower screen
    lv_obj_set_pos(titleLabel, 5, 5);
    
    // Time label (center) - moved closer
    timeLabel = lv_label_create(headerPanel);
    lv_label_set_text(timeLabel, "00:00:00");
    lv_obj_set_style_text_color(timeLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(timeLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(timeLabel, 120, 5);
    
    // Battery display (right side) - adjusted position
    batteryIcon = lv_label_create(headerPanel);
    lv_label_set_text(batteryIcon, ICON_BATTERY);
    lv_obj_set_style_text_color(batteryIcon, UI_COLOR_SUCCESS, 0);
    lv_obj_set_pos(batteryIcon, 250, 5);
    
    batteryLabel = lv_label_create(headerPanel);
    lv_label_set_text(batteryLabel, "100%");
    lv_obj_set_style_text_color(batteryLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(batteryLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(batteryLabel, 270, 5);
}

void UIManager::createFooter() {
    footerPanel = lv_obj_create(mainScreen);
    lv_obj_set_size(footerPanel, BOARD_TFT_WIDTH, UI_FOOTER_HEIGHT);
    lv_obj_set_pos(footerPanel, 0, BOARD_TFT_HEIGHT - UI_FOOTER_HEIGHT);
    lv_obj_set_style_bg_color(footerPanel, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(footerPanel, 1, 0);
    lv_obj_set_style_border_color(footerPanel, UI_COLOR_BORDER, 0);
    lv_obj_set_style_pad_all(footerPanel, 2, 0);
    lv_obj_set_style_radius(footerPanel, 0, 0);
    
    // Buttons adapted for 320px width
    int buttonWidth = 60;
    int buttonSpacing = 4;
    
    // Previous button
    prevButton = lv_btn_create(footerPanel);
    lv_obj_set_size(prevButton, buttonWidth, UI_BUTTON_HEIGHT);
    lv_obj_set_pos(prevButton, 5, 5);
    lv_obj_set_style_bg_color(prevButton, UI_COLOR_SURFACE_2, 0);
    lv_obj_set_style_bg_color(prevButton, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(prevButton, UI_COLOR_BORDER, 0);
    lv_obj_set_style_radius(prevButton, 8, 0);
    lv_obj_add_event_cb(prevButton, buttonEventHandler, LV_EVENT_CLICKED, this);
    
    lv_obj_t* prevLabel = lv_label_create(prevButton);
    lv_label_set_text(prevLabel, ICON_PREV);
    lv_obj_set_style_text_color(prevLabel, UI_COLOR_TEXT, 0);
    lv_obj_center(prevLabel);
    
    // Log button
    logButton = lv_btn_create(footerPanel);
    lv_obj_set_size(logButton, buttonWidth, UI_BUTTON_HEIGHT);
    lv_obj_set_pos(logButton, 5 + buttonWidth + buttonSpacing, 5);
    lv_obj_set_style_bg_color(logButton, UI_COLOR_SUCCESS, 0);
    lv_obj_set_style_bg_color(logButton, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_radius(logButton, 8, 0);
    lv_obj_add_event_cb(logButton, buttonEventHandler, LV_EVENT_CLICKED, this);
    
    lv_obj_t* logLabel = lv_label_create(logButton);
    lv_label_set_text(logLabel, "LOG");
    lv_obj_set_style_text_color(logLabel, UI_COLOR_BACKGROUND, 0);
    lv_obj_center(logLabel);
    
    // Menu button
    menuButton = lv_btn_create(footerPanel);
    lv_obj_set_size(menuButton, buttonWidth, UI_BUTTON_HEIGHT);
    lv_obj_set_pos(menuButton, 5 + 2 * (buttonWidth + buttonSpacing), 5);
    lv_obj_set_style_bg_color(menuButton, UI_COLOR_INFO, 0);
    lv_obj_set_style_bg_color(menuButton, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
    lv_obj_set_style_radius(menuButton, 8, 0);
    lv_obj_add_event_cb(menuButton, buttonEventHandler, LV_EVENT_CLICKED, this);
    
    lv_obj_t* menuLabel = lv_label_create(menuButton);
    lv_label_set_text(menuLabel, ICON_MENU);
    lv_obj_set_style_text_color(menuLabel, UI_COLOR_BACKGROUND, 0);
    lv_obj_center(menuLabel);
    
    // System button  
    systemButton = lv_btn_create(footerPanel);
    lv_obj_set_size(systemButton, buttonWidth, UI_BUTTON_HEIGHT);
    lv_obj_set_pos(systemButton, 5 + 3 * (buttonWidth + buttonSpacing), 5);
    lv_obj_set_style_bg_color(systemButton, UI_COLOR_WARNING, 0);
    lv_obj_set_style_bg_color(systemButton, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_radius(systemButton, 8, 0);
    lv_obj_add_event_cb(systemButton, buttonEventHandler, LV_EVENT_CLICKED, this);
    
    lv_obj_t* systemLabel = lv_label_create(systemButton);
    lv_label_set_text(systemLabel, ICON_SYSTEM);
    lv_obj_set_style_text_color(systemLabel, UI_COLOR_BACKGROUND, 0);
    lv_obj_center(systemLabel);
    
    // Next button
    nextButton = lv_btn_create(footerPanel);
    lv_obj_set_size(nextButton, buttonWidth, UI_BUTTON_HEIGHT);
    lv_obj_set_pos(nextButton, BOARD_TFT_WIDTH - buttonWidth - 5, 5);
    lv_obj_set_style_bg_color(nextButton, UI_COLOR_SURFACE_2, 0);
    lv_obj_set_style_bg_color(nextButton, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(nextButton, UI_COLOR_BORDER, 0);
    lv_obj_set_style_radius(nextButton, 8, 0);
    lv_obj_add_event_cb(nextButton, buttonEventHandler, LV_EVENT_CLICKED, this);
    
    lv_obj_t* nextLabel = lv_label_create(nextButton);
    lv_label_set_text(nextLabel, ICON_NEXT);
    lv_obj_set_style_text_color(nextLabel, UI_COLOR_TEXT, 0);
    lv_obj_center(nextLabel);
}

void UIManager::createStatusBar() {
    statusBar = lv_obj_create(headerPanel);
    lv_obj_set_size(statusBar, 120, 25);
    lv_obj_set_pos(statusBar, BOARD_TFT_WIDTH - 130, 0);
    lv_obj_set_style_bg_opa(statusBar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(statusBar, 0, 0);
    lv_obj_set_style_pad_all(statusBar, 2, 0);
    
    // Status icons positioned for portrait layout
    gpsStatus = lv_label_create(statusBar);
    lv_label_set_text(gpsStatus, ICON_GPS);
    lv_obj_set_style_text_color(gpsStatus, UI_COLOR_DANGER, 0);
    lv_obj_set_pos(gpsStatus, 0, 0);
    
    wifiStatus = lv_label_create(statusBar);
    lv_label_set_text(wifiStatus, ICON_WIFI);
    lv_obj_set_style_text_color(wifiStatus, UI_COLOR_DANGER, 0);
    lv_obj_set_pos(wifiStatus, 18, 0);
    
    bleStatus = lv_label_create(statusBar);
    lv_label_set_text(bleStatus, ICON_BLE);
    lv_obj_set_style_text_color(bleStatus, UI_COLOR_DANGER, 0);
    lv_obj_set_pos(bleStatus, 35, 0);
    
    sdStatus = lv_label_create(statusBar);
    lv_label_set_text(sdStatus, ICON_SD);
    lv_obj_set_style_text_color(sdStatus, UI_COLOR_DANGER, 0);
    lv_obj_set_pos(sdStatus, 50, 0);
}

void UIManager::createScreens() {
    createSpeedometerScreen();
    createMotionScreen();
    createSystemScreen();
    createPerformanceScreen();
}

void UIManager::createSpeedometerScreen() {
    speedometerPanel = lv_obj_create(mainScreen);
    lv_obj_set_size(speedometerPanel, BOARD_TFT_WIDTH, 
                    BOARD_TFT_HEIGHT - UI_HEADER_HEIGHT - UI_FOOTER_HEIGHT);
    lv_obj_set_pos(speedometerPanel, 0, UI_HEADER_HEIGHT);
    lv_obj_set_style_bg_opa(speedometerPanel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(speedometerPanel, 0, 0);
    lv_obj_set_style_pad_all(speedometerPanel, 10, 0);
    
    // Large speed display (centered for portrait layout)
    speedValue = lv_label_create(speedometerPanel);
    lv_label_set_text(speedValue, "NO FIX");
    lv_obj_set_style_text_color(speedValue, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(speedValue, UI_FONT_EXTRA_LARGE, 0);
    lv_obj_set_pos(speedValue, 80, 50);  // Centered for 320px width
    
    // Speed unit
    speedUnit = lv_label_create(speedometerPanel);
    lv_label_set_text(speedUnit, "");
    lv_obj_set_style_text_color(speedUnit, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(speedUnit, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(speedUnit, 200, 90);
    
    // Speed in MPH (smaller)
    speedMph = lv_label_create(speedometerPanel);
    lv_label_set_text(speedMph, "");
    lv_obj_set_style_text_color(speedMph, UI_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(speedMph, UI_FONT_SMALL, 0);
    lv_obj_set_pos(speedMph, 120, 130);
    
    // Coordinates (moved down for portrait layout)
    coordsLabel = lv_label_create(speedometerPanel);
    lv_label_set_text(coordsLabel, "LAT: -- LON: --");
    lv_obj_set_style_text_color(coordsLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(coordsLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(coordsLabel, 10, 170);
    
    // Altitude and heading
    altitudeLabel = lv_label_create(speedometerPanel);
    lv_label_set_text(altitudeLabel, "ALT: --m");
    lv_obj_set_style_text_color(altitudeLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(altitudeLabel, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(altitudeLabel, 10, 200);
    
    headingLabel = lv_label_create(speedometerPanel);
    lv_label_set_text(headingLabel, "HDG: --°");
    lv_obj_set_style_text_color(headingLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(headingLabel, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(headingLabel, 170, 200);
    
    // Date and time
    dateLabel = lv_label_create(speedometerPanel);
    lv_label_set_text(dateLabel, "--/--/-- UTC");
    lv_obj_set_style_text_color(dateLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(dateLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(dateLabel, 10, 230);
    
    // Acceleration indicator
    accelerationIndicator = lv_label_create(speedometerPanel);
    lv_label_set_text(accelerationIndicator, "");
    lv_obj_set_style_text_color(accelerationIndicator, UI_COLOR_SUCCESS, 0);
    lv_obj_set_style_text_font(accelerationIndicator, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(accelerationIndicator, 200, 130);
}

void UIManager::createMotionScreen() {
    motionPanel = lv_obj_create(mainScreen);
    lv_obj_set_size(motionPanel, BOARD_TFT_WIDTH, 
                   BOARD_TFT_HEIGHT - UI_HEADER_HEIGHT - UI_FOOTER_HEIGHT);
    lv_obj_set_pos(motionPanel, 0, UI_HEADER_HEIGHT);
    lv_obj_set_style_bg_opa(motionPanel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(motionPanel, 0, 0);
    lv_obj_set_style_pad_all(motionPanel, 10, 0);
    
    // Motion indicator
    motionIndicator = lv_label_create(motionPanel);
    lv_label_set_text(motionIndicator, "No Motion");
    lv_obj_set_style_text_color(motionIndicator, UI_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(motionIndicator, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(motionIndicator, 80, 10);
    
    // Accelerometer section (stacked for portrait layout)
    lv_obj_t* accelTitle = lv_label_create(motionPanel);
    lv_label_set_text(accelTitle, "Accelerometer (g)");
    lv_obj_set_style_text_color(accelTitle, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(accelTitle, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(accelTitle, 20, 50);
    
    accelXLabel = lv_label_create(motionPanel);
    lv_label_set_text(accelXLabel, "X: 0.00");
    lv_obj_set_style_text_color(accelXLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(accelXLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(accelXLabel, 20, 80);
    
    accelYLabel = lv_label_create(motionPanel);
    lv_label_set_text(accelYLabel, "Y: 0.00");
    lv_obj_set_style_text_color(accelYLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(accelYLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(accelYLabel, 20, 100);
    
    accelZLabel = lv_label_create(motionPanel);
    lv_label_set_text(accelZLabel, "Z: 0.00");
    lv_obj_set_style_text_color(accelZLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(accelZLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(accelZLabel, 20, 120);
    
    magnitudeLabel = lv_label_create(motionPanel);
    lv_label_set_text(magnitudeLabel, "Mag: 0.00g");
    lv_obj_set_style_text_color(magnitudeLabel, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(magnitudeLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(magnitudeLabel, 20, 140);
    
    // Gyroscope section (right side of portrait layout)
    lv_obj_t* gyroTitle = lv_label_create(motionPanel);
    lv_label_set_text(gyroTitle, "Gyroscope (°/s)");
    lv_obj_set_style_text_color(gyroTitle, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(gyroTitle, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(gyroTitle, 20, 170);
    
    gyroXLabel = lv_label_create(motionPanel);
    lv_label_set_text(gyroXLabel, "X: 0.0");
    lv_obj_set_style_text_color(gyroXLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(gyroXLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(gyroXLabel, 20, 200);
    
    gyroYLabel = lv_label_create(motionPanel);
    lv_label_set_text(gyroYLabel, "Y: 0.0");
    lv_obj_set_style_text_color(gyroYLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(gyroYLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(gyroYLabel, 20, 220);
    
    gyroZLabel = lv_label_create(motionPanel);
    lv_label_set_text(gyroZLabel, "Z: 0.0");
    lv_obj_set_style_text_color(gyroZLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(gyroZLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(gyroZLabel, 20, 240);
    
    temperatureLabel = lv_label_create(motionPanel);
    lv_label_set_text(temperatureLabel, "Temp: 0.0°C");
    lv_obj_set_style_text_color(temperatureLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(temperatureLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(temperatureLabel, 20, 260);
}

void UIManager::createSystemScreen() {
    systemPanel = lv_obj_create(mainScreen);
    lv_obj_set_size(systemPanel, BOARD_TFT_WIDTH, 
                   BOARD_TFT_HEIGHT - UI_HEADER_HEIGHT - UI_FOOTER_HEIGHT);
    lv_obj_set_pos(systemPanel, 0, UI_HEADER_HEIGHT);
    lv_obj_set_style_bg_opa(systemPanel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(systemPanel, 0, 0);
    lv_obj_set_style_pad_all(systemPanel, 10, 0);
    
    // Connectivity section (stacked for portrait layout)
    lv_obj_t* connTitle = lv_label_create(systemPanel);
    lv_label_set_text(connTitle, "Connectivity");
    lv_obj_set_style_text_color(connTitle, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(connTitle, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(connTitle, 20, 10);
    
    wifiInfoLabel = lv_label_create(systemPanel);
    lv_label_set_text(wifiInfoLabel, "WiFi: Disconnected");
    lv_obj_set_style_text_color(wifiInfoLabel, UI_COLOR_DANGER, 0);
    lv_obj_set_style_text_font(wifiInfoLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(wifiInfoLabel, 20, 35);
    
    bleInfoLabel = lv_label_create(systemPanel);
    lv_label_set_text(bleInfoLabel, "BLE: Disconnected");
    lv_obj_set_style_text_color(bleInfoLabel, UI_COLOR_DANGER, 0);
    lv_obj_set_style_text_font(bleInfoLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(bleInfoLabel, 20, 55);
    
    sdInfoLabel = lv_label_create(systemPanel);
    lv_label_set_text(sdInfoLabel, "SD: Not Available");
    lv_obj_set_style_text_color(sdInfoLabel, UI_COLOR_DANGER, 0);
    lv_obj_set_style_text_font(sdInfoLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(sdInfoLabel, 20, 75);
    
    touchInfoLabel = lv_label_create(systemPanel);
    lv_label_set_text(touchInfoLabel, "Touch: Not Available");
    lv_obj_set_style_text_color(touchInfoLabel, UI_COLOR_DANGER, 0);
    lv_obj_set_style_text_font(touchInfoLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(touchInfoLabel, 20, 95);
    
    imuInfoLabel = lv_label_create(systemPanel);
    lv_label_set_text(imuInfoLabel, "IMU: Not Available");
    lv_obj_set_style_text_color(imuInfoLabel, UI_COLOR_DANGER, 0);
    lv_obj_set_style_text_font(imuInfoLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(imuInfoLabel, 20, 115);
    
    // System info section
    lv_obj_t* sysTitle = lv_label_create(systemPanel);
    lv_label_set_text(sysTitle, "System Info");
    lv_obj_set_style_text_color(sysTitle, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(sysTitle, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(sysTitle, 20, 145);
    
    ramLabel = lv_label_create(systemPanel);
    lv_label_set_text(ramLabel, "Free RAM: 0 bytes");
    lv_obj_set_style_text_color(ramLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(ramLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(ramLabel, 20, 170);
    
    uptimeLabel = lv_label_create(systemPanel);
    lv_label_set_text(uptimeLabel, "Uptime: 0h 0m");
    lv_obj_set_style_text_color(uptimeLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(uptimeLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(uptimeLabel, 20, 190);
    
    batteryInfoLabel = lv_label_create(systemPanel);
    lv_label_set_text(batteryInfoLabel, "Battery: 0% (0.0V)");
    lv_obj_set_style_text_color(batteryInfoLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(batteryInfoLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(batteryInfoLabel, 20, 210);
    
    chargingInfoLabel = lv_label_create(systemPanel);
    lv_label_set_text(chargingInfoLabel, "Not Charging");
    lv_obj_set_style_text_color(chargingInfoLabel, UI_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(chargingInfoLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(chargingInfoLabel, 20, 230);
    
    logStatusLabel = lv_label_create(systemPanel);
    lv_label_set_text(logStatusLabel, "Logging: Stopped");
    lv_obj_set_style_text_color(logStatusLabel, UI_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(logStatusLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(logStatusLabel, 20, 250);
}

void UIManager::createPerformanceScreen() {
    performancePanel = lv_obj_create(mainScreen);
    lv_obj_set_size(performancePanel, BOARD_TFT_WIDTH, 
                    BOARD_TFT_HEIGHT - UI_HEADER_HEIGHT - UI_FOOTER_HEIGHT);
    lv_obj_set_pos(performancePanel, 0, UI_HEADER_HEIGHT);
    lv_obj_set_style_bg_opa(performancePanel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(performancePanel, 0, 0);
    lv_obj_set_style_pad_all(performancePanel, 10, 0);
    
    // Throughput section (stacked for portrait layout)
    lv_obj_t* throughputTitle = lv_label_create(performancePanel);
    lv_label_set_text(throughputTitle, "Data Throughput");
    lv_obj_set_style_text_color(throughputTitle, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(throughputTitle, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(throughputTitle, 20, 10);
    
    totalPacketsLabel = lv_label_create(performancePanel);
    lv_label_set_text(totalPacketsLabel, "Total: 0");
    lv_obj_set_style_text_color(totalPacketsLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(totalPacketsLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(totalPacketsLabel, 20, 35);
    
    droppedPacketsLabel = lv_label_create(performancePanel);
    lv_label_set_text(droppedPacketsLabel, "Dropped: 0");
    lv_obj_set_style_text_color(droppedPacketsLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(droppedPacketsLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(droppedPacketsLabel, 20, 55);
    
    dropRateLabel = lv_label_create(performancePanel);
    lv_label_set_text(dropRateLabel, "Drop Rate: 0.0%");
    lv_obj_set_style_text_color(dropRateLabel, UI_COLOR_SUCCESS, 0);
    lv_obj_set_style_text_font(dropRateLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(dropRateLabel, 20, 75);
    
    perfStatusLabel = lv_label_create(performancePanel);
    lv_label_set_text(perfStatusLabel, "Status: STARTING");
    lv_obj_set_style_text_color(perfStatusLabel, UI_COLOR_WARNING, 0);
    lv_obj_set_style_text_font(perfStatusLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(perfStatusLabel, 20, 95);
    
    memoryLabel = lv_label_create(performancePanel);
    lv_label_set_text(memoryLabel, "Free RAM: 0 bytes");
    lv_obj_set_style_text_color(memoryLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(memoryLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(memoryLabel, 20, 115);
    
    // Timing section
    lv_obj_t* timingTitle = lv_label_create(performancePanel);
    lv_label_set_text(timingTitle, "Timing Analysis");
    lv_obj_set_style_text_color(timingTitle, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(timingTitle, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(timingTitle, 20, 145);
    
    minDeltaLabel = lv_label_create(performancePanel);
    lv_label_set_text(minDeltaLabel, "Min Δ: 0ms");
    lv_obj_set_style_text_color(minDeltaLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(minDeltaLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(minDeltaLabel, 20, 170);
    
    maxDeltaLabel = lv_label_create(performancePanel);
    lv_label_set_text(maxDeltaLabel, "Max Δ: 0ms");
    lv_obj_set_style_text_color(maxDeltaLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(maxDeltaLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(maxDeltaLabel, 20, 190);
    
    avgDeltaLabel = lv_label_create(performancePanel);
    lv_label_set_text(avgDeltaLabel, "Avg Δ: 0ms");
    lv_obj_set_style_text_color(avgDeltaLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(avgDeltaLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(avgDeltaLabel, 20, 210);
    
    dataRateLabel = lv_label_create(performancePanel);
    lv_label_set_text(dataRateLabel, "Rate: 0.0 pps");
    lv_obj_set_style_text_color(dataRateLabel, UI_COLOR_SUCCESS, 0);
    lv_obj_set_style_text_font(dataRateLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(dataRateLabel, 20, 230);
    
    resetStatsLabel = lv_label_create(performancePanel);
    lv_label_set_text(resetStatsLabel, "Touch center to reset stats");
    lv_obj_set_style_text_color(resetStatsLabel, UI_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(resetStatsLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(resetStatsLabel, 20, 260);
}

// File Transfer UI - adapted for portrait layout
void UIManager::updateFileTransferUI() {
    if (!fileTransferPtr) return;
    
    if (fileTransferPtr->active) {
        if (!progressBar) {
            progressBar = lv_bar_create(mainScreen);
            lv_obj_set_size(progressBar, BOARD_TFT_WIDTH - 40, 15);
            lv_obj_align(progressBar, LV_ALIGN_BOTTOM_MID, 0, -25);
            lv_obj_set_style_bg_color(progressBar, lv_color_hex(0x333333), LV_PART_MAIN);
            lv_obj_set_style_bg_color(progressBar, lv_color_hex(0x00AA00), LV_PART_INDICATOR);
            
            transferLabel = lv_label_create(mainScreen);
            lv_obj_align(transferLabel, LV_ALIGN_BOTTOM_MID, 0, -5);
            lv_obj_set_style_text_color(transferLabel, lv_color_white(), 0);
            lv_obj_set_style_text_font(transferLabel, UI_FONT_SMALL, 0);
        }
        
        lv_bar_set_value(progressBar, (int32_t)fileTransferPtr->progressPercent, LV_ANIM_OFF);
        
        String text = fileTransferPtr->filename + " (" + 
                     String(fileTransferPtr->progressPercent, 1) + "%)";
        if (fileTransferPtr->estimatedTimeRemaining > 0) {
            text += " - " + String(fileTransferPtr->estimatedTimeRemaining / 1000) + "s";
        }
        lv_label_set_text(transferLabel, text.c_str());
        
        lv_obj_clear_flag(progressBar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(transferLabel, LV_OBJ_FLAG_HIDDEN);
        
    } else if (progressBar) {
        lv_obj_add_flag(progressBar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(transferLabel, LV_OBJ_FLAG_HIDDEN);
    }
}

void UIManager::update() {
    unsigned long now = millis();
    
    // Check if update is needed and enough time has passed
    if (!updateRequested && (now - lastUpdate < UPDATE_INTERVAL)) {
        return;
    }
    
    // Update header periodically
    if (now - lastHeaderUpdate >= HEADER_UPDATE_INTERVAL) {
        updateHeader();
        lastHeaderUpdate = now;
    }
    
    // Update status bar less frequently
    if (now - lastStatusUpdate >= STATUS_UPDATE_INTERVAL) {
        updateStatusBar();
        lastStatusUpdate = now;
    }
    
    // Update current screen content
    updateCurrentScreen();
    
    // Update file transfer UI
    updateFileTransferUI();
    
    updateRequested = false;
    lastUpdate = now;
}

void UIManager::requestUpdate() {
    updateRequested = true;
}

void UIManager::updateHeader() {
    // Update time display
    if (gpsData && gpsData->fixType >= 2) {
        char timeStr[32];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", 
                 gpsData->hour, gpsData->minute, gpsData->second);
        lv_label_set_text(timeLabel, timeStr);
    } else {
        lv_label_set_text(timeLabel, "--:--:--");
    }
    
    // Update battery display
    if (batteryData) {
        char battStr[16];
        snprintf(battStr, sizeof(battStr), "%d%%", batteryData->percentage);
        lv_label_set_text(batteryLabel, battStr);
        
        // Set battery color based on level
        lv_color_t battColor = UI_COLOR_SUCCESS;
        if (batteryData->percentage < 50) battColor = UI_COLOR_WARNING;
        if (batteryData->percentage < 20) battColor = UI_COLOR_DANGER;
        if (batteryData->isCharging) battColor = UI_COLOR_PRIMARY;
        
        lv_obj_set_style_text_color(batteryIcon, battColor, 0);
        lv_obj_set_style_text_color(batteryLabel, battColor, 0);
    }
}

void UIManager::updateStatusBar() {
    if (!systemData) return;
    
    // GPS status
    if (gpsData && gpsData->fixType >= 3) {
        lv_obj_set_style_text_color(gpsStatus, UI_COLOR_SUCCESS, 0);
    } else if (gpsData && gpsData->fixType == 2) {
        lv_obj_set_style_text_color(gpsStatus, UI_COLOR_WARNING, 0);
    } else {
        lv_obj_set_style_text_color(gpsStatus, UI_COLOR_DANGER, 0);
    }
    
    // WiFi status
    if (WiFi.status() == WL_CONNECTED) {
        lv_obj_set_style_text_color(wifiStatus, UI_COLOR_SUCCESS, 0);
    } else {
        lv_obj_set_style_text_color(wifiStatus, UI_COLOR_DANGER, 0);
    }
    
    // BLE status (placeholder for now)
    lv_obj_set_style_text_color(bleStatus, UI_COLOR_PRIMARY, 0);
    
    // SD status
    if (systemData->sdCardAvailable) {
        lv_obj_set_style_text_color(sdStatus, UI_COLOR_SUCCESS, 0);
    } else {
        lv_obj_set_style_text_color(sdStatus, UI_COLOR_DANGER, 0);
    }
}

void UIManager::updateCurrentScreen() {
    switch (currentScreen) {
        case SCREEN_SPEEDOMETER:
            updateSpeedometerScreen();
            break;
        case SCREEN_MOTION:
            updateMotionScreen();
            break;
        case SCREEN_SYSTEM:
            updateSystemScreen();
            break;
        case SCREEN_PERFORMANCE:
            updatePerformanceScreen();
            break;
    }
}

void UIManager::updateSpeedometerScreen() {
    if (!gpsData) return;
    
    // Update speed display
    if (gpsData->fixType >= 2) {
        char speedStr[16];
        if (gpsData->speed < 10) {
            snprintf(speedStr, sizeof(speedStr), "%.1f", gpsData->speed);
        } else {
            snprintf(speedStr, sizeof(speedStr), "%d", (int)gpsData->speed);
        }
        lv_label_set_text(speedValue, speedStr);
        lv_obj_set_style_text_color(speedValue, getSpeedColor(gpsData->speed), 0);
        
        // Speed in MPH
        float speedMphValue = gpsData->speed * 0.621371f;
        char mphStr[16];
        snprintf(mphStr, sizeof(mphStr), "%.0f mph", speedMphValue);
        lv_label_set_text(speedMph, mphStr);
        
        // Unit label
        lv_label_set_text(speedUnit, "km/h");
        
        // Coordinates
        char coordStr[64];
        snprintf(coordStr, sizeof(coordStr), "LAT: %.5f° LON: %.5f°", 
                 gpsData->latitude, gpsData->longitude);
        lv_label_set_text(coordsLabel, coordStr);
        
        // Altitude and heading
        char altStr[32];
        snprintf(altStr, sizeof(altStr), "ALT: %dm", gpsData->altitude);
        lv_label_set_text(altitudeLabel, altStr);
        
        char hdgStr[32];
        snprintf(hdgStr, sizeof(hdgStr), "HDG: %.0f°", gpsData->heading);
        lv_label_set_text(headingLabel, hdgStr);
        
        // Date
        char dateStr[32];
        snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%04d", 
                 gpsData->day, gpsData->month, gpsData->year);
        lv_label_set_text(dateLabel, dateStr);
    } else {
        lv_label_set_text(speedValue, "---");
        lv_obj_set_style_text_color(speedValue, UI_COLOR_DANGER, 0);
        lv_label_set_text(speedUnit, "");
        lv_label_set_text(speedMph, "-- mph");
        lv_label_set_text(coordsLabel, "LAT: -- LON: --");
        lv_label_set_text(altitudeLabel, "ALT: --m");
        lv_label_set_text(headingLabel, "HDG: --°");
        lv_label_set_text(dateLabel, "--/--/--");
    }
    
    // Update acceleration indicator
    static float lastSpeed = 0;
    if (imuData && systemData && systemData->mpuAvailable) {
        float acceleration = gpsData->speed - lastSpeed;
        if (abs(acceleration) > 0.5) {
            if (acceleration > 0) {
                lv_label_set_text(accelerationIndicator, "↑");
                lv_obj_set_style_text_color(accelerationIndicator, UI_COLOR_SUCCESS, 0);
            } else {
                lv_label_set_text(accelerationIndicator, "↓");
                lv_obj_set_style_text_color(accelerationIndicator, UI_COLOR_ACCENT, 0);
            }
        } else {
            lv_label_set_text(accelerationIndicator, "");
        }
        lastSpeed = gpsData->speed;
    }
}

void UIManager::updateMotionScreen() {
    if (!imuData) return;
    
    // Update motion indicator
    if (imuData->motionDetected) {
        lv_label_set_text(motionIndicator, "MOTION DETECTED");
        lv_obj_set_style_text_color(motionIndicator, UI_COLOR_SUCCESS, 0);
    } else {
        lv_label_set_text(motionIndicator, "No Motion");
        lv_obj_set_style_text_color(motionIndicator, UI_COLOR_TEXT_MUTED, 0);
    }
    
    // Update accelerometer data
    char accelStr[32];
    snprintf(accelStr, sizeof(accelStr), "X: %.2f", imuData->accelX);
    lv_label_set_text(accelXLabel, accelStr);
    
    snprintf(accelStr, sizeof(accelStr), "Y: %.2f", imuData->accelY);
    lv_label_set_text(accelYLabel, accelStr);
    
    snprintf(accelStr, sizeof(accelStr), "Z: %.2f", imuData->accelZ);
    lv_label_set_text(accelZLabel, accelStr);
    
    snprintf(accelStr, sizeof(accelStr), "Mag: %.2fg", imuData->magnitude);
    lv_label_set_text(magnitudeLabel, accelStr);
    
    // Color code magnitude based on intensity
    lv_color_t magColor = UI_COLOR_TEXT;
    if (imuData->magnitude > 2.5) magColor = UI_COLOR_DANGER;
    else if (imuData->magnitude > 1.2) magColor = UI_COLOR_WARNING;
    else if (imuData->magnitude > 0.5) magColor = UI_COLOR_SUCCESS;
    lv_obj_set_style_text_color(magnitudeLabel, magColor, 0);
    
    // Update gyroscope data
    char gyroStr[32];
    snprintf(gyroStr, sizeof(gyroStr), "X: %.1f", imuData->gyroX);
    lv_label_set_text(gyroXLabel, gyroStr);
    
    snprintf(gyroStr, sizeof(gyroStr), "Y: %.1f", imuData->gyroY);
    lv_label_set_text(gyroYLabel, gyroStr);
    
    snprintf(gyroStr, sizeof(gyroStr), "Z: %.1f", imuData->gyroZ);
    lv_label_set_text(gyroZLabel, gyroStr);
    
    // Update temperature
    char tempStr[32];
    snprintf(tempStr, sizeof(tempStr), "Temp: %.1f°C", imuData->temperature);
    lv_label_set_text(temperatureLabel, tempStr);
}

void UIManager::updateSystemScreen() {
    if (!systemData) return;
    
    // Update connectivity status
    char statusStr[64];
    
    // WiFi
    if (WiFi.status() == WL_CONNECTED) {
        snprintf(statusStr, sizeof(statusStr), "WiFi: Connected (%d dBm)", WiFi.RSSI());
        lv_obj_set_style_text_color(wifiInfoLabel, UI_COLOR_SUCCESS, 0);
    } else {
        snprintf(statusStr, sizeof(statusStr), "WiFi: Disconnected");
        lv_obj_set_style_text_color(wifiInfoLabel, UI_COLOR_DANGER, 0);
    }
    lv_label_set_text(wifiInfoLabel, statusStr);
    
    // BLE
    snprintf(statusStr, sizeof(statusStr), "BLE: Ready");
    lv_obj_set_style_text_color(bleInfoLabel, UI_COLOR_PRIMARY, 0);
    lv_label_set_text(bleInfoLabel, statusStr);
    
    // SD Card
    if (systemData->sdCardAvailable) {
        snprintf(statusStr, sizeof(statusStr), "SD: Ready");
        lv_obj_set_style_text_color(sdInfoLabel, UI_COLOR_SUCCESS, 0);
    } else {
        snprintf(statusStr, sizeof(statusStr), "SD: Not Available");
        lv_obj_set_style_text_color(sdInfoLabel, UI_COLOR_DANGER, 0);
    }
    lv_label_set_text(sdInfoLabel, statusStr);
    
    // Touch
    if (systemData->touchAvailable) {
        snprintf(statusStr, sizeof(statusStr), "Touch: Ready");
        lv_obj_set_style_text_color(touchInfoLabel, UI_COLOR_SUCCESS, 0);
    } else {
        snprintf(statusStr, sizeof(statusStr), "Touch: Not Available");
        lv_obj_set_style_text_color(touchInfoLabel, UI_COLOR_DANGER, 0);
    }
    lv_label_set_text(touchInfoLabel, statusStr);
    
    // IMU
    if (systemData->mpuAvailable) {
        snprintf(statusStr, sizeof(statusStr), "IMU: Ready");
        lv_obj_set_style_text_color(imuInfoLabel, UI_COLOR_SUCCESS, 0);
    } else {
        snprintf(statusStr, sizeof(statusStr), "IMU: Not Available");
        lv_obj_set_style_text_color(imuInfoLabel, UI_COLOR_DANGER, 0);
    }
    lv_label_set_text(imuInfoLabel, statusStr);
    
    // System info
    uint32_t freeHeap = ESP.getFreeHeap();
    snprintf(statusStr, sizeof(statusStr), "Free RAM: %lu bytes", freeHeap);
    lv_label_set_text(ramLabel, statusStr);
    
    // Color code memory
    lv_color_t memColor = UI_COLOR_SUCCESS;
    if (freeHeap < 50000) memColor = UI_COLOR_DANGER;
    else if (freeHeap < 100000) memColor = UI_COLOR_WARNING;
    lv_obj_set_style_text_color(ramLabel, memColor, 0);
    
    // Uptime
    unsigned long uptime = millis() / 1000;
    snprintf(statusStr, sizeof(statusStr), "Uptime: %luh %lum", uptime / 3600, (uptime % 3600) / 60);
    lv_label_set_text(uptimeLabel, statusStr);
    
    // Battery info
    if (batteryData) {
        snprintf(statusStr, sizeof(statusStr), "Battery: %d%% (%.2fV)", 
                 batteryData->percentage, batteryData->voltage);
        lv_label_set_text(batteryInfoLabel, statusStr);
        
        if (batteryData->isCharging) {
            snprintf(statusStr, sizeof(statusStr), "Charging: %.0fmA", batteryData->current);
            lv_obj_set_style_text_color(chargingInfoLabel, UI_COLOR_PRIMARY, 0);
        } else if (batteryData->usbConnected) {
            snprintf(statusStr, sizeof(statusStr), "USB Connected");
            lv_obj_set_style_text_color(chargingInfoLabel, UI_COLOR_WARNING, 0);
        } else {
            snprintf(statusStr, sizeof(statusStr), "Not Charging");
            lv_obj_set_style_text_color(chargingInfoLabel, UI_COLOR_TEXT_MUTED, 0);
        }
        lv_label_set_text(chargingInfoLabel, statusStr);
    }
    
    // Logging status
    if (systemData->loggingActive) {
        snprintf(statusStr, sizeof(statusStr), "Logging: Active");
        lv_obj_set_style_text_color(logStatusLabel, UI_COLOR_SUCCESS, 0);
    } else {
        snprintf(statusStr, sizeof(statusStr), "Logging: Stopped");
        lv_obj_set_style_text_color(logStatusLabel, UI_COLOR_TEXT_MUTED, 0);
    }
    lv_label_set_text(logStatusLabel, statusStr);
}

void UIManager::updatePerformanceScreen() {
    if (!perfStats) return;
    
    char perfStr[64];
    
    // Update throughput data
    snprintf(perfStr, sizeof(perfStr), "Total: %lu", perfStats->totalPackets);
    lv_label_set_text(totalPacketsLabel, perfStr);
    
    snprintf(perfStr, sizeof(perfStr), "Dropped: %lu", perfStats->droppedPackets);
    lv_label_set_text(droppedPacketsLabel, perfStr);
    
    // Calculate and display drop rate
    if (perfStats->totalPackets > 0) {
        float dropRate = (float)perfStats->droppedPackets / perfStats->totalPackets * 100.0f;
        snprintf(perfStr, sizeof(perfStr), "Drop Rate: %.2f%%", dropRate);
        
        lv_color_t dropColor = UI_COLOR_SUCCESS;
        if (dropRate > 5.0) dropColor = UI_COLOR_DANGER;
        else if (dropRate > 1.0) dropColor = UI_COLOR_WARNING;
        lv_obj_set_style_text_color(dropRateLabel, dropColor, 0);
    } else {
        snprintf(perfStr, sizeof(perfStr), "Drop Rate: 0.0%%");
        lv_obj_set_style_text_color(dropRateLabel, UI_COLOR_SUCCESS, 0);
    }
    lv_label_set_text(dropRateLabel, perfStr);
    
    // Update timing data
    snprintf(perfStr, sizeof(perfStr), "Min Δ: %lums", perfStats->minDelta);
    lv_label_set_text(minDeltaLabel, perfStr);
    
    snprintf(perfStr, sizeof(perfStr), "Max Δ: %lums", perfStats->maxDelta);
    lv_label_set_text(maxDeltaLabel, perfStr);
    
    snprintf(perfStr, sizeof(perfStr), "Avg Δ: %lums", perfStats->avgDelta);
    lv_label_set_text(avgDeltaLabel, perfStr);
    
    // Calculate and display data rate
    if (perfStats->avgDelta > 0) {
        float dataRate = 1000.0f / perfStats->avgDelta;
        snprintf(perfStr, sizeof(perfStr), "Rate: %.1f pps", dataRate);
        lv_label_set_text(dataRateLabel, perfStr);
        
        // Performance status
        lv_color_t perfColor = UI_COLOR_DANGER;
        const char* perfStatus = "POOR";
        if (dataRate >= 20) {
            perfColor = UI_COLOR_SUCCESS;
            perfStatus = "EXCELLENT";
        } else if (dataRate >= 10) {
            perfColor = UI_COLOR_WARNING;
            perfStatus = "GOOD";
        }
        
        snprintf(perfStr, sizeof(perfStr), "Status: %s", perfStatus);
        lv_obj_set_style_text_color(perfStatusLabel, perfColor, 0);
        lv_label_set_text(perfStatusLabel, perfStr);
    } else {
        snprintf(perfStr, sizeof(perfStr), "Rate: 0.0 pps");
        lv_label_set_text(dataRateLabel, perfStr);
        snprintf(perfStr, sizeof(perfStr), "Status: STARTING");
        lv_obj_set_style_text_color(perfStatusLabel, UI_COLOR_TEXT_MUTED, 0);
        lv_label_set_text(perfStatusLabel, perfStr);
    }
    
    // Update memory info
    uint32_t freeHeap = ESP.getFreeHeap();
    snprintf(perfStr, sizeof(perfStr), "Free RAM: %lu bytes", freeHeap);
    lv_label_set_text(memoryLabel, perfStr);
    
    lv_color_t memColor = getPerformanceColor(freeHeap, 100000, 200000);
    lv_obj_set_style_text_color(memoryLabel, memColor, 0);
}

// Screen management
void UIManager::showScreen(ScreenType screen) {
    hideAllScreens();
    currentScreen = screen;
    showCurrentScreen();
    
    const char* titles[] = {"SPEEDOMETER", "MOTION", "SYSTEM", "PERFORMANCE"};
    lv_label_set_text(titleLabel, titles[currentScreen]);
    
    requestUpdate();
}

void UIManager::nextScreen() {
    if (currentScreen < SCREEN_PERFORMANCE) {
        showScreen((ScreenType)(currentScreen + 1));
    }
}

void UIManager::previousScreen() {
    if (currentScreen > SCREEN_SPEEDOMETER) {
        showScreen((ScreenType)(currentScreen - 1));
    }
}

void UIManager::hideAllScreens() {
    lv_obj_add_flag(speedometerPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(motionPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(systemPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(performancePanel, LV_OBJ_FLAG_HIDDEN);
}

void UIManager::showCurrentScreen() {
    switch (currentScreen) {
        case SCREEN_SPEEDOMETER:
            lv_obj_clear_flag(speedometerPanel, LV_OBJ_FLAG_HIDDEN);
            break;
        case SCREEN_MOTION:
            lv_obj_clear_flag(motionPanel, LV_OBJ_FLAG_HIDDEN);
            break;
        case SCREEN_SYSTEM:
            lv_obj_clear_flag(systemPanel, LV_OBJ_FLAG_HIDDEN);
            break;
        case SCREEN_PERFORMANCE:
            lv_obj_clear_flag(performancePanel, LV_OBJ_FLAG_HIDDEN);
            break;
    }
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
    lastUpdate = 0;
    lastHeaderUpdate = 0;
    lastStatusUpdate = 0;
}

// Utility functions
lv_color_t UIManager::getSpeedColor(float speed) {
    if (speed > 100) return UI_COLOR_DANGER;
    if (speed > 60) return UI_COLOR_WARNING;
    if (speed > 30) return UI_COLOR_SUCCESS;
    return UI_COLOR_PRIMARY;
}

lv_color_t UIManager::getPerformanceColor(float value, float good, float excellent) {
    if (value >= excellent) return UI_COLOR_SUCCESS;
    if (value >= good) return UI_COLOR_WARNING;
    return UI_COLOR_DANGER;
}

lv_color_t UIManager::getSignalColor(int strength) {
    if (strength > -50) return UI_COLOR_SUCCESS;
    if (strength > -70) return UI_COLOR_WARNING;
    return UI_COLOR_DANGER;
}

void UIManager::setBatteryIcon(uint8_t percentage, bool charging, bool connected) {
    if (!connected) {
        lv_obj_set_style_text_color(batteryIcon, UI_COLOR_DANGER, 0);
        return;
    }
    
    lv_color_t color = UI_COLOR_SUCCESS;
    if (charging) {
        color = UI_COLOR_PRIMARY;
    } else if (percentage < 20) {
        color = UI_COLOR_DANGER;
    } else if (percentage < 50) {
        color = UI_COLOR_WARNING;
    }
    
    lv_obj_set_style_text_color(batteryIcon, color, 0);
}

void UIManager::setStatusIcon(lv_obj_t* icon, bool connected, const char* symbol) {
    lv_label_set_text(icon, symbol);
    if (connected) {
        lv_obj_set_style_text_color(icon, UI_COLOR_SUCCESS, 0);
    } else {
        lv_obj_set_style_text_color(icon, UI_COLOR_DANGER, 0);
    }
}

// Static event handlers
void UIManager::buttonEventHandler(lv_event_t* e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    lv_obj_t* target = lv_event_get_target(e);
    
    if (target == ui->prevButton) {
        ui->previousScreen();
    } else if (target == ui->nextButton) {
        ui->nextScreen();
    } else if (target == ui->logButton) {
        if (ui->loggingCallback) {
            ui->loggingCallback();
        }
        // Update button appearance
        if (ui->systemData && ui->systemData->loggingActive) {
            lv_obj_set_style_bg_color(ui->logButton, UI_COLOR_ACCENT, 0);
        } else {
            lv_obj_set_style_bg_color(ui->logButton, UI_COLOR_SUCCESS, 0);
        }
    } else if (target == ui->menuButton) {
        if (ui->menuCallback) {
            ui->menuCallback();
        }
    } else if (target == ui->systemButton) {
        ui->showScreen(SCREEN_SYSTEM);
    }
}

void UIManager::screenEventHandler(lv_event_t* e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    
    // Handle screen-specific touch events
    if (ui->currentScreen == SCREEN_PERFORMANCE) {
        // Reset performance stats on center touch
        if (ui->perfStats) {
            ui->perfStats->totalPackets = 0;
            ui->perfStats->droppedPackets = 0;
            ui->perfStats->minDelta = 9999;
            ui->perfStats->maxDelta = 0;
            ui->perfStats->avgDelta = 0;
            ui->requestUpdate();
        }
    }
}