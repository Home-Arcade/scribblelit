#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""ScribbleLit's icon and banner (a lit-up scribble, drawn here), and a second of silence."""
import math
import wave
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parent.parent / "build-meta"
INK, PAPER, GLOW = (47, 72, 145), (255, 248, 225), (255, 196, 60)


def scribble(size, width):
    """Drawn big and shrunk, for smooth edges."""
    big = size[0] * 4, size[1] * 4
    image = Image.new("RGB", big, PAPER)
    draw = ImageDraw.Draw(image)
    w, h = big
    draw.ellipse((w * .55, h * .08, w * .95, h * .08 + w * .4), fill=GLOW)
    points = []
    for step in range(121):
        t = step / 120
        points.append((w * (.1 + .8 * t), h * (.62 + .22 * math.sin(t * 16) * (1 - .45 * t))))
    draw.line(points, fill=INK, width=width * 4, joint="curve")
    return image.resize(size, Image.LANCZOS), draw


icon, _ = scribble((48, 48), 4)
icon.save(OUT / "icon.png")
banner, _ = scribble((256, 128), 9)
banner.save(OUT / "banner.png")
# The picture on the letter a console gets when ScribbleLit moves into the BadgeCade helper
letter, _ = scribble((400, 240), 10)
draw = ImageDraw.Draw(letter)
font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 34)
draw.text((24, 24), "ScribbleLit", font=font, fill=INK)
letter.save(OUT / "letter.jpg", "JPEG", quality=85)
with wave.open(str(OUT / "silence.wav"), "wb") as f:
    f.setnchannels(2)
    f.setsampwidth(2)
    f.setframerate(22050)
    f.writeframes(bytes(88200))
