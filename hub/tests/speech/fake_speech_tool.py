"""Executable fixtures only: synthetic text, tone WAVs, and controlled failures."""
import json
import math
import os
import signal
import stat
import struct
import sys
import wave
from pathlib import Path

TRANSCRIPT = "合成测试识别文字"
SPOKEN = "合成测试朗读内容。"
SAY_SAMPLES = 2111

kind = "say" if Path(sys.argv[0]).name == "say" else "whisper"
args = sys.argv[1:]
wav = Path(args[args.index("-o" if kind == "say" else "-f") + 1])
assert stat.S_IMODE(wav.parent.stat().st_mode) == 0o700
event = {"kind": kind, "pid": os.getpid(), "private": True}
if kind == "whisper":
    assert args[args.index("-l") + 1] == "zh"
    assert "简体中文" in args[args.index("--prompt") + 1]
    assert "-nt" in args and "-np" in args and "-otxt" in args
    with wave.open(str(wav), "rb") as audio:
        assert (audio.getframerate(), audio.getnchannels(), audio.getsampwidth()) == (16000, 1, 2)
        assert audio.getnframes() == int(os.environ.get("INK_TEST_INPUT_FRAMES", "25")) * 960
        event["samples"] = audio.getnframes()
else:
    text = sys.stdin.read()
    assert text and text not in args
    assert "--file-format=WAVE" in args and "--data-format=LEI16@16000" in args and "--channels=1" in args
    assert args[args.index("-v") + 1] == os.environ.get("INK_TEST_VOICE", "Tingting")
    event["text"] = text
with open(os.environ["INK_TEST_CALLS"], "a", encoding="utf-8") as output:
    output.write(json.dumps(event, ensure_ascii=False) + "\n")
mode = os.environ.get("INK_TEST_" + kind.upper() + "_MODE", "ok")
if mode == "timeout":
    signal.pause()
if mode in ("fail", "forbidden"):
    print(TRANSCRIPT, file=sys.stderr)
    sys.exit(17)
print(TRANSCRIPT if kind == "whisper" else text)
if kind == "whisper":
    Path(args[args.index("-of") + 1] + ".txt").write_text(TRANSCRIPT + "\n", encoding="utf-8")
else:
    pcm = struct.pack("<" + "h" * SAY_SAMPLES,
                      *(int(9000 * math.sin(2 * math.pi * 440 * n / 16000)) for n in range(SAY_SAMPLES)))
    with wave.open(str(wav), "wb") as audio:
        audio.setparams((1, 2, 16000, 0, "NONE", "not compressed"))
        audio.writeframes(pcm)
