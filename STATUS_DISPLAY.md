# LCD Status Display

## Hardware

```text
Controller: ILI9341
Resolution: 320x240
Bus:        SPI3
SCLK:       GPIO12
MOSI:       GPIO11
MISO:       GPIO13
CS:         GPIO10
DC:         GPIO46
Backlight:  GPIO45
```

The LCD bus does not share the ES8311 I2S pins.

## Design

- RGB565 framebuffer in PSRAM
- Internal DMA-capable transfer buffer
- 20-row SPI chunks at 20MHz
- `MADCTL=0x28`
- `INVON (0x21)` enabled for this panel
- Dedicated Core 1 task at priority 1
- One refresh per second

## Displayed telemetry

```text
BLE state / battery
ATVV ready / frame size
audio frames / fps / age
UAC mounted / streaming / transfers
AGC gain / ring samples / underruns
overall health / web IP / uptime
```

## Fonts

The Chinese glyph set is generated from Noto Sans SC at 12px and 16px,
packed at 4bpp. Only glyphs used by the status page are embedded.

Regenerate:

```powershell
python tools/generate_status_display_font.py
```
