// =============================================================================
//  DevOpsDashboard
//  Screen 0: the dashboard. See ScreenDashboard.h.
// =============================================================================
#include "ScreenDashboard.h"
#include "UI.h"
#include "Layout.h"
#include "Display_ST7701.h"

void ScreenDashboard_Enter() {}

bool ScreenDashboard_Tick() {
  return UI_StatusLineChanged();       // keep the clock at the top moving
}

void ScreenDashboard_Draw() {
  gfx->fillScreen(C_BLACK);
  UI_DrawStatusLine(LY_STATUS);
  UI_TextCentered("DevOps", LCD_HEIGHT / 2 - 40, C_CYAN, 5);
  UI_TextCentered("dashboard", LCD_HEIGHT / 2 + 10, C_WHITE, 4);
}

bool ScreenDashboard_HandleTap(int, int) { return false; }
