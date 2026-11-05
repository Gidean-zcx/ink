#!/usr/bin/env python3
"""Compare the simulated physical panel before and after interruptions, byte for byte."""
import sys
from pathlib import Path
from PIL import Image, ImageChops

folder = Path(sys.argv[1])
for a, b in [('before-call', 'after-call'), ('before-confirm-call', 'after-confirm-call')]:
    diff = ImageChops.difference(Image.open(folder / f'snap-{a}.png'), Image.open(folder / f'snap-{b}.png'))
    if diff.getbbox(): raise SystemExit(f'panel mismatch: {a} -> {b}: {diff.getbbox()}')
    print(f'physical panel identical: {a} -> {b}')
