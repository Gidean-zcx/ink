#!/usr/bin/env python3
"""Compile bounded reverse controls without changing the checkout or needing git history."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
PORT = Path("ports/esp32s31")
SOURCES = [PORT / "hal_audio_s31.c", PORT / "hal_bt_s31.c",
           Path("drivers/chips/bh1098g/bh1098g.c")]
CONTROLS = [
    ("B02-write", "vendor/metalio_bh1098/mode.inc",
     "attempt <= kRetries", "attempt < 1"),
    ("B02-ack", "vendor/metalio_bh1098/mode.inc",
     "attempt <= kRetries", "attempt < 1"),
    ("B02-limit", "vendor/metalio_bh1098/mode.inc",
     "const int kRetries = 3;", "const int kRetries = 2;"),
    ("B04", "vendor/metalio_bh1098/response.inc",
     "if (B.mode == 2) ink_link(true);", "return;"),
    ("B05", str(PORT / "hal_bt_s31.c"),
     "if (bh1098g_set_mode(B.uart, mode)", "B.mode = 0;\n    if (bh1098g_set_mode(B.uart, mode)"),
    ("B05", str(PORT / "hal_bt_s31.c"),
     "if (lost && event) event(ctx, HAL_BT_EV_LOST);", "(void)event; (void)ctx; (void)lost;"),
    ("C19", str(PORT / "hal_bt_s31.c"),
     "!ink_vendor_init_mode(epoch)", "((void)ink_vendor_init_mode, B.mode = 1, false)"),
    (None, "vendor/metalio_bh1098/codec.inc",
     "std_cfg.slot_cfg.ws_width = I2S_DATA_BIT_WIDTH_32BIT;", "std_cfg.slot_cfg.ws_width = 16;"),
    (None, str(PORT / "hal_bt_s31.c"),
     "if (bh1098g_headset_call(B.uart))", "if (bh1098g_headset_call(B.uart) || ink_wait(\"SETUP SCO\", 1000))"),
]


def main():
    executable = Path(sys.argv[1]).resolve()
    for name in [None, "B02-write", "B02-ack", "B02-limit", "B04", "B05", "C19"]:
        subprocess.run([str(executable)] + ([name] if name else []), check=True)
    controls = CONTROLS
    if len(sys.argv) == 3:  # Manual historical comparison; ctest never takes this path.
        controls = [(name, None, sys.argv[2], None)
                    for name in ["B02-write", "B02-ack", "B02-limit", "B04", "B05", "C19"]]
    for index, (name, path, before, after) in enumerate(controls, 1):
        with tempfile.TemporaryDirectory(prefix="bh-reverse-", dir=executable.parent) as folder:
            scratch = Path(folder)
            for source in SOURCES:
                target = scratch / source
                target.parent.mkdir(parents=True, exist_ok=True)
                if path is None:
                    target.write_bytes(subprocess.check_output(
                        ["git", "show", f"{before}:{source}"], cwd=ROOT))
                else:
                    shutil.copyfile(ROOT / source, target)
            shutil.copytree(ROOT / "vendor/metalio_bh1098", scratch / "vendor/metalio_bh1098")
            if path is not None:
                target = scratch / path
                contents = target.read_text()
                assert before in contents, (path, before)
                target.write_text(contents.replace(before, after, 1))
            command = [os.environ.get("CC", "cc"), "-std=gnu11", "-O2", "-Wall", "-Wextra",
                       "-Werror", "-UNDEBUG", "-pthread"]
            for include in [PORT / "tests/audio_stub", PORT, "platform/include", "drivers/chips", "boards"]:
                command += ["-I", str(ROOT / include)]
            command += [str(ROOT / PORT / "tests/audio.c")]
            command += [str(scratch / source) for source in SOURCES]
            command += ["-o", str(scratch / "test")]
            compiled = subprocess.run(command, capture_output=True, text=True)
            assert compiled.returncode == 0, compiled.stderr
            result = subprocess.run([str(scratch / "test")] + ([name] if name else []),
                                    capture_output=True, text=True, timeout=10)
            assert result.returncode != 0, (name, "reverse control unexpectedly passed")
            assert "Assertion" in result.stderr or "Assertion" in result.stdout, result.stderr
            print(f"reverse {index}: {name or 'audio/SCO'} exit={result.returncode}; "
                  f"{result.stderr.strip().splitlines()[0]}")
    print(f"bh vendor: {len(controls)} reverse controls failed as expected; checkout unchanged")


if __name__ == "__main__":
    main()
