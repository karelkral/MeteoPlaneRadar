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
static uint8_t    s_apiAct = API_PROFILE_DEFAULT;

// --- Fixed API profiles (Config.h), checked at compile time ---
struct ApiFixed { const char* name; const char* url; };
static constexpr ApiFixed API_FIXED[] = { API_PROFILE_LIST };

static constexpr size_t cLen(const char* s) { return *s ? 1 + cLen(s + 1) : 0; }
static constexpr bool cHasHttps(const char* s) {
  return s[0] == 'h' && s[1] == 't' && s[2] == 't' && s[3] == 'p' && s[4] == 's' &&
         s[5] == ':' && s[6] == '/' && s[7] == '/' && s[8] != '\0';
}
static constexpr bool fixedProfilesValid() {
  for (size_t i = 0; i < API_PROFILES; i++) {
    const size_t nl = cLen(API_FIXED[i].name), ul = cLen(API_FIXED[i].url);
    if (nl < 1 || nl > API_PROFILE_NAME_MAX) return false;
    if (!cHasHttps(API_FIXED[i].url) || ul > API_URL_MAX || API_FIXED[i].url[ul - 1] == '/') return false;
  }
  return true;
}
static_assert(sizeof(API_FIXED) / sizeof(API_FIXED[0]) == API_PROFILES,
              "API_PROFILE_LIST must have API_PROFILES entries");
static_assert(fixedProfilesValid(),
              "API_PROFILE_LIST: name 1..API_PROFILE_NAME_MAX, https:// URL without a trailing '/' that fits API_URL_MAX");
static_assert(API_PROFILE_DEFAULT < API_PROFILES, "API_PROFILE_DEFAULT out of range");

static void resetProfiles() {
  for (uint8_t i = 0; i < API_PROFILES; i++) {
    s_prof[i].name    = API_FIXED[i].name;
    s_prof[i].baseUrl = API_FIXED[i].url;
    s_prof[i].key[0]  = '\0';
  }
  s_apiAct = API_PROFILE_DEFAULT;
}

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

// NVS keys: akey0..2 (API key of a profile), apiProf (active profile).
static void keyName(char* out, uint8_t slot) {
  snprintf(out, 6, "akey%u", (unsigned)slot);
}

static void storeKey(uint8_t slot) {
  char k[6];
  keyName(k, slot);
  if (!prefs.begin(NS, false)) { Serial.println("Settings: NVS open failed, API key not stored"); return; }
  prefs.putString(k, s_prof[slot].key);
  prefs.end();
}

// v0.2.0 development builds stored free profiles; they are replaced by the fixed
// ones, so the old entries (keys included) are dropped and the keys re-entered.
static void dropLegacyApiKeys() {
  static const char* const LEGACY[] = { "apiUrl", "apAct" };
  static const char kinds[] = { 'N', 'U', 'K' };
  bool found = false;
  if (!prefs.begin(NS, false)) return;
  for (const char* k : LEGACY) if (prefs.isKey(k)) { prefs.remove(k); found = true; }
  for (char kind : kinds) {
    for (uint8_t i = 0; i < API_PROFILES; i++) {
      char k[6];
      snprintf(k, sizeof(k), "ap%c%u", kind, (unsigned)i);
      if (prefs.isKey(k)) { prefs.remove(k); found = true; }
    }
  }
  prefs.end();
  if (found) Serial.println("Settings: old API profiles removed - enter the API keys again in the web UI");
}

void Settings_Begin() {
  resetProfiles();
  if (prefs.begin(NS, true)) {
    s_bri    = prefs.getUChar("bl", BACKLIGHT_DEFAULT);
    s_apiInt = prefs.getUShort("apiInt", API_INTERVAL_S_DEFAULT);
    s_scr    = prefs.getUChar("scr", UI_SCREEN_DASHBOARD);
    s_apiAct = prefs.getUChar("apiProf", API_PROFILE_DEFAULT);
    // isKey() first: getString() on a missing key logs an error line.
    if (prefs.isKey("pw"))     prefs.getString("pw", s_pw, sizeof(s_pw));
    for (int i = 0; i < WIFI_SLOTS; i++) {
      if (prefs.isKey(SSID_KEY[i])) prefs.getString(SSID_KEY[i], s_ssid[i], sizeof(s_ssid[i]));
      if (prefs.isKey(PASS_KEY[i])) prefs.getString(PASS_KEY[i], s_wpass[i], sizeof(s_wpass[i]));
    }
    for (uint8_t i = 0; i < API_PROFILES; i++) {
      char k[6];
      keyName(k, i);
      if (prefs.isKey(k)) prefs.getString(k, s_prof[i].key, sizeof(s_prof[i].key));
    }
    prefs.end();
  }
  if (s_bri > 100) s_bri = BACKLIGHT_DEFAULT;
  if (s_scr >= UI_SCREEN_COUNT) s_scr = UI_SCREEN_DASHBOARD;
  if (s_apiAct >= API_PROFILES) s_apiAct = API_PROFILE_DEFAULT;
  s_apiInt = clampInterval(s_apiInt);
  dropLegacyApiKeys();
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
  if (prefs.begin(NS, false)) { prefs.putUChar("apiProf", s_apiAct); prefs.end(); }
}

bool Settings_SetApiKey(uint8_t slot, const char* key) {
  if (slot >= API_PROFILES) return false;
  if (!key) key = "";
  // A blank pasted along would end up in the header.
  while (*key == ' ') key++;
  size_t kl = strlen(key);
  while (kl && key[kl - 1] == ' ') kl--;
  if (kl > API_KEY_MAX) return false;
  memcpy(s_prof[slot].key, key, kl);
  s_prof[slot].key[kl] = '\0';
  storeKey(slot);
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
  o["apiIntervalSec"] = s_apiInt;
  o["brightness"]     = s_bri;
  o["hasPassword"]    = Settings_HasAdminPassword();
}

bool Settings_FromJson(JsonObjectConst in) {
  bool changed = false;
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
  resetProfiles();
  s_apiInt = API_INTERVAL_S_DEFAULT;
  s_scr = UI_SCREEN_DASHBOARD;
  s_pw[0] = '\0';
  memset(s_ssid, 0, sizeof(s_ssid));
  memset(s_wpass, 0, sizeof(s_wpass));
  s_uiDirty = false;
}
