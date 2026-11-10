#!/usr/bin/env bash
# Builds the self-test image into tools/hw-selftest/build. Compiles only: it does not touch the device.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
B="$HERE/build"
mkdir -p "$B/tmp"
export TMPDIR="$B/tmp" PYTHONDONTWRITEBYTECODE=1 IDF_COMPONENT_MANAGER=0 CCACHE_DISABLE=1
: "${IDF_PATH:=$HOME/esp/esp-idf}"
: "${IDF_TOOLS_PATH:=$HOME/esp/.espressif}"
export IDF_PATH IDF_TOOLS_PATH
unset IDF_DEACTIVATE_FILE_PATH
. "$IDF_PATH/export.sh" > "$B/export.log" 2>&1
cd "$HERE"
[ -f "$B/CMakeCache.txt" ] || idf.py --preview -B "$B" -DIDF_TARGET=esp32s31 -DSDKCONFIG="$B/sdkconfig" reconfigure
cmake --build "$B" -j "${INK_JOBS:-2}" 2>&1 | tee "$B/build.log"
grep -q "warning:" "$B/build.log" && echo "WARNINGS in $B/build.log" || true
ls -l "$B/ink-hw-selftest.bin"
