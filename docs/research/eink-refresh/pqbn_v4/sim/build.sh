#!/bin/bash
# shared libraries the simulator loads (run from sim/)
set -e
cd "$(dirname "$0")"
gcc -O2 -shared -fPIC -o libkernels.so kernels.c -lm
gcc -O2 -shared -fPIC -o libpqbn.so pqbn_fixed.c -lm
gcc -O2 -shared -fPIC -o libpqbnlut.so pqbn_lut.c -lm
gcc -O2 -shared -fPIC -o libpqfixed.so pq_fixed.c -lm
gcc -O2 -shared -fPIC -o libpqbnv2.so pqbn_v2.c -lm
gcc -O2 -shared -fPIC -DMIRROR=1 -o libpqbnv3.so ../kernel/pqbn_v3.c -lm
gcc -O2 -shared -fPIC -o libpqbnv4.so ../kernel/pqbn_v4.c -lm
echo "built; next: python3 make_scenes.py && python3 run_v4_q.py && python3 run_fling.py"
