"""Run the same headless page and validate receipts, snapshots and rendered pixels."""
import argparse
import copy
import hashlib
import json
import os
import platform
import shlex
import statistics
import subprocess
from pathlib import Path

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[2]
FRONTENDS = {"c": "candidate_c.c", "json": "page.json", "lua": "page.lua", "mquickjs": "page.js"}
LABELS = ["显示进度", "显示页码", "记住位置", "阅读提醒", "词语提示", "显示注释", "听书续播", "显示书名"]


def check(result, metrics, candidate):
    m = metrics["metrics"]
    assert metrics["status"] == "ok"
    assert m.get("test.fail", 0) == 0 and m["test.pass"] >= 18, m.get("test.fail")
    assert m["action.user.spike.set"] == 1 and m["action.ai.spike.set"] == 3
    assert m["action.user.spike.submit"] == 1
    # An unsettled VM startup only advances the 0.5 us clock-read annotation.
    if candidate in ("lua", "mquickjs") and metrics["cpu_mode"] == "measured":
        assert result["startup_us"] > 1, "unsettled_cpu_clock"
    for name in ({"json": ["bad_json_rejected"], "lua": ["modules_hidden", "loop_stopped", "quota_rejected", "invalid_row_rejected"],
                  "mquickjs": ["modules_hidden", "loop_stopped", "quota_rejected", "invalid_row_rejected",
                               "invalid_type_rejected", "subset_rejected", "recovered"]}.get(candidate, [])):
        assert m[f"spike.{name}"]["min"] == 1, name
    assert result["released"]
    before, after = copy.deepcopy(result["snapshot"]), copy.deepcopy(result["snapshot_after"])
    assert after.pop("version") > before.pop("version") > 0
    assert before == after
    assert before["member"] == "owner" and before["title"] == "阅读偏好"
    assert before["rows"] == [{"id": i, "label": label, "value": i % 2 == 0} for i, label in enumerate(LABELS)]
    assert before["field"] == {"id": "note", "role": "textbox", "label": "备注", "value": "每天读一会儿"}
    assert before["actions"] == ["spike.set", "text.insert", "text.submit", "spike.submit"]


def run(build, out, candidate, mode, timeout=45):
    out.mkdir(parents=True, exist_ok=True)
    env = {**os.environ, "INK_SPIKE_SOURCE": str(ROOT / "tools/appfw-spike" / FRONTENDS[candidate])}
    command = [str(build / "appfw-spike" / f"appfw-{candidate}"), "--profile", "simulator/profiles/metalio-plus.json",
               "--assets", str(build / "assets"), "--content", str(build / "test-content"), "--out", str(out),
               "--cpu", mode, "--end", "90s", "--quiet", "--no-random-outages"]
    with (out / "run.log").open("w") as log:
        subprocess.run(command, cwd=ROOT, env=env, check=True, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
    result = json.loads((out / "spike.json").read_text())
    metrics = json.loads((out / "metrics.json").read_text())
    check(result, metrics, candidate)
    return result, metrics


def source_metrics(path):
    text = path.read_text()
    return {"lines": len(text.splitlines()), "nonblank_lines": sum(bool(s.strip()) for s in text.splitlines()),
            "characters": len(text), "utf8_bytes": len(text.encode()), "sha256": hashlib.sha256(text.encode()).hexdigest()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--samples", type=int, default=5)
    args = parser.parse_args()
    assert 1 <= args.samples <= 20
    build, out = args.build.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out / "measurements.json").unlink(missing_ok=True)
    cache = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if line and not line.startswith(("#", "//")) and "=" in line and ":" in line.split("=", 1)[0]:
            key, value = line.split("=", 1)
            cache[key.split(":", 1)[0]] = value
    cc = subprocess.run([cache["CMAKE_C_COMPILER"], "--version"], check=True, capture_output=True, text=True).stdout.splitlines()[0]
    processor = platform.processor()
    processor_probe = "platform.processor"
    if platform.system() == "Darwin":
        probe = subprocess.run(["sysctl", "-n", "machdep.cpu.brand_string"], capture_output=True, text=True)
        if probe.returncode == 0:
            processor = probe.stdout.strip()
            processor_probe = "sysctl"
        else:
            processor_probe = "CPU model unavailable in sandbox; using platform.processor"
    font = Path(os.environ["INK_DEPS"]) / "build-deps/fonts/NotoSansCJK-Regular.ttc"
    build_type = cache.get("CMAKE_BUILD_TYPE") or "RelWithDebInfo"
    actual_flags = ""
    flags_file = build / "CMakeFiles/appfw-c.dir/flags.make"
    if flags_file.exists():
        actual_flags = next(line.partition("=")[2].strip() for line in flags_file.read_text().splitlines() if line.startswith("C_FLAGS ="))
        parts = shlex.split(actual_flags)
        actual_flags = " ".join("<sdk>" if i and parts[i - 1] == "-isysroot" else "<path>" if part.startswith("/") else part
                                for i, part in enumerate(parts))
    report = {"evidence": "【实测H】host simulator; numeric fields share the recorded environment and definitions",
              "environment": {"system": platform.system(), "release": platform.release(), "machine": platform.machine(),
                              "macos": platform.mac_ver()[0], "processor": processor, "processor_probe": processor_probe,
                              "compiler": cc, "build_type": build_type, "build_type_defaulted": not bool(cache.get("CMAKE_BUILD_TYPE")),
                              "cflags": cache.get("CMAKE_C_FLAGS", ""), "build_type_flags": cache.get("CMAKE_C_FLAGS_" + build_type.upper(), ""),
                              "actual_c_flags": actual_flags,
                              "werror": cache["INK_WERROR"], "sanitize": cache["INK_SANITIZE"],
                              "font_sha256": hashlib.sha256(font.read_bytes()).hexdigest(), "font_face": 2,
                              "lvgl": "9.5.0", "lua": "5.4.8", "mquickjs": "4119f6819ccb5bae2d21a18a106e89522d18ad45",
                              "profile": "metalio-plus", "measured_cpu_factor": 1},
              "definitions": {"timing": "simulator host-thread CPU x1 plus scheduler/fixed-time costs; cycle annotations disabled; NOT S31 wall time",
                              "heap": "whole-scenario simulated TLSF pool peak; includes shared product and input service",
                              "vm_peak": "VM tag peak in simulated PSRAM including allocator bookkeeping; MicroQuickJS reserves a fixed 128 KiB arena",
                              "characters": "Unicode code points, original whitespace, no prompt/schema/tool overhead"},
              "candidates": {}}
    golden = None
    for candidate, frontend in FRONTENDS.items():
        result, metrics = run(build, out / f"{candidate}-annotated-1", candidate, "annotated")
        again, metrics_again = run(build, out / f"{candidate}-annotated-2", candidate, "annotated")
        assert result == again and metrics == metrics_again, f"nondeterministic {candidate}"
        image = Image.open(out / f"{candidate}-annotated-1" / "snap-page.png").convert("L")
        assert image.size == (684, 1216)
        assert image.getextrema() == (0, 255)
        if golden is None:
            golden = image
        else:
            assert ImageChops.difference(golden, image).getbbox() is None, f"different page pixels: {candidate}"
        samples = [run(build, out / f"{candidate}-measured-{i+1}", candidate, "measured:1")[0] for i in range(args.samples)]
        timing = {key: {"median": statistics.median(s[key] for s in samples), "min": min(s[key] for s in samples),
                        "max": max(s[key] for s in samples)} for key in ("startup_us", "build_us", "toggle_us")}
        snapshot = json.dumps(result["snapshot"], ensure_ascii=False, separators=(",", ":"))
        report["candidates"][candidate] = {"input": source_metrics(ROOT / "tools/appfw-spike" / frontend),
                                            "snapshot_characters": len(snapshot), "snapshot_utf8_bytes": len(snapshot.encode()),
                                            "deterministic": result, "timing_us": timing, "samples": samples,
                                            "screenshot_sha256": hashlib.sha256(image.tobytes()).hexdigest(),
                                            "test_pass": metrics["metrics"]["test.pass"],
                                            "guard_checks": {k: v for k, v in metrics["metrics"].items() if k.startswith("spike.")}}
        print(f"{candidate}: deterministic, same pixels, {metrics['metrics']['test.pass']} checks; {timing}")
    report["source_inventory"] = {path.name: source_metrics(path) for path in sorted((ROOT / "tools/appfw-spike").iterdir())
                                 if path.suffix in (".c", ".h", ".py", ".sh", ".cmake", ".txt") and path.name not in FRONTENDS.values()}
    lua_src = ROOT / "work/appfw-spike/lua-5.4.8/src"
    lua_files = [p for p in sorted(lua_src.glob("*.c")) if p.name not in ("lua.c", "luac.c")]
    report["upstream_lua_source"] = {"compiled_c_files": len(lua_files), "physical_c_lines": sum(source_metrics(p)["lines"] for p in lua_files),
                                      "archive_sha256": hashlib.sha256((lua_src.parent.parent / "lua-5.4.8.tar.gz").read_bytes()).hexdigest()}
    (out / "measurements.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")


if __name__ == "__main__":
    main()
