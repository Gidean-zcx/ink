"""Exercise remote pages, retain visual evidence only when requested."""
import argparse
import json
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
p.add_argument("--mutant", choices=("stream", "loading", "page", "long", "blocked", "timeout", "child"))
a = p.parse_args()
build = a.build.resolve()
work = Path(tempfile.mkdtemp(prefix="remote-app-", dir=build))
binary = build / "sim-remote-app"
if a.mutant:
    source = (root / "system/apps/remote/remote.c").read_text()
    replacements = {
        "loading": ("page_show(LOADING);", "page_show(REMOTE);"),
        "page": ('uint8_t input[2] = {2, !strcmp(id, "remote.next")};', 'uint8_t input[2] = {1, !strcmp(id, "remote.next")};'),
        "long": ("held ? 3 : 1", "1"),
        "blocked": ("page_show(kind == 3 ? BLOCKED : TYPE);", "page_show(kind == 3 ? REMOTE : TYPE);"),
        "timeout": ("300000000ull", "600000000ull"),
        "child": ("if (ink_member_current()->role == INK_ROLE_CHILD)", "if (false)"),
    }
    if a.mutant == "stream":
        source = source.replace("static void page_show(remote_page_t page) {", """static void stream_edit(lv_event_t *e) {
    (void)e;
    uint8_t payload[256] = {4};
    size_t len = strlen(ink_text_value());
    memcpy(payload + 1, ink_text_value(), len);
    ink_rt_send(RS.sid, INK_RT_INPUT, payload, len + 1, 0);
}
static void page_show(remote_page_t page) {""")
        source = source.replace("            ink_textfield_bind_short(RS.field,", "            lv_obj_add_event_cb(RS.field, stream_edit, LV_EVENT_VALUE_CHANGED, NULL);\n            ink_textfield_bind_short(RS.field,")
    else:
        old, new = replacements[a.mutant]
        assert old in source
        source = source.replace(old, new)
    if a.mutant == "child": source = source.replace(".child_ok = false,", ".child_ok = true,")
    product = work / "remote-product.c"
    product.write_text(source)
    driver = work / "remote-test.c"
    driver.write_text((root / "tests/remote_app/remote_app.c").read_text().replace(
        '#include "../../system/apps/remote/remote.c"', '#include "remote-product.c"').replace(
        '#include "../../system/boot/main.c"', f'#include "{root}/system/boot/main.c"').replace(
        '#include "../integration/whole_product.h"', f'#include "{root}/tests/integration/whole_product.h"'))
    flags = {}
    for line in (build / "CMakeFiles/sim-remote-app.dir/flags.make").read_text().splitlines():
        if " = " in line:
            key, value = line.split(" = ", 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / "CMakeFiles/sim-remote-app.dir/link.txt").read_text())
    obj = work / "remote-test.o"
    subprocess.run([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"], "-c", str(driver), "-o", str(obj)], cwd=build, check=True)
    link = [str(obj) if word.endswith("tests/remote_app/remote_app.c.o") else word for word in link]
    binary = work / "sim-remote-app"
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
result = subprocess.run([str(binary), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
    "--script", str(root / "tests/remote_app/remote.script"), "--budget", str(root / "tests/ui_scene.budget.json"),
    "--assets", str(build / "assets"), "--content", str(build / "test-content"), "--out", str(work),
    "--quiet", "--no-random-outages"], capture_output=True, text=True, timeout=160)
(work / "run.log").write_text(result.stdout + result.stderr)
data = json.loads((work / "metrics.json").read_text()) if (work / "metrics.json").exists() else {}
counts = data.get("metrics", {})
ok = result.returncode == 0 and counts.get("test.finished") == 1 and not counts.get("test.fail", 0)
if a.mutant:
    expected = {"stream": "no text leaves device while typing", "loading": "slow first screen shows Loading",
                "page": "both physical paging keys reach hub", "long": "long press reaches hub",
                "blocked": "hub block ends rendering session", "timeout": "five minute limit closes old session",
                "child": "child direct call denied"}[a.mutant]
    assert not ok and any(expected in str(w) for w in data.get("warnings", [])), "intended negative control was not detected"
    print(f"PASS: negative {a.mutant} detected, {counts['test.fail']} failed assertions")
    if not os.environ.get("INK_KEEP_EVIDENCE"): shutil.rmtree(work)
    raise SystemExit(0)
if not ok:
    print(result.stdout[-1200:], result.stderr[-1800:])
    for warning in data.get("warnings", []): print(warning)
    raise SystemExit(f"FAIL: exit={result.returncode}, failures={counts.get('test.fail')}, evidence={work}")
for board in ("Remote", "RemoteLoading", "RemoteOffline", "RemoteAddress", "RemoteType", "RemoteBlocked"):
    snap = json.loads((work / f"{board}.json").read_text())
    if "keyboard" in snap: snap = json.loads(snap["page"])
    assert snap["page"] == "remote" and snap["actions"] and snap["page_version"] > 0
    assert any(f["value"] == board for f in snap["fields"])
    assert counts[f"test.snapshot.{board}"] == 1
    target = build / "remote-compare" / board
    target.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(work / f"snap-{board}.png", target / "snap-scene.png")
    (target / "metrics.json").write_text(json.dumps({"metrics": {"test.finished": 1, "test.fail": 0,
        "test.snapshot.settled": 1, f"test.snapshot.{board}": 1}}))
print(f"PASS: {counts['test.pass']} assertions, six snapshots, hub inputs, virtual recovery window")
if not os.environ.get("INK_KEEP_EVIDENCE"): shutil.rmtree(work)
