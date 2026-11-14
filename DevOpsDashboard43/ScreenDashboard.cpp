// =============================================================================
//  DevOpsDashboard43
//  Screen 0: the dashboard. See ScreenDashboard.h.
//
//  The ring chart of DevOpsDashboard redone in LVGL: one lv_arc per slice,
//  starting at the top and running clockwise, a thin black gap between the
//  slices. The total sits in the middle, the share of each slice is printed on
//  it where it fits, and the legend on the right carries the counts.
// =============================================================================
#include "ScreenDashboard.h"
#include "Theme.h"
#include "src/fonts/Fonts.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// --- Geometry (tile = 800 x 432, below the status bar) -----------------------
#define RING_CX     230
#define RING_CY     (UI_TILE_H / 2)
#define RING_OUT    175
#define RING_IN     115
#define RING_GAP    1              // degrees of black between two slices
#define LEGEND_DOT  470            // x of the legend dots
#define LEGEND_TEXT 500            // x of the names
#define LEGEND_R    30             // counts right-aligned this far from the edge
#define LEGEND_DY   64             // row pitch

static lv_color_t sliceColor(int i) {
  switch (i) {
    case DASH_RUNNING: return COL_RUNNING;
    case DASH_WAITING: return COL_WAITING;
    case DASH_WARNING: return COL_WARNING;
    default:           return COL_ERROR;
  }
}

// Old data keeps a faint hint of its colour, so the slices stay tellable apart.
static lv_color_t shownColor(int i, bool stale) {
  return stale ? lv_color_mix(sliceColor(i), COL_STALE, 40) : sliceColor(i);
}

static lv_obj_t* s_ring;                        // square container of the arcs
static lv_obj_t* s_empty;                       // grey ring while there is no data
static lv_obj_t* s_arc[DASH_STATUS_COUNT];
static lv_obj_t* s_pct[DASH_STATUS_COUNT];
static lv_obj_t* s_dot[DASH_STATUS_COUNT];
static lv_obj_t* s_total;
static lv_obj_t* s_caption;
static lv_obj_t* s_count[DASH_STATUS_COUNT];
static lv_obj_t* s_status;                      // one line under the legend

static lv_obj_t* makeArc(lv_obj_t* parent, lv_color_t color) {
  lv_obj_t* a = lv_arc_create(parent);
  lv_obj_set_size(a, lv_pct(100), lv_pct(100));
  lv_obj_center(a);
  lv_arc_set_rotation(a, 270);                  // 0 degrees = 12 o'clock
  lv_arc_set_bg_angles(a, 0, 360);
  lv_obj_remove_style(a, nullptr, LV_PART_KNOB);
  lv_obj_set_style_pad_all(a, 0, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_width(a, RING_OUT - RING_IN, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(a, false, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(a, color, LV_PART_INDICATOR);
  lv_obj_set_clickable(a, false);
  lv_arc_set_angles(a, 0, 0);
  return a;
}

static lv_obj_t* makeLabel(lv_obj_t* parent, const lv_font_t* font, lv_color_t color) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  lv_label_set_text(l, "");
  return l;
}

void ScreenDashboard_Create(lv_obj_t* tile) {
  s_ring = lv_obj_create(tile);
  lv_obj_remove_style_all(s_ring);
  lv_obj_set_size(s_ring, 2 * RING_OUT, 2 * RING_OUT);
  lv_obj_set_pos(s_ring, RING_CX - RING_OUT, RING_CY - RING_OUT);
  lv_obj_set_scrollable(s_ring, false);
  lv_obj_set_clickable(s_ring, false);

  s_empty = makeArc(s_ring, COL_LINE);
  lv_arc_set_angles(s_empty, 0, 360);
  for (int i = 0; i < DASH_STATUS_COUNT; i++) s_arc[i] = makeArc(s_ring, sliceColor(i));
  for (int i = 0; i < DASH_STATUS_COUNT; i++) s_pct[i] = makeLabel(s_ring, &inter_sb_24, COL_BG);

  s_total = makeLabel(s_ring, &inter_sb_72, COL_TEXT);
  lv_obj_align(s_total, LV_ALIGN_CENTER, 0, -14);
  s_caption = makeLabel(s_ring, &inter_20, COL_DIM);
  lv_label_set_text(s_caption, "Poirot computers");
  lv_obj_align(s_caption, LV_ALIGN_CENTER, 0, 38);

  const int y0 = RING_CY - (DASH_STATUS_COUNT - 1) * LEGEND_DY / 2;
  for (int i = 0; i < DASH_STATUS_COUNT; i++) {
    const int y = y0 + i * LEGEND_DY;

    s_dot[i] = lv_obj_create(tile);
    lv_obj_remove_style_all(s_dot[i]);
    lv_obj_set_size(s_dot[i], 18, 18);
    lv_obj_set_pos(s_dot[i], LEGEND_DOT - 9, y - 9);
    lv_obj_set_style_radius(s_dot[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_dot[i], sliceColor(i), 0);
    lv_obj_set_style_bg_opa(s_dot[i], LV_OPA_COVER, 0);

    lv_obj_t* name = makeLabel(tile, &inter_32, COL_TEXT);
    lv_label_set_text(name, DashData_Name(i));
    lv_obj_align(name, LV_ALIGN_LEFT_MID, LEGEND_TEXT, y - UI_TILE_H / 2);

    s_count[i] = makeLabel(tile, &inter_32, COL_TEXT);
    lv_obj_align(s_count[i], LV_ALIGN_RIGHT_MID, -LEGEND_R, y - UI_TILE_H / 2);
  }

  s_status = makeLabel(tile, &inter_20, COL_DIM);
  lv_obj_set_width(s_status, UI_W - (LEGEND_DOT - 9) - LEGEND_R);
  lv_label_set_long_mode(s_status, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_align(s_status, LV_ALIGN_TOP_LEFT, LEGEND_DOT - 9, y0 + DASH_STATUS_COUNT * LEGEND_DY - 20);
}

// "12 s", "3 min", "2 h"
static void formatAge(char* out, size_t cap, uint32_t sec) {
  if (sec < 90)        snprintf(out, cap, "%lu s", (unsigned long)sec);
  else if (sec < 5400) snprintf(out, cap, "%lu min", (unsigned long)(sec / 60));
  else                 snprintf(out, cap, "%lu h", (unsigned long)(sec / 3600));
}

static void statusLine(const ApiSnapshot& s, uint32_t ageS, char* out, size_t cap) {
  char age[16];
  formatAge(age, sizeof(age), ageS);
  switch (s.state) {
    case API_OFF:     snprintf(out, cap, "API not configured"); break;
    case API_NO_KEY:  snprintf(out, cap, "API key missing"); break;
    case API_WAITING: snprintf(out, cap, "%s", s.errorText[0] ? s.errorText : "waiting for data..."); break;
    case API_OK:      snprintf(out, cap, "updated %s ago", age); break;
    default:
      if (s.valid) snprintf(out, cap, "%s \xC2\xB7 data %s old", s.errorText, age);
      else         snprintf(out, cap, "%s", s.errorText);
      break;
  }
}

void ScreenDashboard_SetData(const ApiSnapshot& s) {
  const uint32_t ageS = s.valid ? (millis() - s.lastOkMs) / 1000UL : 0;
  const bool stale = s.valid &&
      (s.state == API_ERROR || ageS > (uint32_t)API_STALE_FACTOR * s.intervalSec);

  // The status line changes every second, the ring only with the data.
  char line[64];
  statusLine(s, ageS, line, sizeof(line));
  if (strcmp(lv_label_get_text(s_status), line) != 0) lv_label_set_text(s_status, line);

  const uint16_t cnt[DASH_STATUS_COUNT] = { s.running, s.waiting, s.warning, s.error };
  uint32_t key = (s.valid ? 1u : 0u) | (stale ? 2u : 0u) | ((uint32_t)s.total << 8);
  for (int i = 0; i < DASH_STATUS_COUNT; i++) key = key * 31u + cnt[i];
  static uint32_t lastKey = 0;
  static bool     haveKey = false;
  if (haveKey && key == lastKey) return;
  haveKey = true;
  lastKey = key;

  uint32_t sum = 0;
  if (s.valid) for (int i = 0; i < DASH_STATUS_COUNT; i++) sum += cnt[i];

  lv_obj_set_hidden(s_empty, sum != 0);
  if (s.valid) lv_label_set_text_fmt(s_total, "%u", (unsigned)s.total);
  else         lv_label_set_text(s_total, "-");
  lv_obj_set_style_text_color(s_total, s.valid && !stale ? COL_TEXT : COL_DIM, 0);

  // Slices: whole degrees (LV_USE_FLOAT is off). The running angle is rounded,
  // not each sweep, so the slices always add up to the full circle.
  const int rMid = (RING_IN + RING_OUT) / 2;
  uint32_t acc = 0;
  for (int i = 0; i < DASH_STATUS_COUNT; i++) {
    const lv_color_t col = shownColor(i, stale);
    lv_obj_set_style_arc_color(s_arc[i], col, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_dot[i], col, 0);

    const uint16_t v = s.valid ? cnt[i] : 0;
    const int a0 = sum ? (int)lroundf(360.0f * acc / sum) : 0;
    acc += v;
    const int a1 = sum ? (int)lroundf(360.0f * acc / sum) : 0;

    // The gap never eats more than a third of a small slice.
    int gap = RING_GAP;
    if (gap * 3 > a1 - a0) gap = 0;
    const bool show = v && a1 - a0 > gap;
    lv_obj_set_hidden(s_arc[i], !show);
    if (show) lv_arc_set_angles(s_arc[i], a0 + gap, a1);

    lv_obj_set_hidden(s_pct[i], true);
    if (!show) continue;
    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", (unsigned)lroundf(100.0f * v / sum));
    lv_point_t size;
    lv_text_get_size(&size, pct, &inter_sb_24, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const float sweep = (a1 - a0) * (float)M_PI / 180.0f;
    if (sweep * rMid < size.x + 10) continue;    // too narrow for the text
    const float mid = ((a0 + a1) / 2.0f - 90.0f) * (float)M_PI / 180.0f;
    lv_label_set_text(s_pct[i], pct);
    lv_obj_align(s_pct[i], LV_ALIGN_CENTER, (int)lroundf(rMid * cosf(mid)), (int)lroundf(rMid * sinf(mid)));
    lv_obj_set_hidden(s_pct[i], false);
  }

  for (int i = 0; i < DASH_STATUS_COUNT; i++) {
    if (s.valid) lv_label_set_text_fmt(s_count[i], "%u", (unsigned)cnt[i]);
    else         lv_label_set_text(s_count[i], "-");
    lv_obj_set_style_text_color(s_count[i], stale || !s.valid ? COL_DIM : COL_TEXT, 0);
  }
}
