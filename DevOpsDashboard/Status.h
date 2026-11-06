// =============================================================================
//  DevOpsDashboard (based on MeteoPlaneRadar by chiptron.cz)
//  A one-line health report per data source, for the web status page.
//
//  Each source leaves a short note here, and /api/status shows them all - so
//  "the dashboard is empty" can be answered without a USB cable.
// =============================================================================
#pragma once
#include <Arduino.h>

enum StatusSlot : uint8_t {
  ST_API = 0,      // the DevOps REST API
  ST_NTP,          // the clock
  ST_COUNT
};

// printf-style; the text is truncated to something the page can show.
void Status_Set(StatusSlot slot, const char* fmt, ...);

// "OK, 3 items (4 s ago)" - the note plus how long ago it was left.
void Status_Text(StatusSlot slot, char* out, size_t cap);
