# DevOps Dashboard 4.3

A two-screen DevOps dashboard for the **Waveshare ESP32-S3-Touch-LCD-4.3**
(SKU 25948, 800×480 touchscreen), drawn with **LVGL 9**.

Experimental successor of [DevOpsDashboard](../DevOpsDashboard) (round 2.1" /
2.8C boards), which is based on
[MeteoPlaneRadar](https://github.com/petus/MeteoPlaneRadar) by Petr /
chiptron.cz (MIT). The WiFi portal, the web settings page, OTA and the
watchdogs are taken over from there; the display, touch and drawing are new.
Plan and progress: [../Doc/DevOpsDashboard43-plan.md](../Doc/DevOpsDashboard43-plan.md).

## Screens

| # | Screen | Content |
| --- | --- | --- |
| 0 | Dashboard | ring chart of pipeline results in the last 24 h with a legend (**test data** – no REST API yet) |
| 1 | Clock | large time and date |

**Swipe left / right** switches between them; the dots in the status bar show
which one is active. The time comes from NTP (the HTTP `Date` header is a
fallback for networks that block NTP).

## Build

Everything goes through the arduino-cli **profile** in [sketch.yaml](sketch.yaml),
which pins the versions and makes arduino-cli install its own copy of the core
and the libraries (into `%LOCALAPPDATA%\Arduino15\internal`):

| Package | Version |
| --- | --- |
| esp32 by Espressif (core) | 3.3.12 |
| lvgl | 9.6.0 |
| ESP32_Display_Panel | 1.0.4 |
| ESP32_IO_Expander | 1.1.1 |
| esp-lib-utils | 0.2.3 (must stay below 0.3.0) |
| ArduinoJson | 7.4.3 |

```powershell
DevOpsDashboard43\build.cmd          # compile only (the first build takes ~15 min)
DevOpsDashboard43\build.cmd COM9     # compile and upload
```

Board: *Waveshare ESP32-S3-Touch-LCD-4.3*, PSRAM enabled, 16 MB flash, QIO,
**Partition Scheme: Custom** ([partitions.csv](partitions.csv)), USB CDC On
Boot disabled. `lv_conf.h` and `esp_panel_board_supported_conf.h` sit in the
sketch folder; the core puts the sketch folder on every library's include path.

The binaries land in `DevOpsDashboard43\build\`:

- `DevOpsDashboard43.ino.bin` – for the OTA update page,
- `DevOpsDashboard43.ino.merged.bin` – the complete flash image for USB.

Serial log: 115200 Bd on the USB-C connector marked **USB TO UART**.

Fonts: [src/fonts](src/fonts) holds Inter (SIL OFL 1.1) converted for LVGL.
[tools/gen_fonts.cmd](tools/gen_fonts.cmd) regenerates them (needs Node.js).

## First start

1. Flash the firmware and power the board.
2. Join the open WiFi network **`DevOpsDashboard43`** (QR code on the display).
3. The setup page opens by itself (or go to `http://192.168.4.1/`). Pick your
   network, type the password, press **Connect**.
4. The device joins the network and shows the dashboard.

**Factory reset:** press RESET, then within one second press and hold **BOOT**
for 3 s (the screen stays dark; the serial log confirms). BOOT is also a data
line of the display, so it can only be read during that first second.

## Web interface

`http://devopsdashboard43.local/` (or the IP address shown in the serial log):

- **WiFi** – up to three remembered networks.
- **DevOps API** – URL and refresh interval (stored; nothing is fetched yet).
- **Display** – brightness, previous / next screen. The board can only switch
  the backlight on or off, so brightness is a black layer over the picture;
  0 % switches the backlight off.
- **System** – status, firmware update (`/update`), admin password, reboot,
  factory reset.

| Endpoint | Method | Purpose |
| --- | --- | --- |
| `/api/status` | GET | version, board, IP, WiFi, time, heap, PSRAM, running OTA slot (`app`), display health (`lvglAgeMs`, `frames`), API and NTP state |
| `/api/config` | GET / POST | `apiUrl`, `apiIntervalSec`, `brightness`; `password` + `newPassword` |
| `/api/screen` | POST | `{"index":0}` or `{"step":1}` |
| `/api/scan` | GET | WiFi networks in range |
| `/api/wifi` | POST | `{"ssid":"..","pass":".."}` – remember and connect |
| `/api/wifilist` | POST | `{"nets":[{"ssid":"..","pass":".."}, ...]}` – save without connecting |
| `/api/reboot` | POST | restart |
| `/api/reset` | POST | factory reset (`{"password":".."}` when one is set) |
| `/update` | GET / POST | firmware update (HTTP Basic user `admin` when a password is set) |

## How it works

- [LvPort.cpp](LvPort.cpp) – ESP32_Display_Panel brings up the panel, touch and
  expander from its board definition. LVGL draws in **DIRECT** mode into two
  full-screen frame buffers in PSRAM that swap at the end of a frame (no
  tearing, ~30 FPS, 2–3 ms of drawing per animation frame).
- LVGL runs in its own task; `loop()` never draws and changes the UI only
  through `Ui_*` functions, which take the LVGL lock.
- [Ui.cpp](Ui.cpp) – status bar, `lv_tileview` with the screens, overlays (WiFi
  portal with a QR code, messages) and the dimming layer.
- Watchdogs: the task watchdog for `loop()`, plus `loop()` restarts the board
  when the LVGL task (10 s) or the panel's frame counter (3 s) stops.
- Programmer documentation (Czech):
  [../Doc/DevOpsDashboard43-programatorska-dokumentace.md](../Doc/DevOpsDashboard43-programatorska-dokumentace.md).

## Firmware update

Open `/update`, pick `DevOpsDashboard43.ino.bin` (not the *merged* one), or from
the command line:

```powershell
curl.exe -F "update=@DevOpsDashboard43\build\DevOpsDashboard43.ino.bin" http://<ip>/update
```

The display shows "Updating firmware..." for 1.5 s and then goes dark while
the flash is written (with the backlight on, the picture shakes). On success
it shows "Done, restarting..." and restarts; `/api/status` then reports the
other OTA slot in `app`.

**Known issue:** an upload can stall half way ("Empty reply from server"),
mostly the first one after the device has been idle for a minute or more. The
device stays on the old firmware and keeps working – just try again, or flash
over USB. Details and what to try next: the plan, phase 5.2.

## Tested (0.1.0)

- display, touch, swipe, ~30 FPS without tearing
- captive portal with QR code, WiFi scan, connect, reconnect after restart, NTP
- web page, `/api/*`, software dimming 0–100 %, last screen restored
- settings writes to flash without picture glitches
- factory reset with BOOT, restart from the web (`resetReason: software`)
- LVGL watchdog (test build), OTA (works, see the known issue)

Not tested yet: WiFi outage, a 24-hour run, the panel-stall branch of the
watchdog.

## Licence

MIT – see [LICENSE.txt](LICENSE.txt) (copyright of the original author kept).
The Inter font is under the SIL Open Font License 1.1 ([src/fonts/OFL.txt](src/fonts/OFL.txt)).
