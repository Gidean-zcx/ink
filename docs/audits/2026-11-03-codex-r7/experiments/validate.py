#!/usr/bin/env python3
"""Validate audit artifacts and evidence, not product implementation."""
import ast
import collections
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess

HERE = Path(__file__).resolve().parent
AUDIT = HERE.parent
ROOT = HERE.parents[3]
HEAD = '06a882fb7c39ddaac41a111c161e2f26c9798070'
checked = []

def check(name, condition):
    checked.append(dict(check=name, passed=bool(condition)))
    if not condition:
        raise AssertionError(name)

def read(p):
    return json.loads(p.read_text())

def out(row):
    return Path(row['command'][row['command'].index('--out')+1])

def metric(row, key):
    return read(out(row)/'metrics.json')['metrics'].get(key, 0)

rows=read(HERE/'results.json')
commands=[]
for r in rows:
    cmd=['python3',str(HERE.relative_to(ROOT)/'run.py')]
    cmd += ['--before'] if r['before'] else []
    cmd += ['--asan'] if r['sanitizer'] else []
    cmd += ['--pqbn'] if r['pqbn'] else []
    cmd += [r['name']]
    commands.append('AUDIT_END=60s '+shlex.join(cmd))
(HERE/'commands.txt').write_text('\n'.join(commands)+'\n')
(HERE/'artifact-validation.json').write_text('{"status":"checking"}\n')
files = [p for p in AUDIT.rglob('*') if p.is_file()]
check('each_artifact_under_1_MB', all(p.stat().st_size < 1000000 for p in files))
for p in files:
    if p.suffix == '.json': read(p)
    if p.suffix == '.py': ast.parse(p.read_text(), filename=str(p))
check('json_and_python_syntax', True)
ver = read(AUDIT/'verification.json')
check('verification_ids', {r['id'] for r in ver} == {'AUD7-001','AUD7-002','AUD7-003','AUD7-004','AUD2-023'} and len(ver)==5)
check('verification_verdicts', all(r['verdict'] in ('已修复','部分修复','未修复','回归') for r in ver))
report=(AUDIT/'report.md').read_text()
check('report_verdicts_match_json', all('| '+r['id']+' | '+r['verdict']+' |' in report for r in ver))
findings=read(AUDIT/'findings.json')
required={'id','severity','area','title','evidence_type','locations','impact','recommendation','reproduce'}
check('new_findings_schema', all(set(f)==required for f in findings))
check('new_findings_ids', [f['id'] for f in findings]==['AUD8-001'])
for f in findings:
    for loc in f['locations']:
        path,line=loc.rsplit(':',1)
        check('source_location_'+loc, 1<=int(line)<=len((ROOT/path).read_text().splitlines()))
for p in (AUDIT/'report.md',HERE/'README.md'):
    for target in re.findall(r'\]\(([^)]+)\)',p.read_text()):
        if '://' in target or target.startswith('#'): continue
        check('relative_link_'+str(p.relative_to(AUDIT))+':'+target,(p.parent/target.split('#')[0]).exists())

check('84_distinct_probe_combinations',len(rows)==84 and len({(r['name'],r['sanitizer'],r['pqbn'],r['before']) for r in rows})==84)
check('all_probe_processes_finished',all(r['exit_code']==0 for r in rows))
check('16_sanitizer_combinations',sum(r['sanitizer'] for r in rows)==16)
san_errors=[]
for r in rows:
    log=(out(r)/'run.log').read_text(errors='replace')
    if re.search(r'ERROR: AddressSanitizer|runtime error:|SUMMARY: (?:AddressSanitizer|UndefinedBehaviorSanitizer)',log):
        san_errors.append(r['name'])
check('no_probe_sanitizer_diagnostics',not san_errors)
index={(r['name'],r['before'],r['pqbn'],r['sanitizer']):r for r in rows}
for san in (False,True):
    cur=index[('r7_audio_tone_normal',False,False,san)]
    check('tone_regression_reproduced_'+str(san),metric(cur,'audit.tone_ms')['p50']>17000 and metric(cur,'audio.speaker.played_ms')==18060)
    cancel=index[('r6_audio_cancel_reopen',False,False,san)]
    check('cancel_fix_'+str(san),metric(cancel,'audio.speaker.played_ms')==120)
    replace=index[('r7_audio_replace_reopen',False,False,san)]
    check('replace_fix_'+str(san),metric(replace,'audio.speaker.played_ms')==420)
old=index[('r7_audio_tone_normal',True,False,False)]
check('tone_old_response_under_150ms',metric(old,'audit.tone_ms')['p50']<150)
for name,n in (('r7_display_multi',80),('r7_display_control',80),('r7_display_recut',96)):
    r=index[(name,False,True,False)]
    lines=[s for s in r['output'] if 'AUD8 region' in s]
    check(name+'_unique_callbacks',len(lines)==n and len({re.search(r'id=(\d+)',s)[1] for s in lines})==n)
    check(name+'_pixels_at_success', all(abs(float(re.search(r'mean=([0-9.]+)',s)[1])-int(re.search(r'latest=(\d+)',s)[1]))<25 for s in lines if 'ok=1' in s))
    check(name+'_drained',any('display-final pending=0 playing=0' in s for s in r['output']))
for pq in (False,True):
    for name in ('r7_rails_cancel','r7_rails_cancel_fault','r7_rails_wake_fault'):
        r=index[(name,False,pq,False)]
        check(name+'_restored_'+str(pq),any('rails final flag=0 p05=1 p14=1 dirs=c0,e3' in s for s in r['output']))
    r=index[('r7_rails_deadline',False,pq,False)]
    check('rails_deadline_bounded_'+str(pq),metric(r,'display.power_up_ms')['max']<350)
comparison=read(HERE/'comparison.json')['comparisons']
check('9_normal_comparisons_png_equal',len(comparison)==9 and all(r['final_png_byte_equal'] for r in comparison))
check('normal_comparisons_no_warnings',all(not r['warnings_before'] and not r['warnings_after'] for r in comparison))
check('normal_comparisons_no_budget_violations',all(not r['budget_violations_before'] and not r['budget_violations_after'] for r in comparison))
steps=read(HERE/'suite-steps.json')
check('9_build_and_test_steps_pass',len(steps)==9 and all(r['exit_code']==0 for r in steps))
known=read(HERE/'known-results.json')
check('known_clock_still_fails',known[1]['exit_code']==1 and any('30208 (92.2%)' in s for s in known[1]['output']))
check('known_late_tool_still_executes',known[2]['exit_code']==0 and known[2]['metrics']['reader.page_turns']==2 and known[2]['metrics']['tools.calls.done']==1)
state={}
for name,cmd in [('head',['git','rev-parse','HEAD']),('diff',['git','diff','--exit-code']),('cached',['git','diff','--cached','--exit-code']),('status',['git','status','--short']),('untracked',['git','ls-files','--others','--exclude-standard'])]:
    p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True)
    state[name]=dict(exit_code=p.returncode,output=p.stdout.strip())
check('same_head',state['head']['output']==HEAD)
check('no_tracked_mutations',state['diff']['exit_code']==0 and state['cached']['exit_code']==0)
prefix=str(AUDIT.relative_to(ROOT))+'/'
check('all_untracked_in_r7',all(s.startswith(prefix) for s in state['untracked']['output'].splitlines()))
hashes={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files if p.name!='artifact-validation.json'}
result=dict(head=HEAD,checks=checked,git=state,new_findings=dict(collections.Counter(f['severity'] for f in findings)),verification=dict(collections.Counter(v['verdict'] for v in ver)),probe_combinations=len(rows),sanitizer_combinations=16,known_high_rechecked=['AUD-007/F-6','AUD-011/F-8'],artifact_sha256=hashes)
(HERE/'artifact-validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(checks=len(checked),passed=all(r['passed'] for r in checked),probe_combinations=len(rows)),ensure_ascii=False))
