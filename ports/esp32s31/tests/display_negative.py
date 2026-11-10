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
        ("area-offset", "regions", "(a->x + x) / 2", "x / 2"),
        ("gray-conversion", "regions", "(pixels[(size_t)y * stride + x] * 15 + 127) / 255", "0"),
        ("inline-callback", "regions", "    xTaskNotifyGive(D.task);\n    return id;", "    if (done) done(ctx, &D.result);\n    xTaskNotifyGive(D.task);\n    return id;"),
        ("scan-timeout", "timeout", "    epd_power_off(&D.power);", "    (void)D.power;"),
        ("independent-guard", "guard", ">= pdMS_TO_TICKS(GUARD_MS)) trip();", ">= pdMS_TO_TICKS(GUARD_MS)) { }"),
        ("late-line-discard", "lines", "while (D.owed[id] && lq_read(&D.q[id], line) == 0) --D.owed[id];", "(void)id;"),
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
    print("display reverse controls: 6/6 rejected; baseline passed")


if __name__ == "__main__":
    main()
