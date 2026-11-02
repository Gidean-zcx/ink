#!/usr/bin/env python3
"""Synthetic long book for pagination/performance tests: repeats an original
story N times with numbered parts. Test data only (not literature)."""
import sys

src, out, n = sys.argv[1], sys.argv[2], int(sys.argv[3])
text = open(src, encoding="utf-8").read()
with open(out, "w", encoding="utf-8") as f:
    for i in range(1, n + 1):
        f.write(f"【测试长书 第{i}部分】\n")
        f.write(text)
        f.write("\n")
