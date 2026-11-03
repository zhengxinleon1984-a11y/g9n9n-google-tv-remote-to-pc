# Idle voice recovery

## Problem

The remote could remain idle for a long time and then stop starting voice
sessions until the device was rebooted from its web page. The USB microphone and
BLE link could still appear mounted, so a simple process restart was hiding the
real state-machine problem.

The previous watchdog only handled `BLE_STATE_TALKING` with no audio for 10
seconds. It did not cover:

- the BLE task blocking forever inside the wrong `NimBLEScan::start()` overload;
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
5. A voice session with no AUDIO or SYNC for 5 seconds is force-released. Two
   timeouts within 30 seconds escalate to one BLE reconnect.
6. The watchdog reads `millis()` again immediately before evaluating the
   timeout. It must not reuse the `now` value cached at the start of a long task,
   because that made a live ATVV session look stale and forced a false release.
7. After 15 minutes without HOGP, ATVV or audio activity, and only when neither
   a voice session nor a UAC stream is active, perform the first maintenance
   reboot. After a maintenance reboot, another one is allowed only after a
   60-minute cooldown, so an unused device cannot enter a rapid reboot loop but
   also cannot remain stuck forever. Any later button or ATVV activity clears
   the marker and restores the 15-minute first threshold.
8. The marker uses the versioned NVS key `idle_reset_done_v2`. Version 1.2.7
   removes the old `idle_reset_done` key during initialization because a stale
   marker from 1.2.6 could otherwise disable maintenance permanently after a
   reboot or reflash.
9. Maintenance and the BLE stall watchdog run in the main Arduino loop, never
   inside the BLE task they are meant to recover. The BLE task publishes a
   heartbeat every loop; if it is missing for 30 seconds, the device restarts,
   with a persisted three-attempt limit to avoid an unbounded reboot loop.
10. Startup scanning uses `NimBLEScan::start(0, nullptr, false)`. Calling
    `start(0, false)` selects the blocking overload, where duration zero means
    scan forever. Version 1.2.9 verified that this was the root cause: the BLE
    task produced scan callbacks but never returned to its own loop.
11. ATVV control notifications are not all user activity. The `CAPS` response
    produced by the 60-second health refresh must extend the ATVV watchdog but
    must not reset the 15-minute idle-maintenance timer. Version 1.2.10 records
    user activity only for HOGP keys, voice start/release, AUDIO_SYNC and real
    audio frames.

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
ble_task_heartbeat_age_ms
```

Healthy idle operation normally shows:

```text
ble_link_connected=true
atvv_refresh_count increasing about once per minute
atvv_refresh_fail_count=0
ble_recovery_count=0
voice_timeout_count=0
idle_maintenance_done=True
idle_maintenance_due_ms=0
ble_task_heartbeat_age_ms=1
```

## Verification

- PlatformIO build: success.
- Native tests: 6/6 passed.
- Hardware speech test: successful.
- A 3-second threshold produced one false timeout when a valid SYNC arrived at
  the boundary. Version 1.2.5 uses the volatile ATVV activity timestamp and a
  5-second threshold instead.
- Version 1.2.6 fixes a second timeout bug: the watchdog had still reused the
  stale `now` captured at the start of `ble_remote_task()`. It now reads
  `millis()` at the watchdog check itself.
- Observed after an idle period:

```text
version=1.2.10-g9n9n-idle-activity-fix
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
ble_task_heartbeat_age_ms=1
```

The blocking scan bug was confirmed on hardware: version 1.2.8 reported
`ble_task_heartbeat_age_ms=4294967295` even though scan callbacks and the web
server were active. After changing to the non-blocking overload, version 1.2.9
reported a 1-3 ms heartbeat and logged
`Continuous scanning for Xiaomi Bluetooth Remote active...`.

Version 1.2.10 idle-maintenance verification:

```text
08:05:49 up=1142s rst=0 hb=2ms idle_age=876.2s due=23.8s
08:06:26 up=13s   rst=3 hb=2ms idle_age=5.6s  due=894.4s *** REBOOT ***
08:06:56 up=43s   rst=3 hb=1ms idle_age=35.8s due=864.2s
```

The reboot occurred after approximately 900 seconds without user activity.
BLE reconnected automatically, UAC remained mounted after restart, the task
heartbeat stayed in the 0-4 ms range, and no voice timeouts were observed.
