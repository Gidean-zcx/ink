#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
: "${INK_DEPS:?Set INK_DEPS to the read-only dependency checkout}"
action="${1:-all}"
spike_build="${INK_SPIKE_BUILD:-build-spike}"
spike_out="${INK_SPIKE_OUT:-work/appfw-spike/evidence}"
mkdir -p work/appfw-spike

fetch() {
  local archive=work/appfw-spike/lua-5.4.8.tar.gz
  if [[ ! -f "$archive" ]]; then
    curl -fL --connect-timeout 15 --max-time 60 https://www.lua.org/ftp/lua-5.4.8.tar.gz -o "$archive"
  fi
  "$INK_DEPS/build-deps/pyenv/bin/python" - "$archive" <<'PY'
import hashlib, sys
from pathlib import Path
assert hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest() == '4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae'
PY
  if [[ ! -d work/appfw-spike/lua-5.4.8 ]]; then
    tar -xzf "$archive" -C work/appfw-spike
  fi
}
configure() {
  cmake -S . -B "$spike_build" -DINK_WERROR=ON -DINK_GUI=OFF \
    -DINK_SANITIZE="${INK_SPIKE_SANITIZE:-OFF}" \
    -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
    -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
    -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" \
    -DCMAKE_PROJECT_ink_INCLUDE="$PWD/tools/appfw-spike/inject.cmake"
}
build() { cmake --build "$spike_build" --target appfw-c appfw-json appfw-lua -j4; }
measure() {
  "$INK_DEPS/build-deps/pyenv/bin/python" tools/appfw-spike/measure.py --build "$spike_build" --out "$spike_out" --samples "${INK_SPIKE_SAMPLES:-5}"
}
case "$action" in
  fetch) fetch ;;
  configure) configure ;;
  build) build ;;
  measure) measure ;;
  all) fetch; configure; build; measure ;;
  *) echo 'usage: bash tools/appfw-spike/run.sh [all|fetch|configure|build|measure]' >&2; exit 2 ;;
esac
