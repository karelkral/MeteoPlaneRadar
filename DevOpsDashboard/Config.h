// =============================================================================
//  DevOpsDashboard
//  Config.h - ALL compile-time settings in one place.
//
//  Everything here is a default; the API URL, refresh interval, brightness,
//  WiFi networks, last screen and admin password are stored in NVS at runtime
//  (Settings.*) and edited in the web UI.
//
//  Based on MeteoPlaneRadar by Petr / chiptron.cz (MIT, see LICENSE.txt).
//  Boards: Waveshare ESP32-S3-Touch-LCD-2.1 and ESP32-S3-Touch-LCD-2.8C
//          (both round 480x480, ST7701; told apart at boot - see BOARD_FORCE)
// =============================================================================
#pragma once
#include "Version.h"   // FW_VERSION - used in the User-Agent below

// ---------------------------------------------------------------------------
//  Board pins / bus
// ---------------------------------------------------------------------------
#define I2C_SDA   15
#define I2C_SCL   7
#define BOOT_PIN  0        // hold at power-up (~3 s) = factory reset

// ---------------------------------------------------------------------------
//  Which board?
//
//  The 2.1 and the 2.8C differ only in the ST7701 register sequence, the
//  vertical timing and the touch controller, so one binary detects which it is
//  at boot (CST820 at 0x15 = 2.1, GT911 at 0x5D = 2.8C). See Board.h.
//
//  0  = detect at boot (leave it here)
//  21 = always the 2.1
//  28 = always the 2.8C
// ---------------------------------------------------------------------------
#define BOARD_FORCE 0

// Which board to assume when nothing answers on I2C.
#define BOARD_FALLBACK BOARD_LCD_2_1

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
#define AP_SSID     "DevOpsDashboard"
#define AP_PASSWORD ""     // "" = open network

// ---------------------------------------------------------------------------
//  Screens
// ---------------------------------------------------------------------------
#define SCREEN_DASHBOARD_I 0
#define SCREEN_BLANK_I     1
#define SCREEN_N           2

// ---------------------------------------------------------------------------
//  DevOps REST API
// ---------------------------------------------------------------------------
#define API_URL_DEFAULT        ""     // empty = nothing is fetched
#define API_URL_MAX            200
#define API_INTERVAL_S_DEFAULT 30
#define API_INTERVAL_S_MIN     5
#define API_INTERVAL_S_MAX     3600

// ---------------------------------------------------------------------------
//  Brightness
// ---------------------------------------------------------------------------
#define BACKLIGHT_DEFAULT 80   // per cent

// ---------------------------------------------------------------------------
//  Touch
// ---------------------------------------------------------------------------
// Both controllers drop the odd sample in the middle of a drag. Ending the
// gesture on the first empty sample would turn one swipe into several bogus
// taps - so require this much continuous silence before accepting that the
// finger is really up.
#define TOUCH_RELEASE_MS 60

// 1 = only read the controller when its INT line says it has something to
// report (watched by an interrupt), with a slow fallback poll. Set to 0 to go
// back to unconditional polling if the touch ever feels unresponsive.
#define TOUCH_USE_INT 1

// Fallback poll while idle - only a safety net for a missed interrupt.
#define TOUCH_IDLE_POLL_MS 250

// How often to read the I/O expander back and repair it if it does not match
// what we wrote (TCA9554_Verify). Cheap - one I2C register read.
#define EXPANDER_CHECK_MS 5000UL

// ---------------------------------------------------------------------------
//  Display watchdog
//
//  The RGB peripheral can stop scanning (black screen, backlight still on)
//  while loop() keeps running. The VSYNC interrupt counts frames; if that count
//  stops moving, the panel is repaired and, failing that, the board restarts.
// ---------------------------------------------------------------------------
#define DISPLAY_WD 1              // 0 = do not watch the panel at all
#define DISPLAY_WD_DEAD_MS 3000UL
#define DISPLAY_WD_REBOOT_MS 8000UL

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

#define HTTP_USER_AGENT "DevOpsDashboard/" FW_VERSION

// ---------------------------------------------------------------------------
//  Diagnostics (serial log at 115200 Bd over the connector marked "USB")
// ---------------------------------------------------------------------------
#define TOUCH_DEBUG 0     // 1 = log touch gestures
#define FLUSH_DEBUG 0     // 1 = log how long a full-screen flush takes
#define LAYOUT_DEBUG 0    // 1 = check the fixed layout bands at boot

// ---------------------------------------------------------------------------
//  Watchdog
// ---------------------------------------------------------------------------
#define WDT_TIMEOUT_S 20       // reboot after this many seconds of being stuck

// ---------------------------------------------------------------------------
//  Web configuration server
// ---------------------------------------------------------------------------
#define WEB_PORT       80
#define WEB_HOSTNAME   "devopsdashboard"   // -> http://devopsdashboard.local/
#define WEB_ADMIN_USER "admin"

// ---------------------------------------------------------------------------
//  WiFi - up to three remembered networks
// ---------------------------------------------------------------------------
#define WIFI_SLOTS 3
// While the link is down, the device first retries the network it was on and
// only after this long looks for any of the other remembered ones.
#define WIFI_SWITCH_AFTER_MS 60000UL
#define WIFI_TRY_MS          12000UL    // per network at boot
