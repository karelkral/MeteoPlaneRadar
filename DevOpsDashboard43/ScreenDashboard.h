// =============================================================================
//  DevOpsDashboard43
//  Screen 0: the dashboard - a ring chart of pipeline results.
// =============================================================================
#pragma once
#include <lvgl.h>
#include "DashData.h"

// Both run in LVGL context (LVGL task or under LvPort_Lock()).
void ScreenDashboard_Create(lv_obj_t* tile);
void ScreenDashboard_SetData(const DashData& d);
