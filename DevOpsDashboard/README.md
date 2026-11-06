# DevOps Dashboard

A two-screen DevOps dashboard for the **Waveshare ESP32-S3-Touch-LCD-2.1** round
touchscreen (480×480). The **ESP32-S3-Touch-LCD-2.8C** works too – one binary
serves both, the board is detected at boot.

Based on [MeteoPlaneRadar](https://github.com/petus/MeteoPlaneRadar) by
Petr / chiptron.cz (MIT). The display, touch, WiFi portal, OTA update and the
reliability parts (watchdogs) are kept; everything weather/aircraft/energy
related is removed. See [../Doc/DevOpsDashboard-plan.md](../Doc/DevOpsDashboard-plan.md)
for a description of the original project and of every step taken.

## Screens

| # | Screen | Content |
| --- | --- | --- |
| 0 | Dashboard | "DevOps dashboard" title |
| 1 | Blank | only the clock |

**Swipe left / right** switches between them; the dots at the top show which
one is active. The clock at the top comes from NTP (the HTTP `Date` header is a
fallback for networks that block NTP).

## Build

Arduino IDE 2.x or arduino-cli with:

- **esp32 by Espressif** core **3.3.10**
- **GFX Library for Arduino** 1.6.9
- **ArduinoJson** 7.4.3

Board settings: *ESP32S3 Dev Module*, **PSRAM: OPI PSRAM**, Flash Size 16 MB,
Flash Mode QIO, **Partition Scheme: Custom** (`partitions.csv`), USB CDC On
Boot: Enabled.

From the command line (uses the arduino-cli bundled with Arduino IDE):

```powershell
DevOpsDashboard\build.cmd          # compile only
DevOpsDashboard\build.cmd COM3     # compile and upload
```

The binaries land in `DevOpsDashboard\build\`:

- `DevOpsDashboard.ino.bin` – for the OTA update page,
- `DevOpsDashboard.ino.merged.bin` – the complete flash image for USB.

Serial log: 115200 Bd on the USB-C connector marked **USB**.

## First start

1. Flash the firmware and power the board.
2. Join the open WiFi network **`DevOpsDashboard`** (QR code on the display).
3. The setup page opens by itself (or go to `http://192.168.4.1/`). Pick your
   network, type the password, press **Connect**.
4. The device joins the network and shows the dashboard.

Hold **BOOT** for ~3 s while powering up to erase all settings (factory reset).

## Web interface

`http://devopsdashboard.local/` (or the IP address shown in the serial log):

- **WiFi** – up to three remembered networks.
- **DevOps API** – URL and refresh interval.
- **Display** – brightness, previous / next screen.
- **System** – status, firmware update (`/update`), admin password, reboot,
  factory reset.

| Endpoint | Method | Purpose |
| --- | --- | --- |
| `/api/status` | GET | version, board, IP, WiFi, time, heap, API and NTP state |
| `/api/config` | GET / POST | `apiUrl`, `apiIntervalSec`, `brightness`; `password` + `newPassword` |
| `/api/screen` | POST | `{"index":0}` or `{"step":1}` |
| `/api/scan` | GET | WiFi networks in range |
| `/api/wifi` | POST | `{"ssid":"..","pass":".."}` – remember and connect |
| `/api/wifilist` | POST | `{"nets":[{"ssid":"..","pass":".."}, ...]}` – save without connecting |
| `/api/reboot` | POST | restart |
| `/api/reset` | POST | factory reset (`{"password":".."}` when one is set) |
| `/update` | GET / POST | firmware update (HTTP Basic user `admin` when a password is set) |

The admin password is optional and off by default. It is stored in the clear –
it protects against the household, not against an attacker on the network.

## Tested (0.1.0, Waveshare 2.1)

- board detection, display, touch (CST820)
- captive portal, WiFi connection, NTP clock
- swipe between the two screens, last screen restored after restart
- web page and all `/api/*` endpoints, brightness, URL validation
- OTA update over `/update`

Not tested yet: the 2.8C board, a 24-hour run.

## Licence

MIT – see [LICENSE.txt](LICENSE.txt) (copyright of the original author kept).
The Inter font is under the SIL Open Font License 1.1.
