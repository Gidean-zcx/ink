#!/usr/bin/env bash
# Compile only; all generated state stays in this checkout.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="$ROOT/build-s31"
mkdir -p "$BUILD/tmp" "$BUILD/cache"
export TMPDIR="$BUILD/tmp" XDG_CACHE_HOME="$BUILD/cache"
export PYTHONDONTWRITEBYTECODE=1 IDF_COMPONENT_CACHE_PATH="$BUILD/cache/components"
export IDF_COMPONENT_MANAGER=0 CCACHE_DISABLE=1
: "${IDF_PATH:=$HOME/esp/esp-idf}"
: "${IDF_TOOLS_PATH:=$HOME/esp/.espressif}"
export IDF_PATH IDF_TOOLS_PATH
unset IDF_DEACTIVATE_FILE_PATH
. "$IDF_PATH/export.sh" > "$BUILD/export.log" 2>&1
# IDF v6.1 hardcodes cpu_count+2 for Makefiles. Limit that fallback in memory.
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
if [[ -z ${INK_LVGL_SOURCE_DIR:-} ]]; then
    if [[ -n ${INK_DEPS:-} && -d "$INK_DEPS/build/_deps/lvgl-src" ]]; then
        export INK_LVGL_SOURCE_DIR="$INK_DEPS/build/_deps/lvgl-src"
    else
        [[ -d "$BUILD/lvgl" ]] || git clone --depth 1 --branch v9.5.0 https://github.com/lvgl/lvgl.git "$BUILD/lvgl"
        export INK_LVGL_SOURCE_DIR="$BUILD/lvgl"
    fi
fi
cd "$ROOT/ports/esp32s31"
cp sdkconfig.defaults "$BUILD/sdkconfig"
idf_limited --preview -B "$BUILD" -DIDF_TARGET=esp32s31 -DSDKCONFIG="$BUILD/sdkconfig" \
    -DINK_S31_LIGHT_SLEEP="${INK_S31_LIGHT_SLEEP:-OFF}" reconfigure
export CMAKE_BUILD_PARALLEL_LEVEL=2 IDF_PY_BUILD_JOBS=2 MAKEFLAGS=-j2
idf_limited --preview -B "$BUILD" build
idf_limited --preview -B "$BUILD" size | tee "$BUILD/size.txt"
idf_limited --preview -B "$BUILD" size-components > "$BUILD/size-components.txt"
python "$ROOT/tests/esp32s31/stack_report.py" "$BUILD" > "$BUILD/stack.txt"
