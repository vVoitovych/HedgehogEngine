"""Writes Projects/FeatureTest/Assets/Textures/Luts/Neutral.png and Warm.png.

Colour grading LUTs in the layout HedgehogRenderer's src/Graph/ColorLut.hpp reads: a 1024 x 32 strip
of 32 tiles, tile b holding blue b / 31, red along x and green along y, display-encoded (sRGB).
Neutral changes nothing; Warm lifts red, keeps green and pulls blue down, a late-afternoon look.
Paint over Neutral.png in an image editor to make a LUT of your own.
Run from the repository root: python Tools/PostProcess/make_color_luts.py
"""
import os
import struct
import zlib

SIZE = 32
OUT = "Projects/FeatureTest/Assets/Textures/Luts"


def write_png(path, width, height, rgb):
    raw = b"".join(b"\x00" + bytes(rgb[y * width * 3:(y + 1) * width * 3]) for y in range(height))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def make(grade):
    pixels = []
    for y in range(SIZE):
        for x in range(SIZE * SIZE):
            r, g, b = (x % SIZE) / (SIZE - 1), y / (SIZE - 1), (x // SIZE) / (SIZE - 1)
            for c in grade(r, g, b):
                pixels.append(round(255 * min(max(c, 0.0), 1.0)))
    return pixels


def warm(r, g, b):
    return (r * 1.06 + 0.03, g * 1.01 + 0.005, b * 0.86)


os.makedirs(OUT, exist_ok=True)
write_png(os.path.join(OUT, "Neutral.png"), SIZE * SIZE, SIZE, make(lambda r, g, b: (r, g, b)))
write_png(os.path.join(OUT, "Warm.png"), SIZE * SIZE, SIZE, make(warm))
print("Wrote", OUT)
