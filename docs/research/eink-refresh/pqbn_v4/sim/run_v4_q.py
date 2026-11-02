"""Quality of the v3 and v4 kernels on Metalio-like timing (same setup as run_metalio_q.py)."""
import sys, json, time, numpy as np
import sim
from sim import Method, run, CLEAN
from run_exp import fmt
sim.N = 8
CFG = dict(CLEAN, T0=7.6)
scenes = {"video": np.load("V.npy"), "text": np.load("X.npy")}
which = sys.argv[1:] or ["pqbnev", "pqbnv3", "pqbnv4"]
names = {"pqbnev": "PQ-BN   event-driven (previous)", "pqbnv3": "PQ-BN v3 16-bit state, 32KB LUT",
         "pqbnv4": "PQ-BN v4 plan/execute, 2.5 B/px"}
out = {}
for nm, F in scenes.items():
    for q in which:
        m = Method(names[q], q, "pq", marg=2, T0c=7.6)
        t0 = time.time()
        r = run(m, F, lambda t: F[t], CFG)
        extra = ""
        if q == "pqbnv4":
            extra = f"  active words/phase {m.words_active / len(F):.0f}  tiles planned/phase {m.tiles_planned / len(F):.1f}"
        print(f"{nm:<6}", fmt(r), extra, f"({time.time() - t0:.0f}s)", flush=True)
        out[f"{nm}|{m.name}"] = {k: v for k, v in r.items() if k != "rec"}
json.dump(out, open("res_v4_quality.json", "w"), indent=1, default=float)
