// =============================================================================
//  DevOpsDashboard43
//  Screen 1: the clock - large time and the date under it.
// =============================================================================
#pragma once
#include <lvgl.h>

// LVGL context (LVGL task or under LvPort_Lock()). Updates itself every second.
void ScreenClock_Create(lv_obj_t* tile);
