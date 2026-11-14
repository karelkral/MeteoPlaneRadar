# DevOps Dashboard 4.3 – plán: Waveshare ESP32-S3-Touch-LCD-4.3 + LVGL 9

> Související dokumenty: [plán původního DevOps dashboardu](DevOpsDashboard-plan.md),
> [stav a další kroky](DevOpsDashboard-dalsi-kroky.md),
> [doporučení: LVGL v Arduinu](DevOpsDashboard-doporuceni-LVGL.md),
> [podpora dalších desek](DevOpsDashboard-dalsi-desky.md),
> [výukový průvodce REST API](DevOpsDashboard-REST-API.md),
> [plán napojení REST API](DevOpsDashboard43-REST-API-plan.md),
> [README firmwaru 2.1](../DevOpsDashboard/README.md).
>
> Stav: **fáze 0–5 hotové**, firmware 0.1.0 běží na desce ([README](../DevOpsDashboard43/README.md),
> [programátorská dokumentace](DevOpsDashboard43-programatorska-dokumentace.md)).
> **Otevřené:** OTA se občas přeruší ([K ŘEŠENÍ](#výsledky-fáze-5-14-11-2026)), neověřený výpadek
> Wi-Fi a 24h běh. Plán vznikl 14. 11. 2026.

Nový experimentální projekt `DevOpsDashboard43` pro desku **Waveshare ESP32-S3-Touch-LCD-4.3**
(SKU 25948, 800×480). Grafiku obstará **LVGL 9**. Web, Wi-Fi portál a nastavení se převezmou
z [DevOpsDashboard](../DevOpsDashboard). Kompatibilita s kulatými deskami 2.1 / 2.8C se
**nepožaduje**. REST API zatím není: dashboard ukazuje testovací data.

## 0. Shrnutí rozhodnutí

| Oblast | Rozhodnutí |
| --- | --- |
| Umístění | nová složka `DevOpsDashboard43\` vedle `DevOpsDashboard\`, ta zůstane beze změny |
| Prostředí | Arduino (arduino-cli z Arduino IDE), esp32 core **3.3.12**, build **jen přes profil** `sketch.yaml` ([proč](#výsledky-fáze-0-14-11-2026)) |
| Grafika | **LVGL 9.6.0** (nejnovější), displej a dotyk obslouží **ESP32_Display_Panel 1.0.4**, režim vykreslování **DIRECT** se 2 framebuffery |
| Převzetí kódu | **kopie** modulů z `DevOpsDashboard` (nezávislý experiment, žádná společná knihovna) |
| Obrazovky | 2, přepínání **swipem**: **Dashboard** a **Hodiny**; aktivní obrazovku ukazují tečky |
| Dashboard | jen **prstencový graf** jako minule: výsledky pipelines za 24 h, součet uprostřed, procenta na výsečích, legenda s počty. **Testovací data.** |
| Písmo | **Inter** (jako minule), převedené nástrojem `lv_font_conv` do fontů LVGL |
| Jas | **softwarové stmívání** poloprůhlednou černou vrstvou; 0 % = podsvícení vypnout (deska umí podsvícení jen zapnout / vypnout) |
| Web | převzít: Wi-Fi portál s QR kódem, stránka nastavení, `/api/*`, OTA `/update` |
| Jazyk | jen angličtina (displej i web), stejně jako minule |
| Názvy | AP `DevOpsDashboard43`, mDNS `devopsdashboard43.local`, aby nekolidovaly s kulatou deskou |
| Záloha továrního firmwaru | **ne** |
| Git | commit po každé fázi (`Phase 43.N: ...`) |

## 1. Hardware a prostředí (ověřeno)

| Položka | Hodnota | Jak zjištěno |
| --- | --- | --- |
| Port | **COM9** = CH343 („USB TO UART“, VID 1A86 / PID 55D3) | `Get-PnpDevice` |
| Čip | ESP32-S3 (QFN56) rev. v0.2, 240 MHz, MAC 14:c1:9f:35:b3:70 | `esptool flash-id` |
| Paměť | **16 MB flash** (quad), **8 MB PSRAM** (OPI) = N16R8 | `esptool flash-id` |
| arduino-cli | 1.5.1, přibalené k Arduino IDE | `arduino-cli version` |
| Core | esp32 **3.3.12**, obsahuje desku *Waveshare ESP32-S3-Touch-LCD-4.3* | `arduino-cli core list`, `boards.txt` |
| sdkconfig core | `CONFIG_LCD_RGB_RESTART_IN_VSYNC=1`, XIP z PSRAM **vypnuté** | `esp32s3-libs\3.3.12\*\sdkconfig.h` |
| Globální toolchain 3.3.12 | **poškozený**: složka `Arduino15\packages\esp32\tools\esp-x32\2601\xtensa-esp-elf\bin` (obsahuje `as.exe`) je pro Windows „poškozená a nečitelná“ | build skončil chybou `cannot execute 'as'` |

Parametry displeje, dotyku a expandéru CH422G jsou v
[podpoře dalších desek, kap. 3](DevOpsDashboard-dalsi-desky.md#3-waveshare-esp32-s3-touch-lcd-43-sku-25948).
S ESP32_Display_Panel se ručně nezadávají, protože knihovna obsahuje definici desky
`BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_4_3`: ST7262 na 16 MHz, GT911, CH422G a podsvícení přes EXIO2.

**FQBN:**

```
esp32:esp32:waveshare_esp32_s3_touch_lcd_43:PSRAM=enabled,FlashSize=16M,FlashMode=qio,PartitionScheme=custom
```

*USB CDC On Boot* zůstane *Disabled* (výchozí). Sériový výpis i nahrávání jdou přes COM9.

## 2. Knihovny

| Knihovna | Verze | Poznámka |
| --- | --- | --- |
| `lvgl` | **9.6.0** | nejnovější v Arduino indexu. Waveshare ověřil 9.5.0, ta slouží jako záloha. |
| `ESP32_Display_Panel` | **1.0.4** | Espressif, vyžaduje core ≥ 3.1.0 |
| `ESP32_IO_Expander` | 1.1.1 | závislost (CH422G) |
| `esp-lib-utils` | **0.2.x** | závislost. **0.3.0 nejde:** ESP32_Display_Panel 1.0.4 vyžaduje `>=0.2.0 && <0.3.0`. |
| `ArduinoJson` | 7.4.3 | už nainstalováno, používá web |

`GFX Library for Arduino` a `PNGdec` nový projekt nepotřebuje.

**Výchozí příklad:** `waveshareteam/ESP32-S3-Touch-LCD-4.3`, složka
`examples/Arduino/examples/10_lvgl_v9_demo`. Obsahuje vlastní adaptér `esp_lv_adapter_arduino.cpp`
(LVGL task, rekurzivní mutex, tick přes `esp_timer`, 2 framebuffery v PSRAM, režim FULL s přepnutím
bufferu ve VSYNC, bounce buffer 10 řádků). Espressif má oficiální port jen pro LVGL 8, proto
napíšeme vlastní port podle tohoto vzoru.

## 3. Architektura

```
 LVGL task (jádro 1, priorita 2)            loop() (Arduino task)
 ───────────────────────────────            ─────────────────────
 lock → lv_timer_handler() → unlock         WiFi, WebConfig, Settings_Tick,
   ├─ kreslení do 2 FB v PSRAM (FULL/DIRECT)  Watchdog_Feed, hlídání heartbeatu LVGL
   ├─ dotyk GT911 → swipe v lv_tileview     └─ nic nekreslí; změny UI jen přes Ui_*()
   └─ lv_timer 1 s: hodiny, heartbeat           (Ui_* si zámek LVGL vezmou samy)
```

Pravidla z [doporučení, kap. 4](DevOpsDashboard-doporuceni-LVGL.md#4-architektura-síť-ve-vlastním-tasku-ui-jen-pod-zámkem):
LVGL se volá jen z LVGL tasku nebo pod `LvPort_Lock()`. `Settings` se mění jen z `loop()`.
Web nikdy nekreslí přímo.

### Soubory nového sketche

| Soubor | Původ | Obsah |
| --- | --- | --- |
| `DevOpsDashboard43.ino` | nový | `setup()`: Board → LvPort → UI → Settings/WiFi/Web; `loop()` bez kreslení |
| `LvPort.h/.cpp` | nový (vzor Waveshare adaptér) | init desky (ESP32_Display_Panel), LVGL task, zámek, flush, dotyk, podsvícení |
| `lv_conf.h` | nový (šablona LVGL 9.6) | RGB565, paměť LVGL v PSRAM, potřebné widgety, Inter fonty |
| `esp_panel_board_supported_conf.h` | z příkladu | `BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_4_3` |
| `Ui.h/.cpp` | nový | tileview, horní lišta, tečky, stmívací vrstva, obrazovky portálu a OTA, rozhraní `Ui_*()` |
| `ScreenDashboard.h/.cpp` | přepis | prstencový graf z `lv_arc`, součet, procenta, legenda |
| `ScreenClock.h/.cpp` | přepis `ScreenBlank` | velký čas a datum |
| `DashData.h` | nový | struktura dat grafu (dnes testovací data, později z REST API) |
| `src/fonts/inter_*.c`, `OFL.txt` | nové, generované | fonty Inter pro LVGL + licence OFL 1.1 |
| `tools/gen_fonts.cmd` | nový | reprodukovatelné generování fontů (`npx lv_font_conv`) |
| `Settings`, `WiFiPortal`, `WebConfig`, `WebPage.h`, `Net`, `NetSink`, `Clock`, `TimeUtil`, `Status`, `Watchdog`, `Version.h`, `Config.h` | kopie | kreslení nahradit voláním `Ui_*()`; zbytek beze změny |
| `partitions.csv` | kopie | 16 MB, 2× 6 MB aplikace (OTA) |
| `sketch.yaml`, `build.cmd`, `README.md`, `CHANGELOG.md`, `LICENSE.txt`, `.gitignore` | kopie / úprava | nový FQBN, port COM9, připnuté verze, build přes `--profile` |

**Nepřevádí se:** `Display_ST7701`, `Canvas16`, `AAFont`, `ClockFonts.h`, `Layout`, `UI`, `Board`,
`TCA9554`, `Touch*`, `qrcode.c/.h`. Nahradí je ESP32_Display_Panel a LVGL.

## 4. Rozvržení obrazovek (800×480, na šířku)

```
 0 ┌────────────────────────────────────────────────────────────────────┐
   │ DevOps dashboard               ● ○                  ▂▄▆ 12:34      │ lišta 0–48
48 ├────────────────────────────────────────────────────────────────────┤
   │            ▄▄████▄▄                                                │
   │         ▄██▀ 74 % ▀██▄              ● Succeeded     42             │
   │        ██▀          ▀██             ● Failed         7             │
   │       ██      57      ██            ● Running        5             │
   │        ██▄ pipelines ▄██            ● Canceled       3             │
   │         ▀██▄  / 24h ▄██▀                                           │
   │            ▀▀████▀▀                                                │
480└────────────────────────────────────────────────────────────────────┘
      prstenec: střed (230, 264), R 175 / 115     legenda: x 470–770
```

- **Prstenec:** jedna `lv_arc` na výseč (indikátor bez knoflíku, bez zaoblení, šířka = R_out − R_in).
  Výseče začínají nahoře, jdou po směru hodin a odděluje je mezera 0,8°. Procenta (Inter 24) jsou
  uprostřed výseče, jen když se tam vejdou. Algoritmus i barvy (zelená / červená / žlutá / šedá) se
  převezmou z [ScreenDashboard.cpp](../DevOpsDashboard/ScreenDashboard.cpp).
- **Střed:** součet (Inter 72) a pod ním „pipelines / 24h“ (Inter 20, šedě).
- **Legenda:** tečka v barvě výseče, název a počet (Inter 32).
- **Hodiny:** čas HH:MM (Inter 160, jen číslice a dvojtečka) a pod ním datum (Inter 32).
- **Lišta:** název obrazovky vlevo, tečky uprostřed, síla Wi-Fi a čas vpravo (Inter 24).
- **Stmívání:** objekt přes celou obrazovku na `lv_layer_top()`, černý, neklikatelný,
  `opa = 255 × (100 − jas) / 100`, spodní mez jasu 10 %. Při 0 % se podsvícení vypne.

Velikosti fontů jsou orientační. Doladí se na desce, každý font stojí flash (4 bpp; Inter 160 jen
s 11 znaky vyjde asi na 40 kB).

## 5. Fáze

### Fáze 0 – prostředí a ověření hardwaru (bez vlastního kódu)

1. Nainstalovat knihovny:
   `arduino-cli lib install "esp-lib-utils@0.2.3" "ESP32_IO_Expander@1.1.1" "ESP32_Display_Panel@1.0.4" "lvgl@9.6.0"`
2. Stáhnout `10_lvgl_v9_demo` do dočasné složky mimo repozitář, přeložit s FQBN z kap. 1 a nahrát na COM9.
3. Ověřit: obraz bez posunu a blikání, dotyk, výpis na COM9, volnou interní RAM a PSRAM.
4. Zjistit, kde musí ležet `lv_conf.h` a `esp_panel_board_supported_conf.h`, aby je viděly knihovny.
5. Při potížích s LVGL 9.6.0 zopakovat s 9.5.0 a výsledek zapsat sem.

**Výstup:** ověřená sada verzí. Nic se necommituje kromě případné opravy tohoto plánu.

#### Výsledky fáze 0 (14. 11. 2026)

| Bod | Výsledek |
| --- | --- |
| Verze | **core 3.3.12 + lvgl 9.6.0 + ESP32_Display_Panel 1.0.4 + ESP32_IO_Expander 1.1.1 + esp-lib-utils 0.2.3 fungují.** Záloha 9.5.0 není potřeba. |
| Build | Globální toolchain je poškozený (kap. 1), a proto build s globální instalací skončil `cannot execute 'as'`. **Build přes profil** (`arduino-cli compile --profile default`) si core i knihovny rozbalí do `Arduino15\internal\`, kde je toolchain `esp-x32 2601` v pořádku. Proto se bude **vždy překládat přes profil**. První build trval 14 min (instalace core), další asi 1 min. |
| Konfigurace | `lv_conf.h` i `esp_panel_board_supported_conf.h` stačí dát **do složky sketche**. Core přidává `-I{build.source.path}` i knihovnám a LVGL i ESP32_Display_Panel konfiguraci hledají přes `__has_include`. `--build-property` ani kopie do `libraries\` nejsou potřeba. |
| Inicializace desky | `Board (Waveshare:ESP32-S3-Touch-LCD-4.3)` – init i begin OK, CH422G OK, GT911 OK (varování „Unable to initialize the I2C address“ je neškodné, dotyk funguje) |
| Displej, barvy, dotyk | OK: pořadí barev sedí, souřadnice dotyku odpovídají |
| Výkon, režim **FULL** (adaptér Waveshare) | **9 FPS, 97 % CPU**, pohyb trhaný: každá změna překreslí celých 800×480 px v PSRAM |
| Výkon, režim **DIRECT** | **29 FPS, 57 % CPU**, plynulé, bez artefaktů. Strop 30 FPS dává `LV_DEF_REFR_PERIOD` = 33 ms. → **použít DIRECT** |
| Paměť (testovací obrazovka) | interní RAM volná ~286 kB (největší blok 241 kB), PSRAM volná 6,85 MB; 2 framebuffery = 1,5 MB v PSRAM |
| Velikost firmwaru | 715 kB (testovací sketch bez webu) |
| `lv_demo_widgets` | v Arduinu nejde: složka `demos/` leží mimo `src/` knihovny a nepřekládá se. Test proto běžel na vlastní testovací obrazovce. |
| Chyba v adaptéru Waveshare | velikost bufferu `width × height × sizeof(lv_color_t)`: `lv_color_t` má v LVGL 9 **3 B**, správně je 2 B (RGB565) |
| LVGL 9.6 – změny API | `lv_obj_add_flag` / `lv_obj_remove_flag` / `lv_obj_has_flag` jsou **deprecated** (varování v logu), používat settery a gettery `lv_obj_set_<flag>()` / `lv_obj_is_<flag>()`. Veřejné hlavičky se přesunuly do `include/lvgl/`, `#include <lvgl.h>` funguje dál. `LV_COLOR_DEPTH` v `lv_conf.h` už není, formát nastavuje `lv_display_set_color_format()`. |

Testovací sketch (adaptér Waveshare s režimem DIRECT, vlastní testovací obrazovka, `lv_conf.h`,
`sketch.yaml`) zůstal mimo repozitář v pracovní složce session. Na desce teď běží on.

**Pro Arduino IDE** (bez profilu) je nutné opravit globální instalaci:

1. jako správce spustit `chkdsk C: /f` (vyžaduje restart),
2. smazat `%LOCALAPPDATA%\Arduino15\packages\esp32\tools\esp-x32\2601`,
3. v Boards Manageru přeinstalovat *esp32 by Espressif* 3.3.12.

Bez toho nejspíš padá i `DevOpsDashboard\build.cmd`, protože profil nepoužívá a překládá globální
instalací (neověřeno).

### Fáze 1 – kostra sketche a port LVGL

1. Vytvořit `DevOpsDashboard43\`: `.ino`, `LvPort`, `lv_conf.h`, `esp_panel_board_supported_conf.h`,
   `sketch.yaml`, `build.cmd` (výchozí port COM9, `--profile default`), `partitions.csv`, `.gitignore`,
   `LICENSE.txt`.
2. `LvPort`: vlastní kód (MIT), vzor je adaptér Waveshare.
   - LVGL task na jádře 1 s rekurzivním mutexem, tick přes `esp_timer`
   - 2 framebuffery v PSRAM, bounce buffer 10 řádků, přepnutí bufferu ve VSYNC
   - režim **DIRECT** (ověřeno ve fázi 0) se správnou velikostí bufferu (2 B na pixel)
   - dotyk GT911 jako `LV_INDEV_TYPE_POINTER`
   - `LvPort_Lock()`, `LvPort_Unlock()`, `LvPort_SetBacklight(bool)`, heartbeat
   - volitelně vyzkoušet `LV_USE_OS LV_OS_FREERTOS` + `LV_DRAW_SW_DRAW_UNIT_CNT 2` (kreslení na obou jádrech)
3. Testovací obrazovka: text a tlačítko, ověřit dotyk.

**Commit:** `Phase 43.1: LVGL 9 port for Waveshare ESP32-S3-Touch-LCD-4.3`

### Fáze 2 – fonty Inter

1. `tools\gen_fonts.cmd`: stáhne Inter (OFL 1.1, <https://github.com/rsms/inter>) do dočasné složky a
   `npx lv_font_conv --bpp 4 --format lvgl --no-compress` vygeneruje `src/fonts/inter_*.c`:
   ASCII 0x20–0x7E + `°` pro běžné velikosti, jen `0-9:` pro hodiny.
2. Fonty zaregistrovat v `lv_conf.h` (`LV_FONT_CUSTOM_DECLARE`), vestavěné Montserrat vypnout
   (kromě `LV_FONT_DEFAULT` pro symboly, pokud bude potřeba).
3. Přidat `OFL.txt`.

**Commit:** `Phase 43.2: Inter fonts for LVGL`

### Fáze 3 – UI: lišta, swipe, dashboard, hodiny

1. `Ui`: `lv_tileview` se 2 dlaždicemi (Dashboard, Hodiny), lišta s tečkami a časem,
   stmívací vrstva.
2. `ScreenDashboard`: prstencový graf podle kap. 4, data z `DashData.h` (testovací data
   42 / 7 / 5 / 3 jako minule).
3. `ScreenClock`: čas a datum z `lv_timer` (1 s).
4. Bez Wi-Fi: čas zobrazit jako `--:--` (hodiny zatím nemají zdroj).

**Commit:** `Phase 43.3: dashboard ring chart and clock screen in LVGL`

### Fáze 4 – převzetí webu, portálu a nastavení

1. Zkopírovat moduly (kap. 3). V `Config.h` přejmenovat AP a mDNS na `DevOpsDashboard43` /
   `devopsdashboard43`, `User-Agent` na `DevOpsDashboard43/<verze>`.
2. Kreslení v [WiFiPortal.cpp](../DevOpsDashboard/WiFiPortal.cpp) a [WebConfig.cpp](../DevOpsDashboard/WebConfig.cpp)
   nahradit voláním `Ui_ShowPortal()` (QR kód přes `lv_qrcode`), `Ui_ShowConnecting(ssid)`,
   `Ui_ShowOta(stav)` a `Ui_ShowDashboard()`.
3. Jas: `Set_Backlight(pct)` → `Ui_SetBrightness(pct)` (stmívací vrstva, při 0 % vypnout podsvícení).
   Posuvník ve [WebPage.h](../DevOpsDashboard/WebPage.h) zůstane.
4. `/api/status`: `board` = `"Waveshare 4.3"`, doplnit volnou interní RAM a PSRAM.
5. Obnovit poslední obrazovku po restartu (`lv_tileview_set_tile_by_index`), index ukládat
   z `loop()`.
6. Tovární reset tlačítkem BOOT ponechat. GPIO0 je na desce zároveň datový bit G3 displeje, proto
   ho číst jen při startu, před inicializací displeje.

**Commit:** `Phase 43.4: WiFi portal, web config and settings from DevOpsDashboard`

### Fáze 5 – spolehlivost

1. Heartbeat: `lv_timer` zapisuje čas, `loop()` při zaseknutí LVGL (> 10 s) restartuje.
   Během OTA hlídání pozastavit.
2. OTA: zobrazit obrazovku „Updating firmware...“, pak pozastavit časovače LVGL až do restartu.
3. Ověřit chování displeje při zápisu do flash (uložení nastavení, OTA). XIP z PSRAM je v core
   vypnuté, takže krátké zrušení obrazu je možné. `CONFIG_LCD_RGB_RESTART_IN_VSYNC` ale zajistí,
   že se obraz sám srovná. Pokud by to vadilo: méně častý zápis NVS, nižší pixel clock.
4. Testy: swipe, stmívání, portál (QR), web a všechny `/api/*`, OTA, výpadek Wi-Fi, 24h běh.

**Commit:** `Phase 43.5: watchdogs, OTA handling and tests`

#### Výsledky fáze 5 (14. 11. 2026)

**5.1 Hlídání displeje – hotovo** (commit `Phase 43.5.1`)

- `loop()` restartuje desku přes `LvPort_Restart()`, když LVGL task 10 s nedokončil průchod
  nebo panel 3 s nedokončil snímek. Během OTA a 10 s po ní je hlídání pozastavené.
- `/api/status` hlásí `lvglAgeMs` (běžně do 30 ms) a `frames` (přibývá ~39 za sekundu).
- Ověřeno testovací variantou `DOD43_WD_TEST=1`: drží zámek LVGL, restart přišel po 10,0 s.
  V běžném provozu žádný falešný restart.
- Opravena chyba nalezená testem: stáří heartbeatu se počítalo bez znaménka a po startu
  přeteklo na 4294967295 ms, což vedlo k falešnému restartu. Nově `LvPort_BeatAgeMs()`.
- **Neověřeno:** větev „panel stopped scanning“. Zastavení RGB periferie se nepodařilo
  vyvolat uměle.

**5.2 OTA – částečně, otevřený problém** (commit `Phase 43.5.2`)

- Funguje: zpráva „Updating firmware...“ svítí 1,5 s, pak zhasne podsvícení, po zápisu se
  1,5 s ukáže „Done, restarting...“. Restart jde přes `LvPort_Restart()`, `/api/status` hlásí
  `app` (`app0`/`app1`).
- Podsvícení během zápisu **musí** zhasnout. Zkoušel jsem ho nechat zapnuté a obraz během
  zápisu do flash poskakoval všemi směry, blikal a třásl se.
- Přidáno `WiFi.setSleep(false)`. Bez něj selhaly 2 ze 4 OTA, s ním prošlo 14 z 15 OTA
  spuštěných krátce po restartu.
- Do logu přidán průběh: každých 128 kB, nejpomalejší `Update.write()` (~110 ms, mazání sektoru)
  a místo přerušení.

> **K ŘEŠENÍ – OTA se občas přeruší („Empty reply from server“ / „Connection was reset“).**
>
> | Pozorování | Hodnota |
> | --- | --- |
> | Příznak | upload se uprostřed zastaví, po 5 s bez dat ho `WebServer` ukončí (`HTTP_MAX_POST_WAIT`), deska zůstane na staré verzi a dál běží |
> | Log | `OTA: aborted after 415 kB, 24358 ms (no data for 5 s or client gone)` |
> | Kdy | **po nečinnosti**: první OTA po ~90 s klidu selhala, série OTA hned po restartu prochází (8/8) |
> | Bez `setSleep(false)` | 2 ze 4 selhaly (po 0,7 / 1,2 MB) |
> | Propustnost | kolísá 30–100 kB/s; `Update.write()` max. 110 ms, deska příjem nebrzdí |
> | Totéž | „Empty reply“ hlásil i DevOpsDashboard (core 3.3.10). MeteoPlaneRadar (core **3.0.7**) podle autora funguje, kód OTA má stejný. |
>
> Co zkusit dál:
> 1. Stejné měření (90 s klidu, pak OTA) s core 3.0.7, tedy oddělit vliv ESP-IDF 5.5 / lwIP od našeho kódu.
> 2. Za běhu ověřit, že `setSleep(false)` platí i po automatickém znovupřipojení (`esp_wifi_get_ps`).
> 3. Vyloučit RGB panel: během uploadu zastavit snímání (`esp_lcd_rgb_panel` – stop/restart), nebo
>    test na buildu bez displeje.
> 4. Přerušení po 5 s bez dat nastavit delší (`HTTP_MAX_POST_WAIT` v build flags), ať je vidět,
>    jestli se přenos sám rozběhne.
> 5. Z prohlížeče (`/update`) zkusit totéž – posílá jinak než `curl -F`.
>
> Do vyřešení: když OTA selže, zopakovat ji; deska zůstane funkční na staré verzi. Případně nahrát
> přes USB (`build.cmd COM9`).

**5.3 Testy na desce** (commit `Phase 43.5.3`)

| Test | Výsledek |
| --- | --- |
| Displej, barvy, dotyk, plynulost (30 FPS, DIRECT) | OK |
| Swipe Dashboard ↔ Clock, tečky, název v liště | OK |
| Portál: vzhled, QR kód (telefon nabídne připojení), hledání sítí, připojení | OK (po opravě skenování ve fázi 4) |
| Uložení Wi-Fi, automatické připojení po restartu, čas z NTP | OK |
| Web a `/api/status`, `/api/config`, `/api/screen`, `/api/reboot` | OK; restart hlásí `software` |
| Stmívání z webu 100 → 30 → 10 → 0 (podsvícení vypnuto) → 80 % | OK |
| Zápis nastavení do NVS (flash) – rušení obrazu | žádné viditelné |
| Přepnutí obrazovky z webu (s animací) | OK |
| Tovární reset (RESET, do 1 s držet BOOT 3 s) | OK – nastavení smazána, naběhl portál |
| Hlídání LVGL (testovací build) | OK – restart po 10 s |
| OTA | funguje, ale **občas se přeruší** – viz „K ŘEŠENÍ“ výše |
| Výpadek Wi-Fi (vypnutý router) | **neověřeno** |
| 24h běh | **neověřeno** |
| Hlídání panelu (zastavené snímání) | **neověřeno** – nelze uměle vyvolat |

### Fáze 6 – dokumentace

`DevOpsDashboard43\README.md` (build, první start, web, otestováno), `CHANGELOG.md`, aktualizace
tohoto plánu a [stavu a dalších kroků](DevOpsDashboard-dalsi-kroky.md).

**Commit:** `Phase 43.6: README and docs for DevOpsDashboard43`

## 6. Rizika

| Riziko | Dopad | Opatření |
| --- | --- | --- |
| LVGL 9.6.0 není ověřené Waveshare | nepřeloží se / chyby | **vyřešeno** ve fázi 0: funguje |
| `esp-lib-utils` 0.3.0 nainstalovaná jako závislost | nepřeloží se | připnout 0.2.3 v `sketch.yaml` |
| `lv_conf.h` a konfiguraci desky knihovny nevidí | nepřeloží se / špatná deska | **vyřešeno** ve fázi 0: stačí složka sketche |
| Poškozený globální toolchain 3.3.12 | build mimo profil padá | překládat jen přes profil; globální instalaci opravit (fáze 0) |
| Zrušení obrazu při zápisu do flash / Wi-Fi zátěži | krátké bliknutí | bounce buffer, restart ve VSYNC, méně zápisů NVS |
| Interní RAM (LVGL + Wi-Fi + bounce buffery) | pády, selhání HTTP | paměť LVGL a framebuffery v PSRAM, hlídat `/api/status` |
| Režim FULL překresluje celou obrazovku | 9 FPS, 97 % CPU | **vyřešeno** režimem DIRECT: 29 FPS, 57 % CPU |
| GPIO0 sdílený s displejem (G3) | rušení obrazu | tlačítko BOOT číst jen před inicializací displeje |
| EXIO5 na CH422G (USB / CAN) | odpojí nativní USB | nevadí: logy jdou přes CH343 (COM9); ESP32_Display_Panel nastavuje EXIO sama |

## 7. Mimo rozsah

- REST API: navazující krok, plán v [DevOpsDashboard43-REST-API-plan.md](DevOpsDashboard43-REST-API-plan.md)
  (fáze 43.7–43.11: profily prostředí, API klíč, fetch task, ruční obnovení).
- Další desky (7", 5", kulaté 2.1 / 2.8C). ESP32_Display_Panel je podporuje, takže je lze přidat později.

## 8. Zdroje

- Waveshare dokumentace 4.3 (Arduino): <https://docs.waveshare.com/ESP32-S3-Touch-LCD-4.3/Arduino>
- Waveshare příklady 4.3: <https://github.com/waveshareteam/ESP32-S3-Touch-LCD-4.3>
  (`examples/Arduino/examples/10_lvgl_v9_demo`)
- ESP32_Display_Panel: <https://github.com/esp-arduino-libs/ESP32_Display_Panel>
  (definice desky `src/board/supported/waveshare/BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_4_3.h`,
  návod `docs/envs/use_with_arduino.md`)
- LVGL 9: <https://docs.lvgl.io/master/>
- lv_font_conv: <https://github.com/lvgl/lv_font_conv>
- Inter: <https://github.com/rsms/inter> (SIL Open Font License 1.1)
