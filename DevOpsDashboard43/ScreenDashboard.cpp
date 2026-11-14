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

static const lv_color_t* sliceColors() {
  static lv_color_t c[DASH_STATUS_COUNT];
  c[DASH_SUCCEEDED] = COL_OK;
  c[DASH_FAILED]    = COL_FAIL;
  c[DASH_RUNNING]   = COL_RUN;
  c[DASH_CANCELED]  = COL_CANCEL;
  return c;
}

static lv_obj_t* s_ring;                        // square container of the arcs
static lv_obj_t* s_empty;                       // grey ring while there is no data
static lv_obj_t* s_arc[DASH_STATUS_COUNT];
static lv_obj_t* s_pct[DASH_STATUS_COUNT];
static lv_obj_t* s_total;
static lv_obj_t* s_caption;
static lv_obj_t* s_count[DASH_STATUS_COUNT];

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
  const lv_color_t* colors = sliceColors();

  s_ring = lv_obj_create(tile);
  lv_obj_remove_style_all(s_ring);
  lv_obj_set_size(s_ring, 2 * RING_OUT, 2 * RING_OUT);
  lv_obj_set_pos(s_ring, RING_CX - RING_OUT, RING_CY - RING_OUT);
  lv_obj_set_scrollable(s_ring, false);
  lv_obj_set_clickable(s_ring, false);

  s_empty = makeArc(s_ring, COL_LINE);
  lv_arc_set_angles(s_empty, 0, 360);
  for (int i = 0; i < DASH_STATUS_COUNT; i++) s_arc[i] = makeArc(s_ring, colors[i]);
  for (int i = 0; i < DASH_STATUS_COUNT; i++) s_pct[i] = makeLabel(s_ring, &inter_sb_24, COL_BG);

  s_total = makeLabel(s_ring, &inter_sb_72, COL_TEXT);
  lv_obj_align(s_total, LV_ALIGN_CENTER, 0, -14);
  s_caption = makeLabel(s_ring, &inter_20, COL_DIM);
  lv_label_set_text(s_caption, "pipelines / 24h");
  lv_obj_align(s_caption, LV_ALIGN_CENTER, 0, 38);

  const int y0 = RING_CY - (DASH_STATUS_COUNT - 1) * LEGEND_DY / 2;
  for (int i = 0; i < DASH_STATUS_COUNT; i++) {
    const int y = y0 + i * LEGEND_DY;

    lv_obj_t* dot = lv_obj_create(tile);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 18, 18);
    lv_obj_set_pos(dot, LEGEND_DOT - 9, y - 9);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, colors[i], 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);

    lv_obj_t* name = makeLabel(tile, &inter_32, COL_TEXT);
    lv_label_set_text(name, DashData_Name(i));
    lv_obj_align(name, LV_ALIGN_LEFT_MID, LEGEND_TEXT, y - UI_TILE_H / 2);

    s_count[i] = makeLabel(tile, &inter_32, COL_TEXT);
    lv_obj_align(s_count[i], LV_ALIGN_RIGHT_MID, -LEGEND_R, y - UI_TILE_H / 2);
  }
}

void ScreenDashboard_SetData(const DashData& d) {
  uint32_t sum = 0;
  if (d.valid) for (int i = 0; i < DASH_STATUS_COUNT; i++) sum += d.count[i];

  lv_obj_set_hidden(s_empty, sum != 0);
  lv_label_set_text(s_total, sum ? "" : "-");
  if (sum) lv_label_set_text_fmt(s_total, "%lu", (unsigned long)sum);

  // Slices: whole degrees (LV_USE_FLOAT is off). The running angle is rounded,
  // not each sweep, so the slices always add up to the full circle.
  const int rMid = (RING_IN + RING_OUT) / 2;
  uint32_t acc = 0;
  for (int i = 0; i < DASH_STATUS_COUNT; i++) {
    const uint16_t v = d.valid ? d.count[i] : 0;
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
    if (d.valid) lv_label_set_text_fmt(s_count[i], "%u", (unsigned)d.count[i]);
    else         lv_label_set_text(s_count[i], "-");
  }
}
