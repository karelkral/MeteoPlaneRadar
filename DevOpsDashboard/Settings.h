// =============================================================================
//  DevOpsDashboard
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
const char* Settings_ApiUrl();                   // "" = nothing is fetched
// Accepts only "http://..." / "https://..." (or "" to switch off), at most
// API_URL_MAX characters. Returns false when the value was refused.
bool        Settings_SetApiUrl(const char* url);
uint16_t    Settings_ApiIntervalSec();
void        Settings_SetApiIntervalSec(uint16_t s); // clamped to API_INTERVAL_S_MIN..MAX

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
