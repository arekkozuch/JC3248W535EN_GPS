#include <Arduino_GFX_Library.h>
#include <Wire.h>

// Debug configuration
#define DEBUG_SERIAL 1

// Hardware pin definitions from the library source
#define GFX_BL 1
#define TOUCH_ADDR 0x3B
#define TOUCH_SDA 4
#define TOUCH_SCL 8
#define TOUCH_I2C_CLOCK 400000
#define TOUCH_RST_PIN 12
#define TOUCH_INT_PIN 11
#define AXS_MAX_TOUCH_NUMBER 1

// Direct GFX objects
Arduino_DataBus *bus;
Arduino_GFX *gfx;
Arduino_Canvas *canvas;

uint16_t touchX, touchY;
bool screenInitialized = false;
bool touchInitialized = false;
unsigned long lastDebugTime = 0;
unsigned long lastTouchTime = 0;
int touchCount = 0;

// Debug functions
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

bool initializeTouch() {
  debugPrint("Initializing touch controller...");
  
  // Initialize I2C with correct pins
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setClock(TOUCH_I2C_CLOCK);
  
  debugPrintValue("I2C SDA pin", TOUCH_SDA);
  debugPrintValue("I2C SCL pin", TOUCH_SCL);
  debugPrintValue("I2C clock", TOUCH_I2C_CLOCK);
  
  // Configure touch pins
  pinMode(TOUCH_INT_PIN, INPUT_PULLUP);
  pinMode(TOUCH_RST_PIN, OUTPUT);
  
  debugPrintValue("Touch interrupt pin", TOUCH_INT_PIN);
  debugPrintValue("Touch reset pin", TOUCH_RST_PIN);
  
  // Reset sequence
  digitalWrite(TOUCH_RST_PIN, LOW);
  delay(200);
  digitalWrite(TOUCH_RST_PIN, HIGH);
  delay(200);
  
  debugPrint("Touch controller reset completed");
  
  // Test communication
  Wire.beginTransmission(TOUCH_ADDR);
  byte error = Wire.endTransmission();
  
  if (error == 0) {
    debugPrint("Touch controller communication OK");
    
    // Scan for I2C devices to verify
    debugPrint("I2C device scan:");
    int deviceCount = 0;
    for (byte address = 1; address < 127; address++) {
      Wire.beginTransmission(address);
      byte scanError = Wire.endTransmission();
      
      if (scanError == 0) {
        debugPrintHex("I2C device found at address", address);
        deviceCount++;
      }
    }
    debugPrintValue("Total I2C devices found", deviceCount);
    
    return true;
  } else {
    debugPrint("Touch controller communication FAILED");
    debugPrintValue("I2C error code", error);
    return false;
  }
}

bool initializeDirectGFX() {
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
    debugPrint("Backlight enabled");
    
    canvas->fillScreen(0x0000); // Clear to black
    canvas->flush();
    debugPrint("Direct GFX initialized successfully!");
    return true;
  } catch (...) {
    debugPrint("Exception during direct GFX initialization!");
    return false;
  }
}

bool readTouchPoint(uint16_t &x, uint16_t &y) {
  if (!touchInitialized) return false;
  
  uint8_t data[AXS_MAX_TOUCH_NUMBER * 6 + 2] = {0};
  
  // Define the read command array (from library source)
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
      
      // Filter out invalid readings (from library source)
      if (rawX == 273 && rawY == 273) return false;
      if (rawX > 4000 || rawY > 4000) return false;
      
      // Apply coordinate mapping (from library source)
      y = map(rawX, 0, 320, 320, 0);
      x = rawY;
      
      return true;
  }
  
  return false;
}

void drawText(const String& text, uint16_t x, uint16_t y, uint16_t color = 0xFFFF) {
  // Apply rotation for text (matching library behavior)
  uint8_t originalRotation = canvas->getRotation();
  canvas->setRotation(1);
  canvas->setTextColor(color);
  canvas->setTextSize(2);
  canvas->setCursor(x, y);
  canvas->print(text);
  canvas->setRotation(originalRotation);
  canvas->flush();
}

void clearScreen(uint16_t color = 0x0000) {
  canvas->fillScreen(color);
  canvas->flush();
}

void drawCircle(uint16_t x, uint16_t y, uint16_t radius, uint16_t color) {
  // Transform coordinates to match screen orientation (from library source)
  int16_t px = 320 - y;
  int16_t py = x;
  canvas->fillCircle(px, py, radius, color);
  canvas->flush();
}

void drawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
  // Transform coordinates to match screen orientation (from library source)
  int16_t px = 320 - (y + h);
  int16_t py = x;
  int16_t pw = h;
  int16_t ph = w;
  
  // Bounds checking
  if (px < 0) px = 0;
  if (py < 0) py = 0;
  if (px + pw > 320) pw = 320 - px;
  if (py + ph > 480) ph = 480 - py;
  
  canvas->fillRect(px, py, pw, ph, color);
  canvas->flush();
}

void setup() {
  Serial.begin(115200);
  delay(4000);
  
  debugPrint("=== Direct GFX with Touch Implementation ===");
  
  // System info
  debugPrintValue("Free heap at start", ESP.getFreeHeap());
  debugPrintValue("Chip revision", ESP.getChipRevision());
  debugPrint("Chip model: " + String(ESP.getChipModel()));
  
  // Initialize touch first
  touchInitialized = initializeTouch();
  
  // Initialize display
  screenInitialized = initializeDirectGFX();
  
  if (!screenInitialized) {
    debugPrint("CRITICAL: Screen initialization failed!");
    return;
  }
  
  if (!touchInitialized) {
    debugPrint("WARNING: Touch initialization failed!");
  }
  
  // Initial display setup
  clearScreen(0x0000); // Black background
  debugPrint("Screen cleared");
  
  // Draw initial interface
  drawText("Direct GFX Mode", 80, 50, 0xFFFF);  // White text
  drawText("Touch Test", 80, 100, 0xFFFF);
  
  if (touchInitialized) {
    drawText("Touch: Ready", 80, 150, 0x07E0);  // Green
  } else {
    drawText("Touch: Failed", 80, 150, 0xF800); // Red
  }
  
  drawText("Touch the screen!", 80, 200, 0xFFE0); // Yellow
  
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
    delay(1000);
    return;
  }
  
  // Handle touch input
  bool touchDetected = readTouchPoint(touchX, touchY);
  
  if (touchDetected) {
    // Debounce touch
    if (currentTime - lastTouchTime > 150) {
      touchCount++;
      
      debugPrint("Touch #" + String(touchCount) + " at X:" + String(touchX) + " Y:" + String(touchY));
      
      // Clear screen
      clearScreen(0x0000);
      
      // Draw touch point with bright red circle
      drawCircle(touchX, touchY, 8, 0xF800);
      
      // Draw smaller white circle in center
      drawCircle(touchX, touchY, 3, 0xFFFF);
      
      // Display comprehensive information with colors
      drawText("Touch #" + String(touchCount), 10, 30, 0xFFFF);   // White
      drawText("X:" + String(touchX) + " Y:" + String(touchY), 10, 70, 0x07FF); // Cyan
      drawText("Heap:" + String(ESP.getFreeHeap()/1024) + "KB", 10, 110, 0xFFE0); // Yellow
      drawText("Up:" + String(currentTime/1000) + "s", 10, 150, 0xF81F); // Magenta
      
      // Hardware status
      if (touchInitialized) {
        drawText("Touch: OK", 10, 190, 0x07E0);   // Green
      } else {
        drawText("Touch: ERR", 10, 190, 0xF800);  // Red
      }
      
      drawText("Mode: Direct", 10, 230, 0x001F);   // Blue
      
      // Draw a small rectangle as visual feedback
      drawRect(touchX - 15, touchY - 15, 30, 4, 0xFFFF);
      drawRect(touchX - 15, touchY + 11, 30, 4, 0xFFFF);
      drawRect(touchX - 15, touchY - 15, 4, 30, 0xFFFF);
      drawRect(touchX + 11, touchY - 15, 4, 30, 0xFFFF);
      
      lastTouchTime = currentTime;
      
      // Memory monitoring
      static int minHeap = ESP.getFreeHeap();
      int currentHeap = ESP.getFreeHeap();
      if (currentHeap < minHeap) {
        minHeap = currentHeap;
        debugPrintValue("New minimum heap", minHeap);
        if (minHeap < 50000) {
          debugPrint("WARNING: Low heap memory!");
        }
      }
    }
  }
  
  delay(50); // Prevent system overload
}