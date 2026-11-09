"""Reproducibility and full-product quality, with a missing-file negative control."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, required=True)
args = parser.parse_args()
build = args.build.resolve()
work = Path(tempfile.mkdtemp(prefix="ime-lexicon-", dir=build))
corpus = [line.split("\t") for line in Path(__file__).with_name("cases.tsv").read_text().splitlines()
          if line and not line.startswith("#")]
shipping = build / "test-content/.system/ime/base.ikd"
subprocess.run([sys.executable, str(ROOT / "tools/imedict/build_base.py"),
                "--cache", str(ROOT / "build-deps/imedict"), "--out", str(work / "repeat.ikd")], check=True)
assert shipping.read_bytes() == (work / "repeat.ikd").read_bytes(), "same sources must produce identical bytes"
assert shipping.stat().st_size <= 1024 * 1024
empty = work / "empty-content"
empty.mkdir()
summary = {"cases": len(corpus), "bytes": shipping.stat().st_size,
           "sha256": hashlib.sha256(shipping.read_bytes()).hexdigest(), "reproducible": True}
for phase in ("dictionary", "fallback", "reverse"):
    output = work / phase
    result = subprocess.run([str(build / "sim-ime-lexicon"),
                             "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
                             "--assets", str(build / "assets"),
                             "--content", str(build / "test-content" if phase == "dictionary" else empty),
                             "--out", str(output), "--end", "60s", "--quiet", "--no-random-outages",
                             "--budget", str(ROOT / "tests/integration/textinput.budget.json")],
                            env={**os.environ, **({"INK_IME_FALLBACK": "1"} if phase == "fallback" else {}),
                                 "UBSAN_OPTIONS": "halt_on_error=1"},
                            cwd=ROOT, capture_output=True, text=True, timeout=120)
    (work / (phase + ".log")).write_text(result.stdout + result.stderr)
    report = json.loads((output / "metrics.json").read_text())
    metrics = report["metrics"]
    if phase == "reverse":
        assert result.returncode != 0 and metrics.get("test.fail", 0) > 0, "removing dictionary must fail acceptance"
        summary[phase] = {"exit": result.returncode, "test_fail": metrics["test.fail"]}
        continue
    assert result.returncode == 0 and metrics.get("test.finished") == 1 and metrics.get("test.fail", 0) == 0, report.get("warnings")
    rows = [line.split("\t")[1:] for line in result.stdout.splitlines() if line.startswith("IME\t")]
    assert len(rows) == len(corpus), "every fixed query must run"
    first = five = 0
    details = []
    for i, (key, target, category) in enumerate(corpus):
        assert int(rows[i][0]) == i
        choices = [item.rsplit(":", 1) for item in rows[i][1:]]
        matches = [word == target and int(consumed) == len(key) for word, consumed in choices]
        first += bool(matches and matches[0])
        five += any(matches)
        details.append({"key": key, "target": target, "category": category,
                        "candidates": choices, "first": bool(matches and matches[0]), "five": any(matches)})
    assert first == metrics.get("ime.first", 0) and five == metrics.get("ime.five", 0)
    if phase == "dictionary":
        assert first >= 75 and five >= 83, "fixed quality corpus regressed"
    groups = {}
    for name, selected in (("full", [r for r in details if not r["category"].startswith("简拼") and r["category"] != "未收录短句"]),
                           ("initials", [r for r in details if r["category"].startswith("简拼")])):
        groups[name] = {"cases": len(selected), "first": sum(r["first"] for r in selected),
                        "five": sum(r["five"] for r in selected)}
    if phase == "dictionary":
        assert groups["full"]["first"] >= 71 and groups["full"]["five"] >= 73, groups
        assert groups["initials"]["first"] >= 10 and groups["initials"]["five"] >= 14, groups
    summary[phase] = {"first": first, "five": five, "first_percent": round(first * 100 / len(corpus), 2),
                      "five_percent": round(five * 100 / len(corpus), 2),
                      "load_ms": metrics["ime.load_ms"], "external_bytes": metrics["ime.external_bytes"], "groups": groups}
    (work / (phase + "-candidates.json")).write_text(json.dumps(details, ensure_ascii=False, indent=2) + "\n")
assert summary["dictionary"]["first"] > summary["fallback"]["first"]
assert summary["dictionary"]["five"] > summary["fallback"]["five"]
assert summary["dictionary"]["external_bytes"]["max"] == shipping.stat().st_size
assert summary["fallback"]["external_bytes"]["max"] == 0
(work / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
print(json.dumps(summary, ensure_ascii=False, indent=2))
if os.environ.get("INK_KEEP_EVIDENCE"):
    print("evidence:", work)
else:
    shutil.rmtree(work)
