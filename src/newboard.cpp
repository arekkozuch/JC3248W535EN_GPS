#include "LVGLScreenManager.h"
#include "LVGLTouchManager.h"

// Create managers
LVGLScreenManager screen(true);  // Enable debug
LVGLTouchManager touch(true);    // Enable debug

// UI Objects
lv_obj_t* main_screen;
lv_obj_t* status_label;
lv_obj_t* touch_counter_label;
lv_obj_t* coords_label;
lv_obj_t* memory_label;
lv_obj_t* progress_bar;
lv_obj_t* slider;
lv_obj_t* button1;
lv_obj_t* button2;

// State variables
int touch_count = 0;
unsigned long last_update = 0;

// Event handlers
void button_event_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* btn = lv_event_get_target(e);
    
    if (code == LV_EVENT_CLICKED) {
        if (btn == button1) {
            touch_count = 0;
            lv_label_set_text_fmt(touch_counter_label, "Touch Count: %d", touch_count);
            lv_bar_set_value(progress_bar, 0, LV_ANIM_ON);
            Serial.println("Reset button clicked!");
        } else if (btn == button2) {
            // Toggle screen brightness or some other function
            static bool bright = true;
            screen.enableBacklight(bright);
            bright = !bright;
            lv_label_set_text(status_label, bright ? "Status: Bright" : "Status: Dim");
            Serial.println("Brightness button clicked!");
        }
    }
}

void slider_event_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t* slider = lv_event_get_target(e);
        int32_t value = lv_slider_get_value(slider);
        lv_bar_set_value(progress_bar, value, LV_ANIM_ON);
        Serial.print("Slider value changed: ");
        Serial.println(value);
    }
}

void create_ui() {
    // Create main screen
    main_screen = lv_obj_create(NULL);
    lv_scr_load(main_screen);
    
    // Set a gradient background
    static lv_style_t style_bg;
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, lv_color_hex(0x1a1a2e));
    lv_style_set_bg_grad_color(&style_bg, lv_color_hex(0x16213e));
    lv_style_set_bg_grad_dir(&style_bg, LV_GRAD_DIR_VER);
    lv_obj_add_style(main_screen, &style_bg, 0);
    
    // Title
    lv_obj_t* title = screen.createLabel(main_screen, "LVGL Touch Demo", 20, 20);
    static lv_style_t style_title;
    lv_style_init(&style_title);
    lv_style_set_text_color(&style_title, lv_color_white());
    lv_style_set_text_font(&style_title, &lv_font_montserrat_14);  // Use available font
    lv_obj_add_style(title, &style_title, 0);
    
    // Status label
    status_label = screen.createLabel(main_screen, "Status: Ready", 20, 60);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x00ff88), 0);
    
    // Touch counter
    touch_counter_label = screen.createLabel(main_screen, "Touch Count: 0", 20, 90);
    lv_obj_set_style_text_color(touch_counter_label, lv_color_hex(0x88ccff), 0);
    
    // Coordinates label
    coords_label = screen.createLabel(main_screen, "X: 0, Y: 0", 20, 120);
    lv_obj_set_style_text_color(coords_label, lv_color_hex(0xffaa00), 0);
    
    // Memory info
    memory_label = screen.createLabel(main_screen, "Memory: 0 KB", 20, 150);
    lv_obj_set_style_text_color(memory_label, lv_color_hex(0xff6666), 0);
    
    // Progress bar
    progress_bar = screen.createProgressBar(main_screen, 20, 190, 200, 20);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x00ff88), LV_PART_INDICATOR);
    lv_bar_set_range(progress_bar, 0, 100);
    
    // Slider
    slider = screen.createSlider(main_screen, 20, 230, 200, 20);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 50, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x0088ff), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xffffff), LV_PART_KNOB);
    lv_obj_add_event_cb(slider, slider_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Buttons
    button1 = screen.createButton(main_screen, "Reset", 280, 190, 80, 40);
    lv_obj_set_style_bg_color(button1, lv_color_hex(0xff4444), LV_PART_MAIN);
    lv_obj_set_style_bg_color(button1, lv_color_hex(0xff6666), LV_STATE_PRESSED);
    lv_obj_add_event_cb(button1, button_event_handler, LV_EVENT_CLICKED, NULL);
    
    button2 = screen.createButton(main_screen, "Toggle", 280, 240, 80, 40);
    lv_obj_set_style_bg_color(button2, lv_color_hex(0x4444ff), LV_PART_MAIN);
    lv_obj_set_style_bg_color(button2, lv_color_hex(0x6666ff), LV_STATE_PRESSED);
    lv_obj_add_event_cb(button2, button_event_handler, LV_EVENT_CLICKED, NULL);
    
    // Add some visual flair - animated arc
    lv_obj_t* arc = lv_arc_create(main_screen);
    lv_obj_set_size(arc, 100, 100);
    lv_obj_set_pos(arc, 370, 100);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_angles(arc, 0, 0);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x00ff88), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    
    // Arc label
    lv_obj_t* arc_label = screen.createLabel(main_screen, "Touch\nActivity", 395, 140);
    lv_obj_set_style_text_color(arc_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(arc_label, LV_TEXT_ALIGN_CENTER, 0);
    
    Serial.println("UI created successfully!");
}

void update_ui() {
    unsigned long current_time = millis();
    
    // Update every 100ms
    if (current_time - last_update > 100) {
        // Update memory info
        uint32_t free_heap = ESP.getFreeHeap();
        lv_label_set_text_fmt(memory_label, "Memory: %lu KB", free_heap / 1024);
        
        // Update arc animation based on activity
        static int arc_value = 0;
        arc_value = (arc_value + 2) % 360;
        
        // Find the arc object (it should be one of the children)
        lv_obj_t* arc = NULL;
        uint32_t child_count = lv_obj_get_child_cnt(main_screen);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t* child = lv_obj_get_child(main_screen, i);
            if (lv_obj_check_type(child, &lv_arc_class)) {
                arc = child;
                break;
            }
        }
        
        if (arc) {
            lv_arc_set_angles(arc, 0, arc_value);
        }
        
        last_update = current_time;
    }
}

// Touch event handler (called when LVGL detects touch)
void screen_event_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
        lv_indev_t* indev = lv_indev_get_act();
        if (indev) {
            lv_point_t point;
            lv_indev_get_point(indev, &point);
            
            touch_count++;
            lv_label_set_text_fmt(touch_counter_label, "Touch Count: %d", touch_count);
            lv_label_set_text_fmt(coords_label, "X: %d, Y: %d", point.x, point.y);
            
            // Update progress bar based on touch count
            int progress = (touch_count % 100);
            lv_bar_set_value(progress_bar, progress, LV_ANIM_ON);
            
            Serial.printf("Touch #%d at (%d, %d)\n", touch_count, point.x, point.y);
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("=== LVGL Modern UI Demo ===");
    Serial.print("Free heap at start: ");
    Serial.println(ESP.getFreeHeap());
    
    // Initialize touch first
    if (!touch.begin()) {
        Serial.println("ERROR: Touch initialization failed!");
    }
    
    // Initialize screen
    if (!screen.begin()) {
        Serial.println("ERROR: Screen initialization failed!");
        return;
    }
    
    Serial.println("Hardware initialized successfully!");
    
    // Create the user interface
    create_ui();
    
    // Add screen event handler for touch detection
    lv_obj_add_event_cb(main_screen, screen_event_handler, LV_EVENT_ALL, NULL);
    
    Serial.println("Setup completed!");
    Serial.print("Free heap after setup: ");
    Serial.println(ESP.getFreeHeap());
}

void loop() {
    // Update LVGL
    screen.update();
    
    // Update UI elements
    update_ui();
    
    // Small delay to prevent overwhelming the system
    delay(5);
}