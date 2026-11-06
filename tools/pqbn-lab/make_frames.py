#!/usr/bin/env python3
"""Native 4 bpp frames from logical pictures, the way the factory driver's framebuffer lays them out.

make_frames.py OUTDIR [--gray-as LEVEL] name=picture.png ...   Pictures are 684x1216; --gray-as moves the fills
of today's gray (level 8) to another level, to see an older theme.
"""
import os
import sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
W, H = 1216, 684


def native(im, gray_as=None):
    px = im.convert("L").resize((684, 1216)).load()
    out = bytearray(W // 2 * H)
    for ly in range(1216):
        for lx in range(684):
            nx, ny = ly, 683 - lx
            v = px[lx, ly] >> 4
            if gray_as is not None and v == 8:
                v = gray_as
            i = ny * (W // 2) + nx // 2
            out[i] |= v << 4 if nx & 1 else v
    return bytes(out)


def main():
    args = sys.argv[1:]
    out = args.pop(0)
    gray_as = None
    if args and args[0] == "--gray-as":
        args.pop(0)
        gray_as = int(args.pop(0))
    os.makedirs(out, exist_ok=True)
    for a in args:
        name, path = a.split("=", 1)
        open(os.path.join(out, name + ".raw"), "wb").write(native(Image.open(path), gray_as))


if __name__ == "__main__":
    main()
