// =============================================================================
//  MeteoPlaneRadar
//  Per-source status notes. See Status.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "Status.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define STATUS_TEXT_MAX 48

static char          s_txt[ST_COUNT][STATUS_TEXT_MAX];
static unsigned long s_at[ST_COUNT] = {0};

// Written by the API fetch task and the clock (also from that task), read by
// the web server in loop(). The lock only covers copies, never the formatting.
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

void Status_Set(StatusSlot slot, const char* fmt, ...) {
  if (slot >= ST_COUNT) return;
  char tmp[STATUS_TEXT_MAX];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  const unsigned long now = millis();
  portENTER_CRITICAL(&s_mux);
  memcpy(s_txt[slot], tmp, sizeof(tmp));
  s_at[slot] = now;
  portEXIT_CRITICAL(&s_mux);
}

void Status_Text(StatusSlot slot, char* out, size_t cap) {
  if (!out || cap == 0) return;
  out[0] = '\0';
  if (slot >= ST_COUNT) return;
  char tmp[STATUS_TEXT_MAX];
  portENTER_CRITICAL(&s_mux);
  memcpy(tmp, s_txt[slot], sizeof(tmp));
  const unsigned long at = s_at[slot];
  portEXIT_CRITICAL(&s_mux);
  if (!at) { snprintf(out, cap, "-"); return; }

  unsigned long age = (millis() - at) / 1000UL;
  if (age < 90)          snprintf(out, cap, "%s (%lu s ago)",   tmp, age);
  else if (age < 5400)   snprintf(out, cap, "%s (%lu min ago)", tmp, age / 60);
  else                   snprintf(out, cap, "%s (%lu h ago)",   tmp, age / 3600);
}
