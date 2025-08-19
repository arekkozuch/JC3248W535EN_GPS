#include <JC3248W535EN-Touch-LCD.h>
#include <Arduino_GFX_Library.h>

// Debug configuration
#define DEBUG_SERIAL 1
#define USE_DIRECT_GFX 0  // Set to 1 to bypass library and use direct GFX

// Direct GFX setup (for fallback mode)
#if USE_DIRECT_GFX
  // Pin definitions for JC32 board - adjust if needed
  #define TFT_CS     10
  #define TFT_RST    14  
  #define TFT_DC     13
  #define TFT_MOSI   11
  #define TFT_SCLK   12
  #define TFT_BL     45
  
  Arduino_DataBus *bus;
  Arduino_GFX *gfx;
#endif

JC3248W535EN screen;
uint16_t touchX, touchY;
bool screenInitialized = false;
unsigned long lastDebugTime = 0;
unsigned long lastTouchTime = 0;
int touchCount = 0;

// Debug print function
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

// Initialize direct GFX mode
#if USE_DIRECT_GFX
bool initDirectGFX() {
  debugPrint("Initializing direct GFX mode...");
  
  try {
    bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, GFX_NOT_DEFINED, VSPI);
    
    // Try different display controllers
    gfx = new Arduino_AXS15231B(bus, TFT_RST, 0 /* rotation */, false /* IPS */);
    
    if (!gfx->begin()) {
      debugPrint("Arduino_AXS15231B failed, trying ST7796...");
      delete gfx;
      gfx = new Arduino_ST7796(bus, TFT_RST, 0);
      if (!gfx->begin()) {
        debugPrint("ST7796 failed, trying ILI9488...");
        delete gfx;
        gfx = new Arduino_ILI9488(bus, TFT_RST, 0);
        if (!gfx->begin()) {
          debugPrint("All display controllers failed!");
          return false;
        }
      }
    }
    
    // Enable backlight
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    
    debugPrint("Direct GFX initialized successfully!");
    return true;
  } catch (...) {
    debugPrint("Exception during direct GFX initialization!");
    return false;
  }
}

void drawDirectGFX(const String& text, uint16_t x, uint16_t y, uint16_t color = 0x0000) {
  gfx->setTextColor(color);
  gfx->setTextSize(2);
  gfx->setCursor(x, y);
  gfx->print(text);
}

void clearDirectGFX(uint16_t color = 0xFFFF) {
  gfx->fillScreen(color);
}

void drawCircleDirectGFX(uint16_t x, uint16_t y, uint16_t radius, uint16_t color) {
  gfx->fillCircle(x, y, radius, color);
}
#endif

bool initializeScreen() {
  debugPrint("Starting screen initialization...");
  
  #if USE_DIRECT_GFX
    return initDirectGFX();
  #else
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
  #endif
}

void clearScreen() {
  #if USE_DIRECT_GFX
    clearDirectGFX();
  #else
    screen.clear(255, 255, 255);
  #endif
}

void drawTouchPoint(uint16_t x, uint16_t y) {
  #if USE_DIRECT_GFX
    drawCircleDirectGFX(x, y, 5, 0xF800); // Red color
  #else
    screen.setColor(255, 0, 0);
    screen.drawFillCircle(x, y, 5);
  #endif
}

void setup() {
  Serial.begin(115200);
  delay(4000);
  
  debugPrint("=== JC32 Touch Screen Debug Version ===");
  debugPrint("DEBUG_SERIAL: " + String(DEBUG_SERIAL));
  debugPrint("USE_DIRECT_GFX: " + String(USE_DIRECT_GFX));
  
  // Check available heap memory
  debugPrintValue("Free heap at start", ESP.getFreeHeap());
  debugPrintValue("Chip revision", ESP.getChipRevision());
  debugPrint("Chip model: " + String(ESP.getChipModel()));
  
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
  debugPrint("Screen cleared with white background");
  
  displayText("Touch Debug Mode", 80, 100);
  displayText("Touch the screen!", 80, 200);
  debugPrint("Initial text displayed");
  
  debugPrintValue("Free heap after init", ESP.getFreeHeap());
}

void loop() {
  unsigned long currentTime = millis();
  
  // Periodic debug info
  if (currentTime - lastDebugTime > 10000) { // Every 10 seconds
    debugPrintValue("Uptime (seconds)", currentTime / 1000);
    debugPrintValue("Free heap", ESP.getFreeHeap());
    debugPrintValue("Touch count", touchCount);
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
  #if USE_DIRECT_GFX
    // For direct GFX mode, you'd need to implement touch reading
    // This depends on your touch controller (likely FT6236 or similar)
    debugPrint("Touch reading not implemented in direct GFX mode yet");
    delay(5000);
  #else
    bool touchDetected = screen.getTouchPoint(touchX, touchY);
    
    if (touchDetected) {
      // Debounce touch
      if (currentTime - lastTouchTime > 100) {
        touchCount++;
        
        debugPrint("Touch #" + String(touchCount) + " at X:" + String(touchX) + " Y:" + String(touchY));
        
        // Clear and redraw
        clearScreen();
        
        // Draw touch point
        drawTouchPoint(touchX, touchY);
        
        // Display coordinates
        String coords = "Touch #" + String(touchCount);
        displayText(coords, 10, 50);
        
        coords = "X: " + String(touchX) + " Y: " + String(touchY);
        displayText(coords, 10, 100);
        
        // Display some debug info
        displayText("Heap: " + String(ESP.getFreeHeap()), 10, 150);
        displayText("Uptime: " + String(currentTime/1000) + "s", 10, 200);
        
        lastTouchTime = currentTime;
        
        // Check for memory leaks
        static int minHeap = ESP.getFreeHeap();
        int currentHeap = ESP.getFreeHeap();
        if (currentHeap < minHeap) {
          minHeap = currentHeap;
          debugPrintValue("New minimum heap", minHeap);
        }
      }
    }
  #endif
  
  delay(50); // Small delay to prevent overwhelming the system
}