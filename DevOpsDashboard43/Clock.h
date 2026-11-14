// =============================================================================
//  DevOpsDashboard43 (from DevOpsDashboard, based on MeteoPlaneRadar by chiptron.cz)
//  Clock - local time from SNTP, with the HTTP "Date" header as a fallback.
//
//  SNTP (UDP 123) is the primary source. Some networks block it, so every HTTP
//  response that passes through Net.cpp also hands its "Date" header here; it
//  is used only until SNTP has synchronised at least once.
// =============================================================================
#pragma once
#include <Arduino.h>

// Apply TZ_INFO and start SNTP. Call once after WiFi_Begin() - the SNTP client
// needs the network stack to exist.
void Clock_Begin();

// Feed the raw value of an HTTP "Date" header, e.g.
// "Sun, 09 Aug 2026 20:00:56 GMT". Anything unparseable is ignored.
void Clock_NoteHttpDate(const char* date);

bool Clock_TimeValid();

// True once SNTP has delivered the time at least once.
bool Clock_NtpSynced();

// Logs the first valid time. Call from loop().
void Clock_Tick();

// "HH:MM" in local time, or an empty string while the time is unknown.
#define CLOCK_TEXT_MAX 8
void Clock_StatusText(char* buf, size_t cap);
