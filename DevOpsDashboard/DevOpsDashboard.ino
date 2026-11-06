// =============================================================================
//  DevOpsDashboard
//  A two-screen DevOps dashboard on a round 480x480 touchscreen.
//
//  Based on MeteoPlaneRadar by Petr / chiptron.cz (MIT, see LICENSE.txt).
//
//  Boards:  Waveshare ESP32-S3-Touch-LCD-2.1  (ST7701 + CST820)
//           Waveshare ESP32-S3-Touch-LCD-2.8C (ST7701 + GT911)
//           One binary serves both - the board is detected at boot (Board.h).
//
//  Screens (swipe left / right to switch):
//    0) Dashboard - data from the configured REST API
//    1) Blank     - only the clock
//
//  Configuration lives in a browser: an access point with a QR code until a
//  WiFi network is entered, then http://devopsdashboard.local/ on the home
//  network. Firmware updates are at /update on the same server.
//
//  Controls:
//    - swipe left / right           = next / previous screen
//    - hold BOOT at startup (~3 s)  = factory reset
//
//  Serial log: 115200 Bd over the connector marked "USB".
//
//  Libraries (ESP32 core 3.x): GFX Library for Arduino, ArduinoJson 7.
//  QRCode is bundled (qrcode.c). Everything else comes with the core.
// =============================================================================

#include "AAFont.h"
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <time.h>
#include "esp_heap_caps.h"
#include "esp_system.h"

#include "TCA9554.h"
#include "Board.h"
#include "Display_ST7701.h"
#include "Canvas16.h"
#include "Touch.h"
#include "Settings.h"
#include "Version.h"
#include "Config.h"
#include "Layout.h"
#include "UI.h"
#include "Net.h"
#include "WiFiPortal.h"
#include "WebConfig.h"
#include "Clock.h"
#include "Watchdog.h"
#include "ScreenDashboard.h"
#include "ScreenBlank.h"

// gfx = single off-screen canvas in PSRAM. Everything is drawn here, then
// flush() pushes the whole frame to the panel in one shot -> no flicker.
Arduino_GFX* gfx = nullptr;

// =============================================================================
//  Touch: capture and dispatch, deliberately separated
//
//  CAPTURE (touchPump) reads the controller and advances the gesture. It draws
//  nothing and makes no network call, so it is safe to run from inside a
//  download (netPoll). DISPATCH (dispatchTouch) acts on it, and only from
//  loop() - redrawing from inside a download would re-enter the drawing code.
// =============================================================================
enum PendKind : uint8_t { PEND_NONE = 0, PEND_SWIPE, PEND_LONG, PEND_TAP };
static PendKind s_pendKind = PEND_NONE;
static int      s_pendA = 0, s_pendB = 0;

static bool          s_touching = false;
static int           s_startX = 0, s_startY = 0;
static int           s_lastX = 0, s_lastY = 0;
static unsigned long s_startMs = 0;
static unsigned long s_lastSeenMs = 0;
static unsigned long s_lastPump = 0;

// The gesture does NOT end on the first empty sample: both controllers skip one
// now and then, so a release is only accepted after TOUCH_RELEASE_MS of silence.
static void touchPump() {
  unsigned long now = millis();
  if (now - s_lastPump < 5) return;
  s_lastPump = now;

  TouchData t;
  Touch_Read(&t);

  if (t.points > 0) {
    if (!s_touching) {
      s_touching = true;
      s_startX = t.x; s_startY = t.y; s_startMs = now;
    }
    s_lastX = t.x; s_lastY = t.y;
    s_lastSeenMs = now;
    return;
  }

  if (!s_touching || now - s_lastSeenMs < TOUCH_RELEASE_MS) return;

  s_touching = false;
  const int dx = s_lastX - s_startX;
  const int dy = s_lastY - s_startY;
  const unsigned long dur = s_lastSeenMs - s_startMs;
  const bool smallMove = (abs(dx) < 60 && abs(dy) < 60);

#if TOUCH_DEBUG
  Serial.printf("TOUCH: start=(%d,%d) end=(%d,%d) dx=%d dy=%d %lums\n",
                s_startX, s_startY, s_lastX, s_lastY, dx, dy, (unsigned long)dur);
#endif

  if (abs(dx) >= 70 && abs(dy) <= 90 && dur <= 700) {
    s_pendKind = PEND_SWIPE;
    s_pendA = (dx < 0) ? +1 : -1;          // finger to the left = next screen
  } else if (smallMove && dur >= 500) {
    s_pendKind = PEND_LONG;
    s_pendA = s_lastX;
  } else if (smallMove) {
    s_pendKind = PEND_TAP;
    s_pendA = s_lastX; s_pendB = s_lastY;
  }
}

// yield() during long network transfers, feed the watchdog and keep sampling
// the touch, so a gesture made during a download is not lost.
static void netPoll() { yield(); Watchdog_Feed(); touchPump(); }

static void checkBootReset() {
  pinMode(BOOT_PIN, INPUT_PULLUP);
  if (digitalRead(BOOT_PIN) != LOW) return;
  gfx->fillScreen(C_BLACK);
  UI_TextCentered("Hold for factory reset...", LCD_HEIGHT / 2, C_WHITE, 2);
  gfx->flush();
  unsigned long start = millis();
  while (digitalRead(BOOT_PIN) == LOW) {
    if (millis() - start >= 3000) {
      UI_TextCentered("Erasing settings", LCD_HEIGHT / 2 + 30, C_RED, 2);
      gfx->flush();
      Settings_ClearAll();
      delay(800);
      ESP.restart();
    }
    delay(20);
  }
}

// =============================================================================
//  Screen manager
// =============================================================================
static int s_screen = SCREEN_DASHBOARD_I;

// What is on the display right now - for the web status page.
int App_CurrentScreen() { return s_screen; }

static void drawScreenDots() {
  const int gap = 20;
  const int cx = LCD_WIDTH / 2;
  const int y = LY_DOTS;
  const int startX = cx - (SCREEN_N - 1) * gap / 2;
  for (int i = 0; i < SCREEN_N; i++) {
    int x = startX + i * gap;
    if (i == s_screen) gfx->fillCircle(x, y, 4, C_WHITE);
    else               gfx->drawCircle(x, y, 4, C_GRAY);
  }
}

static void drawActive() {
  switch (s_screen) {
    case SCREEN_DASHBOARD_I: ScreenDashboard_Draw(); break;
    case SCREEN_BLANK_I:     ScreenBlank_Draw();     break;
  }
  drawScreenDots();
  gfx->flush();    // hand the framebuffer over; Canvas16 switches to the other
}

static void enterActive() {
  switch (s_screen) {
    case SCREEN_DASHBOARD_I: ScreenDashboard_Enter(); break;
    case SCREEN_BLANK_I:     ScreenBlank_Enter();     break;
  }
  drawActive();
}

static bool activeTick() {
  switch (s_screen) {
    case SCREEN_DASHBOARD_I: return ScreenDashboard_Tick();
    case SCREEN_BLANK_I:     return ScreenBlank_Tick();
  }
  return false;
}

static bool activeTap(int x, int y) {
  switch (s_screen) {
    case SCREEN_DASHBOARD_I: return ScreenDashboard_HandleTap(x, y);
    default: return false;
  }
}

static void gotoScreen(int idx) {
  if (idx < 0 || idx >= SCREEN_N || idx == s_screen) return;
  s_screen = idx;
  Settings_SetScreen((uint8_t)s_screen);
  Serial.printf("Screen: %d\n", s_screen);
  enterActive();
}

// dir -1 = previous, +1 = next, wrapping around.
static void switchScreen(int dir) {
  gotoScreen((s_screen + dir + SCREEN_N) % SCREEN_N);
}

// Act on a gesture captured earlier (possibly during a download). Called only
// from loop(), where nothing else is drawing.
static void dispatchTouch() {
  if (s_pendKind == PEND_NONE) return;
  const PendKind kind = s_pendKind;
  const int a = s_pendA, b = s_pendB;
  s_pendKind = PEND_NONE;

  switch (kind) {
    case PEND_SWIPE: switchScreen(a); break;
    case PEND_TAP:   if (activeTap(a, b)) drawActive(); break;
    default: break;                       // a long press does nothing
  }
}

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

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n=== DevOpsDashboard v%s ===\n", FW_VERSION);
  Serial.printf("Reset reason: %s\n", resetReasonText());
  Serial.printf("Free heap: %u B\n", (unsigned)ESP.getFreeHeap());

  Settings_Begin();
  Layout_SelfTest();         // no-op unless LAYOUT_DEBUG is on

  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  delay(50);
  TCA9554_Init();
  TCA9554_SetPin(EXIO_LCD_PWR, false);
  delay(10);

  // Must happen before ST7701_Init(): the board decides the panel init
  // sequence and the vertical timing.
  Board_Detect();

  // Backlight stays off until the first frame - the panel powers up with
  // random memory content.
  Backlight_Init();
  if (!ST7701_Init()) {
    Serial.println("Display init failed, restarting in 5 s");
    delay(5000);
    ESP.restart();
  }

  Canvas16* canvas = new Canvas16(LCD_WIDTH, LCD_HEIGHT);
  // Draw straight into the panel's second framebuffer (no copy on flush), or
  // fall back to our own buffer if the driver does not expose them.
  uint16_t* fb1 = LCD_FrameBuffer(1);
  if (fb1) {
    canvas->useExternalBuffer(fb1, 1);
    Serial.println("Display: double framebuffer, zero-copy drawing");
  } else if (!canvas->begin()) {
    Serial.println("FATAL: canvas alloc failed (check OPI PSRAM)");
    while (true) delay(1000);
  }
  gfx = canvas;
  gfx->setTextWrap(false);
  gfx->fillScreen(C_BLACK);
  gfx->flush();
  Set_Backlight(Settings_Backlight());

  if (!Touch_Init()) {
    Serial.println("WARNING: touch did not initialise, the screens cannot be switched by touch");
  }

  checkBootReset();

  Net_SetPollFn(netPoll);

  // Connects with the stored credentials, or brings up the access point and
  // leaves it up. Either way the web server is running when this returns.
  WiFi_Begin();

  // After WiFi_Begin(): the SNTP client needs the network stack to exist.
  Clock_Begin();

  s_screen = Settings_Screen();
  if (s_screen < 0 || s_screen >= SCREEN_N) s_screen = SCREEN_DASHBOARD_I;
  // In access-point mode the QR screen owns the display until a network is
  // entered; the screen is still chosen above, ready for when WiFi comes up.
  if (!WiFi_IsAP()) enterActive();

  Watchdog_Begin();   // hardware watchdog for 24/7 operation
  Serial.println("Setup done");
}

// --- Display watchdog -------------------------------------------------------
// The task watchdog cannot see this failure: the sketch is fine, it is the RGB
// peripheral that stops scanning (black screen, backlight still on). So watch
// the frame counter fed by the VSYNC interrupt.
static void displayWatchdog() {
#if DISPLAY_WD
  static uint32_t lastCount = 0;
  static unsigned long lastMove = 0;
  static bool repaired = false;
  static bool armed = false;

  uint32_t now = LCD_VsyncCount();
  unsigned long ms = millis();
  if (now != lastCount) {
    lastCount = now;
    lastMove = ms;
    repaired = false;
    if (!armed && now > 100) armed = true;
    return;
  }
  if (!armed) return;
  if (lastMove == 0) { lastMove = ms; return; }

  unsigned long stalled = ms - lastMove;
  if (!repaired && stalled >= DISPLAY_WD_DEAD_MS) {
    repaired = true;
    Serial.printf("DISPLAY: no frame for %lu ms, trying to recover\n", stalled);
    TCA9554_Verify();
    TCA9554_SetPin(EXIO_LCD_PWR, false);
    TCA9554_SetPin(EXIO_LCD_RST, true);
    Set_Backlight(Settings_Backlight());
  } else if (stalled >= DISPLAY_WD_REBOOT_MS) {
    Serial.printf("DISPLAY: panel not recovered after %lu ms, restarting\n", stalled);
    delay(100);
    ESP.restart();
  }
#endif
}

// No Serial.flush() here: on the native USB CDC it waits for a host to read
// the data, and with no computer attached the watchdog fires first.
static void restartIfAsked() {
  if (!WebConfig_WantsRestart()) return;
  Serial.println("Restart requested");
  delay(400);                       // let the HTTP reply leave the socket
  ESP.restart();
}

void loop() {
  // A firmware upload owns the machine: writing flash suspends the PSRAM cache
  // from under the RGB DMA, so nothing may draw.
  if (WebConfig_UpdateBusy()) {
    WebConfig_Loop();
    delay(1);
    return;
  }

  touchPump();

  // --- Access point: the portal owns the display ----------------------------
  static bool s_apOwnsScreen = false;
  if (WiFi_IsAP()) {
    s_pendKind = PEND_NONE;              // a stray swipe must not take the QR away
    if (!s_apOwnsScreen) {
      s_apOwnsScreen = true;
      WiFi_DrawApScreen();
    }
    WiFi_Loop();                         // may accept credentials and leave AP mode
    WebConfig_Loop();
    restartIfAsked();
    displayWatchdog();
    Settings_Tick();
    Watchdog_Feed();
    delay(5);
    return;
  }
  if (s_apOwnsScreen) {
    // A network was accepted: take the display back.
    s_apOwnsScreen = false;
    s_pendKind = PEND_NONE;
    enterActive();
  }

  dispatchTouch();

  // The same actions asked for from the web. Carried out here rather than in
  // the request handler because drawing must not happen from anywhere else.
  {
    const int scr = WebConfig_TakeScreen();
    if (scr >= 0) gotoScreen(scr);
    const int step = WebConfig_TakeScreenStep();
    if (step) switchScreen(step);
  }

  WiFi_Loop();
  WebConfig_Loop();
  restartIfAsked();

  // Redrawing is decoupled from reading the touch and capped at ~12 FPS.
  static unsigned long lastDraw = 0;
  bool wantDraw = activeTick();
  if (wantDraw && millis() - lastDraw >= 80) {
    drawActive();
    lastDraw = millis();
  }

  // One I2C read every few seconds that checks the expander still holds LCD
  // power and reset where we left them.
  static unsigned long lastExio = 0;
  if (millis() - lastExio >= EXPANDER_CHECK_MS) {
    lastExio = millis();
    TCA9554_Verify();
  }

  displayWatchdog();
  Clock_Tick();
  Settings_Tick();    // debounced persist of UI state to NVS
  Watchdog_Feed();
  delay(5);
}
