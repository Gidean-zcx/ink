"""Whole-product reader settings, restart, 120% frames, and a source-local reverse control."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
PYTHON = __import__('sys').executable


def exercise(build, binary=None, reverse=False):
    area = build / 'test-out' / ('reader-settings-reverse' if reverse else 'reader-settings')
    if area.exists():
        shutil.rmtree(area)
    content = area / 'content'
    shutil.copytree(build / 'test-content', content)  # with the reader's font
    for path in (ROOT / 'tests/reader_epub/fixtures').iterdir():
        shutil.copyfile(path, content / 'books' / path.name)
    # Extend the synthetic second chapter: every tested layout must have several pages.
    fixture = content / 'books' / 'epub2.epub'
    with zipfile.ZipFile(fixture) as source:
        entries = [(info, source.read(info)) for info in source.infolist()]
    with zipfile.ZipFile(fixture, 'w') as target:
        for info, data in entries:
            if info.filename == 'OPS/c2.xhtml':
                paragraphs = ''.join(f'<p>海边段落{i}。我们看着灯塔，记录海风经过沙岸的声音，把一路见闻写在纸上。</p>' for i in range(160))
                data = data.replace(b'</body>', paragraphs.encode() + b'</body>')
            target.writestr(info, data)
    for phase in (('read',) if reverse else ('read', 'restart', 'large-text')):
        out = area / phase
        result = subprocess.run([str(binary or build / 'sim-reader-settings'), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
            '--script', str(ROOT / 'tests/reader_settings/scenario.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
            '--assets', str(build / 'assets'), '--content', str(content), '--out', str(out),
            '--flash-image', str(area / ('large.img' if phase == 'large-text' else 'user.img')),
            '--sd-dir', str(area / 'sd'), '--no-random-outages', '--quiet'], cwd=ROOT, capture_output=True, text=True, timeout=400,
            env={**os.environ, 'INK_READER_SETTINGS_PHASE': phase, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        (area / f'{phase}.log').write_text(result.stdout + result.stderr)
        data = json.loads((out / 'metrics.json').read_text(errors='replace')) if (out / 'metrics.json').exists() else {}
        counts = data.get('metrics', {})
        if reverse:
            assert result.returncode and counts.get('test.fail') and any('layout changes body raster' in str(w) for w in data.get('warnings', [])), data.get('warnings', [])
            print(f'reverse: expected failure exit={result.returncode}, failed assertions={counts["test.fail"]}', flush=True)
        elif result.returncode or counts.get('test.finished') != 1 or counts.get('test.fail', 0):
            raise SystemExit(f'{phase}: FAIL exit={result.returncode}, finished={counts.get("test.finished")}\n{result.stderr[-5000:]}\n{data.get("warnings", [])}')
        else:
            print(f'{phase}: PASS assertions={counts.get("test.pass")}', flush=True)
    if not os.environ.get('INK_KEEP_EVIDENCE'):
        shutil.rmtree(area)


def control(build):
    # The layout change that ignores its values: the page never reflows.
    source = ROOT / 'third_party/mogo/main/apps/ink_book_page.c'
    header = ROOT / 'apps/mogo/port/ink_page_exports.h'
    entry = next(x for x in json.loads((build / 'compile_commands.json').read_text()) if Path(x['file']) == source)
    with tempfile.TemporaryDirectory(prefix='reader-settings-control-', dir=build) as directory:
        temp = Path(directory)
        mutant = temp / source.name
        obj = temp / (source.name + '.o')
        original = header.read_text()
        changed = original.replace('v[i]=values[i]==INK_MOGO_UNSET?ink_mogo_setting_value(i):values[i];', 'v[i]=values[i]==INK_MOGO_UNSET || 1?ink_mogo_setting_value(i):values[i];')
        assert changed != original
        (temp / header.name).write_text(changed)
        mutant.write_text(source.read_text())
        args = shlex.split(entry['command'])
        compile_args, skip = [], False
        for arg in args:
            if skip:
                skip = False
            elif arg in ('-o', '-MT', '-MF', '-MQ'):
                skip = True
            elif arg != '-MD':
                compile_args.append(str(mutant) if arg == str(source) else arg)
        subprocess.run(compile_args + ['-o', str(obj)], cwd=entry['directory'], check=True, capture_output=True)
        library = temp / 'libink_mogo_native.a'
        shutil.copyfile(build / library.name, library)
        subprocess.run(['ar', 'r', str(library), str(obj)], check=True, capture_output=True)
        binary = temp / 'sim-reader-settings'
        link = shlex.split((build / 'CMakeFiles/sim-reader-settings.dir/link.txt').read_text())
        link[link.index('-o') + 1] = str(binary)
        link = [str(library) if Path(x).name == library.name else x for x in link]
        subprocess.run(link, cwd=build, check=True, capture_output=True)
        exercise(build, binary, reverse=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--reverse', action='store_true')
    args = parser.parse_args()
    (control if args.reverse else exercise)(args.build.resolve())
