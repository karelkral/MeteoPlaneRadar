// =============================================================================
//  DevOpsDashboard43
//  Config.h - compile-time settings in one place.
//
//  Everything here is a default; the API URL, refresh interval, brightness,
//  WiFi networks, last screen and admin password are stored in NVS at runtime
//  (Settings.*) and edited in the web UI.
//
//  Taken over from DevOpsDashboard (based on MeteoPlaneRadar by Petr /
//  chiptron.cz, MIT). The display, touch and expander settings of the round
//  boards are gone: ESP32_Display_Panel brings this board up from its own
//  definition (esp_panel_board_supported_conf.h).
// =============================================================================
#pragma once
#include "Version.h"   // FW_VERSION - used in the User-Agent below

#define BOARD_NAME "Waveshare ESP32-S3-Touch-LCD-4.3"

// ---------------------------------------------------------------------------
//  Factory reset button
//
//  GPIO0 (BOOT) is also the G3 data line of the RGB panel, so it can only be
//  read before the display starts. Holding BOOT while the board resets would
//  start the ROM downloader instead, so: press RESET, release it, then press
//  and hold BOOT within BOOT_WINDOW_MS, for BOOT_HOLD_MS.
// ---------------------------------------------------------------------------
#define BOOT_PIN       0
#define BOOT_WINDOW_MS 1000
#define BOOT_HOLD_MS   3000

// ---------------------------------------------------------------------------
//  Time (POSIX TZ string + NTP servers)
//
//  The clock is set by SNTP. As a fallback for networks that block UDP 123,
//  the "Date" header of every HTTP response also sets it (Clock.cpp).
// ---------------------------------------------------------------------------
#define TZ_INFO      "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"

// ---------------------------------------------------------------------------
//  Configuration access point (WiFi portal)
// ---------------------------------------------------------------------------
#define AP_SSID     "DevOpsDashboard43"
#define AP_PASSWORD ""     // "" = open network

// ---------------------------------------------------------------------------
//  DevOps REST API (stored in the settings; nothing fetches it yet)
// ---------------------------------------------------------------------------
#define API_URL_DEFAULT        ""     // empty = nothing is fetched
#define API_URL_MAX            200
#define API_INTERVAL_S_DEFAULT 30
#define API_INTERVAL_S_MIN     5
#define API_INTERVAL_S_MAX     3600

// ---------------------------------------------------------------------------
//  Brightness (software dimming, see Ui_SetBrightness)
// ---------------------------------------------------------------------------
#define BACKLIGHT_DEFAULT 80   // per cent; 0 = backlight off

// ---------------------------------------------------------------------------
//  Network
// ---------------------------------------------------------------------------
// A TLS handshake needs roughly 45 kB of internal RAM. Starting one with less
// than this free fails deep inside mbedTLS, so skip the request instead.
#define NET_MIN_HEAP 60000

// Upper bound for the TLS handshake, in seconds (the default is 120 s, far over
// the watchdog).
#define NET_TLS_HANDSHAKE_S 8

// Wall-clock ceiling for receiving one body.
#define NET_BODY_BUDGET_MS 15000UL

// Largest text body accepted by Net_GetString().
#define NET_MAX_TEXT (128 * 1024)

#define HTTP_USER_AGENT "DevOpsDashboard43/" FW_VERSION

// ---------------------------------------------------------------------------
//  Watchdogs
// ---------------------------------------------------------------------------
#define WDT_TIMEOUT_S 20       // loop() stuck this long = reboot

// loop() restarts the board when the LVGL task has not finished a pass for
// LVGL_WD_STALL_MS, or the RGB panel has not finished a frame for
// PANEL_WD_STALL_MS (black or frozen picture while the sketch still runs).
// Both pause during a firmware upload and for LVGL_WD_STALL_MS after it.
#define DISPLAY_WD        1    // 0 = do not watch the display at all
#define LVGL_WD_STALL_MS  10000UL
#define PANEL_WD_STALL_MS 3000UL

// 1 = test build: 60 s after boot loop() takes the LVGL lock and keeps it,
// which must end in a restart by the watchdog above. Never ship with 1.
#ifndef DOD43_WD_TEST
#define DOD43_WD_TEST 0
#endif

// ---------------------------------------------------------------------------
//  Web configuration server
// ---------------------------------------------------------------------------
#define WEB_PORT       80
#define WEB_HOSTNAME   "devopsdashboard43"   // -> http://devopsdashboard43.local/
#define WEB_ADMIN_USER "admin"

// ---------------------------------------------------------------------------
//  WiFi - up to three remembered networks
// ---------------------------------------------------------------------------
#define WIFI_SLOTS 3
// While the link is down, the device first retries the network it was on and
// only after this long looks for any of the other remembered ones.
#define WIFI_SWITCH_AFTER_MS 60000UL
#define WIFI_TRY_MS          12000UL    // per network at boot
