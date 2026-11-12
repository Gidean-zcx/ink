"""Persist real Mogo reading state through the member store."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import importlib.util
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('mogo_page_fixture',ROOT/'tests/graft_mogo_page/run.py')
fixture=importlib.util.module_from_spec(spec); spec.loader.exec_module(fixture)
content,provenance=fixture.content,fixture.provenance

def member_content(build):
    root=content(build)
    owner=root/".m/owner"
    kid=root/".m/kid"
    shutil.copytree(owner,kid,dirs_exist_ok=True)
    # The child has no font of its own: the reader falls back to the family's one copy (/family/fonts).
    (root/"fonts").mkdir(exist_ok=True)
    shutil.move(str(kid/"fonts/InkSC.ttf"),str(root/"fonts/InkSC.ttf"))
    return root

def flow_provenance():
    import hashlib
    import re
    vendor=ROOT/'third_party/mogo'
    manifest=json.loads((vendor/'flow-upstream-files.json').read_text())
    source=(vendor/'main/apps/ink_book_page.c').read_text()
    for entry in manifest['files']:
        if 'path' in entry: data=(vendor/entry['path']).read_bytes()
        else:
            match=re.search(r'^static [^\n]*\b'+entry['function']+r'\([^;]*?\)\s*\{',source,re.M)
            assert match,entry['function']
            # The exact copied function is recorded by length, including compact one-line helpers.
            data=source[match.start():match.start()+entry['source_chars']].encode()
        assert hashlib.sha256(data).hexdigest()==entry.get('adapted_sha256',entry['sha256']),entry

def exercise(build, binary=None, reverse=False):
    provenance()
    flow_provenance()
    area=build/"test-out"/("mogo-flow-reverse" if reverse else "mogo-flow")
    if area.exists(): shutil.rmtree(area)
    area.mkdir(parents=True)
    with (area/"run.log").open("w") as log:
        result=subprocess.run([str(binary or build/"sim-mogo-flow"),"--profile",str(ROOT/"simulator/profiles/metalio-plus.json"),
            "--script",str(ROOT/"tests/graft_mogo_flow/scenario.script"),"--budget",str(ROOT/"tests/ui_scene.budget.json"),
            "--assets",str(build/"assets"),"--content",str(member_content(build)),"--out",str(area/"out"),
            "--flash-image",str(area/"user.img"),"--sd-dir",str(area/"sd"),"--no-random-outages"],
            cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=420,env={**os.environ,"UBSAN_OPTIONS":"halt_on_error=1"})
    data=json.loads((area/"out/metrics.json").read_text(errors="replace")) if (area/"out/metrics.json").exists() else {}
    counts=data.get("metrics",{})
    if reverse:
        assert result.returncode and counts.get("test.fail") and any("saved progress restores" in str(w) for w in data.get("warnings",[])), (area/"run.log").read_text(errors="replace")[-5000:]
    else:
        assert result.returncode==0 and counts.get("test.finished")==1 and not counts.get("test.fail"), (area/"run.log").read_text(errors="replace")[-6500:]
    print(f"Mogo member flow: {'reverse failed as required' if reverse else 'PASS'}; pass={counts.get('test.pass')} fail={counts.get('test.fail',0)} exit={result.returncode}",flush=True)
    if not os.environ.get("INK_KEEP_EVIDENCE"): shutil.rmtree(area)

if __name__=="__main__":
    p=argparse.ArgumentParser(); p.add_argument("--build",type=Path,required=True)
    a=p.parse_args(); exercise(a.build.resolve())
