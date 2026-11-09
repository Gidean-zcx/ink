"""Check real SMS UI/AT paths and reboot persistence; retain evidence on request."""
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

mutant = sys.argv[-1] == "--mutant"
sim, root, assets, content, output = map(Path, sys.argv[1:-1] if mutant else sys.argv[1:])
sim, root, assets, content, output = (p.resolve() for p in (sim, root, assets, content, output))
shutil.rmtree(output, ignore_errors=True)
output.mkdir(parents=True)
if mutant:
    build = sim.parent.resolve()
    flags = {}
    for line in (build / "CMakeFiles/sim-sms.dir/flags.make").read_text().splitlines():
        if " = " in line:
            key, value = line.split(" = ", 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / "CMakeFiles/sim-sms.dir/link.txt").read_text())
    obj = output / "member-mutant.o"
    subprocess.run([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
                    "-DINK_SMS_NO_MEMBER_CHECK", "-c", str(root / "tests/sms/sms.c"), "-o", str(obj)], check=True)
    link = [str(obj) if arg.endswith("tests/sms/sms.c.o") else arg for arg in link]
    sim = output / "sim-sms-mutant"
    link[link.index("-o") + 1] = str(sim)
    subprocess.run(link, cwd=build, check=True)
ok = True
for phase in ("write", "verify", "SmsThread", "SmsCompose"):
    image = output / ("user.img" if phase in ("write", "verify") else f"{phase}.img")
    out = output / phase
    command = [str(sim), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
               "--script", str(root / "tests/sms/sms.script"), "--budget", str(root / "tests/ui_scene.budget.json"),
               "--assets", str(assets), "--content", str(content), "--flash-image", str(image),
               "--sd-dir", str(output / "sd"), "--out", str(out), "--quiet", "--no-random-outages"]
    result = subprocess.run(command, env=dict(os.environ, INK_SMS_PHASE=phase), capture_output=True, text=True)
    (out / "process.log").write_text(result.stdout + result.stderr)
    if not (out / "metrics.json").exists():
        raise SystemExit(f"{phase}: process exit {result.returncode}, no metrics\n{result.stdout[-3000:]}\n{result.stderr[-4000:]}")
    metrics = json.loads((out / "metrics.json").read_text())
    counters = metrics.get("metrics", {})
    passed = result.returncode == 0 and counters.get("test.finished") == 1 and counters.get("test.fail", 0) == 0
    print(phase, "PASS" if passed else "FAIL", "exit", result.returncode, "checks", counters.get("test.pass", 0), "failures", counters.get("test.fail", 0), flush=True)
    if not passed:
        print(result.stderr[-4000:])
        for warning in metrics.get("warnings", []):
            print(warning)
        ok = False
        break
    if phase != "verify":
        scene = "Sms" if phase == "write" else phase
        target = output.parent / "sms-compare" / scene
        target.mkdir(parents=True, exist_ok=True)
        for name in ("snap-scene.png", "metrics.json"):
            shutil.copyfile(out / name, target / name)
if ok and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(output)
raise SystemExit(0 if ok else 1)
