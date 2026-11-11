"""Compile local mutations of the actual policy/routes; no git history is needed."""
import json
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

build = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parents[3]
port = root / "ports/esp32s31"
policy = root / "system/services/file/transfer.c"
vendor = port / "vendor/ap_transfer"
fixture = port / "tests/transfer_policy.c"
commands = json.loads((build / "compile_commands.json").read_text())
entries = {Path(c["file"]).resolve(): c for c in commands if "test-s31-transfer.dir" in str(c)}
link = shlex.split((build / "CMakeFiles/test-s31-transfer.dir/link.txt").read_text())
source = policy.read_text()
open_line = next(s for s in source.splitlines() if "case HAL_TRANSFER_OPEN:" in s)
cases = [
    ("failed-start-data", policy, 'T.files = 0; T.left = 0; T.received_name[0] = T.path[0] = 0; T.result = NULL;', '', fixture),
    ("failed-start-guard", policy, 'if (r && !T.active && T.guard) { osal_mutex_delete(T.guard); T.guard = NULL; }', '', fixture),
    ("wrong-code", policy, 'if (!T.used && !strcmp(code, expected))', 'if (!T.used && strlen(expected))', fixture),
    ("member", policy, 'T.epoch == ink_member_epoch() &&', '', fixture),
    ("expiry", policy, 'osal_now_us() < T.expires &&', '', fixture),
    ("ai-secret", policy, 'c->src == INK_SRC_AI ? 0u', 'false ? 0u', fixture),
    ("admission", policy, open_line, '        case HAL_TRANSFER_OPEN: r = 0; io->handle = (void *)1; break;', fixture),
    ("delete-policy", policy, 'r = fs_remove_for(&T.actor, path, 0); break;', 'r = 0; break;', fixture),
    ("http-auth", vendor / "ink_adapter.h", 'if (!ink_policy->authorize(code, token, issued)) return false;', '(void)code; (void)token;', vendor / "ap_transfer.cc"),
    ("space", vendor / "ap_transfer.cc", 'if (expected > free_bytes)', 'if (false && expected > free_bytes)', vendor / "ap_transfer.cc"),
    ("frame-auth", vendor / "ap_transfer.cc", 'session == nullptr || !ink_policy->authorize(nullptr, session->token, nullptr)', 'session == nullptr', vendor / "ap_transfer.cc"),
]
with tempfile.TemporaryDirectory(prefix="transfer-reverse-", dir=build) as directory:
    work = Path(directory)
    for name, target, before, after, compiled in cases:
        text = target.read_text()
        assert text.count(before) == 1, name
        changed = work / target.name
        changed.write_text(text.replace(before, after))
        translation_unit = changed
        if compiled == fixture:
            translation_unit = work / fixture.name
            translation_unit.write_text(fixture.read_text().replace('"../../../system/services/file/transfer.c"', '"' + str(changed) + '"'))
        elif target.suffix == ".h":
            translation_unit = work / compiled.name
            translation_unit.write_text(compiled.read_text())
        entry = entries[compiled.resolve()]
        args = shlex.split(entry["command"])
        original_object = args[args.index("-o") + 1]
        obj = work / (name + ".o")
        args[args.index("-o") + 1] = str(obj)
        args[args.index(str(compiled.resolve()))] = str(translation_unit)
        args[1:1] = ["-I" + str(policy.parent), "-I" + str(work)]
        subprocess.run(args, cwd=build, check=True, capture_output=True)
        binary = work / name
        cmd = [str(obj) if p == original_object else p for p in link]
        cmd[cmd.index("-o") + 1] = str(binary)
        subprocess.run(cmd, cwd=build, check=True, capture_output=True)
        result = subprocess.run([str(binary)], cwd=build, capture_output=True, text=True)
        assert result.returncode != 0, name + " mutation escaped detection"
        line = next((s for s in result.stderr.splitlines() if "Assertion" in s), result.stderr[-160:])
        print(f"{name}: expected failure, exit={result.returncode}, {line[:180]}", flush=True)
        changed.unlink()
