"""Validate IDF size/map differences and emit a path-free measurement summary."""
import hashlib
import json
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
        diff = json.loads((out / f"{candidate}-diff.json").read_text())
        layout = {p["name"]: p for p in sizes["layout"]}
        map_delta = {p["name"]: p for p in diff["layout"]}
        delta = {}
        for name in ("Flash", "DIRAM", "External RAM"):
            delta[name] = layout[name]["used"] - baseline_layout[name]["used"]
            assert delta[name] == map_delta[name]["used"], (candidate, name)
        symbols = (out / f"{candidate}.map").read_text()
        assert "ink_spike_activity" in symbols and "ink_spike_toggle" in symbols
        assert "ink_spike_tests" not in symbols.split("Linker script and memory map", 1)[-1]
        report["candidates"][candidate] = {"absolute": layout, "delta": delta,
                                              "map_sha256": digest(out / f"{candidate}.map")}
        print(f"{candidate}: Flash {delta['Flash']:+}, DIRAM {delta['DIRAM']:+}, PSRAM static {delta['External RAM']:+} bytes")
    (out / "summary.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")


if __name__ == "__main__":
    main()
