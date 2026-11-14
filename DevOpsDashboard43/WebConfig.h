// =============================================================================
//  DevOpsDashboard43 (from DevOpsDashboard, based on MeteoPlaneRadar by chiptron.cz)
//  The configuration web server - interface.
//
//  One HTTP server serves two roles:
//
//    ACCESS POINT (no WiFi credentials yet). The device puts up an open network
//    and a captive portal. It stays up until a network is actually entered.
//
//    CONNECTED. The same server runs permanently on the home network and serves
//    the settings page at http://devopsdashboard43.local/ (or the IP).
//
//  Firmware updates happen here too, at /update.
//
//  Based on MeteoPlaneRadar by Petr / chiptron.cz (MIT, see LICENSE.txt).
// =============================================================================
#pragma once
#include <Arduino.h>

// Start (or restart) the server for the current WiFi role.
void WebConfig_Begin(bool apMode);

// Pump the server and the captive-portal DNS. Call from loop().
void WebConfig_Loop();

// Blocking WiFi scan into the list /api/scan serves. Call before the access
// point goes up - scanning while it runs cuts the phone off.
void WebConfig_ScanNow();

// True while a firmware upload is running - the main loop stops drawing so the
// flash write gets the bus to itself.
bool WebConfig_UpdateBusy();

// Credentials arrived from the portal and a connection attempt is wanted.
bool WebConfig_WantsWifiConnect();
void WebConfig_ClearWifiConnect();

// A reboot or factory reset was asked for; loop() restarts once the reply is out.
bool WebConfig_WantsRestart();

// --- Remote control ---------------------------------------------------------
// Requests are QUEUED here and carried out by loop(), for the same reason a
// touch is: switching screens from inside a request handler would re-enter the
// drawing code while the canvas is mid-frame.
//
// Each call returns the pending request and clears it.
int WebConfig_TakeScreen();       // screen index, or -1 when nothing is pending
int WebConfig_TakeScreenStep();   // -1 / +1, or 0
