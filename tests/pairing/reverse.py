"""Remove individual enrollment safeguards in ignored source copies and rerun recovery."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
p.add_argument("--only", action="append")
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
    "escaped-hello-buffer": ('ink_pairing_handshake(hello.method, params, 2048)', 'ink_pairing_handshake(hello.method, params, 512)'),
    "untrusted-revocation": ('code == -32011 && e->trusted', 'code == -32011'),
    "cross-key-authorization": ('!old || !same || s->state == INK_PAIR_REVOKED', '!old || s->state == INK_PAIR_REVOKED'),
    "commit-watchdog": ('? P.retry_at :', '? 0 :'),
    "accept-error-handling": ('if (status == INK_HUB_OK) return;', 'if (true) return;'),
    "ai-hotspot-control": ('.title="关闭设备设置热点", CONTROL', '.title="关闭设备设置热点", OWNER'),
    "ai-hotspot-secret": ('if (c->src == INK_SRC_TOUCH || c->src == INK_SRC_KEY) ink_cap_result(c, result);', 'ink_cap_result(c, result);'),
    "ai-import-confirmation": ('.ask_by_hand=true, .ask="从存储卡加入文件中的家庭？",', ''),
    "key-switch-write-order": ('fresh(s);\n                ok = store_identity(s);', 'ok = durable(CONFIG_FILE, c, sizeof *c);\n                fresh(s);\n                if (ok) ok = store_identity(s);'),
    "member-control-before-auth": ('enqueue_tx(true, cls, data, len, member, generation, false)', 'enqueue_tx(true, cls, data, len, member, generation, true)'),
    "stale-member-frame": ('it->member_generation != ink_member_generation(it->member) || it->member_epoch != __atomic_load_n(&H.member_epoch, __ATOMIC_ACQUIRE)', 'false'),
}
if a.only and set(a.only) - mutations.keys():
    raise SystemExit("unknown mutation selection")
flags = {}
target = build / "CMakeFiles/sim-pairing.dir"
for line in (target / "flags.make").read_text().splitlines():
    if " = " in line:
        key, value = line.split(" = ", 1)
        flags[key] = shlex.split(value)
original_link = shlex.split((target / "link.txt").read_text())
for label, (before, after) in mutations.items():
    if a.only and label not in a.only:
        continue
    original = hub_source if label in ("completion-not-flushed", "ordinary-response-before-auth", "escaped-hello-buffer", "member-control-before-auth", "stale-member-frame") else source
    if original.count(before) != 1:
        raise SystemExit(label + ": safeguard not found exactly once")
    altered = work / (label + ".c")
    altered.write_text(original.replace(before, after, 1))
    storage = label == "key-switch-write-order"
    compile_source = altered
    link_template = original_link
    if storage:
        compile_source = work / "storage.c"
        compile_source.write_text((ROOT / "tests/pairing/storage_faults.c").read_text().replace('../../system/services/hub/pairing.c', str(altered)))
        link_template = shlex.split((build / "CMakeFiles/sim-pairing-storage.dir/link.txt").read_text())
    if label == "stale-member-frame":
        link_template = shlex.split((build / "CMakeFiles/sim-member-wake-stream.dir/link.txt").read_text())
    obj = work / (label + ".o")
    cmd = [original_link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"], "-I" + str(ROOT / "tests/pairing"), "-c", str(compile_source), "-o", str(obj)]
    subprocess.run(cmd, cwd=build, check=True)
    link = link_template.copy()
    binary = work / label
    link[link.index("-o") + 1] = str(binary)
    if storage:
        link[link.index('CMakeFiles/sim-pairing-storage.dir/tests/pairing/storage_faults.c.o')] = str(obj)
    else:
        index = next(i for i, word in enumerate(link) if word.endswith(".a"))
        link.insert(index, str(obj))
    subprocess.run(link, cwd=build, check=True)
    if label == "stale-member-frame":
        out = work / (label + "-out")
        run = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus-handsfree.json"),
                              "--budget", str(ROOT / "tests/integration/member_wake.budget.json"),
                              "--script", str(ROOT / "tests/integration/member_wake_stream.script"),
                              "--assets", str(build / "assets"), "--content", str(build / "test-content"),
                              "--out", str(out), "--quiet", "--no-random-outages"],
                             cwd=ROOT, capture_output=True, text=True, timeout=90,
                             env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})
        (work / (label + ".log")).write_text(run.stdout + run.stderr)
        report = json.loads((out / "metrics.json").read_text())
        metrics = report["metrics"]
        if run.returncode != 1 or metrics.get("test.finished") != 1 or metrics.get("test.fail", 0) < 1 or not any("switching back does not revive the old frame" in warning for warning in report.get("warnings", [])):
            raise SystemExit(label + ": expected member isolation failure not observed")
        print(label, "PASS: removed safeguard rejected by unchanged regression", flush=True)
        continue
    runner = "storage.py" if storage else "run.py"
    run = subprocess.run([sys.executable, str(ROOT / "tests/pairing" / runner), "--build", str(build), "--binary", str(binary)],
                         cwd=ROOT, capture_output=True, text=True, timeout=90,
                         env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})
    (work / (label + ".log")).write_text(run.stdout + run.stderr)
    expected = {"unconfirmed-complete": "unsolicited completion rejected", "accept-not-durable": "accept loss keeps transaction without token",
                "token-not-durable": "saved token used despite missing completion acknowledgement", "revocation-not-durable": "revocation persisted and reconnect stopped",
                "completion-not-flushed": "completion acknowledgement explicitly lost at peer", "wipe-not-boolean": "string true cannot authorize irreversible wipe",
                "nul-transaction": "embedded NUL transaction rejected", "ordinary-response-before-auth": "ordinary responses cannot leave on a preauth connection",
                "unconfirmed-resume": "unconfirmed offer is not resumed as accepted", "escaped-hello-buffer": "escaped token within limits is sent to hello",
                "untrusted-revocation": "untrusted begin cannot authorize wipe", "cross-key-authorization": "authorization and nonce stay only with the same pinned key",
                "commit-watchdog": "single lost progress recovers without reboot", "accept-error-handling": "expired accept cannot advance to token authentication",
                "ai-hotspot-control": "AI can stop setup hotspot", "ai-hotspot-secret": "AI can start hotspot without receiving credentials",
                "ai-import-confirmation": "AI may request card import for a person to confirm",
                "key-switch-write-order": "failed authorization clear leaves only the original trust anchor",
                "member-control-before-auth": "member control frames cannot leave on a preauth connection"}[label]
    destructive_reboot = label == "wipe-not-boolean" and "wipe-string FAIL exit=0" in run.stdout and '"power.reboot": 1' in run.stdout
    destructive_reboot = destructive_reboot or label == "untrusted-revocation" and "guard-begin-wipe FAIL exit=0" in run.stdout and '"power.reboot": 1' in run.stdout
    if run.returncode != 1 or (expected not in run.stdout and not destructive_reboot) or "exit=0" not in run.stdout:
        print(run.stdout[-2500:], run.stderr[-1500:])
        raise SystemExit(label + ": expected behavioral failure not observed")
    print(label, "PASS: removed safeguard rejected by unchanged regression", flush=True)
print("Evidence directory:", work.relative_to(ROOT))
