#!/usr/bin/env bash
# Compile only, with the unchanged factory partition layout; never connect a device.
set -euo pipefail
: "${INK_DEPS:?Set INK_DEPS to the read-only dependency checkout}"
: "${INK_UPSTREAM:?Set INK_UPSTREAM to the factory checkout at 690a023}"
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
BUILD="$ROOT/build-s31"
mkdir -p "$BUILD/tmp" "$BUILD/cache"
export TMPDIR="$BUILD/tmp" XDG_CACHE_HOME="$BUILD/cache" IDF_COMPONENT_CACHE_PATH="$BUILD/cache/components"
export PYTHONDONTWRITEBYTECODE=1 IDF_COMPONENT_MANAGER=0 CCACHE_DISABLE=1
export IDF_PATH="${IDF_PATH:-$HOME/esp/esp-idf}" IDF_TOOLS_PATH="${IDF_TOOLS_PATH:-$HOME/esp/.espressif}"
export INK_LVGL_SOURCE_DIR="$INK_DEPS/build/_deps/lvgl-src"
cp "$INK_UPSTREAM/partitions.csv" "$BUILD/audio-factory-partitions.csv"
python3 - "$ROOT/ports/esp32s31/sdkconfig.defaults" "$BUILD" <<'PY'
from pathlib import Path
import sys
source, build = Path(sys.argv[1]), Path(sys.argv[2])
lines = [s for s in source.read_text().splitlines() if not s.startswith('CONFIG_PARTITION_TABLE_')]
lines += ['CONFIG_PARTITION_TABLE_CUSTOM=y',
          'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="' + str(build / 'audio-factory-partitions.csv') + '"']
(build / 'sdkconfig').write_text('\n'.join(lines) + '\n')
PY
unset IDF_DEACTIVATE_FILE_PATH
. "$IDF_PATH/export.sh" > "$BUILD/export.log" 2>&1
cmake -S "$ROOT/ports/esp32s31" -B "$BUILD" -DIDF_TARGET=esp32s31 -DSDKCONFIG="$BUILD/sdkconfig"
cmake --build "$BUILD" -j2
python "$ROOT/tests/esp32s31/stack_report.py" "$BUILD" > "$BUILD/stack.txt"
