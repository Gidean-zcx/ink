"""Automatic mode selection in the simulator: the same planner budget (what two S31 cores afford, scaled to the
sim's pixel count), with and without auto_motion, on the video, text and fling scenes."""
import json, numpy as np
import sim
from sim import Method, run, CLEAN
from run_exp import fmt
from scipy.ndimage import gaussian_filter
from run_fling import fling_scene
sim.N = 8
CFG = dict(CLEAN, T0=7.6)
F_fl = fling_scene()
scenes = {"video": np.load("V.npy"), "text": np.load("X.npy"), "fling": F_fl}
scale = F_fl[0].size / (1216 * 684)
B2 = int(19500 * scale)
out = {}
for nm, F in scenes.items():
    moving = [t for t in range(1, len(F)) if not np.array_equal(F[t], F[t - 1])]
    cases = [("v4, unlimited planner (reference)", dict(budget=0)),
             ("v4, 2-core budget, PQ-BN only", dict(budget=B2)),
             ("v4, 2-core budget, automatic modes", dict(budget=B2, auto=True))]
    if nm == "fling":
        cases.append(("v4, 2-core budget, scripted hybrid", dict(budget=B2, motion=[(moving[0] + 1, moving[-1] + 2)])))
    for name, kw in cases:
        m = Method(name, "pqbnv4", "pq", marg=2, T0c=7.6, **kw)
        r = run(m, F, lambda t: F[t], CFG, record=True)
        rec = r.pop("rec")
        ps = np.array([np.mean((gaussian_filter(rec[t].astype(np.float32) / 255.0, 1.2) - gaussian_filter(F[t], 1.2)) ** 2) for t in range(len(F))])
        a, b = moving[0], moving[-1]
        r["psnr_during"] = float(10 * np.log10(1 / ps[a:b + 1].mean()))
        r["motion_phases"] = int(m.motion_phases)
        print(f"{nm:<6}", fmt(r), f" during changes {r['psnr_during']:.2f}  phases with motion tiles {m.motion_phases}", flush=True)
        out[f"{nm}|{name}"] = r
json.dump(out, open("res_auto.json", "w"), indent=1, default=float)
