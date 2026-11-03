#!/usr/bin/env python3
"""Execute the unchanged build_size function with a deterministic fake idf.py."""
import os, pathlib, subprocess, tempfile, json
R=pathlib.Path(__file__).resolve().parents[4]; A=pathlib.Path(__file__).resolve().parents[1]
source=(R/'tools/s31-size/run.sh').read_text()
function=source[source.index('build_size ()'):source.index('\nrm -rf hello wifi')]
with tempfile.TemporaryDirectory(prefix='ink-size-audit-') as d:
    w=pathlib.Path(d)
    for name in ['working','broken','bin']: (w/name).mkdir()
    fake=w/'bin/idf.py'
    fake.write_text('#!/bin/sh\ncase "$PWD" in */broken) exit 7;; esac\nprintf "mock size\\n"\n')
    fake.chmod(0o755)
    script='set -euo pipefail\n'+function+'\nbuild_size working "$WORK/working"\nbuild_size broken "$WORK/broken"\nfor f in "$WORK"/*.size.txt; do cat "$f"; done\n'
    env=os.environ|{'WORK':d, 'PATH':str(w/'bin')+':'+os.environ['PATH']}
    p=subprocess.run(['bash','-c',script],env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    result={'exit':p.returncode,'stdout':p.stdout,'method':'unchanged function; one child exits 7; no ESP-IDF firmware built'}
    (A/'logs/size-exit.json').write_text(json.dumps(result,indent=2,ensure_ascii=False)+'\n')
    print(json.dumps(result,ensure_ascii=False))
