// =============================================================================
//  DevOpsDashboard43
//  The UI frame. See Ui.h.
// =============================================================================
#include "Ui.h"
#include "LvPort.h"
#include "Theme.h"
#include "LocalTime.h"
#include "DashData.h"
#include "DevOpsApi.h"
#include "ScreenDashboard.h"
#include "ScreenClock.h"
#include "src/fonts/Fonts.h"
#include <atomic>
#include <string.h>

#define DIM_MIN_PCT 10
#define QR_SIZE     300
#define TITLE_MAX_W 380             // the title never reaches the page dots
#define REFRESH_W   56              // touch target of the refresh icon
#define SPIN_MS     1000

static const char* const TITLES[UI_SCREEN_COUNT] = { "Poirot computers", "Clock" };

static lv_obj_t*    s_tv;
static lv_obj_t*    s_title;
static lv_obj_t*    s_dot[UI_SCREEN_COUNT];
static lv_obj_t*    s_time;
static lv_obj_t*    s_refresh;                 // touch target of the icon
static lv_obj_t*    s_refreshIcon;
static lv_obj_t*    s_spinner;                 // replaces the icon while downloading
static char         s_profile[API_PROFILE_NAME_MAX + 1] = "";
static bool         s_spinning = false;
static lv_obj_t*    s_dim;
static lv_obj_t*    s_overlay;                 // full-screen panel for the two below
static lv_obj_t*    s_portal;
static lv_obj_t*    s_qr;
static lv_obj_t*    s_portalSsid;
static lv_obj_t*    s_portalPass;
static lv_obj_t*    s_portalUrl;
static lv_obj_t*    s_msg;
static lv_obj_t*    s_msgTitle;
static lv_obj_t*    s_msgDetail;
static volatile uint8_t s_screen;
static long         s_shownMin = -2;
static int8_t       s_backlight = -1;          // -1 = not set yet
static std::atomic<int> s_screenChanged{-1};   // written by the LVGL task

// Title (with the name of the active API profile on the dashboard), the dots
// and the refresh icon for the screen on show.
static void updateTitle() {
  char t[64];
  if (s_screen == UI_SCREEN_DASHBOARD && s_profile[0])
    snprintf(t, sizeof(t), "%s \xC2\xB7 %s", TITLES[s_screen], s_profile);
  else
    snprintf(t, sizeof(t), "%s", TITLES[s_screen]);
  if (strcmp(lv_label_get_text(s_title), t) != 0) lv_label_set_text(s_title, t);
}

// A spinner (an arc) instead of a rotated symbol: rotating the label goes through
// an LVGL layer, which made the whole picture flicker in DIRECT render mode.
static void setSpinning(bool on) {
  if (on == s_spinning) return;
  s_spinning = on;
  lv_obj_set_hidden(s_spinner, !on);
  lv_obj_set_hidden(s_refreshIcon, on);
}

// The icon only makes sense on the dashboard, and only with a profile to fetch.
static void updateRefresh(const ApiSnapshot& a) {
  const bool usable = a.state != API_OFF && a.state != API_NO_KEY;
  lv_obj_set_hidden(s_refresh, !(usable && s_screen == UI_SCREEN_DASHBOARD));
  setSpinning(a.busy && usable);
}

static void onRefreshClicked(lv_event_t*) {
  DevOpsApi_RefreshNow();                      // thread-safe, ignored while busy
}

// Every 500 ms: new API data, the age of the old ones, the profile name.
static void apiTimer(lv_timer_t*) {
  static uint32_t lastRev = UINT32_MAX;
  static uint32_t lastSec = UINT32_MAX;
  const ApiSnapshot a = DevOpsApi_Get();
  const uint32_t sec = millis() / 1000UL;
  if (a.revision == lastRev && sec == lastSec) return;
  lastRev = a.revision;
  lastSec = sec;
  if (strcmp(s_profile, a.profile) != 0) {
    strlcpy(s_profile, a.profile, sizeof(s_profile));
    updateTitle();
  }
  ScreenDashboard_SetData(a);
  updateRefresh(a);
}

static void showActive(uint8_t idx) {
  s_screen = idx;
  updateTitle();
  for (int i = 0; i < UI_SCREEN_COUNT; i++)
    lv_obj_set_style_bg_color(s_dot[i], i == idx ? COL_TEXT : COL_LINE, 0);
  lv_obj_set_hidden(s_time, idx == UI_SCREEN_CLOCK);
  updateRefresh(DevOpsApi_Get());
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
  lv_obj_set_width(s_title, TITLE_MAX_W);
  lv_label_set_long_mode(s_title, LV_LABEL_LONG_MODE_DOTS);
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

  // Manual refresh: the icon left of the clock, in a wider touch target.
  s_refresh = plainObj(bar);
  lv_obj_set_size(s_refresh, REFRESH_W, UI_BAR_H);
  lv_obj_align(s_refresh, LV_ALIGN_RIGHT_MID, -(20 + 70 + 4), 0);
  lv_obj_set_clickable(s_refresh, true);
  lv_obj_add_event_cb(s_refresh, onRefreshClicked, LV_EVENT_CLICKED, nullptr);
  s_refreshIcon = lv_label_create(s_refresh);
  lv_obj_set_style_text_font(s_refreshIcon, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(s_refreshIcon, COL_DIM, 0);
  lv_label_set_text(s_refreshIcon, LV_SYMBOL_REFRESH);
  lv_obj_center(s_refreshIcon);
  lv_obj_set_clickable(s_refreshIcon, false);

  s_spinner = lv_spinner_create(s_refresh);
  lv_spinner_set_anim_params(s_spinner, SPIN_MS, 90);
  lv_obj_set_size(s_spinner, 26, 26);
  lv_obj_center(s_spinner);
  lv_obj_set_style_arc_width(s_spinner, 3, LV_PART_MAIN);
  lv_obj_set_style_arc_width(s_spinner, 3, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(s_spinner, COL_LINE, LV_PART_MAIN);
  lv_obj_set_style_arc_color(s_spinner, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_clickable(s_spinner, false);
  lv_obj_set_hidden(s_spinner, true);
  lv_obj_set_hidden(s_refresh, true);
}

static lv_obj_t* addTile(uint8_t idx, lv_dir_t dir) {
  lv_obj_t* tile = lv_tileview_add_tile(s_tv, idx, 0, dir);
  lv_obj_set_user_data(tile, (void*)(intptr_t)idx);
  lv_obj_set_style_pad_all(tile, 0, 0);
  lv_obj_set_scrollable(tile, false);         // drags go to the tileview
  return tile;
}

static lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color, const char* text) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  lv_label_set_text(l, text);
  return l;
}

// The portal: QR code on the left, the three steps on the right.
static void createPortal() {
  s_portal = plainObj(s_overlay);
  lv_obj_set_size(s_portal, UI_W, UI_H);

  s_qr = lv_qrcode_create(s_portal);
  lv_qrcode_set_size(s_qr, QR_SIZE);
  lv_qrcode_set_dark_color(s_qr, lv_color_black());
  lv_qrcode_set_light_color(s_qr, lv_color_white());
  lv_qrcode_set_quiet_zone(s_qr, true);
  lv_obj_align(s_qr, LV_ALIGN_LEFT_MID, 60, 0);

  lv_obj_t* col = plainObj(s_portal);
  lv_obj_set_size(col, 360, LV_SIZE_CONTENT);
  lv_obj_align(col, LV_ALIGN_LEFT_MID, 410, 0);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(col, 8, 0);

  label(col, &inter_32, COL_ACCENT, "Set up WiFi");
  lv_obj_t* gap = plainObj(col);
  lv_obj_set_size(gap, 1, 10);
  label(col, &inter_24, COL_TEXT, "1. Scan the code or join");
  s_portalSsid = label(col, &inter_32, COL_TEXT, "");
  s_portalPass = label(col, &inter_20, COL_DIM, "");
  s_portalUrl = label(col, &inter_24, COL_TEXT, "");
  lv_obj_set_width(s_portalUrl, lv_pct(100));
  lv_label_set_long_mode(s_portalUrl, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_t* step3 = label(col, &inter_24, COL_TEXT, "3. Pick your network and press Connect");
  lv_obj_set_width(step3, lv_pct(100));
  lv_label_set_long_mode(step3, LV_LABEL_LONG_MODE_WRAP);
  gap = plainObj(col);
  lv_obj_set_size(gap, 1, 10);
  label(col, &inter_20, COL_WARNING, "Waiting for your network...");
}

static void createMessage() {
  s_msg = plainObj(s_overlay);
  lv_obj_set_size(s_msg, UI_W, UI_H);
  s_msgTitle = label(s_msg, &inter_32, COL_TEXT, "");
  lv_obj_align(s_msgTitle, LV_ALIGN_CENTER, 0, -24);
  s_msgDetail = label(s_msg, &inter_24, COL_ACCENT, "");
  lv_obj_align(s_msgDetail, LV_ALIGN_CENTER, 0, 24);
}

// Over everything except the dimming layer. Clickable, so it swallows touches.
static void createOverlay() {
  s_overlay = plainObj(lv_layer_top());
  lv_obj_set_size(s_overlay, UI_W, UI_H);
  lv_obj_set_style_bg_color(s_overlay, COL_BG, 0);
  lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
  lv_obj_set_clickable(s_overlay, true);
  createPortal();
  createMessage();
  lv_obj_set_hidden(s_overlay, true);
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
    ScreenDashboard_SetData(DevOpsApi_Get());
    ScreenClock_Create(clock);

    showActive(screen);
    lv_tileview_set_tile_by_index(s_tv, screen, 0, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_tv, onTileChanged, LV_EVENT_VALUE_CHANGED, nullptr);

    barClock(nullptr);
    lv_timer_create(barClock, 1000, nullptr);
    lv_timer_create(apiTimer, 500, nullptr);

    createOverlay();

    // Dimming layer above everything; clicks go through.
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

uint8_t Ui_CurrentScreen() { return s_screen; }

// Backslash in front of the characters the WIFI: QR format reserves.
static void appendEscaped(char* out, size_t cap, const char* s) {
  size_t n = strlen(out);
  for (; s && *s && n + 2 < cap; s++) {
    if (strchr("\\;,:\"", *s)) out[n++] = '\\';
    out[n++] = *s;
  }
  out[n] = '\0';
}

void Ui_ShowPortal(const char* ssid, const char* pass, const char* ip) {
  const bool open = !pass || !*pass;
  char qr[160] = "WIFI:T:";
  strlcat(qr, open ? "nopass" : "WPA", sizeof(qr));
  strlcat(qr, ";S:", sizeof(qr));
  appendEscaped(qr, sizeof(qr), ssid);
  if (!open) {
    strlcat(qr, ";P:", sizeof(qr));
    appendEscaped(qr, sizeof(qr), pass);
  }
  strlcat(qr, ";;", sizeof(qr));

  LvLock lock;
  lv_qrcode_update(s_qr, qr, strlen(qr));
  lv_label_set_text(s_portalSsid, ssid);
  if (open) lv_label_set_text(s_portalPass, "no password");
  else      lv_label_set_text_fmt(s_portalPass, "password: %s", pass);
  lv_label_set_text_fmt(s_portalUrl, "2. The setup page opens by itself, or go to http://%s", ip);
  lv_obj_set_hidden(s_msg, true);
  lv_obj_set_hidden(s_portal, false);
  lv_obj_set_hidden(s_overlay, false);
}

void Ui_ShowMessage(const char* title, const char* detail) {
  LvLock lock;
  lv_label_set_text(s_msgTitle, title ? title : "");
  lv_label_set_text(s_msgDetail, detail ? detail : "");
  lv_obj_set_hidden(s_portal, true);
  lv_obj_set_hidden(s_msg, false);
  lv_obj_set_hidden(s_overlay, false);
}

void Ui_HideOverlay() {
  LvLock lock;
  lv_obj_set_hidden(s_overlay, true);
}
