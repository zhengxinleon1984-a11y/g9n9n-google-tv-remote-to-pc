# Safe Application-Only Flashing

## Critical rule

The ES3C28P retains the original bootloader and partition layout. Write only
the application image:

```text
0x0020000
0x0410000
```

Never write these regions during a normal upgrade:

```text
0x0000000  bootloader
0x0008000  partition table
0x0009000  NVS
```

Do not flash a PlatformIO-generated bootloader to `0x0`.

## Backup

Before writing, read the complete 16MB flash and verify:

- file size is exactly `16777216` bytes
- SHA256 is recorded
- the backup is stored outside the repository

Full flash backups may contain Wi-Fi and BLE credentials. Do not publish them.

## Windows port behavior

When the application is running, the native USB composite device may expose a
CDC port such as `COM5`. esptool can fail to claim that port because it is a
composite CDC interface.

After a failed claim/reset attempt, the ESP32-S3 ROM bootloader can re-enumerate
as a different port such as `COM4`. Confirm the chip and flash size before
writing.

## Example

```powershell
python -m esptool `
  --chip esp32s3 `
  --port COM4 `
  --baud 921600 `
  write-flash `
  --flash-mode dio `
  --flash-size 16MB `
  --flash-freq 80m `
  0x20000 firmware.bin `
  0x410000 firmware.bin
```

Both writes must finish with:

```text
Hash of data verified.
```
