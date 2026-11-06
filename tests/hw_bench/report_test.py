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
# The second copy counts, whole, and only once.
with tempfile.TemporaryDirectory() as d:
    console = os.path.join(d, "console.txt")
    with open(console, "w", encoding="utf-8") as f:
        f.write('BENCH {"test":"chip","name":"first_copy_with_holes"}\nBENCH_REPLAY\n')
        f.write('BENCH {"test":"chip","name":"whole_copy"}\nBENCH_DONE\n')
    out = os.path.join(d, "r.md")
    subprocess.check_call([sys.executable, tool, console, "-o", out, "--restored", "yes"])
    text = open(out, encoding="utf-8").read()
assert "whole_copy" in text and "first_copy_with_holes" not in text and "跑完：是" in text
print("ok")
