"""Remove individual enrollment safeguards in ignored source copies and rerun recovery."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
work = Path(tempfile.mkdtemp(prefix="pairing-reverse-", dir=build))
source = (ROOT / "system/services/hub/pairing.c").read_text()
hub_source = (ROOT / "system/services/hub/hub_link.c").read_text()
mutations = {
    "unconfirmed-complete": (
        '((s->state == INK_PAIR_COMMITTING && !strcmp(s->txid, txid)) ||\n'
        '                   ((s->state == INK_PAIR_PAIRING || s->state == INK_PAIR_UNPAIRED) && s->preauth[0]))',
        'true'),
    "accept-not-durable": (
        'if (!store_identity(s)) fail(c, "没能保存，请重试");',
        'if (false) fail(c, "没能保存，请重试");'),
    "token-not-durable": (
        'ok = complete_config(j, obj) && store_identity(s);',
        'ok = complete_config(j, obj);'),
    "revocation-not-durable": (
        'fresh(s); s->state = INK_PAIR_REVOKED; s->wipe = wipe;',
        'fresh(s); s->state = INK_PAIR_UNPAIRED; s->wipe = wipe;'),
    "completion-not-flushed": ('hal_ws_flush(H.ws, 2000000)', '(-1)'),
    "wipe-not-boolean": ('w >= 0 && j->tok[w].type == 8 && ink_json_eq(j, w, "true")', 'w >= 0 && ink_json_eq(j, w, "true")'),
    "nul-transaction": (' && !memchr(full, 0, (size_t)n)', ''),
    "ordinary-response-before-auth": ('return respond(id, result_json, false);', 'return respond(id, result_json, true);'),
    "unconfirmed-resume": ('if (s->state == INK_PAIR_COMMITTING && s->txid[0])', 'if (s->txid[0])'),
}
flags = {}
target = build / "CMakeFiles/sim-pairing.dir"
for line in (target / "flags.make").read_text().splitlines():
    if " = " in line:
        key, value = line.split(" = ", 1)
        flags[key] = shlex.split(value)
original_link = shlex.split((target / "link.txt").read_text())
for label, (before, after) in mutations.items():
    original = hub_source if label in ("completion-not-flushed", "ordinary-response-before-auth") else source
    if original.count(before) != 1:
        raise SystemExit(label + ": safeguard not found exactly once")
    altered = work / (label + ".c")
    altered.write_text(original.replace(before, after, 1))
    obj = work / (label + ".o")
    cmd = [original_link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"], "-c", str(altered), "-o", str(obj)]
    subprocess.run(cmd, cwd=build, check=True)
    link = original_link.copy()
    binary = work / label
    link[link.index("-o") + 1] = str(binary)
    index = next(i for i, word in enumerate(link) if word.endswith(".a"))
    link.insert(index, str(obj))
    subprocess.run(link, cwd=build, check=True)
    run = subprocess.run([sys.executable, str(ROOT / "tests/pairing/run.py"), "--build", str(build), "--binary", str(binary)],
                         cwd=ROOT, capture_output=True, text=True, timeout=90,
                         env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})
    (work / (label + ".log")).write_text(run.stdout + run.stderr)
    expected = {"unconfirmed-complete": "unsolicited completion rejected", "accept-not-durable": "accept loss keeps transaction without token",
                "token-not-durable": "saved token used despite missing completion acknowledgement", "revocation-not-durable": "revocation persisted and reconnect stopped",
                "completion-not-flushed": "completion acknowledgement explicitly lost at peer", "wipe-not-boolean": "string true cannot authorize irreversible wipe",
                "nul-transaction": "embedded NUL transaction rejected", "ordinary-response-before-auth": "ordinary responses cannot leave on a preauth connection",
                "unconfirmed-resume": "unconfirmed offer is not resumed as accepted"}[label]
    destructive_reboot = label == "wipe-not-boolean" and "wipe-string FAIL exit=0" in run.stdout and '"power.reboot": 1' in run.stdout
    if run.returncode != 1 or (expected not in run.stdout and not destructive_reboot) or "exit=0" not in run.stdout:
        print(run.stdout[-2500:], run.stderr[-1500:])
        raise SystemExit(label + ": expected behavioral failure not observed")
    print(label, "PASS: removed safeguard rejected by unchanged regression", flush=True)
print("Evidence directory:", work.relative_to(ROOT))
