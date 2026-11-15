# Changelog

## 0.2.0 (unreleased)

The dashboard shows real data: the Poirot computers from the REST API.

- `DevOpsApi`: the download runs in a FreeRTOS task (core 0), every 30 s by
  default, with a manual refresh; retry after 5 s on a network error or a 5xx;
  the answer is dropped when the profile changed meanwhile.
- `Net_GetString`: extra request header (`X-Api-Key`), HTTP code and `Content-Type`
  check (a web page instead of JSON = "not JSON"), TLS **verified** against the
  CA bundle of the core instead of `setInsecure()` (`NET_TLS_INSECURE` is the
  emergency switch), last TLS error in the log. The key is never logged.
- Fix: `Net_ReadBody` could not terminate a body that was exactly as long as
  announced in `Content-Length`.
- Settings: three API profiles (name, base URL, key) and the active one; the
  key is write-only. The single `apiUrl` of 0.1.0 moves to profile 1.
- `Status` is thread-safe (written by the API task, read by the web server).
- Dashboard: ring of Running / Waiting / Warning / Error with the server total,
  grey ring for missing or old data, status line (age or error).
- Status bar: active profile name, ⟳ icon (a spinner replaces it while
  downloading, hidden when there is nothing to fetch). Montserrat 24 is enabled
  for the symbol only.
- Fix: bounce buffer 20 lines instead of 10. The refill sometimes came too late
  while LVGL was drawing, and the panel repeated rows from 20 lines higher (ghost
  text under the title, streaks at the ring, flicker during every redraw).
  Costs 32 kB more internal RAM.
- Fix: the whole picture could stay shifted (by one bounce block) after a lost
  DMA interrupt - a bug of ESP-IDF 5.5.5 (espressif/esp-idf#19070). The
  driver's bounce buffer count is reset at every VSYNC, as the upstream fix
  does; a lost interrupt now costs one frame. `/api/status` has `panelResyncs`.
- Web: DevOps API section with profiles, active profile, interval and a Test
  button per profile (`/api/apitest`); `/api/status` has `apiDetail`.

## 0.1.0 (unreleased)

First version of the experimental DevOps dashboard for the Waveshare
ESP32-S3-Touch-LCD-4.3 on LVGL 9.6, built from DevOpsDashboard 0.1.0.

- LVGL 9 port (`LvPort`): ESP32_Display_Panel board definition, DIRECT render
  mode with two PSRAM frame buffers swapped at the end of a frame, LVGL task
  with a recursive lock, GT911 touch.
- Inter fonts converted for LVGL (`tools/gen_fonts.cmd`).
- Screens in an `lv_tileview`: ring chart of pipeline results (test data) and
  a clock; status bar with title, page dots and time.
- WiFi portal with an `lv_qrcode`, web settings page, `/api/*`, OTA at
  `/update`, settings in NVS - taken over from DevOpsDashboard.
- Software dimming (black layer); 0 % switches the backlight off.
- Factory reset: RESET, then hold BOOT within 1 s for 3 s.
- Display watchdog (LVGL task and panel frame counter), clean restart that
  stops the RGB panel first, WiFi modem sleep off.
- Known issue: OTA can stall after the device has been idle (see README).
