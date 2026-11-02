"""Test content. All scenes are 60 fields/s, 10 s, last second static."""
import numpy as np
from scipy.ndimage import affine_transform
from skimage import data, color
from PIL import Image, ImageDraw, ImageFont

FPS = 60


def _gray(img):
    g = color.rgb2gray(img) if img.ndim == 3 else img / 255.0
    return g.astype(np.float32)


def _view(src, cx, cy, zoom, h, w):
    """Sample an h x w window of src centred at (cx,cy) with given zoom (bilinear)."""
    s = 1.0 / zoom
    mat = np.array([[s, 0], [0, s]])
    off = np.array([cy - s * h / 2, cx - s * w / 2])
    return affine_transform(src, mat, offset=off, output_shape=(h, w), order=1, mode="reflect")


def video_scene(h=240, w=320, seconds=10):
    """Natural video: Ken Burns pans/zooms, a crossfade, a fast pan, a moving object, static end."""
    A = _gray(data.astronaut()); C = _gray(data.coffee()); R = _gray(data.rocket()); Cam = _gray(data.camera())
    T = seconds * FPS
    frames = np.zeros((T, h, w), np.float32)
    yy, xx = np.mgrid[0:h, 0:w]
    for t in range(T):
        s = t / FPS
        if s < 3:      # slow pan + zoom in
            a = s / 3
            f = _view(A, 200 + 120 * a, 220 - 40 * a, 1.0 + 0.6 * a, h, w)
        elif s < 4:    # crossfade
            a = s - 3
            f = (1 - a) * _view(A, 320, 180, 1.6, h, w) + a * _view(C, 300, 200, 1.0, h, w)
        elif s < 7:    # moving disc over slowly zooming image
            a = (s - 4) / 3
            f = _view(C, 300, 200, 1.0 + 0.3 * a, h, w)
            cx = 40 + (w - 80) * (0.5 - 0.5 * np.cos(2 * np.pi * a * 1.5)); cy = h / 2 + 60 * np.sin(2 * np.pi * a * 2)
            f = np.where((xx - cx) ** 2 + (yy - cy) ** 2 < 22 ** 2, 0.95, f)
        elif s < 9:    # fast pan (~6 px/field)
            a = (s - 7) / 2
            f = _view(R, 170 + 400 * a, 230, 1.0, h, w)
        else:          # static hold
            f = _view(Cam, 256, 230, 1.0, h, w)
        frames[t] = np.clip(f, 0, 1)
    return frames


def _text_page(w, height, seed):
    rng = np.random.default_rng(seed)
    font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 13)
    img = Image.new("L", (w, height), 255)
    d = ImageDraw.Draw(img)
    words = ("the quick brown fox jumps over lazy dog electrophoretic display waveform pixel "
             "ink particle voltage refresh ghosting latency dither field frame panel").split()
    y = 6
    while y < height - 18:
        if rng.random() < 0.08:
            y += 10; continue
        line, x = [], 8
        while True:
            wd = words[rng.integers(len(words))]
            if x + d.textlength(wd + " ", font=font) > w - 8: break
            line.append(wd); x += d.textlength(wd + " ", font=font)
        d.text((8, y), " ".join(line), fill=0, font=font)
        y += 17
    return np.asarray(img, np.float32) / 255.0


def text_scene(h=240, w=320, seconds=10):
    """Reading/UI: static page, smooth scroll 2 px/field, page jumps, static end."""
    page = _text_page(w, 2400, 1)
    pages = [_text_page(w, h, s) for s in (2, 3, 4, 5)]
    T = seconds * FPS
    frames = np.zeros((T, h, w), np.float32)
    for t in range(T):
        s = t / FPS
        if s < 1:
            f = page[:h]
        elif s < 6:
            y0 = int((t - FPS) * 2); f = page[y0:y0 + h]
        elif s < 9:
            f = pages[int((s - 6) / 0.75) % 4]
        else:
            f = pages[3]
        frames[t] = f
    return frames


# ---------------------------------------------------------------------------
# Game-Boy-like scene: 160x96 four-shade content, background scroll, sprites
# ---------------------------------------------------------------------------
def gb_scene(seconds=10):
    rng = np.random.default_rng(7)
    W, H = 160, 96
    mapw = 512
    bg = np.zeros((H, mapw), np.uint8)                  # shade 0 = lightest
    for i in range(10):                                 # clouds
        cx, cy = rng.integers(0, mapw), rng.integers(8, 34)
        yy, xx = np.mgrid[0:H, 0:mapw]
        d = ((xx - cx) / 18.0) ** 2 + ((yy - cy) / 6.0) ** 2
        bg[d < 1] = 1
    hill = (60 + 10 * np.sin(np.arange(mapw) / 23.0) + 6 * np.sin(np.arange(mapw) / 7.0)).astype(int)
    for x in range(mapw):
        bg[hill[x]:, x] = 2
        bg[hill[x]:hill[x] + 2, x] = 3
    ground = 80
    bg[ground:, :] = 2
    for y in range(ground, H):                          # bricks
        for x in range(mapw):
            if (y - ground) % 8 == 0 or ((x + (4 if ((y - ground) // 8) % 2 else 0)) % 8 == 0):
                bg[y, x] = 3
    for x in range(0, mapw, 40):                        # pipes / blocks
        bg[ground - 12:ground, x + 10:x + 22] = 3
        bg[ground - 11:ground, x + 12:x + 20] = 1
    hero = np.array([[0,0,0,3,3,3,3,0,0,0],[0,0,3,3,3,3,3,3,3,0],[0,0,2,2,1,1,2,1,0,0],
                     [0,2,1,2,1,1,1,2,1,1],[0,2,2,1,1,1,2,2,2,0],[0,0,0,1,1,1,1,1,0,0],
                     [0,0,3,3,2,3,3,0,0,0],[0,3,3,3,2,3,3,3,0,0],[3,3,3,2,2,2,3,3,3,0],
                     [1,1,3,2,1,2,3,1,1,0],[1,1,2,2,2,2,2,1,1,0],[0,0,2,2,0,2,2,0,0,0],
                     [0,3,3,0,0,0,3,3,0,0],[3,3,3,0,0,0,3,3,3,0]], np.uint8)
    T = seconds * 60
    frames = np.zeros((T, H, W), np.uint8)
    for t in range(T):
        st = min(t, T - 60)                              # last second static
        x0 = st % (mapw - W)
        f = bg[:, x0:x0 + W].copy()
        # hero jumping
        hx, hy = 40, int(ground - 14 - abs(30 * np.sin(st / 18.0)))
        m = hero > 0; f[hy:hy + 14, hx:hx + 10][m] = hero[m]
        # enemies walking (opposite direction)
        for k in range(3):
            ex = int((W + 20 - (st * 1.5 + 70 * k)) % (W + 20)) - 10
            if 0 <= ex < W - 8:
                f[ground - 8:ground, ex:ex + 8] = 3
                f[ground - 6:ground - 3, ex + 2:ex + 6] = 1
        # blinking coins
        for k in range(4):
            cx = int((30 + 35 * k - x0) % (mapw)) % 200
            if 0 <= cx < W - 6 and (st // 8) % 2 == 0:
                f[44:50, cx:cx + 6] = 2
                f[45:49, cx + 2:cx + 4] = 0
        frames[t] = f
    return frames


# Panel encodings of 4 GB shades over 3 horizontal pixels (rows repeated x3).
# Values are integrator positions 0..4 (0 black .. 4 white); intensity = 1 - shade/3.
PATTERN_PB = {0: [4, 4, 4], 1: [0, 4, 4], 2: [0, 0, 4], 3: [0, 0, 0]}      # PaperBoy: binary 3-px patterns
PATTERN_ML = {0: [4, 4, 4], 1: [3, 3, 2], 2: [2, 1, 1], 3: [0, 0, 0]}      # proposed: partial-drive levels


def gb_to_panel(frames, pattern, binary):
    T, H, W = frames.shape
    lut = np.array([pattern[s] for s in range(4)], np.uint8)       # 4 x 3
    if binary:
        lut = (lut > 0).astype(np.uint8)
    row = lut[frames]                                              # T,H,W,3
    row = row.reshape(T, H, W * 3)
    return np.repeat(row, 3, axis=1)                               # T, 3H, 3W


def gb_target(frames):
    I = (1.0 - frames.astype(np.float32) / 3.0)
    return np.repeat(np.repeat(I, 3, axis=1), 3, axis=2)
