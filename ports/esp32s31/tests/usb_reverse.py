"""Withdraw each USB fix in a temporary copy and require a runtime assertion."""
import json
from pathlib import Path
import resource
import shlex
import subprocess
import sys
import tempfile

build = Path(sys.argv[1]).resolve()
tests = Path(__file__).resolve().parent
port = tests.parent
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
db = json.loads((build / "compile_commands.json").read_text())
entry = next(e for e in db if Path(e["file"]).resolve() == tests / "usb.c")
source = (port / "hal_usb_s31.c").read_text()
mutations = [
    ("local-write-barrier", '__atomic_store_n(&g_present, false, __ATOMIC_RELEASE);',
     '__atomic_store_n(&g_present, true, __ATOMIC_RELEASE);'),
    ("poll-remount-barrier", 'ink_usb_card ? ESP_ERR_INVALID_STATE', 'false ? ESP_ERR_INVALID_STATE'),
    ("eject", 'if (load_eject && !start)', 'if (false && load_eject && !start)'),
    ("restore", 'if (ink_usb_card && ink_usb_reclaim()) DONE(-1);\n    osal_mutex_unlock(g_lock);\n    card_event(NULL, INK_SD_EVENT, 0, NULL);',
     'if (ink_usb_card && ink_usb_reclaim()) DONE(-1);\n    osal_mutex_unlock(g_lock);'),
    ("drain", 'if (tud_task_event_ready())', 'if (false && tud_task_event_ready())'),
    ("route-to-msc", 'epd_board_metalio_eink4_plus_ana_sw_set(false);',
     'epd_board_metalio_eink4_plus_ana_sw_set(true);'),
    ("route-to-usj", '        epd_board_metalio_eink4_plus_ana_sw_set(true);', ''),
    ("disconnect-delay", 'vTaskDelay(pdMS_TO_TICKS(250));', ''),
    ("route-settle", 'vTaskDelay(pdMS_TO_TICKS(100));\n    __atomic_store_n', '__atomic_store_n'),
    ("switching-events", 'if (!__atomic_load_n(&ink_usb_up, __ATOMIC_ACQUIRE)) return;', ''),
    ("boot-route", 'if (!ink_usb_card) epd_board_metalio_eink4_plus_ana_sw_set(true);', ''),
]
with tempfile.TemporaryDirectory(prefix="usb-reverse-", dir=build) as directory:
    work = Path(directory)
    for name, before, after in [("baseline", "", "")] + mutations:
        assert not before or source.count(before) == 1, name
        subject = work / "hal_usb_s31.c"
        subject.write_text(source.replace(before, after) if before else source)
        fixture = work / "usb.c"
        fixture.write_text((tests / "usb.c").read_text().replace('"../hal_usb_s31.c"', '"' + str(subject) + '"'))
        args = shlex.split(entry["command"])
        args.remove("-c")
        args[args.index("-o") + 1] = str(work / "test")
        args[args.index(entry["file"])] = str(fixture)
        args += ["-I" + str(port), str(port.parents[1] / "third_party/littlefs/lfs.c"),
                 str(port.parents[1] / "third_party/littlefs/lfs_util.c")]
        compiled = subprocess.run(args, cwd=build, capture_output=True, text=True)
        assert compiled.returncode == 0, compiled.stderr
        result = subprocess.run([str(work / "test")], cwd=work, capture_output=True, text=True)
        if not before:
            assert result.returncode == 0, result.stderr
            print("baseline: compile=0, test=0", flush=True)
            continue
        assert result.returncode != 0 and ("assert" in result.stderr.lower()), name + ": " + result.stderr
        print(f"{name}: compile=0, expected runtime failure={result.returncode}", flush=True)
