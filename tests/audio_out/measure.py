"""Collect all output evidence even after a known underrun; still exit nonzero on failure."""
import argparse
import json
from pathlib import Path
import shutil
from run import run, samples, quality, check_phases, check_fresh_prefixes

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
binary = build / 'sim-audio-out'
area = build / 'audio-out-evidence' / binary.name
shutil.rmtree(area, ignore_errors=True)
results, failed = {}, False
for case in ['tone1k', 'tone10k', 'scenario', 'music']:
    folder = area / case
    try:
        pcm = run(build, binary, case, area)
    except AssertionError:
        data = json.loads((folder / 'out/metrics.json').read_text())
        warnings = data.get('warnings', [])
        assert data['metrics'].get('test.finished') == 1
        assert len(warnings) == 1 and 'no underruns:' in warnings[0], warnings
        assert data['metrics'].get('test.fail') == 1
        failed = True
        pcm = samples(folder / 'out/speaker.wav')
    data = json.loads((folder / 'out/metrics.json').read_text())
    m = data['metrics']
    result = {'underruns': m.get('audio.speaker.underruns', 0), 'underrun_ms': m.get('audio.speaker.underrun_ms', 0)}
    if case.startswith('tone'):
        result.update(quality(pcm, 1000 if case == 'tone1k' else 6000))
        result['signal_threshold_met'] = result['snr_db'] >= 60 if case == 'tone1k' else result['attenuation_db'] >= 40
        failed |= not result['signal_threshold_met']
    elif case == 'scenario':
        for name, fn in [('phase_frequencies', lambda: check_phases(pcm, m)),
                         ('fresh_fir_prefixes', lambda: check_fresh_prefixes(build, folder / 'sd/content/audio', folder, pcm, m))]:
            try:
                fn()
                result[name] = True
            except AssertionError:
                result[name] = False
                failed = True
    results[case] = result
    print(case, json.dumps(result), flush=True)
(area / 'quality.json').write_text(json.dumps(results, indent=2) + '\n')
raise SystemExit(1 if failed else 0)
