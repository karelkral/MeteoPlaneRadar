// =============================================================================
//  DevOpsDashboard43
//  Screen 1: the clock. See ScreenClock.h.
// =============================================================================
#include "ScreenClock.h"
#include "Theme.h"
#include "LocalTime.h"
#include "src/fonts/Fonts.h"

static lv_obj_t* s_time;
static lv_obj_t* s_date;
static long      s_shown = -2;      // minute shown; -1 = "--:--"

static const char* const DAYS[7] = {
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};
static const char* const MONTHS[12] = {
  "January", "February", "March", "April", "May", "June",
  "July", "August", "September", "October", "November", "December"
};

// Runs every second but touches the labels only when the minute changes -
// redrawing the 160 px digits costs more than comparing two numbers.
static void update(lv_timer_t*) {
  struct tm t;
  const bool known = LocalTime_Get(t);
  const long key = known ? ((long)t.tm_yday * 24 + t.tm_hour) * 60 + t.tm_min : -1;
  if (key == s_shown) return;
  s_shown = key;

  if (!known) {
    lv_label_set_text(s_time, "--:--");
    lv_label_set_text(s_date, "Waiting for the time...");
    return;
  }
  lv_label_set_text_fmt(s_time, "%02d:%02d", t.tm_hour, t.tm_min);
  lv_label_set_text_fmt(s_date, "%s, %d %s %d", DAYS[t.tm_wday], t.tm_mday,
                        MONTHS[t.tm_mon], t.tm_year + 1900);
}

void ScreenClock_Create(lv_obj_t* tile) {
  s_time = lv_label_create(tile);
  lv_obj_set_style_text_font(s_time, &inter_sb_160, 0);
  lv_obj_set_style_text_color(s_time, COL_TEXT, 0);
  lv_obj_align(s_time, LV_ALIGN_CENTER, 0, -36);

  s_date = lv_label_create(tile);
  lv_obj_set_style_text_font(s_date, &inter_32, 0);
  lv_obj_set_style_text_color(s_date, COL_DIM, 0);
  lv_obj_align(s_date, LV_ALIGN_CENTER, 0, 84);

  update(nullptr);
  lv_timer_create(update, 1000, nullptr);
}
