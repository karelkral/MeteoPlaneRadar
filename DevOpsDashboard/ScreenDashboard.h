// =============================================================================
//  DevOpsDashboard
//  Screen 0: the dashboard.
// =============================================================================
#pragma once
#include <Arduino.h>

void ScreenDashboard_Enter();
void ScreenDashboard_Draw();
bool ScreenDashboard_Tick();               // true = redraw
bool ScreenDashboard_HandleTap(int x, int y);
