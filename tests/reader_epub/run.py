"""EPUB C API, real reader, members and restart, using only synthetic local content."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]

def run(build, binary=None, reverse=False, name=None, expected='reader.open_book returns done'):
    area = build / 'test-out' / (name or ('reader-epub-reverse' if reverse else 'reader-epub'))
    if area.exists(): shutil.rmtree(area)
    content = area / 'content'
    shutil.copytree(build/'test-content', content)
    for path in (ROOT/'tests/reader_epub/fixtures').iterdir(): shutil.copyfile(path,content/'books'/path.name)
    for i in range(24): (content/'books'/f'填充{i:02d}.txt').write_text(f'原创测试书{i}。\n')
    for phase in (('read',) if reverse else ('read','restart')):
        out=area/phase
        result=subprocess.run([str(binary or build/'sim-reader-epub'), '--profile',str(ROOT/'simulator/profiles/metalio-plus.json'),
            '--script',str(ROOT/'tests/kid_3.script'),'--budget',str(ROOT/'tests/ui_scene.budget.json'),
            '--assets',str(build/'assets'),'--content',str(content),'--out',str(out),'--flash-image',str(area/'user.img'),
            '--sd-dir',str(area/'sd'),'--no-random-outages','--quiet'],cwd=ROOT,capture_output=True,text=True,timeout=300,
            env={**os.environ,'INK_READER_EPUB_PHASE':phase,'UBSAN_OPTIONS':'halt_on_error=1'})
        (area/f'{phase}.log').write_text(result.stdout+result.stderr)
        data=json.loads((out/'metrics.json').read_text()) if (out/'metrics.json').exists() else {}
        counts=data.get('metrics',{}); failures=counts.get('test.fail',0)
        if reverse:
            assert result.returncode and failures and any(expected in str(w) for w in data.get('warnings',[])), result.stderr[-5000:]
            print(f'reverse: expected failure exit={result.returncode}, failed assertions={failures}',flush=True)
        elif result.returncode or counts.get('test.finished')!=1 or failures:
            raise SystemExit(f'{phase}: FAIL exit={result.returncode}, finished={counts.get("test.finished")}\n{result.stderr[-7000:]}\n{data.get("warnings",[])}')
        else: print(f'{phase}: PASS assertions={counts.get("test.pass")}',flush=True)
    if not os.environ.get('INK_KEEP_EVIDENCE'): shutil.rmtree(area)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('--build',type=Path,required=True); a=p.parse_args(); run(a.build.resolve())
