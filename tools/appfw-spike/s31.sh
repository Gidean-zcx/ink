#!/usr/bin/env bash
# Compile and compare only. Never connect to a device.
set -euo pipefail
cd "$(dirname "$0")/../.."
: "${INK_DEPS:?Set INK_DEPS to the read-only dependency checkout}"
root="$PWD"
out="$root/work/appfw-spike/s31-evidence"
build="$root/build-s31-spike"
mkdir -p "$out" "$build/tmp" "$build/cache"
rm -f "$out/summary.json"
bash tools/appfw-spike/run.sh fetch
bash ports/esp32s31/build.sh > "$out/default-before.log" 2>&1
cp build-s31/ink-s31.map "$out/default-before.map"
cp build-s31/ink-s31.bin "$out/default-before.bin"
cp build-s31/size.txt "$out/default-before-size.txt"
export TMPDIR="$build/tmp" XDG_CACHE_HOME="$build/cache"
export PYTHONDONTWRITEBYTECODE=1 IDF_COMPONENT_CACHE_PATH="$build/cache/components"
export IDF_COMPONENT_MANAGER=0 CCACHE_DISABLE=1
: "${IDF_PATH:=$HOME/esp/esp-idf}"
: "${IDF_TOOLS_PATH:=$HOME/esp/.espressif}"
export IDF_PATH IDF_TOOLS_PATH
unset IDF_DEACTIVATE_FILE_PATH
. "$IDF_PATH/export.sh" > "$out/export.log" 2>&1
export INK_LVGL_SOURCE_DIR="$INK_DEPS/build/_deps/lvgl-src"
export CMAKE_BUILD_PARALLEL_LEVEL=2 IDF_PY_BUILD_JOBS=2 MAKEFLAGS=-j2
idf_limited() {
    python - "$IDF_PATH/tools/idf.py" "$@" <<'PY'
import os, runpy, sys
sys.path.insert(0, os.path.join(os.environ['IDF_PATH'], 'tools'))
from idf_py_actions.constants import GENERATORS
if 'Unix Makefiles' in GENERATORS:
    GENERATORS['Unix Makefiles']['command'] = ['make', '-j', '2']
sys.argv = sys.argv[1:]
runpy.run_path(sys.argv[0], run_name='__main__')
PY
}
cd ports/esp32s31
idf_limited --preview -B "$root/build-s31" size --format json2 --output-file "$out/default.json" > "$out/default-size-json.log" 2>&1
for candidate in c json lua mquickjs; do
    cp sdkconfig.defaults "$build/sdkconfig"
    idf_limited --preview -B "$build" -DIDF_TARGET=esp32s31 -DSDKCONFIG="$build/sdkconfig" \
        "-DCMAKE_PROJECT_ink-s31_INCLUDE=$root/tools/appfw-spike/s31-inject.cmake" \
        "-DINK_SPIKE_CANDIDATE=$candidate" reconfigure > "$out/$candidate-configure.log" 2>&1
    idf_limited --preview -B "$build" build > "$out/$candidate-build.log" 2>&1
    cp "$build/ink-s31.map" "$out/$candidate.map"
    idf_limited --preview -B "$build" size > "$out/$candidate-size.txt" 2>&1
    idf_limited --preview -B "$build" size --format json2 --output-file "$out/$candidate.json" > "$out/$candidate-size-json.log" 2>&1
    idf_limited --preview -B "$build" size --diff "$out/default-before.map" \
        --format json2 --output-file "$out/$candidate-diff.json" > "$out/$candidate-diff.log" 2>&1
    idf_limited --preview -B "$build" size-components > "$out/$candidate-components.txt" 2>&1
    echo "S31 $candidate: compiled, linked, size and map difference recorded"
done
cd "$root"
bash ports/esp32s31/build.sh > "$out/default-after.log" 2>&1
cmp "$out/default-before.bin" build-s31/ink-s31.bin
cmp "$out/default-before.map" build-s31/ink-s31.map
"$INK_DEPS/build-deps/pyenv/bin/python" tools/appfw-spike/s31_summary.py "$out"
