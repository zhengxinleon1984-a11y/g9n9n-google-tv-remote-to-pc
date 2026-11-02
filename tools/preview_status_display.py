from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
FONT_PATH = Path(r"C:\Windows\Fonts\Noto Sans SC (TrueType).otf")
OUTPUT_PATH = ROOT / "status-display-preview-4x.png"

WIDTH = 320
HEIGHT = 240

BACKGROUND = "#06111c"
HEADER_LINE = "#15344c"
PANEL = "#0b1b2a"
PANEL_BORDER = "#24445b"
TEXT = "#e7f4ff"
MUTED = "#7692a8"
DIM = "#38566c"
CYAN = "#59d8ff"
GREEN = "#46e49a"
AMBER = "#ffc857"
RED = "#ff6b72"
BLUE = "#66a7ff"

FONT_12 = ImageFont.truetype(str(FONT_PATH), 12)
FONT_16 = ImageFont.truetype(str(FONT_PATH), 16)


def text(draw: ImageDraw.ImageDraw, xy: tuple[int, int], value: str, color: str,
         font: ImageFont.FreeTypeFont) -> None:
    draw.text(xy, value, font=font, fill=color, anchor="lt")


def text_right(draw: ImageDraw.ImageDraw, right: int, y: int, value: str,
               color: str, font: ImageFont.FreeTypeFont) -> None:
    width = round(draw.textlength(value, font=font))
    text(draw, (right - width, y), value, color, font)


def text_center(draw: ImageDraw.ImageDraw, center: int, y: int, value: str,
                color: str, font: ImageFont.FreeTypeFont) -> None:
    width = round(draw.textlength(value, font=font))
    text(draw, (center - width // 2, y), value, color, font)


def flow_node(draw: ImageDraw.ImageDraw, x: int, y: int, width: int,
              label: str, color: str) -> None:
    draw.rectangle((x, y, x + width - 1, y + 27), fill=PANEL, outline=color)
    draw.rectangle((x + 3, y + 3, x + 5, y + 24), fill=color)
    text_center(draw, x + width // 2 + 2, y + 7, label, TEXT, FONT_12)


def card(draw: ImageDraw.ImageDraw, x: int, y: int, width: int, height: int,
         label: str, value: str, detail: str, accent: str) -> None:
    draw.rectangle((x, y, x + width - 1, y + height - 1),
                   fill=PANEL, outline=PANEL_BORDER)
    draw.rectangle((x, y, x + 2, y + height - 1), fill=accent)
    text(draw, (x + 7, y + 3), label, MUTED, FONT_12)
    text(draw, (x + 7, y + 18), value, TEXT, FONT_16)
    text_right(draw, x + width - 6, y + 24, detail, accent, FONT_12)


def main() -> None:
    image = Image.new("RGB", (WIDTH, HEIGHT), BACKGROUND)
    draw = ImageDraw.Draw(image)

    text(draw, (8, 4), "G9N9N", CYAN, FONT_12)
    text(draw, (50, 4), "语音链路", TEXT, FONT_12)
    text_right(draw, 312, 4, "语音传输中", GREEN, FONT_12)
    draw.line((8, 24, 311, 24), fill=HEADER_LINE)

    flow_node(draw, 8, 29, 68, "遥控器", GREEN)
    flow_node(draw, 86, 29, 68, "ATVV", GREEN)
    flow_node(draw, 164, 29, 68, "音频", GREEN)
    flow_node(draw, 242, 29, 68, "UAC", GREEN)
    draw.line((76, 42, 84, 42), fill=GREEN, width=2)
    draw.line((154, 42, 162, 42), fill=GREEN, width=2)
    draw.line((232, 42, 240, 42), fill=GREEN, width=2)

    card(draw, 8, 64, 148, 44, "遥控器 BLE", "通话中", "BAT 100%", GREEN)
    card(draw, 164, 64, 148, 44, "ATVV 麦克风", "通话中", "FS 120", GREEN)
    card(draw, 8, 112, 148, 44, "音频帧", "1288", "42fps 0.2s", GREEN)
    card(draw, 164, 112, 148, 44, "UAC 麦克风", "传输中", "TX 500/s", GREEN)
    card(draw, 8, 160, 148, 44, "AGC / 缓冲", "1.6x", "B 1280", CYAN)
    card(draw, 164, 160, 148, 44, "运行状态", "正常", "U 0", GREEN)

    draw.line((8, 210, 311, 210), fill=HEADER_LINE)
    text(draw, (8, 218), "WEB 192.0.2.1", MUTED, FONT_12)
    text_right(draw, 312, 218, "UP 00:12:34", MUTED, FONT_12)

    image.resize((WIDTH * 4, HEIGHT * 4), Image.Resampling.NEAREST).save(OUTPUT_PATH)
    print(OUTPUT_PATH)


if __name__ == "__main__":
    main()
