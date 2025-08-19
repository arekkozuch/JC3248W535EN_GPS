#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include "data_structures.h"
#include "boardconfig.h"

// Forward declarations for Arduino_GFX objects
extern Arduino_DataBus *bus;
extern Arduino_GFX *gfx;

class UIManager {
public:
    UIManager();
    ~UIManager();
    
    // Initialization
    void init(SystemData* sysData, GPSData* gpsData, IMUData* imuData, 
              BatteryData* battData, PerformanceStats* perfData);
    
    // Hardware initialization for JC3248W535EN
    bool initializeDisplayHardware();
    
    // LVGL callbacks (static functions)
    static void lvgl_display_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
    static void lvgl_touch_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);
    
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
    
    // LVGL objects (minimal set for testing)
    lv_obj_t* mainScreen;
    lv_obj_t* progressBar;
    lv_obj_t* transferLabel;
    
    // State variables
    ScreenType currentScreen;
    bool updateRequested;
    unsigned long lastUpdate;
    unsigned long lastHeaderUpdate;
    unsigned long lastStatusUpdate;
    static const unsigned long UPDATE_INTERVAL = 100;
    static const unsigned long HEADER_UPDATE_INTERVAL = 1000;
    static const unsigned long STATUS_UPDATE_INTERVAL = 5000;
    
    // Callbacks
    void (*loggingCallback)();
    void (*menuCallback)();
    void (*systemCallback)();
    
    // Private methods
    void createWorkingTestScreen();
    
    // Utility functions
    lv_color_t getSpeedColor(float speed);
    lv_color_t getPerformanceColor(float value, float good, float excellent);
    lv_color_t getSignalColor(int strength);
    void setBatteryIcon(uint8_t percentage, bool charging, bool connected);
    void setStatusIcon(lv_obj_t* icon, bool connected, const char* symbol);
    
    // Static event handlers
    static void buttonEventHandler(lv_event_t* e);
    static void screenEventHandler(lv_event_t* e);
};

#endif // UI_MANAGER_H