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
    screen.drawEllipse(160, 120, 50, 30); // Draw an ellipse at the center of the screen
    screen.flush(); // Ensure the drawing is rendered on the screen
    delay(1000); // Wait for a second before the next iteration
    screen.clear(255, 255, 255); // Clear the screen again
    delay(500); // Wait before the next drawing
    
  // Your code here
}