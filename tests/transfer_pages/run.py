"""Exercise transfer pages using an external HTTP client and synthetic USB files."""
import argparse
import hashlib
import http.client
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
SCENES = 'Transfer TransferLive TransferUsbBusy TransferUsbOn TransferUsbFail FileList'.split()
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
p.add_argument('--visual', action='store_true')
a = p.parse_args()
build = a.build.resolve()
area = Path(tempfile.mkdtemp(prefix='transfer-pages-', dir=build))
binary = a.binary.resolve() if a.binary else build / 'sim-transfer-pages'
summaries = []
try:
    for scene in SCENES if a.visual else ['Transfer']:
        out = area / scene
        out.mkdir()
        (out / 'usb-fixture.txt').write_text('SYNTHETIC_USB')
        script = out / 'scenario.script'
        script.write_text('@0 net wifi\n@240s end\n')
        command = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
            '--script', str(script), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
            '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
            '--sd-dir', str(out / 'card'), '--out', str(out), '--realtime=20', '--quiet', '--no-random-outages']
        with (out / 'sim.log').open('w+') as log:
            proc = subprocess.Popen(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                env={**os.environ, 'INK_TRANSFER_SCENE': scene, 'UBSAN_OPTIONS': 'halt_on_error=1'})
            sock = None
            try:
                sequence = 0
                deadline = time.monotonic() + 90
                before = None
                while proc.poll() is None:
                    assert time.monotonic() < deadline, 'client/simulator deadline'
                    try:
                        session = json.loads((out / 'session.json').read_text())
                    except (FileNotFoundError, json.JSONDecodeError):
                        time.sleep(0.005)
                        continue
                    if session['sequence'] == sequence:
                        time.sleep(0.005)
                        continue
                    sequence = session['sequence']
                    stage, state = session['stage'], session['session']
                    if stage == 'begin':
                        sock = socket.create_connection(('127.0.0.1', state['port']), timeout=10)
                        head = (f'POST /upload HTTP/1.1\r\nX-Ink-Code: {state["code"]}\r\n'
                                'X-Ink-Name: SYNTH_UPLOAD.txt\r\nContent-Length: 12288\r\n\r\n')
                        sock.sendall(head.encode() + b'U' * 4096)
                    elif stage == 'finish':
                        sock.sendall(b'U' * 8192)
                        response = http.client.HTTPResponse(sock)
                        response.begin()
                        assert (response.status, response.read().decode()) == (201, '文件已收到')
                        sock.close()
                        sock = None
                    elif stage in ('bad-before', 'bad-after'):
                        current = {str(f.relative_to(out / 'card')): hashlib.sha256(f.read_bytes()).hexdigest()
                                   for f in (out / 'card').rglob('*') if f.is_file()}
                        if stage == 'bad-before':
                            before = current
                        else:
                            assert before and current == before, 'bad card changed files or formatted storage'
                    else:
                        raise AssertionError(stage)
                    (out / f'ack-{sequence}').touch()
                data = json.loads((out / 'metrics.json').read_text())
                counts = data['metrics']
                assert proc.returncode == 0 and counts.get('test.finished') == 1 and not counts.get('test.fail', 0), data.get('warnings')
                assert not data.get('warnings'), data['warnings']
                assert counts.get('test.snapshot.settled') == 1 and (out / 'snap-scene.png').is_file()
                snapshots = [json.loads(line) for line in (out / 'snapshots.ndjson').read_text().splitlines()]
                assert {s['scene'] for s in snapshots} == set(SCENES)
                target = build / 'transfer-compare' / scene
                target.mkdir(parents=True, exist_ok=True)
                for name in ('snap-scene.png', 'metrics.json', 'snapshots.ndjson'):
                    shutil.copyfile(out / name, target / name)
                summaries.append({'scene': scene, 'checks': counts['test.pass'], 'exit': proc.returncode})
                print(f'{scene}: PASS {int(counts["test.pass"])} checks; HTTP bytes and USB bad-card write guard', flush=True)
            except BaseException:
                if proc.poll() is None:
                    proc.kill()
                proc.wait()
                log.seek(0)
                print(log.read()[-10000:])
                raise
            finally:
                if sock:
                    sock.close()
    (area / 'summary.json').write_text(json.dumps(summaries, indent=2) + '\n')
except BaseException:
    print('Evidence:', area.relative_to(ROOT), flush=True)
    raise
else:
    if not os.environ.get('INK_KEEP_EVIDENCE'):
        shutil.rmtree(area)
    else:
        print('Evidence:', area.relative_to(ROOT), flush=True)
