"""memo-3: the reference walk (touch and AI, default and largest text, child), autosave at leaving, locking and another
member, saving while typing, members, schema-1 records and the copied ZSWatch screen; the refresh of each main action
from the default walk's display trace; source-only negative controls (--mutant). --shots DIR keeps the pictures."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
MEMO = 'system/apps/memo/memo.c'
VENDOR = 'system/apps/memo/vendor/zswatch/voice_memo_ui.c'
MUTANTS = {  # what goes, and the run that must then fail
    'leave': (MEMO, '    if (M.field && !flush_note(M.mode == FAIL)) { /* to the member', '    if (0 && !flush_note(M.mode == FAIL)) { /* to the member', 'leave'),
    'member': (MEMO, '    if (M.field && !flush_note(M.mode == FAIL)) { /* to the member', '    if (M.field && !strcmp(M.member, ink_member_current()->id) && !flush_note(M.mode == FAIL)) { /*', 'member'),
    # what the review found: each fix taken back must fail the review run
    'reading': (MEMO, '            else if (mode == DETAIL && due) mode = DUE;', '            else if (mode == DETAIL && !n->path[0]) mode = due ? DUE : EDIT;', 'walk'),
    'undo-stays': (MEMO, '        if (!result) U.id = 0;', '        if (0) U.id = 0;', 'review'),
    'open-writes': (MEMO, ['!(save && known) && (store(', 'M.mode == FAIL || (!M.blank && !kept) ? M.note.body : "\\1"'],
                    ['(store(', 'M.mode == FAIL ? M.note.body : "\\1"'], 'review'),  # both guards: the editor's and memo.save's
    'fail-leave': (MEMO, '    if (M.field && !flush_note(M.mode == FAIL)) { /* to the member', '    if (M.field && M.mode != FAIL && !flush_note(false)) { /* to the member', 'review'),
    'blink': (VENDOR, '    if (ui.remaining_label && remaining_s > 0) {\n        lv_label_set_text_fmt(ui.remaining_label, TR(',
              '    lv_obj_set_style_opa(ui.rec_indicator, (elapsed_ms / 400) % 2 ? LV_OPA_TRANSP : LV_OPA_COVER, 0);\n'
              '    if (ui.remaining_label && remaining_s > 0) {\n        lv_label_set_text_fmt(ui.remaining_label, TR(', 'vendor'),
}
ACTIONS = ['open_list', 'search', 'open_memo', 'type', 'find_next', 'check', 'add_item', 'set_due', 'start_record', 'minute', 'stop', 'save_fail']
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--mutant', choices=tuple(MUTANTS))
p.add_argument('--phase')
p.add_argument('--shots', type=Path)
a = p.parse_args()
build = a.build.resolve()
area = build / 'test-out' / 'memo-3'
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)
binary = build / 'sim-memo-3'
if a.mutant:
    path, old, new, run = MUTANTS[a.mutant]
    source = (ROOT / path).read_text()
    for o, n in zip(*((old, new) if isinstance(old, list) else ([old], [new]))):
        assert source.count(o) == 1, (a.mutant, o)
        source = source.replace(o, n)
    target = area / Path(path).name
    target.write_text(source)
    flags = {}
    for line in (build / 'CMakeFiles/sim-memo-3.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            key, value = line.split(' = ', 1)
            flags[key] = shlex.split(value)
    define = ('-DMEMO3_SOURCE=' if path == MEMO else '-DMEMO3_VENDOR=') + json.dumps(str(target))
    obj = area / 'memo3.o'
    port = ['-I' + str(ROOT / 'system/apps/memo/vendor/zswatch/port'), '-I' + str(ROOT / 'system/apps/memo/vendor/zswatch')]
    if subprocess.run([shlex.split((build / 'CMakeFiles/sim-memo-3.dir/link.txt').read_text())[0], *flags['C_DEFINES'], *flags['C_INCLUDES'],
                       *port, *flags['C_FLAGS'], define, '-c', str(ROOT / 'tests/memo_3/memo_3.c'), '-o', str(obj)], cwd=build).returncode:
        raise SystemExit(2)
    link = shlex.split((build / 'CMakeFiles/sim-memo-3.dir/link.txt').read_text())
    link = [str(obj) if w.endswith('tests/memo_3/memo_3.c.o') else w for w in link]
    binary = area / 'sim-memo-3'
    link[link.index('-o') + 1] = str(binary)
    if subprocess.run(link, cwd=build).returncode:
        raise SystemExit(2)
    runs = [(run, '1', '0')]
else:
    runs = [('walk', '1', '0'), ('walk', '3', '0'), ('walk', '1', '1'), ('ai', '1', '0'), ('leave', '1', '0'), ('lock', '1', '0'),
            ('member', '1', '0'), ('busy', '1', '0'), ('isolation', '1', '0'), ('old', '1', '0'), ('vendor', '1', '0'), ('review', '1', '0'), ('review', '3', '0')]
    if a.phase:
        runs = [r for r in runs if r[0] == a.phase]
ok = True
for phase, size, kid in runs:
    name = f'{phase}-{size}' + ('-kid' if kid == '1' else '')
    out = area / name
    trace = phase == 'walk' and size == '1' and kid == '0' and not a.mutant
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/memo_3/memo_3.script'), '--budget', str(ROOT / 'tests/memo_record/budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
        '--flash-image', str(area / (name + '.img')), '--sd-dir', str(area / (name + '-sd')), '--out', str(out),
        '--quiet', '--no-random-outages'] + (['--trace'] if trace else []),
        env={**os.environ, 'INK_MEMO3_PHASE': phase, 'INK_MEMO3_TEXT': size, 'INK_MEMO3_KID': kid}, capture_output=True, text=True, timeout=900)
    (area / f'{name}.log').write_text(result.stdout + result.stderr)
    data = json.loads((out / 'metrics.json').read_text(errors='replace')) if (out / 'metrics.json').exists() else {}
    m = data.get('metrics', {})
    passed = result.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0)
    if a.mutant:
        print(a.mutant, 'negative control', 'FAILS AS IT MUST' if not passed else 'DID NOT FAIL', 'failures', m.get('test.fail', 0), flush=True)
        for w in data.get('warnings', []):
            if 'CHECK FAILED' in str(w): print(' ', w)
        ok = not passed
        continue
    print(name, 'PASS' if passed else 'FAIL', 'exit', result.returncode, 'checks', m.get('test.pass', 0), 'failures', m.get('test.fail', 0), flush=True)
    if not passed:
        for w in data.get('warnings', []): print(' ', w)
        print(result.stderr[-1500:])
    if trace and passed:
        events = json.loads((out / 'trace.json').read_text()) if (out / 'trace.json').exists() else []
        marks = {e['name']: e['ts'] for e in events if str(e.get('name', '')).startswith('memo3.')}
        report = {}
        for action in ACTIONS:
            start, end = marks.get(f'memo3.{action}.before'), marks.get(f'memo3.{action}.after')
            if start is None or end is None: continue
            ups = [e for e in events if e.get('name') in ('DU', 'GC16', 'GL16', 'A2') and 'args' in e and start <= e.get('ts', -1) < end]
            # panel x is the page's y (the panel is turned): each update's band down the page, and its pixels
            report[action] = {'refreshes': len(ups), 'bands': [(e['args']['x'], e['args']['x'] + e['args']['w']) for e in ups],
                              'pixels': sum(e['args']['w'] * e['args'].get('h', 0) for e in ups), 'waveforms': [e['name'] for e in ups]}
        (area / 'refresh.json').write_text(json.dumps(report, ensure_ascii=False, indent=1))
        for action, r in report.items():
            print(f'  refresh {action}: {r["refreshes"]} ({", ".join(r["waveforms"])}) bands {r["bands"]}', flush=True)
    if a.shots:
        a.shots.mkdir(parents=True, exist_ok=True)
        for picture in sorted(out.glob('snap-*.png')):
            suffix = '' if size == '1' and kid == '0' else ('-kid' if kid == '1' else '-big')
            shutil.copyfile(picture, a.shots / f'{picture.stem[5:]}{suffix}.png')
    ok &= passed
if ok and not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
elif not ok:
    print('Evidence:', area, flush=True)
raise SystemExit(0 if ok else 1)
