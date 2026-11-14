"""Runs one of Mogo's tool scripts unchanged, as on the Mac Mogo wrote it on: launch.py <script> [args].

- Pillow lays text out without raqm. The clock digits header was made that way: FreeType's hinted advance of
  Chakra Petch's widest digit at 224 px is 145 (CLOCK_DIGIT_CELL); a Pillow with raqm (Ubuntu's) takes HarfBuzz's
  unhinted 146.048, so generate_clock_digits.py stops at `assert advance == 145`.
- lock_collage_host_test.py keeps its files in macOS's short /private/tmp; where there is none, the system's
  temporary folder (/tmp on Linux, just as short)."""
import runpy
import sys
import tempfile
from pathlib import Path

try:
    from PIL import ImageFont
except ImportError:  # the tests that draw no text run without Pillow
    ImageFont = None
if ImageFont is not None:
    truetype = ImageFont.truetype
    ImageFont.truetype = lambda *a, **k: truetype(*a, **{'layout_engine': ImageFont.Layout.BASIC, **k})
if not Path('/private/tmp').is_dir():
    mkdtemp = tempfile.mkdtemp
    tempfile.mkdtemp = lambda suffix=None, prefix=None, dir=None: mkdtemp(
        suffix, prefix, None if dir == '/private/tmp' else dir)
sys.argv = sys.argv[1:]
runpy.run_path(sys.argv[0], run_name='__main__')
