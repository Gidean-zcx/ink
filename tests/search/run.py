"""Search acceptance and source-only member-isolation negative control."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--reverse", action="store_true")
args = parser.parse_args()
build = args.build.resolve()
work = Path(tempfile.mkdtemp(prefix="search-", dir=build))
binary = build / "sim-search"
if args.reverse:
    source = (ROOT / "system/ui/search.c").read_text()
    old = "ink_db_open_member(ink_db_find_def(collections[i]), q->member)"
    assert source.count(old) == 1
    (work / "search-product.c").write_text(source.replace(old, 'ink_db_open_member(ink_db_find_def(collections[i]), "owner")'))
    test = (ROOT / "tests/search/search.c").read_text().replace(
        '#include "../../system/ui/search.c"', '#include "search-product.c"').replace(
        '#include "../../system/boot/main.c"', f'#include "{ROOT}/system/boot/main.c"').replace(
        '#include "../integration/whole_product.h"', f'#include "{ROOT}/tests/integration/whole_product.h"')
    driver = work / "driver.c"
    driver.write_text(test)
    flags = (build / "CMakeFiles/sim-search.dir/flags.make").read_text()
    options = []
    for key in ("C_DEFINES", "C_INCLUDES", "C_FLAGS"):
        options += shlex.split(re.search(r"^" + key + r" = (.*)$", flags, re.M)[1])
    link = shlex.split((build / "CMakeFiles/sim-search.dir/link.txt").read_text())
    obj = work / "driver.o"
    subprocess.run([link[0], *options, "-c", str(driver), "-o", str(obj)], cwd=build, check=True)
    link = [str(obj) if x.endswith("tests/search/search.c.o") else x for x in link]
    binary = work / "sim-search-reverse"
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)

ok = True
for scene in ("Search",) if args.reverse else ("Search", "SearchEmpty"):
    out = work / scene
    result = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
        "--script", str(ROOT / "tests/search/search.script"), "--budget", str(ROOT / "tests/ui_scene.budget.json"),
        "--assets", str(build / "assets"), "--content", str(build / "test-content"),
        "--flash-image", str(work / (scene + ".img")), "--sd-dir", str(work / (scene + "-sd")),
        "--out", str(out), "--quiet", "--no-random-outages"],
        env={**os.environ, "INK_SEARCH_SCENE": scene}, capture_output=True, text=True, timeout=180)
    (work / (scene + ".log")).write_text(result.stdout + result.stderr)
    if not (out / "metrics.json").exists():
        print(scene, "FAIL: no metrics", "exit", result.returncode, result.stdout[-3000:], result.stderr[-3000:], flush=True)
        ok = False
        break
    metrics = json.loads((out / "metrics.json").read_text())
    counts = metrics["metrics"]
    passed = result.returncode == 0 and counts.get("test.finished") == 1 and not counts.get("test.fail", 0)
    if args.reverse:
        expected = "child query cannot see owner contacts, memos or files"
        passed = result.returncode != 0 and counts.get("test.finished") == 1 and any(expected in w for w in metrics.get("warnings", []))
    print(scene, "PASS" if passed else "FAIL", "exit", result.returncode, "checks", counts.get("test.pass"), "failures", counts.get("test.fail", 0), flush=True)
    ok &= passed
    if not passed:
        print(result.stderr[-3000:])
        print(metrics.get("warnings"))
        break
    if not args.reverse:
        target = build / "search-compare" / scene
        target.mkdir(parents=True, exist_ok=True)
        for name in ("snap-scene.png", "metrics.json"):
            shutil.copyfile(out / name, target / name)
if ok and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(work)
else:
    print("Evidence:", work.relative_to(ROOT))
raise SystemExit(0 if ok else 1)
