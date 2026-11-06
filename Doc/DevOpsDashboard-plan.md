# DevOps Dashboard – plán přestavby z projektu MeteoPlaneRadar

> Cílová deska: **Waveshare ESP32-S3-Touch-LCD-2.1** (kulatý dotykový displej 480×480),
> zachovaná podpora i pro **ESP32-S3-Touch-LCD-2.8C**.
> Výchozí kód: tento repozitář (MeteoPlaneRadar v0.8.5, licence MIT).

Dokument má tři hlavní části:

1. **[Rychlokurz pro C# vývojáře](#1-rychlokurz-esp32--arduino-pro-c-vývojáře)** – jak číst tenhle typ kódu.
2. **[Dokumentace původního projektu](#2-dokumentace-původního-projektu-meteoplaneradar)** – co v projektu je, jak to funguje a co z toho zůstane.
3. **[Plán přestavby](#3-plán-přestavby-na-devopsdashboard)** – konkrétní kroky, jak se dostat k DevOps dashboardu se 2 obrazovkami.

---

## 0. Shrnutí rozhodnutí

| Oblast | Rozhodnutí |
| --- | --- |
| Umístění | Nová složka `DevOpsDashboard\` vedle originálu, originál zůstane beze změny |
| Desky | Obě – 2.1 (CST820) i 2.8C (GT911), včetně autodetekce při startu |
| Obrazovky | 2: **Dashboard** (zatím text „DevOps dashboard“) a **Prázdná** |
| Přepínání obrazovek | **Swipe vlevo/vpravo** (žádné automatické střídání, žádný dlouhý stisk) |
| WiFi | Zachovat portál: přístupový bod + QR kód + webový formulář, až 3 uložené sítě |
| Web zařízení | Malá nová stránka (WiFi, URL API, interval, jas, heslo) + `/api/status` + OTA `/update` |
| REST API | HTTP v lokální síti, bez autentizace; URL a interval se nastavují ve webu (uloženo v NVS) |
| Čas | Hodiny v horní liště, čas z **NTP** (+ záložně z hlavičky `Date` HTTP odpovědí) |
| Písmo | Zachovat vyhlazené písmo (AAFont), zredukovat na jeden typ písma (Inter) |
| Jazyk | **Jen angličtina** (displej i web) – modul `Lang` se odstraní |
| Noční režim | **Odstranit** (jas se nastavuje jen ručně ve webu) |
| Spolehlivost | Zachovat HW watchdog, hlídání displeje, kontrolu expandéru, tovární reset tlačítkem BOOT |

---

## 1. Rychlokurz ESP32 / Arduino pro C# vývojáře

### 1.1 Mapování pojmů

| C# / .NET | Arduino / ESP32 (tento projekt) |
| --- | --- |
| `Program.Main()` | `setup()` v `.ino` – zavolá se jednou po zapnutí |
| herní smyčka / `while(true)` | `loop()` – volá se pořád dokola, nikdy nekončí |
| `.csproj` + NuGet | `sketch.yaml` (verze core a knihoven) + Arduino Library Manager |
| `interface` / veřejné API třídy | soubor `.h` (hlavička) – deklarace funkcí |
| implementace třídy | soubor `.cpp` – definice funkcí |
| `private static` pole třídy | `static` proměnná v `.cpp` (viditelná jen v tom souboru) |
| `static class Settings { ... }` | „modul“: sada funkcí `Settings_Xxx()` + `static` stav v `Settings.cpp` |
| `const` / `enum` | `#define KONSTANTA hodnota` (preprocesor, textová náhrada) |
| `appsettings.json` / registry | **NVS** (Non-Volatile Storage) přes knihovnu `Preferences` |
| `HttpClient` | `HTTPClient` + `WiFiClient` / `WiFiClientSecure` |
| `System.Text.Json` | `ArduinoJson` v7 (`JsonDocument`, `deserializeJson`) |
| ASP.NET minimal API | `WebServer` z ESP32 core (`s_srv.on("/api/status", ...)`) |
| `Task` / `async` | nepoužívá se – vše běží **blokujícím** způsobem v `loop()` |
| GC / heap | žádný GC; paměť se spravuje ručně a jsou **dva typy RAM** (viz níže) |
| `Debug.WriteLine` | `Serial.printf(...)` → sériová konzole 115 200 Bd |
| ClickOnce / nasazení | nahrání `.bin` přes USB, nebo **OTA** přes web `/update` |

### 1.2 Důležité zvláštnosti platformy

- **Jedno „vlákno“ aplikace.** Vše (kreslení, síť, dotyk, web server) běží postupně v `loop()`.
  Když se stahují data, smyčka stojí. Projekt to řeší funkcí `netPoll()`, kterou síťový kód
  volá během přenosu – krmí watchdog a čte dotyk (viz 2.5).
- **Watchdog.** Když `loop()` stojí déle než 20 s (`WDT_TIMEOUT_S`), čip se restartuje.
  Dlouhé operace proto musí volat `Watchdog_Feed()`.
- **Dvě paměti:** ~320 kB rychlé *interní* RAM (potřebuje ji TLS, WiFi, zásobník) a 8 MB
  *PSRAM* (pomalejší, sem jdou framebuffery a velké buffery). Proto `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`.
- **Flash 16 MB** je rozdělená podle `partitions.csv`: dvě aplikační oblasti po 6 MB
  (`app0`/`app1`) kvůli OTA – nová verze se zapíše do druhé oblasti a po restartu se přepne.
- **Nikdy nekreslit z HTTP handleru ani zevnitř stahování.** Kreslí se jen z `loop()`.
  Webový handler jen nastaví příznak („přepni obrazovku“) a `loop()` ho vyzvedne.
- **Kompilace Arduina:** všechny `.cpp`/`.c` soubory ve složce sketche se kompilují automaticky.
  Smazat modul = smazat soubor a odstranit jeho `#include` a volání.

### 1.3 Nástroje

- **Arduino IDE 2.x** (nejjednodušší) nebo **arduino-cli** (reprodukovatelný build přes `sketch.yaml`).
- Nastavení desky v IDE: *ESP32S3 Dev Module*, **PSRAM: OPI PSRAM**, Flash Size 16 MB, Flash Mode QIO,
  **Partition Scheme: Custom** (použije `partitions.csv` ze složky sketche), USB CDC On Boot: Enabled.
- Sériový výpis jde jen přes USB-C konektor označený **USB** (nativní USB ESP32-S3).

---

## 2. Dokumentace původního projektu MeteoPlaneRadar

### 2.1 Co projekt dělá

Hodiny, radar letadel (adsb.fi), srážkový radar (ČHMÚ / RainViewer), předpověď počasí (Open-Meteo),
cena elektřiny a mix výroby na kulatém displeji. Konfiguruje se z prohlížeče.
7 obrazovek: Hodiny, Letadla, Meteoradar, Předpověď, Cena, Výroba, Nastavení.

### 2.2 Hardware desky (co kód ovládá)

| Součástka | Připojení | Soubor v kódu |
| --- | --- | --- |
| ESP32-S3R8 (8 MB PSRAM OPI, 16 MB flash) | – | – |
| Displej 480×480, řadič **ST7701** | 16bitová RGB sběrnice (HSYNC 38, VSYNC 39, DE 40, PCLK 41 + 16 datových pinů), inicializace přes SPI (CLK 2, MOSI 1) | [Display_ST7701.h](../MeteoPlaneRadar/Display_ST7701.h) |
| Podsvícení | GPIO 6, PWM 20 kHz | `Backlight_Init()`, `Set_Backlight(0..100)` |
| I/O expandér **TCA9554** (I2C 0x20) | I2C SDA 15 / SCL 7 | [TCA9554.h](../MeteoPlaneRadar/TCA9554.h) – reset LCD, CS LCD, napájení LCD, reset dotyku |
| Dotyk **CST820** (deska 2.1, I2C 0x15) | I2C + INT | [Touch_CST820.cpp](../MeteoPlaneRadar/Touch_CST820.cpp) |
| Dotyk **GT911** (deska 2.8C, I2C 0x5D/0x14) | I2C + INT | [Touch_GT911.cpp](../MeteoPlaneRadar/Touch_GT911.cpp) |
| Tlačítko BOOT | GPIO 0 | tovární reset při startu |

Obě desky jsou pinově shodné, liší se jen inicializační sekvencí ST7701, vertikálním časováním
a dotykovým řadičem. [Board.cpp](../MeteoPlaneRadar/Board.cpp) při startu zjistí, který dotykový čip
odpovídá, a podle toho zvolí inicializaci (vynutit jde `BOARD_FORCE` v `Config.h`: 0 = auto, 21, 28).

### 2.3 Architektura ve vrstvách

```
┌──────────────────────────────────────────────────────────────────────────┐
│ MeteoPlaneRadar.ino  – setup(), loop(), správce obrazovek, gesta         │
├───────────────┬──────────────────────────────┬───────────────────────────┤
│ OBRAZOVKY     │ DATOVÉ MODULY (stahování)    │ WEB / KONFIGURACE         │
│ ScreenClock   │ ADSB, Route, PlaneWatch      │ WiFiPortal (STA / AP)     │
│ ScreenPlanes  │ CHMU, RainViewer, StormWatch │ WebConfig (+WebPage.h)    │
│ ScreenWeather │ Forecast, Outside, GeoIP     │ Settings (NVS)            │
│ ScreenForecast│ Energy                       │ Status, Lang              │
│ ScreenPrice   │                              │                           │
│ ScreenMix     │ Net + NetSink (HTTP/HTTPS)   │                           │
│ ScreenSettings│                              │                           │
├───────────────┴──────────────────────────────┴───────────────────────────┤
│ KRESLENÍ: Canvas16 (framebuffer), AAFont+ClockFonts (písmo), UI, Layout, │
│           WxIcon, PlaneIcon, EuBorder+EuMapData+CzCitiesData, Geo        │
├──────────────────────────────────────────────────────────────────────────┤
│ HARDWARE: Board, Display_ST7701, TCA9554, Touch (CST820 / GT911),        │
│           Watchdog, NightMode, TimeZone                                  │
└──────────────────────────────────────────────────────────────────────────┘
```

### 2.4 Start zařízení – `setup()`

Pořadí v [MeteoPlaneRadar.ino](../MeteoPlaneRadar/MeteoPlaneRadar.ino) (na pořadí záleží):

1. `Serial.begin(115200)` – výpis verze, důvodu restartu, volné paměti.
2. `Settings_Begin()` – načte nastavení z NVS.
3. `TimeZone_Begin()` – nastaví časové pásmo (`TZ_INFO`).
4. `Wire.begin(15, 7, 400 kHz)` → `TCA9554_Init()` → zapnutí napájení LCD.
5. `Board_Detect()` – **musí být před inicializací displeje**.
6. `Backlight_Init()` (podsvícení zatím zhasnuté, aby neblikl náhodný obsah paměti panelu).
7. `ST7701_Init()` – při selhání restart za 5 s.
8. Vytvoří se `Canvas16`, napojí se na druhý framebuffer panelu → globální `gfx`.
9. `NightMode_Apply()` – rozsvítí podsvícení.
10. `Touch_Init()` – selhání není fatální.
11. `checkBootReset()` – drží-li se BOOT 3 s → `Settings_ClearAll()` + restart.
12. Registrace `netPoll` do síťových modulů (`Net_SetPollFn` atd.).
13. `WiFi_Begin()` – připojí se k uložené síti, nebo spustí přístupový bod (AP) s QR kódem.
14. `GeoIP_DetectIfNeeded()` – poloha podle IP.
15. Výběr úvodní obrazovky + `enterActive()` (v AP režimu ne – displej patří QR kódu).
16. `Watchdog_Begin()`.

### 2.5 Hlavní smyčka – `loop()`

```
loop():
  if běží OTA update → jen WebConfig_Loop(), nic nekreslit, return
  touchPump()                      // přečte dotyk, rozpozná gesto (nekreslí)
  if režim AP → kreslí se jen QR obrazovka, obsluha webu a WiFi, return
  dispatchTouch()                  // provede gesto: swipe / dlouhý stisk / klepnutí
  požadavky z webu (přepni obrazovku, rozsah)
  WiFi_Loop(), WebConfig_Loop()    // udržování WiFi, obsluha HTTP
  restart, pokud ho web vyžádal
  if activeTick() → drawActive()   // max ~12 FPS
  autoRotateTick(), planeAlertTick()
  TCA9554_Verify() každých 5 s, displayWatchdog()
  Outside_Tick(), Forecast_Tick(), Energy_Tick(), StormWatch_Tick(), ...  // stahování dat
  Settings_Tick()                  // odložený zápis do NVS
  Watchdog_Feed(); delay(5)
```

**Dotyk je rozdělen na dvě fáze** (klíčový vzor projektu):

- *Capture* – `touchPump()` jen čte řadič a rozpoznává gesto. Nic nekreslí, takže se dá volat
  i zevnitř stahování přes `netPoll()`.
- *Dispatch* – `dispatchTouch()` gesto provede, ale **jen z `loop()`**.

Rozpoznání gest (`touchPump`): swipe = posun ≥ 70 px vodorovně, ≤ 90 px svisle, do 700 ms;
dlouhý stisk = pohyb < 60 px a ≥ 500 ms; klepnutí = pohyb < 60 px. Konec doteku = 60 ms bez
vzorku (`TOUCH_RELEASE_MS`).

### 2.6 Kreslení

- **`gfx`** – globální ukazatel na `Arduino_GFX` (knihovna *GFX Library for Arduino*), ve skutečnosti
  instance [Canvas16](../MeteoPlaneRadar/Canvas16.h). Všechny obrazovky kreslí přes `gfx->fillRect`,
  `gfx->drawCircle`, `gfx->setCursor` + `gfx->print` atd.
- **Dvojitý framebuffer bez kopírování:** panel má 2 framebuffery v PSRAM. Kreslí se do toho, který
  se zrovna nezobrazuje; `gfx->flush()` předá ukazatel ovladači (přepne se při VSYNC) a kreslení
  přejde na druhý buffer. Výsledek: žádné blikání ani trhání obrazu.
  **Po `flush()` je v bufferu předminulý snímek – každý snímek se kreslí celý znovu** (začni `fillScreen`).
- **Barvy** jsou RGB565 (16 bit). Konstanty `C_BLACK`, `C_WHITE`, `C_CYAN`… v [UI.h](../MeteoPlaneRadar/UI.h).
- **Písmo:** `gfx->setTextSize(1..5)` + `print()`. Canvas16 přesměruje text do
  [AAFont](../MeteoPlaneRadar/AAFont.h), který ho vykreslí vyhlazeným písmem z
  [ClockFonts.h](../MeteoPlaneRadar/ClockFonts.h) (Inter / Poppins / Inter Light; velikosti T1–T5 pro
  text a L/M/S pro velké hodiny). Rozměry respektují „buňku“ vestavěného písma 6×8 px × velikost.
- **Pomocníci** v [UI.h](../MeteoPlaneRadar/UI.h): `UI_TextCentered(text, y, barva, velikost)`,
  `UI_DrawWifiQR(...)`, `UI_DrawStatusLine(y)` (čas nahoře), `UI_StatusLineChanged()`, `UI_FillRing(...)`.
- **[Layout.h](../MeteoPlaneRadar/Layout.h):** pevné řádky (`LY_DOTS`=18, `LY_STATUS`=30, …),
  `Layout_TextW()` (šířka textu) a `Layout_ChordHalf(y)` – kolik místa je na kulatém displeji v dané
  výšce. Nahoře a dole je málo místa, text tam musí být krátký.

### 2.7 Vzor obrazovky

Každá obrazovka je modul se stejným rozhraním (v C# by to byl interface `IScreen`):

| Funkce | Kdy se volá | Co dělá |
| --- | --- | --- |
| `ScreenX_Enter()` | při přepnutí na obrazovku | reset stavu |
| `ScreenX_Tick()` → `bool` | v každém `loop()` | vrátí `true`, když je potřeba překreslit |
| `ScreenX_Draw()` | když `Tick` vrátí `true`, nebo po `Enter` | nakreslí celý snímek do `gfx` |
| `ScreenX_HandleTap(x,y)` → `bool` | volitelně, klepnutí | `true` = překreslit |
| `ScreenX_ChangeRange(dir)` | volitelně, swipe | změna rozsahu (letadla, radar) |

Správce obrazovek v `.ino` je jen sada `switch (s_screen)` ve funkcích `enterActive()`,
`drawActive()`, `activeTick()`, `activeTap()`. `drawActive()` po obrazovce ještě nakreslí tečky
(indikátor obrazovek) a zavolá `gfx->flush()`. Indexy obrazovek jsou `SCREEN_*_I` v `Config.h`.

### 2.8 Síť

- **[Net.h](../MeteoPlaneRadar/Net.h)** – jediné místo pro HTTP(S) GET:
  - `Net_GetString(url, out, tag)` – celé tělo jako `String` (buffer v PSRAM, strop `NET_MAX_TEXT` = 128 kB).
    **Podporuje i `http://`** (bez TLS) – to se hodí pro vlastní API v lokální síti.
  - `Net_GetBinary(...)` – binární data (PNG dlaždice).
  - `Net_SessionBegin/End()` – keep-alive spojení pro sérii požadavků.
  - Timeouty: spojení 6 s, odpověď 10 s, celkový rozpočet na tělo 15 s (`NET_BODY_BUDGET_MS`).
  - Z každé odpovědi čte hlavičku `Date` a nastavuje podle ní hodiny.
- **[NetSink.h](../MeteoPlaneRadar/NetSink.h)** – bezpečné čtení těla (správně dekóduje *chunked*,
  hlídá velikost a čas), `Net_HeapOk()` – kontrola, že je dost interní RAM na TLS (~45 kB).
- TLS je „insecure“ (bez ověření certifikátu).

### 2.9 WiFi, portál, web server, OTA

- **[WiFiPortal](../MeteoPlaneRadar/WiFiPortal.h):** při startu zkusí uložené sítě (až 3, nejsilnější první).
  Nepovede-li se, spustí otevřený AP `MeteoPlaneRadar` a na displeji ukáže QR kód pro připojení.
  AP nemá timeout. Při výpadku se znovu připojuje, po 60 s zkouší ostatní uložené sítě.
- **[WebConfig](../MeteoPlaneRadar/WebConfig.h):** `WebServer` na portu 80, mDNS
  `http://meteoplaneradar.local/`, v AP režimu DNS server pro captive portál (vše přesměruje na 192.168.4.1).
  Captive portál i plné nastavení používají **stejnou** stránku `WebPage.h` (pozná režim podle `apMode`).
  Endpointy:

  | Endpoint | Účel | Osud |
  | --- | --- | --- |
  | `GET /` | celá stránka nastavení (`WebPage.h`, 73 kB HTML+JS) | **nahradit malou stránkou** |
  | `GET/POST /api/config` | čtení/zápis nastavení (JSON) | upravit |
  | `GET /api/status` | stav zařízení (verze, deska, IP, RSSI, uptime, heap, …) | upravit |
  | `POST /api/screen` | dálkové přepnutí obrazovky | ponechat |
  | `POST /api/range` | změna rozsahu | smazat |
  | `GET /api/scan` | seznam WiFi v okolí | ponechat |
  | `POST /api/wifi`, `/api/wifilist` | uložení WiFi | ponechat |
  | `GET /api/ha` | Home Assistant | smazat |
  | `GET /api/geocode` | hledání města | smazat |
  | `GET /api/export`, `POST /api/import` | záloha nastavení | smazat |
  | `POST /api/reboot`, `/api/reset` | restart, tovární reset | ponechat |
  | `GET/POST /update` | OTA aktualizace firmwaru (HTTP Basic, uživatel `admin`) | **ponechat** |

- Volitelné heslo správce (`Settings_SetAdminPassword`) chrání OTA, reset a import. Ukládá se v čitelné podobě.
- Během OTA se zhasne podsvícení (zápis do flash narušuje čtení PSRAM pro displej) a `loop()` nekreslí.

### 2.10 Nastavení – NVS

[Settings.h](../MeteoPlaneRadar/Settings.h) drží ~30 hodnot (WiFi, poloha, jas, noční režim, jazyk,
povolené obrazovky, filtry letadel, barvy, ceny…) v NVS namespace `planeradar`. Čtení jde z RAM
cache, zápis do NVS (`Preferences`). Některé hodnoty (aktuální obrazovka, rozsah) se zapisují
**odloženě** v `Settings_Tick()`, aby se flash zbytečně neopotřebovávala. `Settings_ToJson/FromJson`
slouží webu.

### 2.11 Spolehlivost (provoz 24/7)

- **HW watchdog** ([Watchdog.cpp](../MeteoPlaneRadar/Watchdog.cpp)) – restart po 20 s zaseknutí.
- **Hlídání displeje** (`displayWatchdog()` v `.ino`) – počítá snímky z přerušení VSYNC; když se
  zastaví, zkusí opravit napájení/reset panelu, po 8 s restartuje.
- **`TCA9554_Verify()`** každých 5 s – ověří, že expandér drží napájení a reset LCD.
- **Tovární reset** – podržení BOOT 3 s při startu.
- Pixel clock displeje je záměrně snížen na 8 MHz (stabilita při zatížení PSRAM) – **neměnit**.

### 2.12 Katalog všech souborů a jejich osud

Legenda: **P** = ponechat beze změny, **U** = ponechat a upravit, **S** = smazat, **N** = nový soubor.

#### Kořen repozitáře

| Soubor | Popis | Osud |
| --- | --- | --- |
| `README.md`, `README_EN.md` | dokumentace CZ/EN | N – vlastní README |
| `CHANGELOG.md` | historie verzí | N – vlastní, začít od 0.1.0 |
| `LICENSE.txt` | MIT + podmínky dat a licence písem | **P – nutno zachovat** (MIT vyžaduje ponechat copyright původního autora) |
| `sketch.yaml` | profil pro arduino-cli (core, knihovny) | U – odebrat PNGdec |
| `obrazovky.png` | screenshot | S |

#### Jádro a hardware

| Soubor | Popis | Osud |
| --- | --- | --- |
| `MeteoPlaneRadar.ino` | setup/loop, správce obrazovek, gesta, watchdog displeje | U → `DevOpsDashboard.ino` |
| `Config.h` | všechny konstanty (piny, URL, intervaly, indexy obrazovek) | U – výrazně zkrátit |
| `Version.h` | `FW_VERSION` | U – `0.1.0` |
| `partitions.csv` | rozdělení flash (2× app pro OTA) | P |
| `Board.h/.cpp` | detekce desky 2.1 / 2.8C | P |
| `Display_ST7701.h/.cpp` | ovladač panelu, framebuffery, podsvícení | P |
| `TCA9554.h/.cpp` | I/O expandér | P |
| `Touch.h/.cpp` | fasáda dotyku | P |
| `Touch_CST820.h/.cpp` | dotyk desky 2.1 | P |
| `Touch_GT911.h/.cpp` | dotyk desky 2.8C | P |
| `Watchdog.h/.cpp` | HW watchdog | P |

#### Kreslení a UI

| Soubor | Popis | Osud |
| --- | --- | --- |
| `Canvas16.h` | framebuffer + přesměrování textu do AAFont | P |
| `AAFont.h/.cpp` | vyhlazené písmo | U – pevně Inter, bez `Settings_ClockFont`, bez makra `CZT` |
| `ClockFonts.h` | data písem (3,6 MB zdrojáku, 3 typy × 8 velikostí) | U – nechat jen Inter |
| `UI.h/.cpp` | centrovaný text, QR, stavový řádek | U – stavový řádek z nového modulu `Clock` |
| `Layout.h/.cpp` | pevné řádky, kolize, šířka na kruhu | P |
| `qrcode.h/.c` | generátor QR (pro WiFi portál) | P |
| `WxIcon.h/.cpp` | ikony počasí | S |
| `PlaneIcon.h/.cpp` | ikony letadel | S |
| `EuBorder.h/.cpp`, `EuMapData.h` | mapa hranic Evropy (523 kB dat) | S |
| `CzCitiesData.h` | města ČR | S |
| `Geo.h` | geografické výpočty | S |

#### Síť, web, nastavení

| Soubor | Popis | Osud |
| --- | --- | --- |
| `Net.h/.cpp` | HTTP(S) GET helper | U – `Outside_NoteHttpDate` → `Clock_NoteHttpDate` |
| `NetSink.h/.cpp` | čtení těla odpovědi | P |
| `WiFiPortal.h/.cpp` | připojení / AP / QR obrazovka | U – texty EN, název AP, bez `Lang` |
| `WebConfig.h/.cpp` | web server, API, OTA | U – přepsat seznam endpointů, odstranit závislosti na datových modulech |
| `WebPage.h` | 73 kB stránka nastavení | U – nahradit malou stránkou |
| `Settings.h/.cpp` | NVS nastavení | U – ponechat WiFi, jas, obrazovku, heslo; přidat URL API a interval |
| `Status.h/.cpp` | stav zdrojů dat pro web | U – sloty `ST_API`, `ST_NTP` |
| `Lang.h/.cpp` | CZ/EN řetězce | S – vše anglicky přímo v kódu |

#### Data a čas

| Soubor | Popis | Osud |
| --- | --- | --- |
| `Outside.h/.cpp` | hodiny z hlavičky `Date` + venkovní teplota (Open-Meteo) | S → nahradit novým `Clock.h/.cpp` (NTP + `Date` jako záloha) |
| `TimeZone.h/.cpp` | časové pásmo podle odpovědi předpovědi | S – stačí `configTzTime(TZ_INFO, ...)` |
| `TimeUtil.h` | převod data na epoch bez knihovny | P – použije ho `Clock.cpp` |
| `NightMode.h/.cpp` | denní/noční jas podle slunce | S |
| `GeoIP.h/.cpp` | poloha podle IP (ip-api.com) | S |
| `ADSB.h/.cpp` | letadla z adsb.fi | S |
| `Route.h/.cpp` | trasa letu z adsb.lol | S |
| `PlaneWatch.h/.cpp` | upozornění na letadlo poblíž | S |
| `CHMU.h/.cpp` | srážkový radar ČHMÚ (PNG snímky) | S |
| `RainViewer.h/.cpp` | srážkový radar RainViewer (dlaždice) | S |
| `StormWatch.h/.cpp` | hlídání bouřky ze snímku ČHMÚ | S |
| `Forecast.h/.cpp` | předpověď, slunce, ovzduší z Open-Meteo | S |
| `Energy.h/.cpp` | spotová cena a mix výroby elektřiny | S |

#### Obrazovky

| Soubor | Popis | Osud |
| --- | --- | --- |
| `ScreenClock`, `ScreenPlanes`, `ScreenWeather`, `ScreenForecast`, `ScreenPrice`, `ScreenMix`, `ScreenSettings` (.h/.cpp) | 7 původních obrazovek | S (nejjednodušší `ScreenClock` / `ScreenSettings` si můžeš nechat otevřené v originále jako vzor) |
| `ScreenDashboard.h/.cpp` | obrazovka 1 – „DevOps dashboard“ | N |
| `ScreenBlank.h/.cpp` | obrazovka 2 – prázdná | N |
| `Clock.h/.cpp` | NTP + text času pro stavový řádek | N |
| `DevOpsApi.h/.cpp` | stahování z vlastního REST API (fáze 6) | N (později) |

### 2.13 Knihovny

| Knihovna | Kde | Potřeba v novém projektu |
| --- | --- | --- |
| ESP32 Arduino core 3.x (`WiFi`, `HTTPClient`, `WebServer`, `DNSServer`, `ESPmDNS`, `Update`, `Preferences`, `Wire`, `esp_lcd`) | všude | ano |
| GFX Library for Arduino (moononournation) | kreslení | ano |
| ArduinoJson 7 | web, nastavení, API | ano |
| PNGdec | radar (CHMU, RainViewer) | **ne** |
| QRCode (přibalené jako `qrcode.c`) | WiFi portál | ano |

> Pozor: `sketch.yaml` pinuje core 3.0.7 a GFX 1.4.9, ale README uvádí, že autor překládá s core 3.3.10
> a GFX 1.6.9. Nejdřív ověř, se kterou kombinací originál u tebe projde (fáze 0), a tu pak
> zafixuj v novém `sketch.yaml`.
>
> **Ověřeno (fáze 0):** originál se přeloží a běží na desce 2.1 s **esp32 core 3.3.10**,
> **GFX Library for Arduino 1.6.9**, **ArduinoJson 7.4.3** a **PNGdec 1.1.6** (1.1.7 v indexu není).
> Build: `esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=custom,CDCOnBoot=cdc,FlashMode=qio`.

> Poznámka: hlavička `ClockFonts.h` říká „vygenerováno `tools/make_fonts.py`, neupravovat ručně“,
> ten skript ale v repozitáři není. Redukce písem se proto dělá ručním vyjmutím bloků (krok 3.6).

---

## 3. Plán přestavby na DevOpsDashboard

### 3.1 Cílová struktura

```
DevOpsDashboard\
  DevOpsDashboard.ino      setup/loop, 2 obrazovky, swipe
  Config.h  Version.h  partitions.csv
  Board.*  Display_ST7701.*  TCA9554.*  Touch.*  Touch_CST820.*  Touch_GT911.*  Watchdog.*
  Canvas16.h  AAFont.*  ClockFonts.h (jen Inter)  UI.*  Layout.*  qrcode.*
  Net.*  NetSink.*  WiFiPortal.*  WebConfig.*  WebPage.h (malá)  Settings.*  Status.*
  Clock.*  TimeUtil.h
  ScreenDashboard.*  ScreenBlank.*
  (později) DevOpsApi.*
sketch.yaml  README.md  CHANGELOG.md  LICENSE.txt
```

Výsledný běh:

```
setup: HW init → Board_Detect → displej → dotyk → BOOT reset → WiFi (nebo AP+QR) → NTP → obrazovka
loop:  touchPump → (AP? jen QR) → dispatchTouch (swipe = přepnout) → web → Tick/Draw
       → TCA9554_Verify, displayWatchdog → (později DevOpsApi_Tick) → Settings_Tick → Watchdog_Feed
```

### 3.2 Postup – přehled fází

| Fáze | Obsah | Ověření |
| --- | --- | --- |
| 0 | Příprava prostředí, build a nahrání originálu | originál běží na desce |
| 1 | Kopie do `DevOpsDashboard\`, přejmenování | kopie se přeloží a běží stejně |
| 2 | Nový `.ino` se 2 obrazovkami | přeloží se (zatím s chybami v modulech) |
| 3 | Smazání nepotřebných modulů a oprava závislostí | čistý build |
| 4 | Web: malá stránka, konfigurace API, status, OTA | WiFi portál + `/update` fungují |
| 5 | Test na desce | checklist níže |
| 6 | (další krok) napojení na REST API | data na obrazovce |

Doporučení: **po každém kroku přeložit**. Chybové výpisy jsou dlouhé, vždy začni první chybou.
Po každé fázi `git commit`.

---

### Fáze 0 – Prostředí a ověření originálu

1. Nainstaluj Arduino IDE 2.x (nebo `winget install ArduinoSA.CLI` pro arduino-cli).
2. V *Boards Manager* nainstaluj **esp32 by Espressif Systems** (3.x). V *Library Manager*:
   **GFX Library for Arduino**, **ArduinoJson** (7.x), pro build originálu i **PNGdec**.
3. Nastav desku podle 1.3 (hlavně **PSRAM: OPI** a **Partition Scheme: Custom**).
4. Přelož a nahraj originál `MeteoPlaneRadar`. V sériovém výpisu ověř řádek
   `Deska: ESP32-S3-Touch-LCD-2.1 (ST7701 + CST820)` a že se zobrazí QR kód portálu.
   Tím víš, že toolchain a deska jsou v pořádku, ještě než cokoli změníš.

CLI varianta:

```powershell
arduino-cli core install esp32:esp32
arduino-cli lib install "GFX Library for Arduino" ArduinoJson PNGdec
arduino-cli compile --profile default MeteoPlaneRadar
arduino-cli upload -p COM5 --profile default MeteoPlaneRadar   # COM port podle Správce zařízení
arduino-cli monitor -p COM5 -c baudrate=115200
```

### Fáze 1 – Kopie projektu

1. Zkopíruj `MeteoPlaneRadar\` → `DevOpsDashboard\` (Arduino vyžaduje, aby se složka jmenovala stejně
   jako `.ino`).
2. Přejmenuj `MeteoPlaneRadar.ino` → `DevOpsDashboard.ino`.
3. Nový `sketch.yaml` pro `DevOpsDashboard` – bez `PNGdec`, s verzemi ověřenými ve fázi 0.
4. `Version.h`: `#define FW_VERSION "0.1.0"`.
5. Přelož – musí projít beze změny chování.

### Fáze 2 – Nový `DevOpsDashboard.ino`

Z původního `.ino` **ponech** (téměř beze změny):

- `touchPump()`, `netPoll()`, `checkBootReset()` (texty do angličtiny),
- `displayWatchdog()`, `resetReasonText()` (do angličtiny),
- logiku AP režimu v `loop()` (QR obrazovka vlastní displej, dokud není zadána WiFi),
- `drawScreenDots()`, `switchScreen()`, `gotoScreen()`, `enterActive()`, `drawActive()`, `activeTick()`, `activeTap()`.

**Odstraň:** vše kolem letadel (`s_alert*`, `activeModalOpen`, `alertTakenOver`, `endAlert`,
`planeAlertTick`), `autoRotateTick` + `autoRotatePauseMs` + `s_touchPauseUntil`, `activeChangeRange`,
podmínky ve `screenVisible()`/`inRotation()` (obě obrazovky jsou vždy viditelné), `*_SetPollFn` u
smazaných modulů, `GeoIP_DetectIfNeeded`, `CHMU_SetMasked`, `TimeZone_Begin`, a v `loop()`
`Outside_Tick`, `Forecast_Tick`, `Energy_Tick`, `StormWatch_Tick`, `PlaneWatch_Tick`, `NightMode_Tick`,
`Route_Tick`, `WebConfig_TakeRangeStep`.

**Změň:**

- `NightMode_Apply()` → `Set_Backlight(Settings_Backlight());`
- Gesto **swipe přepíná obrazovky** (v originálu měnilo rozsah), dlouhý stisk se ignoruje:

```cpp
static void dispatchTouch() {
  if (s_pendKind == PEND_NONE) return;
  const PendKind kind = s_pendKind;
  const int a = s_pendA, b = s_pendB;
  s_pendKind = PEND_NONE;

  switch (kind) {
    case PEND_SWIPE: switchScreen(a); break;          // a = +1 (prst doleva) / -1 (doprava)
    case PEND_TAP:   if (activeTap(a, b)) drawActive(); break;
    default: break;                                   // dlouhý stisk nic nedělá
  }
}
```

- Správce obrazovek pro 2 obrazovky:

```cpp
static void drawActive() {
  switch (s_screen) {
    case SCREEN_DASHBOARD_I: ScreenDashboard_Draw(); break;
    case SCREEN_BLANK_I:     ScreenBlank_Draw();     break;
  }
  drawScreenDots();
  gfx->flush();
}
// enterActive(), activeTick(), activeTap() analogicky
```

- Ve `setup()` zavolej `Clock_Begin()` hned po `Settings_Begin()` (nastaví pásmo; NTP se rozběhne
  samo, jakmile je WiFi).
- `MPR_CurrentScreen()` (používá ho `WebConfig.cpp`) přejmenuj na `App_CurrentScreen()`.

### Fáze 3 – Smazání modulů a oprava závislostí

#### 3.1 Smazat soubory

```
ADSB.*  Route.*  PlaneWatch.*  PlaneIcon.*  ScreenPlanes.*
CHMU.*  RainViewer.*  StormWatch.*  ScreenWeather.*  EuBorder.*  EuMapData.h  CzCitiesData.h  Geo.h
Forecast.*  ScreenForecast.*  WxIcon.*
Energy.*  ScreenPrice.*  ScreenMix.*
ScreenClock.*  ScreenSettings.*
Outside.*  TimeZone.*  NightMode.*  GeoIP.*  Lang.*
```

Přehled, kdo na co odkazuje (to jsou místa, kde po smazání vyskočí chyby):

| Ponechaný soubor | Odkazuje na smazané | Náhrada |
| --- | --- | --- |
| `Net.cpp` | `Outside.h` | `Clock.h` |
| `UI.cpp` | `Outside.h` | `Clock.h` |
| `AAFont.cpp` | `Settings_ClockFont()`, písma Poppins/Thin | pevně Inter |
| `AAFont.h` | makro `CZT` | smazat makro |
| `Settings.cpp` | `Lang.h` | odstranit jazyk |
| `Status.cpp` | `Lang.h` | anglické texty |
| `WiFiPortal.cpp` | `Lang.h`, `CZT` | anglické texty |
| `WebConfig.cpp` | téměř vše (viz fáze 4) | přepsat |

#### 3.2 `Config.h`

Ponech sekce: piny/sběrnice, `BOARD_FORCE`/`BOARD_FALLBACK`, `TZ_INFO`, `AP_SSID`/`AP_PASSWORD`,
`TOUCH_*`, `EXPANDER_CHECK_MS`, `DISPLAY_WD*`, `NET_*`, `HTTP_USER_AGENT`, `TOUCH_DEBUG`, `FLUSH_DEBUG`,
`WDT_TIMEOUT_S`, `WEB_*`, `WIFI_SLOTS`/`WIFI_SWITCH_AFTER_MS`/`WIFI_TRY_MS`, `LAYOUT_DEBUG`.

Smaž: ADSB, rozsahy, radar, RainViewer, předpověď, ceny/mix, styly hodin a písma, noční režim,
squawky, výškové barvy, storm, plane alert, okno, Home Assistant, `OUTSIDE_*`, `CLOCK_*`,
`AUTO_ROTATE_*`, `DETAIL_GRACE_POLLS`, `DEFAULT_LAT/LON`.

Přidej / změň:

```cpp
#define AP_SSID         "DevOpsDashboard"
#define WEB_HOSTNAME    "devopsdashboard"        // http://devopsdashboard.local/
#define HTTP_USER_AGENT "DevOpsDashboard/" FW_VERSION

#define NTP_SERVER_1    "pool.ntp.org"
#define NTP_SERVER_2    "time.google.com"

#define SCREEN_DASHBOARD_I 0
#define SCREEN_BLANK_I     1
#define SCREEN_N           2

#define API_URL_DEFAULT        ""               // prázdné = nestahovat
#define API_INTERVAL_S_DEFAULT 30
#define API_INTERVAL_S_MIN     5
```

#### 3.3 `Clock.h/.cpp` (nový, náhrada `Outside` + `TimeZone`)

- `Clock_Begin()` – `configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);` (ESP32 core pak synchronizuje
  čas přes SNTP sám na pozadí).
- `Clock_NoteHttpDate(const char*)` – převzít z `Outside_NoteHttpDate` (parsování hlavičky `Date` +
  `settimeofday`, s pomocí `TimeUtil.h`); záloha pro sítě, které blokují NTP (UDP 123) – na takový
  případ autor originálu narazil.
- `Clock_TimeValid()` – např. `time(nullptr) > 1700000000`.
- `Clock_StatusText(buf, cap)` – `"HH:MM"` (`localtime_r` + `strftime`).

Upravit uživatele: `Net.cpp` (`Outside_NoteHttpDate` → `Clock_NoteHttpDate`), `UI.cpp`
(`Outside_StatusText` → `Clock_StatusText`, `OUTSIDE_TEXT_MAX` → `CLOCK_TEXT_MAX`).

#### 3.4 `Settings.h/.cpp`

Ponech: WiFi sloty, jas (jen jedna úroveň – bez `BrightDay/Night`, `NightAuto`, `IsNight`),
`Screen` (poslední obrazovka, odložený zápis), admin heslo, `ToJson/FromJson`, `Tick`, `ClearAll`.
Smaž: polohu, noční režim, jednotky, jazyk, povolené obrazovky, auto-rotaci, radar, storm, letadla,
okno, HA, legendu, energii, vzhled hodin, výškové barvy, filtry, rozsahy, `TopBearing`.

Přidej:

```cpp
const char* Settings_ApiUrl();
void        Settings_SetApiUrl(const char* url);       // přijmout jen "http://" nebo "https://", max ~200 znaků
uint16_t    Settings_ApiIntervalSec();
void        Settings_SetApiIntervalSec(uint16_t s);     // ořezat na >= API_INTERVAL_S_MIN
```

Změň NVS namespace z `"planeradar"` na např. `"devops"`, aby se nemíchaly klíče z MeteoPlaneRadar.
Stará data ve flash ale zůstanou – **po prvním nahrání nového firmwaru udělej tovární reset**
(BOOT 3 s), nebo jednorázově `Erase All Flash` v Arduino IDE (*Tools → Erase All Flash Before Sketch Upload*).

#### 3.5 `Lang` → angličtina

`Lang` / `CZT` se používá v: `.ino`, `WiFiPortal.cpp`, `WebConfig.cpp`, `Settings.cpp`, `Status.cpp`,
`AAFont.h`. Postup: `L(S_XXX)` nahraď anglickým textem, u větvení `en ? "..." : "..."` ponech
anglickou větev, makro `CZT(plain, accented)` smaž a místa přepiš anglicky.

#### 3.6 `AAFont.cpp` + `ClockFonts.h`

- V `AAFont.cpp` smaž `SET_POPPINS`, `SET_THIN`, `TEXT_POPPINS`, `TEXT_THIN` a výběr podle
  `Settings_ClockFont()` nahraď pevně sadou Inter (`SET_INTER`, `TEXT_INTER`).
- `AAText_Enabled()` vrací vždy `true`.
- Z `ClockFonts.h` vyjmi blok **Poppins** (řádky 10540–22378, začíná `// --- Poppins Medium ---`)
  a blok **Inter Display Light** (od řádku 22379 `// --- Inter Display Light ---` do konce).
  Zdroják se zmenší zhruba na třetinu a firmware o velkou část dat písem.
  (Lze udělat až na konci – nic jiného na tom nezávisí.)

#### 3.7 `Status.h/.cpp`

Sloty nahradit: `enum StatusSlot : uint8_t { ST_API = 0, ST_NTP, ST_COUNT };`, texty do angličtiny.

#### 3.8 `WiFiPortal.cpp`

- Texty na QR obrazovce: `"DevOps Dashboard"`, `"Scan with your phone:"`,
  `"no password  |  then open 192.168.4.1"`, `"Waiting for your network..."`.
- Odstranit `chiptron.cz`, `Lang`, `CZT`.
- Logika připojování zůstává beze změny.

#### 3.9 Nové obrazovky

`ScreenDashboard.h`:

```cpp
#pragma once
#include <Arduino.h>

void ScreenDashboard_Enter();
void ScreenDashboard_Draw();
bool ScreenDashboard_Tick();
bool ScreenDashboard_HandleTap(int x, int y);
```

`ScreenDashboard.cpp`:

```cpp
#include "ScreenDashboard.h"
#include "UI.h"
#include "Layout.h"
#include "Display_ST7701.h"

void ScreenDashboard_Enter() {}

bool ScreenDashboard_Tick() {
  return UI_StatusLineChanged();       // překreslit, když se změní čas nahoře
}

void ScreenDashboard_Draw() {
  gfx->fillScreen(C_BLACK);
  UI_DrawStatusLine(LY_STATUS);                                  // hodiny nahoře
  UI_TextCentered("DevOps", LCD_HEIGHT / 2 - 40, C_CYAN, 5);
  UI_TextCentered("dashboard", LCD_HEIGHT / 2 + 10, C_WHITE, 4);
}

bool ScreenDashboard_HandleTap(int, int) { return false; }
```

`ScreenBlank` – stejné rozhraní, `Draw()` jen `gfx->fillScreen(C_BLACK); UI_DrawStatusLine(LY_STATUS);`,
`Tick()` vrací `UI_StatusLineChanged()`.

> Text na kulatém displeji: velikost 5 = buňka 30×40 px na znak (vyhlazené písmo bývá užší),
> „dashboard“ ve velikosti 4 ≈ 9 × 24 = 216 px – uprostřed je k dispozici celých 480 px.
> Nahoře a dole kontroluj šířku přes `Layout_TextW()` a `Layout_ChordHalf(y)`.

### Fáze 4 – Webové rozhraní

#### 4.1 `WebConfig.cpp`

- Odstranit `#include` smazaných modulů (`ScreenPlanes`, `ScreenWeather`, `ScreenPrice`, `Forecast`,
  `Energy`, `NightMode`, `TimeZone`, `StormWatch`, `PlaneWatch`, `PlaneIcon`, `ADSB`, `Outside`, `Lang`).
- Odstranit handlery: `handleRange`, `handleHa`, `handleGeocode`, `handleExport`, `handleImport`
  a jejich registraci v `WebConfig_Begin()`.
- `handleScreen` zjednodušit (jen index 0/1).
- `handleStatus` zjednodušit, např.:

```json
{
  "version": "0.1.0", "board": "2.1", "ip": "192.168.1.50", "ssid": "MyWiFi", "rssi": -58,
  "uptime": "0d 01:23:45", "heap": 182000, "psram": 7300000, "resetReason": "power on",
  "time": "2026-11-06 16:20", "screen": 0,
  "api": "OK, 3 items (12 s ago)"
}
```

- `handleGetConfig/handlePostConfig` jen pro: `apiUrl`, `apiIntervalSec`, `brightness`, `wifi[]`, `password`.
- Beze změny ponechat: `handleScan`, `handleWifi`, `handleWifiList`, `handleReboot`, `handleReset`,
  `handleNotFound` (přesměrování captive portálu), celou sekci OTA (`/update`, `otaStart/otaEnd` –
  jen texty EN), `WebConfig_Begin/Loop`, DNS + mDNS.
- Z hlavičky `WebConfig.h` smazat `WebConfig_TakeRangeStep()`.

#### 4.2 `WebPage.h` – nová malá stránka

Jedna stránka (HTML + trochu JS, **bez externích knihoven a CDN** – v AP režimu není internet):

1. **WiFi** – tlačítko *Scan* (`GET /api/scan`), výběr sítě, heslo, *Connect* (`POST /api/wifi`);
   3 uložené sloty (`POST /api/wifilist`).
2. **DevOps API** – URL, interval v sekundách, *Save* (`POST /api/config`).
3. **Display** – jas 0–100 (posuvník, ukládá hned), přepnutí obrazovky (`POST /api/screen`).
4. **System** – stav (`GET /api/status`, obnovovat po 5 s), odkaz na `/update`, heslo správce,
   *Reboot*, *Factory reset*.

V AP režimu (`apMode` z `/api/config`) stačí zobrazit jen sekci WiFi.
Jako šablonu vzhledu použij `UPDATE_HTML` z `WebConfig.cpp` (tmavý styl, žádné závislosti).

### Fáze 5 – Test na desce (checklist)

- [ ] Build bez chyb; velikost firmwaru < 6 MB (oblast `app0`).
- [ ] Po nahrání tovární reset (BOOT 3 s) → na displeji QR kód, AP `DevOpsDashboard`.
- [ ] Připojení telefonem → captive portál se otevře sám (nebo `http://192.168.4.1/`).
- [ ] Zadání WiFi → zařízení se připojí a zobrazí obrazovku „DevOps dashboard“.
- [ ] Nahoře se do minuty objeví čas (NTP).
- [ ] Swipe doleva/doprava přepíná mezi 2 obrazovkami, tečky nahoře ukazují pozici.
- [ ] Po restartu se otevře naposledy zobrazená obrazovka.
- [ ] `http://devopsdashboard.local/` a `/api/status` odpovídají.
- [ ] OTA: nahrát `DevOpsDashboard.ino.bin` přes `/update` → displej zhasne, po restartu nová verze.
- [ ] Jas z webu se projeví okamžitě.
- [ ] Na desce 2.8C (pokud ji máš) – sériový výpis hlásí `2.8C`, dotyk funguje.
- [ ] 24h běh bez restartu (`resetReason` ve `/api/status` zůstane `power on`).

Export binárky v Arduino IDE: *Sketch → Export Compiled Binary* → `DevOpsDashboard.ino.bin`
(pro OTA) a `DevOpsDashboard.ino.merged.bin` (pro první nahrání přes USB / webový flasher).

---

### Fáze 6 – Napojení na REST API (další krok po splnění cíle)

#### 6.1 Návrh kontraktu API (doporučení)

Krátký, plochý JSON, aby se vešel do paměti a snadno parsoval (strop `NET_MAX_TEXT` = 128 kB,
ideálně držet pod ~8 kB):

```json
{
  "generatedAt": "2026-11-06T15:20:00Z",
  "title": "Prod",
  "items": [
    { "name": "Build main",  "status": "ok",      "value": "#1234" },
    { "name": "Deploy prod", "status": "running", "value": "62%"   },
    { "name": "Alerts",      "status": "error",   "value": "3"     }
  ]
}
```

`status` mapovat na barvy: `ok` → zelená, `running`/`warning` → žlutá, `error` → červená, jinak šedá.
Na kulatý displej se rozumně vejde 5–6 řádků. Pokud API pošle i hlavičku `Date`, poslouží jako
záloha hodin (dělá to `Net.cpp` automaticky).

#### 6.2 Modul `DevOpsApi.h/.cpp`

```cpp
// DevOpsApi.h
#pragma once
#include <Arduino.h>

#define API_MAX_ITEMS 6
struct ApiItem { char name[24]; char value[16]; uint8_t status; };  // 0 unknown, 1 ok, 2 warn, 3 error

void           DevOpsApi_Tick();          // volat z loop(); stahuje podle intervalu
bool           DevOpsApi_Valid();
uint32_t       DevOpsApi_Revision();      // zvýší se po každých nových datech
uint8_t        DevOpsApi_Count();
const ApiItem* DevOpsApi_Item(uint8_t i);
```

```cpp
// DevOpsApi.cpp (jádro)
void DevOpsApi_Tick() {
  const char* url = Settings_ApiUrl();
  if (!*url || !WiFi_IsConnected()) return;
  if (s_last != 0 && millis() - s_last < Settings_ApiIntervalSec() * 1000UL) return;
  s_last = millis();

  String body;
  if (!Net_GetString(url, body, "API")) { Status_Set(ST_API, "HTTP error"); return; }

  JsonDocument doc;
  if (deserializeJson(doc, body)) { Status_Set(ST_API, "Bad JSON"); return; }

  s_count = 0;
  for (JsonObjectConst it : doc["items"].as<JsonArrayConst>()) {
    if (s_count >= API_MAX_ITEMS) break;
    ApiItem& d = s_items[s_count++];
    strlcpy(d.name,  it["name"]  | "", sizeof(d.name));
    strlcpy(d.value, it["value"] | "", sizeof(d.value));
    d.status = parseStatus(it["status"] | "");
  }
  s_valid = true;
  s_rev++;
  Status_Set(ST_API, "OK, %u items", (unsigned)s_count);
}
```

- `ScreenDashboard_Tick()` pak vrací `true` i při změně `DevOpsApi_Revision()`.
- `Net_GetString` u `http://` nepoužívá TLS → žádný nárok na ~45 kB interní RAM pro handshake.
- Volání blokuje `loop()` nejvýše po dobu timeoutů (6 s spojení + 10 s odpověď); dotyk a watchdog
  se mezitím obsluhují přes `netPoll()` (nezapomeň `Net_SetPollFn(netPoll)` v `setup()`).
  Pro dashboard s intervalem ≥ 5 s je to v pořádku. Kdyby bylo API pomalé, dalším krokem je
  přesun stahování do samostatného FreeRTOS tasku (`xTaskCreatePinnedToCore`) – nedělat hned.
- Když API přestane odpovídat, zobrazit poslední data šedě + „stale“ (vzor `ADSB_DataStale()`
  v originálu).

---

## 4. Rizika a tipy

| Riziko | Jak se mu vyhnout |
| --- | --- |
| Černý / rolující obraz | Neměnit `RGB_FREQ_HZ` a časování v `Display_ST7701.h`; PSRAM musí být **OPI** |
| Restart watchdogem | Dlouhé smyčky musí volat `Watchdog_Feed()` (nebo `netPoll()`) |
| Poškozený obraz / pád | Kreslit jen z `loop()`, nikdy z HTTP handleru ani z callbacku stahování |
| Nedostatek interní RAM | Velké buffery do PSRAM (`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`), `String` používat střídmě |
| Opotřebení flash | Do NVS nezapisovat v každém `loop()` – používat odložený zápis jako `Settings_Tick()` |
| Text mimo kruh | Kontrolovat šířku přes `Layout_TextW()` a `Layout_ChordHalf(y)` |
| OTA „zhasne“ displej | Je to normální – během zápisu je podsvícení vypnuté |
| Počítač nevidí desku | Použít konektor **USB** (ne UART); při zaseknutí držet BOOT a stisknout RESET |
| Licence | Ponechat `LICENSE.txt` s copyrightem původního autora (MIT) a licenci písma Inter (SIL OFL 1.1) |

## 5. Slovníček

| Pojem | Význam |
| --- | --- |
| **ESP32-S3** | mikrokontrolér (2 jádra, WiFi, BLE) |
| **PSRAM** | přídavná 8 MB RAM, pomalejší než interní |
| **NVS** | malé úložiště klíč–hodnota ve flash (nastavení) |
| **OTA** | Over-The-Air – aktualizace firmwaru přes WiFi |
| **AP / STA** | WiFi režim přístupového bodu / klienta |
| **Captive portal** | stránka, která se telefonu otevře sama po připojení k AP |
| **mDNS** | adresa `jmeno.local` bez DNS serveru |
| **RGB565** | 16bitová barva (5 bitů R, 6 G, 5 B) |
| **Framebuffer** | paměť s obrazem celé obrazovky (480×480×2 B = 450 kB) |
| **VSYNC** | signál „začíná nový snímek“ |
| **I2C** | dvoudrátová sběrnice pro dotyk a expandér |
| **Watchdog** | časovač, který restartuje zaseknutý čip |
| **`#define`** | textová náhrada preprocesoru (jako konstanta) |
| **`#pragma once`** | hlavička se vloží jen jednou |
