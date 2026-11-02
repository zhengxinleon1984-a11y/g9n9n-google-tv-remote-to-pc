# RemoteMapper PC Companion

Windows companion that maps F13-F24 hotkeys to local scripts or applications.

## Setup

1. Copy `config.example.json` to `config.json`.
2. Edit each `command` value for the local machine.
3. Start `start_companion.bat`.
4. Configure the ESP32 web UI to emit the matching F13-F24 hotkey.

`config.json` is intentionally ignored by Git because it can contain personal
paths, usernames, or private scripts.

## Web UI

Use the device mDNS address:

```text
http://remotemapper.local
```

If mDNS is unavailable, use the device IP shown on the LCD status screen.
