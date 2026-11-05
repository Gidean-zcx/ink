"""CTest adapter: missing Python dependencies are an explicit skip, never a pass."""
import argparse
import importlib.util
import os
import subprocess
import sys
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--caps", required=True)
parser.add_argument("--samples", required=True)
args = parser.parse_args()
Path(args.samples).unlink(missing_ok=True)
if any(importlib.util.find_spec(name) is None for name in ("pytest", "pytest_asyncio", "aiohttp", "yaml")):
    print("SKIP: install hub/.venv with uv sync --frozen; select INK_HUB_PYTHON in CMake")
    sys.exit(77)
root = Path(__file__).resolve().parents[2]
env = {**os.environ, "INK_HUB_CAPS": str(Path(args.caps).resolve()), "INK_HUB_SAMPLES": str(Path(args.samples).resolve())}
sys.exit(subprocess.run([sys.executable, "-m", "pytest", "-q"], cwd=root / "hub", env=env).returncode)
