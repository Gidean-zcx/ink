"""Remove individual guards in ignored source copies; the unchanged HTTP test must reject them."""
import argparse
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
build = a.build.resolve()
target = build / "CMakeFiles/sim-wireless-transfer.dir"
flags = {}
for line in (target / "flags.make").read_text().splitlines():
    if " = " in line:
        key, value = line.split(" = ", 1)
        flags[key] = shlex.split(value)
template = shlex.split((target / "link.txt").read_text())
service = root / "system/services/file/transfer.c"
hal = root / "ports/sim/hal/hal_transfer_sim.c"
cases = {
    "no_service": (service, 'return ink_cap_register(TRANSFER_CAPS, sizeof TRANSFER_CAPS / sizeof TRANSFER_CAPS[0]);', '(void)TRANSFER_CAPS; return 0;'),
    "no_code_check": (service, 'if (!code || strcmp(code, expected))', 'if (false)'),
    "no_timeouts": (service, 'if (osal_now_us() >= T.expires || (T.connected && osal_now_us() >= T.idle))', 'if (false)'),
    "no_nul_validation": (service, 'v < 32 || v == 127 || ', ''),
    "no_usb_lease": (service, 'ink_fs_card_hold()', '0'),
    "no_response_drain": (hal, '''            char discard[512];
            shutdown(client, SHUT_WR);
            for (size_t drained = 0; drained < 65536;) {
                ssize_t n = recv(client, discard, sizeof discard, 0);
                if (n <= 0) break;
                drained += (size_t)n;
            }
''', ''),
}
with tempfile.TemporaryDirectory(prefix="wireless-reverse-", dir=build) as directory:
    work = Path(directory)
    for name, (source, before, after) in cases.items():
        text = source.read_text()
        assert text.count(before) == 1, name
        text = text.replace(before, after)
        if name == "no_usb_lease":
            text = text.replace('ink_fs_card_release();', '(void)0;')
        changed, obj, binary = work / f"{name}.c", work / f"{name}.o", work / name
        changed.write_text(text)
        subprocess.run([template[0], *flags['C_DEFINES'], *flags['C_INCLUDES'],
                        '-I' + str(service.parent), *flags['C_FLAGS'], '-c', str(changed), '-o', str(obj)], cwd=build, check=True)
        link = list(template)
        link[link.index('-o') + 1] = str(binary)
        first_lib = next(i for i, word in enumerate(link) if word.endswith('.a'))
        link.insert(first_lib, str(obj))
        subprocess.run(link, cwd=build, check=True, capture_output=True)
        run = subprocess.run([sys.executable, str(root / 'tests/wireless_transfer/client.py'), '--build', str(build), '--sim', str(binary)],
                             cwd=root, capture_output=True, text=True, timeout=65)
        # Compiler/link failures do not count as a negative runtime control.
        assert run.returncode != 0, f"removed guard was not detected: {name}"
        evidence = next((line.strip() for line in reversed(run.stderr.splitlines()) if line.strip()), "")
        print(f"PASS: {name} rejected, client exit={run.returncode}: {evidence[:240]}", flush=True)
