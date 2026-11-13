"""reader-one-1: explanation pages, the three book places and back paths, bookmarks, and the old reader's data
moving over at boot (two members, a second boot, a restored older backup, the card out at boot). Synthetic content."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def content(build, area, fonts=True, books=True):
    root = area / 'content'
    if root.exists(): shutil.rmtree(root)
    shutil.copytree(build / 'test-content', root)
    if not fonts: shutil.rmtree(root / 'fonts')
    if not books:
        shutil.rmtree(root / 'books'); (root / 'books').mkdir()
    else:
        (root / '.m/owner/books').mkdir(parents=True, exist_ok=True)
        (root / '.m/owner/books/我的小书.txt').write_text('第一章 开头\n' + '我的小书里的一段原创文字。\n' * 300 + '第二章 后来\n' + '后来的一段原创文字。\n' * 200)
        (root / '.m/owner/收件箱').mkdir(parents=True, exist_ok=True)
        (root / '.m/owner/收件箱/收件箱的书.txt').write_text('传进来的一本原创书。\n' * 50)
        shutil.copyfile(ROOT / 'tests/reader_epub/fixtures/epub2.epub', root / '.m/owner/收件箱/来信.epub')
    return root


def offsets(build):
    """Byte offsets of unique lines in the long family book: the old reader's positions are bytes in the file."""
    data = (build / 'test-content/books/测试长书.txt').read_bytes()
    marks = {}
    for i, part in enumerate((3, 7, 11, 15, 19, 23, 31), 1):
        head = f'【测试长书 第{part}部分】'.encode()
        marks[i] = (data.index(head), head.decode())
    return {'INK_ONE_LAST': str(marks[6][0]), 'INK_ONE_EXCERPT_LAST': marks[6][1], 'INK_ONE_KID': str(marks[7][0]), 'INK_ONE_EXCERPT_KID': marks[7][1],
            **{f'INK_ONE_MARK{i}': str(marks[i][0]) for i in range(1, 6)}, **{f'INK_ONE_EXCERPT{i}': marks[i][1] for i in range(1, 6)}}


def run(build, binary, area, root, phase, image, script='tests/kid_3.script', extra=None):
    out = area / phase
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / script), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(root), '--out', str(out),
        '--flash-image', str(area / image), '--sd-dir', str(area / ('sd-' + image)), '--no-random-outages', '--quiet'],
        cwd=ROOT, capture_output=True, text=True, timeout=500,
        env={**os.environ, 'INK_READER_ONE_PHASE': phase, 'UBSAN_OPTIONS': 'halt_on_error=1', **(extra or {})})
    (area / f'{phase}.log').write_text(result.stdout + result.stderr)
    data = json.loads((out / 'metrics.json').read_text(errors='replace')) if (out / 'metrics.json').exists() else {}
    counts = data.get('metrics', {})
    failures = [str(w) for w in data.get('warnings', []) if 'CHECK FAILED' in str(w)]
    passed = result.returncode == 0 and counts.get('test.finished') == 1 and not counts.get('test.fail', 0)
    print(f'{phase}: {"PASS" if passed else "FAIL"} checks={counts.get("test.pass")} failures={counts.get("test.fail", 0)} exit={result.returncode}', flush=True)
    for f in failures[:20]: print('  ', f, flush=True)
    if not passed and not failures: print(result.stderr[-3000:], flush=True)
    return passed, failures


def mutant(build, area, source, before, after, target):
    """Rebuild the test binary with one change in the product, for a reverse control."""
    entry = next(x for x in json.loads((build / 'compile_commands.json').read_text()) if Path(x['file']) == source)
    original = source.read_text()
    assert before in original, before
    temp = Path(tempfile.mkdtemp(prefix='reader-one-control-', dir=area))
    changed = temp / source.name
    changed.write_text(original.replace(before, after))
    obj = temp / (source.name + '.o')
    args, skip = [], False
    for arg in shlex.split(entry['command']):
        if skip: skip = False
        elif arg in ('-o', '-MT', '-MF', '-MQ'): skip = True
        elif arg != '-MD': args.append(str(changed) if arg == str(source) else arg)
    subprocess.run(args + ['-I', str(source.parent), '-o', str(obj)], cwd=entry['directory'], check=True, capture_output=True)
    library = temp / 'libink_apps.a'
    shutil.copyfile(build / library.name, library)
    subprocess.run(['ar', 'r', str(library), str(obj)], check=True, capture_output=True)
    binary = temp / target
    link = shlex.split((build / f'CMakeFiles/{target}.dir/link.txt').read_text())
    link[link.index('-o') + 1] = str(binary)
    link = [str(library) if Path(x).name == library.name else x for x in link]
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    return binary


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--phase', required=True, choices=['states', 'bookmarks', 'migrate', 'migrate-reverse'])
    a = p.parse_args()
    build = a.build.resolve()
    area = build / 'test-out' / f'reader-one-{a.phase}'
    shutil.rmtree(area, ignore_errors=True); area.mkdir(parents=True)
    binary = build / 'sim-reader-one'
    ok = True
    if a.phase == 'states':
        ok &= run(build, binary, area, content(build, area, fonts=False), 'nofont', 'nofont.img')[0]
        # Reverse control: without the explanation page the same checks must fail.
        control = mutant(build, area, ROOT / 'apps/mogo/mogo.c', '    if (!M.reason || !M.back_button) return;', '    return;', 'sim-reader-one')
        passed, failures = run(build, control, area, content(build, area, fonts=False), 'nofont', 'nofont-reverse.img')
        reverse = not passed and any('not a blank page' in f for f in failures)
        print('reverse (no explanation page):', 'fails as required' if reverse else 'DID NOT FAIL', flush=True)
        ok &= reverse
        ok &= run(build, binary, area, content(build, area), 'nocard', 'nocard.img', 'tests/reader_one/nocard.script')[0]
        ok &= run(build, binary, area, content(build, area, books=False), 'empty', 'empty.img')[0]
        ok &= run(build, binary, area, content(build, area), 'places', 'places.img')[0]
    elif a.phase == 'bookmarks':
        ok &= run(build, binary, area, content(build, area), 'bookmarks', 'marks.img')[0]
    else:
        extra = offsets(build)
        if a.phase == 'migrate-reverse':
            binary = mutant(build, area, ROOT / 'apps/mogo/mogo.c', 'int more = ink_mogo_migrate_step(&C.owner, NULL);', 'int more = 0;', 'sim-reader-one')
            binary2 = binary
        root = content(build, area)
        for phase, image in (('seed', 'user.img'), ('boot', 'user.img')) if a.phase == 'migrate-reverse' else (
                ('seed', 'user.img'), ('boot', 'user.img'), ('again', 'user.img'), ('seed-extra', 'user.img'), ('restored', 'user.img'),
                ('seed', 'nocard.img'), ('boot-nocard', 'nocard.img')):
            script = 'tests/reader_one/nocard.script' if phase == 'boot-nocard' else 'tests/kid_3.script'
            passed, failures = run(build, binary, area, root, phase, image, script, extra)
            if a.phase == 'migrate-reverse' and phase == 'boot':
                reverse = not passed and any('continues the old book' in f for f in failures)
                print('reverse (no migration):', 'fails as required' if reverse else 'DID NOT FAIL', flush=True)
                ok &= reverse
            else:
                ok &= passed
    if ok and not os.environ.get('INK_KEEP_EVIDENCE'): shutil.rmtree(area)
    elif not ok: print('Evidence:', area, flush=True)
    raise SystemExit(0 if ok else 1)


if __name__ == '__main__':
    main()
