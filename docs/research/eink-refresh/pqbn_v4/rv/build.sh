#!/bin/bash
# RV32IM build of old/v3/v4 kernels + host benchmark drivers (needs clang/lld with the riscv32 target)
set -e
cd "$(dirname "$0")"
clang --target=riscv32-unknown-elf -march=rv32im -mabi=ilp32 -O2 -g -ffreestanding -fno-builtin -msmall-data-limit=0 -DMIRROR=1 -c kernels_rv4.c -o k4.o
ld.lld -T link.ld k4.o -o k4.elf && llvm-objcopy -O binary k4.elf k4.bin
gcc -O2 -o rvbench4 rvbench4.c ../sim/pqbn_lut.c ../kernel/pqbn_v4.c -x c - -lm <<'EOT'
#define MIRROR 1
#include "../kernel/pqbn_v3.c"
EOT
gcc -O2 -o rvhybrid rvhybrid.c ../kernel/pqbn_v4.c -lm
sym() { llvm-nm k4.elf | awk -v n=$1 '$3==n{print $1}'; }
echo "old_phase=$(sym old_phase) v3=$(sym pqbn_v3_phase) v4=$(sym pqbn_v4_phase) enter=$(sym pqbn_v4_motion_enter) exit=$(sym pqbn_v4_motion_exit) row=$(sym pqbn_v4_exec_row)"
cat <<EOT
scenarios:  mkdir -p scen && python3 gen_scenarios.py scen          (needs a CJK font, see FONT in the script)
verify:     ./rvbench4 k4.bin $(sym old_phase) $(sym pqbn_v3_phase) $(sym pqbn_v4_phase) scen 60 verify
benchmark:  ./rvbench4 k4.bin $(sym old_phase) $(sym pqbn_v3_phase) $(sym pqbn_v4_phase) scen 60
            BUDGET=10000 ./rvbench4 ...      (planner budget, px/phase)     LINE=$(sym pqbn_v4_exec_row) ./rvbench4 ...   (line mode)
            AUTO=1 BUDGET=20000 ./rvbench4 ...   (automatic mode selection)
fling:      BUDGET=20000 ./rvhybrid k4.bin $(sym pqbn_v4_phase) $(sym pqbn_v4_motion_enter) $(sym pqbn_v4_motion_exit) scen
            (PQ-BN only / scripted hybrid / automatic with the given budget)
EOT
