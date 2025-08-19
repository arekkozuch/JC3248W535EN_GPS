#include <JC3248W535EN-Touch-LCD.h>

JC3248W535EN screen;
uint16_t touchX, touchY;

void setup() {
  Serial.begin(115200);
    delay(4000);
  // Initialize the screen
  screen.begin();
  Serial.println("Screen initialized successfully!");
  screen.clear(255, 255, 255);
  Serial.println("Screen cleared with white background!");
  screen.setColor(0, 0, 0);
  Serial.println("Screen color set to black!");
  screen.prt("Touch the screen!", 80, 200, 2);
    Serial.println("Text printed on the screen!");
  // Clear the screen with white background
  screen.clear(255, 255, 255);
    Serial.println("Screen cleared with white background again!");
}

void loop() {
  if (screen.getTouchPoint(touchX, touchY)) {
    // Draw a small circle where touch is detected
    screen.setColor(255, 0, 0);
    screen.drawFillCircle(touchX, touchY, 5);
    
    // Display coordinates
    screen.setColor(0, 0, 0);
    String coords = "X: " + String(touchX) + " Y: " + String(touchY);
    screen.clear(255, 255, 255);
    screen.prt(coords, 100, 240, 2);
    Serial.print("Touch detected at X: ");
    Serial.print(touchX);
    Serial.print(" Y: ");
    Serial.println(touchY);
        
    delay(100);  // Small delay to prevent too many readings
  }
}