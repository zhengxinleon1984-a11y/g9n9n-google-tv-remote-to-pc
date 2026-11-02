# Changelog

## 1.2.5-g9n9n-idle-recovery

- Increased the stuck voice-session timeout from 3 to 5 seconds after hardware
  logs showed a false timeout when a valid SYNC arrived near the boundary.
- Made the audio timestamp volatile and based the watchdog on the synchronized
  ATVV activity timestamp, avoiding a stale cross-task read.
- Rebuilt without changing the 1.2.4 idle refresh and recovery behavior.

## 1.2.4-g9n9n-idle-recovery

- Added a 60-second idle ATVV health refresh that re-subscribes AUD/CTL and
  queries CAPS without restarting the device.
- Recover when the GATT callback is missed but the BLE state still claims to be
  connected.
- Recover when only AUD or CTL is missing instead of waiting for CMD to disappear.
- Reduced the stuck voice-session timeout from 10 seconds to 3 seconds; repeated
  timeouts escalate to one BLE reconnect.
- Added `/api/status` diagnostics: `reset_reason`, `ble_link_connected`,
  `atvv_activity_age_ms`, `voice_timeout_count`, `atvv_refresh_count`,
  `atvv_refresh_fail_count`, and `ble_recovery_count`.
- Verified on hardware with speech working, two successful idle refreshes, zero
  refresh failures, zero automatic recoveries, and zero voice timeouts.

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
