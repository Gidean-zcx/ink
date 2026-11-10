#!/usr/bin/env python3
"""Manual SDK reverse controls; use a local read-only SDK, never a device."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("idf", type=Path)
parser.add_argument("build", type=Path)
args = parser.parse_args()
args.build.mkdir(parents=True, exist_ok=True)
guard = Path(__file__).resolve().parents[1] / "s31_sleep_sdk_check.cmake"
pmu = Path("components/esp_hw_support/port/esp32s31/pmu_sleep.c")
version = Path("tools/cmake/version.cmake")
with tempfile.TemporaryDirectory(prefix="sdk-check-", dir=args.build.resolve()) as temp:
    sdk = Path(temp)
    for path in (pmu, version):
        (sdk / path).parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(args.idf / path, sdk / path)
    env = {**os.environ, "IDF_PATH": str(sdk)}
    for name in ("baseline", "wrong-version", "wrong-anchor"):
        if name == "wrong-version":
            (sdk / version).write_text("set(IDF_VERSION_MAJOR 7)\nset(IDF_VERSION_MINOR 0)\nset(IDF_VERSION_PATCH 0)\n")
        elif name == "wrong-anchor":
            shutil.copyfile(args.idf / version, sdk / version)
            text = (sdk / pmu).read_text()
            anchor = "s_mpll_freq_mhz_before_sleep = rtc_clk_mpll_get_freq();"
            assert text.count(anchor) == 1
            (sdk / pmu).write_text(text.replace(anchor, "/* removed clock anchor */"))
        result = subprocess.run(["cmake", "-P", str(guard)], env=env, capture_output=True, text=True)
        assert (result.returncode == 0) == (name == "baseline"), result.stderr
        if name != "baseline":
            expected = "unsupported IDF version" if name == "wrong-version" else "PMU source/anchors changed"
            assert expected in result.stderr, result.stderr
        print(f"{name}: cmake={result.returncode}")
