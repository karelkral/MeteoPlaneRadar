// =============================================================================
//  DevOpsDashboard43
//  Local time for the UI, or "not known yet".
// =============================================================================
#pragma once
#include <time.h>

// Earliest time taken as set: 2025-01-01. Until NTP (or the HTTP Date header)
// sets the clock, the ESP32 counts from 1970.
#define LOCALTIME_MIN_EPOCH 1735689600L

// Thread-safe (localtime_r); callable from the LVGL task.
static inline bool LocalTime_Get(struct tm& out) {
  const time_t now = time(nullptr);
  if (now < LOCALTIME_MIN_EPOCH) return false;
  localtime_r(&now, &out);
  return true;
}
