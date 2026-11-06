// =============================================================================
//  DevOpsDashboard
//  Screen 0: the dashboard. See ScreenDashboard.h.
//
//  A ring chart of pipeline results in the style of MeteoPlaneRadar's
//  generation-mix screen: slices start at the top and run clockwise, with a
//  thin black gap between them. The total sits in the middle, the share of each
//  slice is printed on it, and a 2x2 legend underneath carries the counts.
//
//  The values are TEST DATA for now - phase 6 replaces them with the REST API.
// =============================================================================
#include "ScreenDashboard.h"
#include "UI.h"
#include "Layout.h"
#include "Display_ST7701.h"
#include <math.h>

// --- Geometry (480x480 round panel) ------------------------------------------
#define RING_CX   240
#define RING_CY   228
#define RING_OUT  122
#define RING_IN    84
#define TITLE_Y    58
#define LEGEND_Y0 372      // first legend row (top of size-2 text)
#define LEGEND_DY  28
#define LEGEND_XL  92      // left column: dot centre
#define LEGEND_XR 262      // right column: dot centre

#define DEG2RAD 0.0174532925f

struct Slice {
  const char* name;
  uint16_t    value;
  uint16_t    color;
};

// Test data - pipeline runs in the last 24 hours.
static const Slice SLICES[] = {
  { "Succeeded", 42, C_GREEN  },
  { "Failed",     7, C_RED    },
  { "Running",    5, C_YELLOW },
  { "Canceled",   3, C_GRAY   },
};
static const int SLICE_N = sizeof(SLICES) / sizeof(SLICES[0]);

static uint32_t total() {
  uint32_t t = 0;
  for (int i = 0; i < SLICE_N; i++) t += SLICES[i].value;
  return t;
}

// Text centred on a point (cy = vertical centre of the text).
static void textAt(const char* s, int cx, int cy, uint16_t color, uint8_t size) {
  const int w = Layout_TextW(s, size);
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(cx - w / 2, cy - 4 * size);
  gfx->print(s);
}

static void drawRing(uint32_t sum) {
  if (sum == 0) {
    UI_FillRing(RING_CX, RING_CY, RING_IN, RING_OUT, 0, 2 * PI, C_DKGRAY);
    return;
  }

  float a = -90.0f * DEG2RAD;                       // start at the top
  for (int i = 0; i < SLICE_N; i++) {
    if (!SLICES[i].value) continue;
    const float sweep = (float)SLICES[i].value / sum * 2.0f * PI;
    // The black separator can never eat more than a third of a small slice.
    float gap = 0.8f * DEG2RAD;
    if (gap > sweep / 3.0f) gap = sweep / 3.0f;
    const float a0 = a + gap * 0.5f;
    const float a1 = a + sweep - gap * 0.5f;
    if (a1 > a0) UI_FillRing(RING_CX, RING_CY, RING_IN, RING_OUT, a0, a1, SLICES[i].color);
    a += sweep;
  }

  // Percentages in a second pass, on top of the slices. Only where the slice
  // is wide enough to hold the text.
  a = -90.0f * DEG2RAD;
  const int rMid = (RING_IN + RING_OUT) / 2;
  for (int i = 0; i < SLICE_N; i++) {
    if (!SLICES[i].value) continue;
    const float sweep = (float)SLICES[i].value / sum * 2.0f * PI;
    const float mid = a + sweep * 0.5f;
    a += sweep;

    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", (unsigned)lroundf(100.0f * SLICES[i].value / sum));
    const int arc = (int)(sweep * rMid);           // length of the slice along its middle
    if (arc < Layout_TextW(pct, 2) + 10) continue;
    textAt(pct, RING_CX + (int)(rMid * cosf(mid)), RING_CY + (int)(rMid * sinf(mid)),
           C_BLACK, 2);
  }
}

static void drawLegend() {
  for (int i = 0; i < SLICE_N && i < 4; i++) {
    const int x = (i % 2) ? LEGEND_XR : LEGEND_XL;
    const int y = LEGEND_Y0 + (i / 2) * LEGEND_DY;
    gfx->fillCircle(x, y + 7, 6, SLICES[i].color);

    char line[24];
    snprintf(line, sizeof(line), "%s %u", SLICES[i].name, (unsigned)SLICES[i].value);
    gfx->setTextSize(2);
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(x + 12, y);
    gfx->print(line);
  }
}

void ScreenDashboard_Enter() {}

bool ScreenDashboard_Tick() {
  return UI_StatusLineChanged();       // keep the clock at the top moving
}

void ScreenDashboard_Draw() {
  gfx->fillScreen(C_BLACK);
  UI_DrawStatusLine(LY_STATUS);
  UI_TextCentered("DevOps dashboard", TITLE_Y, C_CYAN, 2);

  const uint32_t sum = total();
  drawRing(sum);

  char buf[12];
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)sum);
  textAt(buf, RING_CX, RING_CY - 10, C_WHITE, 5);
  textAt("pipelines / 24h", RING_CX, RING_CY + 30, C_GRAY, 1);

  drawLegend();
}

bool ScreenDashboard_HandleTap(int, int) { return false; }
