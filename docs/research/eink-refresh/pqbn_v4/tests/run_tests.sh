#!/bin/bash
# equivalence and invariant tests for the v4 kernel (run from tests/)
set -e
cd "$(dirname "$0")"
gcc -O1 -c ../kernel/pqbn_v3.c -DMIRROR=1 -o v3.o
gcc -O1 -c ../kernel/pqbn_v4.c -o v4.o
gcc -O1 -c ../kernel/pqbn_v4a.c -o v4a.o
gcc -O1 eq_v3_v4.c v3.o v4.o -lm -o eq_v3_v4 && ./eq_v3_v4 0 20000          # stiction off: v4 drive == v3 drive
gcc -O1 eq_v4ab.c v4.o v4a.o -lm -o eq_v4ab && ./eq_v4ab 30000              # optimized planner == reference planner
gcc -O1 motion_test.c v4.o -lm -o motion_test && ./motion_test             # motion mode positions, DC, per-line executor
gcc -O1 auto_test.c v4.o -lm -o auto_test && ./auto_test                  # automatic mode selection (page turn / scroll / cursor)
