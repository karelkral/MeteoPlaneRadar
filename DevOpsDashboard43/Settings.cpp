// =============================================================================
//  DevOpsDashboard43 (from DevOpsDashboard, based on MeteoPlaneRadar by chiptron.cz)
//  Persisted settings - storage in NVS + JSON serialisation for the web UI.
//
//  Based on MeteoPlaneRadar by Petr / chiptron.cz (MIT, see LICENSE.txt).
// =============================================================================
#include "Settings.h"
#include "Ui.h"     // UI_SCREEN_*
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
static uint16_t s_apiInt = API_INTERVAL_S_DEFAULT;
static ApiProfile s_prof[API_PROFILES] = {};
static uint8_t    s_apiAct = 0;

// --- UI state ---
static uint8_t  s_scr = UI_SCREEN_DASHBOARD;

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

// NVS keys of an API profile: apN0..2 (name), apU0..2 (base URL), apK0..2 (key).
static void profKey(char* out, char kind, uint8_t slot) {
  snprintf(out, 6, "ap%c%u", kind, (unsigned)slot);
}

// Trims blanks at both ends and drops trailing '/', then the endpoint path when
// the whole URL was pasted. Returns false when the result does not fit.
static bool normalizeBaseUrl(const char* in, char* out, size_t cap) {
  if (!in) in = "";
  while (*in == ' ') in++;
  size_t n = strlen(in);
  while (n && (in[n - 1] == ' ' || in[n - 1] == '/')) n--;
  const size_t pl = strlen(API_PATH_POIROT);
  if (n >= pl && strncasecmp(in + n - pl, API_PATH_POIROT, pl) == 0) {
    n -= pl;
    while (n && (in[n - 1] == ' ' || in[n - 1] == '/')) n--;
  }
  if (n >= cap) return false;
  memcpy(out, in, n);
  out[n] = '\0';
  return true;
}

static void storeProfile(uint8_t slot) {
  char k[6];
  if (!prefs.begin(NS, false)) { Serial.println("Settings: NVS open failed, API profile not stored"); return; }
  profKey(k, 'N', slot); prefs.putString(k, s_prof[slot].name);
  profKey(k, 'U', slot); prefs.putString(k, s_prof[slot].baseUrl);
  profKey(k, 'K', slot); prefs.putString(k, s_prof[slot].key);
  prefs.end();
}

void Settings_Begin() {
  char legacyUrl[API_URL_MAX + 1] = "";
  if (prefs.begin(NS, true)) {
    s_bri    = prefs.getUChar("bl", BACKLIGHT_DEFAULT);
    s_apiInt = prefs.getUShort("apiInt", API_INTERVAL_S_DEFAULT);
    s_scr    = prefs.getUChar("scr", UI_SCREEN_DASHBOARD);
    s_apiAct = prefs.getUChar("apAct", 0);
    // isKey() first: getString() on a missing key logs an error line.
    if (prefs.isKey("apiUrl")) prefs.getString("apiUrl", legacyUrl, sizeof(legacyUrl));
    if (prefs.isKey("pw"))     prefs.getString("pw", s_pw, sizeof(s_pw));
    for (int i = 0; i < WIFI_SLOTS; i++) {
      if (prefs.isKey(SSID_KEY[i])) prefs.getString(SSID_KEY[i], s_ssid[i], sizeof(s_ssid[i]));
      if (prefs.isKey(PASS_KEY[i])) prefs.getString(PASS_KEY[i], s_wpass[i], sizeof(s_wpass[i]));
    }
    for (uint8_t i = 0; i < API_PROFILES; i++) {
      char k[6];
      profKey(k, 'N', i); if (prefs.isKey(k)) prefs.getString(k, s_prof[i].name,    sizeof(s_prof[i].name));
      profKey(k, 'U', i); if (prefs.isKey(k)) prefs.getString(k, s_prof[i].baseUrl, sizeof(s_prof[i].baseUrl));
      profKey(k, 'K', i); if (prefs.isKey(k)) prefs.getString(k, s_prof[i].key,     sizeof(s_prof[i].key));
    }
    prefs.end();
  }
  if (s_bri > 100) s_bri = BACKLIGHT_DEFAULT;
  if (s_scr >= UI_SCREEN_COUNT) s_scr = UI_SCREEN_DASHBOARD;
  if (s_apiAct >= API_PROFILES) s_apiAct = 0;
  s_apiInt = clampInterval(s_apiInt);

  // Migration: the single URL of v0.1.0 becomes profile 0.
  if (legacyUrl[0]) {
    bool anyProfile = false;
    for (uint8_t i = 0; i < API_PROFILES; i++) anyProfile |= s_prof[i].baseUrl[0] != '\0';
    if (!anyProfile && normalizeBaseUrl(legacyUrl, s_prof[0].baseUrl, sizeof(s_prof[0].baseUrl))
        && s_prof[0].baseUrl[0]) {
      copyStr(s_prof[0].name, sizeof(s_prof[0].name), "Default");
      storeProfile(0);
      Serial.println("Settings: apiUrl moved to API profile 1");
    }
    if (prefs.begin(NS, false)) { prefs.remove("apiUrl"); prefs.end(); }
  }
}

// --- WiFi -------------------------------------------------------------------
const char* Settings_WifiSsid(uint8_t slot) { return slot < WIFI_SLOTS ? s_ssid[slot]  : ""; }
const char* Settings_WifiPass(uint8_t slot) { return slot < WIFI_SLOTS ? s_wpass[slot] : ""; }
bool Settings_HasWifi() {
  for (int i = 0; i < WIFI_SLOTS; i++) if (s_ssid[i][0]) return true;
  return false;
}

static void storeWifiSlots() {
  if (!prefs.begin(NS, false)) {
    Serial.println("Settings: NVS open failed, WiFi not stored");
    return;
  }
  size_t written = 0;
  for (int i = 0; i < WIFI_SLOTS; i++) {
    written += prefs.putString(SSID_KEY[i], s_ssid[i]);
    written += prefs.putString(PASS_KEY[i], s_wpass[i]);
  }
  prefs.end();
  Serial.printf("Settings: WiFi stored (%u B)\n", (unsigned)written);
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
const ApiProfile& Settings_ApiProfile(uint8_t slot) {
  return s_prof[slot < API_PROFILES ? slot : 0];
}

uint8_t Settings_ApiActive() { return s_apiAct; }

void Settings_SetApiActive(uint8_t slot) {
  if (slot >= API_PROFILES || slot == s_apiAct) return;
  s_apiAct = slot;
  if (prefs.begin(NS, false)) { prefs.putUChar("apAct", s_apiAct); prefs.end(); }
}

bool Settings_SetApiProfile(uint8_t slot, const char* name, const char* baseUrl, const char* key) {
  if (slot >= API_PROFILES) return false;

  char url[API_URL_MAX + 1];
  if (!normalizeBaseUrl(baseUrl, url, sizeof(url))) return false;
  if (!url[0]) {                                   // empty URL clears the slot, key included
    memset(&s_prof[slot], 0, sizeof(ApiProfile));
    storeProfile(slot);
    return true;
  }
  if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) return false;

  ApiProfile p = s_prof[slot];
  copyStr(p.baseUrl, sizeof(p.baseUrl), url);

  // Names and keys are trimmed: a blank pasted along would end up in the header.
  const char* nm = name ? name : "";
  while (*nm == ' ') nm++;
  size_t nl = strlen(nm);
  while (nl && nm[nl - 1] == ' ') nl--;
  if (nl > API_PROFILE_NAME_MAX) return false;
  if (nl) { memcpy(p.name, nm, nl); p.name[nl] = '\0'; }
  else if (!p.name[0]) snprintf(p.name, sizeof(p.name), "Profile %u", (unsigned)slot + 1);

  if (key) {
    while (*key == ' ') key++;
    size_t kl = strlen(key);
    while (kl && key[kl - 1] == ' ') kl--;
    if (kl > API_KEY_MAX) return false;
    memcpy(p.key, key, kl);
    p.key[kl] = '\0';
  }
  s_prof[slot] = p;
  storeProfile(slot);
  return true;
}

// Legacy single-URL access (v0.1.0 web page): the active profile's base URL.
// Goes away with the profile editor of the web page (phase 43.10).
const char* Settings_ApiUrl() { return s_prof[s_apiAct].baseUrl; }

bool Settings_SetApiUrl(const char* url) {
  const ApiProfile& p = s_prof[s_apiAct];
  return Settings_SetApiProfile(s_apiAct, p.name, url, nullptr);
}
uint16_t Settings_ApiIntervalSec() { return s_apiInt; }
void     Settings_SetApiIntervalSec(uint16_t s) {
  s_apiInt = clampInterval(s);
  putU16("apiInt", s_apiInt);
}

// --- UI state ---------------------------------------------------------------
uint8_t Settings_Screen() { return s_scr; }
void    Settings_SetScreen(uint8_t idx) {
  if (idx >= UI_SCREEN_COUNT || idx == s_scr) return;
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
  JsonArray profiles = o["apiProfiles"].to<JsonArray>();
  for (uint8_t i = 0; i < API_PROFILES; i++) {
    JsonObject e = profiles.add<JsonObject>();
    e["name"]    = s_prof[i].name;
    e["baseUrl"] = s_prof[i].baseUrl;
    e["hasKey"]  = s_prof[i].key[0] != '\0';      // the key itself never leaves the device
  }
  o["apiActive"]      = s_apiAct;
  o["apiUrl"]         = s_prof[s_apiAct].baseUrl;  // legacy, see Settings_ApiUrl()
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
  memset(s_prof, 0, sizeof(s_prof));
  s_apiAct = 0;
  s_apiInt = API_INTERVAL_S_DEFAULT;
  s_scr = UI_SCREEN_DASHBOARD;
  s_pw[0] = '\0';
  memset(s_ssid, 0, sizeof(s_ssid));
  memset(s_wpass, 0, sizeof(s_wpass));
  s_uiDirty = false;
}
