#!/usr/bin/env python3
"""Run boot/sleep/panic reverse controls on temporary source copies, without Git history."""
import argparse
from pathlib import Path
import resource
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    args = parser.parse_args()
    args.build.mkdir(parents=True, exist_ok=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    tests = Path(__file__).resolve().parent
    port, root = tests.parent, tests.parents[2]
    common = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-UNDEBUG"]
    for directory in [port, root / "platform/include", root / "drivers/chips", root / "boards"]:
        common += ["-I", str(directory)]
    mutations = [
        ("threshold", "boot", "if (failures >= 3)", "if (failures >= 4)"),
        ("checksum", "boot", "ink_boot.check == ~(ink_boot.magic ^ ink_boot.failures ^ ink_boot.armed)", "true"),
        ("healthy-clear", "boot", "ink_boot_save(0, false);", "ink_boot_save(1, true);"),
        ("cold-reset", "boot", "fault && valid ?", "(fault || why == ESP_RST_POWERON) && valid ?"),
        ("sleep-gate", "awake", "#if INK_S31_LIGHT_SLEEP", "#if 1"),
        ("shutdown-stop", "awake", "    ink_s31_display_shutdown();", ""),
        ("panic-gate", "panic", "gpio_ll_set_level(&GPIO, xoe, 0);", "gpio_ll_set_level(&GPIO, xoe, 1);"),
        ("panic-rails", "panic", "ink_panic_write(port0 & ~7u);", "ink_panic_write((port0 & ~7u) | 4u);"),
        ("modem-stub", "stubs", "osal_task_delay_us((uint64_t)timeout_ms * 1000); return 0;", "return -1;"),
    ]
    with tempfile.TemporaryDirectory(prefix="boot-negative-", dir=args.build.resolve()) as temporary:
        work = Path(temporary)
        for name, kind, old, new in [("baseline-" + k, k, "", "") for k in ["boot", "awake", "panic", "stubs"]] + mutations:
            original = port / {"boot": "s31_boot.c", "awake": "hal_power_s31.c", "panic": "s31_panic.c", "stubs": "hal_stubs.c"}[kind]
            source = original.read_text()
            if old:
                assert old in source, name
                source = source.replace(old, new)
            target = work / original.name
            target.write_text(source)
            command = common.copy()
            if kind == "boot":
                command += ["-I", str(tests / "boot_stub"), f'-DINK_BOOT_SOURCE="{target}"',
                            str(tests / "boot.c"), str(port / "hal_device_s31.c"), str(port / "main/main.c")]
            elif kind == "awake":
                command += ["-I", str(tests / "stub"), "-DINK_S31_LIGHT_SLEEP=0",
                            str(tests / "power.c"), str(target)]
            elif kind == "panic":
                command += ["-I", str(tests / "panic_stub"), "-I", str(tests / "boot_stub"),
                            str(tests / "panic.c"), str(target)]
            else:
                command += ["-Wno-unused-parameter", "-I", str(tests / "stub"), str(tests / "stubs.c"), str(target)]
            command += ["-o", str(work / "test")]
            compiled = subprocess.run(command, capture_output=True, text=True)
            if compiled.returncode:
                raise RuntimeError(f"{name}: compile failed\n{compiled.stderr}")
            result = subprocess.run([str(work / "test")], capture_output=True, text=True, cwd=work)
            if (result.returncode == 0) == bool(old):
                raise RuntimeError(f"{name}: unexpected test status {result.returncode}\n{result.stderr}")
            print(f"{name}: compile=0, test={result.returncode}")
    print(f"boot reverse controls: {len(mutations)}/{len(mutations)} rejected; all four baselines passed")


if __name__ == "__main__":
    main()
