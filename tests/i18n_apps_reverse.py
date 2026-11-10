"""Manual reverse controls: remove translation/layout fixes in temporary objects, never in source files."""
import argparse
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
commands = json.loads((build / 'compile_commands.json').read_text())


def remove_block(text, opening):
    start = text.index(opening)
    left = text.index('{', start)
    end, depth = left + 1, 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[:start] + text[end:]


def mutate(case, text):
    if case == 'calculator':
        return text.replace('K.error ? TR(K.result) : K.result', 'K.result')
    if case == 'phone':
        return remove_block(text, 'if (strcmp(ink_lang_code(), "zh"))')
    text = text.replace('i ? TR("%s · %s")', 'i ? "%s · %s"')
    text = text.replace('strcmp(ink_lang_code(), "zh") ? 2 * ink_ui_line_height(INK_FONT_LIST) : INK_CONTROL', 'INK_CONTROL')
    return remove_block(text, 'if (i && strcmp(ink_lang_code(), "zh"))')


for case in ('calculator', 'phone', 'weather'):
    source = ROOT / f'system/apps/{case}/{case}.c'
    entry = next(x for x in commands if Path(x['file']) == source)
    with tempfile.TemporaryDirectory(prefix='i18n-reverse-', dir=build) as temp:
        temp = Path(temp)
        original = source.read_text()
        mutant = temp / source.name
        changed = mutate(case, original)
        if changed == original:
            raise SystemExit(f'{case}: no fix removed')
        mutant.write_text(changed)
        obj = temp / (source.name + '.o')
        args = shlex.split(entry['command']) if 'command' in entry else entry['arguments']
        compile_args, skip = [], False
        for item in args:
            if skip:
                skip = False
                continue
            if item in ('-o', '-MT', '-MF', '-MQ'):
                skip = True
            elif item != '-MD':
                compile_args.append(str(mutant) if item == str(source) else item)
        subprocess.run(compile_args + ['-o', str(obj)], cwd=entry['directory'], check=True, capture_output=True)
        library = temp / 'libink_system.a'
        shutil.copyfile(build / library.name, library)
        subprocess.run(['ar', 'r', str(library), str(obj)], check=True, capture_output=True)
        binary = temp / 'sim-i18n-apps'
        link = shlex.split((build / 'CMakeFiles/sim-i18n-apps.dir/link.txt').read_text())
        link[link.index('-o') + 1] = str(binary)
        link = [str(library) if Path(x).name == library.name else x for x in link]
        subprocess.run(link, cwd=build, check=True, capture_output=True)
        out = build / 'test-out' / 'i18n-reverse' / case
        if out.exists():
            shutil.rmtree(out)
        import os
        run = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
            '--script', str(ROOT / 'tests/i18n_apps.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
            '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
            '--quiet', '--no-random-outages'], cwd=ROOT, capture_output=True, text=True, timeout=300,
            env={**os.environ, 'INK_I18N_APPS_LANG': 'en', 'UBSAN_OPTIONS': 'halt_on_error=1'})
        data = json.loads((out / 'metrics.json').read_text())
        failures = data['metrics'].get('test.fail', 0)
        expected = {'calculator': 'Calculator-error', 'phone': 'Phone:', 'weather': 'Weather:'}[case]
        if not run.returncode or not failures or not any(expected in w for w in data.get('warnings', [])):
            raise SystemExit(f'{case}: reverse control did not catch the missing fix')
        (out / 'stderr.log').write_text(run.stderr)
        print(f'{case}: expected failure, exit={run.returncode}, assertions={failures}', flush=True)
