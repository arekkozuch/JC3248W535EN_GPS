#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include "data_structures.h"
#include "boardconfig.h"

// Forward declarations for Arduino_GFX objects
extern Arduino_DataBus *bus;
extern Arduino_GFX *gfx;
extern Arduino_Canvas *canvas;

class UIManager {
public:
    UIManager();
    ~UIManager();
    
    // Initialization
    void init(SystemData* sysData, GPSData* gpsData, IMUData* imuData, 
              BatteryData* battData, PerformanceStats* perfData);
    
    // Hardware initialization for JC3248W535EN
    bool initializeHardware();
    bool initializeTouch();
    void initializeLVGL();
    
    // LVGL callbacks (static functions)
    static void lvgl_display_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
    static void lvgl_touch_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);
    
    // Touch reading function
    bool readTouchPoint(uint16_t &x, uint16_t &y);
    
    // Main update function
    void update();
    void requestUpdate();
    
    // Screen management
    void showScreen(ScreenType screen);
    void nextScreen();
    void previousScreen();
    ScreenType getCurrentScreen() const;
    
    // Callback setters
    void setLoggingCallback(void (*callback)());
    void setMenuCallback(void (*callback)());
    void setSystemCallback(void (*callback)());
    
    // File transfer support
    void setFileTransferData(FileTransferState* ft) { fileTransferPtr = ft; }
    void updateFileTransferUI();
    
    // Force refresh
    void forceRefresh();
    
private:
    // Data pointers
    SystemData* systemData;
    GPSData* gpsData;
    IMUData* imuData;
    BatteryData* batteryData;
    PerformanceStats* perfStats;
    FileTransferState* fileTransferPtr;
    
    // LVGL objects
    lv_obj_t* mainScreen;
    lv_obj_t* headerPanel;
    lv_obj_t* contentPanel;
    lv_obj_t* footerPanel;
    lv_obj_t* statusBar;
    
    // Header elements
    lv_obj_t* titleLabel;
    lv_obj_t* timeLabel;
    lv_obj_t* batteryIcon;
    lv_obj_t* batteryLabel;
    
    // Status indicators
    lv_obj_t* gpsStatus;
    lv_obj_t* wifiStatus;
    lv_obj_t* bleStatus;
    lv_obj_t* sdStatus;
    
    // Footer buttons
    lv_obj_t* prevButton;
    lv_obj_t* nextButton;
    lv_obj_t* logButton;
    lv_obj_t* menuButton;
    lv_obj_t* systemButton;
    
    // Content panels for different screens (adapted for portrait 320x480)
    lv_obj_t* speedometerPanel;
    lv_obj_t* motionPanel;
    lv_obj_t* systemPanel;
    lv_obj_t* performancePanel;
    
    // File transfer UI elements
    lv_obj_t* progressBar;
    lv_obj_t* transferLabel;
    
    // Screen-specific elements
    // Speedometer screen
    lv_obj_t* speedValue;
    lv_obj_t* speedUnit;
    lv_obj_t* speedMph;
    lv_obj_t* coordsLabel;
    lv_obj_t* altitudeLabel;
    lv_obj_t* headingLabel;
    lv_obj_t* dateLabel;
    lv_obj_t* accelerationIndicator;
    
    // Motion screen
    lv_obj_t* accelXLabel;
    lv_obj_t* accelYLabel;
    lv_obj_t* accelZLabel;
    lv_obj_t* gyroXLabel;
    lv_obj_t* gyroYLabel;
    lv_obj_t* gyroZLabel;
    lv_obj_t* magnitudeLabel;
    lv_obj_t* temperatureLabel;
    lv_obj_t* motionIndicator;
    
    // System screen
    lv_obj_t* connectivityPanel;
    lv_obj_t* systemInfoPanel;
    lv_obj_t* powerPanel;
    lv_obj_t* wifiInfoLabel;
    lv_obj_t* bleInfoLabel;
    lv_obj_t* sdInfoLabel;
    lv_obj_t* touchInfoLabel;
    lv_obj_t* imuInfoLabel;
    lv_obj_t* ramLabel;
    lv_obj_t* uptimeLabel;
    lv_obj_t* batteryInfoLabel;
    lv_obj_t* chargingInfoLabel;
    lv_obj_t* logStatusLabel;
    
    // Performance screen
    lv_obj_t* throughputPanel;
    lv_obj_t* timingPanel;
    lv_obj_t* resourcePanel;
    lv_obj_t* totalPacketsLabel;
    lv_obj_t* droppedPacketsLabel;
    lv_obj_t* dropRateLabel;
    lv_obj_t* minDeltaLabel;
    lv_obj_t* maxDeltaLabel;
    lv_obj_t* avgDeltaLabel;
    lv_obj_t* dataRateLabel;
    lv_obj_t* perfStatusLabel;
    lv_obj_t* memoryLabel;
    lv_obj_t* resetStatsLabel;
    
    // State variables
    ScreenType currentScreen;
    bool updateRequested;
    unsigned long lastUpdate;
    unsigned long lastHeaderUpdate;
    unsigned long lastStatusUpdate;
    static const unsigned long UPDATE_INTERVAL = 100; // 10 FPS
    static const unsigned long HEADER_UPDATE_INTERVAL = 1000; // 1 Hz
    static const unsigned long STATUS_UPDATE_INTERVAL = 5000; // 0.2 Hz
    
    // Callbacks
    void (*loggingCallback)();
    void (*menuCallback)();
    void (*systemCallback)();
    
    // Private methods for UI creation (adapted for 320x480 portrait)
    void createMainLayout();
    void createHeader();
    void createFooter();
    void createStatusBar();
    void createScreens();
    void createSpeedometerScreen();
    void createMotionScreen();
    void createSystemScreen();
    void createPerformanceScreen();
    
    // Update methods (same logic as original, just different positions)
    void updateHeader();
    void updateStatusBar();
    void updateCurrentScreen();
    void updateSpeedometerScreen();
    void updateMotionScreen();
    void updateSystemScreen();
    void updatePerformanceScreen();
    
    void showCurrentScreen();
    void hideAllScreens();
    
    // Utility functions (same as original)
    void setBatteryIcon(uint8_t percentage, bool charging, bool connected);
    void setStatusIcon(lv_obj_t* icon, bool connected, const char* symbol);
    lv_color_t getSignalColor(int strength);
    lv_color_t getSpeedColor(float speed);
    lv_color_t getPerformanceColor(float value, float good, float excellent);
    
    // Static event handlers
    static void buttonEventHandler(lv_event_t* e);
    static void screenEventHandler(lv_event_t* e);
};

#endif // UI_MANAGER_H