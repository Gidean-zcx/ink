"""Run the product and optional source-copy reverse controls; no history required."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--negative', choices=('name', 'unsupported', 'toggle', 'pause', 'insert', 'invalidate',
                                          'password', 'hidden', 'offline', 'member', 'child', 'child-name', 'prefix'))
args = parser.parse_args()
build = args.build.resolve()
area = build / 'voice-name-assist-evidence' / (args.negative or 'fixed')
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)
binary = build / 'sim-voice-name-assist'
mutations = {
    'name': ('ports/sim/hal/hal_afe_sim.c', 'bool matches = phrase ? !strcmp(phrase, hal_afe_names()[F.name]) : F.name == 0;', 'bool matches = true;', 'NAME:'),
    'unsupported': ('system/ui/shell/settings_pages.c', '这台设备暂不支持叫名字', '暂不能更改', 'UNSUPPORTED:'),
    'toggle': ('system/services/text/text_service.c', 'ink_setting_get("text.assist") && ', '', 'TOGGLE:'),
    'pause': ('system/services/text/text_service.c', '800000', '1', 'PAUSE:'),
    'insert': ('system/services/text/text_service.c', 'ink_text_commit_utf8(word); return;', '(void)word; return;', 'INSERT:'),
    'invalidate': ('system/services/text/text_service.c', 'AS.generation++; AS.n = 0;', 'AS.generation++;', 'INVALIDATE:'),
    'password': ('system/services/text/text_service.c', 'TS.type == INK_TEXT_PLAIN && ', '', 'PASSWORD:'),
    'hidden': ('system/services/text/text_service.c', '!TS.ai_hidden &&', 'true &&', 'HIDDEN:'),
    'offline': ('system/services/text/text_service.c', 'ink_hub_online() && ', '', 'OFFLINE:'),
    'member': ('system/services/text/text_service.c', '.per_member = true, ', '', 'MEMBER:'),
    'child': ('system/services/text/text_service.c', 'ink_member_allows("shell.ai_ask", true) && ', '', 'CHILD_AI:'),
    'child-name': ('system/services/audio/audio_service.c', '.id = "voice.name",', '.id = "voice.name", .child_may_change = true,', 'CHILD_NAME:'),
    'prefix': ('system/services/text/text_service.c', 'chars - 500', 'chars - 501', 'PREFIX:'),
}
if args.negative:
    path, old, new, marker = mutations[args.negative]
    source = (ROOT / path).read_text()
    assert old in source
    source = source.replace(old, new)
    db = json.loads((build / 'compile_commands.json').read_text())
    entry = next(e for e in db if Path(e['file']) == ROOT / path)
    command = shlex.split(entry['command'])
    obj, mutant = area / 'mutant.o', area / 'mutant.c'
    mutant.write_text(source)
    command[command.index('-o') + 1] = str(obj)
    command[command.index(str(ROOT / path))] = str(mutant)
    command += ['-iquote', str((ROOT / path).parent)]
    result = subprocess.run(command, cwd=build, capture_output=True, text=True)
    (area / 'compile.log').write_text(result.stdout + result.stderr)
    assert result.returncode == 0, result.stderr
    link = shlex.split((build / 'CMakeFiles/sim-voice-name-assist.dir/link.txt').read_text())
    binary = area / 'negative-sim'
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, item in enumerate(link) if item.endswith('libink_system.a')), str(obj))
    subprocess.run(link, cwd=build, check=True, capture_output=True)

profiles = ['metalio-plus'] if args.negative == 'unsupported' else ['metalio-plus-handsfree'] if args.negative else ['metalio-plus-handsfree', 'restart', 'metalio-plus']
for phase in profiles:
    profile = 'metalio-plus-handsfree' if phase == 'restart' else phase
    out = area / phase
    state = area / profile
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles' / (profile + '.json')),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--flash-image', str(state / 'user.img'), '--sd-dir', str(state / 'sd'), '--end', '180s',
        '--quiet', '--no-random-outages', '--budget', str(ROOT / 'tests/ui_scene.budget.json')],
        cwd=ROOT, capture_output=True, text=True, timeout=120,
        env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1', **({'INK_ASSIST_RESTART': '1'} if phase == 'restart' else {})})
    out.mkdir(parents=True, exist_ok=True)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    counts = data.get('metrics', {})
    log = result.stdout + result.stderr + '\n'.join(data.get('warnings', []))
    (out / 'run.log').write_text(log)
    print(f'{args.negative or phase}: exit={result.returncode}, pass={counts.get("test.pass", 0)}, '
          f'fail={counts.get("test.fail", 0)}, finished={counts.get("test.finished", 0)}', flush=True)
    if args.negative:
        assert result.returncode != 0 and counts.get('test.fail', 0) and marker in log, log[-4000:]
    else:
        assert result.returncode == 0 and counts.get('test.finished') == 1 and not counts.get('test.fail', 0) and not data.get('warnings'), log[-5000:]
        expected = () if phase == 'restart' else ('voice-name', 'assist-candidates', 'assist-expanded') if profile.endswith('handsfree') else ('voice-unsupported',)
        assert all((out / f'clean-{name}.png').exists() for name in expected)
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
