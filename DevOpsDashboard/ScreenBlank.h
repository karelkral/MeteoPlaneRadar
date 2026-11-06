// =============================================================================
//  DevOpsDashboard
//  Screen 1: blank - only the clock at the top.
// =============================================================================
#pragma once
#include <Arduino.h>

void ScreenBlank_Enter();
void ScreenBlank_Draw();
bool ScreenBlank_Tick();                   // true = redraw
