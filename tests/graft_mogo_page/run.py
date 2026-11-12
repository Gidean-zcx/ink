"""Exercise real Mogo pages in the headless product simulator, with local book fixtures."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]

def content(build):
    root = build / 'mogo-content'
    member = root / '.m/owner'
    (member / 'books').mkdir(parents=True, exist_ok=True)
    (member / 'fonts').mkdir(exist_ok=True)
    source = (ROOT / 'tests/fixtures/content/books/星星邮差.txt').read_text()
    # Retain the original story; repeat its paragraphs to provide ten full native pages.
    title, body = source.split('\n', 1)
    body = body.replace('第二章', '第二段').replace('第三章', '第三段').replace('第四章', '第四段')
    text = title + '\n' + body * 12
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
        subprocess.run([python,str(ROOT/'tools/mogo_font.py'),source_font,str(font)],check=True,capture_output=True)
    return root

def exercise(build, binary=None, reverse=False):
    root = content(build)
    area = build / 'test-out' / ('mogo-page-reverse' if reverse else 'mogo-page')
    if area.exists(): shutil.rmtree(area)
    area.mkdir(parents=True)
    result = subprocess.run([str(binary or build/'sim-mogo-page'),'--profile',str(ROOT/'simulator/profiles/metalio-plus.json'),
        '--script',str(ROOT/'tests/graft_mogo_page/scenario.script'),'--budget',str(ROOT/'tests/ui_scene.budget.json'),
        '--assets',str(build/'assets'),'--content',str(root),'--out',str(area/'out'),
        '--flash-image',str(area/'user.img'),'--sd-dir',str(area/'sd'),'--no-random-outages','--quiet'],
        cwd=ROOT,capture_output=True,text=True,timeout=360,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
    (area/'run.log').write_text(result.stdout+result.stderr)
    data = json.loads((area/'out/metrics.json').read_text()) if (area/'out/metrics.json').exists() else {}
    counts = data.get('metrics',{})
    if reverse:
        assert result.returncode and counts.get('test.fail') and any('forward input reaches native page' in str(w) for w in data.get('warnings',[])), result.stderr[-4000:]
        print(f'reverse: disconnected page input failed as required, exit={result.returncode}',flush=True)
    else:
        assert result.returncode == 0 and counts.get('test.finished') == 1 and not counts.get('test.fail'), f'exit={result.returncode}, metrics={counts}\n{result.stderr[-5500:]}\n{data.get("warnings", [])}'
        print(f'Mogo native page: PASS ({counts.get("test.pass")} assertions); GBK +10/-10 and EPUB',flush=True)
    if not os.environ.get('INK_KEEP_EVIDENCE'): shutil.rmtree(area)

if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('--build',type=Path,required=True)
    a=p.parse_args(); exercise(a.build.resolve())
