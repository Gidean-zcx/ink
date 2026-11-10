"""Manual benchmark: link temporary probe-enabled objects into the configured simulator."""
import argparse
import json
import math
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
BASE = "5cc0a6ebefceb8f28d7b0e49b7d99e63cef014d3"
APPS = ("settings", "files", "sms", "gallery", "ai")
UI = "system/ui/ui_task.c"
SMS = "system/apps/sms/sms.c"


def mark(name):
    return f'    ink_probe_mark("open.{name}");\n'


def before(line, name):
    return line, mark(name) + line


def after(line, name):
    return line, line + "\n" + mark(name).rstrip()


PROBES = {
    UI: (
        after("static void show_top(void) {", "create.begin"),
        after("    if (old) lv_obj_delete(old);", "create.end"),
        after('    osal_cost_units("ui.rotate_px", (uint64_t)w * (uint64_t)h);  /* transpose into the native framebuffer */', "rotate.begin"),
        before("    add_dirty(native_rect(area), intent_for(area));", "rotate.end"),
        before("            if (UI.staged) ink_display_commit_staged", "commit"),
    ),
    "system/policy/cap.c": ((
        "    return call_with_intent(addr, args, o, false);",
        '    bool launch = addr && !strcmp(addr, "shell.launch");\n'
        '    if (launch) ink_probe_mark("open.cap.begin");\n'
        '    int result = call_with_intent(addr, args, o, false);\n'
        '    if (launch) ink_probe_mark("open.cap.end");\n'
        "    return result;",
    ), (
        "            c->run(&p->call);",
        '            bool list = p->call.src == INK_SRC_TOUCH && !strcmp(c->id, "data.list");\n'
        '            if (list) ink_probe_mark("open.data_list.begin");\n'
        '            c->run(&p->call);\n'
        '            if (list) ink_probe_mark("open.data_list.end");',
    )),
    "system/ui/app_registry.c": (
        before("    int n = ink_storage_read(path, buf, INK_APP_STATE_MAX);", "restore.begin"),
        after("/* sees what is still queued for it */", "restore.end"),
    ),
    SMS: (
        before('    S.db = ink_db_open_cached_member(ink_db_find_def("sms.messages"), S.member);', "sms_db.begin"),
        before("    __atomic_store_n(&S.shown_ver, S.db ?", "sms_db.end"),
        ("; build_rows(compose, thread);", ";\n" + mark("sms_rows.begin") + "    build_rows(compose, thread);\n" + mark("sms_rows.end").rstrip()),
    ),
    "system/apps/gallery/gallery.c": (
        after("static void read_photos(void *arg) {", "gallery_index.begin"),
        before("    ink_exec_post(INK_EXEC_UI, &p->job, loaded, p);", "gallery_index.end"),
    ),
    "lv_obj_pos.c": (
        after("    LV_PROFILER_LAYOUT_BEGIN;", "layout.begin"),
        before("    LV_PROFILER_LAYOUT_END;", "layout.end"),
    ),
    "lv_refr.c": (before("    lv_display_send_event(disp_refr, LV_EVENT_RENDER_START, NULL);", "draw"),),
    "ports/sim/hal/hal_display_sim.c": (("    D.pending++;\n    osal_err_t e = (flags",
        '    ink_probe_value("open.submit", q.id);\n    D.pending++;\n    osal_err_t e = (flags'),),
}


def link(build, out, revert):
    db = json.loads((build / "compile_commands.json").read_text())

    def compile_(name, edits=(), driver=False):
        matches = [e for e in db if (Path(e["file"]).name == name if "/" not in name
                   else Path(e["file"]).resolve() == ROOT / name)]
        entry, = matches
        original = Path(entry["file"]).resolve()
        source = HERE / "device.c"
        if not driver:
            code = (subprocess.check_output(["git", "show", f"{BASE}:{name}"], cwd=ROOT, text=True)
                    if name in revert else original.read_text())
            for old, new in edits:
                if code.count(old) != 1:
                    raise ValueError(f"expected one probe anchor in {name}: {old}")
                code = code.replace(old, new)
            source = out / original.name
            source.write_text('#include "ink/probe.h"\n' + code)
        args = shlex.split(entry["command"])
        obj = out / (source.stem + ".o")
        args[args.index("-o") + 1] = str(obj)
        args[args.index("-c") + 1] = str(source)
        args += ["-iquote", str(original.parent), "-I", str(ROOT / "platform/include")]
        subprocess.run(args, cwd=entry["directory"], check=True)
        if driver:
            (out / "compile_commands.json").write_text(json.dumps([
                {"file": str(source), "directory": entry["directory"], "command": shlex.join(args)}
            ]))
        return str(obj)

    command = shlex.split((build / "CMakeFiles/sim-configured.dir/link.txt").read_text())
    binary = out / "sim-open-app"
    command[command.index("-o") + 1] = str(binary)
    i = next(i for i, arg in enumerate(command) if arg.endswith("/tests/configured_boot.c.o"))
    command[i:i + 1] = [compile_("tests/configured_boot.c", driver=True),
                        *(compile_(name, edits) for name, edits in PROBES.items())]
    subprocess.run(command, cwd=build, check=True)
    return binary


def measure(trace):
    tracks = {e["tid"]: e["args"]["name"] for e in trace if e.get("pid") == 2 and e["ph"] == "M" and "tid" in e}
    events = sorted((e for e in trace if e.get("pid") == 2 and "ts" in e), key=lambda e: e["ts"])
    rows = {}
    for app in APPS:
        def only(name, seq=events):
            event, = [e for e in seq if e["name"] == name]
            return event

        start = only("open." + app)["ts"]
        sample = [e for e in events if e["ts"] >= start]
        stop = next(e["ts"] for e in sample if e["name"] == "open.end")
        sample = [e for e in sample if e["ts"] <= stop]
        assert not any(e["name"] == "underrun-recovery" for e in sample), app
        down, up = (only(name, sample)["ts"] for name in ("finger-down", "finger-up"))
        ready = only("open.readable_frame", sample)["ts"]
        commit = max(e["ts"] for e in sample if e["name"] == "open.commit" and e["ts"] <= ready)
        submit = next(e for e in sample if e["name"] == "open.submit" and e["ts"] >= commit)
        waveforms = [e for e in sample if tracks.get(e.get("tid")) == "display" and "id" in e.get("args", {})]
        first, = [e for e in waveforms if e["args"]["id"] == submit["args"]["value"]]
        end = first["ts"] + first["dur"]
        assert down < up < commit <= submit["ts"] <= first["ts"] <= end <= stop, app

        def spans(name):
            begins, ends = ([e["ts"] for e in sample if e["name"] == f"open.{name}.{edge}"]
                            for edge in ("begin", "end"))
            pairs = list(zip(begins, ends, strict=True))
            assert all(a <= b for a, b in pairs), (app, name)
            return pairs

        def track_spans(track, name):
            return [(e["ts"], e["ts"] + e["dur"]) for e in sample
                    if e["name"] == name and tracks.get(e.get("tid")) == track and e["ph"] == "X"]

        cap, = spans("cap")
        creates = spans("create")
        assert creates and up <= cap[0] <= creates[0][0] <= creates[0][1] <= cap[1] <= commit, app
        refresh = [(e["ts"], e["ts"] + e["dur"]) for e in waveforms if e["ts"] <= first["ts"]]
        data = [s for name in ("restore", "sms_db", "sms_rows", "data_list", "gallery_index") for s in spans(name)]
        layout = spans("layout")
        # Charge overlaps once: scan > input > UI layout > data > creation > capability > send.
        phases = {"refresh": refresh, "input": [(up, cap[0])], "layout": layout, "data": data,
                  "create": creates, "capability": [cap], "send": [(up, end)]}
        edges = sorted({max(up, min(end, t)) for pairs in phases.values() for pair in pairs for t in pair})
        costs = dict.fromkeys(phases, 0.0)
        for lo, hi in zip(edges, edges[1:]):
            for key, pairs in phases.items():
                if any(a <= lo and hi <= b for a, b in pairs):
                    costs[key] += (hi - lo) / 1000
                    break

        def elapsed(pairs):
            return sum(max(0, min(b, end) - max(a, up)) for a, b in pairs) / 1000

        draw = max(e["ts"] for e in sample if e["name"] == "open.draw" and e["ts"] <= commit)
        rows[app] = {**{k: round(v, 3) for k, v in costs.items()}, "total": round((end - up) / 1000, 3),
                     "from_down": round((end - down) / 1000, 3), "draw": round((commit - draw) / 1000, 3),
                     "rotate": round(elapsed(spans("rotate")), 3), "power_up": round(elapsed(track_spans("display", "power-up")), 3),
                     "diff": round(elapsed(track_spans("display", "diff")), 3), "flash_read": round(elapsed(track_spans("flash", "read")), 3),
                     "sd_read": round(elapsed(track_spans("sd", "read")), 3), "sd_write": round(elapsed(track_spans("sd", "write")), 3),
                     "data_raw": round(elapsed(data), 3), "waves": len(refresh), "waveform": first["name"],
                     "phases": int(only("open.phases", sample)["args"]["value"]),
                     "frame_hash": f'{int(only("open.frame_hash", sample)["args"]["value"]):08x}',
                     "rect": [first["args"][k] for k in ("x", "y", "w", "h")]}
    return rows


def percentiles(values):
    values = sorted(values)
    return [values[math.ceil(len(values) * p) - 1] for p in (0.50, 0.95)]


def compare(samples, base):
    errors = []
    for app in APPS:
        if {r[app]["frame_hash"] for r in samples} != {r[app]["frame_hash"] for r in base}:
            errors.append(f"{app}: final native framebuffer differs")
    for app in ("files", "ai"):
        old = percentiles(r[app]["rotate"] for r in base)[0]
        new = percentiles(r[app]["rotate"] for r in samples)[0]
        if new > 0.9 * old:
            errors.append(f"{app}: rotation p50 {old:.3f} -> {new:.3f} ms (want at least 10% lower)")
    counts = [r["sms"]["phases"] for r in samples]
    if any(r["sms"]["waves"] != 1 or r["sms"]["phases"] != 48 for r in samples):
        errors.append(f"cold SMS must show its first readable frame in one GC16 scan: {counts}")
    if errors:
        raise SystemExit("\n".join(errors))
    print("Comparison passed: same five framebuffers, less rotation work, one cold SMS scan.", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=Path("build"))
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--runs", type=int, default=20)
    parser.add_argument("--reverse", choices=("rotate", "sms", "all"), help="link selected product source from the baseline")
    parser.add_argument("--compare", type=Path, help="check against baseline samples from this machine")
    parser.add_argument("--seed", type=Path, help="reuse a configured seed image from a previous run")
    args = parser.parse_args()
    build, out = args.build.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    revert = {"rotate": (UI,), "sms": (SMS,), "all": (UI, SMS)}.get(args.reverse, ())
    binary = link(build, out, revert)
    script = out / "run.script"
    script.write_text("@0 net wifi\n@120s end\n")
    common = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
              "--script", str(script), "--assets", str(build / "assets"), "--content", str(build / "test-content"),
              "--quiet", "--no-random-outages", "--cpu", "measured", "--trace"]
    seed = args.seed.resolve() if args.seed else out / "seed" / "user.img"
    samples = []
    for i in range(1 if args.seed else 0, args.runs + 1):
        case = out / (f"run-{i:02}" if i else "seed")
        case.mkdir()
        image = case / "user.img"
        if i:
            shutil.copyfile(seed, image)
        env = {k: v for k, v in os.environ.items() if k != "INK_OPEN_SEED"}
        if not i:
            env["INK_OPEN_SEED"] = "1"
        with (case / "run.log").open("w") as log:
            subprocess.run([*common, "--out", str(case), "--flash-image", str(image), "--sd-dir", str(case / "sd")],
                           cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=150, check=True)
        m = json.loads((case / "metrics.json").read_text())["metrics"]
        if m.get("test.fail", 0) or m.get("test.finished") != 1:
            raise SystemExit(f"{case.name}: benchmark failed; see {case / 'run.log'}")
        if i:
            samples.append(measure(json.loads((case / "trace.json").read_text())))
            (out / "samples.json").write_text(json.dumps(samples, ensure_ascii=False, indent=2) + "\n")
        print(f"{case.name}: {m['test.pass']} checks, no failures", flush=True)
    summary = {}
    for app in APPS:
        summary[app] = {k: percentiles(row[app][k] for row in samples)
                        for k, v in samples[0][app].items() if isinstance(v, (int, float))}
    (out / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
    if args.compare:
        compare(samples, json.loads(args.compare.read_text()))


if __name__ == "__main__":
    main()
