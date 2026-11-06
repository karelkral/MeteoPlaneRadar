// =============================================================================
//  DevOpsDashboard (based on MeteoPlaneRadar by chiptron.cz)
//  Anti-aliased bitmap fonts.
//
//  The glyphs are rendered from real outlines (Inter Display Medium, SIL OFL
//  1.1) with 16 levels of coverage per pixel, and blended into whatever is
//  already in the framebuffer - so the edges are smooth on any background.
//
//  The tables live in ClockFonts.h. Text is UTF-8; a character the font does
//  not have is skipped rather than drawn as a box.
// =============================================================================
#pragma once
#include <Arduino.h>

struct AAGlyph {
  uint32_t off;      // into the 4-bit bitmap, in bytes
  uint16_t cp;       // Unicode code point
  uint8_t  w, h;     // bitmap size
  int8_t   dx;       // from the pen position to the left edge of the bitmap
  int8_t   dy;       // from the baseline to the TOP of the bitmap (negative = up)
  uint8_t  adv;      // pen advance
};

struct AAFont {
  const uint8_t* bits;     // 2 pixels per byte, high nibble first, row-major
  const AAGlyph* glyphs;   // sorted by code point
  uint16_t count;
  uint8_t  ascent;         // height of the digits above the baseline
};

// Width of a UTF-8 string in pixels.
int  AAFont_TextW(const AAFont* f, const char* utf8);

// Draw with the baseline at y. Returns the pen position after the last glyph.
int  AAFont_Draw(const AAFont* f, const char* utf8, int x, int baselineY, uint16_t color);

// Centred horizontally on cx.
void AAFont_DrawCentered(const AAFont* f, const char* utf8, int cx, int baselineY, uint16_t color);

// Three large sizes of the typeface, for big numbers (L ~98 px digits,
// M ~29 px, S ~20 px). Drawn with AAFont_Draw / AAFont_DrawCentered.
struct AAFontSet {
  const AAFont* big;
  const AAFont* mid;
  const AAFont* small;
};
const AAFontSet* AAFont_ClockSet();

// --- Every print() ----------------------------------------------------------
//
// Canvas16 sends every print() through AAText_Print(), which draws the string
// in the smooth face that stands in for the built-in font's text size (1..5).
//
// The layout was built for the 6x8 px built-in cell, and the smooth text is
// made to respect it: capitals are as tall as the built-in ones, the baseline
// sits where the built-in one does, and a string is never WIDER than the
// built-in font would have drawn it - one that would be is squeezed to fit.
// Layout_TextW() returns the real (usually narrower) width, so centred text
// stays centred.
bool AAText_Enabled();                 // always true here

// Width print() will produce for this string at this text size.
int  AAText_Width(const uint8_t* s, size_t n, uint8_t size);
// Draws at the print() cursor (top-left of the 6x8 cell) and returns the
// width drawn. bg != fg fills the cell background like the built-in font does.
int  AAText_Print(const uint8_t* s, size_t n, int x, int y, uint8_t size,
                  uint16_t fg, uint16_t bg);
