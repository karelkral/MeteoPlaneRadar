// =============================================================================
//  DevOpsDashboard43
//  Screen geometry and colours shared by the UI modules.
// =============================================================================
#pragma once
#include <lvgl.h>

// --- Geometry (800x480 landscape) --------------------------------------------
#define UI_W          800
#define UI_H          480
#define UI_BAR_H      48             // status bar at the top
#define UI_TILE_H     (UI_H - UI_BAR_H)

// --- Colours -----------------------------------------------------------------
#define COL_BG        lv_color_black()
#define COL_TEXT      lv_color_white()
#define COL_DIM       lv_color_hex(0x9E9E9E)   // secondary text
#define COL_LINE      lv_color_hex(0x303030)   // separators, empty ring
#define COL_ACCENT    lv_color_hex(0x26C6DA)   // titles
#define COL_OK        lv_color_hex(0x4CAF50)   // succeeded
#define COL_FAIL      lv_color_hex(0xF44336)   // failed
#define COL_RUN       lv_color_hex(0xFFC107)   // running
#define COL_CANCEL    lv_color_hex(0x9E9E9E)   // canceled
