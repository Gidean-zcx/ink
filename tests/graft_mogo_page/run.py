"""Exercise real Mogo pages in the headless product simulator, with local book fixtures."""
import argparse
import hashlib
import re
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]

def provenance():
    vendor = ROOT / 'third_party/mogo'
    manifest = json.loads((vendor/'page-upstream-files.json').read_text())
    subset = (vendor/'main/apps/ink_book_page.c').read_text()
    for entry in manifest['files']:
        if 'path' in entry:
            data = (vendor/entry['path']).read_bytes()
        else:
            match = re.search(r'^static [^\n]*\b'+entry['function']+r'\([^;]*?\)\s*\{',subset,re.M)
            assert match, entry['function']
            end = subset.index('\n}\n',match.end())+3
            data = subset[match.start():end].encode()
        assert hashlib.sha256(data).hexdigest() == entry['sha256'], entry


def content(build):
    root = build / 'mogo-content'
    member = root / '.m/owner'
    (member / 'books').mkdir(parents=True, exist_ok=True)
    (member / 'fonts').mkdir(exist_ok=True)
    source = (ROOT / 'tests/fixtures/content/books/星星邮差.txt').read_text()
    # Retain the original story; repeat its paragraphs to provide ten full native pages.
    first_chapter, rest = source.split('第二章', 1)
    title, body = first_chapter.split('\n', 1)
    text = title + '\n' + body * 20 + '\n第二章' + rest
    (member / 'books/星星邮差GBK.txt').write_bytes(text.encode('gbk'))
    with zipfile.ZipFile(member / 'books/星星邮差.epub', 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('mimetype', 'application/epub+zip', compress_type=zipfile.ZIP_STORED)
        z.writestr('META-INF/container.xml', '<container><rootfiles><rootfile full-path="book.opf"/></rootfiles></container>')
        z.writestr('book.opf', '<package xmlns="http://www.idpf.org/2007/opf" version="2.0"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>星星邮差</dc:title></metadata><manifest><item id="c1" href="chapter.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="c1"/></spine></package>')
        z.writestr('chapter.xhtml', '<html><body>' + ''.join('<p>'+p+'</p>' for p in text.splitlines() if p) + '</body></html>')
    font = member / 'fonts/InkSC.ttf'
    if not font.exists():
        source_font = next(line.split('=', 1)[1] for line in (build/'CMakeCache.txt').read_text().splitlines() if line.startswith('INK_CJK_FONT:'))
        python = next((p for p in dict.fromkeys([sys.executable, shutil.which('python3')]) if p and subprocess.run([p,'-c','import fontTools'],capture_output=True).returncode == 0), None)
        assert python, 'fontTools is required for the real Mogo page test'
        # mogo-page, mogo-flow and the reader tests share this folder and may run at once: each makes its own copy beside it
        # and renames it into place, so none reads a font another is still writing.
        scratch = build / f'mogo-content.{os.getpid()}'
        shutil.rmtree(scratch, ignore_errors=True); scratch.mkdir()
        made = scratch / font.name
        subprocess.run([python,str(ROOT/'tools/mogo_font.py'),source_font,str(made)],check=True,capture_output=True)
        os.replace(made.with_suffix('.OFL.txt'), font.with_suffix('.OFL.txt'))
        os.replace(made, font)
        shutil.rmtree(scratch)
    return root

def exercise(build, binary=None, reverse=False):
    provenance()
    root = content(build)
    area = build / 'test-out' / ('mogo-page-reverse' if reverse else 'mogo-page')
    if area.exists(): shutil.rmtree(area)
    area.mkdir(parents=True)
    log = (area/'run.log').open('w')
    try:
        result = subprocess.run([str(binary or build/'sim-mogo-page'),'--profile',str(ROOT/'simulator/profiles/metalio-plus.json'),
        '--script',str(ROOT/'tests/graft_mogo_page/scenario.script'),'--budget',str(ROOT/'tests/ui_scene.budget.json'),
        '--assets',str(build/'assets'),'--content',str(root),'--out',str(area/'out'),
        '--flash-image',str(area/'user.img'),'--sd-dir',str(area/'sd'),'--no-random-outages'],
        cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,text=True,timeout=15 if reverse == 'status' else 360,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
    except subprocess.TimeoutExpired:
        log.close()
        output = (area/'run.log').read_text(errors='replace')
        assert reverse == 'status' and 'Mogo: closing' in output and 'Mogo: closed' not in output, output[-4000:]
        print('reverse: stale status-bar ownership stalls reader close, as required', flush=True)
        return
    log.close()
    output = (area/'run.log').read_text(errors='replace')
    data = json.loads((area/'out/metrics.json').read_text(errors='replace')) if (area/'out/metrics.json').exists() else {}
    counts = data.get('metrics',{})
    if reverse:
        assert result.returncode and counts.get('test.fail') and any('forward input reaches native page' in str(w) for w in data.get('warnings',[])), output[-4000:]
        print(f'reverse: disconnected page input failed as required, exit={result.returncode}',flush=True)
    else:
        assert result.returncode == 0 and counts.get('test.finished') == 1 and not counts.get('test.fail'), f'exit={result.returncode}, passed={counts.get("test.pass")}, failed={counts.get("test.fail")}, finished={counts.get("test.finished")}\n{output[-5500:]}\n{data.get("warnings", [])}'
        print(f'Mogo native page: PASS ({counts.get("test.pass")} assertions); GBK +10/-10 and EPUB',flush=True)
    if not os.environ.get('INK_KEEP_EVIDENCE'): shutil.rmtree(area)

if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('--build',type=Path,required=True)
    a=p.parse_args(); exercise(a.build.resolve())
