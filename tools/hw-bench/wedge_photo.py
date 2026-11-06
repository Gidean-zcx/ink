#!/usr/bin/env python3
"""Reads a photo of the wedge (panel_lab.c): how many pushes does a full swing take on this panel?

wedge_photo.py photo.jpg [--corners x,y,x,y,x,y,x,y] [--debug out.png]
Finds the four corner marks, straightens the screen, reads every patch against the factory's 16 gray
levels in the strip, with the white paper next to each as its local light. Needs numpy and Pillow.
"""
import argparse
import os
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image, ImageOps

W, H = 684, 1216
COLS, ROWS, X0, Y0, PATCH, PX, PY = 6, 8, 30, 118, 84, 106, 124
STRIP_Y, STRIP_H = 1120, 52
MARKS = ((17, 17), (667, 17), (17, 1199), (667, 1199))  # centres: top left, top right, bottom left, bottom right


def load(path):
    if path.lower().endswith((".heic", ".heif")):  # what an iPhone sends
        out = os.path.join(tempfile.mkdtemp(), "photo.jpg")
        subprocess.check_call(["sips", "-s", "format", "jpeg", path, "--out", out], stdout=subprocess.DEVNULL)
        path = out
    return ImageOps.exif_transpose(Image.open(path)).convert("L")


def find_marks(im):
    """Small dark squares; the ones furthest out in each corner direction are the marks."""
    scale = 800 / max(im.size)
    small = np.asarray(im.resize((int(im.width * scale), int(im.height * scale)), Image.BILINEAR), dtype=np.float32)
    dark = small < 0.55 * np.percentile(small, 80)
    seen = np.zeros(dark.shape, bool)
    blobs = []
    h, w = dark.shape
    for y0, x0 in zip(*np.nonzero(dark)):
        if seen[y0, x0]:
            continue
        stack, pts = [(y0, x0)], []
        seen[y0, x0] = True
        while stack:
            y, x = stack.pop()
            pts.append((y, x))
            for ny, nx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
                if 0 <= ny < h and 0 <= nx < w and dark[ny, nx] and not seen[ny, nx]:
                    seen[ny, nx] = True
                    stack.append((ny, nx))
        p = np.array(pts)
        dy, dx = np.ptp(p[:, 0]) + 1, np.ptp(p[:, 1]) + 1
        if 4 <= dx <= 40 and 4 <= dy <= 40 and 0.6 < dx / dy < 1.7 and len(pts) > 0.6 * dx * dy:
            blobs.append((p[:, 1].mean() / scale, p[:, 0].mean() / scale))
    if len(blobs) < 4:
        sys.exit("cannot find the corner marks: pass --corners")
    b = np.array(blobs)
    pick = lambda v: tuple(b[np.argmax(v)])
    return [pick(-b[:, 0] - b[:, 1]), pick(b[:, 0] - b[:, 1]), pick(-b[:, 0] + b[:, 1]), pick(b[:, 0] + b[:, 1])]


def straighten(im, corners):
    """PIL's perspective map goes from the output to the photo."""
    a, r = [], []
    for (x, y), (u, v) in zip(MARKS, corners):
        a.append([x, y, 1, 0, 0, 0, -u * x, -u * y])
        a.append([0, 0, 0, x, y, 1, -v * x, -v * y])
        r += [u, v]
    return im.transform((W, H), Image.PERSPECTIVE, np.linalg.solve(np.array(a, float), np.array(r, float)), Image.BICUBIC)


def analyse(flat):
    px = np.asarray(flat, dtype=np.float32)
    mean = lambda x0, y0, x1, y1: float(px[y0:y1, x0:x1].mean())
    strip = []
    for level in range(16):
        x = 22 + level * 40
        white = mean(x + 8, STRIP_Y - 16, x + 32, STRIP_Y - 6)
        strip.append(mean(x + 8, STRIP_Y + 10, x + 32, STRIP_Y + STRIP_H - 10) / white)
    strip = np.array(strip)
    rows = {"toward_black": {}, "toward_white": {}}
    for r in range(ROWS):
        for c in range(COLS):
            x, y = X0 + c * PX, Y0 + r * PY
            n = (r % (ROWS // 2)) * COLS + c + 1
            white = mean(x - 16, y + 20, x - 6, y + 64)
            value = mean(x + 21, y + 21, x + 63, y + 63) / white
            level = float(np.interp(value, strip, np.arange(16))) if np.all(np.diff(strip) > 0) else float(np.argmin(abs(strip - value)))
            rows["toward_black" if r < ROWS // 2 else "toward_white"][n] = level
    return strip, rows


def swing(levels, tolerance=1.0):
    """The push count from which more pushes change less than one gray level."""
    final = levels[max(levels)]
    for n in sorted(levels):
        if all(abs(levels[m] - final) <= tolerance for m in levels if m >= n):
            return n
    return max(levels)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("photo")
    ap.add_argument("--corners", help="x,y four times: top left, top right, bottom left, bottom right")
    ap.add_argument("--debug", help="write the straightened screen here")
    a = ap.parse_args()
    im = load(a.photo)
    if a.corners:
        v = [float(x) for x in a.corners.split(",")]
        corners = [(v[i], v[i + 1]) for i in range(0, 8, 2)]
    else:
        corners = find_marks(im)
    flat = straighten(im, corners)
    if a.debug:
        flat.save(a.debug)
    strip, rows = analyse(flat)
    print("factory levels, against white:", " ".join(f"{v:.2f}" for v in strip))
    for name, levels in rows.items():
        print(f"{name}:", " ".join(f"{n}:{levels[n]:.1f}" for n in sorted(levels)))
        print(f"  full swing about {swing(levels)} pushes, first push moves {abs(levels[1] - (15 if name == 'toward_black' else 0)):.1f} levels")


if __name__ == "__main__":
    main()
