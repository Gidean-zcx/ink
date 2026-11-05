import subprocess
import sys
import json
import tempfile
from pathlib import Path

if not Path(sys.argv[2]).exists():
    print("SKIP: generated hub messages missing; run hub-contract with its locked dependencies")
    sys.exit(77)
result = subprocess.run(sys.argv[1:])
if result.returncode:
    sys.exit(result.returncode)
with tempfile.TemporaryDirectory(dir=Path(sys.argv[2]).parent) as directory:
    path = Path(directory) / "boundary.jsonl"
    path.write_text(json.dumps({"x": [0] * 125}) + "\n")
    if subprocess.run([sys.argv[1], str(path)]).returncode:
        sys.exit(1)
    path.write_text(json.dumps({"x": [0] * 126}) + "\n")
    negative = subprocess.run([sys.argv[1], str(path)], capture_output=True)
    if negative.returncode != 1:
        sys.exit(1)
print("production parser: 128-token boundary accepted; 129-token counterexample rejected")
