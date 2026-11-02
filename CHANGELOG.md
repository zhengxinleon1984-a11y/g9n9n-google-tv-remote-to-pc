# Changelog

## 1.2.3-g9n9n-lcd

- Added ES3C28P ILI9341 status display.
- Added BLE, ATVV, audio frame, UAC, AGC, buffer, and uptime diagnostics.
- Added status telemetry to `/api/status`.
- Added Chinese 4bpp status-font subset generation.
- Preserved the existing G9N9N HID and UAC behavior.
- Reduced LCD SPI transactions from 20-row to 4-row chunks and paused status
  rendering while BLE voice or UAC streaming is active. This prevents the LCD
  refresh from starving the 2 ms isochronous UAC IN endpoint.
- Verified Windows DirectShow capture and a real G9N9N voice session with
  `uac_streaming=true`, continuously increasing UAC transfers, decoded ATVV
  frames, no re-enumeration, and no UAC error.

## 1.2.2-g9n9n

- Added conservative near-field AGC.
- Added keycode help dialog.
- Improved custom key recording UI.
- Fixed voice session restart and UAC recovery.
