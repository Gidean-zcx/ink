"""Exercise actual package files and lifecycle. Mutants compile only temporary source copies."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', required=True, type=Path)
p.add_argument('--mutant', choices=('install', 'uninstall', 'bad', 'permission', 'snapshot', 'child', 'card'))
a = p.parse_args()
build = a.build.resolve()
work = Path(tempfile.mkdtemp(prefix='app-runtime-', dir=build))
binary = build / 'sim-app-runtime'
if a.mutant:
    source = (ROOT / 'system/ui/app_packages.c').read_text()
    changes = {
        'install': [('else { PK.installed[slot] = bundle; changed = true; }', 'else { ink_app_unregister(bundle->id); osal_free(bundle); }')],
        'uninstall': [(' || !strncmp(path, "/apps", 5)', '')],
        'bad': [(' || strcmp(id, bundle->id)', '')],
        'permission': [('if (!cap || !declared(n->capability) ||', 'if (!cap || false ||')],
        'snapshot': [('return ink_ui_semantics_snapshot(&RP.spec, out, cap);', 'if (cap) out[0] = 0; return -1;')],
        'child': [],
        'card': [('static void storage(void *ctx, int h, const char *id) { (void)ctx; (void)h; (void)id; request_scan(); }',
                  'static void storage(void *ctx, int h, const char *id) { (void)ctx; (void)h; (void)id; if (hal_fs_card_present()) request_scan(); }')],
    }[a.mutant]
    for old, new in changes:
        assert source.count(old) == 1, old
        source = source.replace(old, new)
    (work / 'runtime.c').write_text(source)
    driver = (ROOT / 'tests/app_runtime/app.c').read_text().replace(
        '#include "../../system/ui/app_packages.c"', '#include "runtime.c"').replace(
        '#include "../configured_boot.c"', f'#include "{ROOT}/tests/configured_boot.c"').replace(
        '#include "../integration/whole_product.h"', f'#include "{ROOT}/tests/integration/whole_product.h"')
    (work / 'driver.c').write_text(driver)
    flags = {}
    for line in (build / 'CMakeFiles/sim-app-runtime.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            k, v = line.split(' = ', 1)
            flags[k] = shlex.split(v)
    link = shlex.split((build / 'CMakeFiles/sim-app-runtime.dir/link.txt').read_text())
    obj = work / 'driver.o'
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'], '-c', str(work / 'driver.c'), '-o', str(obj)], cwd=build, check=True)
    link = [str(obj) if x.endswith('tests/app_runtime/app.c.o') else x for x in link]
    if a.mutant == 'child':
        registry = (ROOT / 'system/ui/app_registry.c').read_text()
        guard = 'if (ink_member_current()->role == INK_ROLE_CHILD && (!def->child_ok'
        assert registry.count(guard) == 1
        (work / 'registry.c').write_text(registry.replace(guard, 'if (false && (!def->child_ok'))
        registry_obj = work / 'registry.o'
        subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'], '-c', str(work / 'registry.c'), '-o', str(registry_obj)], cwd=build, check=True)
        link.insert(1, str(registry_obj))
    binary = work / 'sim-app-runtime'
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)

sd = work / 'sd/content'
package = sd / 'apps/units'
shutil.copytree(ROOT / 'examples/units', package)
manifest = json.loads((package / 'manifest.json').read_text())
manifest['pages'].append('controls')
manifest['permissions'] += ['test.runtime_echo', 'synthetic']
(package / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False))
controls = {'title': '合成控件', 'nodes': [
    {'id': 'denied', 'type': 'button', 'label': '未声明动作', 'capability': 'app.cards'},
    {'id': 'switch', 'type': 'toggle', 'label': '合成开关', 'value': False, 'capability': 'test.runtime_echo', 'args': {'on': '$switch'}, 'from': 'on'},
    {'id': 'slider', 'type': 'slider', 'label': '合成滑杆', 'value': 2, 'capability': 'test.runtime_echo', 'args': {'level': '$slider'}, 'from': 'level'},
    {'id': 'segment', 'type': 'segments', 'label': '合成分段', 'options': ['甲', '乙'], 'value': 0, 'capability': 'test.runtime_echo', 'args': {'choice': '$segment'}},
    {'id': 'row', 'type': 'list', 'label': '合成列表行', 'value': '合成内容'}]}
(package / 'controls.json').write_text(json.dumps(controls, ensure_ascii=False))
shutil.copytree(package, sd / '.m/owner/SYNTH_package')
# A complete otherwise valid manifest with an id that disagrees with its directory.
shutil.copytree(package, sd / 'apps/bad')
bad = {**manifest, 'id': 'imposter'}
(sd / 'apps/bad/manifest.json').write_text(json.dumps(bad))
shutil.copytree(package, sd / 'apps/broken')
(sd / 'apps/broken/manifest.json').write_text(json.dumps({**manifest, 'id': 'broken'}))
(sd / 'apps/broken/main.json').write_text('{"title":"坏页","nodes":[{"id":"bad","type":"script","label":"禁止脚本"}]}')
script = work / 'scenario.script'
script.write_text('@0 net wifi\n@180s end\n')
result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'), '--script', str(script),
    '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(work / 'out'),
    '--flash-image', str(work / 'flash.img'), '--sd-dir', str(work / 'sd'), '--quiet', '--no-random-outages',
    '--budget', str(ROOT / 'tests/ui_scene.budget.json')], capture_output=True, text=True, timeout=180)
(work / 'run.log').write_text(result.stdout + result.stderr)
data = json.loads((work / 'out/metrics.json').read_text()) if (work / 'out/metrics.json').exists() else {}
metrics = data.get('metrics', {})
passed = result.returncode == 0 and metrics.get('test.finished') == 1 and not metrics.get('test.fail', 0)
print('app-runtime', a.mutant or 'positive', 'PASS' if passed else 'FAIL', 'exit', result.returncode,
      'checks', metrics.get('test.pass', 0), 'failures', metrics.get('test.fail', 0), flush=True)
(work / 'summary.json').write_text(json.dumps({'passed': passed, 'exit': result.returncode, 'checks': metrics.get('test.pass', 0),
    'failures': metrics.get('test.fail', 0), 'warnings': data.get('warnings', [])}, ensure_ascii=False, indent=2))
if not passed:
    print(result.stderr[-6500:], data.get('warnings', [])[-25:], flush=True)
if passed and not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(work)
else:
    print('Evidence:', work.relative_to(ROOT), flush=True)
raise SystemExit(0 if passed else 1)
