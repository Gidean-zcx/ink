"""lock-styles-1: our wallpaper drawing against Mogo's, on the same decoded pixels, as the panel would show them.

Ours (system/ui/shell/wallpaper.c): ink_image_dither at the picture's own size (4x4 Bayer between the panel's measured
levels), then LVGL scales the dithered picture to cover the screen (nearest pixel). Mogo (ui_wallpaper.c +
ui_image_dither.c): scale the gray picture to cover first, then a 4x4 Bayer between 16 equal steps at screen pixels.
Both start from the same gray picture (white under transparency, as ink_image_decode gives). Output: what the panel
shows (each level by its measured brightness), side by side, ours left."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw

LEVEL = [.20, .26, .34, .41, .47, .57, .65, .72, .80, .86, .90, .94, .96, .98, .99, 1.00]  # drivers/display/pqbn/panel_e0470a14.h
BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5]
W, H = 684, 1216


def gray(path):
    im = Image.open(path)
    if im.mode in ('RGBA', 'LA', 'P'):
        im = im.convert('RGBA')
        bg = Image.new('RGBA', im.size, (255, 255, 255, 255))
        im = Image.alpha_composite(bg, im)
    return im.convert('L')


def ours(g):  # system/services/media/image.c ink_image_dither, then cover with nearest sampling
    px = g.load(); out = Image.new('L', g.size); o = out.load()
    for y in range(g.height):
        for x in range(g.width):
            t = LEVEL[0] + (LEVEL[15] - LEVEL[0]) * px[x, y] / 255
            l = 0
            while l < 14 and t > LEVEL[l + 1]:
                l += 1
            f = (t - LEVEL[l]) / (LEVEL[l + 1] - LEVEL[l])
            if f > (BAYER[(y % 4) * 4 + x % 4] + .5) / 16:
                l += 1
            o[x, y] = l
    return cover(out)


def cover(im):
    s = max(W / im.width, H / im.height)
    sw, sh = round(im.width * s), round(im.height * s)
    big = im.resize((sw, sh), Image.NEAREST)
    return big.crop(((sw - W) // 2, (sh - H) // 2, (sw - W) // 2 + W, (sh - H) // 2 + H))


def mogo(g):  # ui_wallpaper.c image_pixels (cover, centre crop, nearest) + ui_image_dither_gray
    c = cover(g); px = c.load(); out = Image.new('L', (W, H)); o = out.load()
    for y in range(H):
        for x in range(W):
            v = px[x, y]; base = v // 17
            if base >= 15:
                o[x, y] = 15; continue
            rem = v - base * 17
            o[x, y] = base + (1 if rem * 16 > BAYER[(y & 3) * 4 + (x & 3)] * 17 else 0)
    return out


def panel(levels):
    return levels.point(lambda l: round(LEVEL[min(l, 15)] * 255))


def synthetic_alpha(path):
    im = Image.new('RGBA', (600, 900), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    for r in range(260, 0, -4):
        d.ellipse((300 - r, 450 - r, 300 + r, 450 + r), fill=(int(255 * r / 260), 80, 40, int(255 * (1 - r / 300))))
    d.rectangle((60, 60, 540, 140), fill=(20, 20, 20, 255)); d.line((60, 800, 540, 700), fill=(0, 0, 0, 255), width=6)
    im.save(path)


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--out', type=Path, default=Path(__file__).parent)
p.add_argument('images', nargs='*', type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[3]
images = a.images or [root / 'docs/tasks/replan-reader/reference-images/crossink-reading.jpg',
                      root / 'docs/tasks/frameworks-research-1/images/fcitx-symbols.jpg', a.out / 'wallpaper-alpha-source.png']
if not images[2].exists():
    synthetic_alpha(images[2])
for name, path in zip(('photo', 'lineart', 'alpha'), images):
    g = gray(path)
    pair = Image.new('L', (2 * W + 12, H), 255)
    pair.paste(panel(ours(g)), (0, 0)); pair.paste(panel(mogo(g)), (W + 12, 0))
    pair = pair.resize((pair.width // 2, pair.height // 2), Image.LANCZOS)
    pair.save(a.out / f'wallpaper-{name}.png', optimize=True)
    print(name, path.name, 'saved')
