// =============================================================================
//  DevOpsDashboard43
//  Fetches the Poirot computers from the REST API in a FreeRTOS task of its own.
//
//  GET {baseUrl}/IotDashboard/PoirotComputers with the X-Api-Key header. The
//  TLS handshake plus the response can take ~24 s, longer than the task
//  watchdog allows loop(), so the network never runs there: loop() and the UI
//  only read a snapshot copy (DevOpsApi_Get) and never wait for the network.
//  The task never touches LVGL or Settings; it gets its configuration as a copy.
// =============================================================================
#pragma once
#include <Arduino.h>
#include "Config.h"

enum ApiState : uint8_t {
  API_OFF,          // the active profile has no URL
  API_NO_KEY,       // URL set, key missing - nothing is fetched
  API_WAITING,      // waiting for WiFi / the first download
  API_OK,
  API_ERROR         // the last attempt failed (older data stays)
};

struct ApiSnapshot {
  uint16_t total, waiting, running, warning, error;
  bool     valid;            // data arrived at least once (since the last Configure change)
  bool     busy;             // a download is running
  ApiState state;
  int      httpCode;         // last HTTP code or NET_CODE_* (0 = no answer)
  uint32_t lastOkMs;         // millis() of the last success
  uint32_t revision;         // changes with every attempt and with busy
  uint16_t intervalSec;
  char     profile[API_PROFILE_NAME_MAX + 1];
  char     errorText[48];    // short text for the display and /api/status
};

void        DevOpsApi_Begin();                 // from setup(), after Settings_Begin()

// From loop() only: pushes the active profile and the interval of the Settings
// to the task. A different URL or key discards the old data (it belongs to
// another environment) and fetches at once; a changed name or interval does not.
void        DevOpsApi_ApplySettings();
void        DevOpsApi_Configure(const char* name, const char* baseUrl,
                                const char* key, uint16_t intervalSec);

void        DevOpsApi_RefreshNow();            // from anywhere; ignored while busy
ApiSnapshot DevOpsApi_Get();                   // from anywhere; returns a copy
