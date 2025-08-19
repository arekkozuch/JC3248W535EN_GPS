#include <JC3248W535EN-Touch-LCD.h>

JC3248W535EN screen;

void setup() {
  Serial.begin(115200);
    delay(4000);
  // Initialize the screen
  if (!screen.begin()) {
    Serial.println("Screen initialization failed!");
    return;
  }
  
  // Clear the screen with white background
  screen.clear(255, 255, 255);
}

void loop() {
    Serial.println("Hello, JC3248W535EN!");
  // Your code here
}