#!/usr/bin/env python3
"""Bounded R4 probes and negative controls; never modify product or existing tests.

Compilation reuses the previous audit's helper and this checkout's build flags.
Variants exist only in compiler stdin. Binaries and full logs stay in build dirs.
Each completed run is immediately appended to a small JSON evidence file here.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True  # importing the old audit's helper must not create files beside it

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
helper = ROOT / "docs/audits/2026-11-04-codex-system-r3/experiments/build_probe.py"
spec = importlib.util.spec_from_file_location("r3_build_probe", helper)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def once(text, old, new):
    assert text.count(old) == 1, (old, text.count(old))
    return text.replace(old, new)


def inline(target, product, text):
    source = ROOT / f"tests/integration/{target}.c"
    return once(source.read_text(), f'#include "../../{product}"',
                f'\n#line 1 "{product}"\n{text}\n#line 1 "{source}"\n')


def run(build, name, source, script, budget="tests/integration/hal_switch.budget.json",
        profile="metalio-plus", text=None, extras=(), description="", expected=0):
    build = (ROOT / build).resolve()
    directory = build / "audit-r4"
    directory.mkdir(exist_ok=True)
    log = directory / (name + ".log")
    record = {"name": name, "build": build.name, "description": description,
              "source": str(source.relative_to(ROOT)), "expected_exit": expected,
              "log": str(log.relative_to(ROOT))}
    if text is not None:
        record["test_compiler_input_sha256"] = hashlib.sha256(text.encode()).hexdigest()
    record["extra_compiler_inputs"] = [
        {"source": str(p.relative_to(ROOT)), "sha256": hashlib.sha256(t.encode()).hexdigest()}
        for p, t in extras]
    try:
        binary = module.build_probe(build, source, "r4_" + name, text, extras)
        record["compile_exit"] = 0
        out = directory / name
        cmd = [str(binary), "--profile", str(ROOT / f"simulator/profiles/{profile}.json"),
               "--script", str(ROOT / script), "--budget", str(ROOT / budget),
               "--assets", str(build / "assets"), "--content", str(build / "test-content"),
               "--out", str(out), "--quiet", "--no-random-outages"]
        record["command"] = cmd
        with log.open("w") as stream:
            done = subprocess.run(cmd, cwd=ROOT, stdout=stream, stderr=stream, timeout=90)
        record["exit"] = done.returncode
        if (out / "metrics.json").exists():
            data = json.loads((out / "metrics.json").read_text())
            record["metrics"] = {k: v for k, v in data.get("metrics", {}).items()
                                 if k.startswith(("test.", "audit.", "ai.", "rt.", "remote.", "cap."))}
            record["warnings"] = data.get("warnings", [])
        record["observations"] = [line for line in log.read_text(errors="replace").splitlines()
                                  if "AUDIT_" in line or "CHECK FAILED" in line
                                  or "ERROR: AddressSanitizer" in line or "runtime error:" in line][:40]
        record["expected_result"] = done.returncode == expected
    except subprocess.CalledProcessError as exc:
        record["compile_exit"] = exc.returncode
        record["expected_result"] = False
    except subprocess.TimeoutExpired:
        record["timeout_seconds"] = 90
        record["expected_result"] = False
    path = HERE / ("mutation_results.json" if name.startswith("mut_") else "probe_results.json")
    records = json.loads(path.read_text()) if path.exists() else []
    records.append(record)
    path.write_text(json.dumps(records, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(record, ensure_ascii=False), flush=True)
    return record["expected_result"]


def mutations(build):
    outcomes = []
    def check(name, target, text=None, extras=(), description=""):
        ok = run(build, name, ROOT / f"tests/integration/{target}.c",
                   f"tests/integration/{target}.script", f"tests/integration/{target}.budget.json",
                   "metalio-plus-handsfree" if target == "lifecycle" else "metalio-plus",
                   text, extras, description, 1)
        outcomes.append(ok)
        return ok

    hal_path = ROOT / "ports/sim/hal/hal_net_sim.c"
    hal = hal_path.read_text()
    no_notify = once(hal, '    else notify_link_locked(); /* not in use, but what is around changed (hal_net_available) */\n', '')
    check("mut_cell_notify", "net_auto", extras=[(hal_path, no_notify)],
          description="AUD13-001: remove only the notification for an unselected cellular network")

    ai_path = "system/services/ai/ai_service.c"
    ai = (ROOT / ai_path).read_text()
    no_stop = once(ai, '    AI.turn_over = true;\n    end_capture(true);', '    AI.turn_over = true;')
    check("mut_ai_capture", "ai_race", inline("ai_race", ai_path, no_stop),
          description="AUD13-002: leave capture running when the turn ends")
    no_guard = once(ai, '    if (AI.turn_over) return; /* it ended while the key was held */\n', '')
    check("mut_ai_release", "ai_race", inline("ai_race", ai_path, no_guard),
          description="AUD13-002: allow key release to overwrite a finished turn's status")

    remote_path = "system/apps/remote/remote.c"
    remote = (ROOT / remote_path).read_text()
    old_restore = once(remote,
                       '    for (int y = RS.spot.y1; y <= RS.spot.y2; y++)\n'
                       '        for (int x = RS.spot.x1; x <= RS.spot.x2; x++) RS.px[(size_t)y * (size_t)RS.w + (size_t)x] ^= 0xFF;',
                       '    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);\n'
                       '    copy_rect(RS.px, RS.back, &RS.spot);\n'
                       '    osal_mutex_unlock(RS.lock);')
    check("mut_remote_restore", "lifecycle", inline("lifecycle", remote_path, old_restore),
          description="AUD13-003: restore the feedback region from the incomplete back canvas")
    old_note = once(remote, 'lv_obj_align(RS.note, LV_ALIGN_TOP_MID, 0, ink_dp(16));',
                    'lv_obj_align(RS.note, LV_ALIGN_BOTTOM_MID, 0, -ink_dp(16));')
    check("mut_remote_note", "lifecycle", inline("lifecycle", remote_path, old_note),
          description="AUD13-004: put the note back beneath the answer sheet")

    wrong_link = once(hal, '        set_link(t, env_ok(t)); /* from the old link',
                      '        N.type = t;\n        set_link(t, env_ok(t)); /* from the old link')
    check("mut_hal_switch", "hal_switch", extras=[(hal_path, wrong_link)],
          description="HAL regression: overwrite the old network type before invalidating its sockets")
    return all(outcomes)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=["mutations", "probe"])
    parser.add_argument("--build", default="build-audit")
    parser.add_argument("--name")
    args = parser.parse_args()
    if args.mode == "mutations":
        ok = mutations(args.build)
    else:
        assert args.name
        ok = run(args.build, args.name, HERE / (args.name + ".c"),
            str((HERE / (args.name + ".script")).relative_to(ROOT)),
            description="Focused R4 boundary probe; unmodified product code included verbatim")
    raise SystemExit(0 if ok else 1)
