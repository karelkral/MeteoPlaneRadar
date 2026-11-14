// =============================================================================
//  DevOpsDashboard43
//  The UI frame: status bar (screen title, page dots, clock), the screens in an
//  lv_tileview (swipe left / right), full-screen overlays (WiFi portal,
//  messages) and the software dimming layer.
//
//  Ui_* functions take the LVGL lock themselves - call them from loop() or
//  setup(), never with Settings or the network in the middle.
// =============================================================================
#pragma once
#include <stdint.h>

#define UI_SCREEN_DASHBOARD 0
#define UI_SCREEN_CLOCK     1
#define UI_SCREEN_COUNT     2

// Build the UI. Once, from setup(), after LvPort_Begin().
void Ui_Begin(uint8_t screen, uint8_t brightnessPct);

// 0 switches the backlight off. 1..100 dims in software: a black layer over
// everything, never below 10 % - the board can only switch the backlight.
void Ui_SetBrightness(uint8_t pct);

void Ui_ShowScreen(uint8_t screen, bool animate);

// Screen on the display now (any task).
uint8_t Ui_CurrentScreen();

// Screen picked by a swipe since the last call, or -1. For loop(), which
// stores it: Settings must not be touched from the LVGL task.
int Ui_TakeScreenChange();

// --- Full-screen overlays (over the screens and the status bar) -------------
// They swallow touches, so nothing can be swiped away underneath.

// Access point instructions with a WiFi QR code. pass "" = open network.
void Ui_ShowPortal(const char* ssid, const char* pass, const char* ip);

// A centred message, e.g. "Connecting to WiFi..." + the network name.
void Ui_ShowMessage(const char* title, const char* detail);

// Back to the screens.
void Ui_HideOverlay();
