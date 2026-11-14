// =============================================================================
//  DevOpsDashboard43
//  Screen 0: the dashboard - a ring chart of the Poirot computers by state.
// =============================================================================
#pragma once
#include <lvgl.h>
#include "DashData.h"
#include "DevOpsApi.h"

// Both run in LVGL context (LVGL task or under LvPort_Lock()).
void ScreenDashboard_Create(lv_obj_t* tile);
// Shows a snapshot of the API: no data = grey empty ring and "-"; old data (the
// last download failed, or it is older than API_STALE_FACTOR intervals) = grey
// ring. Cheap to call every second - the age text needs it - and only redraws
// what changed.
void ScreenDashboard_SetData(const ApiSnapshot& s);
