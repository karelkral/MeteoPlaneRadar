# DevOps Dashboard 4.3 – plán: napojení na REST API

> Související dokumenty: [plán DevOpsDashboard43](DevOpsDashboard43-plan.md),
> [doporučení: LVGL v Arduinu, kap. 4](DevOpsDashboard-doporuceni-LVGL.md#4-architektura-síť-ve-vlastním-tasku-ui-jen-pod-zámkem),
> [výukový průvodce REST API](DevOpsDashboard-REST-API.md).
>
> Stav: **plán**, neimplementováno. Realizuje se po fázích 43.5 (spolehlivost) a 43.6 (dokumentace)
> z [plánu DevOpsDashboard43](DevOpsDashboard43-plan.md). Plán vznikl 14. 11. 2026.
>
> Repozitář je veřejný: skutečné adresy serveru a API klíč sem **nepatří**. V textu jsou
> zástupné hodnoty `<api-host>` a `<Environment>`. Skutečné hodnoty se zadávají jen ve webu zařízení.

Dashboard má místo testovacích dat zobrazovat skutečná data z REST API. Musí umět přepínat mezi
prostředími serveru, autorizovat se API klíčem, obnovovat se každých 30 s a jít obnovit i ručně.

## 0. Shrnutí rozhodnutí

| Oblast | Rozhodnutí |
| --- | --- |
| Endpoint | `GET {baseUrl}/IotDashboard/PoirotComputers`; cestu doplní firmware, ve webu se zadává jen base URL |
| Autorizace | hlavička `X-Api-Key` (na serveru `ApiHeaderNames.ApiKey`) |
| Prostředí | **3 profily** (název, base URL, API klíč). Aktivní profil se vybírá **jen ve webu**, displej ukazuje jen jeho název v liště. |
| API klíč | jen pro zápis: web ho nikdy nevrací, ukazuje jen „nastaven / nenastaven“; prázdné pole = ponechat uložený |
| Dashboard | prstencový graf místo pipelines ukazuje **Poirot počítače**: uprostřed `total`, popisek „Poirot computers“ |
| Barvy | Running **zelená**, Waiting **modrá**, Warning **žlutá**, Error **červená** |
| Obnovení | automaticky podle intervalu (výchozí **30 s**, 5–3600 s); ručně **ikonou ⟳ v liště** |
| TLS | **ověřovat certifikát** proti vestavěnému balíku kořenových CA v core (`useBuiltinCACertBundle()`) |
| Stahování | vlastní FreeRTOS task; `loop()` a LVGL na síť nikdy nečekají |
| Chyby | stará data zůstanou a zešednou; krátká chyba a stáří dat na displeji, detail v `/api/status` |

## 1. Zjištění

### 1.1 Server

| Co | Hodnota |
| --- | --- |
| Controller | `IotDashboardController.GetPoirotComputers` (TfsAppServer), `[AllowAnonymous]` + kontrola klíče `ClientApiKeys:IotDashboard` |
| URL | `https://<api-host>/<Environment>/IotDashboard/PoirotComputers` (API běží v IIS jako sub-aplikace prostředí) |
| Hlavička | `X-Api-Key: <klíč>`; chybějící nebo špatný klíč = `401` (`application/problem+json`) |
| Odpověď | `200`, `application/json`, ~60 B |
| Prostředí | klíč je **jiný v každém prostředí** (Key Vault). Endpoint je zatím nasazen jen v jednom prostředí, jinde vrací `404`. |
| Pozor | adresa **SPA** (frontend) vypadá podobně, ale vrací `200 text/html`. Firmware to musí poznat (kap. 4.3). |
| Proxy | server je za CDN (Cloudflare); vydavatel certifikátu se může měnit, proto žádné pevné vložení jednoho kořenového CA |

Kontrakt (`IotDashboardPoirotComputersDto`, camelCase):

```json
{ "total": 42, "waiting": 0, "running": 42, "warning": 0, "error": 0 }
```

- `total` = `waiting + running + warning + error`. Aktivní počítače ve stavu Unknown se nepočítají.
- Počítají se jen aktivní počítače (`Active = true`).

Ověření z PC:

```powershell
curl.exe -i -H "X-Api-Key: <klíč>" https://<api-host>/<Environment>/IotDashboard/PoirotComputers
```

### 1.2 Firmware dnes (v0.1.0)

| Co | Stav |
| --- | --- |
| `Settings` | jedna `apiUrl` (max. 200 znaků) a `apiIntervalSec` (výchozí 30) v NVS, web je umí uložit |
| `Net_GetString` | neumí vlastní hlavičky; TLS přes `setInsecure()` (bez ověření) |
| Stahování | **žádné**, nic API nevolá |
| Dashboard | prstenec „pipelines / 24h“ Succeeded/Failed/Running/Canceled z `DashData_Test()` |
| `Status` | slot `ST_API` existuje, `/api/status` ho vrací v poli `api`; není thread-safe |

### 1.3 TLS v core esp32 3.3.12

- `NetworkClientSecure::useBuiltinCACertBundle()` připojí balík kořenových CA z ESP-IDF
  (`CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL`, max. 200 certifikátů). Balík je už v knihovnách core,
  takže nic dalšího se nevkládá.
- `CONFIG_MBEDTLS_HAVE_TIME_DATE` je vypnuté: platnost certifikátu podle data se nekontroluje.
  Ověření tedy funguje hned po startu, ještě před nastavením hodin (SNTP / hlavička `Date`).
- Řetěz ověřuje podpis a jméno serveru (SNI / CN), takže API klíč nejde odposlechnout podvrženým serverem.

## 2. Přepínání prostředí – zvážené varianty

| Varianta | + | − |
| --- | --- | --- |
| A. Jedna celá URL + klíč | skoro hotové | přepnutí = přepsat URL i klíč |
| B. Base URL + klíč, cestu doplní firmware | jedna adresa i pro budoucí další endpointy | přepnutí pořád = přepsat obojí |
| **C. Profily (3 sloty) + výběr aktivního** | přepnutí jedním výběrem, klíč zůstane u profilu; vzor existuje (Wi-Fi sloty) | víc kódu ve webu a `Settings` |

**Vybráno C** (s base URL podle B). Přepínání na displeji (ťuknutím na název v liště) se **nedělá**.

## 3. Ruční obnovení – zvážené varianty

| Způsob | Rozhodnutí |
| --- | --- |
| **Ikona ⟳ v liště** vpravo vedle času, při stahování se točí | **ano** |
| Swipe dolů (pull-to-refresh) | ne |
| Ťuknutí na prstenec | ne (náhodné doteky) |
| Tlačítko ve webu | ne (web má tlačítko **Test** u profilu, kap. 6) |

Pravidla: ruční obnovení během běžícího stahování se ignoruje. Po každém stahování (automatickém
i ručním) se interval počítá znovu od jeho konce.

## 4. Návrh

### 4.1 Architektura

```
 loop() (Arduino task)          devops_api task (jádro 0)            LVGL task
 ─────────────────────          ─────────────────────────            ─────────
 WebConfig: uložení profilu ──► DevOpsApi_Configure(kopie)
 Ui: ⟳ (přes Ui_TakeRefresh) ─► DevOpsApi_RefreshNow()
                                 Net_GetString(url, hlavička)
                                 parse JSON → ApiSnapshot (lokálně)
                                 lock → s_data = kopie → unlock
                                                                     lv_timer 500 ms:
                                                                     DevOpsApi_Get() → když se
                                                                     změnila revize nebo stáří,
                                                                     ScreenDashboard_SetData()
```

Proč task: TLS handshake (až 8 s) + spojení (6 s) + odpověď (10 s) může trvat ~24 s, víc než
task watchdog `loop()` (20 s). Pravidla z [doporučení, kap. 4](DevOpsDashboard-doporuceni-LVGL.md#4-architektura-síť-ve-vlastním-tasku-ui-jen-pod-zámkem):

1. Task nesahá na LVGL ani na `Settings`; konfiguraci dostává jen jako kopii.
2. Během síťové operace se nedrží žádný zámek.
3. `Net_*` se volá jen z tohoto tasku (sdílený statický buffer v `Net.cpp`).
   `Net_SetPollFn(nullptr)`: task watchdog hlídá jen `loop()`, ne fetch task.
4. Stáří dat se počítá od posledního úspěchu, ne od posledního pokusu.
5. `Status` dostane mutex: `Status_Set()` volá fetch task i `Clock_NoteHttpDate()` (přes `Net.cpp`),
   `Status_Text()` volá web v `loop()`.
6. `Clock_NoteHttpDate()` volá `settimeofday()` z fetch tasku. To je thread-safe, `s_dateSet` je jen `bool`.

### 4.2 `Config.h`

```cpp
#define API_PROFILES           3
#define API_PROFILE_NAME_MAX   20
#define API_URL_MAX            200          // base URL
#define API_KEY_MAX            128
#define API_PATH_POIROT        "/IotDashboard/PoirotComputers"
#define API_KEY_HEADER         "X-Api-Key"  // ApiHeaderNames.ApiKey on the server
#define API_STALE_FACTOR       3            // data older than 3 intervals = stale
#define API_TASK_STACK         8192
```

### 4.3 `Net`

- `Net_GetString(url, out, tag, const char* hdrName = nullptr, const char* hdrValue = nullptr)`:
  hlavičku přidat přes `http.addHeader()` před `http.GET()`.
- `WiFiClientSecure`: místo `setInsecure()` volat `useBuiltinCACertBundle()`. `Net_GetBinary` a session
  se v tomto projektu nepoužívají; můžou zůstat, nebo je smazat.
- Vrátit HTTP kód volajícímu: nový výstupní parametr `int* httpCode` (nebo `Net_LastHttpCode()`).
  UI pak rozliší `401` / `404` / chybu sítě / chybu TLS.
- Klíč **nikdy nevypisovat** na sériovou linku. Dnešní výpis URL při chybě je v pořádku (URL klíč neobsahuje).
- Odpověď s `Content-Type` jiným než `application/json` označit za chybu „not JSON (wrong URL?)“.
  Takhle se pozná adresa SPA místo API.

### 4.4 `Settings`

```cpp
struct ApiProfile {                       // jen pro čtení zvenku
  char name[API_PROFILE_NAME_MAX + 1];
  char baseUrl[API_URL_MAX + 1];
  char key[API_KEY_MAX + 1];
};

const ApiProfile& Settings_ApiProfile(uint8_t slot);
uint8_t     Settings_ApiActive();                       // 0..API_PROFILES-1
void        Settings_SetApiActive(uint8_t slot);
// key == nullptr = ponechat uložený klíč; prázdné baseUrl = slot vymazat (i klíč).
// false = odmítnuto (URL nezačíná http:// / https://, příliš dlouhé hodnoty).
bool        Settings_SetApiProfile(uint8_t slot, const char* name, const char* baseUrl, const char* key);
uint16_t    Settings_ApiIntervalSec();                  // beze změny
```

- NVS klíče: `apN0..2` (název), `apU0..2` (base URL), `apK0..2` (klíč), `apAct` (aktivní).
- Base URL: ořezat mezery a koncová `/`. Když uživatel vloží celou URL končící na
  `/IotDashboard/PoirotComputers`, tuto cestu odříznout.
- **Migrace:** když existuje stará `apiUrl` a žádný profil, přesune se do profilu 0 (název „Default“,
  odříznout známou cestu) a klíč `apiUrl` se smaže.
- `Settings_ToJson`: `apiProfiles: [{name, baseUrl, hasKey}]`, `apiActive`, `apiIntervalSec`.
  **Bez klíče.**
- `Settings_ClearAll()` (tovární reset) maže i profily a klíče.

### 4.5 `DevOpsApi.h/.cpp` (nový)

```cpp
enum ApiState : uint8_t {
  API_OFF,          // aktivní profil nemá URL
  API_NO_KEY,       // URL je, klíč chybí - nic se nestahuje
  API_WAITING,      // čeká na Wi-Fi / první stažení
  API_OK,
  API_ERROR         // poslední pokus selhal (stará data zůstávají)
};

struct ApiSnapshot {
  uint16_t total, waiting, running, warning, error;
  bool     valid;            // aspoň jednou přišla data
  bool     busy;             // právě se stahuje (⟳ se točí)
  ApiState state;
  int      httpCode;         // poslední HTTP kód (0 = žádná odpověď)
  uint32_t lastOkMs;         // millis() posledního úspěchu
  uint32_t revision;         // mění se s každým pokusem a se změnou busy
  uint16_t intervalSec;
  char     profile[API_PROFILE_NAME_MAX + 1];
  char     error[48];        // krátký text pro displej a /api/status
};

void        DevOpsApi_Begin();                     // ze setup(), po Settings_Begin()
void        DevOpsApi_Configure(const char* name, const char* baseUrl,
                                const char* key, uint16_t intervalSec);   // z loop()
void        DevOpsApi_RefreshNow();                // odkudkoli; ignoruje se, když busy
ApiSnapshot DevOpsApi_Get();                       // odkudkoli, vrací kopii
```

- Změna konfigurace (jiný profil, URL, klíč) zahodí stará data (`valid = false`): data z jiného
  prostředí se nesmí ukazovat pod novým názvem. Pak stáhne hned.
- Parsování přes ArduinoJson do lokální struktury. Chybějící pole = chyba „bad data“, ne nula.
- Krátké chybové texty: `offline`, `401 bad API key`, `404 not found`, `HTTP 5xx`,
  `TLS / network error`, `not JSON (wrong URL?)`, `bad data`.
- `Status_Set(ST_API, ...)`: `OK 42 (DeveX)` / chybový text. Pro `/api/status`.

### 4.6 Data a displej

- `DashData.h`: stavy `DASH_RUNNING, DASH_WAITING, DASH_WARNING, DASH_ERROR` (pořadí výsečí po směru
  hodin od 12 h), názvy „Running / Waiting / Warning / Error“, `total` ze serveru. `DashData_Test()` smazat.
- `Theme.h`: `COL_RUNNING` zelená (dnešní `COL_OK`), `COL_WAITING` modrá (např. `0x42A5F5`),
  `COL_WARNING` žlutá (dnešní `COL_RUN`), `COL_ERROR` červená (dnešní `COL_FAIL`), `COL_STALE` šedá.
- `ScreenDashboard`:
  - popisek pod součtem „Poirot computers“
  - `SetData(const ApiSnapshot&)`: bez dat šedý prázdný prstenec a „-“; stará data (chyba nebo
    stáří > `API_STALE_FACTOR × interval`) se vykreslí šedě
  - pod legendou řádek stavu (Inter 20, šedě): „updated 12 s ago“ / „401 bad API key“ / „API not configured“
- `Ui` (lišta):
  - název aktivního profilu vedle názvu obrazovky, např. „DevOps dashboard · DeveX“
  - ikona ⟳ vlevo od času, klikací plocha aspoň 48×48 px. Při `busy` se točí (`lv_anim` rotace),
    bez profilu je skrytá. Ikona: `LV_SYMBOL_REFRESH` z Montserrat 24 (zapnout jen tento font v
    `lv_conf.h`), nebo doplnit glyf do Inter fontu v `tools/gen_fonts.cmd`.
  - klik → `atomic` příznak → `loop()` zavolá `DevOpsApi_RefreshNow()` (vzor `Ui_TakeScreenChange()`).
    `RefreshNow` je thread-safe, takže stačí volat přímo z LVGL eventu; příznak je jen pro jednotnost.
  - `lv_timer` 500 ms: `DevOpsApi_Get()`, při změně revize překreslit dashboard a ikonu,
    jednou za sekundu text stáří.

### 4.7 Web

- Sekce **DevOps API**:
  - výběr aktivního profilu (`<select>` s názvy)
  - 3 řádky profilu: název, base URL (placeholder `https://<api-host>/<Environment>`),
    klíč (`type=password`, placeholder „(stored)“, když `hasKey`)
  - interval [s]
  - **Save** a u každého profilu **Test**
  - nápověda: „Base URL of the API, without /IotDashboard/...“
- `POST /api/config`: `{apiProfiles:[{name, baseUrl, key?}], apiActive, apiIntervalSec}`.
  Chybějící nebo prázdný `key` = ponechat. Validace jako dnes, chyba `400 {"error":"apiProfile0"}`.
  Po uložení `loop()` zavolá `DevOpsApi_Configure(...)` s aktivním profilem.
- `POST /api/apitest {slot}`: `202`. Fetch task jednorázově zavolá endpoint daného profilu a výsledek
  (HTTP kód, text, data) vrátí `GET /api/apitest` → web se ptá každou sekundu, max. 30 s.
  Neblokuje web ani `loop()`. Snapshot dashboardu se tím nemění.
- `GET /api/status` → `api`: stav, profil, HTTP kód, stáří dat (`Status_Text(ST_API)`).
- Klíč neodchází ze zařízení nikdy: ani v `GET /api/config`, ani v logu, ani v `/api/status`.

## 5. Fáze

Číslování navazuje na [plán DevOpsDashboard43](DevOpsDashboard43-plan.md) (43.5 spolehlivost,
43.6 dokumentace). Commit po každé fázi.

### Fáze 43.7 – Net a Settings

1. `Net_GetString`: volitelná hlavička, HTTP kód ven, kontrola `Content-Type`,
   `useBuiltinCACertBundle()` místo `setInsecure()`.
2. `Settings`: profily, aktivní profil, migrace `apiUrl`, JSON bez klíčů, tovární reset.
3. `Status`: mutex.
4. Dočasný test ze `setup()` (sériová linka): jedno volání API s klíčem z aktivního profilu → očekávat `200` + JSON.
   Před commitem test odstranit.

**Commit:** `Phase 43.7: API key header, verified TLS and API profiles in settings`

### Fáze 43.8 – DevOpsApi task

1. `DevOpsApi.h/.cpp` podle kap. 4.5, `DevOpsApi_Begin()` v `setup()`, `Configure` po startu a po uložení ve webu.
2. `/api/status` → `api`.
3. Ověření na sériové lince: interval 30 s, výpadek Wi-Fi, špatný klíč (`401`), špatná URL (`404`, HTML).

**Commit:** `Phase 43.8: fetch Poirot computers from the REST API in a FreeRTOS task`

### Fáze 43.9 – UI

1. `DashData.h`, `Theme.h`, `ScreenDashboard` podle kap. 4.6.
2. Lišta: název profilu, ikona ⟳ s animací, ruční obnovení.
3. Řádek stavu (stáří / chyba), šedá stará data.

**Commit:** `Phase 43.9: dashboard shows Poirot computers, manual refresh in the status bar`

### Fáze 43.10 – Web

1. Sekce DevOps API s profily, výběrem aktivního a intervalem.
2. `POST/GET /api/apitest` a tlačítko Test.

**Commit:** `Phase 43.10: API profiles and connection test in the web UI`

### Fáze 43.11 – ověření a dokumentace

1. Testy na desce (kap. 7).
2. `FW_VERSION` → `0.2.0`, `CHANGELOG.md`, `README.md` (nastavení API, profily, ruční obnovení),
   tento plán (stav), [plán 43](DevOpsDashboard43-plan.md) kap. 7, [výukový průvodce REST API](DevOpsDashboard-REST-API.md)
   (autentizace už není „žádná“, nový kontrakt).

**Commit:** `Phase 43.11: REST API tests and docs`

## 6. Soubory

| Soubor | Změna |
| --- | --- |
| `Config.h` | konstanty kap. 4.2 |
| `Net.h/.cpp` | hlavička, HTTP kód, `Content-Type`, ověření TLS |
| `Settings.h/.cpp` | profily, migrace, JSON |
| `Status.cpp` | mutex |
| `DevOpsApi.h/.cpp` | **nový** – task, snapshot |
| `DashData.h` | stavy Poirot počítačů, bez testovacích dat |
| `Theme.h` | barvy stavů |
| `ScreenDashboard.h/.cpp` | `SetData(ApiSnapshot)`, popisek, řádek stavu, šedá data |
| `Ui.h/.cpp` | profil v liště, ⟳, `lv_timer` pro data |
| `lv_conf.h` / `tools/gen_fonts.cmd` | glyf ⟳ |
| `WebConfig.cpp`, `WebPage.h` | profily, test, status |
| `DevOpsDashboard43.ino` | `DevOpsApi_Begin()`, předání konfigurace, ruční obnovení |
| `Version.h`, `CHANGELOG.md`, `README.md`, `Doc/*` | verze a dokumentace |

## 7. Testy na desce

| Test | Očekávání |
| --- | --- |
| Profil s platnou URL a klíčem | do 2 s po startu Wi-Fi data, pak každých 30 s (log) |
| ⟳ v liště | ikona se točí, data se obnoví, interval se počítá znovu |
| ⟳ během stahování | ignoruje se, žádný druhý požadavek |
| Špatný klíč | „401 bad API key“ na displeji, prstenec bez dat |
| URL frontendu (SPA) | „not JSON (wrong URL?)“ |
| Prostředí bez endpointu | „404 not found“ |
| Přepnutí profilu ve webu | stará data zmizí, název v liště se změní, stáhne se hned |
| Výpadek Wi-Fi / serveru | data zešednou po 3 intervalech, po obnovení se vrátí |
| Test ve webu | ukáže HTTP kód a data, dashboard se nezmění |
| `GET /api/config` | žádný klíč, jen `hasKey` |
| Sériová linka | klíč se nikde nevypíše |
| Tovární reset | profily i klíče smazané |
| Paměť | `/api/status`: interní RAM po hodině stabilní, TLS handshake projde (heap guard) |
| 24h běh | žádný restart watchdogem |

## 8. Rizika

| Riziko | Opatření |
| --- | --- |
| Málo interní RAM na TLS (~45 kB) | heap guard v `Net` (`NET_MIN_HEAP`), sledovat `/api/status` |
| Balík CA neobsahuje CA, kterou CDN začne používat | balík obsahuje všechny běžné veřejné CA; při chybě TLS se ukáže „TLS / network error“, nouzově přepínač `setInsecure()` v `Config.h` |
| Souběh `Status` / `Net` ze dvou tasků | mutex v `Status`, `Net` jen z fetch tasku |
| Klíč v NVS je nešifrovaný | jako heslo Wi-Fi; NVS šifrování je mimo rozsah |
| Endpoint zatím jen v jednom prostředí | ostatní profily ukážou `404`, to je očekávané |

## 9. Mimo rozsah

- Přepínání profilu na displeji.
- Další endpointy (pipelines za 24 h atd.); base URL je na ně připravená.
- Šifrování NVS, ověření certifikátu pinováním.
