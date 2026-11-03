"""Extract compact, repeatable evidence from local audit runtime artifacts."""
import hashlib
import json
from pathlib import Path
import re

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
rows = json.loads((HERE / 'results.json').read_text())
metrics = []
exact_keys = {
    'audio.start_without_chip', 'audio.bt_mode_sets', 'audio.mic.captured_ms',
    'audio.mic.lost_ms', 'audio.speaker.played_ms', 'audio.speaker.underruns',
    'audio.speaker.underrun_ms', 'ai.mic_unavailable', 'ai.answers', 'ai.failures',
    'hub.requests.ai.ask', 'hub.requests.ai.listen', 'hub.requests.ai.listen.end',
    'input.gpio2_rechecks', 'input.expander_restored_late', 'input.key_read_errors',
    'power.sleep_restore_errors', 'power.sleep_restore_retries', 'power.sleep_count',
    'power.sleep_entries', 'i2c.busy_ms', 'i2c.timeouts', 'display.power_recycled',
    'display.power_lost_during_waveform', 'display.power_failures',
    'display.unpowered_waveforms', 'display.unpowered_phases', 'display.power_ups',
    'display.pmic.order_warnings', 'display.pmic.attempts', 'display.pqbn.phases',
    'board.shared_power_warnings', 'board.shared_reset_warnings', 'camera.captures',
    'audio.play_concealed_frames', 'task.input.cpu_ms', 'panic.count',
    'display.latency_ms', 'display.phase_ms', 'audit.play_heard_ms',
}
for row in rows:
    cmd = row['command']
    out = Path(cmd[cmd.index('--out') + 1])
    d = json.loads((out / 'metrics.json').read_text())
    m = d['metrics']
    metrics.append({
        **{k: row.get(k, False) for k in ['name', 'sanitizer', 'pqbn', 'before']},
        'exit_code': row['exit_code'], 'metrics_file': str((out / 'metrics.json').relative_to(ROOT)),
        'metrics': {k: v for k, v in m.items() if k in exact_keys},
        'warnings': d['warnings'],
        'i2c_transfers': {k: v['count'] for k, v in m.items() if k.startswith('i2c.wait_ms.')},
    })
(HERE / 'metrics-evidence.json').write_text(json.dumps(metrics, ensure_ascii=False, indent=2) + '\n')

by_id = {(r['name'], r['sanitizer'], r.get('pqbn', False), r.get('before', False)): r for r in rows}
comparisons = []
for name, pq in [('r4_normal',False),('r4_normal_activity',False),('r4_service_normal',False),('r4_normal',True),('r4_normal_activity',True)]:
    before = by_id[(name,False,pq,True)]
    after = by_id[(name,False,pq,False)]
    b_us = [int(re.search(r'latency_us=(\d+)',s).group(1)) for s in before['output'] if 'latency_us=' in s]
    a_us = [int(re.search(r'latency_us=(\d+)',s).group(1)) for s in after['output'] if 'latency_us=' in s]
    def get_dir(r):
        c = r['command']; return Path(c[c.index('--out')+1])
    bm = json.loads((get_dir(before)/'metrics.json').read_text())
    am = json.loads((get_dir(after)/'metrics.json').read_text())
    different = {k: [bm['metrics'].get(k), am['metrics'].get(k)] for k in set(bm['metrics']) | set(am['metrics']) if bm['metrics'].get(k) != am['metrics'].get(k)}
    comparisons.append(dict(name=name,pqbn=pq,before_exit=before['exit_code'],after_exit=after['exit_code'],
                            output_identical=before['output']==after['output'],
                            waveform_us_before=b_us,waveform_us_after=a_us,
                            waveform_delta_us=[a-b for a,b in zip(a_us,b_us)],
                            metric_differences=different,
                            warnings_before=bm['warnings'],warnings_after=am['warnings'],
                            final_png_identical=(get_dir(before)/'final.png').read_bytes()==(get_dir(after)/'final.png').read_bytes()))
faults=[]
for build in ['build-audit/before','build-audit']:
    p=ROOT/build/'test-out/reader_basic-panel-power-faults/metrics.json'
    d=json.loads(p.read_text())
    faults.append(dict(build=build,power_lost=d['metrics'].get('display.power_lost_during_waveform',0),
                       i2c_timeouts=d['metrics'].get('i2c.timeouts',0),budget_violations=d['budget_violations']))
payload={'before_commit':'95587e5a5f641fa4fd44c313f366e69f244faeb1',
         'after_commit':'89b112620730902f0fa1594e83ffb146b08749be',
         'comparisons':comparisons,'panel_fault_scenario':faults}
(HERE/'comparison.json').write_text(json.dumps(payload,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'configurations':len(rows),'sanitizer':sum(r['sanitizer'] for r in rows),
                  'before':sum(r.get('before',False) for r in rows),
                  'normal_comparisons':[dict(name=c['name'],pqbn=c['pqbn'],delta_us=c['waveform_delta_us'],png_identical=c['final_png_identical'],metric_changes=len(c['metric_differences'])) for c in comparisons],
                  'panel_fault_scenario':faults},ensure_ascii=False))
