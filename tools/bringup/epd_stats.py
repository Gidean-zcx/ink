#!/usr/bin/env python3
"""Refresh timing from factory-firmware console logs.

Pairs each "EpdTiming: phase cadence us: avg=.. min=.. max=.. frames=.."
line with the "EpdRefresh: mode=.. full=.. cost=..ms" line that follows it
and summarises phase length and total refresh time per (mode, full, frames).

  epd_stats.py private/bootlog_1.txt private/runtime_1.txt
"""

import collections
import re
import statistics
import sys

TIMING = re.compile(r"EpdTiming: phase cadence us: avg=(\d+) min=(\d+) max=(\d+) "
                    r"frames=(\d+) completed=(\d+) cycle=(\d+)ms")
REFRESH = re.compile(r"EpdRefresh: mode=(\d+) full=(\d+) clean=(\d+) cost=(\d+)ms err=(\S+)")


def main():
    groups = collections.defaultdict(list)
    for path in sys.argv[1:]:
        timing = None
        for line in open(path, encoding="utf-8", errors="replace"):
            if m := TIMING.search(line):
                timing = [int(x) for x in m.groups()]
            elif (m := REFRESH.search(line)) and timing:
                mode, full, clean, cost = (int(x) for x in m.groups()[:4])
                avg, lo, hi, frames, done, cycle = timing
                groups[(mode, full, frames)].append((avg, lo, hi, cycle, cost, done == frames))
                timing = None
    print(f"{'mode':>4} {'full':>4} {'frames':>6} {'n':>3}  {'phase avg us (min..max of avg)':<30}"
          f" {'worst phase us':>14}  {'cycle ms':<12} {'cost ms (median, range)':<24}")
    for (mode, full, frames), rows in sorted(groups.items()):
        avgs = [r[0] for r in rows]
        costs = [r[4] for r in rows]
        cycles = [r[3] for r in rows]
        print(f"{mode:>4} {full:>4} {frames:>6} {len(rows):>3}  "
              f"{statistics.median(avgs):>6.0f} ({min(avgs)}..{max(avgs)}){'':<10}"
              f" {max(r[2] for r in rows):>14}  {min(cycles)}..{max(cycles):<7}"
              f" {statistics.median(costs):>4.0f} ({min(costs)}..{max(costs)})"
              + ("" if all(r[5] for r in rows) else "  INCOMPLETE"))


if __name__ == "__main__":
    main()
