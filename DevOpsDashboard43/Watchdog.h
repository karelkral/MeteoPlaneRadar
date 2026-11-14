// =============================================================================
//  MeteoPlaneRadar
//  Hardware watchdog - interface.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
// =============================================================================
#pragma once
#include <Arduino.h>
#include "Config.h"   // WDT_TIMEOUT_S

void Watchdog_Begin();
void Watchdog_Feed();
