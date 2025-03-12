#include <Wire.h>
#include <M5Unified.h>

// Define shades (from light to dark brown)
uint16_t shades[3] = {
    M5.Lcd.color565(235, 175, 105),  // Light brown
    M5.Lcd.color565(190, 120, 65),   // Medium brown
    M5.Lcd.color565(145, 75, 30)     // Darker brown
};

int shadeIndex = 0;  // Current shade (0-2)

struct rect_t {
  int x, y, w, h;
  
  bool contains(int tx, int ty) {
    return tx >= x && tx <= (x + w) && ty >= y && ty <= (y + h);
  }
};

rect_t shadeBoxes[3]; // Array to store boxes for each shade
const int startX = 50;  // Starting X position for the first box (slightly moved left)
const int startY = 100;  // Y position for the row of boxes (slightly moved up)
const int boxSize = 30;   // Size of the boxes
const int spacing = 50;   // Space between each box

void displayShade() {
  M5.Lcd.fillScreen(shades[shadeIndex]);  // Fill screen with selected shade
  M5.Lcd.setCursor(75, 60);  // Adjusted position for text (slightly moved up)
  M5.Lcd.setTextColor(TFT_WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.print("Shade: ");
  M5.Lcd.println(shadeIndex + 1);

  // Draw the selection boxes in a horizontal line from lightest to darkest
  for (int i = 0; i < 3; i++) {
    int boxX = startX + i * spacing;  // Position boxes from left to right

    shadeBoxes[i] = {boxX, startY, boxSize, boxSize}; // Store the box coordinates
    M5.Lcd.drawRect(boxX, startY, boxSize, boxSize, TFT_WHITE); // Draw box
    M5.Lcd.fillRect(boxX + 2, startY + 2, boxSize - 4, boxSize - 4, shades[i]); // Fill box with shade color

    if (i == shadeIndex) {
      M5.Lcd.drawRect(boxX, startY, boxSize, boxSize, TFT_GREEN); // Green border for selection
    }
  }
}

void setup() {
  M5.begin();
  M5.Lcd.setEpdMode(epd_mode_t::epd_fastest);
  M5.Lcd.setCursor(50, 80);
  M5.Lcd.println("Select Toast Shade");
  displayShade();
  Wire.begin();
}

int lastShadeIndex = -1;  // Declare lastShadeIndex globally

void sendShadeIndex() {
  if (shadeIndex != lastShadeIndex) {  // Avoid sending the same value repeatedly
    Wire.beginTransmission(8);
    Wire.write(shadeIndex);
    Wire.endTransmission();
    Serial.print("Sent shadeIndex: ");
    Serial.println(shadeIndex);
    
    lastShadeIndex = shadeIndex;  // Update last sent value
  }
}

void loop() {
  M5.update();

  if (M5.Touch.getCount() > 0) {
    auto touch = M5.Touch.getDetail();
    int touchX = touch.x;
    int touchY = touch.y;

    for (int i = 0; i < 3; i++) {  // Loop over 3 shades
      if (shadeBoxes[i].contains(touchX, touchY)) {
        if (shadeIndex != i) {  // Only update if shadeIndex actually changes
          shadeIndex = i;
          displayShade();
          sendShadeIndex();  // Send update only when it changes
        }
      }
    }
  }
}
