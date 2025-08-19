#include <JC3248W535EN-Touch-LCD.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>

// Debug configuration
#define DEBUG_SERIAL 1
#define USE_DIRECT_GFX 0  // Set to 1 to bypass library and use direct GFX
#define ENABLE_HARDWARE_TEST 1  // Enable hardware diagnostic tests

// Hardware pin definitions from the library source
#define GFX_BL 1
#define TOUCH_ADDR 0x3B
#define TOUCH_SDA 4
#define TOUCH_SCL 8
#define TOUCH_I2C_CLOCK 400000
#define TOUCH_RST_PIN 12
#define TOUCH_INT_PIN 11
#define AXS_MAX_TOUCH_NUMBER 1

// Direct GFX setup (for fallback mode)
#if USE_DIRECT_GFX
  Arduino_DataBus *bus;
  Arduino_GFX *gfx;
  Arduino_Canvas *canvas;
#endif

JC3248W535EN screen;
uint16_t touchX, touchY;
bool screenInitialized = false;
bool touchInitialized = false;
unsigned long lastDebugTime = 0;
unsigned long lastTouchTime = 0;
int touchCount = 0;

// Debug print functions
void debugPrint(const String& message) {
  #if DEBUG_SERIAL
    Serial.print("[DEBUG] ");
    Serial.print(millis());
    Serial.print("ms: ");
    Serial.println(message);
  #endif
}

void debugPrintValue(const String& label, int value) {
  #if DEBUG_SERIAL
    Serial.print("[DEBUG] ");
    Serial.print(label);
    Serial.print(": ");
    Serial.println(value);
  #endif
}

void debugPrintHex(const String& label, uint32_t value) {
  #if DEBUG_SERIAL
    Serial.print("[DEBUG] ");
    Serial.print(label);
    Serial.print(": 0x");
    Serial.println(value, HEX);
  #endif
}

// Hardware diagnostic functions
#if ENABLE_HARDWARE_TEST
void testBacklight() {
  debugPrint("Testing backlight...");
  pinMode(GFX_BL, OUTPUT);
  
  // Test backlight on/off
  digitalWrite(GFX_BL, LOW);
  debugPrint("Backlight OFF");
  delay(500);
  
  digitalWrite(GFX_BL, HIGH);
  debugPrint("Backlight ON");
  delay(500);
}

void testI2C() {
  debugPrint("Testing I2C bus...");
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setClock(TOUCH_I2C_CLOCK);
  
  debugPrintValue("I2C SDA pin", TOUCH_SDA);
  debugPrintValue("I2C SCL pin", TOUCH_SCL);
  debugPrintValue("I2C clock", TOUCH_I2C_CLOCK);
  
  // Scan for I2C devices
  debugPrint("Scanning I2C bus...");
  int deviceCount = 0;
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
      debugPrintHex("I2C device found at address", address);
      deviceCount++;
    }
  }
  debugPrintValue("Total I2C devices found", deviceCount);
}

void testTouchController() {
  debugPrint("Testing touch controller...");
  
  // Reset touch controller
  pinMode(TOUCH_INT_PIN, INPUT_PULLUP);
  pinMode(TOUCH_RST_PIN, OUTPUT);
  
  debugPrintValue("Touch interrupt pin", TOUCH_INT_PIN);
  debugPrintValue("Touch reset pin", TOUCH_RST_PIN);
  
  digitalWrite(TOUCH_RST_PIN, LOW);
  delay(200);
  digitalWrite(TOUCH_RST_PIN, HIGH);
  delay(200);
  
  debugPrint("Touch controller reset completed");
  
  // Test touch communication
  Wire.beginTransmission(TOUCH_ADDR);
  byte error = Wire.endTransmission();
  
  if (error == 0) {
    debugPrint("Touch controller communication OK");
    touchInitialized = true;
  } else {
    debugPrint("Touch controller communication FAILED");
    debugPrintValue("I2C error code", error);
    touchInitialized = false;
  }
}

void testSystemInfo() {
  debugPrint("=== SYSTEM INFORMATION ===");
  debugPrintValue("Free heap", ESP.getFreeHeap());
  debugPrintValue("Chip revision", ESP.getChipRevision());
  debugPrint("Chip model: " + String(ESP.getChipModel()));
  debugPrintValue("CPU frequency (MHz)", ESP.getCpuFreqMHz());
  debugPrintValue("Flash size", ESP.getFlashChipSize());
  debugPrintValue("PSRAM size", ESP.getPsramSize());
  debugPrint("SDK version: " + String(ESP.getSdkVersion()));
}
#endif

// Initialize direct GFX mode
#if USE_DIRECT_GFX
bool initDirectGFX() {
  debugPrint("Initializing direct GFX mode...");
  
  try {
    // Use the exact same configuration as the library
    bus = new Arduino_ESP32QSPI(45, 47, 21, 48, 40, 39);
    gfx = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, 320, 480);
    canvas = new Arduino_Canvas(320, 480, gfx, 0, 0, 0);
    
    if (!canvas->begin()) {
      debugPrint("Direct GFX canvas initialization failed!");
      return false;
    }
    
    // Enable backlight
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
    
    canvas->fillScreen(0); // Clear screen
    debugPrint("Direct GFX initialized successfully!");
    return true;
  } catch (...) {
    debugPrint("Exception during direct GFX initialization!");
    return false;
  }
}

void drawDirectGFX(const String& text, uint16_t x, uint16_t y, uint16_t color = 0xFFFF) {
  canvas->setTextColor(color);
  canvas->setTextSize(2);
  canvas->setCursor(x, y);
  canvas->print(text);
  canvas->flush();
}

void clearDirectGFX(uint16_t color = 0x0000) {
  canvas->fillScreen(color);
  canvas->flush();
}

void drawCircleDirectGFX(uint16_t x, uint16_t y, uint16_t radius, uint16_t color) {
  // Transform coordinates to match screen orientation
  int16_t px = 320 - y;
  int16_t py = x;
  canvas->fillCircle(px, py, radius, color);
  canvas->flush();
}
#endif

bool initializeScreen() {
  debugPrint("Starting screen initialization...");
  
  #if USE_DIRECT_GFX
    return initDirectGFX();
  #else
    // Hardware tests first
    #if ENABLE_HARDWARE_TEST
      testSystemInfo();
      testBacklight();
      testI2C();
      testTouchController();
    #endif
    
    // Try multiple initialization attempts
    for (int attempt = 1; attempt <= 3; attempt++) {
      debugPrint("Screen init attempt " + String(attempt));
      
      delay(500 * attempt); // Progressive delay
      
      if (screen.begin()) {
        debugPrint("Screen.begin() succeeded!");
        delay(100);
        
        // Test basic functionality
        screen.clear(255, 255, 255);
        delay(50);
        screen.flush();
        
        debugPrint("Screen initialization successful!");
        return true;
      } else {
        debugPrint("Screen.begin() failed on attempt " + String(attempt));
      }
    }
    
    debugPrint("All screen initialization attempts failed!");
    return false;
  #endif
}

void displayText(const String& text, uint16_t x, uint16_t y) {
  #if USE_DIRECT_GFX
    drawDirectGFX(text, x, y);
  #else
    screen.setColor(0, 0, 0);
    screen.prt(text, x, y, 2);
    screen.flush();
  #endif
}

void clearScreen() {
  #if USE_DIRECT_GFX
    clearDirectGFX();
  #else
    screen.clear(255, 255, 255);
    screen.flush();
  #endif
}

void drawTouchPoint(uint16_t x, uint16_t y) {
  #if USE_DIRECT_GFX
    drawCircleDirectGFX(x, y, 5, 0xF800); // Red color
  #else
    screen.setColor(255, 0, 0);
    screen.drawFillCircle(x, y, 5);
    screen.flush();
  #endif
}

// Enhanced touch reading with debugging
bool readTouch(uint16_t &x, uint16_t &y) {
  #if USE_DIRECT_GFX
    // For direct GFX mode, implement basic touch reading
    if (!touchInitialized) return false;
    
    uint8_t data[AXS_MAX_TOUCH_NUMBER * 6 + 2] = {0};
    
    const uint8_t read_cmd[11] = {
        0xb5, 0xab, 0xa5, 0x5a, 0x00, 0x00,
        (uint8_t)((AXS_MAX_TOUCH_NUMBER * 6 + 2) >> 8),
        (uint8_t)((AXS_MAX_TOUCH_NUMBER * 6 + 2) & 0xff),
        0x00, 0x00, 0x00
    };
    
    Wire.beginTransmission(TOUCH_ADDR);
    Wire.write(read_cmd, 11);
    if (Wire.endTransmission() != 0) {
      debugPrint("Touch I2C write failed");
      return false;
    }
    
    if (Wire.requestFrom(TOUCH_ADDR, sizeof(data)) != sizeof(data)) {
      debugPrint("Touch I2C read failed");
      return false;
    }
    
    for (int i = 0; i < sizeof(data); i++) {
        data[i] = Wire.read();
    }
    
    if (data[1] > 0 && data[1] <= AXS_MAX_TOUCH_NUMBER) {
        uint16_t rawX = ((data[2] & 0x0F) << 8) | data[3];
        uint16_t rawY = ((data[4] & 0x0F) << 8) | data[5];
        
        if (rawX == 273 && rawY == 273) return false;
        if (rawX > 4000 || rawY > 4000) return false;
        
        y = map(rawX, 0, 320, 320, 0);
        x = rawY;
        
        debugPrint("Direct touch: raw(" + String(rawX) + "," + String(rawY) + ") -> mapped(" + String(x) + "," + String(y) + ")");
        return true;
    }
    
    return false;
  #else
    return screen.getTouchPoint(x, y);
  #endif
}

void setup() {
  Serial.begin(115200);
  delay(4000);
  
  debugPrint("=== JC32 Enhanced Debug Version ===");
  debugPrint("DEBUG_SERIAL: " + String(DEBUG_SERIAL));
  debugPrint("USE_DIRECT_GFX: " + String(USE_DIRECT_GFX));
  debugPrint("ENABLE_HARDWARE_TEST: " + String(ENABLE_HARDWARE_TEST));
  
  // Initialize screen
  screenInitialized = initializeScreen();
  
  if (!screenInitialized) {
    debugPrint("CRITICAL: Screen initialization failed completely!");
    #if !USE_DIRECT_GFX
      debugPrint("Consider setting USE_DIRECT_GFX to 1 and recompiling");
    #endif
    return;
  }
  
  // Initial display setup
  clearScreen();
  debugPrint("Screen cleared");
  
  displayText("Enhanced Debug", 80, 50);
  displayText("Touch Mode", 80, 100);
  
  if (touchInitialized) {
    displayText("Touch: Ready", 80, 150);
  } else {
    displayText("Touch: Failed", 80, 150);
  }
  
  displayText("Touch screen!", 80, 200);
  debugPrint("Initial display setup completed");
  
  debugPrintValue("Final free heap", ESP.getFreeHeap());
}

void loop() {
  unsigned long currentTime = millis();
  
  // Periodic debug info
  if (currentTime - lastDebugTime > 15000) { // Every 15 seconds
    debugPrintValue("Uptime (seconds)", currentTime / 1000);
    debugPrintValue("Free heap", ESP.getFreeHeap());
    debugPrintValue("Touch count", touchCount);
    debugPrintValue("Touch initialized", touchInitialized);
    debugPrintValue("Screen initialized", screenInitialized);
    lastDebugTime = currentTime;
  }
  
  if (!screenInitialized) {
    // Try to reinitialize if failed initially
    if (currentTime > 30000) { // After 30 seconds, try once more
      debugPrint("Attempting screen reinitialization...");
      screenInitialized = initializeScreen();
    }
    delay(1000);
    return;
  }
  
  // Handle touch input
  bool touchDetected = readTouch(touchX, touchY);
  
  if (touchDetected) {
    // Debounce touch
    if (currentTime - lastTouchTime > 150) {
      touchCount++;
      
      debugPrint("Touch #" + String(touchCount) + " at X:" + String(touchX) + " Y:" + String(touchY));
      
      // Clear and redraw
      clearScreen();
      
      // Draw touch point
      drawTouchPoint(touchX, touchY);
      
      // Display comprehensive information
      displayText("Touch #" + String(touchCount), 10, 30);
      displayText("X:" + String(touchX) + " Y:" + String(touchY), 10, 70);
      displayText("Heap:" + String(ESP.getFreeHeap()/1024) + "KB", 10, 110);
      displayText("Up:" + String(currentTime/1000) + "s", 10, 150);
      
      // Display hardware status
      if (touchInitialized) {
        displayText("Touch: OK", 10, 190);
      } else {
        displayText("Touch: ERR", 10, 190);
      }
      
      #if USE_DIRECT_GFX
        displayText("Mode: Direct", 10, 230);
      #else
        displayText("Mode: Library", 10, 230);
      #endif
      
      lastTouchTime = currentTime;
      
      // Memory leak detection
      static int minHeap = ESP.getFreeHeap();
      int currentHeap = ESP.getFreeHeap();
      if (currentHeap < minHeap) {
        minHeap = currentHeap;
        debugPrintValue("New minimum heap", minHeap);
        if (minHeap < 50000) { // Alert if heap gets low
          debugPrint("WARNING: Low heap memory!");
        }
      }
    }
  }
  
  delay(50); // Prevent overwhelming the system
}