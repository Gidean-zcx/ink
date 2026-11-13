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
import zipfile

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


def review_content(build, area):
    """A shelf longer than 4 KB as JSON and an EPUB with 300 long contents entries, with quotes, backslashes and a tab in
    what the AI is given (synthetic text)."""
    root = content(build, area)
    me = root / '.m/owner/books'
    for i in range(70):
        (me / f'把书架列表撑过四千字节第{i:02d}本.txt').write_text(f'第{i}本书的一段原创文字。\n' * 20)
    with zipfile.ZipFile(me / '目录很长.epub', 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('mimetype', 'application/epub+zip', compress_type=zipfile.ZIP_STORED)
        z.writestr('META-INF/container.xml', '<container><rootfiles><rootfile full-path="book.opf"/></rootfiles></container>')
        items = ''.join(f'<item id="c{i}" href="c{i}.xhtml" media-type="application/xhtml+xml"/>' for i in range(300))
        spine = ''.join(f'<itemref idref="c{i}"/>' for i in range(300))
        z.writestr('book.opf', '<package xmlns="http://www.idpf.org/2007/opf" version="3.0"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/">'
                   '<dc:title>目录很长的"书"\\测试</dc:title></metadata><manifest><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>'
                   + items + '</manifest><spine>' + spine + '</spine></package>')
        # Long titles (about 90 bytes): 24 bookmark labels pass 4 KB as JSON; the tab is read as a space (whitespace)
        nav = ''.join(f'<li><a href="c{i}.xhtml">第{i + 1}节 一个很长很长的章节标题用来撑长书签 "引号"\\与\t制表</a></li>' for i in range(300))
        z.writestr('nav.xhtml', '<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops"><body><nav epub:type="toc"><ol>' + nav + '</ol></nav></body></html>')
        for i in range(300):
            z.writestr(f'c{i}.xhtml', f'<html><body><h1>第{i + 1}节</h1>' + f'<p>第{i + 1}节的原创正文。</p>' * 8 + '</body></html>')
    return root


def results(area, phase):
    """The whole results the device printed: each must be valid JSON."""
    found = {}
    for line in (area / f'{phase}.log').read_text(errors='replace').splitlines():
        if line.startswith('READER-ONE-RESULT '):
            _, cap, size, text = line.split(' ', 3)
            assert len(text.encode()) == int(size), (cap, size, len(text.encode()))
            found[cap] = json.loads(text)
    return found


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


def mutant(build, area, source, before, after, target, library_name='libink_apps.a', more=()):
    """Rebuild the test binary with a change in the product (more: further (source, before, after) in the same
    library), for a reverse control."""
    temp = Path(tempfile.mkdtemp(prefix='reader-one-control-', dir=area))
    library = temp / library_name
    shutil.copyfile(build / library.name, library)
    commands = json.loads((build / 'compile_commands.json').read_text())
    for source, before, after in ((source, before, after), *more):
        entry = next(x for x in commands if Path(x['file']) == source)
        original = source.read_text()
        assert before in original, before
        changed = temp / source.name
        changed.write_text(original.replace(before, after))
        obj = temp / (source.name + '.o')
        args, skip = [], False
        for arg in shlex.split(entry['command']):
            if skip: skip = False
            elif arg in ('-o', '-MT', '-MF', '-MQ'): skip = True
            elif arg != '-MD': args.append(str(changed) if arg == str(source) else arg)
        subprocess.run(args + ['-I', str(source.parent), '-o', str(obj)], cwd=entry['directory'], check=True, capture_output=True)
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
    p.add_argument('--phase', required=True, choices=['states', 'bookmarks', 'migrate', 'migrate-reverse', 'review', 'late'])
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
    elif a.phase == 'review':
        passed = run(build, binary, area, review_content(build, area), 'review', 'review.img')[0]
        try:
            found = results(area, 'review')
            shelf, toc, marks = found['reader.list_books'], found['reader.chapters'], found['reader.bookmarks']
            assert len(shelf) >= 70 and len(toc['entries']) == 300 and len(marks['bookmarks']) == 24, (len(shelf), len(toc['entries']), len(marks['bookmarks']))
            assert toc['title'] == '目录很长的"书"\\测试' and toc['entries'][0]['title'] == '第1节 一个很长很长的章节标题用来撑长书签 "引号"\\与 制表', (toc['title'], toc['entries'][0])
            assert all('"引号"' in m['label'] for m in marks['bookmarks']), marks['bookmarks'][0]
            print('results: whole and valid JSON (shelf %d, contents %d, bookmarks %d)' % (len(shelf), len(toc['entries']), len(marks['bookmarks'])), flush=True)
        except (AssertionError, KeyError, ValueError) as e:
            print('results: NOT VALID', repr(e)[:400], flush=True)
            passed = False
        ok &= passed
    elif a.phase == 'late':
        ok &= run(build, binary, area, content(build, area), 'late-control', 'late-control.img')[0]
        ok &= run(build, binary, area, content(build, area), 'late', 'late.img')[0]
        # Reverse control: the carrier lets a session's late write through for a member who is not current, and the member
        # table lets writers in for a member being removed: the same checks must see the write.
        guard = ('    if (!owner_current(owner) || !app || !def || !def->name || def->scope != INK_DB_MEMBER) return NULL;\n'
                 '    size_t n = strlen(app);\n    if (!n || strncmp(def->name, app, n) || def->name[n] != \'.\') return NULL;\n'
                 '    ink_db_t *db = ink_db_open(def, owner->member);\n    if (db && !owner_current(owner)) {')
        control = mutant(build, area, ROOT / 'system/graft/graft.c', guard, guard.replace('!owner_current(owner) || ', '').replace('db && !owner_current(owner)', '0'),
                         'sim-reader-one', 'libink_system.a', more=[(ROOT / 'system/policy/members.c',
                         'bool ok = s && ink_member_exists(id) && s->writers < UINT16_MAX;', 'bool ok = s && s->writers < UINT16_MAX;')])
        passed, failures = run(build, control, area, content(build, area), 'late', 'late-reverse.img')
        reverse = not passed and any('wrote nothing' in f for f in failures)
        print('reverse (late write let through):', 'fails as required' if reverse else 'DID NOT FAIL', flush=True)
        ok &= reverse
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
