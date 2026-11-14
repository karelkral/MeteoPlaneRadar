// =============================================================================
//  DevOpsDashboard43
//  DevOps dashboard for the Waveshare ESP32-S3-Touch-LCD-4.3 (800x480) on LVGL 9.
//
//  Experimental successor of DevOpsDashboard (round 2.1" / 2.8C boards), which
//  is based on MeteoPlaneRadar by Petr / chiptron.cz (MIT, see LICENSE.txt).
//  Plan and progress: ../Doc/DevOpsDashboard43-plan.md
//
//  Screens (swipe left / right to switch):
//    0) Dashboard - ring chart of pipeline results (test data for now)
//    1) Clock     - large time and date
//
//  Board:   Waveshare ESP32-S3-Touch-LCD-4.3 (SKU 25948): ST7262 RGB panel,
//           GT911 touch, CH422G expander, ESP32-S3 N16R8.
//  Serial:  115200 Bd on the connector marked "USB TO UART".
//  Build:   build.cmd [COMx] - see sketch.yaml for the pinned versions.
// =============================================================================

#include <Arduino.h>
#include "esp_heap_caps.h"

#include "LvPort.h"
#include "Ui.h"
#include "Version.h"

#define HEAP_LOG_PERIOD_MS 60000

void setup() {
  Serial.begin(115200);
  Serial.printf("\n=== DevOpsDashboard43 v%s ===\n", FW_VERSION);

  if (!LvPort_Begin()) {
    Serial.println("Display init failed - restarting in 10 s");
    delay(10000);
    ESP.restart();
  }
  Ui_Begin(UI_SCREEN_DASHBOARD, 100);
  Serial.println("Setup done");
}

void loop() {
  const int screen = Ui_TakeScreenChange();
  if (screen >= 0) Serial.printf("Screen: %d\n", screen);

  static uint32_t lastLog = 0;
  if (millis() - lastLog >= HEAP_LOG_PERIOD_MS) {
    lastLog = millis();
    Serial.printf("Heap: internal %u free (largest %u), PSRAM %u free\n",
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  }
  delay(20);
}
