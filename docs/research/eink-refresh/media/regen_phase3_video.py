"""Archive copy of the phase-3 video: same pipeline as metalio_analysis/make_clip2.py, but the
scikit-image 'astronaut' photo (a real person) in the cover grid and wallpaper is replaced by data.moon()."""
import os, sys, numpy as np
OUT = sys.argv[1]
from skimage import data
_m = data.moon()
data.astronaut = lambda: np.stack([_m] * 3, -1)
src = open("/home/claude/metalio_analysis/make_clip2.py").read()
src = src.replace('"-crf", "24"', '"-crf", "30", "-movflags", "+faststart"')
src = src.replace('"/home/claude/metalio_analysis/metalio_algorithms_compare.mp4"', repr(os.path.join(OUT, "phase3_metalio_algorithms.mp4")))
src = src.replace('f"/tmp/claude-0/-home-claude/d162fb0d-4946-543b-9784-2d7efbacf79e/scratchpad/clip2_{k}.png"', 'os.path.join(%r, f"clip2_{k}.png")' % OUT)
assert "phase3_metalio_algorithms.mp4" in src and 'clip2_{k}.png")' in src
src = "import os\n" + src
exec(compile(src, "make_clip2_archive", "exec"), {"__name__": "__main__"})
print("done")
