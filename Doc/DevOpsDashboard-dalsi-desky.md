# DevOps Dashboard – podpora dalších desek

> Související dokumenty: [plán a dokumentace projektu](DevOpsDashboard-plan.md),
> [stav a další kroky](DevOpsDashboard-dalsi-kroky.md),
> [výukový průvodce REST API](DevOpsDashboard-REST-API.md),
> [doporučení: Waveshare 7" + LVGL v Arduinu](DevOpsDashboard-doporuceni-LVGL.md),
> [README firmwaru](../DevOpsDashboard/README.md).
>
> Stav: **analýza a plán**, nic z toho zatím není implementováno.

Dokument řeší dvě desky navíc k dnešním kulatým Waveshare 2.1 / 2.8C:

- **Waveshare ESP32-S3-Touch-LCD-4.3** (SKU 25948) – obdélník 800×480, displej RGB,
- **SC01 Plus 3.5"** (Smart Panlee / Wireless-Tag, také WT32-SC01 Plus) – obdélník 320×480,
  displej s paralelní sběrnicí.

## 1. Shrnutí

- **Nový projekt není potřeba**, a nedoporučuje se. Obě desky se přidají do současného projektu
  jako další **cíle buildu**; z jednoho zdrojového kódu vznikne pro každý cíl vlastní binárka.
- **Waveshare 4.3 je dnešnímu kódu velmi blízko:** stejný typ displeje (RGB), stejná PSRAM
  (8 MB OPI) a stejný dotykový čip GT911. Driver displeje i dotyku lze z velké části převzít.
- **SC01 Plus je jiná platforma:** jiný typ PSRAM (rozhoduje se při kompilaci), jiné rozhraní
  displeje a jiný dotykový čip. Jedna binárka s ostatními deskami pro ni nejde.
- **Doporučené pořadí:** nejdřív společný základ, pak deska 4.3 (menší riziko, prověří přepínač
  cíle i obdélníkové rozvržení), nakonec SC01 Plus. Celé to stihnout **před fází 6**
  (napojení REST API), aby se dashboard rovnou navrhl pro všechny tvary displeje.

## 2. Srovnání desek

| | Waveshare 2.1 / 2.8C (dnes) | **Waveshare 4.3** | **SC01 Plus 3.5"** |
| --- | --- | --- | --- |
| Displej | kulatý 480×480, ST7701, **RGB** | obdélník 800×480, ST7262, **RGB** | obdélník 320×480, ST7796U, **8bit paralelní (i80)** |
| Inicializace panelu | sekvence příkazů po SPI | **žádná** – ST7262 nemá příkazové rozhraní | příkazy po paralelní sběrnici |
| Paměť obrazu | panel čte obraz neustále z PSRAM, 2× 450 kB | panel čte obraz neustále z PSRAM, **2× 750 kB** | panel má vlastní GRAM, canvas 300 kB |
| Modul / flash / PSRAM | 16 MB / 8 MB **OPI** | ESP32-S3-WROOM-1-N16R8: 16 MB / 8 MB **OPI** | N16R2: 16 MB / 2 MB **QSPI** |
| Dotyk | CST820 / GT911 | **GT911** | FT6336U (0x38) |
| Expandér | TCA9554 (0x20) | **CH422G** (jiný protokol) | **žádný** |
| I2C (SDA / SCL) | 15 / 7 | 8 / 9 | 6 / 5 |
| Reset LCD | EXIO1 | EXIO3 | GPIO4 |
| Podsvícení | PWM, GPIO6 | **jen zapnout / vypnout** (EXIO2) | PWM, GPIO45 |
| GPIO0 (BOOT) | volné | **datový vodič displeje G3** | **vodič D/C displeje** |
| Sériová linka | nativní USB (CDC) | 2× USB-C: převodník CH343P (UART0) a nativní USB (přepínané s CAN) | ověřit |
| Jedna binárka s kulatými deskami | – | technicky možná, nedoporučeno ([kap. 5](#5-proč-samostatné-binárky)) | nemožná |

## 3. Waveshare ESP32-S3-Touch-LCD-4.3 (SKU 25948)

### Parametry

| Položka | Hodnota |
| --- | --- |
| Modul | ESP32-S3-WROOM-1-**N16R8** (16 MB flash, 8 MB OPI PSRAM) |
| Displej | 4,3" IPS, **800×480**, 65K barev, rozhraní RGB 16 bit (RGB565), řadič ST7262 |
| Dotyk | kapacitní, 5 bodů, **GT911** na I2C |
| Expandér | **CH422G** |
| Podsvícení | zesilovač MP3302, ovládaný jen zapnutím přes EXIO2 – **bez regulace jasu** |
| Další | CAN (TJA1051), RS485 (SP3485), slot TF karty, nabíjení Li-Ion článku (CS8501), tlačítka BOOT a RESET |
| USB | „USB TO UART“ (CH343P → UART0, GPIO43/44) a nativní USB (GPIO19/20, sdílené s CAN přes přepínač FSUSB42) |

### Displej (RGB)

Pořadí datových vodičů tak, jak je čeká `esp_lcd` (`data_gpio_nums[0..15]` = B, G, R od nejnižšího bitu):

| D0–D4 (B3–B7) | D5–D10 (G2–G7) | D11–D15 (R3–R7) |
| --- | --- | --- |
| 14, 38, 18, 17, 10 | 39, **0**, 45, 48, 47, 21 | 1, 2, 42, 41, 40 |

| Signál | GPIO |
| --- | --- |
| HSYNC | 46 |
| VSYNC | 3 |
| DE | 5 |
| PCLK | 7 |

Časování podle oficiální konfigurace desky (ESP32_Display_Panel):

| Parametr | Hodnota |
| --- | --- |
| Pixel clock | **16 MHz** |
| HSYNC pulse / back porch / front porch | 4 / 8 / 8 |
| VSYNC pulse / back porch / front porch | 4 / 8 / 8 |
| `pclk_active_neg` | **1** (data na sestupnou hranu – kulaté desky mají 0) |
| Bounce buffer | 10 řádků = 8000 px (384 000 / (2 × 8000) = 24, podmínka driveru splněna) |
| Délka snímku při 16 MHz | 820 × 500 = 410 000 taktů = 25,6 ms (~39 Hz) |

### Dotyk, I2C a expandér CH422G

| Funkce | Připojení |
| --- | --- |
| I2C SDA / SCL (dotyk i expandér) | GPIO8 / GPIO9 |
| Dotyk INT | GPIO4 |
| Dotyk RESET | CH422G **EXIO1** |
| Podsvícení (DISP) | CH422G **EXIO2** |
| LCD RESET | CH422G **EXIO3** (podle ESP32_Display_Panel, ve wiki chybí – **ověřit ve schématu**) |
| SD_CS (TF karta) | CH422G EXIO4 |
| USB_SEL / CAN_SEL | CH422G EXIO5 – log. 0 = nativní USB, log. 1 = CAN |
| TF karta MOSI / SCK / MISO | GPIO11 / 12 / 13 |
| RS485 RXD / TXD | GPIO16 / GPIO15 |
| CAN RX / TX | GPIO19 / GPIO20 |

CH422G **nemá registry** jako TCA9554. Každý příkaz má vlastní I2C adresu:

| Adresa | Příkaz |
| --- | --- |
| 0x24 | WR_SET – systémové nastavení (bit 0 = výstupy IO0–IO7 zapnuté) |
| 0x38 | WR_IO – hodnoty výstupů IO0–IO7 |
| 0x26 | RD_IO – čtení stavu IO0–IO7 |
| 0x23 | WR_OC – výstupy s otevřeným kolektorem OC0–OC3 |

Adresy nekolidují s GT911 (0x5D / 0x14). GT911 se při resetu s INT v log. 0 ozve na 0x5D,
stejně jako dnes na 2.8C.

### Výběr desky v Arduino core

Arduino core obsahuje definici desky **„Waveshare ESP32-S3-Touch-LCD-4.3“**
(`waveshare_esp32_s3_touch_lcd_43`), která definuje makro
`ARDUINO_WAVESHARE_ESP32_S3_TOUCH_LCD_43` – podle něj firmware pozná cíl buildu.

- Ve výchozím stavu má **PSRAM vypnutou** a **flash 8 MB** – v menu je nutné nastavit
  *PSRAM: Enabled* (nastaví OPI) a *Flash Size: 16MB*.
- *USB CDC On Boot* je ve výchozím stavu *Disabled*: sériový výpis jde přes port
  **USB TO UART** (CH343P). To je pro tuto desku správně, viz [krok 9](#krok-9--build).
- Ověřeno v nainstalovaném core **3.3.12**. Projekt připíná **3.3.10** – ověřit, že tam
  definice desky je také, případně verzi v profilu zvednout.

## 4. SC01 Plus 3.5"

Údaje z krabičky a z veřejných konfigurací desky (LovyanGFX, RIOT, ESPHome, viz [zdroje](#12-zdroje)).

| Položka | Hodnota |
| --- | --- |
| Výrobce / dokumentace | Smart Panlee (Wireless-Tag), `doc.smartpanle.cn`, označení také **WT32-SC01 Plus** |
| MCU | ESP32-S3 (modul N16R2) |
| Flash / PSRAM | 16 MB / **2 MB, QSPI** (ne OPI) |
| Displej | 3,5" IPS, **320×480**, řadič **ST7796U** |
| Rozhraní displeje | **8bitová paralelní sběrnice (i80)**, displej má vlastní paměť obrazu (GRAM) |
| Dotyk | **FT6336U** na I2C (adresa 0x38) |
| Další rozhraní | RS485, USB, konektor reproduktoru, SD karta, rozšiřující IO |

| Funkce | GPIO |
| --- | --- |
| LCD WR | 47 |
| LCD RS (D/C) | **0** (zároveň strapping pin / BOOT) |
| LCD D0–D7 | 9, 46, 3, 8, 18, 17, 16, 15 |
| LCD RESET | 4 |
| Podsvícení (PWM) | 45 |
| Dotyk I2C SDA / SCL | 6 / 5 |
| Dotyk INT | 7 |
| Dotyk RESET | pravděpodobně společný s LCD RESET (GPIO4) – **ověřit ve schématu** |
| SD karta (CS / CLK / CMD / D0) | 41 / 39 / 40 / 38 |
| I2S zvuk (BCLK / LRC / DOUT) | 36 / 35 / 37 |

## 5. Proč samostatné binárky

Desky 2.1 a 2.8C mají stejné piny a liší se jen inicializací panelu a dotykovým čipem, proto je
[Board.cpp](../DevOpsDashboard/Board.cpp) rozlišuje za běhu. U nových desek:

**SC01 Plus – jedna binárka nejde:**

1. **Typ PSRAM se volí při kompilaci.** Arduino core 3.x má pro OPI a QSPI PSRAM jiné
   předkompilované knihovny (`PSRAM=opi` / `PSRAM=enabled` ve FQBN). Binárka pro OPI na SC01 Plus
   PSRAM vůbec nenajde, a naopak.
2. **Piny se kříží.** Dnešní start volá `Wire.begin(15, 7)` a hledá dotykový čip na I2C. Na SC01 Plus
   je ale GPIO15 datový vodič D7 displeje a GPIO7 je INT dotyku.

**Waveshare 4.3 – jedna binárka s kulatými deskami technicky jde, ale nedoporučuje se:**

- PSRAM je stejná (OPI), takže by šlo hledat TCA9554 na pinech 15/7 a když neodpoví, CH422G na 8/9.
  Na druhé desce jsou tyto piny vstupy displeje (případně RS485 TXD), před inicializací displeje
  tedy nic nehrozí.
- Rozměry displeje (`LCD_WIDTH`/`LCD_HEIGHT`) a rozvržení obrazovek jsou ale konstanty při
  kompilaci. Musely by se změnit na hodnoty určené za běhu – to je větší přestavba.
- Riziko nahrání špatné binárky přes OTA pokryje [ochrana OTA](#krok-10--ochrana-ota).
- Lze se k tomu vrátit později, pokud by jedna binárka pro všechny OPI desky dávala smysl.

**Varianty řešení:**

| Varianta | Hodnocení |
| --- | --- |
| A) Nový projekt pro každou desku (kopie) | Začátek je rychlý, ale každou další změnu (REST API, web, opravy) by bylo nutné dělat vícekrát. Repozitář už jednu takovou kopii má (MeteoPlaneRadar → DevOpsDashboard). |
| **B) Jeden projekt, cíl se volí při buildu** | **Doporučeno.** Většina kódu na hardwaru nezávisí a zůstane společná. |
| C) Jedna binárka pro všechny desky | Pro SC01 Plus nelze; pro 4.3 + kulaté desky možné, ale zatím nevýhodné (viz výše). |

## 6. Kde je dnešní kód vázaný na hardware

### Beze změny (společné pro všechny desky)

`Net`, `NetSink`, `Settings`, `Clock`, `Status`, `Watchdog`, `AAFont`, `ClockFonts.h`, `qrcode`,
`TimeUtil.h`, `Version.h`, [ScreenBlank.cpp](../DevOpsDashboard/ScreenBlank.cpp) a budoucí
`DevOpsApi` (fáze 6). Hlášky centrované přes `LCD_HEIGHT / 2` (OTA, připojování k WiFi,
tovární reset) fungují na všech rozměrech.

### Vyžaduje změnu

| Soubor | Vazba na hardware | Co udělat |
| --- | --- | --- |
| [Config.h](../DevOpsDashboard/Config.h) | `I2C_SDA`/`I2C_SCL`, `BOOT_PIN`, `BOARD_FORCE`, `DISPLAY_WD` | přepínač cíle, piny po cílech |
| [Display_ST7701.h](../DevOpsDashboard/Display_ST7701.h) / [.cpp](../DevOpsDashboard/Display_ST7701.cpp) | piny RGB, časování, rozměry, SPI init, podsvícení GPIO6 | rozdělit: společný RGB driver (kulaté + 4.3), init sekvence ST7701 zvlášť, rozhraní do `Display.h` |
| [Canvas16.h](../DevOpsDashboard/Canvas16.h) | includuje `Display_ST7701.h` | přejít na `Display.h`; záložní vlastní buffer (pro SC01 Plus) už má |
| [Board.h](../DevOpsDashboard/Board.h) / [.cpp](../DevOpsDashboard/Board.cpp) | detekce přes CST820/GT911 a TCA9554 | pro 4.3 a SC01 Plus pevně daný model, bez detekce |
| [TCA9554.h](../DevOpsDashboard/TCA9554.h) / [.cpp](../DevOpsDashboard/TCA9554.cpp) | expandér kulatých desek | schovat za společné API expandéru, přidat CH422G |
| [Touch.cpp](../DevOpsDashboard/Touch.cpp) | výběr driveru podle desky | přidat GT911 pro 4.3 a FT6336 pro SC01 Plus |
| [Touch_GT911.h](../DevOpsDashboard/Touch_GT911.h) / [.cpp](../DevOpsDashboard/Touch_GT911.cpp) | `GT911_INT_PIN 16`, reset přes `TCA9554_SetPin` | INT pin po cílech (4.3: GPIO4), reset přes API expandéru |
| [Touch_CST820.cpp](../DevOpsDashboard/Touch_CST820.cpp) | reset přes TCA9554 | jen pro kulaté desky; reset přes API expandéru |
| [Layout.h](../DevOpsDashboard/Layout.h) / [.cpp](../DevOpsDashboard/Layout.cpp) | pásy `LY_*` pro kruh, `Layout_ChordHalf` | hodnoty po cílech, pro obdélník plná šířka |
| [UI.cpp](../DevOpsDashboard/UI.cpp) | `UI_ChordHalfWidth` (stavový řádek) | pro obdélník plná šířka |
| [ScreenDashboard.cpp](../DevOpsDashboard/ScreenDashboard.cpp) | pevná geometrie pro 480×480 (`RING_CX 240`, legenda y 372–400) | obdélníkové rozvržení poměrově k rozměrům displeje |
| [WiFiPortal.cpp](../DevOpsDashboard/WiFiPortal.cpp) | QR kód a texty na y 40–348 | obdélníkové rozvržení (QR vlevo, text vpravo) |
| [WebConfig.cpp](../DevOpsDashboard/WebConfig.cpp) | jméno desky ve `/api/status`, OTA | jméno nových desek, informace „jas lze regulovat“, ochrana OTA |
| [WebPage.h](../DevOpsDashboard/WebPage.h) | posuvník jasu | na desce 4.3 posuvník skrýt |
| [DevOpsDashboard.ino](../DevOpsDashboard/DevOpsDashboard.ino) | `Wire.begin`, volání `TCA9554_*`, `Board_Detect`, `ST7701_Init`, `displayWatchdog`, `checkBootReset` | větvení podle cíle, BOOT číst před inicializací displeje |
| [sketch.yaml](../DevOpsDashboard/sketch.yaml), [build.cmd](../DevOpsDashboard/build.cmd) | jeden FQBN s `PSRAM=opi` | profil / parametr pro každý cíl |

## 7. Plán implementace (varianta B)

### Krok 1 – přepínač cíle

- V [Config.h](../DevOpsDashboard/Config.h) zavést `HW_TARGET` s hodnotami `HW_WS_ROUND`
  (2.1 / 2.8C), `HW_WS_43` a `HW_SC01_PLUS`.
- Výběr automaticky podle nastavení desky v IDE, s možností přebít ho přes `-DHW_TARGET=...`:

  | Podmínka | Cíl |
  | --- | --- |
  | `ARDUINO_WAVESHARE_ESP32_S3_TOUCH_LCD_43` (deska „Waveshare ESP32-S3-Touch-LCD-4.3“) | `HW_WS_43` |
  | `CONFIG_SPIRAM_MODE_QUAD` (*PSRAM: QSPI*) | `HW_SC01_PLUS` |
  | `CONFIG_SPIRAM_MODE_OCT` (*PSRAM: OPI*, dnešní nastavení) | `HW_WS_ROUND` |
  | nic z toho | `#error` s návodem, co nastavit |

  Samotná volba PSRAM nestačí, protože 4.3 má OPI stejně jako kulaté desky. Názvy maker
  `CONFIG_SPIRAM_MODE_*` ověřit při implementaci v `sdkconfig.h` core.
- Piny (I2C, BOOT, podsvícení) mít v jednom bloku pro každý cíl.
- Arduino kompiluje **všechny** `.cpp` ve složce sketche, proto soubory pro konkrétní desku celé
  obalit `#if HW_TARGET == ...`.

### Krok 2 – společné rozhraní displeje `Display.h`

- Obsah: `LCD_WIDTH`/`LCD_HEIGHT`, `Display_Init()`, `LCD_FrameBuffer()`, `LCD_Flush()`,
  `LCD_VsyncCount()`, `Backlight_Init()`, `Set_Backlight()`, `Backlight_Dimmable()`.
- Dnes [Display_ST7701.h](../DevOpsDashboard/Display_ST7701.h) includuje 9 souborů; ty přejdou
  na `Display.h`.

### Krok 3 – společný RGB driver (kulaté desky + 4.3)

- Z [Display_ST7701.cpp](../DevOpsDashboard/Display_ST7701.cpp) oddělit část s RGB panelem do
  `Display_RGB.cpp`: dva framebuffery, bounce buffery, synchronizace na VSYNC, počítadlo snímků
  pro hlídač displeje. Tato část je pro obě rodiny desek stejná.
- Parametry (piny, časování, `pclk_active_neg`, rozměry) brát z konfigurace cíle.
- Init sekvence ST7701 po SPI (s CS přes TCA9554) zůstane jen pro kulaté desky. Deska 4.3 žádnou
  nepotřebuje – jen reset LCD přes CH422G EXIO3.
- [Canvas16.h](../DevOpsDashboard/Canvas16.h) a kreslení bez kopírování (zero-copy) fungují beze
  změny.

### Krok 4 – společné API expandéru

- `Expander.h` s logickými funkcemi: reset LCD, reset dotyku, CS LCD, napájení LCD, podsvícení.
  Každý cíl je namapuje na svůj čip, nebo ohlásí, že je nemá.
  - Kulaté desky: TCA9554 (dnešní [TCA9554.cpp](../DevOpsDashboard/TCA9554.cpp)).
  - 4.3: nový `CH422G.cpp` (asi 60–80 řádků).
  - SC01 Plus: žádný expandér, reset LCD je přímo GPIO4.
- CH422G při startu: **nejdřív zapsat hodnoty** (0x38), **pak zapnout výstupy** (0x24), aby
  nevznikl zákmit. Výchozí hodnoty: reset dotyku a LCD v log. 1, podsvícení vypnuté do prvního
  snímku, SD_CS v log. 1, USB_SEL v log. 0 (nativní USB zůstane funkční).
- Obdoba `TCA9554_Verify()`: přečíst stav (0x26) a při neshodě znovu zapsat 0x38 i 0x24.
  CH422G nemá konfigurační registr; po jeho resetu jsou výstupy vypnuté.

### Krok 5 – driver displeje SC01 Plus `Display_ST7796.cpp`

- Sběrnice i80 přes `Arduino_ESP32LCD8` a panel `Arduino_ST7796` z knihovny
  *GFX Library for Arduino*, která už je v závislostech.
- `LCD_FrameBuffer()` vrátí `nullptr`, takže [Canvas16.h](../DevOpsDashboard/Canvas16.h) použije
  vlastní buffer v PSRAM (480 × 320 × 2 = 300 kB). Tato záložní cesta už v `setup()` existuje.
- `LCD_Flush()` pošle celý obraz do GRAM displeje, odhadem ~15 ms.
- Otočení obrazu na šířku zařídí přímo panel (registr MADCTL), `Canvas16` zůstane bez rotace.
- `DISPLAY_WD 0`: panel má vlastní paměť a žádný VSYNC, takže selhání, které hlídač displeje řeší
  (RGB periferie přestane posílat obraz), tu nenastává.

### Krok 6 – dotyk

- **4.3:** stávající [Touch_GT911.cpp](../DevOpsDashboard/Touch_GT911.cpp) – INT pin po cílech
  (GPIO4), reset přes API expandéru (EXIO1). Souřadnice hlásí přímo v 800×480, bez otáčení.
- **SC01 Plus:** nový `Touch_FT6336.cpp/.h`. Registry 0x02–0x06 mají stejné rozložení jako
  u CST820, takže většinu kódu lze převzít včetně čtení jen po přerušení na INT (GPIO7). Přidat
  přepočet souřadnic podle orientace (dotyk hlásí 320×480 na výšku).
- [Touch.cpp](../DevOpsDashboard/Touch.cpp) vybere driver podle cíle.

### Krok 7 – Board, podsvícení a `DevOpsDashboard.ino`

- `Board_Detect()` pro 4.3 a SC01 Plus nic nehledá, jen nastaví model; `Board_Name()` vrátí
  `"4.3"` / `"SC01 Plus"` (zobrazí se ve `/api/status`).
- `Wire.begin` s piny podle cíle; volání `TCA9554_*` nahradit API expandéru.
- **Tlačítko BOOT číst ještě před inicializací displeje.** Dnešní `checkBootReset()` volá
  `pinMode(0, INPUT_PULLUP)` až po ní. Na desce 4.3 by tím **odpojila zelený bit G3 od displeje**,
  na SC01 Plus vodič D/C. Viz [otevřená rozhodnutí](#10-otevřená-rozhodnutí).
- **Podsvícení na 4.3:** `Set_Backlight(0)` vypne, jakákoli vyšší hodnota zapne.
  `Backlight_Dimmable()` vrátí `false`; `/api/status` to předá webu a
  [WebPage.h](../DevOpsDashboard/WebPage.h) posuvník jasu skryje. Plynulá regulace by šla jen
  úpravou hardwaru (vést podsvícení na PWM pin).

### Krok 8 – rozvržení pro obdélníkový displej

Největší část práce, protože UI je navržené na kruh. Jedno obdélníkové rozvržení poslouží oběma
novým deskám (800×480 a 480×320 mají podobný poměr stran), pokud bude geometrie **poměrová**
k `LCD_WIDTH`/`LCD_HEIGHT` a velikosti písma po cílech.

- [Layout.h](../DevOpsDashboard/Layout.h) a [Layout.cpp](../DevOpsDashboard/Layout.cpp): pásy `LY_*`
  po cílech, `Layout_ChordHalf()` na obdélníku vrací polovinu šířky. Totéž `UI_ChordHalfWidth()`
  v [UI.cpp](../DevOpsDashboard/UI.cpp).
- [ScreenDashboard.cpp](../DevOpsDashboard/ScreenDashboard.cpp): prstenec vlevo, legenda ve čtyřech
  řádcích vpravo. Geometrii mít v konstantách po cílech, kreslicí kód společný.
- [WiFiPortal.cpp](../DevOpsDashboard/WiFiPortal.cpp): obrazovka přístupového bodu – QR kód vlevo,
  text vpravo.
- Prahy gest (swipe ≥ 70 px) v `touchPump()` jsou v pixelech a fungují na všech rozměrech.
- Zapnout `LAYOUT_DEBUG` a zkontrolovat výpis samotestu pásů.

### Krok 9 – build

- [sketch.yaml](../DevOpsDashboard/sketch.yaml): profil pro každý cíl (každý profil musí mít
  vlastní seznam `platforms` a `libraries`), např.

  ```yaml
  ws43:
    fqbn: esp32:esp32:waveshare_esp32_s3_touch_lcd_43:PSRAM=enabled,FlashSize=16M,PartitionScheme=custom,FlashMode=qio
  sc01plus:
    fqbn: esp32:esp32:esp32s3:PSRAM=enabled,FlashSize=16M,PartitionScheme=custom,CDCOnBoot=cdc,FlashMode=qio
  ```

- **4.3:** logy a flashování přes port **USB TO UART** (CH343P), *USB CDC On Boot: Disabled*
  (výchozí v definici desky). Nativní USB je přepínané s CAN.
- **SC01 Plus:** `CDCOnBoot` ověřit podle toho, zda je USB-C připojené na nativní USB ESP32-S3,
  nebo přes převodník USB-UART.
- [build.cmd](../DevOpsDashboard/build.cmd): parametr cíle (`build.cmd ws43 COM5`) a samostatná
  výstupní složka pro každý cíl (`build\round\`, `build\ws43\`, `build\sc01plus\`), aby se
  binárky nepletly.
- [partitions.csv](../DevOpsDashboard/partitions.csv) zůstává, všechny desky mají 16 MB flash.
  Výjimka: pokud by deska 4.3 byla starší revize s 8 MB flash, je potřeba druhá tabulka oddílů
  (např. 2× 3 MB pro aplikaci; firmware má dnes 1,34 MB).

### Krok 10 – ochrana OTA

Se třemi binárkami hrozí, že se přes `/update` nahraje ta špatná. Výsledkem by byl černý displej
a nutnost flashovat přes USB.

- Do firmwaru vložit značku cíle (konstantní řetězec, např. `"DOD-HW:WS43"`).
- Upload handler ve [WebConfig.cpp](../DevOpsDashboard/WebConfig.cpp) ji vyhledává v přijímaných
  datech a před `Update.end()` ověří, že odpovídá běžícímu cíli; při neshodě `Update.abort()`
  a srozumitelná chyba v prohlížeči.

### Krok 11 – dokumentace

- [README firmwaru](../DevOpsDashboard/README.md): podporované desky, nastavení IDE pro každý cíl,
  názvy binárek, port pro sériový výpis.
- [Stav a další kroky](DevOpsDashboard-dalsi-kroky.md): build a nahrání pro nové desky.

## 8. Doporučené pořadí

1. **Společný základ bez změny chování:** přepínač cíle, `Display.h`, oddělení RGB driveru, API
   expandéru, čtení BOOT před inicializací displeje, INT pin GT911 po cílech → **znovu otestovat
   desku 2.1**.
2. **Waveshare 4.3:** CH422G, konfigurace RGB panelu, podsvícení zap/vyp + web, obdélníkové
   rozvržení 800×480, profil buildu, ladění pixel clocku pod zátěží WiFi.
3. **SC01 Plus:** driver ST7796, dotyk FT6336, ověření obdélníkového rozvržení na 480×320, profil
   buildu.
4. **Ochrana OTA a dokumentace.**

Deska 4.3 jde první, protože používá stejnou cestu přes RGB displej a GT911 – přepínač cíle,
API expandéru a obdélníkové rozvržení se na ní vyzkouší s menším rizikem. SC01 Plus pak přidá už
jen driver displeje s paralelní sběrnicí a dotyk FT6336.

**Rozsah (odhad):** nové soubory `Display.h`, `Display_RGB.cpp`, `Expander.h`, `CH422G.cpp`,
`Display_ST7796.cpp`, `Touch_FT6336.h/.cpp`; zhruba 15 upravených.

## 9. Rizika

**Společná**

- **Regrese na kulatých deskách** – po kroku 1–4 je nutné znovu otestovat 2.1 (a 2.8C, pokud je
  k dispozici).
- **Piny a zapojení** jsou převzaté z oficiální dokumentace (4.3) a veřejných konfigurací
  (SC01 Plus). Body označené „ověřit“ je potřeba potvrdit ve schématech.

**Waveshare 4.3**

- **Zatížení PSRAM.** Panel 800×480 při 16 MHz čte z PSRAM 32 MB/s, dvakrát víc než kulatá deska
  při 8 MHz. Ujíždění obrazu při zátěži WiFi je u této desky známý problém. Bounce buffery už kód
  má; pixel clock bude potřeba vyladit zhruba v rozmezí 12–16 MHz. Pod asi 12 MHz klesne
  obnovovací frekvence pod 30 Hz a obraz začne blikat.
- **Vnitřní RAM.** Bounce buffery zaberou 2 × 16 kB = 32 kB místo dnešních 19 kB, pro TLS zbude
  o 13 kB méně (`NET_MIN_HEAP`). Ověřit volnou haldu ve `/api/status`.
- **Kreslení** do 768 kB framebufferu v PSRAM je pomalejší než dnes; pro dashboard, který se
  překresluje jednou za sekundy, to nevadí.
- **Komentáře a ladicí výpisy** počítají se snímkem ~34 ms (`FLUSH_DEBUG`); při 16 MHz má snímek
  25,6 ms.
- **Velikost flash.** Definice desky v core nabízí i 8 MB, což ukazuje na starší revizi. Ověřit
  značení modulu (N16R8) nebo `esptool flash_id`.

**SC01 Plus**

- **Barvy, inverze a zrcadlení panelu ST7796U** se budou muset doladit na konkrétním kusu.
- **Pořadí bajtů RGB565** – po 8bitové sběrnici jde vyšší bajt první. Knihovna GFX to řeší sama;
  při vlastním DMA přenosu by bylo nutné bajty prohodit.
- **Trhání obrazu** – bez signálu TE může být při překreslení celé obrazovky vidět trhání.
  U dashboardu je to prakticky zanedbatelné.
- **Paměť** – 2 MB QSPI PSRAM je menší a pomalejší, na canvas (300 kB) a síťové buffery ale stačí.

## 10. Otevřená rozhodnutí

| Otázka | Možnosti | Doporučení |
| --- | --- | --- |
| Tovární reset (BOOT na GPIO0) | a) jedna cesta pro všechny desky: při startu čekat na držené BOOT ~3 s **před** inicializací displeje, pak hláška „Erasing settings“; b) kulaté desky jako dnes (s odpočtem na displeji), nové desky podle a); c) na nových deskách jen přes web (`/api/reset`) | **a)** – jednodušší, drobná změna chování i na kulatých deskách (bez hlášky během držení) |
| Jas na desce 4.3 | skrýt posuvník / nahradit přepínačem zap-vyp / úprava hardwaru na PWM | **skrýt posuvník**, podsvícení trvale zapnuté |
| Pixel clock na desce 4.3 | 16 MHz (Waveshare) / nižší | začít na **16 MHz**, snížit při ujíždění obrazu |
| Orientace SC01 Plus | na šířku 480×320 / na výšku 320×480 | **na šířku** – stejné rozvržení jako 4.3 |
| Výběr cíle | automaticky (definice desky + PSRAM) / ručně v `Config.h` | **automaticky**, s možností přebít přes `-D` |
| Jedna binárka 4.3 + kulaté desky | ano (detekce za běhu) / ne | **zatím ne**, vrátit se k tomu později |

## 11. Ověřit na hardwaru / ve schématu

- [ ] 4.3: LCD RESET na CH422G EXIO3
- [ ] 4.3: velikost flash (N16R8 vs. starší 8 MB)
- [ ] 4.3: definice desky `waveshare_esp32_s3_touch_lcd_43` v core 3.3.10
- [ ] SC01 Plus: reset dotyku (společný s GPIO4?)
- [ ] SC01 Plus: USB-C na nativním USB, nebo přes převodník
- [ ] SC01 Plus: dostupnost tlačítka BOOT
- [ ] názvy maker `CONFIG_SPIRAM_MODE_OCT` / `CONFIG_SPIRAM_MODE_QUAD` v core

## 12. Zdroje

**Waveshare ESP32-S3-Touch-LCD-4.3**

- [Oficiální dokumentace Waveshare](https://docs.waveshare.com/ESP32-S3-Touch-LCD-4.3)
  (parametry, piny, CH422G EXIO, USB/CAN)
- [Schéma](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-4.3/ESP32-S3-Touch-LCD-4.3-Sch.pdf)
- [Konfigurace desky v ESP32_Display_Panel](https://github.com/esp-arduino-libs/ESP32_Display_Panel/blob/master/src/board/supported/waveshare/BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_4_3.h)
  (časování RGB, reset LCD a dotyku přes CH422G, podsvícení jako spínač)
- [Definice pinů v arduino-esp32](https://github.com/espressif/arduino-esp32/blob/master/variants/waveshare_esp32_s3_touch_lcd_43/pins_arduino.h)
  a lokální `boards.txt` z core 3.3.12 (menu PSRAM / flash)
- [CH422G v ESP32_IO_Expander](https://github.com/esp-arduino-libs/ESP32_IO_Expander/blob/master/src/port/esp_io_expander_ch422g.c)
  a [ESPHome](https://api-docs.esphome.io/ch422g_8cpp_source) (I2C adresy příkazů)
- [Regulace jasu – diskuse](https://github.com/esp-arduino-libs/ESP32_Display_Panel/discussions/185)
  a [ESPHome projekt pro 4.3](https://github.com/bundoon/Waveshare-ESP32-S3-Touch-LCD-4.3-Working-with-ESPHome)
  (podsvícení jen zap/vyp)

**SC01 Plus**

- [WT32-SC01-PLUS – dokumentace projektu s konfigurací LovyanGFX](https://github.com/SubCoderHUN/WT32-SC01-PLUS/blob/main/docs/WT32-SC01-PLUS_DOCUMENTATION_EN.md)
  (piny displeje, podsvícení, dotyku, SD karty a I2S)
- [RIOT OS – ESP32-S3 WT32-SC01 Plus](https://api.riot-os.org/group__boards__esp32s3__wt32__sc01__plus.html)
  (popis desky, INT dotyku)
- [Home Assistant Community – WT32-SC01 Plus fully working with PSRAM](https://community.home-assistant.io/t/w32-sc01-plus-fully-working-with-psram/988753)
  (PSRAM v režimu quad)
- [LVGL Forum – V9 on WT32-SC01 Plus](https://forum.lvgl.io/t/v9-on-wt32-sc01-plus/15069)
- Výrobce: `doc.smartpanle.cn` (schéma, datasheet)
