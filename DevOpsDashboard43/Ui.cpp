// =============================================================================
//  DevOpsDashboard43
//  The UI frame. See Ui.h.
// =============================================================================
#include "Ui.h"
#include "LvPort.h"
#include "Theme.h"
#include "LocalTime.h"
#include "DashData.h"
#include "ScreenDashboard.h"
#include "ScreenClock.h"
#include "src/fonts/Fonts.h"
#include <atomic>

#define DIM_MIN_PCT 10

static const char* const TITLES[UI_SCREEN_COUNT] = { "DevOps dashboard", "Clock" };

static lv_obj_t*    s_tv;
static lv_obj_t*    s_title;
static lv_obj_t*    s_dot[UI_SCREEN_COUNT];
static lv_obj_t*    s_time;
static lv_obj_t*    s_dim;
static uint8_t      s_screen;
static long         s_shownMin = -2;
static int8_t       s_backlight = -1;          // -1 = not set yet
static std::atomic<int> s_screenChanged{-1};   // written by the LVGL task

// Title, dots and the bar clock for the active screen. The clock screen shows
// the time itself, so the bar leaves it out there.
static void showActive(uint8_t idx) {
  s_screen = idx;
  lv_label_set_text(s_title, TITLES[idx]);
  for (int i = 0; i < UI_SCREEN_COUNT; i++)
    lv_obj_set_style_bg_color(s_dot[i], i == idx ? COL_TEXT : COL_LINE, 0);
  lv_obj_set_hidden(s_time, idx == UI_SCREEN_CLOCK);
}

static void onTileChanged(lv_event_t*) {
  lv_obj_t* tile = lv_tileview_get_tile_active(s_tv);
  const uint8_t idx = (uint8_t)(intptr_t)lv_obj_get_user_data(tile);
  if (idx == s_screen) return;
  showActive(idx);
  s_screenChanged = idx;
}

static void barClock(lv_timer_t*) {
  struct tm t;
  const bool known = LocalTime_Get(t);
  const long key = known ? t.tm_hour * 60 + t.tm_min : -1;
  if (key == s_shownMin) return;
  s_shownMin = key;
  if (known) lv_label_set_text_fmt(s_time, "%02d:%02d", t.tm_hour, t.tm_min);
  else       lv_label_set_text(s_time, "--:--");
}

static lv_obj_t* plainObj(lv_obj_t* parent) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_scrollable(o, false);
  lv_obj_set_clickable(o, false);
  return o;
}

static void createBar(lv_obj_t* scr) {
  lv_obj_t* bar = plainObj(scr);
  lv_obj_set_size(bar, UI_W, UI_BAR_H);
  lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(bar, 1, 0);
  lv_obj_set_style_border_color(bar, COL_LINE, 0);

  s_title = lv_label_create(bar);
  lv_obj_set_style_text_font(s_title, &inter_24, 0);
  lv_obj_set_style_text_color(s_title, COL_ACCENT, 0);
  lv_obj_align(s_title, LV_ALIGN_LEFT_MID, 20, 0);

  for (int i = 0; i < UI_SCREEN_COUNT; i++) {
    s_dot[i] = plainObj(bar);
    lv_obj_set_size(s_dot[i], 10, 10);
    lv_obj_set_style_radius(s_dot[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_dot[i], LV_OPA_COVER, 0);
    lv_obj_align(s_dot[i], LV_ALIGN_CENTER, (2 * i - (UI_SCREEN_COUNT - 1)) * 10, 0);
  }

  s_time = lv_label_create(bar);
  lv_obj_set_style_text_font(s_time, &inter_24, 0);
  lv_obj_set_style_text_color(s_time, COL_TEXT, 0);
  lv_obj_align(s_time, LV_ALIGN_RIGHT_MID, -20, 0);
}

static lv_obj_t* addTile(uint8_t idx, lv_dir_t dir) {
  lv_obj_t* tile = lv_tileview_add_tile(s_tv, idx, 0, dir);
  lv_obj_set_user_data(tile, (void*)(intptr_t)idx);
  lv_obj_set_style_pad_all(tile, 0, 0);
  lv_obj_set_scrollable(tile, false);         // drags go to the tileview
  return tile;
}

void Ui_Begin(uint8_t screen, uint8_t brightnessPct) {
  if (screen >= UI_SCREEN_COUNT) screen = UI_SCREEN_DASHBOARD;
  {
    LvLock lock;
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(scr, false);

    createBar(scr);

    s_tv = lv_tileview_create(scr);
    lv_obj_set_size(s_tv, UI_W, UI_TILE_H);
    lv_obj_set_pos(s_tv, 0, UI_BAR_H);
    lv_obj_set_style_bg_color(s_tv, COL_BG, 0);
    lv_obj_set_style_bg_opa(s_tv, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(s_tv, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* dash = addTile(UI_SCREEN_DASHBOARD, LV_DIR_RIGHT);
    lv_obj_t* clock = addTile(UI_SCREEN_CLOCK, LV_DIR_LEFT);
    ScreenDashboard_Create(dash);
    ScreenDashboard_SetData(DashData_Test());
    ScreenClock_Create(clock);

    showActive(screen);
    lv_tileview_set_tile_by_index(s_tv, screen, 0, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_tv, onTileChanged, LV_EVENT_VALUE_CHANGED, nullptr);

    barClock(nullptr);
    lv_timer_create(barClock, 1000, nullptr);

    // Dimming layer above everything; clicks go through to the screens.
    s_dim = plainObj(lv_layer_top());
    lv_obj_set_size(s_dim, UI_W, UI_H);
    lv_obj_set_style_bg_color(s_dim, lv_color_black(), 0);
    lv_obj_set_hidden(s_dim, true);
  }
  Ui_SetBrightness(brightnessPct);
}

void Ui_SetBrightness(uint8_t pct) {
  LvLock lock;
  const bool on = pct > 0;
  if (on) {
    if (pct < DIM_MIN_PCT) pct = DIM_MIN_PCT;
    if (pct > 100) pct = 100;
    const lv_opa_t opa = (lv_opa_t)((100 - pct) * 255 / 100);
    lv_obj_set_style_bg_opa(s_dim, opa, 0);
    lv_obj_set_hidden(s_dim, opa == 0);
  }
  if (s_backlight != (int8_t)on) {
    s_backlight = on;
    LvPort_SetBacklight(on);
  }
}

void Ui_ShowScreen(uint8_t screen, bool animate) {
  if (screen >= UI_SCREEN_COUNT) return;
  LvLock lock;
  showActive(screen);
  lv_tileview_set_tile_by_index(s_tv, screen, 0, animate ? LV_ANIM_ON : LV_ANIM_OFF);
}

int Ui_TakeScreenChange() {
  return s_screenChanged.exchange(-1);
}
