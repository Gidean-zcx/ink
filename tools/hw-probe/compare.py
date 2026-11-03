#!/usr/bin/env python3
"""Set the probe's measured cycles against the instruction-set simulator.

Reads the "HWPROBE region <name> units=U aux=A cycles=C" lines of a probe console log and, per
region, prints device cycles per unit next to the ISS fitted value, its slow and ideal corners
(tools/iss/out/costs.json) and the device/fit ratio. The feed kernel (k_feed.c) is a standalone
ISS program; its reference values are the table in docs/research/simulator/iss-calibration.md 2.6.

  compare.py ink-hw/phase1/probe_console.txt
"""

import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
COSTS = os.path.join(HERE, "..", "iss", "out", "costs.json")
FEED = {  # cycles per line: fit, slow, ideal (iss-calibration.md 2.6)
    "feed.upstream": (10680, 15451, None),
    "feed.t4": (9253, 13662, None),
    "feed.skip": (116, 196, None),
}
LINE = re.compile(r"HWPROBE region (\S+) units=(\d+) aux=(\d+) cycles=(\d+)")


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    regions = json.load(open(COSTS))["regions"]
    rows, other = [], []
    for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
        m = LINE.search(line)
        if m:
            name, units, cycles = m.group(1), int(m.group(2)), int(m.group(4))
            per = cycles / units if units else float("nan")
            if name in FEED:
                fit, slow, ideal = FEED[name]
            elif name in regions:
                r = regions[name]
                fit, slow, ideal = r.get("cycles_per_unit"), r.get("slow"), r.get("ideal")
            else:
                fit = slow = ideal = None
            rows.append((name, units, per, fit, slow, ideal))
        elif "HWPROBE" in line and "region" not in line:
            other.append(line.split("HWPROBE", 1)[1].strip())
    for o in other:
        print("#", o)
    print(f"\n| region | units | device cyc/unit | ISS fit | device/fit | ISS slow | ISS ideal |")
    print("|---|---|---|---|---|---|---|")
    for name, units, per, fit, slow, ideal in rows:
        ratio = f"{per / fit:.2f}" if fit else "-"
        f = lambda v: f"{v:.3f}" if isinstance(v, (int, float)) and v is not None else "-"  # noqa: E731
        print(f"| {name} | {units} | {per:.3f} | {f(fit)} | {ratio} | {f(slow)} | {f(ideal)} |")


if __name__ == "__main__":
    main()
