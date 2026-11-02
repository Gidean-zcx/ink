"""Archive copy of the phase-1 video: same pipeline as eink_sim/make_video.py, but the
scikit-image 'astronaut' photo (a real person) in scene 1 is replaced by data.moon()."""
import os, sys, numpy as np
OUT = sys.argv[1]
sys.path.insert(0, "/home/claude/eink_sim")
from skimage import data
_m = data.moon()
data.astronaut = lambda: np.stack([_m] * 3, -1)
import scenes
os.chdir(OUT)
np.save("V.npy", scenes.video_scene())
for k in ("X", "G"):
    if not os.path.exists(k + ".npy"):
        os.symlink(f"/home/claude/eink_sim/{k}.npy", k + ".npy")
src = open("/home/claude/eink_sim/make_video.py").read()
src = src.replace('"-crf", "16"', '"-crf", "30", "-movflags", "+faststart"')
src = src.split('if __name__ == "__main__":')[0]
g = {"__name__": "mv"}
exec(compile(src, "make_video_archive", "exec"), g)
g["main"](out=os.path.join(OUT, "phase1_compare.mp4"))
print("done")
