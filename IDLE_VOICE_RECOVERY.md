# Idle voice recovery

## Problem

The remote could remain idle for a long time and then stop starting voice
sessions until the device was rebooted from its web page. The USB microphone and
BLE link could still appear mounted, so a simple process restart was hiding the
real state-machine problem.

The previous watchdog only handled `BLE_STATE_TALKING` with no audio for 10
seconds. It did not cover:

- a GATT callback that was missed while the state still claimed `CONNECTED`;
- only AUD or CTL disappearing while CMD remained present;
- an ATVV subscription that silently stopped receiving notifications during
  a long idle period;
- a session that never reached `TALKING` because the first voice notification
  was lost.

## Recovery policy

1. Every 60 seconds while idle and connected, re-subscribe to the ATVV AUD and
   CTL characteristics and send a low-risk `GET_CAPS` query.
2. If the local GATT link reports disconnected while the state is still
   `CONNECTED` or `TALKING`, restart scanning immediately.
3. If CMD, AUD or CTL is missing, run the existing ATVV background recovery and
   re-subscribe all discovered characteristics.
4. Two consecutive refresh failures trigger one BLE reconnect. Failures are
   retried after 2 seconds.
5. A voice session with no AUDIO or SYNC for 3 seconds is force-released. Two
   timeouts within 30 seconds escalate to one BLE reconnect.
6. Recovery is rate-limited by state transitions; there is no timer that reboots
   the whole device.

## Status fields

`GET http://remotemapper.local/api/status`

```text
reset_reason
ble_state
ble_link_connected
atvv_activity_age_ms
voice_timeout_count
atvv_refresh_count
atvv_refresh_fail_count
ble_recovery_count
```

Healthy idle operation normally shows:

```text
ble_link_connected=true
atvv_refresh_count increasing about once per minute
atvv_refresh_fail_count=0
ble_recovery_count=0
voice_timeout_count=0
```

## Verification

- PlatformIO build: success.
- Native tests: 6/6 passed.
- Hardware speech test: successful.
- Observed after an idle period:

```text
version=1.2.4-g9n9n-idle-recovery
ble_state=3
ble_link_connected=True
atvv_refresh_count=2
atvv_refresh_fail_count=0
ble_recovery_count=0
voice_timeout_count=0
frames_decoded=239
uac_mounted=True
uac_last_error=empty
audio_underruns=0
```
