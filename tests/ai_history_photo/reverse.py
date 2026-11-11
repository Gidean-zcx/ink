"""Manual reverse checks. Compile temporary source variants; leave the checkout untouched."""
import argparse
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
log = a.build / "verification/reverse"
log.mkdir(parents=True, exist_ok=True)
for name in ("write", "limit", "bytes", "order", "paging", "member", "clear", "delete",
             "replay", "offline", "photo", "photo-member", "photo-changed", "photo-id"):
    case = name if name.startswith("photo") else "replay" if name in ("replay", "offline") else "history"
    r = subprocess.run([os.sys.executable, "-B", str(root / "tests/ai_history_photo/run.py"),
                        "--build", str(a.build), "--mutant", name, "--case", case],
                       cwd=root, capture_output=True, text=True, timeout=180)
    (log / f"{name}.log").write_text(r.stdout + r.stderr)
    if r.returncode != 1 or "CHECK FAILED" not in r.stdout:
        raise SystemExit(f"{name}: expected an assertion failure, got {r.returncode}\n{r.stdout}\n{r.stderr[-2500:]}")
    print(name, "PASS: removing the fix causes an assertion failure", flush=True)
