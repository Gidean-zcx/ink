"""Writes the two 600-phase test scenes (240x320) used by run_v4_q.py: V.npy (video), X.npy (text/UI)."""
import numpy as np, scenes
np.save("V.npy", scenes.video_scene())
np.save("X.npy", scenes.text_scene())
