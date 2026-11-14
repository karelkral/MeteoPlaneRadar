// =============================================================================
//  DevOpsDashboard43 (from DevOpsDashboard, based on MeteoPlaneRadar by chiptron.cz)
//  WiFi - connect, or put up an access point until someone tells us a network.
//
//  A plain access point plus the device's own web page (no WiFiManager), so
//  the main loop keeps running the whole time. The access point has NO
//  timeout: without a network the device has nothing to show anyway.
// =============================================================================
#pragma once
#include <Arduino.h>
#include "Config.h"   // AP_SSID / AP_PASSWORD
#define PORTAL_IP "192.168.4.1"

// Connect with the stored credentials, or bring up the access point. Blocks
// only for the initial connection attempt; everything after that is in Loop().
void WiFi_Begin();

// Connection upkeep and the handover from the portal. Call from loop().
void WiFi_Loop();

bool   WiFi_IsConnected();
bool   WiFi_IsAP();
String WiFi_SSID();
String WiFi_IP();

// Forget the network and go back to the access point.
void WiFi_Reset();

// Redraw the access-point instructions (SSID, QR code, address).
void WiFi_DrawApScreen();
