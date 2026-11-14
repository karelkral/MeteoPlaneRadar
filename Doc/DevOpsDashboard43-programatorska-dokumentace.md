# DevOps Dashboard 4.3 – programátorská dokumentace

> Firmware: [DevOpsDashboard43](../DevOpsDashboard43) **0.1.0** pro Waveshare ESP32-S3-Touch-LCD-4.3
> (SKU 25948), Arduino + LVGL 9.6.
> Související dokumenty: [README firmwaru](../DevOpsDashboard43/README.md) (uživatel),
> [plán a průběh prací](DevOpsDashboard43-plan.md) (rozhodnutí, výsledky testů, otevřené problémy),
> [doporučení: LVGL v Arduinu](DevOpsDashboard-doporuceni-LVGL.md) (návrh napojení REST API).
>
> Dokument popisuje stav ke commitu `Phase 43.6` (14. 11. 2026). Při změně architektury
> ho aktualizujte spolu s kódem.

## Obsah

1. [Přehled](#1-přehled)
2. [Prostředí a build](#2-prostředí-a-build)
3. [Struktura projektu](#3-struktura-projektu)
4. [Architektura: tasky, jádra, zámky](#4-architektura-tasky-jádra-zámky)
5. [LvPort – displej, dotyk, LVGL](#5-lvport--displej-dotyk-lvgl)
6. [UI – rám, obrazovky, překryvy](#6-ui--rám-obrazovky-překryvy)
7. [Fonty](#7-fonty)
8. [Síť: Wi-Fi a portál](#8-síť-wi-fi-a-portál)
9. [Webový server a API](#9-webový-server-a-api)
10. [Aktualizace firmwaru (OTA)](#10-aktualizace-firmwaru-ota)
11. [Nastavení (NVS)](#11-nastavení-nvs)
12. [Čas](#12-čas)
13. [Spolehlivost: watchdogy a restart](#13-spolehlivost-watchdogy-a-restart)
14. [Konfigurace při překladu](#14-konfigurace-při-překladu)
15. [Diagnostika](#15-diagnostika)
16. [Postupy: jak přidat …](#16-postupy-jak-přidat-)
17. [Nástrahy a známé problémy](#17-nástrahy-a-známé-problémy)

---

## 1. Přehled

| | |
| --- | --- |
| Deska | ESP32-S3-WROOM-1 **N16R8** (16 MB flash, 8 MB OPI PSRAM), displej 4,3" 800×480 RGB565 (ST7262), dotyk GT911, expandér CH422G |
| Grafika | LVGL **9.6.0**, režim vykreslování **DIRECT**, 2 framebuffery v PSRAM |
| Ovladače | ESP32_Display_Panel 1.0.4 (Espressif) – deska se inicializuje z jeho vestavěné definice |
| Obrazovky | 0 = Dashboard (prstencový graf výsledků pipelines, **testovací data**), 1 = Clock |
| Web | portál s QR kódem (přístupový bod), stránka nastavení, `/api/*`, OTA `/update` |
| Licence | MIT; fonty Inter pod SIL OFL 1.1; knihovny MIT / Apache-2.0 (viz [LICENSE.txt](../DevOpsDashboard43/LICENSE.txt)) |

Projekt vznikl z [DevOpsDashboard](../DevOpsDashboard) (kulaté desky, Arduino_GFX). Moduly nezávislé
na displeji (Wi-Fi, web, nastavení, čas, síť) jsou **kopie** s malými úpravami; vše kolem displeje
je nové. Projekty nesdílejí kód – oprava v jednom se do druhého musí přenést ručně.

## 2. Prostředí a build

### 2.1 Verze (připnuté v [sketch.yaml](../DevOpsDashboard43/sketch.yaml))

| Balík | Verze | Poznámka |
| --- | --- | --- |
| esp32 core | 3.3.12 | obsahuje desku `waveshare_esp32_s3_touch_lcd_43` |
| lvgl | 9.6.0 | |
| ESP32_Display_Panel | 1.0.4 | vyžaduje core ≥ 3.1.0 |
| ESP32_IO_Expander | 1.1.1 | |
| esp-lib-utils | **0.2.3** | **ne 0.3.x** – Display_Panel vyžaduje `>=0.2.0 && <0.3.0` |
| ArduinoJson | 7.4.3 | |

FQBN: `esp32:esp32:waveshare_esp32_s3_touch_lcd_43:PSRAM=enabled,FlashSize=16M,FlashMode=qio,PartitionScheme=custom`
(USB CDC On Boot zůstává vypnuté – log jde přes převodník CH343).

### 2.2 Build vždy přes profil

```powershell
DevOpsDashboard43\build.cmd          # překlad do DevOpsDashboard43\build\
DevOpsDashboard43\build.cmd COM9     # překlad + nahrání přes konektor "USB TO UART"
```

[build.cmd](../DevOpsDashboard43/build.cmd) volá `arduino-cli compile --profile default`. Profil si core
i knihovny rozbalí do `%LOCALAPPDATA%\Arduino15\internal\` a nezávisí na globální instalaci.
Na vývojovém PC je globální toolchain core 3.3.12 poškozený (složka s `as.exe` je nečitelná,
build mimo profil končí `cannot execute 'as'`) – detaily a oprava v
[plánu, fáze 0](DevOpsDashboard43-plan.md#výsledky-fáze-0-14-11-2026).

- První build ~15 min (instalace core + celé LVGL), další 1–10 min podle změn.
- Nová `--build-path` = celý LVGL znovu. Pro varianty s jinými `-D` (viz kap. 14) počítejte s plným
  překladem.
- Výstupy: `DevOpsDashboard43.ino.bin` (OTA), `DevOpsDashboard43.ino.merged.bin` (celá flash).

### 2.3 Konfigurační hlavičky knihoven ve složce sketche

`lv_conf.h` a `esp_panel_board_supported_conf.h` leží **ve složce sketche**. Core přidává
`-I{build.source.path}` do překladu i knihovnám a obě knihovny hledají konfiguraci přes
`__has_include`. Nic se nekopíruje do `libraries\`, nic se nepředává přes `--build-property`.

### 2.4 Nahrání a log

- Port: konektor **USB TO UART** (CH343, na vývojovém PC COM9), 115 200 Bd.
- Reset z PC: RTS (používá ho arduino-cli i esptool). Čtení logu bez resetu: otevřít port s
  `DTR = RTS = false`.

## 3. Struktura projektu

| Soubor | Vrstva | Obsah |
| --- | --- | --- |
| [DevOpsDashboard43.ino](../DevOpsDashboard43/DevOpsDashboard43.ino) | aplikace | `setup()`, `loop()`, tovární reset, hlídání displeje, obsluha přepnutí obrazovek |
| [LvPort.h](../DevOpsDashboard43/LvPort.h) / [.cpp](../DevOpsDashboard43/LvPort.cpp) | displej | deska, LVGL, task, zámek, flush, dotyk, podsvícení, heartbeat, čistý restart |
| [Ui.h](../DevOpsDashboard43/Ui.h) / [.cpp](../DevOpsDashboard43/Ui.cpp) | UI | lišta, `lv_tileview`, překryvy (portál, zprávy), stmívání; vláknově bezpečné `Ui_*` |
| [ScreenDashboard.h](../DevOpsDashboard43/ScreenDashboard.h) / [.cpp](../DevOpsDashboard43/ScreenDashboard.cpp) | UI | prstencový graf + legenda |
| [ScreenClock.h](../DevOpsDashboard43/ScreenClock.h) / [.cpp](../DevOpsDashboard43/ScreenClock.cpp) | UI | velké hodiny a datum |
| [Theme.h](../DevOpsDashboard43/Theme.h) | UI | rozměry (800×480, lišta 48 px) a barvy |
| [DashData.h](../DevOpsDashboard43/DashData.h) | data | struktura dat dashboardu, testovací data |
| [LocalTime.h](../DevOpsDashboard43/LocalTime.h) | data | místní čas nebo „neznámý“ (pro UI) |
| [src/fonts/](../DevOpsDashboard43/src/fonts) | UI | generované fonty Inter + `Fonts.h` + `OFL.txt` |
| [WiFiPortal.h](../DevOpsDashboard43/WiFiPortal.h) / [.cpp](../DevOpsDashboard43/WiFiPortal.cpp) | síť | připojení k uloženým sítím, přístupový bod, přepínání sítí |
| [WebConfig.h](../DevOpsDashboard43/WebConfig.h) / [.cpp](../DevOpsDashboard43/WebConfig.cpp), [WebPage.h](../DevOpsDashboard43/WebPage.h) | síť | HTTP server, `/api/*`, captive portal DNS, OTA |
| [Net.h](../DevOpsDashboard43/Net.h) / [.cpp](../DevOpsDashboard43/Net.cpp), [NetSink.h](../DevOpsDashboard43/NetSink.h) / [.cpp](../DevOpsDashboard43/NetSink.cpp) | síť | HTTP(S) GET klient pro budoucí REST API (zatím nevolaný) |
| [Settings.h](../DevOpsDashboard43/Settings.h) / [.cpp](../DevOpsDashboard43/Settings.cpp) | služby | nastavení v RAM + NVS, JSON pro web |
| [Clock.h](../DevOpsDashboard43/Clock.h) / [.cpp](../DevOpsDashboard43/Clock.cpp), [TimeUtil.h](../DevOpsDashboard43/TimeUtil.h) | služby | SNTP + záložně hlavička HTTP `Date` |
| [Status.h](../DevOpsDashboard43/Status.h) / [.cpp](../DevOpsDashboard43/Status.cpp) | služby | stavové poznámky pro `/api/status` |
| [Watchdog.h](../DevOpsDashboard43/Watchdog.h) / [.cpp](../DevOpsDashboard43/Watchdog.cpp) | služby | task watchdog pro `loop()` |
| [Config.h](../DevOpsDashboard43/Config.h), [Version.h](../DevOpsDashboard43/Version.h) | konfigurace | konstanty při překladu, verze firmwaru |
| [lv_conf.h](../DevOpsDashboard43/lv_conf.h), [esp_panel_board_supported_conf.h](../DevOpsDashboard43/esp_panel_board_supported_conf.h) | konfigurace | LVGL, volba desky |
| [partitions.csv](../DevOpsDashboard43/partitions.csv) | konfigurace | 16 MB: NVS 20 kB, 2× aplikace 6 MB (OTA), SPIFFS, coredump |
| [tools/gen_fonts.cmd](../DevOpsDashboard43/tools/gen_fonts.cmd) | nástroje | generování fontů |

Závislosti (zjednodušeně): `.ino` → `LvPort`, `Ui`, `WiFiPortal`, `WebConfig`, `Settings`, `Clock`,
`Watchdog`. `Ui` → `LvPort`, obrazovky. `WiFiPortal` → `Ui`, `WebConfig`, `Settings`.
`WebConfig` → `Ui`, `LvPort`, `Settings`, `Clock`, `Status`. Obrazovky nevolají nic mimo LVGL
a datové hlavičky.

## 4. Architektura: tasky, jádra, zámky

```
 jádro 0 (typicky)                jádro 1
 ───────────────────────          ─────────────────────────────────────────────────────
 Wi-Fi task ESP-IDF, lwIP         "lvgl" task (priorita 2, LvPort)
 SNTP callback                      loop: lock → lv_timer_handler() → unlock → heartbeat
                                    ├─ kreslení do skrytého framebufferu (DIRECT)
                                    ├─ flush: přepnutí bufferu, čekání na konec snímku
                                    ├─ čtení dotyku GT911 (I2C)
                                    └─ lv_timer: hodiny (1 s), lišta (1 s)
                                  Arduino loopTask (priorita 1)
                                    loop(): displayWatchdog, WiFi_Loop, WebConfig_Loop,
                                            obsluha obrazovek, Clock_Tick, Settings_Tick,
                                            Watchdog_Feed
 přerušení: konec snímku RGB      → frameDoneIsr: čítač snímků + notifikace LVGL tasku
```

### 4.1 Pravidla (porušení = náhodné pády)

1. **LVGL není vláknově bezpečné.** Volat LVGL smí jen kód, který drží zámek
   `LvPort_Lock()` / `LvPort_Unlock()` (rekurzivní mutex), nebo `LvLock` (RAII):
   ```cpp
   { LvLock lock; lv_label_set_text(label, "..."); }
   ```
   Callbacky událostí widgetů a `lv_timer` běží v LVGL tasku **se zámkem** – tam se nezamyká.
2. **`loop()` nekreslí přímo.** UI mění jen přes funkce `Ui_*`, které si zámek berou samy.
3. **LVGL task nesahá na `Settings`, síť ani web.** Když potřebuje něco předat, nastaví
   atomický příznak, který si `loop()` vyzvedne. Vzor: `Ui_TakeScreenChange()` (swipe →
   `std::atomic<int>` → `loop()` uloží do `Settings`).
4. **Nedržet zámek LVGL během blokující operace** (síť, Wi-Fi scan, `delay`). Výjimka: OTA,
   kde je zastavení kreslení záměr (kap. 10).
5. **Dotyk a expandér sdílí I2C sběrnici.** Dotyk čte LVGL task, proto `LvPort_SetBacklight()`
   (expandér CH422G) bere zámek LVGL.
6. **Dlouhé blokování `loop()`** (připojování k Wi-Fi až 12 s na síť) musí krmit
   `Watchdog_Feed()` – kód ve `WiFiPortal` to dělá.

## 5. LvPort – displej, dotyk, LVGL

Soubor [LvPort.cpp](../DevOpsDashboard43/LvPort.cpp) je vlastní kód (MIT) podle vzoru adaptéru
z příkladu Waveshare `10_lvgl_v9_demo`, ale s jiným režimem vykreslování.

### 5.1 Inicializace (`LvPort_Begin`)

1. `new Board()` + `init()` – ESP32_Display_Panel načte definici
   `BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_4_3` (piny RGB, časování 16 MHz, GT911, CH422G,
   podsvícení přes EXIO2).
2. Před `begin()`: `configFrameBufferNumber(2)` a bounce buffer 10 řádků
   (`configRGB_BounceBufferSize(800 × 10)`) v interní RAM – RGB periferie pak čte PSRAM po
   blocích přes procesor, obraz je stabilnější při zátěži PSRAM.
3. `begin()` – zapne panel, expandér, dotyk, podsvícení.
4. LVGL: `lv_init()`, tick z `millis()` (`lv_tick_set_cb`), displej 800×480 **RGB565**
   (`lv_display_set_color_format` **před** `lv_display_set_buffers`), buffery = oba framebuffery
   panelu, velikost `800 × 480 × 2` B, režim `LV_DISPLAY_RENDER_MODE_DIRECT`.
5. Vstup: `LV_INDEV_TYPE_POINTER`, čtení 1 bodu z GT911.
6. `attachRefreshFinishCallback(frameDoneIsr)` a vytvoření tasku `lvgl` (12 kB zásobník,
   priorita 2, jádro 1).

### 5.2 Proč DIRECT a jak funguje flush

| Režim | Výsledek na desce |
| --- | --- |
| FULL (adaptér Waveshare) | 9 FPS, 97 % CPU – každá změna překreslí celých 800×480 v PSRAM |
| **DIRECT** (tento projekt) | **30 FPS, ~50 % CPU**, render 2–3 ms na snímek animace |

V režimu DIRECT kreslí LVGL přímo do skrytého framebufferu a jen do změněných oblastí. Oblasti
změněné v minulém snímku si LVGL samo dokopíruje do druhého bufferu (sync areas).

```cpp
static void flushCb(lv_display_t* disp, const lv_area_t*, uint8_t* pxMap) {
  if (lv_display_flush_is_last(disp) && xTaskGetCurrentTaskHandle() == s_task) {
    s_lcd->switchFrameBufferTo(pxMap);           // ukázat dokreslený buffer
    ulTaskNotifyTake(pdTRUE, 0);                 // zahodit staré notifikace
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)); // počkat na konec snímku (~26 ms)
  }
  lv_display_flush_ready(disp);
}
```

Čekání na konec snímku zaručí, že panel už nečte buffer, do kterého LVGL začne kreslit – obraz
se netrhá. Strop 30 FPS dává `LV_DEF_REFR_PERIOD` = 33 ms v `lv_conf.h`.

### 5.3 Veřejné API

| Funkce | Účel |
| --- | --- |
| `LvPort_Begin()` | vše výše; `false` = displej nepoužitelný (`.ino` pak restartuje) |
| `LvPort_Lock(timeoutMs = -1)` / `LvPort_Unlock()`, `LvLock` | zámek LVGL (rekurzivní) |
| `LvPort_SetBacklight(bool)` | podsvícení jen zap/vyp (deska jinak neumí) |
| `LvPort_LastBeatMs()`, `LvPort_BeatAgeMs()` | heartbeat LVGL tasku; `BeatAgeMs` je bezpečné proti souběhu (nikdy nepřeteče) |
| `LvPort_FrameCount()` | počet snímků poslaných panelu (~39/s při 16 MHz) |
| `LvPort_Display()` | `lv_display_t*` |
| `LvPort_Restart()` | čistý restart (kap. 13.2) |

## 6. UI – rám, obrazovky, překryvy

### 6.1 Vrstvy

```
lv_layer_top()
  ├─ s_dim       stmívací vrstva (černá, průhlednost podle jasu, neklikací)   ← nejvýš
  └─ s_overlay   překryv přes celou obrazovku (klikací = pohltí dotyky)
       ├─ s_portal   QR kód + instrukce (Ui_ShowPortal)
       └─ s_msg      zpráva + detail (Ui_ShowMessage)
lv_screen_active()
  ├─ lišta (48 px): název obrazovky, tečky, čas
  └─ s_tv  lv_tileview 800×432
       ├─ dlaždice 0 (LV_DIR_RIGHT)  ScreenDashboard
       └─ dlaždice 1 (LV_DIR_LEFT)   ScreenClock
```

### 6.2 Funkce `Ui_*` (vláknově bezpečné)

| Funkce | Volá | Poznámka |
| --- | --- | --- |
| `Ui_Begin(screen, jas)` | `setup()` | vytvoří vše, nastaví testovací data dashboardu |
| `Ui_SetBrightness(pct)` | web, OTA | 0 = podsvícení vyp; 1–100 = stmívací vrstva, minimum 10 % |
| `Ui_ShowScreen(i, anim)` | `loop()` (požadavek z webu) | |
| `Ui_CurrentScreen()` | web, `loop()` | |
| `Ui_TakeScreenChange()` | `loop()` | index po swipe (−1 = nic); `loop()` ho uloží do `Settings` |
| `Ui_ShowPortal(ssid, pass, ip)` | `WiFiPortal` | QR ve formátu `WIFI:T:nopass;S:...;;` (speciální znaky escapované) |
| `Ui_ShowMessage(title, detail)` | `WiFiPortal`, OTA | |
| `Ui_HideOverlay()` | `WiFiPortal`, OTA | zpět na obrazovky |

### 6.3 Obrazovky

- **Dashboard** ([ScreenDashboard.cpp](../DevOpsDashboard43/ScreenDashboard.cpp)): jedna `lv_arc` na
  výseč (rotace 270° = začátek nahoře, indikátor bez zaoblení, šířka = R_out − R_in = 60 px),
  mezera 1°. Úhly jsou celé stupně (`LV_USE_FLOAT 0`); zaokrouhluje se kumulativní úhel, takže
  výseče vždy dají 360°. Procenta se ukážou jen tam, kde se text vejde (měřeno
  `lv_text_get_size`). Bez dat šedý prstenec a „-“.
- **Clock** ([ScreenClock.cpp](../DevOpsDashboard43/ScreenClock.cpp)): `lv_timer` 1 s, popisky mění
  jen při změně minuty (překreslení 160px číslic je drahé). Bez platného času „--:--“.

## 7. Fonty

| Font | Řez | Znaky | Použití |
| --- | --- | --- | --- |
| `inter_20` | Regular | ASCII 0x20–0x7E, `°`, `·`, `–` | výchozí font LVGL (`LV_FONT_DEFAULT`), drobné popisky |
| `inter_24` | Regular | totéž | lišta, portál |
| `inter_32` | Regular | totéž | legenda, datum, nadpisy |
| `inter_sb_24` | SemiBold | `0–9 %` | procenta na prstenci |
| `inter_sb_72` | SemiBold | `0–9 -` | součet uprostřed prstence |
| `inter_sb_160` | SemiBold | `0–9 : -` | hodiny |

- Generuje [tools/gen_fonts.cmd](../DevOpsDashboard43/tools/gen_fonts.cmd): stáhne Inter 4.1 a
  `lv_font_conv@1.5.3` (Node.js), 4 bpp, bez komprese. Spouští se v pracovní složce, aby hlavičky
  generovaných souborů neobsahovaly lokální cesty.
- **Číselné fonty mají jen vypsané znaky.** Text se znakem mimo sadu se vykreslí bez něj.
- Vestavěný Montserrat je vypnutý; LVGL symboly (`LV_SYMBOL_*`) proto **nejsou k dispozici**.
  Pokud budou potřeba, přidejte do generování druhý `--font` s FontAwesome a rozsahem symbolů.

## 8. Síť: Wi-Fi a portál

[WiFiPortal.cpp](../DevOpsDashboard43/WiFiPortal.cpp) – stavový automat běžící v `loop()`:

```
 start ── uložená síť? ── ano ─► connectAny(): scan, uložené sítě v dosahu od nejsilnější,
   │                                pak ty, které scan neviděl (skryté SSID); 12 s na síť
   │                                ├─ úspěch → STA, WebConfig_Begin(false), mDNS
   │                                └─ neúspěch ┐
   └──────────────────────── ne ───────────────┴─► startAP(): scan (do cache) → AP+STA
                                                     "DevOpsDashboard43", captive DNS,
                                                     Ui_ShowPortal
 AP: POST /api/wifi → Settings_SetWifi (první místo) → příznak → WiFi_Loop: tryConnect (20 s)
                                                     ├─ úspěch → STA, Ui_HideOverlay
                                                     └─ neúspěch → znovu AP
 STA: výpadek → každých 15 s WiFi.reconnect(); po 60 s zkusit ostatní uložené sítě v dosahu
```

- `WiFi.setSleep(false)` – bez úsporného režimu rádia (kap. 10, OTA).
- **Scan v režimu AP odpojí telefon** (rádio přeladí kanály) a požadavek z portálu spadne
  („Scan failed: failed to fetch“). Proto se síť nasnímá **před** spuštěním AP
  (`WebConfig_ScanNow`), AP běží jako AP+STA a `/api/scan` v AP vrací uložený seznam a jen
  spustí asynchronní scan na pozadí (výsledek vyzvedne `WebConfig_Loop`).
- `Net` / `NetSink` (HTTP GET, kontrola interní RAM pro TLS, hlavička `Date` → `Clock`) jsou
  připravené pro REST API, zatím je nic nevolá.

## 9. Webový server a API

[WebConfig.cpp](../DevOpsDashboard43/WebConfig.cpp), `WebServer` z core, port 80, obsluha v `loop()`.
Handlery se registrují jednou; při přechodu AP → STA se jen přepne DNS / mDNS.

| Endpoint | Metoda | Popis |
| --- | --- | --- |
| `/` | GET | stránka nastavení ([WebPage.h](../DevOpsDashboard43/WebPage.h)) |
| `/api/status` | GET | `version`, `board`, `ip`, `ssid`, `rssi`, `uptime`, `heap`, `heapLargest`, `psram`, `lvglAgeMs`, `frames`, `resetReason`, `app`, `time`, `screen`, `api`, `ntp` |
| `/api/config` | GET / POST | `apiUrl`, `apiIntervalSec`, `brightness`; heslo `password` + `newPassword`; GET navíc seznam sítí (bez hesel) |
| `/api/screen` | POST | `{"index":n}` nebo `{"step":±1}` – jen fronta, provede `loop()` |
| `/api/scan` | GET | sítě v dosahu (v AP z cache) |
| `/api/wifi` | POST | `{"ssid","pass"}` – uložit a připojit |
| `/api/wifilist` | POST | uložit až 3 sítě bez připojení (prázdné heslo = ponechat) |
| `/api/reboot` | POST | restart po odeslání odpovědi |
| `/api/reset` | POST | tovární reset (s heslem, je-li nastavené) |
| `/update` | GET / POST | stránka a příjem firmwaru (HTTP Basic `admin`, je-li heslo) |

- **Handlery nekreslí a nemění obrazovku** – nastaví frontu (`WebConfig_TakeScreen*`), kterou
  zpracuje `loop()`. Výjimka: `Ui_SetBrightness` a `Ui_*` v OTA (berou zámek samy).
- V režimu AP odpovídá DNS na vše IP adresou zařízení a neznámé URL přesměruje na `/` →
  telefon sám nabídne přihlášení k síti.
- Heslo správce je uložené v otevřené podobě (ochrana proti domácnosti, ne proti útočníkovi).

## 10. Aktualizace firmwaru (OTA)

Tok v `handleUpdateUpload()` (vše běží v `loop()`, uvnitř jednoho `handleClient()`):

| Fáze | Co se děje |
| --- | --- |
| `UPLOAD_FILE_START` | `otaStart()`: zpráva „Updating firmware...“, 1,5 s čekání (LVGL ji vykreslí), **`loop()` vezme zámek LVGL** (kreslení stojí), podsvícení vyp; `Update.begin(UPDATE_SIZE_UNKNOWN)` |
| `UPLOAD_FILE_WRITE` | `Update.write()` po ~1,4 kB, `Watchdog_Feed()`, log průběhu po 128 kB |
| `UPLOAD_FILE_END` | `Update.end(true)` → `otaEnd(true)`: uvolnit zámek, „Done, restarting...“, jas zpět |
| `UPLOAD_FILE_ABORTED` | log, `Update.abort()`, `otaEnd(false)` – zařízení běží dál na staré verzi |
| po těle požadavku | `200 OK` → 1,5 s → `LvPort_Restart()`; jinak `500` s důvodem |

- **Podsvícení musí být při zápisu vypnuté.** Zápis do flash vypíná cache, RGB periferie
  nedostává data z PSRAM a obraz viditelně poskakuje (ověřeno).
- `displayWatchdog()` je během OTA a 10 s po ní pozastavený.
- **Otevřený problém:** upload se občas zastaví (hlavně první po nečinnosti zařízení) a
  `WebServer` ho po 5 s bez dat ukončí. Měření a návrhy dalšího postupu:
  [plán, K ŘEŠENÍ](DevOpsDashboard43-plan.md#výsledky-fáze-5-14-11-2026).

## 11. Nastavení (NVS)

[Settings.cpp](../DevOpsDashboard43/Settings.cpp), jmenný prostor **`devops`** (stejný jako
DevOpsDashboard – jde o jiné zařízení, nevadí). Vše se drží v RAM; `Settings_*` nejsou vláknově
bezpečné → volat jen z `loop()` / `setup()` / web handlerů (vše stejný task).

| Klíč | Typ | Obsah | Zápis |
| --- | --- | --- | --- |
| `ssid`, `ssid1`, `ssid2` | string | uložené sítě | ihned |
| `wpass`, `wpass1`, `wpass2` | string | hesla | ihned |
| `bl` | u8 | jas 0–100 | odloženě (2 s) |
| `scr` | u8 | poslední obrazovka | odloženě (2 s) |
| `apiUrl` | string | URL REST API (`http(s)://`, max. 200 znaků) | ihned |
| `apiInt` | u16 | interval 5–3600 s | ihned |
| `pw` | string | heslo správce | ihned |

Odložený zápis (`Settings_Tick`) šetří flash při tahání posuvníku. Tovární reset
(`Settings_ClearAll`) smaže celý jmenný prostor.

## 12. Čas

- [Clock.cpp](../DevOpsDashboard43/Clock.cpp): `configTzTime(TZ_INFO, NTP...)` po startu Wi-Fi;
  záložně hlavička HTTP `Date` (jen dokud SNTP neodpovědělo – pro sítě, které blokují UDP 123).
- UI čte čas přes [LocalTime.h](../DevOpsDashboard43/LocalTime.h): `localtime_r`, čas před
  1. 1. 2025 se bere jako neznámý (ESP32 po startu počítá od 1970).
- Časové pásmo: `TZ_INFO` v [Config.h](../DevOpsDashboard43/Config.h) (POSIX, výchozí CET/CEST).

## 13. Spolehlivost: watchdogy a restart

### 13.1 Watchdogy

| Hlídá | Mechanismus | Reakce |
| --- | --- | --- |
| `loop()` | task watchdog ESP-IDF, `WDT_TIMEOUT_S` = 20 s, `Watchdog_Feed()` | panic → restart |
| LVGL task | `displayWatchdog()` v `loop()`: `LvPort_BeatAgeMs() ≥ 10 s` | `LvPort_Restart()` |
| RGB panel | `displayWatchdog()`: `LvPort_FrameCount()` se 3 s nezměnil | `LvPort_Restart()` |

Hlídání displeje je pozastavené během OTA a 10 s po ní. Ověřeno testovací variantou
`DOD43_WD_TEST=1` (kap. 14); větev „panel“ se uměle vyvolat nepodařilo.

### 13.2 Čistý restart

**Nevolejte `ESP.restart()` přímo.** S běžícím RGB panelem (DMA čte PSRAM) se restart na této
desce zasekne a zařízení po několika sekundách resetuje RTC watchdog (`resetReason` pak hlásí
`WATCHDOG`). `LvPort_Restart()` vezme zámek LVGL (max. 1 s), vypne podsvícení, smaže panel
(`LCD::del()`) a teprve pak restartuje.

### 13.3 Tovární reset tlačítkem

GPIO0 (BOOT) je zároveň datový vodič G3 displeje, takže se čte **jen v `setup()` před
inicializací displeje**: po RESETu je 1 s okno, BOOT se musí držet 3 s. Držení BOOT už při resetu
by spustilo bootloader ROM (stahovací režim), proto to okno.

## 14. Konfigurace při překladu

### 14.1 [Config.h](../DevOpsDashboard43/Config.h)

| Konstanta | Výchozí | Význam |
| --- | --- | --- |
| `AP_SSID`, `AP_PASSWORD` | `DevOpsDashboard43`, `""` | přístupový bod portálu |
| `WEB_HOSTNAME` | `devopsdashboard43` | mDNS |
| `TZ_INFO`, `NTP_SERVER_1/2` | CET/CEST, pool.ntp.org, time.google.com | čas |
| `BACKLIGHT_DEFAULT` | 80 | jas po továrním resetu |
| `BOOT_WINDOW_MS`, `BOOT_HOLD_MS` | 1000, 3000 | tovární reset |
| `WDT_TIMEOUT_S` | 20 | task watchdog |
| `DISPLAY_WD`, `LVGL_WD_STALL_MS`, `PANEL_WD_STALL_MS` | 1, 10000, 3000 | hlídání displeje |
| `WIFI_SLOTS`, `WIFI_TRY_MS`, `WIFI_SWITCH_AFTER_MS` | 3, 12000, 60000 | Wi-Fi |
| `API_*`, `NET_*` | | REST API / HTTP klient (pro budoucí použití) |

### 14.2 Přepínače přes `-D` (bez úpravy zdrojů)

```powershell
arduino-cli compile --profile default --build-path C:\Temp\b43x `
  --build-property "compiler.cpp.extra_flags=-DDOD43_PERF=2" `
  --build-property "compiler.c.extra_flags=-DDOD43_PERF=2" DevOpsDashboard43
```

| Makro | Hodnoty | Význam |
| --- | --- | --- |
| `DOD43_PERF` | 0 (výchozí), 1, 2 | monitor výkonu LVGL: 1 = FPS/CPU v rohu displeje, 2 = do sériového logu (`sysmon: ...`). Platí pro C i C++ – předat oběma. |
| `DOD43_WD_TEST` | 0 (výchozí), 1 | test hlídání: 60 s po startu `loop()` vezme zámek LVGL a nepustí → restart za 10 s. **Nikdy nenasazovat.** |

### 14.3 [lv_conf.h](../DevOpsDashboard43/lv_conf.h) – změny proti šabloně 9.6.0

`#if 1`; `LV_USE_STDLIB_MALLOC = LV_STDLIB_CLIB` (malloc core dává bloky ≥ 4 kB do PSRAM);
`LV_USE_LOG` + `LV_LOG_PRINTF` (varování do logu); `LV_USE_SYSMON` / `LV_USE_PERF_MONITOR`
podle `DOD43_PERF`; `LV_USE_QRCODE 1`; `LV_USE_CUSTOM_FONT_DEFAULT 1` + `LV_FONT_DEFAULT &inter_20`;
`LV_FONT_MONTSERRAT_14 0`; `LV_THEME_DEFAULT_DARK 1`. Ostatní hodnoty jsou ze šablony – při
upgradu LVGL vygenerujte `lv_conf.h` z nové šablony a změny přeneste (seznam je i v hlavičce
souboru).

## 15. Diagnostika

### 15.1 Sériový log (115 200 Bd)

| Zpráva | Význam |
| --- | --- |
| `Reset reason: ...` | důvod posledního resetu (`software` po OTA / restartu z webu) |
| `LvPort: 800x480, LVGL 9.6.0, DIRECT mode, ...` | displej běží |
| `[W][Panel] ... Unable to initialize the I2C address` | GT911 – neškodné, dotyk funguje |
| `WiFi: scan [...]` | sítě nasnímané před spuštěním AP |
| `Web: WiFi 'x' received`, `Settings: WiFi stored (n B)` | portál přijal síť a uložil ji |
| `WiFi ok (ssid), IP ...` / `connecting to ... failed` | výsledek připojení |
| `Clock: ... (NTP)` | první platný čas |
| `Screen: n` / `Screen: n (web)` | změna obrazovky (swipe / web) |
| `OTA: n kB in t ms (slowest write w ms)` | průběh OTA |
| `OTA: aborted after n kB ...` | přerušený upload |
| `DISPLAY WATCHDOG: ... - restarting` | zaseknutý LVGL task / panel |
| `Heap: internal ... PSRAM ...` | po startu a pak každých 10 min |

### 15.2 `/api/status` – orientační hodnoty v pořádku

| Pole | Typicky | Pozor když |
| --- | --- | --- |
| `heap` / `heapLargest` | ~180 / ~135 kB | pod ~60 kB (TLS pro HTTPS nepůjde) |
| `psram` | ~6,8 MB | trvale klesá (únik) |
| `lvglAgeMs` | < 50 | stovky ms a víc |
| `frames` | +39 za sekundu | neroste |
| `resetReason` | `power on`, `software` | `WATCHDOG`, `PANIC`, `BROWNOUT` |

### 15.3 Výkon

Varianta `DOD43_PERF=2` vypisuje každou sekundu
`sysmon: 30 FPS (...) refr 16ms (render 2ms | flush 12ms), CPU 50%`. Čas `flush` zahrnuje čekání
na konec snímku, takže „CPU“ je nadhodnocené – skutečná práce je `render`.

## 16. Postupy: jak přidat …

### 16.1 Novou obrazovku

1. `ScreenFoo.h/.cpp` s `void ScreenFoo_Create(lv_obj_t* tile)`; widgety vytvořit jednou,
   obnovovat z `lv_timer` (běží se zámkem).
2. [Ui.h](../DevOpsDashboard43/Ui.h): `#define UI_SCREEN_FOO 2`, `UI_SCREEN_COUNT 3`.
3. [Ui.cpp](../DevOpsDashboard43/Ui.cpp): název do `TITLES[]`; v `Ui_Begin` `addTile(...)` se
   směry – krajní dlaždice jen dovnitř (`LV_DIR_LEFT` / `LV_DIR_RIGHT`), prostřední `LV_DIR_HOR`;
   zavolat `ScreenFoo_Create`. Tečky v liště se přidají samy.
4. `Settings` i `/api/screen` kontrolují rozsah přes `UI_SCREEN_COUNT` – nic dalšího.
5. Pokud obrazovka sama ukazuje čas, skrýt čas v liště v `showActive()` (jako u Clock).

### 16.2 Napojení dat z REST API

1. Nový modul (např. `DevOpsApi`) s vlastním FreeRTOS taskem na jádře 0: stáhne JSON
   (`Net_GetString` – volat jen z jednoho tasku, sdílí buffer), naparsuje do lokální
   `DashData`, pod vlastním mutexem zkopíruje do sdílené kopie. Během stahování **žádný** zámek.
2. URL a interval předávat do tasku jako kopii (z `loop()` po uložení ve webu).
3. V `ScreenDashboard` `lv_timer` (např. 500 ms) vezme kopii a při změně revize zavolá
   `ScreenDashboard_SetData()`.
4. Stav do `Status_Set(ST_API, ...)` – zobrazí se v `/api/status` a na webu.

Podrobný návrh: [doporučení LVGL, kap. 4](DevOpsDashboard-doporuceni-LVGL.md#4-architektura-síť-ve-vlastním-tasku-ui-jen-pod-zámkem).

### 16.3 Nový font nebo znaky

Přidat řádek do [tools/gen_fonts.cmd](../DevOpsDashboard43/tools/gen_fonts.cmd), spustit ho,
deklarovat v [src/fonts/Fonts.h](../DevOpsDashboard43/src/fonts/Fonts.h) (`LV_FONT_DECLARE`).
Soubory v `src/` Arduino přeloží automaticky. Zdrojový soubor fontu 4 bpp s ASCII má ~55 kB
(20 px) až ~110 kB (32 px); ve flash zabere zhruba třetinu.

### 16.4 Nový endpoint

Handler v `WebConfig.cpp` + registrace v `WebConfig_Begin` (blok `s_srv.on(...)`, registruje se
jen jednou). Handler běží v `loop()`: smí volat `Settings` a `Ui_*`, nesmí volat LVGL přímo.

## 17. Nástrahy a známé problémy

| Téma | Detail |
| --- | --- |
| OTA se občas přeruší | otevřený problém, viz kap. 10 a plán |
| `ESP.restart()` | zasekne se → vždy `LvPort_Restart()` (kap. 13.2) |
| Zápis do flash | ruší obraz jen při dlouhém zápisu (OTA); krátké zápisy NVS bez viditelné změny |
| GPIO0 / BOOT | datový vodič displeje, číst jen před `LvPort_Begin()` |
| CH422G EXIO5 | přepíná nativní USB / CAN; ESP32_Display_Panel ho nastavuje sám, log jde přes CH343 |
| Jas | panel umí jen zap/vyp – jas je černá vrstva; skutečný příkon se nemění (kromě 0 %) |
| LVGL 9.6 API | `lv_obj_add_flag` / `remove_flag` / `has_flag` jsou deprecated (varování v logu) → `lv_obj_set_hidden`, `lv_obj_set_clickable`, `lv_obj_set_scrollable`, …; hlavičky v `include/lvgl/`, `#include <lvgl.h>` stačí |
| `sizeof(lv_color_t)` | v LVGL 9 jsou to 3 B – velikost bufferu RGB565 počítat `× 2`, ne `× sizeof(lv_color_t)` (chyba v adaptéru Waveshare) |
| `lv_demo_widgets` | v Arduinu nejde (složka `demos/` je mimo `src/`) |
| `esp-lib-utils` | aktualizace na 0.3.x rozbije build |
| Scan v AP | odpojí klienta – viz kap. 8 |
| mDNS | `devopsdashboard43.local` z některých PC nefunguje – použít IP z logu nebo z routeru |
