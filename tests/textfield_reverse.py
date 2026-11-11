"""Remove individual field fixes in temporary objects and require the real UI test to fail."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'textfield-reverse'
area.mkdir(exist_ok=True)
flags = {}
for line in (build / 'CMakeFiles/ink_system.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
source = (ROOT / 'system/ui/textfield.c').read_text()
variants = {
    'legacy-size': ('        lv_obj_set_width(f->obj, LV_PCT(100));\n        lv_obj_set_height(f->obj, LV_SIZE_CONTENT);', '', 'same style ignores legacy dimensions'),
    'large-text': ('lv_obj_set_height(f->area, ink_ui_px(height));', 'lv_obj_set_height(f->area, ink_ui_px(INK_FIELD_HEIGHT));', '120% enlarges both label and input'),
}
for name, (old, new, expected) in variants.items():
    assert source.count(old) == 1, name
    path, obj = area / (name + '.c'), area / (name + '.o')
    path.write_text(source.replace(old, new))
    link = shlex.split((build / 'CMakeFiles/sim-ui-input-host.dir/link.txt').read_text())
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                    '-c', str(path), '-o', str(obj)], cwd=build, check=True)
    link[1:1] = [str(obj)]
    binary = area / name
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
    out = area / (name + '-out')
    run = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/ui_input_host.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--quiet', '--no-random-outages'], cwd=ROOT, capture_output=True, text=True, timeout=90)
    (area / (name + '.log')).write_text(run.stdout + run.stderr)
    data = json.loads((out / 'metrics.json').read_text())
    assert run.returncode != 0 and any(expected in line for line in data.get('warnings', [])), (name, run.returncode, data.get('warnings'))
    print(name, 'expected failure', run.returncode, 'checks failed', data['metrics'].get('test.fail', 0), flush=True)
