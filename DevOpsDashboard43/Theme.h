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
#define COL_RUNNING   lv_color_hex(0x4CAF50)   // green
#define COL_WAITING   lv_color_hex(0x42A5F5)   // blue
#define COL_WARNING   lv_color_hex(0xD29922)   // amber
#define COL_ERROR     lv_color_hex(0xFF1A1A)   // bright red
#define COL_STALE     lv_color_hex(0x757575)   // old or missing data
