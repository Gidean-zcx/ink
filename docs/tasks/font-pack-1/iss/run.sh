#!/usr/bin/env bash
# font-pack-1: the per glyph cost of unpacking on the ESP32-S31 model (tools/iss), before and after.
#   bash docs/tasks/font-pack-1/iss/run.sh <build dir with assets/ and assets-raw/ and the iss binary> <out dir>
# Needs riscv32-esp-elf-gcc (RV_GCC=, or ~/esp/.espressif/tools) and an LVGL checkout (LVGL_SRC=, default <build>/_deps/lvgl-src).
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../../../.." && pwd)
BUILD=$(cd "$1" && pwd)
OUT=$2
mkdir -p "$OUT/data"
RV_GCC=${RV_GCC:-$(find "$HOME/esp/.espressif/tools" -name riscv32-esp-elf-gcc -type f 2>/dev/null | sort | tail -n 1)}
LVGL_SRC=${LVGL_SRC:-$BUILD/_deps/lvgl-src}
cp "$BUILD/assets-raw/fonts/sans-44.inkf" "$OUT/data/v1.inkf"
cp "$BUILD/assets/fonts/sans-44.inkf" "$OUT/data/v2.inkf"
head -c 6000 "$BUILD/test-content/books/测试长书.txt" > "$OUT/data/book.txt"
RT=$REPO/tools/iss/rt
# The calibration kernels' flags (tools/iss/calibrate.sh): IDF's -march for the S31, -O2, IDF defaults.
"$RV_GCC" -march=rv32imafc_zicsr_zifencei_zaamo_zalrsc -mabi=ilp32f -mtune=esp-base -ffunction-sections -fdata-sections \
  -fstrict-volatile-bitfields -std=gnu17 -O2 -fno-jump-tables -fno-tree-switch-conversion -DLV_CONF_INCLUDE_SIMPLE \
  -I"$RT" -I"$REPO/platform/include" -I"$REPO/system/include" -I"$REPO/system/ui" -I"$LVGL_SRC" -Wa,-I"$OUT/data" \
  -nostartfiles -L"$RT" -Wl,--gc-sections -Wl,--no-warn-rwx-segments -T xip.ld "$RT/crt0.S" "$RT/syscalls.c" \
  "$HERE/data.S" "$HERE/glyphs.c" -lc -lm -lgcc -o "$OUT/glyphs.elf"
# The same program on this machine, for the simulator's measured CPU mode (host time x host_to_target_default).
cc -std=gnu17 -O2 -DLV_CONF_INCLUDE_SIMPLE -I"$REPO/platform/include" -I"$REPO/system/include" -I"$REPO/system/ui" -I"$LVGL_SRC" \
  -Wa,-I"$OUT/data" "$HERE/data.S" "$HERE/glyphs.c" -o "$OUT/glyphs-host" 2>/dev/null && "$OUT/glyphs-host" || echo "host build skipped"
"$BUILD/iss" -q -c "$REPO/tools/iss/out/esp32s31.fitted.cfg" -s icache.enable=0 -o "$OUT/report.json" "$OUT/glyphs.elf"
python3 - "$OUT/report.json" <<'EOF'
import json, sys
r = json.load(open(sys.argv[1]))
regions = {g['name']: g for g in r.get('regions', [])}
for name in ('reader', 'a8', 'scaled38'):
    a, b = regions[name + '.before'], regions[name + '.after']
    per = lambda g: g['cycles'] / g['units']
    print(f"{name:9s} before {per(a):8.0f} cycles/glyph  after {per(b):8.0f}  change {per(b) - per(a):+7.0f} ({100 * (per(b) / per(a) - 1):+.1f}%)")
after, table = regions['reader.after'], regions['reader.table']
print(f"unpacking alone (reader.after - reader.table, the same loop on the v1 face): "
      f"{after['cycles'] / after['units'] - table['cycles'] / table['units']:+.0f} cycles/glyph")
EOF
