#include "M5Dial.h"
#include <Arduino.h>
#include <lvgl.h>
#include <Wire.h>
#include <SPI.h>
#include "ui.h"

// init the tft espi
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;  // Descriptor of a display driver
static lv_indev_drv_t indev_drv; // Descriptor of a touch driver

long oldPosition = -999;
int currentShade = 1;  // Starts at shade 1
const int num_positions = 6;

#define EXAMPLE_LCD_H_RES 240
#define EXAMPLE_LCD_V_RES 240
#define LV_VER_RES_MAX 240
#define LV_HOR_RES_MAX 240
M5GFX *tft;


void tft_lv_initialization() {
  lv_init();
  static lv_color_t buf1[(LV_HOR_RES_MAX * LV_VER_RES_MAX) / 10];  // Declare a buffer for 1/10 screen siz
  static lv_color_t buf2[(LV_HOR_RES_MAX * LV_VER_RES_MAX) / 10];  // second buffer is optionnal
  // Initialize `disp_buf` display buffer with the buffer(s).
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, (LV_HOR_RES_MAX * LV_VER_RES_MAX) / 10);
  tft=&M5Dial.Lcd;
}

// Display flushing
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft->startWrite();
  tft->setAddrWindow(area->x1, area->y1, w, h);
  tft->pushColors((uint16_t *)&color_p->full, w * h, true);
  tft->endWrite();
  lv_disp_flush_ready(disp);
}

void init_disp_driver() {
  lv_disp_drv_init(&disp_drv);  // Basic initialization
  disp_drv.flush_cb = my_disp_flush;  // Set your driver function
  disp_drv.draw_buf = &draw_buf;      // Assign the buffer to the display
  disp_drv.hor_res = LV_HOR_RES_MAX;  // Set the horizontal resolution of the display
  disp_drv.ver_res = LV_VER_RES_MAX;  // Set the vertical resolution of the display
  lv_disp_drv_register(&disp_drv);                   // Finally register the driver
  lv_disp_set_bg_color(NULL, lv_color_hex3(0x000));  // Set default background color to black
}

void my_touchpad_read(lv_indev_drv_t * drv, lv_indev_data_t * data)
{
  uint32_t currentTime = millis(); 
  M5.Touch.update(currentTime);

  if(M5.Touch.getCount() > 0)
  {

 
  auto pos = M5.Touch.getDetail();
    data->state = LV_INDEV_STATE_PRESSED; 
    data->point.x = pos.x;
    data->point.y = pos.y;
  }
  else
  {
 data->state = LV_INDEV_STATE_RELEASED;
  }
}

void init_touch_driver() {
  lv_disp_drv_register(&disp_drv);
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_t * my_indev = lv_indev_drv_register(&indev_drv);  // register
}

void setup()
{
  auto cfg = M5.config();
  M5Dial.begin(cfg, true, false);
  //M5Dial.Display.setBrightness(100);
  Serial.begin(115200);
  tft_lv_initialization();
  init_disp_driver();
  init_touch_driver();
  ui_init();
}

void loop() {
    uint32_t wait_ms = lv_timer_handler();
    M5.delay(wait_ms);

    M5Dial.update();

    long newPosition = M5Dial.Encoder.read();

    // Rotary turned
    if (newPosition != oldPosition) {
        int diff = newPosition - oldPosition;

        if (diff > 0) {
            currentShade = (currentShade % num_positions) + 1;  // 1→6 wraps to 1
        } else {
            currentShade = (currentShade - 2 + num_positions) % num_positions + 1;
        }

        // Calculate angle for shade (position 1 starts at top)
        const int radius = 85;
        const int circle_offset_x = 30;
        const int circle_offset_y = 25;
        float angle_rad = (2 * PI / num_positions) * (currentShade - 1) - PI / 2;

        int dot_x = radius * cos(angle_rad) + circle_offset_x;
        int dot_y = radius * sin(angle_rad) + circle_offset_y;

        lv_obj_set_pos(ui_Image20, dot_x - 27, dot_y - 27);

        Serial.printf("Rotated: Shade %d selected\n", currentShade);

        oldPosition = newPosition;
    }

    // Button pressed
    if (M5Dial.BtnA.wasPressed()) {
        M5Dial.Speaker.tone(1000, 50);  // clicky sound
        Serial.printf("🔥 Toast shade selected: %d\n", currentShade);
    }
}