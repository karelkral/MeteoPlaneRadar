// =============================================================================
//  DevOpsDashboard
//  Persisted settings - storage in NVS + JSON serialisation for the web UI.
//
//  Based on MeteoPlaneRadar by Petr / chiptron.cz (MIT, see LICENSE.txt).
// =============================================================================
#include "Settings.h"
#include <Preferences.h>
#include <string.h>

static Preferences prefs;
static const char* NS = "devops";

// --- WiFi ---
static char s_ssid[WIFI_SLOTS][33]  = {};
static char s_wpass[WIFI_SLOTS][65] = {};
static const char* const SSID_KEY[WIFI_SLOTS] = { "ssid", "ssid1", "ssid2" };
static const char* const PASS_KEY[WIFI_SLOTS] = { "wpass", "wpass1", "wpass2" };

// --- Display / API ---
static uint8_t  s_bri = BACKLIGHT_DEFAULT;
static char     s_apiUrl[API_URL_MAX + 1] = API_URL_DEFAULT;
static uint16_t s_apiInt = API_INTERVAL_S_DEFAULT;

// --- UI state ---
static uint8_t  s_scr = SCREEN_DASHBOARD_I;

// --- Admin password (see the note in Settings.h) ---
static char s_pw[33] = "";

static bool          s_uiDirty   = false;
static unsigned long s_uiDirtyAt = 0;
static void markDirty() { s_uiDirty = true; s_uiDirtyAt = millis(); }

static void putU16(const char* k, uint16_t v) {
  if (prefs.begin(NS, false)) { prefs.putUShort(k, v); prefs.end(); }
}
static void putStr(const char* k, const char* v) {
  if (prefs.begin(NS, false)) { prefs.putString(k, v); prefs.end(); }
}

static void copyStr(char* dst, size_t cap, const char* src) {
  strncpy(dst, src ? src : "", cap - 1);
  dst[cap - 1] = '\0';
}

static uint16_t clampInterval(uint16_t s) {
  if (s < API_INTERVAL_S_MIN) return API_INTERVAL_S_MIN;
  if (s > API_INTERVAL_S_MAX) return API_INTERVAL_S_MAX;
  return s;
}

void Settings_Begin() {
  if (prefs.begin(NS, true)) {
    s_bri    = prefs.getUChar("bl", BACKLIGHT_DEFAULT);
    s_apiInt = prefs.getUShort("apiInt", API_INTERVAL_S_DEFAULT);
    s_scr    = prefs.getUChar("scr", SCREEN_DASHBOARD_I);
    // isKey() first: getString() on a missing key logs an error line.
    if (prefs.isKey("apiUrl")) prefs.getString("apiUrl", s_apiUrl, sizeof(s_apiUrl));
    if (prefs.isKey("pw"))     prefs.getString("pw", s_pw, sizeof(s_pw));
    for (int i = 0; i < WIFI_SLOTS; i++) {
      if (prefs.isKey(SSID_KEY[i])) prefs.getString(SSID_KEY[i], s_ssid[i], sizeof(s_ssid[i]));
      if (prefs.isKey(PASS_KEY[i])) prefs.getString(PASS_KEY[i], s_wpass[i], sizeof(s_wpass[i]));
    }
    prefs.end();
  }
  if (s_bri > 100) s_bri = BACKLIGHT_DEFAULT;
  if (s_scr >= SCREEN_N) s_scr = SCREEN_DASHBOARD_I;
  s_apiInt = clampInterval(s_apiInt);
}

// --- WiFi -------------------------------------------------------------------
const char* Settings_WifiSsid(uint8_t slot) { return slot < WIFI_SLOTS ? s_ssid[slot]  : ""; }
const char* Settings_WifiPass(uint8_t slot) { return slot < WIFI_SLOTS ? s_wpass[slot] : ""; }
bool Settings_HasWifi() {
  for (int i = 0; i < WIFI_SLOTS; i++) if (s_ssid[i][0]) return true;
  return false;
}

static void storeWifiSlots() {
  if (!prefs.begin(NS, false)) return;
  for (int i = 0; i < WIFI_SLOTS; i++) {
    prefs.putString(SSID_KEY[i], s_ssid[i]);
    prefs.putString(PASS_KEY[i], s_wpass[i]);
  }
  prefs.end();
}

void Settings_SetWifiSlot(uint8_t slot, const char* ssid, const char* pass) {
  if (slot >= WIFI_SLOTS) return;
  if (!ssid || !*ssid) {
    s_ssid[slot][0] = '\0';
    s_wpass[slot][0] = '\0';
  } else {
    // A renamed slot with no password typed keeps nothing: the old password
    // belonged to a different network.
    const bool sameNet = strcmp(s_ssid[slot], ssid) == 0;
    copyStr(s_ssid[slot], sizeof(s_ssid[slot]), ssid);
    if (pass)          copyStr(s_wpass[slot], sizeof(s_wpass[slot]), pass);
    else if (!sameNet) s_wpass[slot][0] = '\0';
  }
  storeWifiSlots();
}

void Settings_SetWifi(const char* ssid, const char* pass) {
  if (!ssid || !*ssid) return;
  for (int i = 0; i < WIFI_SLOTS; i++) {
    if (strcmp(s_ssid[i], ssid) == 0) {
      copyStr(s_wpass[i], sizeof(s_wpass[i]), pass);
      storeWifiSlots();
      return;
    }
  }
  // New: first place, the others move down and the last one falls off.
  for (int i = WIFI_SLOTS - 1; i > 0; i--) {
    memcpy(s_ssid[i],  s_ssid[i - 1],  sizeof(s_ssid[i]));
    memcpy(s_wpass[i], s_wpass[i - 1], sizeof(s_wpass[i]));
  }
  copyStr(s_ssid[0],  sizeof(s_ssid[0]),  ssid);
  copyStr(s_wpass[0], sizeof(s_wpass[0]), pass);
  storeWifiSlots();
}

void Settings_ClearWifi() {
  memset(s_ssid, 0, sizeof(s_ssid));
  memset(s_wpass, 0, sizeof(s_wpass));
  storeWifiSlots();
}

// --- Brightness -------------------------------------------------------------
uint8_t Settings_Backlight() { return s_bri; }

// A slider sends many values in a row - they go through the debounce so the
// flash is written once per adjustment, not once per sample.
void Settings_SetBacklight(uint8_t pct) {
  if (pct > 100) pct = 100;
  if (pct == s_bri) return;
  s_bri = pct;
  markDirty();
}

// --- DevOps REST API --------------------------------------------------------
const char* Settings_ApiUrl() { return s_apiUrl; }

bool Settings_SetApiUrl(const char* url) {
  if (!url) url = "";
  while (*url == ' ') url++;
  const size_t n = strlen(url);
  if (n > API_URL_MAX) return false;
  if (n && strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) return false;
  copyStr(s_apiUrl, sizeof(s_apiUrl), url);
  // Trailing blanks pasted from somewhere else would end up in the request.
  for (int i = (int)strlen(s_apiUrl) - 1; i >= 0 && s_apiUrl[i] == ' '; i--) s_apiUrl[i] = '\0';
  putStr("apiUrl", s_apiUrl);
  return true;
}

uint16_t Settings_ApiIntervalSec() { return s_apiInt; }
void     Settings_SetApiIntervalSec(uint16_t s) {
  s_apiInt = clampInterval(s);
  putU16("apiInt", s_apiInt);
}

// --- UI state ---------------------------------------------------------------
uint8_t Settings_Screen() { return s_scr; }
void    Settings_SetScreen(uint8_t idx) {
  if (idx >= SCREEN_N || idx == s_scr) return;
  s_scr = idx;
  markDirty();
}

// --- Admin password ---------------------------------------------------------
bool Settings_HasAdminPassword() { return s_pw[0] != '\0'; }
const char* Settings_AdminPassword() { return s_pw; }

void Settings_SetAdminPassword(const char* plain) {
  if (!plain) plain = "";
  // A single space is the agreed way to say "remove the password" from the web
  // form, where an empty field has to mean "leave it alone".
  if (strcmp(plain, " ") == 0) plain = "";
  copyStr(s_pw, sizeof(s_pw), plain);
  putStr("pw", s_pw);
}

bool Settings_CheckAdminPassword(const char* plain) {
  if (!Settings_HasAdminPassword()) return true;    // protection disabled
  if (!plain) return false;
  // Constant time over the full buffer.
  uint8_t diff = 0;
  const size_t len = strlen(plain);
  for (size_t i = 0; i < sizeof(s_pw); i++) {
    char a = (i < len) ? plain[i] : '\0';
    diff |= (uint8_t)(a ^ s_pw[i]);
  }
  return diff == 0 && len < sizeof(s_pw);
}

// --- Serialisation ----------------------------------------------------------
void Settings_ToJson(JsonObject o) {
  o["apiUrl"]         = s_apiUrl;
  o["apiIntervalSec"] = s_apiInt;
  o["brightness"]     = s_bri;
  o["hasPassword"]    = Settings_HasAdminPassword();
}

bool Settings_FromJson(JsonObjectConst in) {
  bool changed = false;
  if (!in["apiUrl"].isNull()) {
    Settings_SetApiUrl(in["apiUrl"].as<const char*>());
    changed = true;
  }
  if (!in["apiIntervalSec"].isNull()) {
    Settings_SetApiIntervalSec(in["apiIntervalSec"].as<uint16_t>());
    changed = true;
  }
  if (!in["brightness"].isNull()) {
    Settings_SetBacklight(in["brightness"].as<uint8_t>());
    changed = true;
  }
  return changed;
}

// --- Debounced flush --------------------------------------------------------
void Settings_Tick() {
  if (!s_uiDirty) return;
  if (millis() - s_uiDirtyAt < 2000) return;
  if (prefs.begin(NS, false)) {
    prefs.putUChar("scr", s_scr);
    prefs.putUChar("bl",  s_bri);
    prefs.end();
  }
  s_uiDirty = false;
}

void Settings_ClearAll() {
  if (prefs.begin(NS, false)) { prefs.clear(); prefs.end(); }
  s_bri = BACKLIGHT_DEFAULT;
  copyStr(s_apiUrl, sizeof(s_apiUrl), API_URL_DEFAULT);
  s_apiInt = API_INTERVAL_S_DEFAULT;
  s_scr = SCREEN_DASHBOARD_I;
  s_pw[0] = '\0';
  memset(s_ssid, 0, sizeof(s_ssid));
  memset(s_wpass, 0, sizeof(s_wpass));
  s_uiDirty = false;
}
