#!/usr/bin/env python3
"""Negative controls: only in-memory copies change. Original sources/tests stay untouched.
Each variant is linked to the real simulator, with the existing scenario and budget.
"""
import json
import os
from pathlib import Path
import subprocess
from build_probe import ROOT, build_probe

HERE = Path(__file__).resolve().parent
BUILD = ROOT / "build-audit"
results = []


def replace_once(text, old, new):
    assert text.count(old) == 1, (old, text.count(old))
    return text.replace(old, new)


def run(name, target, script, budget, test_text=None, extras=(), profile="metalio-plus", hub=None):
    source = ROOT / f"tests/integration/{target}.c"
    log = Path("/tmp") / ("ink-audit12-" + name + ".log")
    with log.open("w") as f:
        try:
            binary = build_probe(BUILD, source, name, test_text, extras)
            out = BUILD / "audit12" / name
            cmd = [str(binary), "--profile", str(ROOT/f"simulator/profiles/{profile}.json"),
                   "--script", str(ROOT/script), "--budget", str(ROOT/budget),
                   "--assets", str(BUILD/"assets"), "--content", str(BUILD/"test-content"),
                   "--out", str(out), "--quiet", "--no-random-outages"]
            if hub:
                cmd += ["--hub", str(ROOT/hub)]
            completed = subprocess.run(cmd, stdout=f, stderr=f, timeout=50)
            metrics = json.loads((out/"metrics.json").read_text())
            counts = metrics.get("metrics", {})
            results.append(dict(name=name, exit=completed.returncode, command=cmd,
                                observations={k:v for k,v in counts.items() if k.startswith("test.") or k.startswith("hub.link_")},
                                log=str(log), excerpts=metrics.get("warnings", [])[:8]))
        except subprocess.TimeoutExpired:
            results.append(dict(name=name, timeout_seconds=50, log=str(log)))
    (HERE/"mutation_results.json").write_text(json.dumps(results, ensure_ascii=False, indent=2)+"\n")
    print(json.dumps(results[-1], ensure_ascii=False), flush=True)


def expanded(target, product, text):
    source = ROOT/f"tests/integration/{target}.c"
    test = source.read_text()
    directive = f'#include "../../{product}"'
    return replace_once(test, directive, f'\n#line 1 "{product}"\n{text}\n#line 1 "{source}"\n')


comm_path = ROOT/"system/services/comm/comm_service.c"
comm = replace_once(comm_path.read_text(), "text[INK_ARGS_TEXT]", "text[320]")
run("mut_sms_buffer", "lifecycle", "tests/integration/lifecycle.script", "tests/integration/lifecycle.budget.json",
    extras=[(comm_path, comm)], profile="metalio-plus-handsfree")

rt_path = "system/services/hub/rt.c"
rt = replace_once((ROOT/rt_path).read_text(),
    '''    if (announce(sid) < 0) { /* its owner hears it from the return value */
        drop(sid);
        ink_probe_count("rt.refused");
        return -1;
    }''', '    (void)announce(sid);')
run("mut_rt_nomem", "starts", "tests/integration/starts.script", "tests/integration/starts.budget.json", expanded("starts", rt_path, rt))

exec_path = "system/services/res/exec.c"
old_exec = subprocess.check_output(["git", "show", "c95f48c:"+exec_path], cwd=ROOT, text=True)
run("mut_exec_yield", "starts", "tests/integration/starts.script", "tests/integration/starts.budget.json", expanded("starts", exec_path, old_exec))

ai_path = "system/services/ai/ai_service.c"
ai = (ROOT/ai_path).read_text()
a = ai.index("static void on_reply(")
b = ai.index("/* --------------------------------------------------------- voice uplink", a)
old = ai[a:b]
new = replace_once(old, "sid = take_session();", "sid = AI.session;")
new = replace_once(new, "if (sid) ink_rt_close(sid);", "if (sid) close_session();")
ai = ai[:a] + new + ai[b:]
run("mut_ai_close_current", "ai_race", "tests/integration/ai_race.script", "tests/integration/ai_race.budget.json", expanded("ai_race", ai_path, ai))

remote_path = "system/apps/remote/remote.c"
remote = replace_once((ROOT/remote_path).read_text(), "region_job_t *j = RS.holding ? NULL : g_head;", "region_job_t *j = g_head;")
run("mut_remote_holding", "lifecycle", "tests/integration/lifecycle.script", "tests/integration/lifecycle.budget.json",
    expanded("lifecycle", remote_path, remote), profile="metalio-plus-handsfree")

net_path = ROOT/"system/services/net/net_service.c"
net = replace_once(net_path.read_text(), "    if (want != st.type) {", "    if (prefer == 0) want = st.type;\n    if (want != st.type) {")
run("mut_net_auto", "net_auto", "tests/integration/net_auto.script", "tests/integration/net_auto.budget.json", extras=[(net_path, net)])

hal_path = ROOT/"ports/sim/hal/hal_net_sim.c"
hal = replace_once(hal_path.read_text(), "        set_link(t, env_ok(t)); /* from the old link", "        N.type = t;\n        set_link(t, env_ok(t)); /* from the old link")
run("mut_hal_switch", "net_auto", "tests/integration/net_auto.script", "tests/integration/net_auto.budget.json", extras=[(hal_path, hal)])

# Deliberately one digit beyond the original 20-digit boundary: rejection is expected.
# The first audit run mislabeled this "sms_twenty_digits"; the original fixture was already 20.
source = ROOT/"tests/integration/lifecycle.c"
test = replace_once(source.read_text(), '"10086000000000000000"', '"100860000000000000000"')
run("sms_twenty_one_digits", "lifecycle", "tests/integration/lifecycle.script", "tests/integration/lifecycle.budget.json", test, profile="metalio-plus-handsfree")
