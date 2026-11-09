"""Cancellation restores the settled caller frame byte for byte."""
import sys
from pathlib import Path
from PIL import Image, ImageChops

root = Path(sys.argv[1])
before = Image.open(root / "snap-caller-before.png").convert("L")
after = Image.open(root / "snap-caller-after.png").convert("L")
assert before.size == after.size
assert ImageChops.difference(before, after).getbbox() is None, "cancel changed the caller frame"
print("PASS: cancel restores every caller pixel")
