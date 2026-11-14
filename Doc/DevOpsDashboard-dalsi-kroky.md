# DevOps Dashboard – stav a návod na dokončení

> Související dokumenty: [plán a dokumentace projektu](DevOpsDashboard-plan.md),
> [výukový průvodce REST API](DevOpsDashboard-REST-API.md),
> [podpora dalších desek](DevOpsDashboard-dalsi-desky.md),
> [plán DevOpsDashboard43 (Waveshare 4.3 + LVGL 9)](DevOpsDashboard43-plan.md),
> [README firmwaru](../DevOpsDashboard/README.md).

## 1. Aktuální stav (firmware 0.1.0)

| Oblast | Stav |
| --- | --- |
| Fáze 0–4 plánu (kopie, 2 obrazovky, odstranění modulů, web) | hotovo, commitnuto |
| Build | firmware 1,34 MB, globální proměnné 52 kB |
| Ověřeno na desce 2.1 | detekce desky, displej, dotyk, WiFi portál, NTP čas, swipe, obnovení poslední obrazovky po restartu, web a `/api/*` |
| OTA přes `/update` | 1× prošla, 2× „Empty reply from server“ – **nevyřešeno** |
| Oprava `Serial.flush()` v `DevOpsDashboard.ino` | **necommitnuto, na desce neověřeno** |
| `DevOpsDashboard/README.md` | **necommitnuto** |
| Fáze 6 (napojení REST API) | **nezačato** – návrh v [DevOpsDashboard-REST-API.md](DevOpsDashboard-REST-API.md) |
| Deska 2.8C, 24h běh | neověřeno |
| Další desky: Waveshare 4.3 (SKU 25948), SC01 Plus 3.5" | **analýza a plán** v [DevOpsDashboard-dalsi-desky.md](DevOpsDashboard-dalsi-desky.md), neimplementováno |
| Nový projekt `DevOpsDashboard43` (Waveshare 4.3 + LVGL 9) | **plán** v [DevOpsDashboard43-plan.md](DevOpsDashboard43-plan.md), neimplementováno |

## 2. Prostředí (jednorázově, už je nainstalováno)

- Arduino IDE 2.x, **esp32 core 3.3.10**, **GFX Library for Arduino 1.6.9**, **ArduinoJson 7.4.3**
- Nastavení desky: *ESP32S3 Dev Module*, **PSRAM: OPI PSRAM**, Flash 16 MB QIO,
  **Partition Scheme: Custom**, USB CDC On Boot: Enabled
- Deska je na portu **COM3** (konektor označený **USB**)

## 3. Build a nahrání

```powershell
cd f:\GitHub-KarelKral-Repos\MeteoPlaneRadar
DevOpsDashboard\build.cmd          # jen kompilace (první build ~10 min, další rychleji)
DevOpsDashboard\build.cmd COM3     # kompilace + nahrání přes USB
```

Výstupy v `DevOpsDashboard\build\`:

- `DevOpsDashboard.ino.bin` – pro OTA (`http://<ip-zařízení>/update`)
- `DevOpsDashboard.ino.merged.bin` – celý obraz flash pro USB / webový flasher

Sériový výpis: 115 200 Bd (Arduino IDE → Serial Monitor, nebo
`arduino-cli monitor -p COM3 -c baudrate=115200`).

## 4. Zbývající kroky

### Krok 1 – ověřit a commitnout opravu restartu

Problém: `/api/reboot` bez připojeného počítače skončil resetem od watchdogu, protože
`Serial.flush()` na nativním USB čeká na hosta. Oprava (odstranění `Serial.flush()` ve funkcích
`restartIfAsked()` a `displayWatchdog()`) je v `DevOpsDashboard.ino`.

1. Nahrát přes USB: `DevOpsDashboard\build.cmd COM3`
2. Odpojit Serial Monitor (port nesmí být otevřený).
3. `Invoke-WebRequest http://<ip>/api/reboot -Method Post -Body '{}' -ContentType 'application/json'`
4. Po ~15 s: `GET http://<ip>/api/status` → `"resetReason"` musí být `"software"`, ne `"WATCHDOG"`.
5. Commit:
   ```powershell
   git add DevOpsDashboard/DevOpsDashboard.ino DevOpsDashboard/README.md
   git commit -m "Phase 5: drop Serial.flush() before restart and add README"
   ```

### Krok 2 – (volitelně) prošetřit OTA

Příznak: `curl -F "update=@DevOpsDashboard.ino.bin" http://<ip>/update` vrátil „Empty reply“,
deska zůstala na staré verzi (log ukázal `OTA: DevOpsDashboard.ino.bin` dvakrát).

Co zkusit:

- nahrát přes webovou stránku `/update` v prohlížeči místo `curl`,
- sledovat sériový výpis během nahrávání (chybová hláška `Update.errorString()`),
- ověřit, že se neplete `DevOpsDashboard.ino.bin` s `*_flashed.bin`.

Do vyřešení nahrávejte firmware přes USB.

### Krok 3 – fáze 6: napojení na REST API

Postup je popsán v [DevOpsDashboard-REST-API.md](DevOpsDashboard-REST-API.md):

1. Napsat a spustit server (příklad ASP.NET Core v kap. 2.3), ověřit `curl` z jiného PC.
2. Přidat `DevOpsApi.h/.cpp` (kap. 4.1–4.2).
3. V `DevOpsDashboard.ino` přidat `#include "DevOpsApi.h"` a v `loop()` `DevOpsApi_Tick();`.
4. V `WebConfig.cpp` (`handlePostConfig`) zavolat `DevOpsApi_Invalidate();`.
5. Upravit `ScreenDashboard.cpp` (kap. 4.3).
6. Build, nahrát, zadat URL ve webu zařízení, zkontrolovat `"api"` v `/api/status`.
7. Commit: `git commit -m "Phase 6: fetch the DevOps REST API and draw it on the dashboard"`

### Krok 4 – závěrečné testy

- [ ] 24h běh bez restartu (`resetReason` zůstane `power on` / `software`)
- [ ] deska 2.8C (pokud je k dispozici) – výpis `Board: ...2.8C`, dotyk funguje
- [ ] výpadek API → data zešednou, po obnovení se vrátí
- [ ] změna URL ve webu → data se načtou hned

## 5. Užitečné adresy zařízení

| Adresa | Účel |
| --- | --- |
| `http://192.168.4.1/` | portál v režimu nastavení (síť `DevOpsDashboard`) |
| `http://<ip>/` nebo `http://devopsdashboard.local/` | nastavení |
| `http://<ip>/api/status` | stav (JSON) |
| `http://<ip>/update` | aktualizace firmwaru |
| BOOT 3 s při zapnutí | tovární reset |

Poslední známá IP zařízení v síti: `192.168.1.111`.
