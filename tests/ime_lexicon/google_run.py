"""Reproduce comparison, restart isolation, password immutability and keyboard frames."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--build', type=Path, required=True)
p.add_argument('--acceptance', action='store_true')
p.add_argument('--reverse', action='store_true')
a = p.parse_args()
build = a.build.resolve()
area = Path(tempfile.mkdtemp(prefix='ime-google-', dir=build))
content = area / 'content'
(content / 'ime').mkdir(parents=True)
shutil.copyfile(ROOT / 'assets/ime/dict_pinyin.dat', content / 'ime/dict_pinyin.dat')
assert hashlib.sha256((content / 'ime/dict_pinyin.dat').read_bytes()).hexdigest() == '6bf0bbde4e3134cce38d08524a9f4dc1af40435243c8e36b38eb68d1e14462b2'

def run(mode, state, binary=None, expected=False):
    (state / 'sd').mkdir(parents=True, exist_ok=True)
    out = state / mode
    command = [str(binary or build / 'sim-google-ime'), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
               '--assets', str(build / 'assets'), '--content', str(content), '--flash-image', str(state / 'user.img'),
               '--sd-dir', str(state / 'sd'), '--out', str(out), '--end', '240s', '--quiet', '--no-random-outages',
               '--budget', str(ROOT / 'tests/integration/textinput.budget.json')]
    r = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180,
                       env={**os.environ, 'INK_GOOGLE_MODE': mode, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (state / (mode + '.stdout')).write_text(r.stdout)
    (state / (mode + '.stderr')).write_text(r.stderr)
    assert 'SYNTHETIC_SECRET' not in r.stdout + r.stderr, 'password must not appear in logs'
    if expected and not (out / 'metrics.json').exists():
        assert r.returncode < 0, (mode, r.returncode, r.stderr)
        print(json.dumps({'mode': mode, 'signal': -r.returncode, 'metrics': 'unavailable after the expected crash'}), flush=True)
        return r, {}, out
    report = json.loads((out / 'metrics.json').read_text())
    m = report['metrics']
    if expected:
        assert r.returncode and m.get('test.fail', 0), report
    else:
        assert r.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail') and not report.get('warnings'), (mode, report.get('warnings'))
    print(json.dumps({'mode': mode, 'exit': r.returncode, 'checks': m.get('test.pass', 0), 'failures': m.get('test.fail', 0)}, ensure_ascii=False), flush=True)
    return r, report, out

if a.reverse:
    import shlex
    db = json.loads((build / 'compile_commands.json').read_text())
    changes = [
        ('width', ROOT / 'system/services/text/vendor/googlepinyin/share/spellingtrie.cpp',
         'ink_gp_read_32(&spelling_size_, 1, fp)', 'fread(&spelling_size_, sizeof(size_t), 1, fp)'),
        ('member', ROOT / 'system/services/text/google_pinyin.cpp',
         'GP.ready = false; GP.count = 0; GP.fixed_letters = GP.fixed_chars = 0; im_close_decoder();', 'GP.count = 0;'),
    ]
    for name, source, before, after in changes:
        original = source.read_text()
        assert before in original
        folder = area / name
        folder.mkdir()
        mutated = folder / source.name
        text = original.replace(before, after)
        if name == 'member':
            text = text.replace('extern "C" bool py_google_open(void) {\n    lock();', 'extern "C" bool py_google_open(void) {\n    lock(); if (GP.ready) { unlock(); return true; }')
        mutated.write_text(text)
        entry = next(e for e in db if Path(e['file']) == source)
        obj = folder / (name + '.o')
        command = shlex.split(entry['command'])
        command[command.index('-o') + 1] = str(obj)
        command[command.index(str(source))] = str(mutated)
        command += ['-I' + str(source.parent)]
        # Upstream sources use relative include paths; preserve those via an include search root.
        if name == 'width':
            command += ['-I' + str(source.parent)]
        subprocess.run(command, cwd=entry['directory'], check=True, capture_output=True, text=True)
        link = shlex.split((build / 'CMakeFiles/sim-google-ime.dir/link.txt').read_text())
        binary = folder / 'sim-google-ime'
        link[link.index('-o') + 1] = str(binary)
        link.insert(link.index('libink_system.a'), str(obj))
        subprocess.run(link, cwd=build, check=True, capture_output=True, text=True)
        run('learn', folder / 'state', binary, expected=True)
    print('reverse controls failed as required; evidence:', area)
    raise SystemExit(0)

results = {}
for mode in ('old', 'new'):
    r, report, out = run(mode, area / mode)
    rows = []
    for line in r.stdout.splitlines():
        if not line.startswith('GOOGLE\t'):
            continue
        _, index, key, us, *words = line.split('\t')
        rows.append({'index': int(index), 'key': key, 'query_us': float(us),
                     'candidates': [w.rsplit(':', 1)[0] for w in words],
                     'consumed': [int(w.rsplit(':', 1)[1]) for w in words]})
    assert len(rows) == 110
    results[mode] = {'rows': rows, 'metrics': report['metrics']}
    assert len(list(out.glob('snap-*.png'))) == 10
    assert all(p.stat().st_size < 300 * 1024 for p in out.glob('snap-*.png'))
state = area / 'members'
run('learn', state)
files = sorted((state / 'sd').rglob('google.user'))
assert len(files) == 2
before = {str(f.relative_to(state)): hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
run('verify', state)
after = {str(f.relative_to(state)): hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
assert before == after, 'verification and password input must not change either member dictionary'
corpus = [line.split('\t') for line in (ROOT / 'tests/ime_lexicon/cases.tsv').read_text().splitlines() if line and not line.startswith('#')]
summary = {}
for mode in results:
    rows = results[mode]['rows']
    counts = {}
    for group, indices in [('all', range(90)), ('full', range(73)), ('initials', range(73, 89))]:
        counts[group] = {'first': sum(corpus[i][1] in rows[i]['candidates'][:1] and rows[i]['consumed'][0] == len(corpus[i][0]) for i in indices),
                         'five': sum(any(w == corpus[i][1] and n == len(corpus[i][0]) for w, n in zip(rows[i]['candidates'], rows[i]['consumed'])) for i in indices)}
    summary[mode] = counts
summary['replace_default'] = (summary['new']['all']['first'] >= 83 and summary['new']['full']['first'] >= 71 and summary['new']['full']['five'] == 73 and summary['new']['initials']['first'] >= 12 and summary['new']['initials']['five'] >= 14)
(area / 'comparison.json').write_text(json.dumps(results, ensure_ascii=False, indent=2) + '\n')
(area / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(summary, ensure_ascii=False, indent=2))
print('evidence:', area)
if a.acceptance:
    assert summary['replace_default'], 'quality gate failed: keep the old decoder and resource pack'
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
