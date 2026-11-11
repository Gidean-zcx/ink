"""Inbox, source bookmarks, restart and 120% screenshots; no device or network."""
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


def exercise(build, binary=None, reverse=None):
    area = build / 'test-out' / ('reader-more-' + reverse if reverse else 'reader-more')
    if area.exists():
        shutil.rmtree(area)
    content = area / 'content'
    (content / 'books').mkdir(parents=True)
    (content / 'books/共同读本.txt').write_text('共同读本。我们一起读一段原创文字。\n' * 80)
    with zipfile.ZipFile(ROOT / 'tests/reader_epub/fixtures/epub2.epub') as source, zipfile.ZipFile(content / 'incoming.epub', 'w') as target:
        for info in source.infolist():
            data = source.read(info)
            if info.filename == 'OPS/book.opf':
                data = data.replace('山间来信'.encode(), '收件箱来信'.encode())
            if info.filename == 'OPS/c2.xhtml':
                data = data.replace(b'</body>', ''.join(f'<p>海边段落{i}。我们看着灯塔，记录海风经过沙岸的声音，把一路见闻写在纸上。</p>' for i in range(160)).encode() + b'</body>')
            target.writestr(info, data)
    for phase in (('large',) if reverse == 'layout' else ('read',) if reverse else ('read', 'restart', 'large')):
        out = area / phase
        result = subprocess.run([str(binary or build / 'sim-reader-more'), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
            '--script', str(ROOT / 'tests/reader_settings/scenario.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
            '--assets', str(build / 'assets'), '--content', str(content), '--out', str(out),
            '--flash-image', str(area / ('large.img' if phase == 'large' else 'user.img')), '--sd-dir', str(area / 'sd'), '--no-random-outages', '--quiet'],
            cwd=ROOT, capture_output=True, text=True, timeout=400,
            env={**os.environ, 'INK_READER_MORE_PHASE': phase, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        (area / f'{phase}.log').write_text(result.stdout + result.stderr)
        data = json.loads((out / 'metrics.json').read_text(errors='replace')) if (out / 'metrics.json').exists() else {}
        counts = data.get('metrics', {})
        if reverse:
            expected = 'inbox books appear' if reverse == 'shelf' else 'bookmark delete label fits' if reverse == 'layout' else 'touch adds current page mark'
            assert result.returncode and counts.get('test.fail') and any(expected in str(w) for w in data.get('warnings', [])), data.get('warnings', [])
            print(f'{reverse} reverse: expected failure exit={result.returncode}, failed assertions={counts["test.fail"]}', flush=True)
        elif result.returncode or counts.get('test.finished') != 1 or counts.get('test.fail', 0):
            raise SystemExit(f'{phase}: FAIL exit={result.returncode}, finished={counts.get("test.finished")}\n{result.stderr[-5000:]}\n{data.get("warnings", [])}')
        else:
            print(f'{phase}: PASS assertions={counts.get("test.pass")}', flush=True)
    if not os.environ.get('INK_KEEP_EVIDENCE'):
        shutil.rmtree(area)


def control(build):
    source = ROOT / 'apps/reader/reader.c'
    entry = next(x for x in json.loads((build / 'compile_commands.json').read_text()) if Path(x['file']) == source)
    for name, before, after in [('shelf', 'inbox<2', 'inbox<1'), ('bookmark', 'ok=ink_bookmarks_add(&MARK);', 'ok=false;'),
            ('layout', 'delete_w=ink_ui_text_width(TR("删除"),INK_FONT_LIST,INK_WEIGHT_MEDIUM)+2*INK_GAP', 'delete_w=INK_CONTROL')]:
        with tempfile.TemporaryDirectory(prefix='reader-more-control-', dir=build) as directory:
            temp = Path(directory)
            mutant, obj = temp / source.name, temp / (source.name + '.o')
            original = source.read_text()
            assert before in original
            mutant.write_text(original.replace(before, after, 1))
            args, skip = [], False
            for arg in shlex.split(entry['command']):
                if skip:
                    skip = False
                elif arg in ('-o', '-MT', '-MF', '-MQ'):
                    skip = True
                elif arg != '-MD':
                    args.append(str(mutant) if arg == str(source) else arg)
            subprocess.run(args + ['-I', str(source.parent), '-o', str(obj)], cwd=entry['directory'], check=True, capture_output=True)
            library = temp / 'libink_apps.a'
            shutil.copyfile(build / library.name, library)
            subprocess.run(['ar', 'r', str(library), str(obj)], check=True, capture_output=True)
            binary = temp / 'sim-reader-more'
            link = shlex.split((build / 'CMakeFiles/sim-reader-more.dir/link.txt').read_text())
            link[link.index('-o') + 1] = str(binary)
            link = [str(library) if Path(x).name == library.name else x for x in link]
            subprocess.run(link, cwd=build, check=True, capture_output=True)
            exercise(build, binary, name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--reverse', action='store_true')
    args = parser.parse_args()
    (control if args.reverse else exercise)(args.build.resolve())
