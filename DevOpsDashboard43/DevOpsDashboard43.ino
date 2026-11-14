// =============================================================================
//  DevOpsDashboard43
//  DevOps dashboard for the Waveshare ESP32-S3-Touch-LCD-4.3 (800x480) on LVGL 9.
//
//  Experimental successor of DevOpsDashboard (round 2.1" / 2.8C boards), which
//  is based on MeteoPlaneRadar by Petr / chiptron.cz (MIT, see LICENSE.txt).
//  Plan and progress: ../Doc/DevOpsDashboard43-plan.md
//
//  Phase 1: LVGL port only - a test screen with a button, the touch position
//  and a moving block to check touch, smoothness and tearing.
//
//  Board:   Waveshare ESP32-S3-Touch-LCD-4.3 (SKU 25948): ST7262 RGB panel,
//           GT911 touch, CH422G expander, ESP32-S3 N16R8.
//  Serial:  115200 Bd on the connector marked "USB TO UART".
//  Build:   build.cmd [COMx] - see sketch.yaml for the pinned versions.
// =============================================================================

#include <Arduino.h>
#include "esp_heap_caps.h"

#include "LvPort.h"
#include "Version.h"

#define HEAP_LOG_PERIOD_MS 10000

static lv_obj_t* s_touchLabel;
static lv_obj_t* s_countLabel;
static uint32_t  s_taps;

static void onTap(lv_event_t*) {
  lv_label_set_text_fmt(s_countLabel, "Taps: %lu", (unsigned long)++s_taps);
}

static void onPressing(lv_event_t*) {
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  lv_label_set_text_fmt(s_touchLabel, "Touch: %d, %d", (int)p.x, (int)p.y);
}

static void moveX(void* obj, int32_t x) { lv_obj_set_x((lv_obj_t*)obj, x); }

static void buildTestScreen() {
  lv_obj_t* scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
  lv_obj_set_scrollable(scr, false);
  lv_obj_add_event_cb(scr, onPressing, LV_EVENT_PRESSING, nullptr);

  lv_obj_t* title = lv_label_create(scr);
  lv_label_set_text_fmt(title, "DevOpsDashboard43 v%s - LVGL %d.%d.%d", FW_VERSION,
                        LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
  lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_CYAN), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

  lv_obj_t* btn = lv_button_create(scr);
  lv_obj_set_size(btn, 200, 70);
  lv_obj_align(btn, LV_ALIGN_CENTER, 0, -40);
  lv_obj_add_event_cb(btn, onTap, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* btnLabel = lv_label_create(btn);
  lv_label_set_text(btnLabel, "Tap me");
  lv_obj_center(btnLabel);

  s_countLabel = lv_label_create(scr);
  lv_label_set_text(s_countLabel, "Taps: 0");
  lv_obj_set_style_text_color(s_countLabel, lv_color_white(), 0);
  lv_obj_align_to(s_countLabel, btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 16);

  s_touchLabel = lv_label_create(scr);
  lv_label_set_text(s_touchLabel, "Touch: -");
  lv_obj_set_style_text_color(s_touchLabel, lv_color_white(), 0);
  lv_obj_align(s_touchLabel, LV_ALIGN_BOTTOM_LEFT, 20, -70);

  lv_obj_t* block = lv_obj_create(scr);
  lv_obj_set_size(block, 60, 40);
  lv_obj_set_y(block, 430);
  lv_obj_set_style_bg_color(block, lv_palette_main(LV_PALETTE_YELLOW), 0);
  lv_obj_set_clickable(block, false);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, block);
  lv_anim_set_exec_cb(&a, moveX);
  lv_anim_set_values(&a, 0, 800 - 60);
  lv_anim_set_duration(&a, 2000);
  lv_anim_set_reverse_duration(&a, 2000);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a);
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\n=== DevOpsDashboard43 v%s ===\n", FW_VERSION);

  if (!LvPort_Begin()) {
    Serial.println("Display init failed - restarting in 10 s");
    delay(10000);
    ESP.restart();
  }
  {
    LvLock lock;
    buildTestScreen();
  }
}

void loop() {
  static uint32_t lastLog = 0;
  if (millis() - lastLog >= HEAP_LOG_PERIOD_MS) {
    lastLog = millis();
    Serial.printf("Heap: internal %u free (largest %u), PSRAM %u free, LVGL beat %lu ms ago\n",
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                  (unsigned long)(millis() - LvPort_LastBeatMs()));
  }
  delay(20);
}
