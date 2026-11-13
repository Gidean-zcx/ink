"""Reverse checks for control-1: take one piece of the feature out, the control-center walk that covers it must fail;
then put it back. Manual only, not in ctest (it rebuilds sources).

Run from the repository root: python3 tests/control_center_reverse.py [build] [python] [mutation...]. Each mutation is applied to the
source text, sim-control-center is rebuilt, the named walks run, and the original text is written back whatever
happens."""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path.cwd()
BUILD = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'build'
PYTHON = sys.argv[2] if len(sys.argv) > 2 else sys.executable
CC, BAR, LIST = (ROOT / 'system/ui/shell' / f for f in ('control_center.c', 'status_bar.c', 'notification_list.c'))
MUTATIONS = [
    ('whole-panel-redraws', CC, [('    if (C.kid != child() || C.expanded', '    if (true || C.kid != child() || C.expanded')], ['touch']),
    ('minute-redraws-icons', BAR, [('        if (!memcmp(&s, &B.shown[i], sizeof s)) continue;',
                                    '        if (!memcmp(&s, &B.shown[i], sizeof s) && false) continue;')], ['bar']),
    ('bluetooth-always-drawn', BAR, [('    if (s->bt) put_icon(bar, x, "bluetooth");', '    put_icon(bar, x, "bluetooth");')], ['bar']),
    ('airplane-keeps-network', BAR, [('    s->cell = s->plane ? CELL_OFF :', '    s->cell = 0 ? CELL_OFF :'),
                                      ('    else if (s->cell >= 0) {', '    if (s->cell >= 0) {')], ['bar']),
    ('panel-stays-after-airplane', CC, [('    if (C.airplane && !ink_setting_get("net.airplane")) {',
                                         '    if (false && C.airplane && !ink_setting_get("net.airplane")) {')], ['ai']),
    ('child-sees-mobile-network', CC, [('{TR_NOOP("移动网络"), "signal", "shell.mobile", false, true}',
                                        '{TR_NOOP("移动网络"), "signal", "shell.mobile", true, true}')], ['kid']),
    ('no-family-limit', CC, [('    if (child() && t->usable && (!c', '    if (false && child() && t->usable && (!c')], ['kid']),
    ('plus-is-not-a-step', CC, [('    if (!step) {', '    if (1) {')], ['touch']),
    ('one-stage-only', CC, [('        if (C.kid ? !TILE[i].child : !C.expanded && i >= FIRST) continue;',
                             '        if (C.kid ? !TILE[i].child : false) continue;')], ['touch', 'ai']),
    ('empty-notifications-said', LIST, [('    N.control_count = count;\n',
                                         '    if (!count) ink_ui_notice(parent, TR("还没有通知"), y + INK_GAP_L);\n    N.control_count = count;\n')],
     ['shots']),
]


def build():
    r = subprocess.run(['taskpolicy', '-c', 'utility', 'cmake', '--build', str(BUILD), '-j3', '--target', 'sim-control-center'],
                       capture_output=True, text=True)
    if r.returncode:
        print(r.stdout[-3000:], r.stderr[-3000:])
    return r.returncode == 0


def run(case):
    r = subprocess.run(['taskpolicy', '-c', 'utility', PYTHON, '-B', str(ROOT / 'tests/control_center.py'), '--build', str(BUILD),
                        '--case', case], capture_output=True, text=True)
    lines = r.stdout.splitlines()  # the refresh verdicts first, then the first failed checks
    return r.returncode != 0, ([line.strip()[:160] for line in lines if 'refreshes only' in line] +
                               [line.strip()[:160] for line in lines if 'FAILED' in line])[:3]


ONLY = set(sys.argv[3:])
results = []
for name, path, edits, cases in MUTATIONS:
    if ONLY and name not in ONLY:
        continue
    original = path.read_text()
    text = original
    for old, new in edits:
        assert text.count(old) == 1, (name, old)
        text = text.replace(old, new)
    try:
        path.write_text(text)
        assert build(), name
        failed = [(case, *run(case)) for case in cases]
    finally:
        path.write_text(original)
    results.append({'mutation': name, 'cases': cases, 'failed_as_expected': all(f[1] for f in failed),
                    'first_failures': [line for f in failed for line in f[2]][:3]})
    print(json.dumps(results[-1], ensure_ascii=False), flush=True)
assert build(), 'restored build'
again = [run(case)[0] for case in ('touch', 'ai', 'kid', 'bar', 'shots')]
print(json.dumps({'restored': True, 'walks_pass_again': not any(again)}, ensure_ascii=False))
sys.exit(0 if all(r['failed_as_expected'] for r in results) and not any(again) else 1)
