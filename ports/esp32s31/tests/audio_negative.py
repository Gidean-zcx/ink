#!/usr/bin/env python3
"""Reverse audio controls in temporary source copies; no Git history needed."""
import argparse
import json
from pathlib import Path
import resource
import shlex
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    args = parser.parse_args()
    build = args.build.resolve()
    port = Path(__file__).resolve().parents[1]
    commands = json.loads((build / "compile_commands.json").read_text())
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    changes = [
        ("baseline", "", ""),
        ("remove-implementation", "    ink_s31_audio_init();", "    ink_s31_audio_init();"),
        ("enable-pa-at-start", "A.playing = true; A.tail = 0;", "A.playing = true; ink_amp(true); A.tail = 0;"),
        ("omit-pa-off", "        ink_amp(false);", "        (void)0;"),
        ("omit-drain", "if (drain && left > 0)", "if (drain && left < 0)"),
        ("omit-dma-clear", "    for (int i = 0; i < INK_AUDIO_DMA; i++) {", "    for (int i = 0; i < 0; i++) {"),
        ("accept-no-mode-ack", " || ink_wait(ack, 1000)", ""),
        ("stale-mode-after-wake", "if (B.epoch != epoch || !B.uart || !B.mode)", "if (B.epoch == UINT_MAX || !B.uart || !B.mode)"),
        ("drop-reference-channel", "int32_t value = bit32_buffer[(size_t)i] >> 12;",
         "int32_t value = i % 2 ? 0 : bit32_buffer[(size_t)i] >> 12;"),
        ("ignore-capture-overflow", "__atomic_add_fetch(&A.lost, ev->size / 8, __ATOMIC_RELAXED);", "(void)ev;"),
        ("master-clock-default", "std_cfg.clk_cfg.sample_rate_hz = HAL_AUDIO_RATE;",
         "std_cfg.clk_cfg.sample_rate_hz = HAL_AUDIO_RATE;\n    std_cfg.clk_cfg.bclk_div = 8;"),
        ("opus-baseline", "", ""),
        ("opus-no-dtx", "OPUS_SET_DTX(1)", "OPUS_SET_DTX(0)"),
        ("opus-wrong-application", "OPUS_APPLICATION_VOIP", "OPUS_APPLICATION_AUDIO"),
        ("opus-short-frame", "HAL_OPUS_FRAME_SAMPLES, out, cap", "HAL_OPUS_FRAME_SAMPLES / 3, out, cap"),
        ("opus-no-plc", "(!pkt && len)", "!pkt"),
    ]
    with tempfile.TemporaryDirectory(prefix="audio-negative-", dir=build) as temp:
        work = Path(temp)
        for name, old, new in changes:
            codec = name.startswith("opus-")
            source = port / ("hal_opus_s31.c" if codec else "hal_bt_s31.c" if name in
                             ("accept-no-mode-ack", "stale-mode-after-wake") else "hal_audio_s31.c")
            entry = next(e for e in commands if Path(e["file"]).resolve() == source)
            compile_args = shlex.split(entry["command"])
            text = source.read_text()
            if name in ("drop-reference-channel", "master-clock-default"):
                included = port.parents[1] / "vendor/metalio_bh1098/codec.inc"
                text = text.replace('#include "../../vendor/metalio_bh1098/codec.inc"', included.read_text())
            if name == "remove-implementation":
                # Link the actual baseline-shaped audio stubs instead of the port.
                text = '#include "ink/hal_audio.h"\n' + "\n".join([
                    "void ink_s31_audio_init(void) {}",
                    "int hal_audio_route_set(hal_audio_route_t r) { (void)r; return -1; }",
                    "void hal_audio_get_info(hal_audio_info_t *i) { *i = (hal_audio_info_t){0}; }",
                    "int hal_audio_play_start(void) { return -1; }",
                    "int hal_audio_play_write(const int16_t *p, int n, int64_t t) { (void)p; (void)n; (void)t; return -1; }",
                    "int64_t hal_audio_play_delay_us(void) { return -1; }",
                    "void hal_audio_play_stop(bool d) { (void)d; }",
                    "int hal_audio_capture_start(hal_audio_capture_cb cb, void *c) { (void)cb; (void)c; return -1; }",
                    "void hal_audio_capture_stop(void) {}",
                ])
            elif old:
                assert text.count(old) == 1, name
                text = text.replace(old, new)
            altered = work / "audio.c"
            altered.write_text(text)
            obj = work / "audio.o"
            command = compile_args.copy()
            command[command.index("-o") + 1] = str(obj)
            command[command.index(str(source))] = str(altered)
            subprocess.run(command, cwd=build, check=True, capture_output=True, text=True)
            target = "test-s31-opus" if codec else "test-s31-audio"
            link = shlex.split((build / f"CMakeFiles/{target}.dir/link.txt").read_text())
            for i, arg in enumerate(link):
                if arg.endswith(source.name + ".o"):
                    link[i] = str(obj)
            link[link.index("-o") + 1] = str(work / "test")
            subprocess.run(link, cwd=build, check=True, capture_output=True, text=True)
            run = subprocess.run([str(work / "test")], cwd=work, capture_output=True, text=True, timeout=10)
            assert (run.returncode == 0) == (name in ("baseline", "opus-baseline")), (name, run.stdout, run.stderr)
            evidence = run.stderr.strip().splitlines()
            print(f"{name}: compile=0, test={run.returncode}" + (f"; {evidence[-1]}" if evidence else ""))


if __name__ == "__main__":
    main()
