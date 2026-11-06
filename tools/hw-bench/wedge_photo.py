#!/usr/bin/env python3
"""Reads a photo of the wedge (panel_lab.c): how many pushes does a full swing take on this panel?

wedge_photo.py photo.jpg [--corners x,y,x,y,x,y,x,y] [--debug out.png]
Finds the four corner marks, straightens the screen, reads every patch against the factory's 16 gray
levels in the strip, with the white paper next to each as its local light. Needs numpy and Pillow, which
the system python3 has here and build-deps does not.
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
# the wedge since 2026-11-06 16:42: pairs of rows; the first wedge had 24 patches each way (--first-wedge)
SETS = ("toward_black", "toward_white_after_swing", "toward_white", "toward_black_after_swing")
MARKS = ((17, 17), (667, 17), (17, 1199), (667, 1199))  # centres: top left, top right, bottom left, bottom right


def load(path):
    if path.lower().endswith((".heic", ".heif")):  # what an iPhone sends
        out = os.path.join(tempfile.mkdtemp(), "photo.jpg")
        subprocess.check_call(["sips", "-s", "format", "jpeg", path, "--out", out], stdout=subprocess.DEVNULL)
        path = out
    return ImageOps.exif_transpose(Image.open(path)).convert("L")


def find_blobs(im):
    """Small dark squares in the photo: (x, y, size)."""
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
            blobs.append((p[:, 1].mean() / scale, p[:, 0].mean() / scale, (dx + dy) / 2 / scale))
    return blobs


def transform_for(corners):
    """Screen to photo, the form PIL's perspective map wants."""
    a, r = [], []
    for (x, y), (u, v) in zip(MARKS, corners):
        a.append([x, y, 1, 0, 0, 0, -u * x, -u * y])
        a.append([0, 0, 0, x, y, 1, -v * x, -v * y])
        r += [u, v]
    return np.linalg.solve(np.array(a, float), np.array(r, float))


def strip_in_photo(px, t):
    """The 16 levels as the photo has them, without straightening it first."""
    out = []
    for level in range(16):
        x, y = 42 + level * 40, STRIP_Y + STRIP_H / 2
        w = t[6] * x + t[7] * y + 1
        u, v = int((t[0] * x + t[1] * y + t[2]) / w), int((t[3] * x + t[4] * y + t[5]) / w)
        if not (4 <= u < px.shape[1] - 4 and 4 <= v < px.shape[0] - 4):
            return None
        out.append(float(px[v - 4:v + 4, u - 4:u + 4].mean()))
    return np.array(out)


def looks_like_strip(strip):
    """Dark to light from one end to the other, which the phone's tone curve and the top levels' sameness leave alone."""
    return strip is not None and strip[-1] > 1.5 * strip[:3].mean() and np.corrcoef(strip, np.arange(16))[0, 1] > 0.85


def find_marks(im):
    """Anything dark and square may be a mark, keys and icons around the device too. The four that make a screen of
    the right shape, and put the gray strip where it belongs, are the marks."""
    blobs = find_blobs(im)
    px = np.asarray(im, dtype=np.float32)
    cx, cy = im.width / 2, im.height / 2
    parts = [[b for b in blobs if (b[0] < cx) == left and (b[1] < cy) == top] for top in (True, False) for left in (True, False)]
    best, best_area = None, 0
    for tl in parts[0]:
        for tr in parts[1]:
            if tr[0] - tl[0] < 0.25 * im.width or abs(tr[1] - tl[1]) > 0.2 * (tr[0] - tl[0]):
                continue
            for bl in parts[2]:
                for br in parts[3]:
                    if br[0] - bl[0] < 0.25 * im.width or abs(br[1] - bl[1]) > 0.2 * (br[0] - bl[0]):
                        continue
                    sizes = (tl[2], tr[2], bl[2], br[2])
                    if max(sizes) > 2.5 * min(sizes):
                        continue
                    wide, tall = (tr[0] - tl[0] + br[0] - bl[0]) / 2, (bl[1] - tl[1] + br[1] - tr[1]) / 2
                    if not 1.4 < tall / wide < 2.3 or abs(bl[0] - tl[0]) > 0.25 * tall or abs(br[0] - tr[0]) > 0.25 * tall:
                        continue
                    corners = [(b[0], b[1]) for b in (tl, tr, bl, br)]
                    if wide * tall > best_area and looks_like_strip(strip_in_photo(px, transform_for(corners))):
                        best, best_area = corners, wide * tall
    if best is None:
        sys.exit("cannot find the corner marks: pass --corners")
    return best


def straighten(im, corners):
    """PIL's perspective map goes from the output to the photo."""
    return im.transform((W, H), Image.PERSPECTIVE, transform_for(corners), Image.BICUBIC)


def analyse(flat, first_wedge=False):
    px = np.asarray(flat, dtype=np.float32)
    mean = lambda x0, y0, x1, y1: float(px[y0:y1, x0:x1].mean())
    strip = []
    for level in range(16):
        x = 22 + level * 40
        white = mean(x + 8, STRIP_Y - 16, x + 32, STRIP_Y - 6)
        strip.append(mean(x + 8, STRIP_Y + 10, x + 32, STRIP_Y + STRIP_H - 10) / white)
    strip = np.array(strip)
    rising = np.maximum.accumulate(strip) + np.arange(16) * 1e-6  # the darkest levels can come out in the wrong order
    rows, bright = {}, {}
    for r in range(ROWS):
        for c in range(COLS):
            x, y = X0 + c * PX, Y0 + r * PY
            n = (r % (ROWS // 2)) * COLS + c + 1
            left, right = mean(x - 16, y + 20, x - 6, y + 64), mean(x + PATCH + 6, y + 20, x + PATCH + 16, y + 64)
            white = right if c == 0 else left if c == COLS - 1 else (left + right) / 2  # the outer margins are in the bezel's shadow
            value = mean(x + 21, y + 21, x + 63, y + 63) / white
            level = float(np.interp(value, rising, np.arange(16)))
            if first_wedge:
                name, n = ("toward_black" if r < ROWS // 2 else "toward_white"), (r % (ROWS // 2)) * COLS + c + 1
            else:
                name, n = SETS[r // 2], (r % 2) * COLS + c + 1
            rows.setdefault(name, {})[n] = level
            bright.setdefault(name, {})[n] = value
    return strip, rows, bright


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
    ap.add_argument("--first-wedge", action="store_true", help="the photo is of the first wedge's layout")
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
    strip, rows, bright = analyse(flat, a.first_wedge)
    if not (strip[-1] > 1.5 * strip[:3].mean() and np.corrcoef(strip, np.arange(16))[0, 1] > 0.85):
        sys.exit("this does not read as the gray strip: the corners are wrong, pass --corners and look at --debug")
    print("factory levels, against white:", " ".join(f"{v:.2f}" for v in strip))
    for name, levels in rows.items():
        start = 15 if name.startswith("toward_black") else 0
        print(f"{name} (factory levels):", " ".join(f"{n}:{levels[n]:.1f}" for n in sorted(levels)))
        print(f"{name} (against white):", " ".join(f"{n}:{bright[name][n]:.2f}" for n in sorted(levels)))
        if abs(levels[max(levels)] - start) < 3:
            print("  these patches have not moved: the photo is from before the pushes, or the pushes did nothing")
        else:
            print(f"  full swing about {swing(levels)} pushes, first push moves {abs(levels[1] - start):.1f} levels")


if __name__ == "__main__":
    main()
