#include <JC3248W535EN-Touch-LCD.h>

JC3248W535EN screen;
uint16_t touchX, touchY;

void setup() {
  Serial.begin(115200);
  screen.begin();
  screen.clear(255, 255, 255);
  screen.setColor(0, 0, 0);
  screen.prt("Touch the screen!", 80, 200, 2);
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
    
    delay(100);  // Small delay to prevent too many readings
  }
}