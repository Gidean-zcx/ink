"""Re-link one original baseline implementation; leave source and build outputs intact."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
BASE = "c680b4b52454df342a79251d8a40bb3cd60243ec"
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", type=Path, required=True)
p.add_argument("--measure-baseline", action="store_true")
p.add_argument("--out", type=Path)
a = p.parse_args()
build = a.build.resolve()
work = a.out.resolve() if a.out else Path(tempfile.mkdtemp(prefix="perf-reverse-", dir=build))
work.mkdir(parents=True, exist_ok=True)
objects = []

def flags(target):
    found = {}
    for line in (build / f"CMakeFiles/{target}.dir/flags.make").read_text().splitlines():
        if " = " in line:
            key, val = line.split(" = ", 1)
            found[key] = shlex.split(val)
    return found

for case, product, wrapper, expected in (
    ("activity", "system/ui/shell/activity_card.c", "activity.c", "home reads job.list once"),
    ("count", "system/ui/shell/ai_pages.c", "count.c", "recent records need no full scan"),
    ("thumb", "system/services/file/file_service.c", None, "file lookup completes while thumbnail decoding continues"),
):
    source = work / f"{case}-original.c"
    source.write_bytes(subprocess.check_output(["git", "show", f"{BASE}:{product}"], cwd=ROOT))
    if wrapper:
        text = (ROOT / "tests/perf_latency" / wrapper).read_text()
        text = text.replace(f'"../../{product}"', f'"{source}"')
        driver = work / f"{case}-driver.c"
        driver.write_text(text)
    else:
        driver = source
    opts = flags("sim-perf-latency" if wrapper else "ink_system")
    link = shlex.split((build / "CMakeFiles/sim-perf-latency.dir/link.txt").read_text())
    obj = work / f"{case}.o"
    subprocess.run([link[0], *opts["C_DEFINES"], *opts["C_INCLUDES"], *opts["C_FLAGS"],
                    "-I" + str(ROOT / Path(product).parent), "-c", str(driver), "-o", str(obj)], cwd=build, check=True)
    objects.append((wrapper, obj))
    if a.measure_baseline:
        continue
    if wrapper:
        suffix = f"tests/perf_latency/{wrapper}.o"
        link = [str(obj) if part.endswith(suffix) else part for part in link]
    else:
        link.insert(next(i for i, part in enumerate(link) if part.endswith(".a")), str(obj))
    binary = work / f"sim-{case}"
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    result = subprocess.run([os.sys.executable, str(ROOT / "tests/perf_latency/run.py"), "--build", str(build),
                             "--regression", "--binary", str(binary), "--out", str(work / case)],
                            cwd=ROOT, capture_output=True, text=True, timeout=180)
    (work / f"{case}.log").write_text(result.stdout + result.stderr)
    data = json.loads((work / case / "run-01/metrics.json").read_text())
    errors = [w for w in data.get("warnings", []) if "CHECK FAILED" in w]
    if result.returncode != 1 or not errors or not all(expected in w for w in errors):
        raise SystemExit(f"{case}: unexpected negative control: exit={result.returncode}, errors={errors}")
    print(f"{case}: expected FAIL exit={result.returncode}, matching failures={len(errors)}", flush=True)
if a.measure_baseline:
    link = shlex.split((build / "CMakeFiles/sim-perf-latency.dir/link.txt").read_text())
    for wrapper, obj in objects:
        if wrapper:
            link = [str(obj) if part.endswith(f"tests/perf_latency/{wrapper}.o") else part for part in link]
        else:
            link.insert(next(i for i, part in enumerate(link) if part.endswith(".a")), str(obj))
    binary = work / "sim-baseline"
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    subprocess.run([os.sys.executable, str(ROOT / "tests/perf_latency/run.py"), "--build", str(build),
                    "--binary", str(binary), "--runs", "20", "--out", str(work / "measurements")], cwd=ROOT, check=True)
print("Evidence:", work.relative_to(ROOT))
