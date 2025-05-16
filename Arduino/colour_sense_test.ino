/*
  APDS9960 Color Sensor + Manual PWM Control + RGB to L*a*b* Conversion

  Description:
  This Arduino sketch interfaces with a SparkFun APDS9960 color sensor to read RGB color values,
  converts them to the CIE L*a*b* color space, and prints them via serial. It also listens for 
  serial input to update a user-defined PWM duty cycle (0–100%) to manually control a heating 
  element or other load on pin 12.

  The system uses:
    - An APDS9960 color sensor via I2C
    - An Adafruit NeoPixel ring (for display or testing)
    - Manual PWM (via digitalWrite delays) to simulate analog output
    - RGB to L*a*b* color space conversion for perceptual color analysis

  Author: Stephanie Vo
*/

#include <Wire.h>
#include <SparkFun_APDS9960.h>
#include <math.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN    6
#define LED_COUNT  16

SparkFun_APDS9960 apds = SparkFun_APDS9960();
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

uint16_t ambient_light = 0;
uint16_t red_light = 0;
uint16_t green_light = 0;
uint16_t blue_light = 0;
float L, a, b;

const int pwmPin = 12;
int duty_cycle = 80;  // Initial duty cycle %

String inputString = "";  
bool inputComplete = false;

void setup() {
  Serial.begin(9600);
  Serial.println("APDS-9960 Color Sensor Test with Arduino Nano + LAB Conversion");

  strip.begin();
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(255, 255, 255));  // Always white
  }
  strip.show();

  Wire.begin();
  if (apds.init()) {
    Serial.println("APDS-9960 initialization complete");
  } else {
    Serial.println("Something went wrong during APDS-9960 init!");
  }

  if (apds.enableLightSensor(false)) {
    Serial.println("Light sensor is now running");
  } else {
    Serial.println("Something went wrong during light sensor init!");
  }

  delay(500);
  pinMode(pwmPin, OUTPUT);
}

void loop() {
  // Non-blocking Serial read
  while (Serial.available() > 0) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      inputComplete = true;
    } else {
      inputString += inChar;
    }
  }

  if (inputComplete) {
    int new_duty = inputString.toInt();
    if (new_duty >= 0 && new_duty <= 100) {
      duty_cycle = new_duty;
    }
    inputString = "";
    inputComplete = false;
  }

  // Read Color Sensor
  if (apds.readAmbientLight(ambient_light) &&
      apds.readRedLight(red_light) &&
      apds.readGreenLight(green_light) &&
      apds.readBlueLight(blue_light)) {

    Serial.print("Ambient: ");
    Serial.print(ambient_light);
    Serial.print(" Red: ");
    Serial.print(red_light);
    Serial.print(" Green: ");
    Serial.print(green_light);
    Serial.print(" Blue: ");
    Serial.println(blue_light);

    convertRGBToLAB(red_light, green_light, blue_light);

    Serial.print("L: ");
    Serial.print(L);
    Serial.print(" a: ");
    Serial.print(a);
    Serial.print(" b: ");
    Serial.println(b);
    Serial.println("-------------------");
  }

  // Manual PWM Section
  if (duty_cycle >= 100) {
    digitalWrite(pwmPin, HIGH);
    delay(100);  // Control loop speed
  } 
  else if (duty_cycle <= 0) {
    digitalWrite(pwmPin, LOW);
    delay(100);
  } 
  else {
    int mini_cycle_time = 125;  // ms per mini cycle
    int high_time = mini_cycle_time * duty_cycle / 100;
    int low_time = mini_cycle_time - high_time;

    digitalWrite(pwmPin, HIGH); delay(high_time);
    digitalWrite(pwmPin, LOW);  delay(low_time);

    digitalWrite(pwmPin, HIGH); delay(high_time);
    digitalWrite(pwmPin, LOW);  delay(low_time);

    digitalWrite(pwmPin, HIGH); delay(high_time);
    digitalWrite(pwmPin, LOW);  delay(low_time);

    digitalWrite(pwmPin, HIGH); delay(high_time);
    digitalWrite(pwmPin, LOW);  delay(low_time);

    digitalWrite(pwmPin, HIGH); delay(high_time);
    digitalWrite(pwmPin, LOW);  delay(low_time);

    digitalWrite(pwmPin, HIGH); delay(high_time);
    digitalWrite(pwmPin, LOW);  delay(low_time);

    digitalWrite(pwmPin, HIGH); delay(high_time);
    digitalWrite(pwmPin, LOW);  delay(low_time);

    digitalWrite(pwmPin, HIGH); delay(high_time);
    digitalWrite(pwmPin, LOW);  delay(low_time);

    digitalWrite(pwmPin, HIGH); delay(high_time);
    digitalWrite(pwmPin, LOW);  delay(low_time);

    digitalWrite(pwmPin, HIGH); delay(high_time);
    digitalWrite(pwmPin, LOW);  delay(low_time);

    digitalWrite(pwmPin, HIGH); delay(high_time);
    digitalWrite(pwmPin, LOW);  delay(low_time);

    digitalWrite(pwmPin, HIGH); delay(high_time);
    digitalWrite(pwmPin, LOW);  delay(low_time);

    digitalWrite(pwmPin, HIGH); delay(high_time);
    digitalWrite(pwmPin, LOW);  delay(low_time);

    digitalWrite(pwmPin, HIGH); delay(high_time);
    digitalWrite(pwmPin, LOW);  delay(low_time);

    digitalWrite(pwmPin, HIGH); delay(high_time);
    digitalWrite(pwmPin, LOW);  delay(low_time);

    digitalWrite(pwmPin, HIGH); delay(high_time);
    digitalWrite(pwmPin, LOW);  delay(low_time);
  }
}

// RGB to LAB Conversion
void convertRGBToLAB(float R, float G, float B) {
  float maxVal = max(max(R, G), B);
  if (maxVal <= 0) {
    L = 0; a = 0; b = 0;
    return;
  }

  R = R / maxVal;
  G = G / maxVal;
  B = B / maxVal;

  R = R / 255.0;
  G = G / 255.0;
  B = B / 255.0;

  if (R > 0.04045) R = pow((R + 0.055) / 1.055, 2.4);
  else R = R / 12.92;
  if (G > 0.04045) G = pow((G + 0.055) / 1.055, 2.4);
  else G = G / 12.92;
  if (B > 0.04045) B = pow((B + 0.055) / 1.055, 2.4);
  else B = B / 12.92;

  R *= 100.0;
  G *= 100.0;
  B *= 100.0;

  float X = R * 0.4124 + G * 0.3576 + B * 0.1805;
  float Y = R * 0.2126 + G * 0.7152 + B * 0.0722;
  float Z = R * 0.0193 + G * 0.1192 + B * 0.9505;

  float Xn = 95.047;
  float Yn = 100.0;
  float Zn = 108.883;

  float x = X / Xn;
  float y = Y / Yn;
  float z = Z / Zn;

  if (x > 0.008856) x = pow(x, 1.0 / 3.0);
  else x = (7.787 * x) + (16.0 / 116.0);

  if (y > 0.008856) y = pow(y, 1.0 / 3.0);
  else y = (7.787 * y) + (16.0 / 116.0);

  if (z > 0.008856) z = pow(z, 1.0 / 3.0);
  else z = (7.787 * z) + (16.0 / 116.0);

  L = (116.0 * y) - 16.0;
  a = 500.0 * (x - y);
  b = 200.0 * (y - z);
}
