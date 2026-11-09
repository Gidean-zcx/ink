"""Validate IDF size/map differences and emit a path-free measurement summary."""
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    out = Path(sys.argv[1])
    baseline = json.loads((out / "default.json").read_text())
    baseline_layout = {p["name"]: p for p in baseline["layout"]}
    report = {"evidence": "【实测】ESP32-S31 compile/link only; no device execution",
              "baseline_commit": "96887c78874e3e58945fb6aaf6f511911f739e88",
              "default_bin_sha256": digest(out / "default-before.bin"),
              "default_bin_bytes": (out / "default-before.bin").stat().st_size,
              "default_unchanged": True, "default": baseline_layout, "candidates": {}}
    for candidate in ("c", "json", "lua", "mquickjs"):
        sizes = json.loads((out / f"{candidate}.json").read_text())
        # IDF's json2 summary omits used_diff; raw preserves the map delta.
        idf_python = Path(os.environ["IDF_PYTHON_ENV_PATH"]) / "bin/python"
        raw = out / f"{candidate}-diff-raw.json"
        subprocess.run([str(idf_python), "-B", str(Path(os.environ["IDF_PATH"]) / "tools/idf_size.py"),
                        "--format", "raw", "--diff", str(out / "default-before.map"),
                        str(out / f"{candidate}.map"), "-o", str(raw)], check=True)
        diff = json.loads(raw.read_text())
        layout = {p["name"]: p for p in sizes["layout"]}
        map_delta = diff["memory_types"]
        delta = {}
        for name in ("Flash", "DIRAM", "External RAM"):
            delta[name] = layout[name]["used"] - baseline_layout[name]["used"]
            assert delta[name] == map_delta[name]["used_diff"], (candidate, name)
        symbols = (out / f"{candidate}.map").read_text()
        retained = symbols.split("Linker script and memory map", 1)[-1].split("Cross Reference Table", 1)[0]
        assert all(name in retained for name in ("ink_spike_activity", "ink_spike_toggle", "ink_spike_register"))
        assert "ink_spike_tests" not in retained
        report["candidates"][candidate] = {"absolute": layout, "delta": delta,
                                              "map_sha256": digest(out / f"{candidate}.map")}
        print(f"{candidate}: Flash {delta['Flash']:+}, DIRAM {delta['DIRAM']:+}, PSRAM static {delta['External RAM']:+} bytes")
    (out / "summary.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")


if __name__ == "__main__":
    main()
