// =============================================================================
//  DevOpsDashboard43
//  Poirot computers from the REST API. See DevOpsApi.h.
// =============================================================================
#include "DevOpsApi.h"
#include "Settings.h"
#include "Net.h"
#include "Status.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

struct ApiCfg {
  char     name[API_PROFILE_NAME_MAX + 1];
  char     baseUrl[API_URL_MAX + 1];
  char     key[API_KEY_MAX + 1];
  uint16_t intervalSec;
};

// Both guard copies only; nothing slow ever runs under the lock.
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static ApiSnapshot  s_snap;
static ApiCfg       s_cfg;
static bool         s_cfgChanged = false;
static bool         s_refresh    = false;
static TaskHandle_t s_task       = nullptr;

#define LOCK()   portENTER_CRITICAL(&s_mux)
#define UNLOCK() portEXIT_CRITICAL(&s_mux)

static void copyStr(char* dst, size_t cap, const char* src) {
  strncpy(dst, src ? src : "", cap - 1);
  dst[cap - 1] = '\0';
}

// --- Public ------------------------------------------------------------------
ApiSnapshot DevOpsApi_Get() {
  ApiSnapshot s;
  LOCK(); s = s_snap; UNLOCK();
  return s;
}

void DevOpsApi_RefreshNow() {
  bool wake = false;
  LOCK();
  if (!s_snap.busy && s_snap.state != API_OFF && s_snap.state != API_NO_KEY) {
    s_refresh = true;
    wake = true;
  }
  UNLOCK();
  if (wake && s_task) xTaskNotifyGive(s_task);
}

void DevOpsApi_Configure(const char* name, const char* baseUrl, const char* key,
                         uint16_t intervalSec) {
  if (!name)    name = "";
  if (!baseUrl) baseUrl = "";
  if (!key)     key = "";

  ApiState st = !*baseUrl ? API_OFF : (!*key ? API_NO_KEY : API_WAITING);
  bool wake = false;
  LOCK();
  const bool target = strcmp(s_cfg.baseUrl, baseUrl) != 0 || strcmp(s_cfg.key, key) != 0;
  const bool renamed = strcmp(s_cfg.name, name) != 0;
  copyStr(s_cfg.name, sizeof(s_cfg.name), name);
  s_cfg.intervalSec = intervalSec;
  s_snap.intervalSec = intervalSec;
  if (target) {
    copyStr(s_cfg.baseUrl, sizeof(s_cfg.baseUrl), baseUrl);
    copyStr(s_cfg.key, sizeof(s_cfg.key), key);
    s_cfgChanged = true;
    // Data from another environment must not stay under the new name.
    s_snap.total = s_snap.waiting = s_snap.running = s_snap.warning = s_snap.error = 0;
    s_snap.valid    = false;
    s_snap.state    = st;
    s_snap.httpCode = 0;
    s_snap.lastOkMs = 0;
    s_snap.errorText[0] = '\0';
    wake = true;
  }
  if (target || renamed) {
    copyStr(s_snap.profile, sizeof(s_snap.profile), name);
    s_snap.revision++;
  }
  UNLOCK();

  if (target) {
    if (st == API_OFF)         Status_Set(ST_API, "not configured");
    else if (st == API_NO_KEY) Status_Set(ST_API, "no API key (%s)", name);
    else                       Status_Set(ST_API, "waiting (%s)", name);
    Serial.printf("API: profile '%s' %s\n", name,
                  st == API_OFF ? "has no URL" : st == API_NO_KEY ? "has no key" : "selected");
  }
  if (wake && s_task) xTaskNotifyGive(s_task);
}

void DevOpsApi_ApplySettings() {
  const ApiProfile& p = Settings_ApiProfile(Settings_ApiActive());
  DevOpsApi_Configure(p.name, p.baseUrl, p.key, Settings_ApiIntervalSec());
}

// --- Task --------------------------------------------------------------------
static void setBusy(bool busy) {
  LOCK();
  if (s_snap.busy != busy) { s_snap.busy = busy; s_snap.revision++; }
  UNLOCK();
}

static const char* errorText(int code, bool wifiUp, char* buf, size_t cap) {
  if (code == 401)                  return "401 bad API key";
  if (code == 404)                  return "404 not found";
  if (code == NET_CODE_NOT_JSON)    return "not JSON (wrong URL?)";
  if (code == NET_CODE_BAD_BODY)    return "bad data";
  if (code == NET_CODE_NONE)        return wifiUp ? "low memory" : "offline";
  if (code > 0) { snprintf(buf, cap, "HTTP %d", code); return buf; }
  return "TLS / network error";
}

static void publishError(const char* text, int code, bool offline) {
  char prof[API_PROFILE_NAME_MAX + 1];
  LOCK();
  s_snap.httpCode = code;
  copyStr(s_snap.errorText, sizeof(s_snap.errorText), text);
  // Offline before the first data is still "waiting", not a failure.
  s_snap.state = (offline && !s_snap.valid) ? API_WAITING : API_ERROR;
  s_snap.revision++;
  copyStr(prof, sizeof(prof), s_snap.profile);
  UNLOCK();
  Status_Set(ST_API, "%s (%s)", text, prof);
}

// Parses the body into a snapshot. Missing or negative fields = bad data,
// not zeros: a wrong zero on the display is worse than no number.
static bool parseBody(const String& body, uint16_t out[5]) {
  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;
  static const char* const F[5] = { "total", "waiting", "running", "warning", "error" };
  for (int i = 0; i < 5; i++) {
    JsonVariantConst v = doc[F[i]];
    if (!v.is<int>() || v.as<int>() < 0 || v.as<int>() > 65535) return false;
    out[i] = (uint16_t)v.as<int>();
  }
  return true;
}

// One request to the endpoint of a profile; false with *code set on any failure.
static bool requestPoirot(const char* baseUrl, const char* key, uint16_t v[5], int* code) {
  String url = String(baseUrl) + API_PATH_POIROT;
  String body;
  *code = 0;
  bool ok = Net_GetString(url.c_str(), body, "API", API_KEY_HEADER, key, true, code);
  if (ok && !parseBody(body, v)) { ok = false; *code = NET_CODE_BAD_BODY; }
  return ok;
}

static bool fetchOnce(const ApiCfg& cfg) {
  setBusy(true);
  int code = 0;
  uint16_t v[5] = {};
  const unsigned long t0 = millis();
  bool ok = requestPoirot(cfg.baseUrl, cfg.key, v, &code);
  const unsigned long ms = millis() - t0;

  // The profile was switched while this request ran: the answer is stale.
  bool discard;
  LOCK(); discard = s_cfgChanged; UNLOCK();
  if (discard) { setBusy(false); return true; }

  if (ok) {
    char prof[API_PROFILE_NAME_MAX + 1];
    LOCK();
    s_snap.total = v[0]; s_snap.waiting = v[1]; s_snap.running = v[2];
    s_snap.warning = v[3]; s_snap.error = v[4];
    s_snap.valid = true;
    s_snap.state = API_OK;
    s_snap.httpCode = code;
    s_snap.lastOkMs = millis();
    s_snap.errorText[0] = '\0';
    s_snap.busy = false;
    s_snap.revision++;
    copyStr(prof, sizeof(prof), s_snap.profile);
    UNLOCK();
    Status_Set(ST_API, "OK %u (%s)", (unsigned)v[0], prof);
    Serial.printf("API: OK total=%u waiting=%u running=%u warning=%u error=%u (%lu ms)\n",
                  v[0], v[1], v[2], v[3], v[4], ms);
  } else {
    char tmp[16];
    const char* text = errorText(code, WiFi.status() == WL_CONNECTED, tmp, sizeof(tmp));
    publishError(text, code, false);
    setBusy(false);
    Serial.printf("API: failed - %s (code %d, %lu ms)\n", text, code, ms);
  }
  // A transport error (HTTPClient codes -1..-11) or a 5xx is usually momentary:
  // try again soon instead of showing an error for a whole interval. 401, 404 or
  // a wrong body will not heal by themselves.
  const bool transient = !ok && ((code < 0 && code > -100) || code >= 500);
  return !transient;
}

// --- Connection test ---------------------------------------------------------
static char          s_testUrl[API_URL_MAX + 1];
static char          s_testKey[API_KEY_MAX + 1];
static bool          s_testReq = false;
static ApiTestResult s_test;

ApiTestResult DevOpsApi_GetTest() {
  ApiTestResult t;
  LOCK(); t = s_test; UNLOCK();
  return t;
}

bool DevOpsApi_StartTest(uint8_t slot) {
  if (slot >= API_PROFILES || !s_task) return false;
  const ApiProfile& p = Settings_ApiProfile(slot);
  bool started = false;
  LOCK();
  if (s_test.state != API_TEST_RUNNING) {
    memset(&s_test, 0, sizeof(s_test));
    s_test.slot = slot;
    if (!p.key[0]) {
      s_test.state = API_TEST_DONE;
      copyStr(s_test.text, sizeof(s_test.text), "no API key");
    } else {
      copyStr(s_testUrl, sizeof(s_testUrl), p.baseUrl);
      copyStr(s_testKey, sizeof(s_testKey), p.key);
      s_test.state = API_TEST_RUNNING;
      s_testReq = true;
      started = true;
    }
    UNLOCK();
    if (started) xTaskNotifyGive(s_task);
    return true;
  }
  UNLOCK();
  return false;
}

static void runTest() {
  char url[API_URL_MAX + 1], key[API_KEY_MAX + 1];
  LOCK();
  copyStr(url, sizeof(url), s_testUrl);
  copyStr(key, sizeof(key), s_testKey);
  UNLOCK();

  const bool wifi = WiFi.status() == WL_CONNECTED;
  uint16_t v[5] = {};
  int code = NET_CODE_NONE;
  const bool ok = wifi && requestPoirot(url, key, v, &code);
  char tmp[16];
  const char* text = ok ? "OK" : errorText(code, wifi, tmp, sizeof(tmp));

  LOCK();
  s_test.ok = ok;
  s_test.httpCode = code;
  copyStr(s_test.text, sizeof(s_test.text), text);
  s_test.total = v[0]; s_test.waiting = v[1]; s_test.running = v[2];
  s_test.warning = v[3]; s_test.error = v[4];
  s_test.state = API_TEST_DONE;
  UNLOCK();
  Serial.printf("API test: profile %u - %s (code %d)\n", (unsigned)s_test.slot + 1, text, code);
}

static void apiTask(void*) {
  uint32_t nextAt = 0;
  bool     logStack = true;
  bool     offlineShown = false;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500));

    bool test;
    LOCK(); test = s_testReq; s_testReq = false; UNLOCK();
    if (test) runTest();

    ApiCfg cfg;
    bool changed, refresh;
    LOCK();
    cfg = s_cfg;
    changed = s_cfgChanged;
    refresh = s_refresh;
    s_refresh = false;
    UNLOCK();

    if (!cfg.baseUrl[0] || !cfg.key[0]) {
      LOCK(); s_cfgChanged = false; UNLOCK();
      continue;
    }
    const uint32_t now = millis();
    if (changed || refresh) nextAt = now;
    if ((int32_t)(now - nextAt) < 0) continue;

    if (WiFi.status() != WL_CONNECTED) {
      // The change flag stays: when the link is back, the first fetch is for this config.
      if (!offlineShown) { publishError("offline", NET_CODE_NONE, true); offlineShown = true; }
      nextAt = now + 2000;
      continue;
    }
    offlineShown = false;

    // The change is being served now; a Configure during the fetch sets it again.
    LOCK(); s_cfgChanged = false; UNLOCK();
    const bool settled = fetchOnce(cfg);
    nextAt = millis() + (settled ? (uint32_t)cfg.intervalSec * 1000UL : API_RETRY_MS);

    if (logStack) {
      logStack = false;
      Serial.printf("API: task stack high-water mark %u B free\n",
                    (unsigned)uxTaskGetStackHighWaterMark(nullptr));
    }
  }
}

void DevOpsApi_Begin() {
  memset(&s_snap, 0, sizeof(s_snap));
  memset(&s_cfg, 0, sizeof(s_cfg));
  s_snap.state = API_OFF;
  s_snap.intervalSec = API_INTERVAL_S_DEFAULT;
  // The task runs on core 0 next to the WiFi stack; it never touches LVGL.
  if (xTaskCreatePinnedToCore(apiTask, "devops_api", API_TASK_STACK, nullptr, 1,
                              &s_task, 0) != pdPASS) {
    Serial.println("API: cannot start the fetch task");
    s_task = nullptr;
  }
}
