# Changelog

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
