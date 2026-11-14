// =============================================================================
//  DevOpsDashboard43 (from DevOpsDashboard, based on MeteoPlaneRadar by chiptron.cz)
//  Persisted settings - interface.
//
//  Everything is cached in RAM and stored in NVS (namespace "devops"). The web
//  UI reads and writes the same values through Settings_ToJson/FromJson.
//
//  Based on MeteoPlaneRadar by Petr / chiptron.cz (MIT, see LICENSE.txt).
// =============================================================================
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Config.h"

void Settings_Begin();

// --- WiFi credentials -------------------------------------------------------
// Up to WIFI_SLOTS networks. The slots can be filled in the web UI even when
// the network is not in range.
const char* Settings_WifiSsid(uint8_t slot = 0);
const char* Settings_WifiPass(uint8_t slot = 0);
bool        Settings_HasWifi();                 // at least one slot filled
// From the captive portal: remember this network. Updates the password if the
// SSID is already known, otherwise puts it first and pushes the rest down.
void        Settings_SetWifi(const char* ssid, const char* pass);
// From the web UI: write one slot exactly. An empty ssid clears the slot; a
// nullptr pass keeps the stored password (the page never sees it).
void        Settings_SetWifiSlot(uint8_t slot, const char* ssid, const char* pass);
void        Settings_ClearWifi();

// --- Brightness (0-100 %) ---------------------------------------------------
uint8_t Settings_Backlight();
void    Settings_SetBacklight(uint8_t pct);      // debounced write

// --- DevOps REST API --------------------------------------------------------
// Up to API_PROFILES server profiles (e.g. one per environment); the active one
// is what the dashboard fetches. The key is write-only for the outside world:
// the web page only learns whether one is stored.
struct ApiProfile {                       // read-only from outside
  char name[API_PROFILE_NAME_MAX + 1];
  char baseUrl[API_URL_MAX + 1];          // without the endpoint path
  char key[API_KEY_MAX + 1];
};

const ApiProfile& Settings_ApiProfile(uint8_t slot);
uint8_t     Settings_ApiActive();                       // 0..API_PROFILES-1
void        Settings_SetApiActive(uint8_t slot);
// key == nullptr keeps the stored key; an empty baseUrl clears the slot (key
// included). The base URL is trimmed, a trailing '/' and a pasted
// API_PATH_POIROT are removed. Returns false when refused: the URL does not
// start with http:// / https://, or a value is too long.
bool        Settings_SetApiProfile(uint8_t slot, const char* name,
                                   const char* baseUrl, const char* key);
uint16_t    Settings_ApiIntervalSec();
void        Settings_SetApiIntervalSec(uint16_t s); // clamped to API_INTERVAL_S_MIN..MAX

// Legacy single-URL access, acts on the active profile (until phase 43.10).
const char* Settings_ApiUrl();
bool        Settings_SetApiUrl(const char* url);

// --- UI state (debounced writes) --------------------------------------------
uint8_t Settings_Screen();
void    Settings_SetScreen(uint8_t idx);

// --- Admin password (protects OTA, factory reset and the password itself) ---
//
// Stored in the clear: the firmware update page authenticates with HTTP Basic,
// and the library has to be handed the actual password to check it against.
// This protects against the household, not against an attacker on the network.
bool Settings_HasAdminPassword();
void Settings_SetAdminPassword(const char* plain);   // "" or " " = protection off
bool Settings_CheckAdminPassword(const char* plain);
const char* Settings_AdminPassword();                // for HTTP Basic auth

// --- Serialisation ----------------------------------------------------------
// Everything except the passwords and the WiFi credentials.
void Settings_ToJson(JsonObject out);
// Applies whatever is present; missing keys keep their current value. Returns
// true when at least one value was present.
bool Settings_FromJson(JsonObjectConst in);

void Settings_Tick();       // call once per loop(); debounced NVS flush
void Settings_ClearAll();
