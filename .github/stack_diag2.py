#!/usr/bin/env python3
"""Diagnostic only (temporary CI step): raw own-zone stack use of the branch build on Linux, and what
sits at the deepest point of the probe task's own zone (symbolized). Run from the repo root after the build."""
import bisect, os, re, subprocess

ROOT = os.getcwd()
B = os.path.join(ROOT, "build")
args = ["--profile", "profiles/metalio-plus.json", "--script", "tests/scenarios/reader_basic.script",
        "--assets", B + "/assets", "--content", B + "/test-content", "--out", "/tmp/stack-diag2", "--quiet",
        "--no-random-outages"]
r = subprocess.run([B + "/ink-sim"] + args, capture_output=True, text=True, env=dict(os.environ, STACK_DIAG="1"))
maps, used, words, depth = [], [], {}, {}
for line in r.stderr.splitlines():
    if line.startswith("DIAG2M "):
        f = line.split()
        lo, hi = (int(x, 16) for x in f[1].split("-"))
        maps.append((lo, hi, int(f[3], 16), f[6] if len(f) > 6 else "?"))
    elif line.startswith("DIAG2 "):
        used.append(line[6:])
    elif line.startswith("DIAG2Z "):
        f = line.split()
        depth[f[1]] = f[2]
    elif line.startswith("DIAG2W "):
        f = line.split()
        words.setdefault(f[1], []).append((int(f[2]), int(f[3], 16)))
syms = {}
def nearest(path, off):
    if path not in syms:
        out = subprocess.run(["nm", "-D", "--defined-only", path], capture_output=True, text=True).stdout
        out += subprocess.run(["nm", "--defined-only", path], capture_output=True, text=True).stdout
        tab = sorted((int(a, 16), n) for a, t, n in (l.split()[:3] for l in out.splitlines() if len(l.split()) >= 3)
                     if t.lower() in ("t", "w"))
        syms[path] = tab
    tab = syms[path]
    i = bisect.bisect_right([a for a, _ in tab], off) - 1
    return f"{tab[i][1]}+0x{off - tab[i][0]:x}" if i >= 0 else "?"
def sym(v):
    for lo, hi, off, path in maps:
        if lo <= v < hi:
            a = v - lo + off
            if path.endswith("ink-sim"):
                s = subprocess.run(["addr2line", "-f", "-C", "-e", path, hex(a)], capture_output=True, text=True).stdout.split()
                return f"ink-sim:{s[0] if s else '?'}"
            return f"{os.path.basename(path)}:{nearest(path, a)}"
    return None
def notice(title, lines):
    body = "%0A".join(l.replace("%", "%25") for l in lines)
    print(f"::notice title={title}::{body}", flush=True)
notice("new scheme raw (reader_basic, plain)", used)
for task, ws in words.items():
    out = [f"{task}: deepest byte {depth.get(task)} below entry; code addresses on the stack (depth: symbol)"]
    for d, v in ws:
        s = sym(v)
        if s: out.append(f"{d}: {s}")
    notice(f"own zone of {task}", out[:120])
