// =============================================================================
//  DevOpsDashboard
//  Screen 1: blank. See ScreenBlank.h.
// =============================================================================
#include "ScreenBlank.h"
#include "UI.h"
#include "Layout.h"

void ScreenBlank_Enter() {}

bool ScreenBlank_Tick() {
  return UI_StatusLineChanged();
}

void ScreenBlank_Draw() {
  gfx->fillScreen(C_BLACK);
  UI_DrawStatusLine(LY_STATUS);
}
