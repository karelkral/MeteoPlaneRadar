// =============================================================================
//  DevOpsDashboard43
//  LVGL 9 port for the Waveshare ESP32-S3-Touch-LCD-4.3 (800x480, RGB panel,
//  GT911 touch, CH422G expander).
//
//  ESP32_Display_Panel brings the board up from its built-in board definition
//  (esp_panel_board_supported_conf.h). This module connects it to LVGL:
//
//   - The RGB peripheral scans out two full-screen frame buffers in PSRAM.
//     LVGL draws in DIRECT mode straight into the hidden one, and only the
//     areas that changed. At the end of a frame the buffers swap, so the
//     picture never tears; LVGL copies the changed areas into the other buffer
//     by itself.
//   - One FreeRTOS task runs lv_timer_handler(). LVGL is not thread-safe:
//     code running anywhere else (loop(), web handlers) must wrap LVGL calls
//     in LvPort_Lock() / LvPort_Unlock() or an LvLock. Event and lv_timer
//     callbacks already run inside the lock.
//
//  Written for this project after Waveshare's 10_lvgl_v9_demo adapter, which
//  redraws the full screen every time (9 FPS here; DIRECT mode gives 29).
// =============================================================================
#pragma once
#include <lvgl.h>

// Board, display, touch, LVGL and its task. false = the display is unusable.
bool LvPort_Begin();

// timeoutMs < 0 waits forever. The lock is recursive.
bool LvPort_Lock(int32_t timeoutMs = -1);
void LvPort_Unlock();

// The board can only switch the backlight on or off (CH422G EXIO2).
void LvPort_SetBacklight(bool on);

// millis() of the last completed lv_timer_handler() pass - for a watchdog.
uint32_t LvPort_LastBeatMs();

lv_display_t* LvPort_Display();

// Scoped lock: { LvLock lock; lv_label_set_text(...); }
class LvLock {
public:
  LvLock() { LvPort_Lock(); }
  ~LvLock() { LvPort_Unlock(); }
  LvLock(const LvLock&) = delete;
  LvLock& operator=(const LvLock&) = delete;
};
