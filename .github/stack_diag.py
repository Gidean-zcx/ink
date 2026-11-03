#!/usr/bin/env python3
"""Diagnostic only (temporary CI step): host stack estimates of current main vs this branch.

Builds origin/main next to the branch build, runs every ink-sim / sim-selftest test command of the
branch's ctest with both binaries, and prints GitHub notice annotations:
  - per task: target stack, max estimate over all runs on main and on the branch;
  - main's raw probe baseline (patched debug print), which is what main subtracts.
Run from the repository root after `cmake --build build`.
"""
import json
import os
import re
import subprocess
import sys

ROOT = os.getcwd()
NEW = os.path.join(ROOT, "build")
OLD_SRC = os.environ.get("STACK_DIAG_OLD_SRC", "/tmp/ink-old")  # an existing tree here skips the fetch (local runs)
OLD = OLD_SRC + "/build"
OUT = os.environ.get("STACK_DIAG_OUT", "/tmp/stack-diag")
CMAKE_ARGS = os.environ.get("STACK_DIAG_CMAKE_ARGS", "").split()
CTEST = os.environ.get("STACK_DIAG_CTEST", "ctest")


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, **kw)


def notice(title, lines):
    body = "%0A".join(l.replace("%", "%25").replace("\r", "").replace("\n", " ") for l in lines)
    print(f"::notice title={title}::{body}", flush=True)


if not os.path.isdir(OLD_SRC):
    run(["git", "fetch", "--no-tags", "--depth=1", "origin", "main"])
    run(["git", "worktree", "add", "--detach", OLD_SRC, "FETCH_HEAD"])
sha = subprocess.run(["git", "-C", OLD_SRC, "rev-parse", "--short", "HEAD"], capture_output=True, text=True).stdout.strip() or "?"
vtk = os.path.join(OLD_SRC, "sim/vtk/vtk.c")
src = open(vtk).read()
anchor = "    size_t net = used_host > base ? used_host - base : 0;\n"
assert anchor in src
src = src.replace(anchor, anchor + '    if (getenv("STACK_DIAG")) fprintf(stderr, "DIAG %s used=%zu base=%zu\\n", t->name, used_host, base);\n', 1)
open(vtk, "w").write(src)
run(["cmake", "-S", OLD_SRC, "-B", OLD] + CMAKE_ARGS, stdout=subprocess.DEVNULL)
run(["cmake", "--build", OLD, "-j", "--target", "ink-sim", "sim-selftest"], stdout=subprocess.DEVNULL)

tests = json.loads(subprocess.run([CTEST, "--test-dir", NEW, "--show-only=json-v1"], capture_output=True, text=True,
                                  check=True).stdout)["tests"]
targets, est = {}, {}  # task -> target bytes; (variant, task) -> (max est, test)
base_old = set()
for t in tests:
    cmd = t.get("command") or []
    if not cmd or os.path.basename(cmd[0]) not in ("ink-sim", "sim-selftest"):
        continue
    env = {}
    for p in t.get("properties", []):
        if p["name"] == "ENVIRONMENT":
            env = dict(e.split("=", 1) for e in p["value"])
    if env:
        continue  # death test
    group = "self" if os.path.basename(cmd[0]) == "sim-selftest" else "sys"
    for variant, bindir in (("main", OLD), ("branch", NEW)):
        c = [os.path.join(bindir, os.path.basename(cmd[0]))] + cmd[1:]
        out = os.path.join(OUT, variant, t["name"])
        c[c.index("--out") + 1] = out
        r = subprocess.run(c, capture_output=True, text=True, env=dict(os.environ, STACK_DIAG="1"))
        for m in re.finditer(r"DIAG \(stack-probe\) used=(\d+) base=(\d+)", r.stderr):
            base_old.add(int(m.group(2)))
        try:
            met = json.load(open(os.path.join(out, "metrics.json")))["metrics"]
        except (OSError, ValueError):
            continue
        for k, v in met.items():
            m = re.fullmatch(r"task\.(.+)\.stack_used_est_bytes", k)
            if not m:
                continue
            task = (group, m.group(1))
            targets[task] = int(met.get(f"task.{m.group(1)}.stack_bytes", 0))
            cur = est.get((variant, task))
            if cur is None or v > cur[0]:
                est[(variant, task)] = (int(v), t["name"])

for group, title in (("sys", "system tasks"), ("self", "selftest tasks")):
    lines = [f"main {sha} vs branch; max over runs; task target main branch (where)"]
    for task in sorted(k for k in targets if k[0] == group):
        o, n = est.get(("main", task), (0, "")), est.get(("branch", task), (0, ""))
        flag = " OVER" if n[0] > targets[task] else ""
        lines.append(f"{task[1]} {targets[task]} {o[0]} {n[0]}{flag} ({n[1]})")
    notice(f"stack estimates, {title}", lines)
notice("main probe baseline", [f"main subtracts the probe's host stack use: {sorted(base_old)} bytes"])
