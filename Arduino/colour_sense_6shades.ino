/*
  This Arduino sketch controls ToastBot using real-time color sensing 
  and feedback from multiple APDS-9960 RGB sensors. It converts RGB values to the CIE 
  L*a*b* color space to monitor the browning level of toast. 

  The user selects one of six toast shade levels (0 to 5), which correspond to a target 
  lightness (L*) value. Once the average L* value from three sensors falls below the 
  specified threshold, the heating elements are turned off and a toast-ready signal is triggered.

  Features:
    - Uses I2C multiplexer (PCA9546) to manage 4 APDS-9960 color sensors
    - Converts RGB to L*a*b* color space for perceptual color analysis
    - Supports 6 user-defined toast shades based on L* thresholds
    - Drives 4 heating elements and a toast-ready output
    - Communicates with master device via I2C

  Author: Stephanie Vo
*/

#include <Wire.h>
#include <Adafruit_APDS9960.h>

// I2C multiplexer address
#define PCAADDR 0x70

// Output pins for heat control and toast readiness
#define TOAST_READY_PIN 7
#define HEAT_PIN_1 3
#define HEAT_PIN_2 4
#define HEAT_PIN_3 5
#define HEAT_PIN_4 6

// Initialize four APDS-9960 color sensors
Adafruit_APDS9960 sensor1, sensor2, sensor3, sensor4;

// Variables for toast shade control
int shadeIndex = -1;
bool shadeReceived = false;
float targetL = -1;  // Target lightness based on shade

// Select a specific sensor channel on the PCA9546 I2C multiplexer
void pcaselect(uint8_t i) {
  if (i > 3) return;
  Wire.beginTransmission(PCAADDR);
  Wire.write(1 << i);  // Activate specific channel
  Wire.endTransmission();
}

// Normalize RGB value from 0–255 to 0–1 range
float normalizeRGB(int color) {
  return color / 255.0;
}

// Convert RGB to CIE XYZ color space
void RGBtoXYZ(float r, float g, float b, float &X, float &Y, float &Z) {
  r = (r > 0.04045) ? pow((r + 0.055) / 1.055, 2.4) : r / 12.92;
  g = (g > 0.04045) ? pow((g + 0.055) / 1.055, 2.4) : g / 12.92;
  b = (b > 0.04045) ? pow((b + 0.055) / 1.055, 2.4) : b / 12.92;

  // Linear transformation
  X = r * 0.4124564 + g * 0.3575761 + b * 0.1804375;
  Y = r * 0.2126729 + g * 0.7151522 + b * 0.0721750;
  Z = r * 0.0193339 + g * 0.1191920 + b * 0.9503041;
}

// Convert XYZ to CIE L*a*b* color space
void XYZtoLAB(float X, float Y, float Z, float &L, float &a, float &b) {
  float Xn = 0.95047, Yn = 1.00000, Zn = 1.08883;
  float Xr = X / Xn, Yr = Y / Yn, Zr = Z / Zn;

  Xr = (Xr > 0.008856) ? pow(Xr, 1.0 / 3.0) : (903.3 * Xr + 16) / 116;
  Yr = (Yr > 0.008856) ? pow(Yr, 1.0 / 3.0) : (903.3 * Yr + 16) / 116;
  Zr = (Zr > 0.008856) ? pow(Zr, 1.0 / 3.0) : (903.3 * Zr + 16) / 116;

  L = (116 * Yr) - 16;
  a = 500 * (Xr - Yr);
  b = 200 * (Yr - Zr);
}

// Initialization
void setup() {
  Serial.begin(9600);
  Wire.begin(8); // I2C slave address
  Wire.onReceive(receiveEvent); // Register I2C receive callback

  // Set output pins for heater and toast-ready indicator
  pinMode(TOAST_READY_PIN, OUTPUT);
  digitalWrite(TOAST_READY_PIN, HIGH); // High = not ready

  digitalWrite(HEAT_PIN_1, HIGH);
  digitalWrite(HEAT_PIN_2, HIGH);
  digitalWrite(HEAT_PIN_3, HIGH);
  digitalWrite(HEAT_PIN_4, HIGH);

  // Initialize all color sensors via multiplexer
  Adafruit_APDS9960* sensors[] = {&sensor1, &sensor2, &sensor3, &sensor4};
  for (int i = 0; i < 4; i++) {
    pcaselect(i);
    if (!sensors[i]->begin()) {
      Serial.print("Error initializing APDS-9960 sensor ");
      Serial.println(i + 1);
    } else {
      Serial.print("APDS-9960 sensor ");
      Serial.print(i + 1);
      Serial.println(" initialized.");
      sensors[i]->enableColor(true); // Turn on RGB sensing
    }
  }
}

// Main loop: reads sensors and checks if toast is ready
void loop() {
  float L1 = readSensor(sensor1, 1);
  float L2 = readSensor(sensor2, 2);
  float L3 = readSensor(sensor3, 3);

  float avgL = (L1 + L2 + L3) / 3.0;

  Serial.print("Average L*: ");
  Serial.println(avgL);

  // Check if average L* reaches target
  if (targetL > 0 && avgL <= targetL) {
    Serial.println("Target shade reached! Turning off heaters.");
    digitalWrite(HEAT_PIN_1, LOW);
    digitalWrite(HEAT_PIN_2, LOW);
    digitalWrite(HEAT_PIN_3, LOW);
    digitalWrite(HEAT_PIN_4, LOW);
    digitalWrite(TOAST_READY_PIN, LOW); // Signal toast is ready
    targetL = -1; // Prevent retrigger
  }

  delay(1000); // Poll every second
}

// Called when a shade index is sent from master (e.g. Raspberry Pi)
void receiveEvent(int howMany) {
  while (Wire.available()) {
    shadeIndex = Wire.read();
    Serial.print("Received Shade Index: ");
    Serial.println(shadeIndex);
    shadeReceived = true;

    // Map shade index (0 to 5) to target L* value
    switch (shadeIndex) {
      case 0: targetL = 85; break; // Very light
      case 1: targetL = 75; break; // Light
      case 2: targetL = 65; break; // Medium-light
      case 3: targetL = 55; break; // Medium
      case 4: targetL = 45; break; // Medium-dark
      case 5: targetL = 35; break; // Very dark
      default: 
        targetL = -1;  // Invalid input
        Serial.println("Invalid shade index received.");
        break;
    }
  }
}

// Reads color from one sensor and returns the L* value
float readSensor(Adafruit_APDS9960 &sensor, uint8_t sensorID) {
  uint16_t r, g, b, c;

  pcaselect(sensorID - 1); // Activate sensor via I2C mux
  sensor.getColorData(&r, &g, &b, &c); // Read RGB

  float rNorm = normalizeRGB(r);
  float gNorm = normalizeRGB(g);
  float bNorm = normalizeRGB(b);

  float X, Y, Z;
  RGBtoXYZ(rNorm, gNorm, bNorm, X, Y, Z);

  float L, a, bLab;
  XYZtoLAB(X, Y, Z, L, a, bLab);

  // Debug output
  Serial.print("Sensor ");
  Serial.print(sensorID);
  Serial.print(" - L: ");
  Serial.print(L);
  Serial.print(" a: ");
  Serial.print(a);
  Serial.print(" b: ");
  Serial.println(bLab);

  return L;
}
