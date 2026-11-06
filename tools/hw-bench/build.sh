#!/bin/bash
# Builds the bench image. Compiles only: it does not touch the device.
set -e
cd "$(dirname "$0")"
ROOT=$(cd ../../.. && pwd)
export IDF_TOOLS_PATH=${IDF_TOOLS_PATH:-$HOME/esp/.espressif}
export INK_UPSTREAM=${INK_UPSTREAM:-$ROOT/ink-hw/upstream/Metalio-E-INK4-Plus}
. "$HOME/esp/esp-idf/export.sh" > /dev/null
cp ../hw-probe/partitions.csv partitions.csv
export SDKCONFIG_DEFAULTS="$INK_UPSTREAM/sdkconfig;sdkconfig.bench"
[ -d build ] || idf.py --preview set-target esp32s31 > /dev/null
mkdir -p build
if [ -d "$ROOT/ink-design/png" ]; then
    "$ROOT/ink/build-deps/pyenv/bin/python" gen_images.py "$ROOT/ink-design/png" build/bench_images.h Main Settings Control Today
fi
idf.py build
ls -l build/ink-hw-bench.bin
