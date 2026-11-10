"""Manual reverse check for accel-use: take each piece of the power service's rule out, find the check that catches it,
put it back and find the main phase passing. Usage: reverse.py BUILD_DIR (rebuilds sim-accel-use, about a minute a case)."""
from pathlib import Path
import os
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve()
source = root / "system/services/power/power_service.c"
original = source.read_text()
CASES = [  # recording and playing also hold audio focus: only both checks out together show there
    ("no motion wake", "        cfg.wake_mask |= HAL_WAKE_MOTION;", "        cfg.wake_mask |= 0;",
     "lifting it from the table to a reading angle wakes it"),
    ("setting ignored", "if (__atomic_load_n(&PS.lift, __ATOMIC_RELAXED) && PS.lift_misses", "if (PS.lift_misses",
     "off: lifting does not wake it"),
    ("no back-off", "PS.lift_misses < LIFT_MISSES && accel_z()", "accel_z()", "shaking flat in a bag"),
    ("watches when not flat", "accel_z() >= FLAT_Z_MG)\n        cfg.wake_mask", "accel_z() > -20000)\n        cfg.wake_mask",
     "not lying flat it does not watch"),
    ("one look only", "#define LIFT_LOOKS 4", "#define LIFT_LOOKS 1", "within the second after the movement"),
    ("no still lock", "if (still) PS.lock_req = true;", "if (0) PS.lock_req = true;", "lying flat and still it locks early"),
    ("still when held", "bool still = flat && ", "bool still = (flat || 1) && ", "held at a reading angle it keeps"),
    ("movement ignored", "moves == PS.still_moves && ", "", "a movement starts the still time over"),
    ("sound ignored", "quiet_audio = audio_finished() && ink_audio_focus_top() == 0;", "quiet_audio = true;",
     "recording: not locked early"),
    ("call ignored", " && ink_audio_focus_top() == 0;", ";", "in a call: not locked early"),
]


def rebuild():
    r = subprocess.run(["taskpolicy", "-c", "utility", "cmake", "--build", str(build), "--target", "sim-accel-use", "-j2"],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stdout[-3000:] + r.stderr[-3000:]


def run():
    return subprocess.run([sys.executable, str(root / "tests/accel_use/run.py"), "--build", str(build), "--phases", "main"],
                          env=dict(os.environ, INK_KEEP_EVIDENCE="1"), capture_output=True, text=True)


try:
    for name, old, new, expect in CASES:
        assert original.count(old) == 1, name
        source.write_text(original.replace(old, new))
        rebuild()
        r = run()
        caught = r.returncode != 0 and expect in r.stdout + r.stderr
        print(f"{name}: {'FAIL as required' if caught else 'NOT CAUGHT'} ({expect})", flush=True)
        assert caught, (r.stdout + r.stderr)[-4000:]
finally:
    source.write_text(original)
    rebuild()
r = run()
assert r.returncode == 0, (r.stdout + r.stderr)[-4000:]
print("restored: PASS")
