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
    board = (root / "boards/metalio_plus/metalio_board.c").read_text()
    mutations = [
        ("debounce", "logic", "now - k->since >= S31_KEY_DEBOUNCE_US", "true"),
        ("rotation", "logic", "MT_PANEL_H, MT_PANEL_W, S31_PANEL_ROTATION", "MT_PANEL_H, MT_PANEL_W, 0"),
        ("touch-release", "logic", "s31_touch_emit(id, &rp[id], HAL_TOUCH_UP, t, emit, ctx);", "(void)t;"),
        ("motion-cause", "logic", "if ((armed & HAL_WAKE_MOTION) && moved) why |= HAL_WAKE_MOTION;", "(void)moved;"),
        ("cancel", "power", "!(cfg->cancel && cfg->cancel(cfg->cancel_ctx))", "true"),
        ("final-busy", "power", "if (!busy() &&", "if (true &&"),
        ("stale-wake", "power", "if (wake_pins & (1ull << S31_GPIO_EXP_INT))", "if (esp_sleep_get_ext1_wakeup_status() & (1ull << S31_GPIO_EXP_INT))"),
        ("restore-order", "power", "    if (masked) {", "    if (cut) restore(restore_rails);\n    cut = false;\n    if (masked) {"),
        ("input-pause-order", "power", "    epd_power_sleep(&panel);\n    s31_input_pause();", "    s31_input_pause();\n    epd_power_sleep(&panel);"),
        ("usb-revive", "power", "if (woke_at && cause != HAL_WAKE_TIMER) ink_usb_revive();", "if (false && woke_at && cause != HAL_WAKE_TIMER) ink_usb_revive();"),
        ("vendor-sleep", "power", "sleep_rc = ink_s31_light_sleep();", "sleep_rc = esp_light_sleep_start();"),
        ("dynamic-frequency", "power", ".min_freq_mhz = mhz", ".min_freq_mhz = S31_IDLE_MHZ"),
        ("gpio61-wake", "power", "    return us || (armed & ~HAL_WAKE_TIMER);", "    gpio_wakeup_enable(S31_GPIO_AI_KEY, GPIO_INTR_LOW_LEVEL);\n    esp_sleep_enable_gpio_wakeup();\n    return us || (armed & ~HAL_WAKE_TIMER);"),
        ("rtc-arm-idle", "board", "        if (s == HAL_I2C_OK && !(levels & MB_P1_RTC_INT)) s = HAL_I2C_ERR;", ""),
    ]
    with tempfile.TemporaryDirectory(prefix="s31-negative-", dir=args.build.resolve()) as temp:
        work = Path(temp)
        for name, kind, old, new in [("baseline-input", "logic", "", ""), ("baseline-power", "power", "", ""), ("baseline-board", "board", "", "")] + mutations:
            text = {"logic": header, "power": power, "board": board}[kind]
            if old:
                assert text.count(old) == 1, name
                text = text.replace(old, new)
            (work / "s31_input_logic.h").write_text(header)
            target = work / {"logic": "s31_input_logic.h", "power": "hal_power_s31.c", "board": "metalio_board.c"}[kind]
            target.write_text(text)
            sources = [tests / "logic.c", root / "boards/metalio_plus/metalio_touch.c"] if kind == "logic" else [tests / "power.c", target]
            if kind == "board":
                sources = [tests / "board.c", target, root / "boards/metalio_plus/metalio_epd_power.c",
                           root / "drivers/chips/tca9555/tca9555.c", root / "drivers/chips/tps65185/tps65185.c"]
            command = common[:1] + ["-I", str(work)] + common[1:] + [str(s) for s in sources] + ["-o", str(work / "test")]
            compiled = subprocess.run(command, capture_output=True, text=True)
            if compiled.returncode:
                raise RuntimeError(f"{name}: compile failed\n{compiled.stderr}")
            result = subprocess.run([str(work / "test")], capture_output=True, text=True, cwd=work)
            if result.returncode == 0 and old:
                raise RuntimeError(f"{name}: mutation unexpectedly passed")
            if result.returncode != 0 and not old:
                raise RuntimeError(f"{name}: baseline failed\n{result.stderr}")
            evidence = result.stderr.strip().splitlines()
            print(f"{name}: compile=0, test={result.returncode}" + (f"; {evidence[-1]}" if evidence else ""))
    print(f"reverse controls: {len(mutations)}/{len(mutations)} rejected; all three baselines passed")


if __name__ == "__main__":
    main()
