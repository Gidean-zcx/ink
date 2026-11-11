#!/usr/bin/env python3
"""Manual reverse controls on isolated copies of the actual display port."""
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
    source = (port / "hal_display_s31.c").read_text()
    mutations = [
        ("abort-before-callback-clear", "abort-contract", "    epd_lcd_abort_frame(); /* Task context: the old ISR must exit before callback/queue reuse. */\n    epd_lcd_line_source_cb(NULL, NULL);", "    epd_lcd_line_source_cb(NULL, NULL);\n    epd_lcd_abort_frame();"),
        ("off-must-be-confirmed", "unconfirmed-off", "    atomic_store(&D.powered, !ink_panel_off_confirmed(&D.power));", "    atomic_store(&D.powered, false);"),
        ("drain-old-completion", "abort-contract", "    while (xSemaphoreTake(D.frame_done, 0) == pdTRUE) { }", "    (void)D.frame_done;"),
        ("unconfirmed-rails-block-refresh", "unconfirmed-off", "    if (ATOMIC(D.powered)) atomic_store(&D.stop, true);", ""),
        ("area-offset", "regions", "(a->x + x) / 2", "x / 2"),
        ("gray-conversion", "regions", "(pixels[(size_t)y * stride + x] * 15 + 127) / 255", "0"),
        ("inline-callback", "regions", "    xTaskNotifyGive(D.task);\n    return id;", "    if (done) done(ctx, &D.result);\n    xTaskNotifyGive(D.task);\n    return id;"),
        ("scan-timeout", "timeout", "    epd_power_off(&D.power);", "    (void)D.power;"),
        ("independent-guard", "guard", "if (!ATOMIC(D.testing)) { trip(); return; }", "if (!ATOMIC(D.testing)) return;"),
        ("guard-proves-power-off", "selftest", "    power_off();\n    atomic_store(&D.armed, ATOMIC(D.powered));\n    atomic_store(&D.tripped, !ATOMIC(D.powered));", "    atomic_store(&D.tripped, true);"),
        ("unproven-guard-blocks", "selftest-dead", "        atomic_store(&D.stop, true);\n        hal_log_write(\"display: the guard", "        hal_log_write(\"display: the guard"),
        ("selftest-in-init", "selftest", "    if (!guard_self_test()) {", "    if (0 && !guard_self_test()) {"),
        ("late-line-discard", "lines", "while (D.owed[id] && lq_read(&D.q[id], line) == 0) --D.owed[id];", "(void)id;"),
        ("wake-pending", "wake-overlap", "D.result.id || ATOMIC(D.powered) || (D.wake_until && (int32_t)(D.wake_until - xTaskGetTickCount()) > 0)", "D.result.id || ATOMIC(D.powered) || wake"),
        # trial report 2026-11-11: a refresh crossing into idle ran at the idle clock, missed lines, and one failure stopped the panel for good
        ("refresh-at-full-clock", "regions", "            hal_cpu_boost_acquire(D.boost);\n", ""),
        ("failed-refresh-recovers", "recover", "            if (!r.ok) fail();", "            (void)fail;\n            if (!r.ok) trip();"),
        # trial report 2026-11-11: every page change cleared to black, then white, then drew (~2 s, dark first), against D-110
        ("gl16-is-one-pass", "regions", "            if (D.first || r.waveform == HAL_WF_GC16) {", "            if (true) {"),
        ("gc16-cleans", "regions", "            if (D.first || r.waveform == HAL_WF_GC16) {", "            if (D.first) {"),
        ("declares-cleans-pages", "regions", "        .cleans_pages = true};", "        .cleans_pages = false};"),
    ]
    common = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-UNDEBUG", "-pthread"]
    for p in [tests / "display_stub", port, root / "platform/include", root / "drivers/chips", root / "drivers/display/pqbn", root / "boards"]:
        common += ["-I", str(p)]
    with tempfile.TemporaryDirectory(prefix="display-negative-", dir=args.build.resolve()) as temp:
        work = Path(temp)
        for name, case, old, new in [("baseline", "regions", "", "")] + mutations:
            changed = source
            if old:
                assert source.count(old) == 1, name
                changed = source.replace(old, new)
            path = work / "hal_display_s31.c"
            path.write_text(changed)
            command = common + [f'-DINK_DISPLAY_SOURCE="{path}"', str(tests / "display.c"),
                                str(root / "drivers/display/pqbn/pqbn_v4.c"), "-lm", "-o", str(work / "test")]
            built = subprocess.run(command, capture_output=True, text=True)
            if built.returncode:
                raise RuntimeError(f"{name}: compile failed\n{built.stderr}")
            result = subprocess.run([str(work / "test"), case], capture_output=True, text=True)
            if (result.returncode == 0) == bool(old):
                raise RuntimeError(f"{name}: unexpected test status {result.returncode}\n{result.stderr}")
            print(f"{name}: compile=0, test={result.returncode}")
        old = 'if (pause) {\n        if (!ink_s31_display_acquire(UINT32_MAX)) osal_panic("display: power-off unconfirmed");\n    } else ink_s31_display_release();'
        assert source.count(old) == 1
        for name, changed, must_stop in [
            ('pause-off-failure-baseline', source, True),
            ('pause-off-failure-reverted', source.replace(old, 'if (pause) (void)ink_s31_display_acquire(UINT32_MAX);\n    else ink_s31_display_release();'), False),
        ]:
            path.write_text(changed)
            built = subprocess.run(command, capture_output=True, text=True)
            assert built.returncode == 0, built.stderr
            result = subprocess.run([str(work / "test"), 'pause-off-fails'], capture_output=True, text=True)
            assert (result.returncode != 0) == must_stop, name
            print(f'{name}: compile=0, test={result.returncode}')
    print(f"display reverse controls: {len(mutations)}/{len(mutations)} rejected; baseline passed")


if __name__ == "__main__":
    main()
