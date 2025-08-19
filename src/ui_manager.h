#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <lvgl.h>
#include "data_structures.h"
#include "boardconfig.h"

class UIManager {
public:
    UIManager();
    ~UIManager();
    
    // Initialization
    void init(SystemData* sysData, GPSData* gpsData, IMUData* imuData, 
              BatteryData* battData, PerformanceStats* perfData);
    
    // Simplified LVGL callbacks (static functions)
    static void lvgl_display_flush_simple(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
    static void lvgl_touch_read_simple(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);
    
    // Main update function
    void update();
    void requestUpdate();
    
    // Screen management (simplified)
    void showScreen(ScreenType screen);
    void nextScreen();
    void previousScreen();
    ScreenType getCurrentScreen() const;
    
    // Callback setters
    void setLoggingCallback(void (*callback)());
    void setMenuCallback(void (*callback)());
    void setSystemCallback(void (*callback)());
    
    // File transfer support (placeholder)
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
    
    // LVGL objects (minimal set)
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
    
    // Private methods (simplified)
    void createSimpleTestScreen();
    
    // Utility functions (simplified)
    lv_color_t getSpeedColor(float speed);
    lv_color_t getPerformanceColor(float value, float good, float excellent);
    lv_color_t getSignalColor(int strength);
    void setBatteryIcon(uint8_t percentage, bool charging, bool connected);
    void setStatusIcon(lv_obj_t* icon, bool connected, const char* symbol);
    
    // Static event handlers (simplified)
    static void buttonEventHandler(lv_event_t* e);
    static void screenEventHandler(lv_event_t* e);
};

#endif // UI_MANAGER_H