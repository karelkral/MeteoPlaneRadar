// =============================================================================
//  MeteoPlaneRadar
//  Shared UI helpers - colours, global gfx, interface.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once
#include <Arduino_GFX_Library.h>

// Colours (RGB565)
#define C_BLACK  0x0000
#define C_BLUE   0x001F
#define C_RED    0xF800
#define C_GREEN  0x07E0
#define C_WHITE  0xFFFF
#define C_YELLOW 0xFFE0
#define C_GRAY   0x8410
#define C_DKGRAY 0x2124
#define C_CYAN   0x05FF
#define C_ORANGE 0xFC00   // altitude band 2-6 km

// Global display (defined in the .ino).
extern Arduino_GFX* gfx;

// Draw a WiFi QR code (for joining the AP). open=true -> open network.
void UI_DrawWifiQR(const char* ssid, const char* password, bool open,
                   int x, int y, int size_px);

// Horizontally centred text (size 1-4).
void UI_TextCentered(const char* text, int cy, uint16_t color, uint8_t size);

// Text centred inside the rectangle [x, x+w) - used for labels above the map.
void UI_TextCenteredIn(const char* text, int x, int w, int cy,
                       uint16_t color, uint8_t size);

// A filled sector of an annulus - the building block of both energy dials.
// Angles in RADIANS, zero at three o'clock, increasing clockwise (screen y
// grows downwards), a0 <= a1. Pass a0 == a1 for a single radial line.
//
// Radial lines rather than a scanline fill: the GFX build here has no arc
// primitive, and the colour changes from one sector to the next, so a
// span-based fill would need a separate pass per sector anyway. The step is
// fine enough that the outer edge has no gaps at the radii these screens use.
void UI_FillRing(int cx, int cy, int rIn, int rOut, float a0, float a1,
                 uint16_t color);

// The clock line, centred under the screen dots. Draws nothing while the time
// is unknown, and refuses to draw text that would not fit inside the circle.
void UI_DrawStatusLine(int cy);

// Has that line changed since the last time anyone asked? Screens call this
// from their Tick() so the minute keeps moving even when nothing else changes.
// One shared record is enough: only the active screen is ticked, and switching
// screens redraws everything anyway.
bool UI_StatusLineChanged();
