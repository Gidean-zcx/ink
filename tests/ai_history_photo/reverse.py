"""Manual reverse checks. Compile temporary source variants; leave the checkout untouched."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
p.add_argument("--hub-python", type=Path)
p.add_argument("--audio", action="store_true")
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
log = a.build / "verification/reverse"
log.mkdir(parents=True, exist_ok=True)
for name in ("write", "limit", "bytes", "order", "paging", "member", "clear", "delete",
             "replay", "offline", "photo", "photo-member", "photo-changed", "photo-id", "photo-display", "photo-switch"):
    case = "photo" if name == "photo-display" else name if name.startswith("photo") else "replay" if name in ("replay", "offline") else "history"
    r = subprocess.run([os.sys.executable, "-B", str(root / "tests/ai_history_photo/run.py"),
                        "--build", str(a.build), "--mutant", name, "--case", case],
                       cwd=root, capture_output=True, text=True, timeout=180)
    (log / f"{name}.log").write_text(r.stdout + r.stderr)
    if r.returncode != 1 or "CHECK FAILED" not in r.stdout:
        raise SystemExit(f"{name}: expected an assertion failure, got {r.returncode}\n{r.stdout}\n{r.stderr[-2500:]}")
    print(name, "PASS: removing the fix causes an assertion failure", flush=True)
if a.hub_python:
    code = ("import ink_hub.voice.flow as flow; flow.validate_photos = lambda attachments: None; "
            "import pytest; raise SystemExit(pytest.main(['-q', 'hub/tests/contract/test_ai_photos.py']))")
    r = subprocess.run([str(a.hub_python), "-B", "-c", code], cwd=root, capture_output=True, text=True, timeout=120)
    (log / "hub-photo-validation.log").write_text(r.stdout + r.stderr)
    if r.returncode != 1 or "7 failed, 2 passed" not in r.stdout or "DID NOT RAISE" not in r.stdout:
        raise SystemExit(f"hub validation: expected seven assertion failures\n{r.stdout}\n{r.stderr}")
    print("hub-photo-validation PASS: removing validation fails all seven rejection assertions", flush=True)
if a.audio:
    build = a.build.resolve()
    path = root / "system/services/audio/audio_service.c"
    source = path.read_text()
    assert source.count("#define JB_PREBUFFER 6") == 1
    target = log.resolve() / "audio-five.c"
    target.write_text(source.replace("#define JB_PREBUFFER 6", "#define JB_PREBUFFER 5"))
    entry = next(e for e in json.loads((build / "compile_commands.json").read_text()) if e["file"] == str(path))
    cmd = shlex.split(entry["command"])
    obj = target.with_suffix(".o")
    cmd[cmd.index("-o") + 1] = str(obj)
    cmd[cmd.index(str(path))] = str(target)
    subprocess.run(cmd, cwd=build, check=True)
    link = shlex.split((build / "CMakeFiles/sim-configured.dir/link.txt").read_text())
    binary = target.with_suffix("")
    link.insert(1, str(obj))
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
    line = next(l for l in (build / "CTestTestfile.cmake").read_text().splitlines()
                if l.startswith("add_test(scenario-ai_ask-metalio-plus-seed-29 "))
    cmd = shlex.split(line[len("add_test("):-1])[1:]
    cmd[0] = str(binary)
    cmd[cmd.index("--out") + 1] = str(log.resolve() / "audio-five")
    r = subprocess.run(cmd, cwd=root, capture_output=True, text=True, timeout=120)
    (log / "audio-five.log").write_text(r.stdout + r.stderr)
    if r.returncode != 1 or "BUDGET FAIL audio.speaker.underruns" not in r.stderr:
        raise SystemExit(f"audio: expected the original audible gap\n{r.stdout}\n{r.stderr}")
    print("audio-prefill PASS: restoring five frames reproduces the audible gap", flush=True)
