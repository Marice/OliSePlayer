#!/usr/bin/env python3
"""Generate sce_sys/icon0.png (512x512): the logo over a starfield.

Usage: tools/make_icon.py   (run make_logo.py first)
"""
import os
import random

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def main():
    random.seed(1337)
    size = 512
    img = Image.new("RGBA", (size, size), (0, 0, 0, 255))
    px = img.load()
    for _ in range(260):
        x, y = random.randrange(size), random.randrange(size)
        v = random.choice([90, 120, 160, 200, 255])
        px[x, y] = (v, v, min(255, v + 20), 255)
        if v > 180 and random.random() < 0.4:
            for dx, dy in ((1, 0), (0, 1), (-1, 0), (0, -1)):
                if 0 <= x + dx < size and 0 <= y + dy < size:
                    px[x + dx, y + dy] = (v // 2, v // 2, v // 2 + 10, 255)

    logo = Image.open(os.path.join(ROOT, "assets", "logo.png")).convert("RGBA")
    w = 470
    h = int(logo.height * w / logo.width)
    logo = logo.resize((w, h), Image.NEAREST)
    img.alpha_composite(logo, ((size - w) // 2, (size - h) // 2 - 30))

    draw = ImageDraw.Draw(img)
    font_path = "/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf"
    font = ImageFont.truetype(font_path, 34) if os.path.exists(font_path) else ImageFont.load_default()
    text = "TRACKER RADIO FOR PS5"
    tw = draw.textlength(text, font=font)
    x, y = (size - tw) / 2, (size + h) // 2 + 10
    draw.text((x + 2, y + 2), text, font=font, fill=(0, 0, 0, 255))
    draw.text((x, y), text, font=font, fill=(0xc0, 0xc0, 0xe0, 255))

    os.makedirs(os.path.join(ROOT, "sce_sys"), exist_ok=True)
    img.convert("RGB").save(os.path.join(ROOT, "sce_sys", "icon0.png"))
    print("icon0.png written")


if __name__ == "__main__":
    main()
