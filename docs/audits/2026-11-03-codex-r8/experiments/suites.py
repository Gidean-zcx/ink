#!/usr/bin/env python3
"""Offline builds; logs/runtime outputs remain in TASK-authorized build directories."""
import json
import os
from pathlib import Path
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
DEP = Path('/Users/bytedance/Documents/Workspace/ink')
os.chdir(ROOT)
dest = HERE / 'suite-steps.json'
steps = json.loads(dest.read_text()) if dest.exists() else []

def run(label, cmd, logfile):
    started = time.monotonic()
    with open(logfile, 'w') as log:
        rc = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT,
                            env={**os.environ, 'SDL_VIDEODRIVER': 'dummy'}).returncode
    row = dict(label=label, command=cmd, exit_code=rc, elapsed_s=round(time.monotonic()-started, 3), log=str(logfile))
    steps.append(row)
    dest.write_text(json.dumps(steps, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(row), flush=True)
    if rc:
        print(Path(logfile).read_text()[-6000:], flush=True)
        raise SystemExit(rc)

mode = sys.argv[1]
build = Path('build-audit-asan' if mode == 'asan' else 'build-audit/before' if mode == 'before' else 'build-audit')
build.mkdir(parents=True, exist_ok=True)
source = '.'
if mode == 'before':
    source = 'build-audit/before-src'
    Path(source).mkdir(exist_ok=False)
    archive = build / 'source.tar'
    with archive.open('wb') as out:
        subprocess.run(['git', 'archive', '06a882fb7c39ddaac41a111c161e2f26c9798070',
                        'CMakeLists.txt','apps','system','platform','drivers','boards','ports','simulator','tests','tools','third_party'],
                       stdout=out, check=True)
    subprocess.run(['tar','-xf',str(archive),'-C',source], check=True)
args = ['cmake','-S',source,'-B',str(build),'-DINK_WERROR=ON','-DFETCHCONTENT_FULLY_DISCONNECTED=ON',
        f'-DINK_CJK_FONT={DEP}/build-deps/fonts/NotoSansCJK-Regular.ttc','-DINK_CJK_FACE=2',
        f'-DPython3_EXECUTABLE={DEP}/build-deps/pyenv/bin/python',
        f'-DFETCHCONTENT_SOURCE_DIR_LVGL={DEP}/build/_deps/lvgl-src',
        f'-DFETCHCONTENT_SOURCE_DIR_IMGUI={DEP}/build/_deps/imgui-src',
        f'-DFETCHCONTENT_SOURCE_DIR_SDL2={DEP}/build/_deps/sdl2-src']
if mode != 'current': args += ['-DINK_GUI=OFF']
if mode == 'asan': args += ['-DINK_SANITIZE=ON']
run(mode+'-configure', args, build/'r8-configure.log')
run(mode+'-build', ['cmake','--build',str(build),'-j','8'], build/'r8-build.log')
args = [str(DEP/'build-deps/pyenv/bin/ctest'),'--test-dir',str(build),'-j','8','--timeout','300','--output-on-failure']
if mode == 'before': args += ['-R','^scenario-(ai_ask|standby|reader_basic)-metalio-plus(-pqbn)?$']
run(mode+'-ctest', args, build/'r8-ctest.log')
