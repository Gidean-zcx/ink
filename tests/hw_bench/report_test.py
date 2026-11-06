#!/usr/bin/env python3
"""The report tool on a canned console: the numbers land in the right rows and nothing private gets through."""
import os, subprocess, sys, tempfile

here = os.path.dirname(os.path.abspath(__file__))
tool = os.path.join(here, "..", "..", "tools", "hw-bench", "report.py")
with tempfile.TemporaryDirectory() as d:
    out = os.path.join(d, "r.md")
    subprocess.check_call([sys.executable, tool, os.path.join(here, "sample_console.txt"), "-o", out, "--restored", "yes"])
    text = open(out, encoding="utf-8").read()
for want in ("跑完：是", "已复原并和备份核对：yes", "42.0 ms", "**低于底线，RK-04 成立**", "118 ms", "gray_ramp_gc16 | 1680 ms", "p16_changed_ms=-1"):
    assert want in text, want
for never in ("860123456789012", "00:11:22:33:44:55"):
    assert never not in text, never
print("ok")
