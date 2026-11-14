// =============================================================================
//  DevOpsDashboard43
//  Slices of the dashboard ring: the Poirot computers by state. The numbers come
//  from the REST API (DevOpsApi.h); the order is the order of the slices
//  clockwise from 12 o'clock and of the legend rows.
// =============================================================================
#pragma once
#include <stdint.h>

enum DashStatus : uint8_t {
  DASH_RUNNING,
  DASH_WAITING,
  DASH_WARNING,
  DASH_ERROR,
  DASH_STATUS_COUNT
};

static inline const char* DashData_Name(uint8_t s) {
  static const char* const NAMES[DASH_STATUS_COUNT] = { "Running", "Waiting", "Warning", "Error" };
  return s < DASH_STATUS_COUNT ? NAMES[s] : "?";
}
