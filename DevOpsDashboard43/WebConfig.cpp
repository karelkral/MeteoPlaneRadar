// =============================================================================
//  DevOpsDashboard43 (from DevOpsDashboard, based on MeteoPlaneRadar by chiptron.cz)
//  The configuration web server. See WebConfig.h.
//
//  Based on MeteoPlaneRadar by Petr / chiptron.cz (MIT, see LICENSE.txt).
// =============================================================================
#include "WebConfig.h"
#include "WebPage.h"
#include "Settings.h"
#include "Status.h"
#include "Version.h"
#include "Config.h"
#include "Clock.h"
#include "Ui.h"
#include "LvPort.h"
#include "Watchdog.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <time.h>

static WebServer  s_srv(WEB_PORT);
static DNSServer  s_dns;
static bool s_apMode = false;
static bool s_running = false;
static bool s_wantConnect = false;
static bool s_wantRestart = false;
static volatile bool s_updating = false;

// Queued remote-control requests - see the note in WebConfig.h.
static int s_reqScreen     = -1;
static int s_reqScreenStep = 0;

bool WebConfig_UpdateBusy()        { return s_updating; }
bool WebConfig_WantsWifiConnect()  { return s_wantConnect; }
void WebConfig_ClearWifiConnect()  { s_wantConnect = false; }
bool WebConfig_WantsRestart()      { return s_wantRestart; }

int WebConfig_TakeScreen()     { int v = s_reqScreen;     s_reqScreen = -1;    return v; }
int WebConfig_TakeScreenStep() { int v = s_reqScreenStep; s_reqScreenStep = 0; return v; }

// --- Helpers ----------------------------------------------------------------
static void sendJson(int code, JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  s_srv.send(code, "application/json", out);
}

static void sendError(int code, const char* what) {
  JsonDocument doc;
  doc["error"] = what;
  sendJson(code, doc);
}

static bool readBody(JsonDocument& doc) {
  if (!s_srv.hasArg("plain")) return false;
  return deserializeJson(doc, s_srv.arg("plain")) == DeserializationError::Ok;
}

// Every destructive endpoint goes through here. When no password is set this
// waves everything through - that is the documented default, and the device
// only listens on the home LAN.
static bool authed(JsonDocument& body) {
  const char* pw = body["password"] | "";
  if (Settings_CheckAdminPassword(pw)) return true;
  sendError(403, "password");
  return false;
}

static String deviceIp() {
  return s_apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}

// --- Endpoints --------------------------------------------------------------
static void handleRoot() {
  s_srv.sendHeader("Cache-Control", "no-store");
  // The charset belongs in the HEADER, not only in a meta tag.
  s_srv.send_P(200, "text/html; charset=utf-8", PAGE_HTML);
}

static void handleGetConfig() {
  JsonDocument doc;
  JsonObject o = doc.to<JsonObject>();
  Settings_ToJson(o);
  // The remembered networks: names only. A password never leaves the device.
  JsonArray w = o["wifi"].to<JsonArray>();
  for (uint8_t i = 0; i < WIFI_SLOTS; i++) {
    JsonObject e = w.add<JsonObject>();
    e["ssid"] = Settings_WifiSsid(i);
    e["hasPass"] = Settings_WifiPass(i)[0] != '\0';
  }
  o["version"] = FW_VERSION;
  o["apMode"]  = s_apMode;
  o["ip"]      = deviceIp();
  o["apiIntervalMin"] = API_INTERVAL_S_MIN;
  o["apiIntervalMax"] = API_INTERVAL_S_MAX;
  sendJson(200, doc);
}

static void handlePostConfig() {
  JsonDocument doc;
  if (!readBody(doc)) { sendError(400, "json"); return; }

  // Changing the password requires the current one, so nobody who can reach
  // the page can lock the owner out of the update and reset endpoints.
  JsonVariantConst np = doc["newPassword"];
  if (!np.isNull()) {
    const char* p = np.as<const char*>();
    if (p && *p) {
      if (!authed(doc)) return;
      Settings_SetAdminPassword(p);
    }
  }

  // Validated here rather than silently ignored, so the page can say why.
  JsonVariantConst au = doc["apiUrl"];
  if (!au.isNull()) {
    if (!Settings_SetApiUrl(au.as<const char*>())) { sendError(400, "apiUrl"); return; }
    doc.remove("apiUrl");
  }

  Settings_FromJson(doc.as<JsonObjectConst>());
  Ui_SetBrightness(Settings_Backlight());  // see the change while on the slider

  JsonDocument res;
  res["ok"] = true;
  sendJson(200, res);
}

// Jump to a screen ({"index":n}) or step one along ({"step":-1|1}).
static void handleScreen() {
  JsonDocument doc;
  if (!readBody(doc)) { sendError(400, "json"); return; }

  JsonVariantConst idx = doc["index"];
  if (!idx.isNull()) {
    int i = idx.as<int>();
    if (i < 0 || i >= UI_SCREEN_COUNT) { sendError(400, "range"); return; }
    s_reqScreen = i;
  } else {
    int st = doc["step"] | 0;
    s_reqScreenStep = (st < 0) ? -1 : ((st > 0) ? +1 : 0);
  }
  s_srv.send(200, "application/json", "{\"ok\":true}");
}

static const char* resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "power on";
    case ESP_RST_SW:       return "software";
    case ESP_RST_PANIC:    return "PANIC";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return "WATCHDOG";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_EXT:      return "reset pin";
    default:               return "?";
  }
}

static void handleStatus() {
  JsonDocument doc;
  doc["version"] = FW_VERSION;
  doc["board"] = BOARD_NAME;
  doc["ip"]   = deviceIp();
  doc["ssid"] = s_apMode ? String(AP_SSID) : WiFi.SSID();
  doc["rssi"] = s_apMode ? 0 : WiFi.RSSI();

  unsigned long up = millis() / 1000UL;
  char ub[32];
  snprintf(ub, sizeof(ub), "%lud %02lu:%02lu:%02lu",
           up / 86400UL, (up / 3600UL) % 24UL, (up / 60UL) % 60UL, up % 60UL);
  doc["uptime"] = ub;

  doc["heap"]  = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  doc["heapLargest"] = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  doc["psram"] = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  doc["resetReason"] = resetReason();

  if (Clock_TimeValid()) {
    time_t now = time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    char tb[24];
    strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S", &lt);
    doc["time"] = tb;
  } else {
    doc["time"] = "";
  }
  doc["screen"] = Ui_CurrentScreen();

  char b[64];
  Status_Text(ST_API, b, sizeof(b)); doc["api"] = b;
  Status_Text(ST_NTP, b, sizeof(b)); doc["ntp"] = b;
  sendJson(200, doc);
}

// --- WiFi -------------------------------------------------------------------
// A scan hops across all channels for a few seconds. On the access point that
// cuts the phone off, and the request that asked for the scan dies with
// "failed to fetch". So in AP mode the page gets the last result at once and a
// background scan refreshes it for the next press; WiFi_Begin() fills the list
// before the access point goes up.
static String s_scanJson = "[]";

static void storeScan(int n) {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n && i < 20; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
  }
  WiFi.scanDelete();
  s_scanJson = "";
  serializeJson(doc, s_scanJson);
}

void WebConfig_ScanNow() {
  storeScan(WiFi.scanNetworks());
  Serial.printf("WiFi: scan %s\n", s_scanJson.c_str());
}

static void handleScan() {
  if (s_apMode) {
    if (WiFi.scanComplete() != WIFI_SCAN_RUNNING) WiFi.scanNetworks(true);
    s_srv.send(200, "application/json", s_scanJson);
    return;
  }
  WebConfig_ScanNow();
  s_srv.send(200, "application/json", s_scanJson);
}

// Remember a network and connect to it (used by the captive portal).
static void handleWifi() {
  JsonDocument doc;
  if (!readBody(doc)) { sendError(400, "json"); return; }
  const char* ssid = doc["ssid"] | "";
  const char* pass = doc["pass"] | "";
  if (!*ssid) { sendError(400, "ssid"); return; }
  Serial.printf("Web: WiFi '%s' received (password %u chars)\n", ssid, (unsigned)strlen(pass));
  Settings_SetWifi(ssid, pass);
  s_wantConnect = true;
  s_srv.send(200, "application/json", "{\"ok\":true}");
}

// Save the remembered networks WITHOUT connecting. A missing or empty "pass"
// keeps whatever is stored, because the page never knows the stored passwords.
static void handleWifiList() {
  JsonDocument doc;
  if (!readBody(doc)) { sendError(400, "json"); return; }
  JsonArrayConst nets = doc["nets"];
  if (nets.isNull()) { sendError(400, "nets"); return; }
  for (uint8_t i = 0; i < WIFI_SLOTS && i < nets.size(); i++) {
    const char* ssid = nets[i]["ssid"] | "";
    const char* pass = nets[i]["pass"] | "";
    Settings_SetWifiSlot(i, ssid, (pass && *pass) ? pass : nullptr);
  }
  s_srv.send(200, "application/json", "{\"ok\":true}");
}

// --- System -----------------------------------------------------------------
static void handleReboot() {
  s_srv.send(200, "application/json", "{\"ok\":true}");
  s_wantRestart = true;          // loop() restarts once the reply is out
}

static void handleReset() {
  JsonDocument doc;
  readBody(doc);
  if (!authed(doc)) return;
  s_srv.send(200, "application/json", "{\"ok\":true}");
  Settings_ClearAll();
  s_wantRestart = true;
}

// Captive portal: whatever the phone asks for, hand it the setup page. Without
// this the "sign in to network" banner never appears.
static void handleNotFound() {
  if (s_apMode) {
    s_srv.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    s_srv.send(302, "text/plain", "");
    return;
  }
  s_srv.send(404, "text/plain", "404");
}

// --- OTA callbacks ----------------------------------------------------------
// The RGB panel streams its frame buffer out of PSRAM continuously, and writing
// flash suspends the cache from under it - the picture tears and jumps. So the
// message goes up, the backlight goes off for the duration and the browser
// shows the real progress bar. loop() (this task) holds the LVGL lock for the
// whole upload, which stops all drawing until otaEnd().
static bool s_otaLocked = false;

static void otaStart() {
  s_updating = true;
  Ui_ShowMessage("Updating firmware...", "Do not disconnect power");
  delay(300);                          // let the LVGL task draw the message
  LvPort_Lock();
  s_otaLocked = true;
  Ui_SetBrightness(0);                 // backlight off
}

static void otaEnd(bool ok) {
  if (!s_otaLocked) return;
  s_otaLocked = false;
  LvPort_Unlock();
  if (ok) Ui_ShowMessage("Done, restarting...", "");
  else    Ui_HideOverlay();            // the browser shows the reason
  Ui_SetBrightness(Settings_Backlight());
  s_updating = false;
}

// --- Firmware update --------------------------------------------------------
// This replaces the ElegantOTA library. That library is AGPL-3.0, which would
// have made every binary built from this project AGPL too; the Update class
// ships with the ESP32 core and the web server was already here, so the page
// below was the only piece actually missing.

static bool   s_updOk = false;
static String s_updErr;          // empty = no failure yet

// HTTP Basic, and only when a password is set - the open default is documented
// in Settings.h and in the README.
static bool updateAuthed() {
  if (!Settings_HasAdminPassword()) return true;
  return s_srv.authenticate(WEB_ADMIN_USER, Settings_AdminPassword());
}

static const char UPDATE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Firmware update</title><style>
body{background:#111;color:#eee;font:16px system-ui,sans-serif;margin:0;padding:24px;
 max-width:520px;margin-inline:auto}
h1{font-size:20px;margin:0 0 4px}
p{color:#aaa;line-height:1.5}
input[type=file]{width:100%;padding:12px;background:#1c1c1c;border:1px solid #333;
 border-radius:8px;color:#eee;box-sizing:border-box}
button{width:100%;padding:14px;margin-top:12px;font-size:16px;border:0;border-radius:8px;
 background:#2d7;color:#000;font-weight:600;cursor:pointer}
button:disabled{background:#444;color:#888;cursor:default}
progress{width:100%;height:10px;margin-top:16px}
#s{margin-top:10px;min-height:1.4em;font-weight:600}
a{color:#5bf}
</style></head><body>
<h1>Firmware update</h1>
<p>Pick the <b>.ino.bin</b> file (the one without <i>merged</i>). The display goes dark while the flash is written and comes back on when it is done.</p>
<input type="file" id="f" accept=".bin">
<button id="b">Upload</button>
<progress id="p" value="0" max="100"></progress>
<div id="s"></div>
<p><a href="/">Back to settings</a></p>
<script>
var f=document.getElementById('f'),b=document.getElementById('b'),
    p=document.getElementById('p'),s=document.getElementById('s');
b.onclick=function(){
 if(!f.files.length){s.textContent='Pick a file first.';return;}
 var fd=new FormData();fd.append('update',f.files[0]);
 var x=new XMLHttpRequest();x.open('POST','/update');
 x.upload.onprogress=function(e){if(e.lengthComputable){
   var v=Math.round(e.loaded/e.total*100);p.value=v;s.textContent=v+' %';}};
 x.onload=function(){b.disabled=false;f.disabled=false;
   if(x.status==200){p.value=100;s.textContent='Done. The device is restarting.';}
   else{s.textContent='Update failed: '+(x.responseText||x.status);}};
 x.onerror=function(){b.disabled=false;f.disabled=false;s.textContent='Update failed';};
 b.disabled=true;f.disabled=true;s.textContent='0 %';
 x.send(fd);};
</script></body></html>)rawliteral";

static void handleUpdatePage() {
  if (!updateAuthed()) { s_srv.requestAuthentication(); return; }
  s_srv.sendHeader("Cache-Control", "no-store");
  s_srv.send_P(200, "text/html; charset=utf-8", UPDATE_HTML);
}

// Called repeatedly by WebServer as the body arrives. The whole transfer runs
// inside one handleClient(), so nothing else can be drawing meanwhile - but the
// watchdog still has to be fed by hand.
static void handleUpdateUpload() {
  HTTPUpload& up = s_srv.upload();

  switch (up.status) {
    case UPLOAD_FILE_START:
      s_updOk = false;
      s_updErr = "";
      if (!updateAuthed()) { s_updErr = "auth"; return; }
      Serial.printf("OTA: %s\n", up.filename.c_str());
      otaStart();
      // The browser does not announce the image size up front, so let Update
      // take the whole free OTA slot.
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        s_updErr = Update.errorString();
        otaEnd(false);
      }
      break;

    case UPLOAD_FILE_WRITE:
      if (s_updErr.length()) return;              // already failed, drain the body
      if (Update.write(up.buf, up.currentSize) != up.currentSize) {
        s_updErr = Update.errorString();
        Update.abort();
        otaEnd(false);
        return;
      }
      Watchdog_Feed();
      break;

    case UPLOAD_FILE_END:
      if (s_updErr.length()) return;
      if (Update.end(true)) {
        s_updOk = true;
        Serial.printf("OTA: done, %u B\n", (unsigned)up.totalSize);
        otaEnd(true);
      } else {
        s_updErr = Update.errorString();
        otaEnd(false);
      }
      break;

    case UPLOAD_FILE_ABORTED:
      if (s_updErr == "auth") return;             // never started, keep the 401
      if (Update.isRunning()) Update.abort();
      if (s_updating) otaEnd(false);              // only if the screen was taken over
      s_updErr = "aborted";
      break;
  }
}

// Runs once the body has been consumed, so this is where the verdict is sent.
static void handleUpdateDone() {
  if (s_updErr == "auth") { s_updErr = ""; s_srv.requestAuthentication(); return; }
  s_srv.sendHeader("Connection", "close");
  if (s_updOk) {
    s_srv.send(200, "text/plain", "OK");
    delay(400);
    LvPort_Restart();
  }
  s_srv.send(500, "text/plain", s_updErr.length() ? s_updErr : String("update failed"));
  s_updErr = "";
}

// --- Lifecycle --------------------------------------------------------------
static void startMdns() {
  if (MDNS.begin(WEB_HOSTNAME)) {
    MDNS.addService("http", "tcp", WEB_PORT);
    Serial.printf("Web: http://%s.local/ or http://%s/\n",
                  WEB_HOSTNAME, WiFi.localIP().toString().c_str());
  } else {
    Serial.printf("Web: http://%s/  (mDNS did not start)\n",
                  WiFi.localIP().toString().c_str());
  }
}

void WebConfig_Begin(bool apMode) {
  s_apMode = apMode;

  // The handlers are registered ONCE: this is called again when the device
  // moves from the access point to the home network, and WebServer::on()
  // appends rather than replaces.
  if (s_running) {
    if (apMode) {
      s_dns.setErrorReplyCode(DNSReplyCode::NoError);
      s_dns.start(53, "*", WiFi.softAPIP());
    } else {
      s_dns.stop();
      startMdns();
    }
    return;
  }

  s_srv.on("/", HTTP_GET, handleRoot);
  s_srv.on("/api/config", HTTP_GET, handleGetConfig);
  s_srv.on("/api/config", HTTP_POST, handlePostConfig);
  s_srv.on("/api/status", HTTP_GET, handleStatus);
  s_srv.on("/api/screen", HTTP_POST, handleScreen);
  s_srv.on("/api/scan", HTTP_GET, handleScan);
  s_srv.on("/api/wifi", HTTP_POST, handleWifi);
  s_srv.on("/api/wifilist", HTTP_POST, handleWifiList);
  s_srv.on("/api/reboot", HTTP_POST, handleReboot);
  s_srv.on("/api/reset", HTTP_POST, handleReset);
  // HTTP Basic when a password is set - see Settings.h.
  s_srv.on("/update", HTTP_GET, handleUpdatePage);
  s_srv.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  s_srv.onNotFound(handleNotFound);

  s_srv.begin();
  s_running = true;

  if (apMode) {
    s_dns.setErrorReplyCode(DNSReplyCode::NoError);
    s_dns.start(53, "*", WiFi.softAPIP());
    Serial.printf("Web: portal at http://%s/\n", WiFi.softAPIP().toString().c_str());
  } else {
    startMdns();
  }
}

void WebConfig_Loop() {
  if (!s_running) return;
  if (s_apMode) {
    s_dns.processNextRequest();
    const int n = WiFi.scanComplete();      // background scan from handleScan()
    if (n >= 0) storeScan(n);
  }
  s_srv.handleClient();
}
