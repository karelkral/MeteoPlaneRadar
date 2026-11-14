// =============================================================================
//  DevOpsDashboard43
//  Data shown on the dashboard: pipeline runs in the last 24 hours by result.
//
//  TEST DATA for now. The REST API step replaces DashData_Test() with a copy
//  of the last API response (see ../Doc/DevOpsDashboard-doporuceni-LVGL.md).
// =============================================================================
#pragma once
#include <stdint.h>

enum DashStatus : uint8_t {
  DASH_SUCCEEDED,
  DASH_FAILED,
  DASH_RUNNING,
  DASH_CANCELED,
  DASH_STATUS_COUNT
};

struct DashData {
  uint16_t count[DASH_STATUS_COUNT];
  bool     valid;                  // false = nothing to show yet
};

static inline const char* DashData_Name(uint8_t s) {
  static const char* const NAMES[DASH_STATUS_COUNT] = { "Succeeded", "Failed", "Running", "Canceled" };
  return s < DASH_STATUS_COUNT ? NAMES[s] : "?";
}

// The same numbers as DevOpsDashboard's ring chart.
static inline DashData DashData_Test() {
  DashData d = { { 42, 7, 5, 3 }, true };
  return d;
}
