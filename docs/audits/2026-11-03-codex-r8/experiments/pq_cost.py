#!/usr/bin/env python3
import json, os, subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3];os.chdir(ROOT)
BUILD=ROOT/'build-audit';ELF=BUILD/'r8-pq-cost.elf'
GCC='/Users/bytedance/esp/.espressif/tools/riscv32-esp-elf/esp-15.2.0_20251204/riscv32-esp-elf/bin/riscv32-esp-elf-gcc'
cmd=[GCC,'-march=rv32imafc_zicsr_zifencei_zaamo_zalrsc','-mabi=ilp32f','-mtune=esp-base','-ffunction-sections','-fdata-sections','-fstrict-volatile-bitfields','-std=gnu17','-O2','-fno-jump-tables','-fno-tree-switch-conversion','-DPQBN_HOT=__attribute__((section(".iram.text"),noinline))','-Itools/iss/rt','-Idrivers/display/pqbn','-nostartfiles','-Ltools/iss/rt','-Wl,--gc-sections','-Wl,--no-warn-rwx-segments','-T','xip.ld','tools/iss/rt/crt0.S','tools/iss/rt/syscalls.c',str(HERE/'pq_cost.c'),'drivers/display/pqbn/pqbn_v4.c','-lc','-lm','-lgcc','-o',str(ELF)]
dest=HERE/'pq-cost-results.json'
rows=json.loads(dest.read_text()) if dest.exists() else []
def run(label,cmd):
    p=subprocess.run(cmd,capture_output=True,text=True,timeout=55)
    row=dict(label=label,command=cmd,exit_code=p.returncode,output=(p.stdout+p.stderr).splitlines())
    rows.append(row);(HERE/'pq-cost-results.json').write_text(json.dumps(rows,indent=2)+'\n')
    print(json.dumps(row),flush=True)
    if p.returncode: raise SystemExit(p.returncode)
run('compile',cmd)
for mode in ['fitted','slow']:
    out=BUILD/('r8-pq-cost-'+mode+'.json')
    cmd=[str(BUILD/'iss'),'-c','tools/iss/out/esp32s31.fitted.cfg','-o',str(out)]
    if mode=='slow':
        for x in ['lat.load_sub=3','lat.mul=2','br.mispredict=3','br.jal_nobtb=3','dcache.hit_extra=1','mem.sram_extra=1','psram.fill=287.6','psram.writeback=328.6']:cmd+=['-s',x]
    cmd+=[str(ELF)];run(mode,cmd)
    d=json.loads(out.read_text());rows[-1]['regions']=[{k:r[k] for k in ['name','units','aux_units','cycles','cycles_per_unit']}|{'instret':r['stats']['instret']} for r in d['regions']]
    (HERE/'pq-cost-results.json').write_text(json.dumps(rows,indent=2)+'\n')
    print(json.dumps(rows[-1]['regions']),flush=True)
