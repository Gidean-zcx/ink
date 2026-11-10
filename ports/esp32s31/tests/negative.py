#!/usr/bin/env python3
"""Manual reverse controls: compile isolated mutations, require each existing check to fail."""
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
    includes = [port, root / "platform/include", root / "drivers/chips", root / "boards", tests / "stub"]
    for path in includes:
        common += ["-I", str(path)]
    header = (port / "s31_input_logic.h").read_text()
    power = (port / "hal_power_s31.c").read_text()
    mutations = [
        ("debounce", "logic", "now - k->since >= S31_KEY_DEBOUNCE_US", "true"),
        ("rotation", "logic", "MT_PANEL_H, MT_PANEL_W, 90", "MT_PANEL_H, MT_PANEL_W, 0"),
        ("touch-release", "logic", "s31_touch_emit(id, &rp[id], HAL_TOUCH_UP, t, emit, ctx);", "(void)t;"),
        ("motion-cause", "logic", "if ((armed & HAL_WAKE_MOTION) && moved) why |= HAL_WAKE_MOTION;", "(void)moved;"),
        ("cancel", "power", "!(cfg->cancel && cfg->cancel(cfg->cancel_ctx))", "true"),
        ("final-busy", "power", "if (!busy() &&", "if (true &&"),
        ("stale-wake", "power", "if (wake_pins & (1ull << S31_GPIO_EXP_INT))", "if (esp_sleep_get_ext1_wakeup_status() & (1ull << S31_GPIO_EXP_INT))"),
        ("restore-order", "power", "    if (masked) {", "    if (cut) restore(restore_rails);\n    cut = false;\n    if (masked) {"),
    ]
    with tempfile.TemporaryDirectory(prefix="s31-negative-", dir=args.build.resolve()) as temp:
        work = Path(temp)
        for name, kind, old, new in [("baseline-input", "logic", "", ""), ("baseline-power", "power", "", "")] + mutations:
            text = header if kind == "logic" else power
            if old:
                assert text.count(old) == 1, name
                text = text.replace(old, new)
            target = work / ("s31_input_logic.h" if kind == "logic" else "hal_power_s31.c")
            target.write_text(text)
            sources = [tests / "logic.c", root / "boards/metalio_plus/metalio_touch.c"] if kind == "logic" else [tests / "power.c", target]
            command = common[:1] + ["-I", str(work)] + common[1:] + [str(s) for s in sources] + ["-o", str(work / "test")]
            compiled = subprocess.run(command, capture_output=True, text=True)
            if compiled.returncode:
                raise RuntimeError(f"{name}: compile failed\n{compiled.stderr}")
            result = subprocess.run([str(work / "test")], capture_output=True, text=True, cwd=work)
            if result.returncode == 0 and old:
                raise RuntimeError(f"{name}: mutation unexpectedly passed")
            if result.returncode != 0 and not old:
                raise RuntimeError(f"{name}: baseline failed\n{result.stderr}")
            print(f"{name}: compile=0, test={result.returncode}")
    print("reverse controls: 8/8 rejected; both baselines passed")


if __name__ == "__main__":
    main()
