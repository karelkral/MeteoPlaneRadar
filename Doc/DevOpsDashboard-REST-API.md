# DevOps Dashboard – výukový průvodce: REST API a jeho napojení

Tento dokument vysvětluje:

1. **co** musí umět REST API na serveru (kontrakt, příklad v C# / ASP.NET Core),
2. **kudy** data tečou v C++ firmwaru od URL až po pixel na displeji,
3. **kde** v kódu je co napojeno a co je potřeba dopsat (fáze 6 plánu).

> Stav: firmware 0.1.0 umí URL a interval API **uložit** (webová stránka → NVS) a má hotovou
> síťovou vrstvu (`Net_GetString`). Modul, který API skutečně volá a kreslí data
> (`DevOpsApi.cpp` + úprava `ScreenDashboard.cpp`), je **fáze 6** a zatím není napsaný.
> Tento dokument popisuje jeho návrh.
>
> Pro Waveshare 7" s LVGL je doporučena varianta, kde se API stahuje ve vlastním FreeRTOS
> tasku: [DevOpsDashboard-doporuceni-LVGL.md](DevOpsDashboard-doporuceni-LVGL.md), kap. 4.

---

## 1. Celkový obrázek

```
┌──────────────────────────┐        HTTP GET (každých N s)        ┌──────────────────────────┐
│ ESP32-S3 (DevOpsDashboard)│ ───────────────────────────────────► │ Váš REST API server      │
│                          │                                      │ (ASP.NET Core, …)        │
│ Settings: apiUrl, interval│ ◄─────────────────────────────────── │ GET /api/dashboard       │
│ Net_GetString()          │        200 OK + JSON (< 8 kB)        │  → agreguje CI/CD, alerty│
│ ArduinoJson → ApiItem[]  │                                      └──────────────────────────┘
│ ScreenDashboard_Draw()   │
└──────────────────────────┘
```

Zařízení je **klient**, který se periodicky ptá (polling). Server nic neposílá sám od sebe.
Server je „backend for frontend“: sám si sbírá data z Azure DevOps / GitHubu / Grafany…
a zařízení posílá jen **hotový, malý a plochý** výsledek. Veškerá logika patří na server,
firmware jen zobrazuje.

---

## 2. Co naprogramovat na straně REST API

### 2.1 Endpoint

| Vlastnost | Hodnota |
| --- | --- |
| Metoda | `GET` |
| Cesta | libovolná, např. `/api/dashboard` – celá URL se zadává ve webu zařízení |
| Protokol | **`http://` v lokální síti** (doporučeno); `https://` funguje, ale bez ověření certifikátu a stojí ~45 kB RAM |
| Autentizace | **žádná** (firmware zatím neumí posílat vlastní hlavičky – viz 4.3) |
| Odpověď | `200 OK`, `Content-Type: application/json; charset=utf-8` |
| Velikost | ideálně **< 8 kB**, tvrdý strop 128 kB (`NET_MAX_TEXT`) |
| Doba odpovědi | **< 2 s**; firmware čeká max. 6 s na spojení + 10 s na odpověď |
| Jiný kód než 200 | firmware ho bere jako chybu a ponechá na displeji stará data |

### 2.2 JSON kontrakt

```json
{
  "title": "Prod",
  "generatedAt": "2026-11-06T15:20:00Z",
  "items": [
    { "name": "Build main",  "status": "ok",      "value": "#1234" },
    { "name": "Deploy prod", "status": "running", "value": "62%"   },
    { "name": "Alerts",      "status": "error",   "value": "3"     }
  ]
}
```

| Pole | Typ | Limit ve firmwaru | Význam |
| --- | --- | --- | --- |
| `title` | string | ~20 znaků | nadpis obrazovky (volitelné) |
| `generatedAt` | string ISO 8601 | – | kdy server data připravil (volitelné, pro ladění) |
| `items` | pole | **max. 6 položek**, další se ignorují | řádky dashboardu |
| `items[].name` | string | **23 bajtů** | popisek vlevo |
| `items[].value` | string | **15 bajtů** | hodnota vpravo – posílejte už naformátovaný text |
| `items[].status` | string | `ok` / `running` / `warning` / `error` / jiné | barva: zelená / žlutá / žlutá / červená / šedá |

Pravidla, proč je kontrakt takový:

- **Plochý a krátký** – ESP32 má málo RAM; parsuje se celý dokument najednou.
- **Hodnoty jako text** (`"62%"`, ne `0.62`) – formátování dělá server, firmware je hloupý.
- **Limity v bajtech UTF-8** – znak s diakritikou zabírá 2 bajty. Delší text se ořízne.
- **Diakritika:** malé velikosti písma (1–3) mají rozšířenou sadu 289 znaků, velké (4–5) jen 126 znaků.
  Pro jistotu posílejte **ASCII** (bez háčků a čárek); chybějící znak se vynechá.
- **Neměnit názvy polí** bez úpravy firmwaru – chybějící pole se tiše bere jako prázdné.

### 2.3 Příklad serveru v C# (ASP.NET Core minimal API)

```csharp
// dotnet new web -n DevOpsDashboardApi
var builder = WebApplication.CreateBuilder(args);
builder.Services.ConfigureHttpJsonOptions(o =>
    o.SerializerOptions.PropertyNamingPolicy = System.Text.Json.JsonNamingPolicy.CamelCase);
var app = builder.Build();

app.MapGet("/api/dashboard", async () =>
{
    // Tady posbírejte data (Azure DevOps REST API, GitHub Actions, Prometheus, ...).
    // Výsledek cachujte – zařízení se ptá každých pár sekund.
    var items = new List<DashboardItem>
    {
        new("Build main",  "ok",      "#1234"),
        new("Deploy prod", "running", "62%"),
        new("Alerts",      "error",   "3"),
    };
    return Results.Ok(new DashboardDto("Prod", DateTime.UtcNow, items.Take(6).ToList()));
});

app.Run("http://0.0.0.0:5000");   // poslouchat na všech rozhraních, ne jen localhost!

record DashboardItem(string Name, string Status, string Value);
record DashboardDto(string Title, DateTime GeneratedAt, List<DashboardItem> Items);
```

Doporučení pro server:

- **Cache** (např. `IMemoryCache` nebo `BackgroundService`, který data obnovuje každých 30 s) –
  endpoint pak odpovídá okamžitě a nezatěžuje Azure DevOps při každém dotazu zařízení.
- **Ořezávat texty na serveru** (`name` ≤ 23, `value` ≤ 15 znaků ASCII), ať je vidět, co se zobrazí.
- **Žádné stránkování, žádné vnoření** – jedna odpověď = jedna obrazovka.
- `Kestrel` posílá hlavičku `Date` – firmware ji použije jako záložní zdroj času, když nefunguje NTP.

### 2.4 Zpřístupnění serveru pro zařízení

1. Server musí poslouchat na **IP v síti**, ne na `localhost` (`http://0.0.0.0:5000`).
2. **Windows Firewall:** povolit příchozí TCP port, např.
   `New-NetFirewallRule -DisplayName "DevOps API" -Direction Inbound -Protocol TCP -LocalPort 5000 -Action Allow`
3. Zařízení a server musí být ve **stejné síti** (zařízení vidí jen to, co jeho WiFi).
4. Používejte **IP adresu** (`http://192.168.1.10:5000/api/dashboard`); jména `.local` firmware při
   volání API nepřekládá spolehlivě.

### 2.5 Test před napojením zařízení

```powershell
# z jiného počítače ve stejné síti:
curl.exe -i http://192.168.1.10:5000/api/dashboard
```

Musí vrátit `HTTP/1.1 200 OK` a JSON výše. Pak URL vložte do webu zařízení
(`http://<ip-zařízení>/` → sekce **DevOps API** → Save). Stav posledního volání ukazuje
`GET http://<ip-zařízení>/api/status` v poli `"api"`.

---

## 3. Kudy tečou data ve firmwaru (C++)

```
 Web stránka (WebPage.h)                 POST /api/config {"apiUrl": "...", "apiIntervalSec": 30}
        │
        ▼
 WebConfig.cpp  handlePostConfig()  ──►  Settings_SetApiUrl() / Settings_SetApiIntervalSec()
                                             │  (validace http/https, uložení do NVS "devops")
                                             ▼
 DevOpsDashboard.ino  loop()  ──►  DevOpsApi_Tick()            ← FÁZE 6 (zatím neexistuje)
                                        │  je čas? (interval) je WiFi?
                                        ▼
                                   Net_GetString(url, body, "API")   (Net.cpp – hotové)
                                        │  HTTPClient GET, timeouty, strop 128 kB,
                                        │  hlavička Date → Clock_NoteHttpDate()
                                        ▼
                                   deserializeJson(doc, body)        (ArduinoJson)
                                        │  items[] → ApiItem s_items[6]
                                        ▼
                                   Status_Set(ST_API, "OK, 3 items")  → /api/status
                                   s_rev++  (nová data)
                                        │
 loop() ──► activeTick() ──► ScreenDashboard_Tick()  vrátí true, když se změnila revize
        └─► drawActive() ──► ScreenDashboard_Draw()  nakreslí řádky z DevOpsApi_Item(i)
                              gfx->flush()           výměna framebufferu
```

### 3.1 Mapa souborů

| Soubor | Role pro API | Stav |
| --- | --- | --- |
| [Config.h](../DevOpsDashboard/Config.h) | `API_URL_DEFAULT`, `API_INTERVAL_S_*`, `NET_MAX_TEXT`, timeouty | hotové |
| [Settings.h](../DevOpsDashboard/Settings.h) / `.cpp` | `Settings_ApiUrl()`, `Settings_ApiIntervalSec()` – uložení v NVS | hotové |
| [WebPage.h](../DevOpsDashboard/WebPage.h) | formulář „DevOps API“ | hotové |
| [WebConfig.cpp](../DevOpsDashboard/WebConfig.cpp) | `POST /api/config` (validace URL), `GET /api/status` (pole `api`) | hotové |
| [Net.h](../DevOpsDashboard/Net.h) / `.cpp` | `Net_GetString()` – samotné HTTP volání | hotové |
| [Status.h](../DevOpsDashboard/Status.h) | slot `ST_API` – text stavu pro web | hotové |
| `DevOpsApi.h` / `.cpp` | periodické stahování + parsování JSON | **fáze 6** |
| [ScreenDashboard.cpp](../DevOpsDashboard/ScreenDashboard.cpp) | vykreslení položek | **fáze 6** (teď jen nápis) |
| [DevOpsDashboard.ino](../DevOpsDashboard/DevOpsDashboard.ino) | volání `DevOpsApi_Tick()` v `loop()` | **fáze 6** (1 řádek) |

---

## 4. Co dopsat v C++ (fáze 6)

### 4.1 `DevOpsApi.h`

```cpp
#pragma once
#include <Arduino.h>

#define API_MAX_ITEMS 6
enum ApiStatus : uint8_t { API_ST_UNKNOWN = 0, API_ST_OK, API_ST_WARN, API_ST_ERROR };

struct ApiItem {
  char    name[24];     // 23 bajtů + koncová nula
  char    value[16];
  uint8_t status;       // ApiStatus
};

void           DevOpsApi_Tick();          // volat z loop()
void           DevOpsApi_Invalidate();    // stáhnout hned (po změně URL ve webu)
bool           DevOpsApi_Valid();         // máme aspoň jednou data?
bool           DevOpsApi_Stale();         // poslední pokus selhal
uint32_t       DevOpsApi_Revision();      // mění se s každými novými daty
const char*    DevOpsApi_Title();
uint8_t        DevOpsApi_Count();
const ApiItem* DevOpsApi_Item(uint8_t i);
```

Pro C# vývojáře: tohle je „statická třída“. Žádné `new`, žádné výjimky – stav je v `static`
proměnných v `.cpp`, chyby se vrací přes `bool` a hlásí do `Status`.

### 4.2 `DevOpsApi.cpp` – jádro

```cpp
#include "DevOpsApi.h"
#include "Settings.h"
#include "Net.h"
#include "Status.h"
#include "WiFiPortal.h"
#include <ArduinoJson.h>

static ApiItem       s_items[API_MAX_ITEMS];
static uint8_t       s_count = 0;
static char          s_title[24] = "";
static bool          s_valid = false, s_stale = false;
static uint32_t      s_rev = 0;
static unsigned long s_last = 0;
static bool          s_force = true;

static uint8_t parseStatus(const char* s) {
  if (!strcmp(s, "ok")) return API_ST_OK;
  if (!strcmp(s, "running") || !strcmp(s, "warning")) return API_ST_WARN;
  if (!strcmp(s, "error")) return API_ST_ERROR;
  return API_ST_UNKNOWN;
}

void DevOpsApi_Invalidate() { s_force = true; }

void DevOpsApi_Tick() {
  const char* url = Settings_ApiUrl();
  if (!*url || !WiFi_IsConnected()) return;
  const unsigned long now = millis();
  if (!s_force && now - s_last < Settings_ApiIntervalSec() * 1000UL) return;
  s_force = false;
  s_last = now;

  String body;
  if (!Net_GetString(url, body, "API")) {          // blokuje max. ~16 s, dotyk běží přes netPoll()
    s_stale = true; s_rev++;
    Status_Set(ST_API, "HTTP error");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) { s_stale = true; s_rev++; Status_Set(ST_API, "Bad JSON: %s", err.c_str()); return; }

  strlcpy(s_title, doc["title"] | "", sizeof(s_title));
  s_count = 0;
  for (JsonObjectConst it : doc["items"].as<JsonArrayConst>()) {
    if (s_count >= API_MAX_ITEMS) break;
    ApiItem& d = s_items[s_count++];
    strlcpy(d.name,  it["name"]  | "", sizeof(d.name));
    strlcpy(d.value, it["value"] | "", sizeof(d.value));
    d.status = parseStatus(it["status"] | "");
  }
  s_valid = true; s_stale = false; s_rev++;
  Status_Set(ST_API, "OK, %u items", (unsigned)s_count);
}
// + jednoduché gettery DevOpsApi_Valid(), _Stale(), _Revision(), _Title(), _Count(), _Item(i)
```

Klíčové věci:

- `doc["name"] | ""` = C# `?? ""` – když pole chybí, použije se výchozí hodnota.
- `strlcpy` ořízne text na velikost bufferu – nikdy nepřepíše paměť.
- `Net_GetString` **blokuje** `loop()`; během čekání se volá `netPoll()` (watchdog + dotyk).
  Proto má server odpovídat rychle.

### 4.3 Napojení do zbytku

1. **`DevOpsDashboard.ino`** – v `loop()` za `Clock_Tick();` přidat `DevOpsApi_Tick();`
   a nahoru `#include "DevOpsApi.h"`.
2. **`WebConfig.cpp`** – v `handlePostConfig()` po úspěšném uložení URL/intervalu zavolat
   `DevOpsApi_Invalidate();`, aby se data stáhla hned, ne až po intervalu.
3. **`ScreenDashboard.cpp`**:

   ```cpp
   static uint32_t s_seenRev = 0;

   bool ScreenDashboard_Tick() {
     bool redraw = UI_StatusLineChanged();
     if (DevOpsApi_Revision() != s_seenRev) { s_seenRev = DevOpsApi_Revision(); redraw = true; }
     return redraw;
   }

   void ScreenDashboard_Draw() {
     gfx->fillScreen(C_BLACK);
     UI_DrawStatusLine(LY_STATUS);
     if (!DevOpsApi_Valid()) {                     // ještě nic nepřišlo → původní nápis
       UI_TextCentered("DevOps", LCD_HEIGHT / 2 - 40, C_CYAN, 5);
       UI_TextCentered("dashboard", LCD_HEIGHT / 2 + 10, C_WHITE, 4);
       return;
     }
     UI_TextCentered(DevOpsApi_Title(), 80, C_CYAN, 3);
     const uint16_t COL[] = { C_GRAY, C_GREEN, C_YELLOW, C_RED };
     for (uint8_t i = 0; i < DevOpsApi_Count(); i++) {
       const ApiItem* it = DevOpsApi_Item(i);
       const int y = 130 + i * 44;                  // 6 řádků se vejde mezi y=130 a y=390
       const uint16_t c = DevOpsApi_Stale() ? C_GRAY : COL[it->status];
       gfx->fillCircle(110, y + 8, 7, c);           // barevná tečka stavu
       gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
       gfx->setCursor(128, y); gfx->print(it->name);
       const int w = Layout_TextW(it->value, 2);
       gfx->setTextColor(c); gfx->setCursor(370 - w, y); gfx->print(it->value);
     }
   }
   ```

   Displej je **kulatý**: řádky drží mezi x = 100 a 380, kde je kruh dost široký
   (ověřit lze přes `Layout_ChordHalf(y)`).

4. **(volitelně) API klíč** – pokud server bude chtít autentizaci, je potřeba rozšířit
   `Net_GetString()` v [Net.cpp](../DevOpsDashboard/Net.cpp) o parametr s hlavičkou
   (`http.addHeader("Authorization", "Bearer ...")` před `http.GET()`) a přidat do `Settings`
   pole pro klíč. Klíč pak nikdy nevracet v `GET /api/config`.

### 4.4 Jak ladit

| Kde | Co uvidíte |
| --- | --- |
| Sériová linka 115 200 Bd (konektor USB) | `API: HTTP 404`, `API: URL ...`, `API: response: ...` – vypisuje `Net.cpp` |
| `GET http://<zařízení>/api/status` → `"api"` | `OK, 3 items (12 s ago)` / `HTTP error` / `Bad JSON: ...` |
| `curl` na server z PC | zda server vůbec odpovídá mimo localhost |

Typické chyby:

| Příznak | Příčina |
| --- | --- |
| `HTTP -1` | server neběží, špatná IP/port, firewall, server poslouchá jen na `localhost` |
| `HTTP 404` | špatná cesta v URL |
| `Bad JSON: InvalidInput` | server vrací HTML (chybová stránka) nebo BOM |
| `Bad JSON: NoMemory` | odpověď je příliš velká – zmenšit |
| ořezané texty | delší než 23 / 15 bajtů – zkrátit na serveru |
| chybějící znaky | diakritika ve velkém písmu – používat ASCII |
