"""Fling scroll (fast start, exponential decay, then rest) at Metalio timing; compares PQ-BN v4 with an
unlimited planner against planners limited to what one or two S31 cores can afford, and the integrator."""
import sys, json, numpy as np
import sim, scenes
from sim import Method, run, CLEAN
from run_exp import fmt
from scipy.ndimage import gaussian_filter
sim.N = 8
CFG = dict(CLEAN, T0=7.6)

def fling_scene(h=240, w=320, T=600, v0=18.0, decay=0.93, start=30, frame_phases=3):
    page = scenes._text_page(w, 4000, 9)
    frames = np.zeros((T, h, w), np.float32)
    off, v, y = 0.0, v0, 0
    for t in range(T):
        if t >= start and (t - start) % frame_phases == 0 and v > 0.5:
            off += v; v *= decay
        frames[t] = page[int(off):int(off) + h]
    return frames

if __name__ == "__main__":
    F = fling_scene()
    np.save("F_fling.npy", F)
    moving = [t for t in range(1, len(F)) if not np.array_equal(F[t], F[t - 1])]
    print(f"fling: {len(F)} phases, content changes during phases {moving[0]}..{moving[-1]}")
    scale = F[0].size / (1216 * 684)
    # the sim calls step() with field_no starting at 1; content phase t is field t+1
    a0 = moving[0] + 1
    vs = []; v = 18.0
    while v > 0.5: vs.append(v); v *= 0.93
    def exit_at(vmin):   # first content frame whose step is below vmin px/frame
        i = next(i for i, x in enumerate(vs) if x < vmin); return a0 + 3 * i
    cases = [("PQ-BN v4, unlimited planner", dict(quant="pqbnv4", budget=0)),
             ("hybrid: motion mode until v<4 px/frame, then PQ-BN (2-core budget)", dict(quant="pqbnv4", budget=int(19500 * scale), motion=[(a0, exit_at(4.0))])),
             ("hybrid: motion mode until v<2 px/frame, then PQ-BN (2-core budget)", dict(quant="pqbnv4", budget=int(19500 * scale), motion=[(a0, exit_at(2.0))])),
             ("hybrid: motion mode until stop, then PQ-BN (2-core budget)", dict(quant="pqbnv4", budget=int(19500 * scale), motion=[(a0, moving[-1] + 2)])),
             ("PQ-BN v4, 2-core budget (19.5K px/phase)", dict(quant="pqbnv4", budget=int(19500 * scale))),
             ("PQ-BN v4, 1-core budget (7.6K px/phase)", dict(quant="pqbnv4", budget=int(7600 * scale))),
             ("INT-BN  integrator + blue-noise binary", dict(quant="bn", drive="int"))]
    out = {}
    for name, kw in cases:
        q = kw.pop("quant"); drv = kw.pop("drive", "pq")
        m = Method(name, q, drv, marg=2, T0c=7.6, **kw)
        r = run(m, F, lambda t: F[t], CFG, record=True)
        rec = r.pop("rec")
        # PSNR split: during motion vs. after it (the last content change)
        ps = []
        for t in range(len(F)):
            GI = gaussian_filter(F[t], 1.2); GR = gaussian_filter(rec[t].astype(np.float32) / 255.0, 1.2)
            ps.append(np.mean((GR - GI) ** 2))
        ps = np.array(ps); a, b = moving[0], moving[-1]
        r["psnr_motion"] = float(10 * np.log10(1 / ps[a:b + 1].mean()))
        r["psnr_after"] = float(10 * np.log10(1 / ps[b + 1:].mean()))
        r["settle_phases"] = int(next((t - b for t in range(b + 1, len(F)) if 10 * np.log10(1 / ps[t]) > r["psnr_final"] - 0.5), -1))
        extra = f"  motion {r['psnr_motion']:.2f}  after {r['psnr_after']:.2f}  settle {r['settle_phases']} ph"
        if q == "pqbnv4":
            extra += f"  planned px/phase {m.v4ctx.stat_planned / len(F):.0f} (budget {m.budget}) motion phases {m.motion_phases}"
        print(fmt(r), extra, flush=True)
        out[name] = r
    json.dump(out, open("res_fling.json", "w"), indent=1, default=float)
