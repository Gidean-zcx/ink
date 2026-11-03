#!/usr/bin/env python3
"""Validate completed audit artifacts and read-only scope; does not rerun product tests."""
import ast
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess

HERE = Path(__file__).resolve().parent
AUDIT = HERE.parent
ROOT = HERE.parents[3]
EXPECTED_HEAD = '38511ec9b490b20210087dc17a9df31eec0ded6d'
OUTPUT = HERE/'artifact-validation.json'
checks = []

def check(name, ok, evidence):
    checks.append(dict(name=name, passed=bool(ok), evidence=evidence))

def git(*args):
    p = subprocess.run(['git', *args], cwd=ROOT, capture_output=True, text=True, check=True)
    return p.stdout.strip()

def read(name):
    return json.loads((HERE/name).read_text())

head = git('rev-parse', 'HEAD')
check('audited_head_unchanged', head == EXPECTED_HEAD, head)
changed = git('diff', '--name-only')
staged = git('diff', '--cached', '--name-only')
check('tracked_files_unchanged', not changed and not staged, dict(unstaged=changed, staged=staged))
untracked = git('ls-files', '--others', '--exclude-standard').splitlines()
prefix = str(AUDIT.relative_to(ROOT))+'/'
check('untracked_outputs_confined', all(s.startswith(prefix) for s in untracked), untracked)

files = sorted(p for p in AUDIT.rglob('*') if p.is_file() and p != OUTPUT)
check('files_below_1_MB', all(p.stat().st_size < 1000000 for p in files),
      dict(files=len(files), largest_bytes=max(p.stat().st_size for p in files)))
bad_json = []
for p in files:
    if p.suffix == '.json':
        try: json.loads(p.read_text())
        except (ValueError, UnicodeError) as e: bad_json.append(str(p.relative_to(AUDIT))+': '+str(e))
check('json_parse', not bad_json, bad_json)
bad_python = []
for p in HERE.glob('*.py'):
    try: ast.parse(p.read_text(), filename=p.name)
    except SyntaxError as e: bad_python.append(str(e))
check('audit_python_syntax_without_bytecode', not bad_python, bad_python)

broken_links = []
for p in [AUDIT/'report.md', HERE/'README.md']:
    for target in re.findall(r'\[[^\]]*\]\(([^)]+)\)', p.read_text()):
        if target.startswith(('https://', 'http://', '#')): continue
        dest = (p.parent/target.split('#')[0]).resolve()
        if not dest.exists() and dest != OUTPUT: broken_links.append([str(p.relative_to(AUDIT)), target])
check('report_and_experiment_links', not broken_links, broken_links)

report = (AUDIT/'report.md').read_text()
check('report_finalized', '状态：已完成' in report and '完成后填写' not in report and '### 待执行' not in report,
      'final status and no pending skeleton sections')
findings = json.loads((AUDIT/'findings.json').read_text())
required = {'id','severity','area','title','evidence_type','locations','impact','recommendation','reproduce'}
check('findings_schema', all(set(f) == required for f in findings), [sorted(f) for f in findings])
check('findings_ids_and_counts', [f['id'] for f in findings] == ['AUD9-001','AUD9-002']
      and Counter(f['severity'] for f in findings) == {'中':1,'高':1},
      dict(ids=[f['id'] for f in findings], severity=dict(Counter(f['severity'] for f in findings))))
bad_locations = []
for f in findings:
    if f['id'] not in report: bad_locations.append(f['id']+' missing in report')
    for loc in f['locations']:
        path, line = loc.rsplit(':', 1)
        p = ROOT/path
        if not p.is_file() or not (1 <= int(line) <= len(p.read_text().splitlines())): bad_locations.append(loc)
check('finding_locations_exist', not bad_locations, bad_locations)
verification = json.loads((AUDIT/'verification.json').read_text())
expected = {'AUD8-001':'已修复', 'AUD-011/F-8':'部分修复', 'AUD2-023':'部分修复', 'AUD-007/F-6':'未修复'}
check('verification_schema_and_verdicts',
      all(set(v) == {'id','verdict','evidence_type','note'} for v in verification)
      and {v['id']:v['verdict'] for v in verification} == expected, expected)

steps = read('suite-steps.json')
check('all_build_and_ctest_steps_exit_0', len(steps) == 9 and all(s['exit_code'] == 0 for s in steps),
      [{k:s[k] for k in ('label','exit_code')} for s in steps])
suites = read('suite-evidence.json')['suites']
check('formal_test_counts', [len(s['tests']) for s in suites] == [43,42]
      and all(all('Passed' in t for t in s['tests']) for s in suites),
      [s['summary'] for s in suites])
results = read('results.json')
counts = Counter('before' if r['before'] else 'asan' if r['sanitizer'] else 'current' for r in results)
check('probe_counts_and_exits', len(results) == 45 and all(r['exit_code'] == 0 for r in results)
      and counts == {'before':11,'asan':6,'current':28}, dict(counts))
markers = ('ERROR: AddressSanitizer','runtime error:', 'SUMMARY: AddressSanitizer','SUMMARY: UndefinedBehaviorSanitizer')
bad_sanitizers = []
for row in results:
    if not row['sanitizer']: continue
    out = Path(row['command'][row['command'].index('--out')+1])
    log = (out/'run.log').read_text()
    if any(m in log for m in markers): bad_sanitizers.append(row['name'])
check('supplementary_sanitizer_diagnostics', not bad_sanitizers, bad_sanitizers)

pq = read('pq-results.json')
check('pq_expected_defect_exits', len(pq) == 4
      and all(r['exit_code'] == (0 if r['label']=='compile' else 1) for r in pq)
      and all('differential_failed=11 total=23' in r['output'] for r in pq if r['label']=='run')
      and not any(any(m in line for m in markers) for r in pq for line in r['output']),
      [{k:r[k] for k in ('label','sanitizer','exit_code')} for r in pq])
clock = read('clock-cycle-results.json')
check('clock_positive_negative_control', len(clock) == 4 and
      all(r['exit_code'] == (1 if r['label']=='run' and r['old_idle_algorithm'] else 0) for r in clock),
      [{k:r[k] for k in ('label','old_idle_algorithm','exit_code')} for r in clock])
costs = read('pq-cost-results.json')
latest = {r['label']:r for r in costs}
check('iss_completed_with_measured_regions', all(latest[label]['exit_code'] == 0 for label in ('compile','fitted','slow'))
      and all(len(latest[label]['regions']) == 5 for label in ('fitted','slow')),
      dict(final_exits={label:latest[label]['exit_code'] for label in ('compile','fitted','slow')},
           prior_auxiliary_compile_failures=sum(r['label']=='compile' and r['exit_code']!=0 for r in costs)))
audio = read('audio-integrity.json')
check('audio_sample_checks', audio['control_samples'] == 48000 and len(audio['insertions']) == 5
      and all(audio[k] for k in ('all_48000_voice_samples_identical','fault_cancel_tail_is_tone','fault_replace_tail_is_new_stream')),
      {k:v for k,v in audio.items() if k not in ('hashes','insertions','note')})
comparisons = read('comparison.json')['comparisons']
check('normal_comparisons', len(comparisons)==11 and sum(c['final_png_byte_equal'] for c in comparisons)==9
      and all(c['warnings_before']==[] and c['warnings_after']==[] and c['budget_violations_before']==0
              and c['budget_violations_after']==0 for c in comparisons),
      dict(comparisons=len(comparisons), identical_pngs=sum(c['final_png_byte_equal'] for c in comparisons)))
known = read('known-results.json')[0]
check('known_F6_reproduced', known['exit_code']==0 and known['metrics']['reader.page_turns']==2
      and known['metrics']['tools.calls.done']==1, known['metrics'])

result = dict(head=head, passed=all(c['passed'] for c in checks), checks=checks,
              files=[dict(path=str(p.relative_to(AUDIT)), bytes=p.stat().st_size,
                          sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in files],
              note='Artifact consistency check, not additional product test or hardware acceptance; build caches are task-authorized and ignored by git.')
OUTPUT.write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n')
print(json.dumps(dict(passed=result['passed'], checks=len(checks), files=len(files),
                      failed=[c['name'] for c in checks if not c['passed']]), ensure_ascii=False))
raise SystemExit(0 if result['passed'] else 1)
