"""Keep small, local summaries of the exact baseline and audit runs."""
import json
import re
from pathlib import Path

root=Path(__file__).resolve().parents[4]
out=Path(__file__).resolve().parent
names=['record-call','voice-call','text-oom','shutdown-audio','shutdown-audio-reboot',
       'headset','audio-focus','power-picture','voice-lifecycle','text-boundary',
       'record-card','power-wait','record-end-original','interrupt-fixed',
       'reverse-media-pause','reverse-media-member','reverse-interrupt',
       'text-oom-asan','text-boundary-asan']
expected_fail={'record-end-original':2,'reverse-media-pause':1,'reverse-media-member':1,'reverse-interrupt':2}
runs=[]
for name in names:
    log=(out/(name+'.log')).read_text()
    match=re.search(r'report (\S+/metrics\.json)',log)
    assert match, name
    path=Path(match.group(1))
    obj=json.loads(path.read_text()); m=obj['metrics']
    assert m.get('test.finished')==1,(name,m.get('test.finished'))
    assert m.get('test.fail',0)==expected_fail.get(name,0),(name,m.get('test.fail'))
    assert 'ink-sim: ok,' in log, name
    runs.append({'name':name,'log':name+'.log','simulator_exit':0,
                 'runner_exit':1 if name in expected_fail else 0,
                 'metrics':str(path.relative_to(root)),
                 'test_finished':m.get('test.finished'),'test_fail':m.get('test.fail',0),
                 'observations':[line for line in log.splitlines() if line.startswith('AUDIT ')],
                 'warnings':obj.get('warnings',[])})
(out/'probe-results.json').write_text(json.dumps(runs,ensure_ascii=False,indent=2)+'\n')
tasks=['hub','esp_timer','power','input','exec.bg','exec.media']
rows=[]
for path in sorted((root/'build-audit/test-out').rglob('metrics.json')):
    obj=json.loads(path.read_text()); m=obj['metrics']
    keep={k:v for k,v in m.items() if k.startswith('mem.internal_total.') or k in ['test.finished','test.fail'] or any(k.startswith('task.'+t+'.') and ('stack_' in k or k.endswith('max_burst_ms')) for t in tasks)}
    rows.append({'path':str(path.relative_to(root)),'metrics':keep,'warning_count':len(obj.get('warnings',[]))})
(out/'baseline-metrics.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
print('valid audit logs:',len(runs),'baseline metric files:',len(rows))
