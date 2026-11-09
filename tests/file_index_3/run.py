"""Exercise automatic card indexing, persistence, card removal and real storage UI."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--baseline')
p.add_argument('--mutant', choices=['polling', 'blocking', 'file-change', 'card-change', 'identity', 'entropy'])
a = p.parse_args()
build = a.build.resolve()
area = build / 'test-out' / ('file-index-3-baseline' if a.baseline else 'file-index-3-' + a.mutant if a.mutant else 'file-index-3')
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)
binary = build / 'sim-file-index-3'
if a.baseline or a.mutant:
    source = area / 'file_service.c'
    if a.baseline:
        source.write_bytes(subprocess.check_output(['git', 'show', f'{a.baseline}:system/services/file/file_service.c'], cwd=ROOT))
    else:
        current = (ROOT / 'system/services/file/file_service.c').read_text()
        replacements = {
            'polling': ('if (!r && more) ink_svc_at(&FS.index_at, osal_now_us() + 50000ull, index_refresh_cb, NULL);',
                        'ink_svc_at(&FS.index_at, osal_now_us() + (more ? 50000ull : 1000000ull), index_refresh_cb, NULL);'),
            'file-change': ('    if (added) index_invalidate();\n', '    (void)added;\n'),
            'card-change': ('index_invalidate();\n    fs_refresh_cb(NULL);', 'fs_refresh_cb(NULL);'),
            'blocking': ('    if (scanned) ink_db_flush_background(db);\n    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);',
                         '    if (db && ink_db_wait(db, ink_db_last_ver(db), 3000000)) r = INK_FS_ERR_IO;\n    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);'),
            'identity': ('''    if (!f) {
        hal_dir_t *d = hal_fs_opendir("/content");
        if (!d) return card_error();
        hal_dirent_t e; int more;
        while ((more = hal_fs_readdir(d, &e)) == 1)
            if (!strcmp(e.name, ".file-index-card")) break;
        hal_fs_closedir(d);
        if (more) return card_error();
    }
''', ''),
        }
        if a.mutant != 'entropy':
            old, new = replacements[a.mutant]
            assert current.count(old) == 1, 'counterexample must remove exactly the intended fix'
            current = current.replace(old, new)
        source.write_text(current)
    flags = {}
    for line in (build / 'CMakeFiles/sim-file-index-3.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            k, v = line.split(' = ', 1)
            flags[k] = shlex.split(v)
    link = shlex.split((build / 'CMakeFiles/sim-file-index-3.dir/link.txt').read_text())
    obj = area / 'baseline.o'
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
        '-I' + str(ROOT / 'system/services/file'), '-DINK_INDEX_SOURCE=' + json.dumps(str(source)),
        '-c', str(ROOT / 'tests/file_index_3/boundary.c'), '-o', str(obj)], cwd=build, check=True)
    binary = area / 'baseline'
    link[link.index('-o') + 1] = str(binary)
    link = [str(obj) if x.endswith('/tests/file_index_3/boundary.c.o') else x for x in link]
    if a.mutant == 'entropy':
        power = (ROOT / 'ports/sim/hal/hal_storage_sim.c').read_text()
        old = 'out[i] = (uint8_t)rand_r(&entropy);'
        assert power.count(old) == 1, 'counterexample must restore shared model randomness'
        seed_line = '    static unsigned entropy = 1; entropy ^= g_sim.seed ^ (uint32_t)osal_now_us();\n'
        assert power.count(seed_line) == 1, 'counterexample removes the unused private stream'
        target = area / 'hal_storage_sim.c'
        target.write_text(power.replace(old, 'out[i] = (uint8_t)(sim_rand01() * 256);').replace(seed_line, ''))
        power_flags = {}
        for line in (build / 'CMakeFiles/ink_port_sim.dir/flags.make').read_text().splitlines():
            if ' = ' in line:
                k, v = line.split(' = ', 1)
                power_flags[k] = shlex.split(v)
        obj = area / 'power.o'
        subprocess.run([link[0], *power_flags['C_DEFINES'], *power_flags['C_INCLUDES'], *power_flags['C_FLAGS'],
            '-c', str(target), '-o', str(obj)], cwd=build, check=True)
        link.insert(next(i for i, word in enumerate(link) if word.endswith('.a')), str(obj))
    subprocess.run(link, cwd=build, check=True, capture_output=True)


def seed(path):
    content = path / 'sd/content'
    content.mkdir(parents=True)
    for i in range(1000):
        (content / f'SYNTH_{i:04d}.txt').write_bytes(b'X')
    for member, size in [('owner', 17), ('kid', 11)]:
        target = content / '.m' / member
        target.mkdir(parents=True)
        (target / 'SYNTH_PRIVATE.txt').write_bytes(b'X' * size)


summary = {}


def run(path, stage, negative=False):
    out = path / stage
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--assets', str(build / 'assets'), '--sd-dir', str(path / 'sd'), '--flash-image', str(path / 'user.img'),
        '--out', str(out), '--end', '200s', '--quiet', '--no-random-outages'], cwd=ROOT,
        env={**os.environ, 'INK_INDEX_3_STAGE': 'legacy' if a.baseline and stage == 'compare' else stage, 'UBSAN_OPTIONS': 'halt_on_error=1'},
        capture_output=True, text=True, timeout=150)
    (path / f'{stage}.stdout.log').write_text(result.stdout)
    (path / f'{stage}.stderr.log').write_text(result.stderr)
    data = json.loads((out / 'metrics.json').read_text())
    m = data.get('metrics', {})
    ok = result.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0) and not data.get('warnings')
    summary[stage] = {k: v for k, v in m.items() if k.startswith(('test.index.', 'file.index.'))}
    if negative:
        assert not ok and m.get('test.fail', 0) > 0, 'baseline must fail automatic-index acceptance'
    elif not ok:
        raise SystemExit(f'FAIL {stage} exit={result.returncode}\n{result.stderr[-2000:]}\n{data.get("warnings")}')
    print(f'{"EXPECTED_FAIL" if negative else "PASS"} {stage} checks={m.get("test.pass")} {summary[stage]}', flush=True)
    if not a.baseline and stage in ('boot', 'ui', 'remove', 'swap'):
        assert m['test.index.max_hold_us']['max'] < 250000, 'batch edit hold must stay under 250 ms in simulator'
    return m


if a.mutant:
    path = area / 'negative'
    seed(path)
    run(path, {'polling': 'idle', 'blocking': 'busy'}.get(a.mutant, a.mutant), negative=True)
elif a.baseline:
    path = area / 'comparison'
    seed(path)
    old = run(path, 'compare')
    current = json.loads((build / 'test-out/file-index-3/summary.json').read_text())
    assert current['compare']['test.index.writes']['max'] * 10 <= old['test.index.writes']['max'], 'writes must fall by an order of magnitude'
    path = area / 'negative'
    seed(path)
    run(path, 'boot', negative=True)
else:
    path = area / 'persist'
    seed(path)
    first = run(path, 'boot')
    second = run(path, 'reboot')
    assert first['test.index.first_id']['max'] == second['test.index.first_id']['max'], 'actual restart retains ids'
    for stage in ('ui', 'remove', 'swap', 'idle', 'busy', 'file-change', 'card-change', 'identity', 'entropy'):
        path = area / stage
        seed(path)
        run(path, stage)
    path = area / 'comparison'
    seed(path)
    run(path, 'compare')
(area / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
