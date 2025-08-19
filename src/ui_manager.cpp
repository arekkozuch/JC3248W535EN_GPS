#include "ui_manager.h"
#include <WiFi.h>

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
    
    // Create the main UI layout
    createMainLayout();
    
    // Set initial screen
    showScreen(SCREEN_SPEEDOMETER);
    
    Serial.println("✅ UIManager initialized");
}

void UIManager::createMainLayout() {
    // Create main screen
    mainScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(mainScreen, UI_COLOR_BACKGROUND, 0);
    lv_obj_set_style_pad_all(mainScreen, 0, 0);
    lv_scr_load(mainScreen);
    
    // Create layout panels
    createHeader();
    createFooter();
    createStatusBar();
    createScreens();
    
    Serial.println("✅ Main UI layout created");
}

void UIManager::createHeader() {
    headerPanel = lv_obj_create(mainScreen);
    lv_obj_set_size(headerPanel, UI_SCREEN_WIDTH, UI_HEADER_HEIGHT);
    lv_obj_set_pos(headerPanel, 0, 0);
    lv_obj_set_style_bg_color(headerPanel, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(headerPanel, 1, 0);
    lv_obj_set_style_border_color(headerPanel, UI_COLOR_BORDER, 0);
    lv_obj_set_style_pad_all(headerPanel, 5, 0);
    lv_obj_set_style_radius(headerPanel, 0, 0);
    
    // Title label (left side)
    titleLabel = lv_label_create(headerPanel);
    lv_label_set_text(titleLabel, "GPS LOGGER");
    lv_obj_set_style_text_color(titleLabel, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(titleLabel, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(titleLabel, 5, 5);
    
    // Time label (center)
    timeLabel = lv_label_create(headerPanel);
    lv_label_set_text(timeLabel, "00:00:00 UTC");
    lv_obj_set_style_text_color(timeLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(timeLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(timeLabel, UI_SCREEN_WIDTH - 250 , 5);
    
    // Battery display (right side)
    batteryIcon = lv_label_create(headerPanel);
    lv_label_set_text(batteryIcon, ICON_BATTERY);
    lv_obj_set_style_text_color(batteryIcon, UI_COLOR_SUCCESS, 0);
    lv_obj_set_pos(batteryIcon, UI_SCREEN_WIDTH- 960, 5);
    
    batteryLabel = lv_label_create(headerPanel);
    lv_label_set_text(batteryLabel, "100%");
    lv_obj_set_style_text_color(batteryLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(batteryLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(batteryLabel, UI_SCREEN_WIDTH - 50, 5);
}

void UIManager::createFooter() {
    footerPanel = lv_obj_create(mainScreen);
    lv_obj_set_size(footerPanel, UI_SCREEN_WIDTH, UI_FOOTER_HEIGHT);
    lv_obj_set_pos(footerPanel, 0, UI_SCREEN_HEIGHT - UI_FOOTER_HEIGHT);
    lv_obj_set_style_bg_color(footerPanel, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(footerPanel, 1, 0);
    lv_obj_set_style_border_color(footerPanel, UI_COLOR_BORDER, 0);
    lv_obj_set_style_pad_all(footerPanel, 2, 0);
    lv_obj_set_style_radius(footerPanel, 0, 0);
    
    // Previous button
    prevButton = lv_btn_create(footerPanel);
    lv_obj_set_size(prevButton, 60, UI_BUTTON_HEIGHT);
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
    
    // Next button
    nextButton = lv_btn_create(footerPanel);
    lv_obj_set_size(nextButton, 60, UI_BUTTON_HEIGHT);
    lv_obj_set_pos(nextButton, UI_SCREEN_WIDTH - 65, 5);
    lv_obj_set_style_bg_color(nextButton, UI_COLOR_SURFACE_2, 0);
    lv_obj_set_style_bg_color(nextButton, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(nextButton, UI_COLOR_BORDER, 0);
    lv_obj_set_style_radius(nextButton, 8, 0);
    lv_obj_add_event_cb(nextButton, buttonEventHandler, LV_EVENT_CLICKED, this);
    
    lv_obj_t* nextLabel = lv_label_create(nextButton);
    lv_label_set_text(nextLabel, ICON_NEXT);
    lv_obj_set_style_text_color(nextLabel, UI_COLOR_TEXT, 0);
    lv_obj_center(nextLabel);
    
    // Log button
    logButton = lv_btn_create(footerPanel);
    lv_obj_set_size(logButton, 80, UI_BUTTON_HEIGHT);
    lv_obj_set_pos(logButton, 75, 5);
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
    lv_obj_set_size(menuButton, 80, UI_BUTTON_HEIGHT);
    lv_obj_set_pos(menuButton, 165, 5);
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
    lv_obj_set_size(systemButton, 80, UI_BUTTON_HEIGHT);
    lv_obj_set_pos(systemButton, 255, 5);
    lv_obj_set_style_bg_color(systemButton, UI_COLOR_WARNING, 0);
    lv_obj_set_style_bg_color(systemButton, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_radius(systemButton, 8, 0);
    lv_obj_add_event_cb(systemButton, buttonEventHandler, LV_EVENT_CLICKED, this);
    
    lv_obj_t* systemLabel = lv_label_create(systemButton);
    lv_label_set_text(systemLabel, ICON_SYSTEM);
    lv_obj_set_style_text_color(systemLabel, UI_COLOR_BACKGROUND, 0);
    lv_obj_center(systemLabel);
}
void UIManager::createStatusBar() {
    statusBar = lv_obj_create(headerPanel);
    lv_obj_set_size(statusBar, 200, 25);
    lv_obj_set_pos(statusBar, UI_SCREEN_WIDTH - 220, 0);
    lv_obj_set_style_bg_opa(statusBar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(statusBar, 0, 0);
    lv_obj_set_style_pad_all(statusBar, 2, 0);
    
    // GPS status
    gpsStatus = lv_label_create(statusBar);
    lv_label_set_text(gpsStatus, ICON_GPS);
    lv_obj_set_style_text_color(gpsStatus, UI_COLOR_DANGER, 0);
    lv_obj_set_pos(gpsStatus, 100, 0);
    
    // WiFi status
    wifiStatus = lv_label_create(statusBar);
    lv_label_set_text(wifiStatus, ICON_WIFI);
    lv_obj_set_style_text_color(wifiStatus, UI_COLOR_DANGER, 0);
    lv_obj_set_pos(wifiStatus, 118, 0);
    
    // BLE status
    bleStatus = lv_label_create(statusBar);
    lv_label_set_text(bleStatus, ICON_BLE);
    lv_obj_set_style_text_color(bleStatus, UI_COLOR_DANGER, 0);
    lv_obj_set_pos(bleStatus, 135, 0);
    
    // SD status
    sdStatus = lv_label_create(statusBar);
    lv_label_set_text(sdStatus, ICON_SD);
    lv_obj_set_style_text_color(sdStatus, UI_COLOR_DANGER, 0);
    lv_obj_set_pos(sdStatus, 150, 0);
}

void UIManager::createScreens() {
    createSpeedometerScreen();
    createMotionScreen();
    createSystemScreen();
    createPerformanceScreen();
}

void UIManager::createSpeedometerScreen() {
    speedometerPanel = lv_obj_create(mainScreen);
    lv_obj_set_size(speedometerPanel, UI_SCREEN_WIDTH, 
                    UI_SCREEN_HEIGHT - UI_HEADER_HEIGHT - UI_FOOTER_HEIGHT);
    lv_obj_set_pos(speedometerPanel, 0, UI_HEADER_HEIGHT);
    lv_obj_set_style_bg_opa(speedometerPanel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(speedometerPanel, 0, 0);
    lv_obj_set_style_pad_all(speedometerPanel, 10, 0);
    
    // Large speed display (center-left)
    speedValue = lv_label_create(speedometerPanel);
    lv_label_set_text(speedValue, "NO FIX");
    lv_obj_set_style_text_color(speedValue, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(speedValue, UI_FONT_EXTRA_LARGE, 0);
    lv_obj_set_pos(speedValue, 30, 20);
    
    // Speed unit
    speedUnit = lv_label_create(speedometerPanel);
    lv_label_set_text(speedUnit, "");
    lv_obj_set_style_text_color(speedUnit, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(speedUnit, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(speedUnit, 200, 60);
    
    // Speed in MPH (smaller)
    speedMph = lv_label_create(speedometerPanel);
    lv_label_set_text(speedMph, "");
    lv_obj_set_style_text_color(speedMph, UI_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(speedMph, UI_FONT_SMALL, 0);
    lv_obj_set_pos(speedMph, -30, -80);
    
    // Acceleration indicator
    accelerationIndicator = lv_label_create(speedometerPanel);
    lv_label_set_text(accelerationIndicator, "");
    lv_obj_set_style_text_color(accelerationIndicator, UI_COLOR_SUCCESS, 0);
    lv_obj_set_style_text_font(accelerationIndicator, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(accelerationIndicator, 200, 20);
    
    // Coordinates
    coordsLabel = lv_label_create(speedometerPanel);
    lv_label_set_text(coordsLabel, "LAT: -- LON: --");
    lv_obj_set_style_text_color(coordsLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(coordsLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(coordsLabel, 175, 90);
    
    // Altitude and heading
    altitudeLabel = lv_label_create(speedometerPanel);
    lv_label_set_text(altitudeLabel, "ALT: --m");
    lv_obj_set_style_text_color(altitudeLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(altitudeLabel, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(altitudeLabel, 280, 5);
    
    headingLabel = lv_label_create(speedometerPanel);
    lv_label_set_text(headingLabel, "HDG: --°");
    lv_obj_set_style_text_color(headingLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(headingLabel, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(headingLabel, 280, 35);
    
    // Date and time
    dateLabel = lv_label_create(speedometerPanel);
    lv_label_set_text(dateLabel, "--/--/-- UTC");
    lv_obj_set_style_text_color(dateLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(dateLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(dateLabel, 280, 60);
}

void UIManager::createMotionScreen() {
    motionPanel = lv_obj_create(mainScreen);
    lv_obj_set_size(motionPanel, UI_SCREEN_WIDTH, 
                   UI_SCREEN_HEIGHT - UI_HEADER_HEIGHT - UI_FOOTER_HEIGHT);
    lv_obj_set_pos(motionPanel, 0, UI_HEADER_HEIGHT);
    lv_obj_set_style_bg_opa(motionPanel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(motionPanel, 0, 0);
    lv_obj_set_style_pad_all(motionPanel, 10, 0);
    
    // Motion indicator
    motionIndicator = lv_label_create(motionPanel);
    lv_label_set_text(motionIndicator, "No Motion");
    lv_obj_set_style_text_color(motionIndicator, UI_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(motionIndicator, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(motionIndicator, 180, 10);
    
    // Accelerometer section (left column)
    lv_obj_t* accelTitle = lv_label_create(motionPanel);
    lv_label_set_text(accelTitle, "Accelerometer (g)");
    lv_obj_set_style_text_color(accelTitle, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(accelTitle, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(accelTitle, 20, 30);
    
    accelXLabel = lv_label_create(motionPanel);
    lv_label_set_text(accelXLabel, "X: 0.00");
    lv_obj_set_style_text_color(accelXLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(accelXLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(accelXLabel, 20, 55);
    
    accelYLabel = lv_label_create(motionPanel);
    lv_label_set_text(accelYLabel, "Y: 0.00");
    lv_obj_set_style_text_color(accelYLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(accelYLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(accelYLabel, 20, 75);
    
    accelZLabel = lv_label_create(motionPanel);
    lv_label_set_text(accelZLabel, "Z: 0.00");
    lv_obj_set_style_text_color(accelZLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(accelZLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(accelZLabel, 20, 95);
    
    magnitudeLabel = lv_label_create(motionPanel);
    lv_label_set_text(magnitudeLabel, "Mag: 0.00g");
    lv_obj_set_style_text_color(magnitudeLabel, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(magnitudeLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(magnitudeLabel, 20, 115);
    
    // Gyroscope section (right column)
    lv_obj_t* gyroTitle = lv_label_create(motionPanel);
    lv_label_set_text(gyroTitle, "Gyroscope (°/s)");
    lv_obj_set_style_text_color(gyroTitle, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(gyroTitle, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(gyroTitle, 250, 30);
    
    gyroXLabel = lv_label_create(motionPanel);
    lv_label_set_text(gyroXLabel, "X: 0.0");
    lv_obj_set_style_text_color(gyroXLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(gyroXLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(gyroXLabel, 250, 55);
    
    gyroYLabel = lv_label_create(motionPanel);
    lv_label_set_text(gyroYLabel, "Y: 0.0");
    lv_obj_set_style_text_color(gyroYLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(gyroYLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(gyroYLabel, 250, 75);
    
    gyroZLabel = lv_label_create(motionPanel);
    lv_label_set_text(gyroZLabel, "Z: 0.0");
    lv_obj_set_style_text_color(gyroZLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(gyroZLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(gyroZLabel, 250, 95);
    
    temperatureLabel = lv_label_create(motionPanel);
    lv_label_set_text(temperatureLabel, "Temp: 0.0°C");
    lv_obj_set_style_text_color(temperatureLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(temperatureLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(temperatureLabel, 250, 115);
}
void UIManager::createSystemScreen() {
    systemPanel = lv_obj_create(mainScreen);
    lv_obj_set_size(systemPanel, UI_SCREEN_WIDTH, 
                   UI_SCREEN_HEIGHT - UI_HEADER_HEIGHT - UI_FOOTER_HEIGHT);
    lv_obj_set_pos(systemPanel, 0, UI_HEADER_HEIGHT);
    lv_obj_set_style_bg_opa(systemPanel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(systemPanel, 0, 0);
    lv_obj_set_style_pad_all(systemPanel, 10, 0);
    
    // Connectivity section (left column)
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
    
    // System info section (right column)
    lv_obj_t* sysTitle = lv_label_create(systemPanel);
    lv_label_set_text(sysTitle, "System Info");
    lv_obj_set_style_text_color(sysTitle, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(sysTitle, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(sysTitle, 250, 10);
    
    ramLabel = lv_label_create(systemPanel);
    lv_label_set_text(ramLabel, "Free RAM: 0 bytes");
    lv_obj_set_style_text_color(ramLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(ramLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(ramLabel, 250, 35);
    
    uptimeLabel = lv_label_create(systemPanel);
    lv_label_set_text(uptimeLabel, "Uptime: 0h 0m");
    lv_obj_set_style_text_color(uptimeLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(uptimeLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(uptimeLabel, 250, 55);
    
    batteryInfoLabel = lv_label_create(systemPanel);
    lv_label_set_text(batteryInfoLabel, "Battery: 0% (0.0V)");
    lv_obj_set_style_text_color(batteryInfoLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(batteryInfoLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(batteryInfoLabel, 250, 75);
    
    chargingInfoLabel = lv_label_create(systemPanel);
    lv_label_set_text(chargingInfoLabel, "Not Charging");
    lv_obj_set_style_text_color(chargingInfoLabel, UI_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(chargingInfoLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(chargingInfoLabel, 250, 95);
    
    logStatusLabel = lv_label_create(systemPanel);
    lv_label_set_text(logStatusLabel, "Logging: Stopped");
    lv_obj_set_style_text_color(logStatusLabel, UI_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(logStatusLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(logStatusLabel, 250, 115);
}

void UIManager::createPerformanceScreen() {
    performancePanel = lv_obj_create(mainScreen);
    lv_obj_set_size(performancePanel, UI_SCREEN_WIDTH, 
                    UI_SCREEN_HEIGHT - UI_HEADER_HEIGHT - UI_FOOTER_HEIGHT);
    lv_obj_set_pos(performancePanel, 0, UI_HEADER_HEIGHT);
    lv_obj_set_style_bg_opa(performancePanel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(performancePanel, 0, 0);
    lv_obj_set_style_pad_all(performancePanel, 10, 0);
    
    // Throughput section (left column)
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
    
    // Timing section (right column)
    lv_obj_t* timingTitle = lv_label_create(performancePanel);
    lv_label_set_text(timingTitle, "Timing Analysis");
    lv_obj_set_style_text_color(timingTitle, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_text_font(timingTitle, UI_FONT_MEDIUM, 0);
    lv_obj_set_pos(timingTitle, 250, 10);
    
    minDeltaLabel = lv_label_create(performancePanel);
    lv_label_set_text(minDeltaLabel, "Min Δ: 0ms");
    lv_obj_set_style_text_color(minDeltaLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(minDeltaLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(minDeltaLabel, 250, 35);
    
    maxDeltaLabel = lv_label_create(performancePanel);
    lv_label_set_text(maxDeltaLabel, "Max Δ: 0ms");
    lv_obj_set_style_text_color(maxDeltaLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(maxDeltaLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(maxDeltaLabel, 250, 55);
    
    avgDeltaLabel = lv_label_create(performancePanel);
    lv_label_set_text(avgDeltaLabel, "Avg Δ: 0ms");
    lv_obj_set_style_text_color(avgDeltaLabel, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(avgDeltaLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(avgDeltaLabel, 250, 75);
    
    dataRateLabel = lv_label_create(performancePanel);
    lv_label_set_text(dataRateLabel, "Rate: 0.0 pps");
    lv_obj_set_style_text_color(dataRateLabel, UI_COLOR_SUCCESS, 0);
    lv_obj_set_style_text_font(dataRateLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(dataRateLabel, 250, 95);
    
    resetStatsLabel = lv_label_create(performancePanel);
    lv_label_set_text(resetStatsLabel, "Touch center to reset stats");
    lv_obj_set_style_text_color(resetStatsLabel, UI_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(resetStatsLabel, UI_FONT_SMALL, 0);
    lv_obj_set_pos(resetStatsLabel, 120, 115);
}

// File Transfer UI Function
void UIManager::updateFileTransferUI() {
    if (!fileTransferPtr) return;
    
    if (fileTransferPtr->active) {
        if (!progressBar) {
            progressBar = lv_bar_create(mainScreen);
            lv_obj_set_size(progressBar, 300, 15);
            lv_obj_align(progressBar, LV_ALIGN_BOTTOM_MID, 0, -25);
            lv_obj_set_style_bg_color(progressBar, lv_color_hex(0x333333), LV_PART_MAIN);
            lv_obj_set_style_bg_color(progressBar, lv_color_hex(0x00AA00), LV_PART_INDICATOR);
            
            transferLabel = lv_label_create(mainScreen);
            lv_obj_align(transferLabel, LV_ALIGN_BOTTOM_MID, 0, -5);
            lv_obj_set_style_text_color(transferLabel, lv_color_white(), 0);
        }
        
        lv_bar_set_value(progressBar, (int32_t)fileTransferPtr->progressPercent, LV_ANIM_OFF);
        
        String text = "Transfer: " + fileTransferPtr->filename + " (" + 
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
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d UTC", 
                 gpsData->hour, gpsData->minute, gpsData->second);
        lv_label_set_text(timeLabel, timeStr);
    } else {
        lv_label_set_text(timeLabel, "--:--:-- UTC");
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
                lv_label_set_text(accelerationIndicator, " ");
                lv_obj_set_style_text_color(accelerationIndicator, UI_COLOR_SUCCESS, 0);
            } else {
                lv_label_set_text(accelerationIndicator, " ");
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
    
    const char* titles[] = {" ", "MOTION", "SYSTEM", "PERFORMANCE"};
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