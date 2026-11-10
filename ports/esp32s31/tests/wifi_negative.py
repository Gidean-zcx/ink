#!/usr/bin/env python3
"""Manual reverse controls; mutate temporary copies, require runtime failure after compilation."""
import argparse
import json
from pathlib import Path
import resource
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("build", type=Path)
parser.add_argument("--sim-build", type=Path)
args = parser.parse_args()
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
tests = Path(__file__).resolve().parent
port, root = tests.parent, tests.parents[2]
args.build.mkdir(parents=True, exist_ok=True)
source = (port / "hal_wifi_s31.c").read_text()
mutations = [
    ("nvs-disabled", "cfg.nvs_enable = 0;", "cfg.nvs_enable = 1;"),
    ("ram-storage", "esp_wifi_set_storage(WIFI_STORAGE_RAM)", "esp_wifi_set_storage(1)"),
    ("dhcp-link", "ink_wifi.link.up = ink_wifi.available = true;", "ink_wifi.available = true;"),
    ("known-copy", "memcpy(ink_known, list, (size_t)ink_wifi.n * sizeof *list)", "memset(ink_known, 0, sizeof ink_known)"),
    ("reconnect", "if (ink_wifi.mode == INK_AUTO) ink_rejoin();\n        ESP_LOGI(\"ink_wifi\", \"link down", "if (false && ink_wifi.mode == INK_AUTO) ink_rejoin();\n        ESP_LOGI(\"ink_wifi\", \"link down"),
    ("sntp-delivery", "if (cb && utc > 0) cb(ctx, utc);", "if (false && cb && utc > 0) cb(ctx, utc);"),
    ("scan-bound", "bool full = ink_wifi.count == ink_wifi.max;", "bool full = false;"),
    ("scan-result", "int result = done ? ink_wifi.count : HAL_WIFI_ERR_TIMEOUT;", "int result = done ? ink_wifi.result : HAL_WIFI_ERR_TIMEOUT;"),
]


def run(command, cwd=None):
    return subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=90)


def expect(name, compiled, command, baseline=False, cwd=None):
    if compiled.returncode:
        raise RuntimeError(f"{name}: compile failed\n{compiled.stderr[-3000:]}")
    result = run(command, cwd)
    if (result.returncode == 0) != baseline:
        raise RuntimeError(f"{name}: unexpected exit {result.returncode}\n{result.stdout[-2000:]}\n{result.stderr[-2000:]}")
    evidence = result.stderr.strip().splitlines()
    print(f"{name}: compile=0, test={result.returncode}" + (f"; {evidence[-1]}" if evidence else ""), flush=True)


with tempfile.TemporaryDirectory(prefix="wifi-negative-", dir=args.build.resolve()) as temp:
    work = Path(temp)
    for name, old, new in [("baseline-wifi", "", ""), *mutations]:
        if old:
            assert source.count(old) == 1, name
        target = work / "hal_wifi_s31.c"
        text = source.replace(old, new) if old else source
        if name == "scan-result":
            anchor = "if (((wifi_event_sta_scan_done_t *)data)->status) ink_wifi.count = HAL_WIFI_ERR_TIMEOUT;"
            assert text.count(anchor) == 1
            text = text.replace(anchor, "ink_wifi.result = ((wifi_event_sta_scan_done_t *)data)->status ? HAL_WIFI_ERR_TIMEOUT : ink_wifi.count;")
        target.write_text(text)
        binary = work / name
        command = ["cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-UNDEBUG",
                   "-I", str(tests / "wifi_stub"), "-I", str(root / "platform/include"),
                   f'-DINK_TEST_WIFI_SOURCE="{target}"', str(tests / "wifi.c"), "-o", str(binary)]
        expect(name, run(command), [str(binary)], baseline=not old, cwd=work)
    if args.sim_build:
        build = args.sim_build.resolve()
        entries = json.loads((build / "compile_commands.json").read_text())
        entry = next(e for e in entries if e["file"] == str(root / "tests/esp32s31/net_time.c"))
        original = (root / "system/services/time/time_service.c").read_text()
        checks = [
            ("baseline-time", "", ""),
            ("time-registration", "if (!r) hal_net_set_time_callback(on_network_time, NULL);", "if (r) hal_net_set_time_callback(on_network_time, NULL);"),
            ("time-executor", "ink_exec_post(INK_EXEC_BG, &T.network_job, network_time_job, NULL);", "network_time_job(NULL);"),
            ("time-rtc", "if (!hal_rtc_set(utc_s))", "if (false && !hal_rtc_set(utc_s))"),
        ]
        for name, old, new in checks:
            if old:
                assert original.count(old) == 1, name
            target = work / "time_service.c"
            target.write_text(original.replace(old, new) if old else original)
            command = shlex.split(entry["command"])
            command[command.index("-o") + 1] = str(work / "time.o")
            command += [f'-DINK_TEST_TIME_SOURCE="{target}"', "-I", str(root / "system/services/time")]
            compiled = run(command, entry["directory"])
            if compiled.returncode:
                raise RuntimeError(compiled.stderr)
            link = shlex.split((build / "CMakeFiles/sim-s31-net-time.dir/link.txt").read_text())
            link = [str(work / "time.o") if x.endswith("tests/esp32s31/net_time.c.o") else x for x in link]
            binary = work / name
            link[link.index("-o") + 1] = str(binary)
            cmd = [str(binary), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
                   "--budget", str(root / "tests/ui_scene.budget.json"), "--out", str(work / (name + "-out")),
                   "--end", "8s", "--quiet", "--no-random-outages"]
            expect(name, run(link, build), cmd, baseline=not old, cwd=work)
print("reverse controls passed; temporary copies removed; no Git history required")
