"""Interaction latency on the Metalio E-Ink4-Plus timing model.

Panel time step = one EPDiy phase (12 ms). Content (LVGL) produces a new frame every 3 phases (~28 fps).
Compared drivers:
  GL-DU20  current architecture, default profile: worker takes the latest frame only when idle,
           plays the whole DU waveform (18 pushes + 2 idle = 20 phases) on changed pixels.
  GL-DU8   current architecture, fast profiles: short DU (7 black / 8 white pushes, 8 phases).
  ASYNC    per-pixel integrator (N = 8): target refreshed every phase, no global lock.
Panel model from eink_sim (finite speed, saturation, per-pixel mobility, first-push stiction),
full swing ~7 phases. TPS power-up time between updates is NOT charged to the baseline.
"""
import sys, numpy as np
sys.path.insert(0, "/home/claude/eink_sim")
from sim import Panel, CLEAN
from scenes import _text_page
from scipy.ndimage import gaussian_filter

PH_MS = 12.0
CONTENT_EVERY = 3          # phases per LVGL frame
H, W = 608, 342            # half-resolution portrait crop (latency is resolution independent)
CFG = dict(CLEAN, T0=7.0)  # full swing ~7 phases at 12 ms

def make_session(seed=3):
    page = _text_page(W, 9000, seed)
    page[:, :] = np.where(page < 0.5, 0.0, 1.0)          # LVGL I1: binary text
    T_content = 260                                       # content frames (~28 s? no: 260*36ms = 9.4 s)
    offsets, taps = [], []
    off, v = 0.0, 0.0
    flicks = {20: 30.0, 95: -22.0, 170: 40.0}            # flick start frame -> px/frame
    for f in range(T_content):
        if f in flicks: v = flicks[f]
        off = max(0.0, off + v); v *= 0.93
        if abs(v) < 0.5: v = 0.0
        offsets.append(int(off))
    # taps: list-item press highlight (invert a row band) for ~10 content frames
    tap_frames = [8, 60, 130, 150, 210, 240]
    frames = []
    for f in range(T_content):
        img = page[offsets[f]:offsets[f] + H].copy()
        img[:40, :] = 0.0; img[12:28, 20:200] = 1.0       # static title bar
        for tf in tap_frames:
            if tf <= f < tf + 10:
                y0 = 300; img[y0:y0 + 56, 10:W - 10] = 1.0 - img[y0:y0 + 56, 10:W - 10]
        frames.append(img.astype(np.float32))
    return frames, tap_frames, offsets

def du_plan(npush_white, npush_black, length):
    return dict(w=npush_white, b=npush_black, len=length)

def run(driver, frames, tap_frames, du=None, seed=1):
    T = len(frames) * CONTENT_EVERY
    panel = Panel(H, W, CFG, seed=seed, p0=1.0)
    panel.p[...] = frames[0]                               # start settled on frame 0
    back = frames[0] > 0.5                                 # what the driver believes is on panel
    pos = np.where(back, 8, 0).astype(np.int16)
    remaining = np.zeros((H, W), np.int16); direction = np.zeros((H, W), np.int16)
    busy = 0
    tgt_in_update = back.copy()
    log_p = []
    sq = 0.0; pulses = 0
    for ph in range(T):
        cur = frames[min(ph // CONTENT_EVERY, len(frames) - 1)]
        tgt = cur > 0.5
        if driver == "async":
            t = np.where(tgt, 8, 0)
            u = np.sign(t - pos).astype(np.int8); pos += u
        else:
            if busy == 0 and np.any(tgt != back):
                changed = tgt != back
                direction = np.where(changed, np.where(tgt, 1, -1), 0).astype(np.int16)
                remaining = np.where(changed, np.where(tgt, du["w"], du["b"]), 0).astype(np.int16)
                busy = du["len"]; tgt_in_update = tgt.copy()
            u = np.where(remaining > 0, direction, 0).astype(np.int8)
            remaining = np.maximum(remaining - 1, 0)
            if busy:
                busy -= 1
                if busy == 0: back = tgt_in_update
        panel.step(u)
        pulses += np.count_nonzero(u)
        log_p.append((panel.p * 255 + 0.5).astype(np.uint8))
        GI = gaussian_filter(cur, 1.2); GR = gaussian_filter(panel.p, 1.2)
        sq += np.mean((GR - GI) ** 2)
    # residual DC imbalance: drive everything back to the initial image with the same driver; a balanced
    # scheme ends with sum(u) == 0 for every pixel (all started white)
    white = frames[0] > 0.5                                # return to the initial image
    for k in range(60):
        if driver == "async":
            u = np.sign(np.where(white, 8, 0) - pos).astype(np.int8); pos += u
        else:
            if busy == 0 and np.any(white != back):
                changed = white != back
                direction = np.where(changed, np.where(white, 1, -1), 0).astype(np.int16)
                remaining = np.where(changed, np.where(white, du["w"], du["b"]), 0).astype(np.int16)
                busy = du["len"]; tgt_in_update = white.copy()
            u = np.where(remaining > 0, direction, 0).astype(np.int8)
            remaining = np.maximum(remaining - 1, 0)
            if busy:
                busy -= 1
                if busy == 0: back = tgt_in_update
        panel.step(u)
    resid = panel.dc.astype(float)
    # tap latency: band y 300..356 interior, time to 50% / 90% of the press change
    lat50, lat90 = [], []
    for tf in tap_frames:
        ph0 = tf * CONTENT_EVERY
        before = np.mean(frames[tf - 1][300:356, 14:W - 14]); after = np.mean(frames[tf][300:356, 14:W - 14])
        d = after - before
        t50 = t90 = None
        for k in range(ph0, min(ph0 + 120, T)):
            m = np.mean(log_p[k][300:356, 14:W - 14]) / 255.0
            prog = (m - before) / d if abs(d) > 1e-6 else 1
            if t50 is None and prog >= 0.5: t50 = (k - ph0 + 1) * PH_MS
            if t90 is None and prog >= 0.9: t90 = (k - ph0 + 1) * PH_MS; break
        lat50.append(t50); lat90.append(t90)
    return dict(psnr=10 * np.log10(1 / (sq / T)), lat50=lat50, lat90=lat90,
                dc_max=int(np.abs(resid).max()), dc_rms=float(np.sqrt(np.mean(resid ** 2))),
                dc_frac=float(np.mean(resid != 0)),
                pulses_per_px_s=pulses / (H * W) / (T * PH_MS / 1000), log=log_p)

if __name__ == "__main__":
    frames, taps, offs = make_session()
    res = {}
    for name, drv, du in (("GL-DU20 (default profile)", "gl", du_plan(18, 18, 20)),
                          ("GL-DU8  (fast profiles)", "gl", du_plan(8, 7, 8)),
                          ("ASYNC integrator N=8", "async", None)):
        r = run(drv, frames, taps, du); res[name] = r
        fmt = lambda xs: " ".join(f"{x:.0f}" if x is not None else "-" for x in xs)
        print(f"{name:<27} scroll/tap PSNR {r['psnr']:5.2f} dB | tap->50%: {fmt(r['lat50'])} ms (mean {np.mean([x for x in r['lat50'] if x]):.0f}) "
              f"| tap->90%: mean {np.mean([x for x in r['lat90'] if x]):.0f} ms | residual DC after returning to frame 0: max {r['dc_max']} rms {r['dc_rms']:.2f} ({100*r['dc_frac']:.1f}% px) | pushes/px/s {r['pulses_per_px_s']:.2f}")
    np.save("/home/claude/metalio_analysis/offsets.npy", np.array(offs))
    import pickle; pickle.dump({k: v["log"][::1] for k, v in res.items()}, open("/home/claude/metalio_analysis/logs.pkl", "wb"))
    pickle.dump(frames, open("/home/claude/metalio_analysis/frames.pkl", "wb"))
