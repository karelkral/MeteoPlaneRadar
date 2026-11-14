// =============================================================================
//  DevOpsDashboard43
//  DevOps dashboard for the Waveshare ESP32-S3-Touch-LCD-4.3 (800x480) on LVGL 9.
//
//  Experimental successor of DevOpsDashboard (round 2.1" / 2.8C boards), which
//  is based on MeteoPlaneRadar by Petr / chiptron.cz (MIT, see LICENSE.txt).
//  Plan and progress: ../Doc/DevOpsDashboard43-plan.md
//
//  Screens (swipe left / right to switch):
//    0) Dashboard - ring chart of pipeline results (test data for now)
//    1) Clock     - large time and date
//
//  Configuration lives in a browser: an access point with a QR code until a
//  WiFi network is entered, then http://devopsdashboard43.local/ on the home
//  network. Firmware updates are at /update on the same server.
//
//  Threads: LVGL draws in its own task (LvPort). loop() runs WiFi, the web
//  server, the clock and the settings and never draws - it changes the UI only
//  through the Ui_* functions, which take the LVGL lock.
//
//  Controls:
//    - swipe left / right                       = next / previous screen
//    - RESET, then hold BOOT within 1 s for 3 s = factory reset
//
//  Board:   Waveshare ESP32-S3-Touch-LCD-4.3 (SKU 25948): ST7262 RGB panel,
//           GT911 touch, CH422G expander, ESP32-S3 N16R8.
//  Serial:  115200 Bd on the connector marked "USB TO UART".
//  Build:   build.cmd [COMx] - see sketch.yaml for the pinned versions.
// =============================================================================

#include <Arduino.h>
#include "esp_heap_caps.h"
#include "esp_system.h"

#include "Config.h"
#include "Version.h"
#include "LvPort.h"
#include "Ui.h"
#include "Settings.h"
#include "WiFiPortal.h"
#include "WebConfig.h"
#include "Clock.h"
#include "Watchdog.h"

#define HEAP_LOG_PERIOD_MS 600000UL

static const char* resetReasonText() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "power on";
    case ESP_RST_EXT:       return "external reset (button)";
    case ESP_RST_SW:        return "software restart (e.g. after OTA)";
    case ESP_RST_PANIC:     return "PANIC - exception";
    case ESP_RST_INT_WDT:   return "WATCHDOG (interrupt)";
    case ESP_RST_TASK_WDT:  return "WATCHDOG (stuck loop)";
    case ESP_RST_WDT:       return "WATCHDOG (other)";
    case ESP_RST_DEEPSLEEP: return "wake from deep sleep";
    case ESP_RST_BROWNOUT:  return "BROWNOUT - supply voltage dropped";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "unknown";
  }
}

// GPIO0 (BOOT) is also the panel's G3 data line, so it is read here, before
// the display starts, and never again. The screen is still dark: the log on
// the serial port is the only feedback.
static void checkBootReset() {
  pinMode(BOOT_PIN, INPUT_PULLUP);
  const unsigned long start = millis();
  while (digitalRead(BOOT_PIN) != LOW) {
    if (millis() - start >= BOOT_WINDOW_MS) return;
    delay(10);
  }
  Serial.println("BOOT pressed - keep holding for a factory reset");
  const unsigned long held = millis();
  while (digitalRead(BOOT_PIN) == LOW) {
    if (millis() - held >= BOOT_HOLD_MS) {
      Serial.println("Factory reset: settings erased, release BOOT");
      Settings_ClearAll();
      while (digitalRead(BOOT_PIN) == LOW) delay(10);   // not into the ROM downloader
      delay(200);
      ESP.restart();
    }
    delay(10);
  }
  Serial.println("BOOT released early - no reset");
}

// No Serial.flush() here: with no host reading the port it could block, and
// the watchdog would fire first.
static void restartIfAsked() {
  if (!WebConfig_WantsRestart()) return;
  Serial.println("Restart requested");
  delay(400);                       // let the HTTP reply leave the socket
  LvPort_Restart();
}

// A swipe changed the screen (store it), or the web page asked for one (show
// it and store it). Settings are only ever touched from here, in loop().
static void handleScreenChanges() {
  int scr = Ui_TakeScreenChange();
  if (scr >= 0) {
    Settings_SetScreen((uint8_t)scr);
    Serial.printf("Screen: %d\n", scr);
  }

  scr = WebConfig_TakeScreen();
  const int step = WebConfig_TakeScreenStep();
  if (scr < 0 && step) scr = (Ui_CurrentScreen() + step + UI_SCREEN_COUNT) % UI_SCREEN_COUNT;
  if (scr >= 0 && scr != Ui_CurrentScreen()) {
    Ui_ShowScreen((uint8_t)scr, true);
    Settings_SetScreen((uint8_t)scr);
    Serial.printf("Screen: %d (web)\n", scr);
  }
}

static void logHeap() {
  static unsigned long last = 0;
  if (last && millis() - last < HEAP_LOG_PERIOD_MS) return;
  last = millis();
  Serial.printf("Heap: internal %u free (largest %u), PSRAM %u free\n",
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\n=== DevOpsDashboard43 v%s ===\n", FW_VERSION);
  Serial.printf("Reset reason: %s\n", resetReasonText());

  checkBootReset();
  Settings_Begin();

  if (!LvPort_Begin()) {
    Serial.println("Display init failed - restarting in 10 s");
    delay(10000);
    ESP.restart();
  }
  Ui_Begin(Settings_Screen(), Settings_Backlight());

  // Connects with the stored credentials ("Connecting to WiFi..." on the
  // display), or brings up the access point with the QR code. Either way the
  // web server is running when this returns.
  WiFi_Begin();

  // After WiFi_Begin(): the SNTP client needs the network stack to exist.
  Clock_Begin();

  Watchdog_Begin();
  Serial.println("Setup done");
}

void loop() {
  // A firmware upload owns the machine; WebConfig holds the LVGL lock meanwhile.
  if (WebConfig_UpdateBusy()) {
    WebConfig_Loop();
    delay(1);
    return;
  }

  WiFi_Loop();                // may accept credentials and leave the access point
  WebConfig_Loop();
  restartIfAsked();
  handleScreenChanges();

  Clock_Tick();
  Settings_Tick();            // debounced writes to NVS
  logHeap();
  Watchdog_Feed();
  delay(5);
}
