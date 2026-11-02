"""Quality on Metalio-like timing: full swing ~7.6 phases (12 ms each), integrator N=8."""
import sys, json, numpy as np
import sim
from sim import Method, run, CLEAN
from run_exp import fmt
sim.N = 8
CFG = dict(CLEAN, T0=7.6)
scenes = {"video": np.load("V.npy"), "text": np.load("X.npy")}
methods = lambda: [
    Method("INT-BN  integrator + blue-noise binary", "bn", "int"),
    Method("INT-FS  integrator + FS binary", "fs", "int"),
    Method("PQ-ED   serial ED over reachable states", "pq", "pq", mu=0.05, marg=3, T0c=7.6, maxn=12),
    Method("PQ-BN   parallel, blue-noise over reachable", "pqbn", "pq", mu=0.0, marg=2, lam=-1.0, T0c=7.6, maxn=12),
    Method("PQ-BN   integer (pqbn_fixed.c)", "pqbnq", "pq", marg=2, T0c=7.6),
    Method("PQ-BN   64KB 2-bit LUT, 4bpp target", "pqbnlut2", "pq", marg=2, T0c=7.6),
    Method("PQ-BN   event-driven (previous)", "pqbnev", "pq", marg=2, T0c=7.6),
    Method("PQ-BN v2 16-bit state, 32KB LUT, tiles", "pqbnv2", "pq", marg=2, T0c=7.6),
]
out = {}
for nm, F in scenes.items():
    for m in methods():
        r = run(m, F, lambda t: F[t], CFG)
        print(f"{nm:<6}", fmt(r), flush=True)
        out[f"{nm}|{m.name}"] = {k: v for k, v in r.items() if k != "rec"}
json.dump(out, open("res_metalio_quality.json", "w"), indent=1, default=float)
