import sys, json, time, numpy as np
from sim import *
from scenes import gb_to_panel, gb_target, PATTERN_PB, PATTERN_ML

def natural_methods(mus=(0.05,)):
    return [
        Method("PB-FS  counter+FS", "fs", "counter"),
        Method("PB-BN  counter+bluenoise", "bn", "counter"),
        Method("INT-FS integrator+FS", "fs", "int"),
    ] + [Method(f"PQ     physical quantizer (int16) mu={m}", "pqf", "pq", mu=m, marg=3) for m in mus]

def run_natural(scene, cfg, methods, record=()):
    F = np.load(scene)
    out = []
    for m in methods:
        t = time.time()
        r = run(m, F, lambda t: F[t], cfg, record=m.name in record)
        r["sec"] = round(time.time() - t, 1)
        out.append(r)
        print(fmt(r), flush=True)
    return out

def run_gb(cfg, record=False):
    G = np.load("G.npy")
    tgt = gb_target(G)
    # drive grid: 96 rows x 480 cols, each drive row repeated on 3 panel rows (as in PaperBoy)
    Iin = np.repeat(1.0 - G.astype(np.float32) / 3.0, 3, axis=2)
    out = []
    cases = (("PB     counter+3px binary pattern", "pattern", "counter", gb_to_panel(G, PATTERN_PB, True)[:, ::3], 2),
             ("INT-PB integrator+3px binary pattern", "pattern", "int", gb_to_panel(G, PATTERN_PB, True)[:, ::3], 2),
             ("ML5    integrator+fixed 5-level pattern", "pattern", "int", gb_to_panel(G, PATTERN_ML, False)[:, ::3], 5),
             ("PQ     physical quantizer (int16) mu=0.05", "pqf", "pq", Iin, 2))
    for name, q, drive, F, nlev in cases:
        m = Method(name, q, drive, nlev=nlev, mu=0.05, marg=3)
        top = (nlev - 1) if q == "pattern" else 1.0
        F = np.ascontiguousarray(F.astype(np.float32) if q != "pattern" else F)
        fl = (lambda v, F=F, top=top: np.full(F[0].shape, top if v else 0, F.dtype))
        t = time.time()
        r = run(m, F, lambda t: tgt[t], cfg, rr=3, record=record, flush_input=fl)
        r["sec"] = round(time.time() - t, 1)
        out.append(r); print(fmt(r), flush=True)
    return out

def fmt(r):
    s = (f"{r['name']:<44} PSNR {r['psnr']:6.2f}  final {r['psnr_final']:6.2f}  shimmer {r['shimmer']:6.2f}  "
         f"pulses {r['pulses']:5.2f}%  DCrms {r['dc_rms']:7.2f} DCmax {r['dc_max']:5d}")
    if "ghost_white" in r:
        s += f"  ghostW {r['ghost_white']:5.2f} ghostB {r['ghost_black']:5.2f}"
    return s + f"  ({r.get('sec','')}s)"

if __name__ == "__main__":
    which = sys.argv[1]
    cfg = DCSENS if (len(sys.argv) > 2 and sys.argv[2] == "dc") else CLEAN
    if which == "gb":
        res = run_gb(cfg)
    else:
        res = run_natural(which + ".npy", cfg, natural_methods())
    tag = which + ("_dc" if cfg is DCSENS else "_clean")
    json.dump([{k: v for k, v in r.items() if k != "rec"} for r in res], open(f"res_{tag}.json", "w"), indent=1, default=float)
