// =============================================================================
//  DevOpsDashboard43
//  LVGL 9 port for the Waveshare ESP32-S3-Touch-LCD-4.3. See LvPort.h.
// =============================================================================
#include "LvPort.h"
#include <Arduino.h>
#include <esp_display_panel.hpp>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

#define LVPORT_TASK_STACK    (12 * 1024)
#define LVPORT_TASK_PRIO     2          // above loop() (1)
#define LVPORT_TASK_CORE     1          // with loop(); WiFi runs on core 0
#define LVPORT_DELAY_MIN_MS  2
#define LVPORT_DELAY_MAX_MS  100
#define LVPORT_BOUNCE_LINES  10         // 800 x 10 px, in internal RAM
#define LVPORT_FRAME_WAIT_MS 100        // a frame takes ~26 ms at 16 MHz

static Board*                s_board;
static LCD*                  s_lcd;
static Touch*                s_touch;
static Backlight*            s_backlight;
static lv_display_t*         s_disp;
static SemaphoreHandle_t     s_mutex;
static TaskHandle_t volatile s_task;
static volatile uint32_t     s_beatMs;

static uint32_t tickCb() { return millis(); }

// End of a frame (ISR). Wakes the LVGL task waiting in flushCb(). It fires on
// every frame; flushCb() drops the counts that piled up before it waited.
IRAM_ATTR static bool frameDoneIsr(void*) {
  BaseType_t woken = pdFALSE;
  if (s_task) vTaskNotifyGiveFromISR(s_task, &woken);
  return woken == pdTRUE;
}

static void flushCb(lv_display_t* disp, const lv_area_t*, uint8_t* pxMap) {
  // DIRECT mode: the pixels are already in place in the hidden buffer. After
  // the last area of a frame, show that buffer and wait until the panel has
  // finished the frame that was still reading the old one - only then may
  // LVGL draw into it.
  if (lv_display_flush_is_last(disp) && xTaskGetCurrentTaskHandle() == s_task) {
    s_lcd->switchFrameBufferTo(pxMap);
    ulTaskNotifyTake(pdTRUE, 0);
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(LVPORT_FRAME_WAIT_MS));
  }
  lv_display_flush_ready(disp);
}

static void touchReadCb(lv_indev_t*, lv_indev_data_t* data) {
  TouchPoint p;
  if (s_touch && s_touch->readPoints(&p, 1, 0) > 0) {
    data->point.x = p.x;
    data->point.y = p.y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

static void lvglTask(void*) {
  for (;;) {
    uint32_t waitMs = LVPORT_DELAY_MAX_MS;
    if (LvPort_Lock()) {
      waitMs = lv_timer_handler();
      LvPort_Unlock();
    }
    s_beatMs = millis();
    if (waitMs < LVPORT_DELAY_MIN_MS) waitMs = LVPORT_DELAY_MIN_MS;
    if (waitMs > LVPORT_DELAY_MAX_MS) waitMs = LVPORT_DELAY_MAX_MS;   // also LV_NO_TIMER_READY
    vTaskDelay(pdMS_TO_TICKS(waitMs));
  }
}

bool LvPort_Begin() {
  s_board = new Board();
  if (!s_board->init()) {
    Serial.println("LvPort: board init failed");
    return false;
  }
  s_lcd = s_board->getLCD();
  if (!s_lcd) {
    Serial.println("LvPort: no LCD in the board definition");
    return false;
  }
  // Two frame buffers for DIRECT mode. The RGB peripheral reads them through a
  // small bounce buffer in internal RAM, which keeps the picture steady while
  // WiFi and the CPU load PSRAM.
  s_lcd->configFrameBufferNumber(2);
  auto* bus = s_lcd->getBus();
  if (bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
    static_cast<BusRGB*>(bus)->configRGB_BounceBufferSize(s_lcd->getFrameWidth() * LVPORT_BOUNCE_LINES);
  }
  if (!s_board->begin()) {
    Serial.println("LvPort: board begin failed");
    return false;
  }
  s_touch = s_board->getTouch();
  s_backlight = s_board->getBacklight();

  const int w = s_lcd->getFrameWidth();
  const int h = s_lcd->getFrameHeight();
  void* fb0 = s_lcd->getFrameBufferByIndex(0);
  void* fb1 = s_lcd->getFrameBufferByIndex(1);
  if (!fb0 || !fb1) {
    Serial.println("LvPort: frame buffers missing");
    return false;
  }

  s_mutex = xSemaphoreCreateRecursiveMutex();
  lv_init();
  lv_tick_set_cb(tickCb);

  s_disp = lv_display_create(w, h);
  lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);   // before the buffers (stride)
  lv_display_set_buffers(s_disp, fb0, fb1, (uint32_t)w * h * 2, LV_DISPLAY_RENDER_MODE_DIRECT);
  lv_display_set_flush_cb(s_disp, flushCb);

  if (s_touch) {
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchReadCb);
    lv_indev_set_display(indev, s_disp);
  } else {
    Serial.println("LvPort: no touch controller");
  }

  s_lcd->attachRefreshFinishCallback(frameDoneIsr, nullptr);
  s_beatMs = millis();
  TaskHandle_t task = nullptr;
  if (xTaskCreatePinnedToCore(lvglTask, "lvgl", LVPORT_TASK_STACK, nullptr,
                              LVPORT_TASK_PRIO, &task, LVPORT_TASK_CORE) != pdPASS) {
    Serial.println("LvPort: cannot start the LVGL task");
    return false;
  }
  s_task = task;

  Serial.printf("LvPort: %dx%d, LVGL %d.%d.%d, DIRECT mode, 2 frame buffers in PSRAM\n",
                w, h, LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
  return true;
}

bool LvPort_Lock(int32_t timeoutMs) {
  if (!s_mutex) return false;
  const TickType_t ticks = timeoutMs < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
  return xSemaphoreTakeRecursive(s_mutex, ticks) == pdTRUE;
}

void LvPort_Unlock() {
  if (s_mutex) xSemaphoreGiveRecursive(s_mutex);
}

void LvPort_SetBacklight(bool on) {
  if (!s_backlight) return;
  // The expander shares the I2C bus with the touch controller, which the LVGL
  // task reads - the lock keeps the two apart.
  LvLock lock;
  if (on) s_backlight->on();
  else    s_backlight->off();
}

uint32_t LvPort_LastBeatMs() { return s_beatMs; }

lv_display_t* LvPort_Display() { return s_disp; }

void LvPort_Restart() {
  // The lock is never given back, so the LVGL task cannot draw into the frame
  // buffers that del() frees. Bounded wait: restart even if LVGL is stuck.
  const bool locked = LvPort_Lock(1000);
  if (s_backlight) s_backlight->off();
  if (locked && s_lcd) s_lcd->del();
  ESP.restart();
  for (;;) {}
}
