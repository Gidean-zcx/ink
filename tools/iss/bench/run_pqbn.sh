#!/usr/bin/env bash
# PQ-BN v4 on the ISS (ESP32-S31 model): builds tools/iss/bench/pqbn_bench.c with the ESP-IDF S31
# flags, runs the research's scenarios at the fitted and the slow timing corner, in line mode
# (planner per phase + executor per row) and in drive-plane mode (the research's own numbers),
# and summarizes with summarize_pqbn.py.
#
#   tools/iss/bench/run_pqbn.sh SCENARIO_DIR OUT_DIR
#
# SCENARIO_DIR: *.bin from docs/research/eink-refresh/pqbn_v4/rv/gen_scenarios.py (needs numpy,
# Pillow, scikit-image and Noto Sans CJK). Needs the iss binary (cmake target `iss`, or
# tools/iss/build/iss from calibrate.sh) and riscv32-esp-elf-gcc (RV_GCC=..., as calibrate.sh).
set -euo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ISSDIR=$(cd "$HERE/.." && pwd)
REPO=$(cd "$ISSDIR/../.." && pwd)
SCEN=${1:?scenario dir}
OUT=${2:?output dir}
mkdir -p "$OUT"

if [ -z "${RV_GCC:-}" ]; then
  RV_GCC=$(command -v riscv32-esp-elf-gcc || true)
  for d in "$HOME/.espressif/tools" /home/claude/sim/.espressif/tools; do
    [ -n "$RV_GCC" ] && break
    [ -d "$d" ] && RV_GCC=$(find "$d" -name riscv32-esp-elf-gcc -type f 2>/dev/null | sort | tail -n 1)
  done
fi
[ -x "${RV_GCC:-}" ] || { echo "run_pqbn: riscv32-esp-elf-gcc not found (set RV_GCC)" >&2; exit 1; }
ISS=${ISS:-}
for c in "$ISS" "$REPO/build/iss" "$ISSDIR/build/iss"; do [ -n "$c" ] && [ -x "$c" ] && { ISS=$c; break; }; done
[ -x "${ISS:-}" ] || { echo "run_pqbn: iss not found (cmake --build build --target iss)" >&2; exit 1; }

# the calibration build's flags (calibrate.sh): IDF -march for the S31 without xesploop/xespv
TFLAGS="-march=rv32imafc_zicsr_zifencei_zaamo_zalrsc -mabi=ilp32f -mtune=esp-base -ffunction-sections -fdata-sections -fstrict-volatile-bitfields -std=gnu17 -O2 -fno-jump-tables -fno-tree-switch-conversion"
LDFLAGS="-nostartfiles -L$ISSDIR/rt -Wl,--gc-sections -Wl,--no-warn-rwx-segments -T xip.ld"
build() { # name defines...
  local name=$1; shift
  # shellcheck disable=SC2086
  "$RV_GCC" $TFLAGS "$@" -I"$ISSDIR/rt" -I"$REPO/drivers/display/pqbn" $LDFLAGS "$ISSDIR/rt/crt0.S" "$ISSDIR/rt/syscalls.c" \
    "$HERE/pqbn_bench.c" "$REPO/drivers/display/pqbn/pqbn_v4.c" -lc -lm -lgcc -o "$OUT/$name.elf"
}
# the executor in IRAM as in the port (PQBN_HOT); the planner stays in flash (XIP)
HOT='-DPQBN_HOT=__attribute__((section(".iram.text"),noinline))'
build line -DLINE=1 "$HOT"
build plane -DLINE=0 "$HOT"

FIT=$ISSDIR/out/esp32s31.fitted.cfg
# slow corner = calib.py SLOW_CORE + the PSRAM fit against ESP32-S3 rates (out/costs.json fit.psram.slow)
SLOW="-s lat.load_sub=3 -s lat.mul=2 -s br.mispredict=3 -s br.jal_nobtb=3 -s dcache.hit_extra=1 -s mem.sram_extra=1 -s psram.fill=287.6 -s psram.writeback=328.6"
for s in static ai_stream page_turn scroll_half scroll_full scroll_full_14fps covers_scroll; do
  [ -f "$SCEN/$s.bin" ] || { echo "missing $SCEN/$s.bin" >&2; exit 1; }
  for mode in line plane; do
    "$ISS" -c "$FIT" -i "$SCEN/$s.bin" -o "$OUT/$s.$mode.fitted.json" "$OUT/$mode.elf" > "$OUT/$s.$mode.fitted.txt" 2>> "$OUT/iss.log"
    # shellcheck disable=SC2086
    "$ISS" -c "$FIT" $SLOW -i "$SCEN/$s.bin" -o "$OUT/$s.$mode.slow.json" "$OUT/$mode.elf" > "$OUT/$s.$mode.slow.txt" 2>> "$OUT/iss.log"
  done
  echo "  $s done"
done
python3 "$HERE/summarize_pqbn.py" "$OUT" | tee "$OUT/summary.md"
