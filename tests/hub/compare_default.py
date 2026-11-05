"""Compare bridge-disabled output with an executable built from the task baseline."""
import argparse
import json
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--before", required=True)
parser.add_argument("--after", required=True)
parser.add_argument("--assets", required=True)
parser.add_argument("--content", required=True)
parser.add_argument("--out", required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
for scenario in ("home_basic", "reader_basic", "ai_ask"):
    outputs = []
    for name, executable in (("before", args.before), ("after", args.after)):
        output = Path(args.out) / scenario / name
        command = [executable, "--profile", str(root / "simulator/profiles/metalio-plus.json"),
                   "--script", str(root / f"tests/scenarios/{scenario}.script"), "--assets", args.assets,
                   "--content", args.content, "--out", str(output), "--quiet"]
        run = subprocess.run(command, capture_output=True, text=True)
        assert run.returncode == 0, run.stderr[-2000:]
        outputs.append(output)
    for filename in ("metrics.json", "final.png"):
        left, right = (p / filename for p in outputs)
        if left.read_bytes() != right.read_bytes():
            if filename == "metrics.json":
                a, b = json.loads(left.read_text()), json.loads(right.read_text())
                print("changed metrics:", {k: (a["metrics"].get(k), b["metrics"].get(k)) for k in a["metrics"] | b["metrics"]
                                          if a["metrics"].get(k) != b["metrics"].get(k)})
            raise AssertionError(f"{scenario}: {filename} changed")
    print(f"{scenario}: metrics.json and final.png are byte-identical to baseline")
