#!/usr/bin/env python3
"""Summarize grayscale output differences; uses existing PNGs and never edits them."""
import json
from pathlib import Path
from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
rows = []
for name in ['reader_basic-metalio-plus-pqbn', 'ai_ask-metalio-plus-pqbn']:
    with Image.open(ROOT/'build-audit/before/test-out'/name/'final.png') as a:
        size = a.size
        old = list(a.convert('L').getdata())
    with Image.open(ROOT/'build-audit/test-out'/name/'final.png') as b:
        assert b.size == size
        new = list(b.convert('L').getdata())
    diff = [abs(x-y) for x, y in zip(old, new)]
    rows.append(dict(name=name, pixels=len(diff), changed=sum(x != 0 for x in diff),
                     mean_abs_diff=sum(diff)/len(diff), max_abs_diff=max(diff),
                     pixels_over_16=sum(x > 16 for x in diff)))
(HERE/'image-comparison.json').write_text(json.dumps(rows, indent=2)+'\n')
print(json.dumps(rows))
