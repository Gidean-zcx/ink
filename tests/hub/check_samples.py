import subprocess
import sys
from pathlib import Path

if not Path(sys.argv[2]).exists():
    print("SKIP: generated hub messages missing; run hub-contract with its locked dependencies")
    sys.exit(77)
sys.exit(subprocess.run(sys.argv[1:]).returncode)
