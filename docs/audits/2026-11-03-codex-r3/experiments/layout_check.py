#!/usr/bin/env python3
"""Check the rename-only revision and compile header boundaries without editing product files."""
import json, re, shlex, subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]

def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT)

def norm(s):
    s = s.replace('"board/', '"metalio_plus/').replace('"/profiles/metalio-plus-psram16.json"', '"/simulator/profiles/metalio-plus-psram16.json"')
    s = re.sub(r'/\*.*?\*/|//[^\n]*', '', s, flags=re.S)
    return re.sub(r'\s+', '', s)

raw = git('diff', '--name-status', '-z', '-M', '4309087', 'dc085cb').decode().split('\0')
i = 0
pairs, added, deleted = [], [], []
while i < len(raw) and raw[i]:
    status, old = raw[i:i+2]; i += 2
    new = old
    if status[0] in 'RC':
        new = raw[i]; i += 1
    if status[0] == 'A': added.append(new)
    elif status[0] == 'D': deleted.append(old)
    else: pairs.append((old, new, status))

code = []
for old, new, status in pairs:
    if Path(new).suffix not in ('.c','.h','.m','.cpp'):
        continue
    a = git('show', f'4309087:{old}').decode()
    b = git('show', f'dc085cb:{new}').decode()
    code.append(dict(old=old,new=new,status=status,byte_identical=a==b,code_identical=norm(a)==norm(b)))

commands = json.loads((ROOT/'build-audit/compile_commands.json').read_text())
checks = []
for target, suffix in [('ink_system','system/services/power/power_service.c'),('ink_apps','apps/home/home.c')]:
    c = next(x for x in commands if x['file'].endswith('/'+suffix))
    argv = shlex.split(c['command']); flags=[]; j=1
    while j < len(argv):
        a=argv[j]
        if a in ('-o','-c'): j+=2; continue
        flags.append(a); j+=1
    for hdr, allowed in [('ink/osal.h',True),('ink/sys/display.h',True),('sim.h',False),('vtk.h',False),('tca9555/tca9555.h',False),('metalio_plus/metalio_board.h',False)]:
        cmd=[argv[0],*flags,'-x','c','-fsyntax-only','-']
        p=subprocess.run(cmd,cwd=ROOT,input=f'#include "{hdr}"\n',text=True,capture_output=True)
        checks.append(dict(target=target,header=hdr,expected_visible=allowed,exit_code=p.returncode,passed=(p.returncode==0)==allowed,command=cmd,evidence=p.stderr.splitlines()[:2]))

out=dict(before='4309087',restructure='dc085cb',code_files=code,added=added,deleted=deleted,header_checks=checks)
(HERE/'layout-evidence.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(code_pairs=len(code),non_comment_code_differences=[x for x in code if not x['code_identical']],added=added,deleted=deleted,header_checks=len(checks),header_checks_passed=sum(x['passed'] for x in checks)),ensure_ascii=False,indent=2))
