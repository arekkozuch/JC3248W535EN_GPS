#include <JC3248W535EN-Touch-LCD.h>

JC3248W535EN screen;

void setup() {
  Serial.begin(115200);
  screen.begin();
  screen.clear(255, 255, 255);
  
  // Draw a blue filled rectangle
  screen.setColor(0, 0, 255);
  screen.drawFillRect(50, 50, 100, 80);
  
  // Draw a red filled circle
  screen.setColor(255, 0, 0);
  screen.drawFillCircle(200, 150, 40);
  
  // Draw text
  screen.setColor(0, 0, 0);
  screen.prt("Hello World!", 120, 250, 2);
}

void loop() {
}