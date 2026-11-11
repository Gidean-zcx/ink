"""Measure existing scenarios with temporary probes; product sources stay unchanged."""
import argparse
import json
import math
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
BASE = "6fbe0c4051843810a3252769ab34d150ac4c6097"
UI = "system/ui/ui_task.c"
REMOTE = "system/apps/remote/remote.c"
PANEL = "system/ui/shell/ai_panel.c"
DISPLAY = "system/services/display/display_service.c"
CASES = [(name, profile) for name in ("ai_ask", "reader_basic")
         for profile in ("metalio-plus", "metalio-plus-pqbn")]
CASES += [("remote_browse", "metalio-plus"), ("standby", "metalio-plus")]
METRICS = ("ai.press_to_listening_visible_ms", "ai.release_to_first_text_visible_ms",
           "ui.action_to_visible_ms.TEXT", "ui.action_to_first_change_ms",
           "remote.tap_feedback_visible_ms", "remote.tap_to_update_visible_ms",
           "power.unlock_to_visible_ms")
PROBES = {
    UI: [("    const ink_input_event_t *e = &m->ev;",
          '    ink_probe_value("lat.input", e->t_us);'),
         ("    if (code == LV_EVENT_REFR_START) {", '    ink_probe_mark("lat.draw");'),
         ("        if (UI.ndirty) {", '    ink_probe_value("lat.commit", UI.cause_us);')],
    DISPLAY: [("    bool preview = DS.flight[k].preview;",
               '    ink_probe_value("lat.id", r->id);\n'
               '    ink_probe_value("lat.cause", cur.cause_us);\n'
               '    ink_probe_value("lat.started", r->started_us);\n'
               '    ink_probe_value("lat.finished", r->done_us);')],
    REMOTE: [], PANEL: [],
}


def link(build, out, reverse):
    db = json.loads((build / "compile_commands.json").read_text())
    objects = []
    for name, edits in PROBES.items():
        entry, = [e for e in db if Path(e["file"]).resolve() == ROOT / name]
        code = (subprocess.check_output(["git", "show", f"{BASE}:{name}"], cwd=ROOT, text=True)
                if name in reverse else (ROOT / name).read_text())
        for anchor, probe in edits:
            assert code.count(anchor) == 1, (name, anchor)
            code = code.replace(anchor, anchor + "\n" + probe)
        source = out / Path(name).name
        source.write_text(code)
        obj = source.with_suffix(".o")
        args = shlex.split(entry["command"])
        args[args.index("-o") + 1] = str(obj)
        args[args.index("-c") + 1] = str(source)
        args += ["-iquote", str((ROOT / name).parent)]
        subprocess.run(args, cwd=entry["directory"], check=True)
        objects.append(str(obj))
    args = shlex.split((build / "CMakeFiles/sim-configured.dir/link.txt").read_text())
    binary = out / "sim-latency"
    args[args.index("-o") + 1] = str(binary)
    args[1:1] = objects
    subprocess.run(args, cwd=build, check=True)
    return binary


def samples(path):
    trace = json.loads(path.read_text())
    events = sorted((e for e in trace if e.get("pid") == 2 and "ts" in e), key=lambda e: e["ts"])
    tracks = {e.get("tid"): e.get("args", {}).get("name") for e in trace if e.get("pid") == 2 and e["ph"] == "M"}
    waves = {e["args"]["id"]: e for e in events
             if tracks.get(e.get("tid")) == "display" and "id" in e.get("args", {})}
    inputs = {}
    for event in events:
        if event["name"] == "lat.input":
            inputs.setdefault(event["args"]["value"], event["ts"])
    commits = [e for e in events if e["name"] == "lat.commit"]
    draws = [e["ts"] for e in events if e["name"] == "lat.draw"]
    done, rows = {}, []
    for event in events:
        name = event["name"]
        if name.startswith("lat.") and name not in ("lat.input", "lat.commit", "lat.draw"):
            done[name[4:]] = event["args"]["value"]
        if name not in METRICS:
            continue
        cause = done["cause"]
        if not cause:
            continue
        end = done["started"] if name.endswith("first_change_ms") else done["finished"]
        matches = [e["ts"] for e in commits if e["args"]["value"] == cause and e["ts"] <= end]
        assert matches, (name, cause)
        commit = matches[-1]
        draw = max(t for t in draws if t <= commit)
        handled = min(commit, max(cause, inputs.get(cause, cause)))
        start = max(commit, done["started"])
        # Four non-overlapping intervals, plus the callback/backcopy tail.
        wave = waves.get(done["id"])
        assert wave, done
        wave_end = min(end, wave["ts"] + wave["dur"])
        waveform = max(0, wave_end - start)
        tail = end - start - waveform
        row = {"metric": name, "total": event["args"]["value"],
               "input": (handled - cause) / 1000, "processing": (commit - handled) / 1000,
               "send": (start - commit) / 1000, "wave": waveform / 1000, "tail": tail / 1000,
               "draw": (commit - draw) / 1000, "waveform": wave["name"], "rect": wave["args"]}
        if name == "ai.press_to_listening_visible_ms":
            release = next(e["ts"] for e in events if e["name"] == "key-up" and e["ts"] > cause)
            row["listening_cleans"] = sum(w["name"] == "GC16" and cause <= w["ts"] < release for w in waves.values())
        assert abs(sum(row[k] for k in ("input", "processing", "send", "wave", "tail")) - row["total"]) < .003, row
        rows.append(row)
    return rows


def distribution(values):
    values = sorted(values)
    return {"count": len(values), **{f"p{p}": values[math.ceil(len(values) * p / 100) - 1] for p in (50, 95, 99)}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=Path("build"))
    parser.add_argument("--out", type=Path)
    parser.add_argument("--runs", type=int, default=20)
    parser.add_argument("--reverse", choices=("all", "remote", "panel"))
    parser.add_argument("--case", choices=sorted({n for n, _ in CASES}))
    parser.add_argument("--compare", type=Path)
    parser.add_argument("--regression", action="store_true")
    args = parser.parse_args()
    build = args.build.resolve()
    out = args.out.resolve() if args.out else Path(tempfile.mkdtemp(prefix="perf-latency-2-", dir=build))
    out.mkdir(parents=True, exist_ok=True)
    reverse = {"all": tuple(PROBES), "remote": (REMOTE,), "panel": (PANEL,)}.get(args.reverse, ())
    binary = link(build, out, reverse)
    rows, checks = [], []
    for name, profile in CASES:
        if args.case and name != args.case:
            continue
        for run in range(args.runs):
            case = out / f"{name}-{profile}-{run:02}"
            case.mkdir()
            cmd = [str(binary), "--profile", str(ROOT / f"simulator/profiles/{profile}.json"),
                   "--script", str(ROOT / f"tests/scenarios/{name}.script"),
                   "--budget", str(ROOT / f"tests/scenarios/{name}.budget.json"),
                   "--assets", str(build / "assets"), "--content", str(build / "test-content"),
                   "--out", str(case), "--quiet", "--trace", "--no-random-outages", "--seed", "42"]
            if name == "reader_basic" and profile.endswith("pqbn"):
                cmd[cmd.index("--budget") + 1] = str(ROOT / "tests/scenarios/reader_basic.pqbn.budget.json")
            if name == "remote_browse":
                cmd += ["--hub", str(ROOT / "tests/scenarios/remote_browse.hub.json")]
            with (case / "run.log").open("w") as log:
                result = subprocess.run(cmd, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=120)
            assert result.returncode == 0, (case.name, result.returncode)
            measured = samples(case / "trace.json")
            assert measured, case.name
            rows += [dict(row, scenario=name, profile=profile, run=run) for row in measured]
            metrics = json.loads((case / "metrics.json").read_text())["metrics"]
            checks.append({"scenario": name, "profile": profile, "run": run,
                           "updates": {k: v for k, v in metrics.items() if k.startswith("display.updates.")}})
            if args.regression and name == "remote_browse":
                remote = [r for r in measured if r["metric"] == "remote.tap_to_update_visible_ms"]
                assert len(remote) == 3 and all(r["waveform"] == "GL16" for r in remote), remote
            if args.regression and name == "ai_ask" and profile == "metalio-plus":
                assert all(r.get("listening_cleans", 0) == 0 for r in measured), measured
            if not os.environ.get("INK_KEEP_EVIDENCE"):
                (case / "trace.json").unlink()
            print(case.name, "passed", flush=True)
    summary = {}
    for row in rows:
        key = "/".join(row[k] for k in ("scenario", "profile", "metric"))
        if key not in summary:
            group = [r for r in rows if all(r[k] == row[k] for k in ("scenario", "profile", "metric"))]
            summary[key] = {phase: distribution(r[phase] for r in group)
                            for phase in ("total", "input", "processing", "send", "wave", "tail", "draw")}
    (out / "samples.ndjson").write_text("".join(json.dumps(r, ensure_ascii=False) + "\n" for r in rows))
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    (out / "checks.json").write_text(json.dumps(checks, indent=2) + "\n")
    if args.compare:
        before = json.loads(args.compare.read_text())
        for key, metric in summary.items():
            if key.endswith("remote.tap_to_update_visible_ms"):
                assert metric["total"]["p95"] < .95 * before[key]["total"]["p95"], key
    if not args.out and not os.environ.get("INK_KEEP_EVIDENCE"):
        shutil.rmtree(out)


if __name__ == "__main__":
    main()
