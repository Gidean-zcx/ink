"""Bounded product acceptance runs; evidence is retained only when requested or failing."""
import argparse
import copy
import json
import os
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--case', choices=('pages', 'phone', 'voice'))
parser.add_argument('--binary', type=Path)
parser.add_argument('--language', choices=('zh', 'en'))
args = parser.parse_args()
build = args.build.resolve()
area = build / 'test-out' / ('small-gaps-' + (args.binary.name if args.binary else 'product'))
area.mkdir(parents=True, exist_ok=True)
fixture = json.loads((root / 'tests/ai_answer_long/hub.json').read_text())
voice = next(r for r in fixture['rules'] if r['method'] == 'ai.listen')
voice = copy.deepcopy(voice.get('turns', [voice])[0])
voice['stream']['chunks'] = ['这是一段足够长的合成回答。' * 40]
voice['listen'].pop('tts_speed', None)
fixture['rules'] = [r for r in fixture['rules'] if r['method'] not in ('ai.listen', 'ai.ask')]
fixture['rules'].append({'method': 'ai.listen', 'turns': [voice] + [copy.deepcopy(voice) for _ in range(3)]})
for case in ([args.case] if args.case else ('pages', 'phone', 'voice')):
    for lang in ([args.language] if args.language else (('zh',) if case == 'voice' else ('zh', 'en'))):
        out = area / (case + '-' + lang)
        shutil.rmtree(out, ignore_errors=True)
        out.mkdir()
        script = out / 'run.script'
        script.write_text('@0 net wifi\n@260s end\n')
        hub = out / 'hub.json'
        hub.write_text(json.dumps(fixture, ensure_ascii=False))
        profile = 'metalio-plus-handsfree.json' if case == 'voice' else 'metalio-plus.json'
        command = [str((args.binary or build / ('sim-small-gaps-' + case)).resolve()),
                   '--profile', str(root / 'simulator/profiles' / profile), '--script', str(script),
                   '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
                   '--hub', str(hub), '--budget', str(root / 'tests/ui_scene.budget.json'),
                   '--out', str(out), '--quiet', '--no-random-outages']
        env = {**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'}
        if lang == 'en': env['INK_GAPS_EN'] = '1'
        result = subprocess.run(command, cwd=root, capture_output=True, text=True, env=env, timeout=180)
        (out / 'run.log').write_text(result.stdout + result.stderr)
        data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
        counts = data.get('metrics', {})
        ok = result.returncode == 0 and counts.get('test.finished') == 1 and not counts.get('test.fail', 0) and not data.get('warnings')
        print(case, lang, 'PASS' if ok else 'FAIL', 'exit', result.returncode, 'checks', counts.get('test.pass'), flush=True)
        if not ok:
            raise SystemExit(result.stderr[-8000:] + '\n' + str(data.get('warnings', [])))
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
