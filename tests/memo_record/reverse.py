"""Manual negative controls for the voice memo test: take one piece out, rebuild, the test must fail; then put it back.
Not registered in ctest (it edits the source tree for a moment). Run: reverse.py --build build [--only NAME]."""
import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
MUTANTS = {
    "cancel": ("system/services/audio/media_service.c", 'if (!strcmp(why, "cancelled")) ink_fs_remove_for', 'if (false) ink_fs_remove_for', "main"),
    "ai-key": ("system/services/ai/ai_service.c", "if (ink_audio_recording()) { /* a recording is not ended by asking", "if (false) { /* a recording is not ended by asking", "main"),
    "busy-say": ("system/services/audio/media_service.c", 'snprintf(c->rc.say, sizeof c->rc.say, "AI 正在听或在说', 'if (0) snprintf(c->rc.say, sizeof c->rc.say, "AI 正在听或在说', "main"),
    "words": ("system/services/audio/media_service.c", "        strcat(TX.text, part);", "        (void)part;", "main"),
    "member": ("system/apps/memo/memo.c", "const char *who = ink_cap_call_member(c);", 'const char *who = "owner";', "main"),
    "power-cut": ("system/apps/memo/memo.c", "while (db && audio && cursor != UINT32_MAX && !ink_audio_recording() && ink_fs_index", "while (false && ink_fs_index", "cut"),
}
parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--only")
args = parser.parse_args()
failed_as_expected = True
for name, (path, old, new, phase) in MUTANTS.items():
    if args.only and name != args.only:
        continue
    file = ROOT / path
    source = file.read_text()
    assert source.count(old) == 1, (name, source.count(old))
    try:
        file.write_text(source.replace(old, new))
        built = subprocess.run(["cmake", "--build", str(args.build), "-j4", "--target", "sim-memo-record"], capture_output=True, text=True)
        if built.returncode:
            print(name, "BUILD FAILED"); print(built.stdout[-1500:], built.stderr[-1500:]); failed_as_expected = False; continue
        run = subprocess.run([sys.executable, str(ROOT / "tests/memo_record/run.py"), "--build", str(args.build), "--phase", phase],
                             capture_output=True, text=True)
        caught = run.returncode != 0
        print(name, "caught" if caught else "NOT CAUGHT", "|", " ".join(l.split(") ", 1)[-1][:90] for l in run.stdout.splitlines() if "CHECK FAILED" in l)[:300])
        failed_as_expected &= caught
    finally:
        file.write_text(source)
subprocess.run(["cmake", "--build", str(args.build), "-j4", "--target", "sim-memo-record"], capture_output=True)
raise SystemExit(0 if failed_as_expected else 1)
