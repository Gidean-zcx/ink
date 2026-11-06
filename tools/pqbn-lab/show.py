#!/usr/bin/env python3
"""Brightness frames from optics.c back to logical pictures: show.py out.png frame.b8 [frame.b8 ...] (side by side)."""
import sys
from PIL import Image

W, H = 1216, 684


def logical(path):
    raw = open(path, "rb").read()
    im = Image.frombytes("L", (W, H), raw)  # native: nx = ly, ny = 683 - lx
    return im.transpose(Image.TRANSPOSE).transpose(Image.FLIP_LEFT_RIGHT)


def main():
    out, frames = sys.argv[1], sys.argv[2:]
    sheet = Image.new("L", (len(frames) * 694, 1216), 128)
    for i, f in enumerate(frames):
        sheet.paste(logical(f).point(lambda v: min(255, int(v * 0.92))), (i * 694, 0))
    sheet.save(out)


if __name__ == "__main__":
    main()
