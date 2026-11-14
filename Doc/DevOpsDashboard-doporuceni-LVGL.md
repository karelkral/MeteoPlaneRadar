# DevOps Dashboard – doporučení: Waveshare 7" + LVGL v Arduinu

> Související dokumenty: [plán a dokumentace projektu](DevOpsDashboard-plan.md),
> [stav a další kroky](DevOpsDashboard-dalsi-kroky.md),
> [výukový průvodce REST API](DevOpsDashboard-REST-API.md),
> [podpora dalších desek](DevOpsDashboard-dalsi-desky.md),
> [README firmwaru](../DevOpsDashboard/README.md).
>
> Stav: **doporučení**, nic z toho zatím není implementováno. Vzniklo 14. 11. 2026 z analýzy
> projektu ClawPal (lokálně `F:\GitHub-KarelKral-Repos\ClawPal`, upstream
> <https://github.com/Kaayoos/clawpal>), který autorovi běží na desce Waveshare 7".

## 0. Shrnutí

| Oblast | Doporučení |
| --- | --- |
| Framework | Zůstat u **Arduina (C++)**. ESP-IDF ne: ClawPal je v C a autorovi je Arduino bližší. Arduino core 3.x stejně běží nad ESP-IDF 5.x, takže FreeRTOS tasky, `esp_lcd` i NVS jsou k dispozici i tak. |
| Cílová deska | **Waveshare ESP32-S3-Touch-LCD-7** (800×480). Běží na ní ClawPal (ESP-IDF + LVGL), takže kombinace RGB panel + LVGL + Wi-Fi + HTTPS je na tomto kusu hardwaru ověřená. |
| Grafika | **LVGL 9.5** přes knihovnu **`ESP32_Display_Panel`** (Espressif) místo Arduino_GFX + Canvas16/AAFont. |
| Projekt | **Nový sketch** pro 7". Z `DevOpsDashboard` převzít moduly nezávislé na desce ([kap. 5](#5-mapa-migrace-z-devopsdashboard)). |
| ClawPal | Jen **zdroj nápadů**. **Kód nekopírovat**: ClawPal je GPLv3, tento projekt je MIT. |
| REST API | Stahovat ve **vlastním FreeRTOS tasku**, výsledek držet v kopii pod mutexem, UI čte jen kopii ([kap. 4](#4-architektura-síť-ve-vlastním-tasku-ui-jen-pod-zámkem)). |

## 1. Co si z ClawPal vzít a co ne

ClawPal je „mazlíček“ na ESP32-S3 displeji a k němu aplikace pro Windows. Firmware je v C
(ESP-IDF 5.5.2, FreeRTOS, LVGL 8.4). Pro DevOps dashboard je cenný ve čtyřech věcech:

1. **Ověřené nastavení 7" displeje** ([kap. 2](#2-waveshare-esp32-s3-touch-lcd-7--hardware)).
   ClawPal vede profil 7" jako experimentální (jeho autoři ho na hardwaru nezkoušeli),
   ale autorovi tohoto projektu na jeho 7" běží.
2. **Oddělení sítě od UI.** Data stahuje samostatný task, UI dostává jen kopii a do LVGL se
   sahá jen pod zámkem.
3. **Stáří dat.** Výsledek nese čas posledního úspěchu a UI rozlišuje aktuální, zastaralá
   a vypršelá data.
4. **UI navržené na 800×480**, tedy na stejné rozlišení jako 7".

Co z ClawPal **nebrat**:

- ESP-IDF build a kód v C (GPLv3),
- párování přes HMAC, potvrzování akcí na displeji, podepsané OTA,
- WebSocket server pro PC a fonty pro 8 jazyků,
- strukturu UI: `buddy_ui.c` má 3 457 řádků v jednom souboru. Lepší je jedna obrazovka = jeden soubor.

## 2. Waveshare ESP32-S3-Touch-LCD-7 – hardware

S knihovnou `ESP32_Display_Panel` se nic z toho nezadává ručně, protože definici desky už
obsahuje. Tabulka slouží ke kontrole a ladění. Zdrojem je ClawPal `board_rgb.c` (větev
`CLAWPAL_DRIVER_WAVESHARE_RGB`), který vychází z Espressif ESP32_Display_Panel.

| Položka | Hodnota |
| --- | --- |
| Modul | ESP32-S3, 8 MB OPI PSRAM, flash **8 MB nebo 16 MB**. ClawPal má pro každou variantu vlastní profil. Zjistit z označení modulu (N8R8 / N16R8) nebo přes `esptool.py flash_id`. |
| Displej | 800×480, řadič ST7262, RGB565 16 bit, **bez inicializačních příkazů** |
| Časování | PCLK 16 MHz; HSYNC i VSYNC: pulse / back porch / front porch = 4 / 8 / 8; `pclk_active_neg = 1` |
| RGB signály | HSYNC 46, VSYNC 3, DE 5, PCLK 7 |
| Data D0–D15 | 14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40 |
| Framebuffery | 2× v PSRAM (2 × 750 kB), bounce buffer 10 řádků, přepnutí na konci snímku, takže se obraz netrhá |
| I2C | SDA 8, SCL 9, 400 kHz (dotyk i expandér) |
| Dotyk | GT911, INT na GPIO4; při resetu s INT v log. 0 se ozve na adrese 0x5D |
| Expandér CH422G | adresa 0x24 = režim (1 = výstupy zapnuté), 0x38 = hodnoty výstupů |
| EXIO1 | reset dotyku |
| EXIO2 | podsvícení – **jen zapnout / vypnout**, jas regulovat nejde (ClawPal stmívá softwarově vrstvou přes UI) |
| EXIO3 | reset LCD |
| EXIO5 | **musí zůstat 0**, jinak se nativní USB přepne na CAN. ClawPal pro 7" zapisuje `0x0E` (EXIO1–3 = 1, EXIO5 = 0). |

Piny i časování jsou stejné jako u Waveshare 4.3 (SKU 25948) z
[podpory dalších desek](DevOpsDashboard-dalsi-desky.md) a u 5". Jeden kód tak pokryje celou
řadu. Pozor: adaptér ClawPal pro **4.3B** (`board.c`) zapisuje `0x2E`, tedy EXIO5 = 1.

## 3. Knihovny a prostředí

Ověřeno v dokumentaci Waveshare: <https://docs.waveshare.com/ESP32-S3-Touch-LCD-7/Arduino>

| Co | Verze | Poznámka |
| --- | --- | --- |
| Deska v Arduino IDE | Waveshare ESP32-S3-Touch-LCD-7 | Waveshare uvádí esp32 core **3.0.7**, `DevOpsDashboard` pinuje 3.3.10. Při potížích s displejem nejdřív zkusit verzi core. |
| `ESP32_Display_Panel` | 0.1.4+ | od Espressifu, obsahuje definici desky 7" |
| `ESP32_IO_Expander` | 0.0.4+ | CH422G |
| `lvgl` | **9.5.0** (nebo 8.4.0) | MIT; ke každé verzi patří vlastní `lv_conf.h` z příkladů |
| Výchozí příklad | `10_lvgl_v9_demo` | v <https://github.com/waveshareteam/ESP32-S3-Touch-LCD-7>, knihovny jsou v `examples/Arduino/libraries` |

Postup: nejdřív nahrát `10_lvgl_v9_demo` **beze změn** a teprve pak do něj přidávat vlastní
kód. Ověřené verze core a knihoven potom zapsat do profilu v `sketch.yaml`, stejně jako dnes.

## 4. Architektura: síť ve vlastním tasku, UI jen pod zámkem

```
 fetchTask (jádro 0)                        LVGL task (port z příkladu)
 ───────────────────                        ───────────────────────────
 Net_GetString(url)                         lv_timer_handler(): kreslení, dotyk, gesta
 deserializeJson → ApiSnapshot (lokálně)         │
 lock → s_data = kopie → unlock                  └─ lv_timer (500 ms): DevOpsApi_Get()
                                                    → když se změnila revize nebo stáří,
                                                      aktualizuje widgety
 loop(): WiFi, WebConfig, Settings_Tick, Watchdog_Feed – nic nekreslí
```

Pravidla (převzatá z ClawPal `main.c` a `buddy_weather.c`):

1. **LVGL volat jen z LVGL kontextu.** Callbacky `lv_timer` a události widgetů už běží
   s drženým zámkem. Odkudkoli jinud (třeba z `loop()`) se volání LVGL obalí do
   `lvgl_port_lock()` / `lvgl_port_unlock()`. Přesný název je podle portu v příkladu.
2. **Síť nikdy nesahá na LVGL** ani na `Settings`, protože nejsou thread-safe. Task jen vyplní
   strukturu.
3. **Během síťové operace se nedrží žádný zámek.** Data se parsují do lokální kopie a zamyká
   se jen na zkopírování.
4. **`Net_GetString` volat jen z jednoho tasku**, protože používá sdílený statický buffer.
   V 7" sketchi nastavit `Net_SetPollFn(nullptr)`. Dnešní `netPoll()` volá `Watchdog_Feed()`
   a `touchPump()`, které patří do `loop()`. `loop()` už během stahování nestojí, takže je
   nepotřebuje.
5. **Konfigurace se do tasku předává jen jako kopie.** `WebConfig` (běží v `loop()`) po uložení
   URL nebo intervalu zavolá `DevOpsApi_Configure(...)`. Task si pod zámkem vezme kopii a
   stáhne data hned. Nahrazuje `DevOpsApi_Invalidate()`.
6. **Stáří dat počítat z času posledního úspěchu**, ne jen z výsledku posledního pokusu.
   Tím projde test „výpadek API → data zešednou, po obnovení se vrátí“.
7. **Do `Status` zapisovat opatrně.** `Net.cpp` nastavuje hodiny z hlavičky `Date` a ty volají
   `Status_Set()`. Z fetch tasku se tak `Status` mění souběžně s `/api/status`. Ověřit, že to
   nevadí, případně `Status` chránit mutexem.

### 4.1 Kostra `DevOpsApi` ve variantě s taskem

Navazuje na návrh v [REST-API, kap. 4](DevOpsDashboard-REST-API.md#4-co-dopsat-v-c-fáze-6).
`ApiItem`, `API_MAX_ITEMS`, `parseStatus()` a JSON kontrakt zůstávají. Mění se jen to, *kde*
stahování běží.

```cpp
// DevOpsApi.h (doplnění)
struct ApiSnapshot {
  char     title[24];
  ApiItem  items[API_MAX_ITEMS];
  uint8_t  count;
  bool     valid;          // aspoň jednou přišla data
  bool     lastFailed;     // poslední pokus selhal
  uint32_t lastOkMs;       // millis() posledního úspěchu
  uint32_t revision;       // mění se s každým pokusem
  uint16_t intervalSec;    // aktuální interval (pro výpočet stáří v UI)
  char     error[48];      // poslední chyba (pro /api/status)
};

void        DevOpsApi_Begin();                                          // ze setup()
void        DevOpsApi_Configure(const char* url, uint16_t intervalSec); // z loop() / WebConfig
ApiSnapshot DevOpsApi_Get();                                            // odkudkoli, vrací kopii
```

```cpp
// DevOpsApi.cpp (jádro)
static SemaphoreHandle_t s_lock;
static ApiSnapshot   s_data = {};          // chráněno s_lock
static char          s_url[256] = "";      // chráněno s_lock
static uint16_t      s_intervalSec = 30;   // chráněno s_lock
static volatile bool s_force = true;

static bool fetchAndParse(const char* url, ApiSnapshot& out) {
  String body;
  if (!Net_GetString(url, body, "API")) {
    strlcpy(out.error, "HTTP error", sizeof(out.error));
    return false;
  }
  JsonDocument doc;
  if (DeserializationError e = deserializeJson(doc, body)) {
    snprintf(out.error, sizeof(out.error), "Bad JSON: %s", e.c_str());
    return false;
  }
  strlcpy(out.title, doc["title"] | "", sizeof(out.title));
  for (JsonObjectConst it : doc["items"].as<JsonArrayConst>()) {
    if (out.count >= API_MAX_ITEMS) break;
    ApiItem& d = out.items[out.count++];
    strlcpy(d.name,  it["name"]  | "", sizeof(d.name));
    strlcpy(d.value, it["value"] | "", sizeof(d.value));
    d.status = parseStatus(it["status"] | "");
  }
  return true;
}

static void fetchTask(void*) {
  uint32_t nextMs = 0;
  for (;;) {
    char url[sizeof(s_url)];
    uint16_t interval;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    strlcpy(url, s_url, sizeof(url));
    interval = s_intervalSec;
    xSemaphoreGive(s_lock);

    if (url[0] && WiFi.isConnected() && (s_force || (int32_t)(millis() - nextMs) >= 0)) {
      s_force = false;
      ApiSnapshot next = {};
      const bool ok = fetchAndParse(url, next);   // bez zámku – může trvat sekundy
      xSemaphoreTake(s_lock, portMAX_DELAY);
      if (ok) {
        next.valid = true;
        next.lastOkMs = millis();
        next.revision = s_data.revision + 1;
        s_data = next;
      } else {                                    // stará data zůstanou, jen se označí
        s_data.lastFailed = true;
        strlcpy(s_data.error, next.error, sizeof(s_data.error));
        s_data.revision++;
      }
      xSemaphoreGive(s_lock);
      nextMs = millis() + interval * 1000UL;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void DevOpsApi_Begin() {
  s_lock = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(fetchTask, "devops_api", 8192, nullptr, 1, nullptr, 0);
}

void DevOpsApi_Configure(const char* url, uint16_t intervalSec) {
  xSemaphoreTake(s_lock, portMAX_DELAY);
  strlcpy(s_url, url ? url : "", sizeof(s_url));
  s_intervalSec = intervalSec;
  xSemaphoreGive(s_lock);
  s_force = true;
}

ApiSnapshot DevOpsApi_Get() {
  xSemaphoreTake(s_lock, portMAX_DELAY);
  ApiSnapshot copy = s_data;
  copy.intervalSec = s_intervalSec;
  xSemaphoreGive(s_lock);
  return copy;
}
```

### 4.2 Obrazovky v LVGL

```cpp
// Swipe mezi obrazovkami zajistí lv_tileview (náhrada touchPump / dispatchTouch).
lv_obj_t* tv    = lv_tileview_create(lv_screen_active());
lv_obj_t* dash  = lv_tileview_add_tile(tv, 0, 0, LV_DIR_RIGHT);
lv_obj_t* blank = lv_tileview_add_tile(tv, 1, 0, LV_DIR_LEFT);
ScreenDashboard_Create(dash);
ScreenBlank_Create(blank);
```

```cpp
// ScreenDashboard.cpp – obnova dat z lv_timer (běží v LVGL tasku, zámek už je držen)
static uint32_t s_seenRev   = 0;
static bool     s_seenStale = false;

static void refreshTimer(lv_timer_t*) {
  const ApiSnapshot d = DevOpsApi_Get();
  const bool stale = !d.valid || d.lastFailed ||
                     millis() - d.lastOkMs > 3UL * d.intervalSec * 1000UL;
  if (d.revision == s_seenRev && stale == s_seenStale) return;
  s_seenRev = d.revision;
  s_seenStale = stale;
  updateWidgets(d, stale);   // lv_label_set_text, barvy podle ApiStatus, šedě když stale
}

void ScreenDashboard_Create(lv_obj_t* tile) {
  // widgety vytvořit jednou (nadpis, seznam / tabulka položek, horní lišta s časem)
  lv_timer_create(refreshTimer, 500, nullptr);
}
```

Obnovení poslední obrazovky po restartu: při startu
`lv_tileview_set_tile_by_index(tv, Settings_Screen(), 0, LV_ANIM_OFF)`. V
`LV_EVENT_VALUE_CHANGED` jen uložit index do proměnné a `Settings_SetScreen()` zavolat
z `loop()`, protože `Settings` nejsou thread-safe.

## 5. Mapa migrace z `DevOpsDashboard`

| Modul | Osud v 7" sketchi |
| --- | --- |
| `WiFiPortal`, `WebConfig` + `WebPage.h`, `Settings`, `Net` + `NetSink`, `Watchdog`, `Status`, `Clock`, `TimeUtil`, `Version` | **převzít** (nezávislé na desce a ověřené) |
| `DevOpsDashboard.ino` | **nový**: `setup()` podle příkladu `10_lvgl_v9_demo` + inicializace převzatých modulů, `loop()` bez kreslení a bez gest |
| `Display_ST7701`, `Canvas16`, `AAFont`, `ClockFonts.h` | **nahradí** `ESP32_Display_Panel` + LVGL. Písmo z LVGL, např. Montserrat; projekt je jen anglicky, takže stačí vestavěná. |
| `Board` (autodetekce 2.1 / 2.8C), `TCA9554`, `Touch*` | **nahradí** definice desky a ovladače GT911 / CH422G z knihoven |
| `UI`, `Layout`, `ScreenDashboard`, `ScreenBlank` | **přepsat** jako LVGL obrazovky v `lv_tileview` |
| `qrcode.c` + `UI_DrawWifiQR` (QR v režimu AP) | nahradit widgetem `lv_qrcode` (`LV_USE_QRCODE` v `lv_conf.h`) |
| Jas ve `WebConfig` | 7" nemá regulaci podsvícení. Buď jen zapnout / vypnout, nebo softwarové stmívání poloprůhlednou černou vrstvou přes `lv_layer_top()`. |
| `displayWatchdog()` v `.ino` | nahradit heartbeatem: `lv_timer` zapisuje čas, `loop()` hlídá jeho stáří a při zaseknutí restartuje |

## 6. Na co si dát pozor

- **Verze core:** Waveshare deklaruje 3.0.7, projekt používá 3.3.10.
- **Varianta flash:** podle 8 / 16 MB nastavit *Flash Size* i `partitions.csv`. Dnešní tabulka
  počítá s 16 MB (dva aplikační sloty po 6 MB).
- **Interní RAM:** LVGL, Wi-Fi a TLS soupeří o interní RAM. Velké buffery patří do PSRAM a
  `Net_HeapOk()` dál hlídat. Pro API v LAN přes `http://` TLS potřeba není.
- **Nikdy nekreslit** z handlerů `WebConfig` ani z fetch tasku. Jen nastavit příznak nebo
  zavolat `DevOpsApi_Configure`.
- **OTA:** dnes `loop()` během OTA nic nekreslí, to zachovat (během OTA neměnit widgety a
  pozastavit `lv_timer`). Problém „Empty reply“ u `/update` řešit zvlášť.
- **Kulaté desky 2.1 / 2.8C** přidat později jako další cíl. Ověřit, jestli pro ně má
  `ESP32_Display_Panel` definici. Pokud ne, napsat vlastní konfiguraci podle dnešního
  ověřeného kódu (inicializace ST7701, CST820).

## 7. Doporučené pořadí kroků

1. Zjistit variantu flash (N8R8 / N16R8).
2. Nainstalovat knihovny podle [kap. 3](#3-knihovny-a-prostředí) a nahrát `10_lvgl_v9_demo`
   beze změn. Ověřit displej, dotyk a plynulost.
3. Nový sketch: z příkladu ponechat inicializaci displeje a LVGL portu, přidat `lv_tileview`
   se dvěma obrazovkami (Dashboard, Prázdná).
4. Přenést `Settings`, `WiFiPortal` (QR přes `lv_qrcode`), `WebConfig`, `Clock`, `Watchdog`
   a `Status`.
5. Přidat `DevOpsApi` ve variantě s taskem ([kap. 4.1](#41-kostra-devopsapi-ve-variantě-s-taskem))
   a widgety dashboardu podle JSON kontraktu.
6. Projít testy z [dalších kroků, krok 4](DevOpsDashboard-dalsi-kroky.md#krok-4--závěrečné-testy):
   24h běh, výpadek API, změna URL.
7. Zapsat verze core a knihoven do `sketch.yaml`.

## 8. Odkazy do ClawPal (jen ke čtení – GPLv3, nekopírovat)

Cesty jsou relativní k lokálnímu klonu vedle tohoto repozitáře
(`F:\GitHub-KarelKral-Repos\ClawPal`). Na GitHubu tyto odkazy nefungují, upstream je
<https://github.com/Kaayoos/clawpal>.

| Soubor | Co v něm je |
| --- | --- |
| [firmware/components/board/board_rgb.c](../../ClawPal/firmware/components/board/board_rgb.c) | ovladač RGB desek včetně 7" (větev `CLAWPAL_DRIVER_WAVESHARE_RGB`): piny, časování, CH422G, reset GT911 |
| [firmware/components/board/lvgl_port.c](../../ClawPal/firmware/components/board/lvgl_port.c) | LVGL task, zámek, 2 framebuffery, přepnutí na konci snímku |
| [firmware/main/weather/buddy_weather.c](../../ClawPal/firmware/main/weather/buddy_weather.c) | vzor „stahovací task + cache pod mutexem + stáří dat“ |
| [firmware/main/main.c](../../ClawPal/firmware/main/main.c) | fronta událostí ze sítě do UI, UI se mění jen pod zámkem |
| [firmware/boards/waveshare-esp32-s3-touch-lcd-7-16mb.h](../../ClawPal/firmware/boards/waveshare-esp32-s3-touch-lcd-7-16mb.h), […-7-8mb.h](../../ClawPal/firmware/boards/waveshare-esp32-s3-touch-lcd-7-8mb.h) | profily 7" (rozlišení, flash, rozložení OTA) |
| [boards/README.md](../../ClawPal/boards/README.md) | stav desek a zdroje pinoutů (Espressif ESP32_Display_Panel) |
| [tests/simulator/README.md](../../ClawPal/tests/simulator/README.md) | simulátor LVGL UI na Windows – nápad, jak ladit obrazovky bez flashování |
