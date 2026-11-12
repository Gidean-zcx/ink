#!/bin/sh
set -eu
# 78/esp-opus 1.0.5, BSD-3-Clause; retain COPYING in the extracted tree.
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
archive=${INK_DEPS:?请设置 INK_DEPS}/build-s31/esp-opus-1.0.5.zip
out=${1:-$root/work/hub-opus}
hash=$(cmake -E sha256sum "$archive")
if [ "${hash%% *}" != f04982787072427616f2ef86e228844d4912f0a6952a2ee597bb4dc6e5ced829 ]; then
    echo 'Opus 压缩包 SHA-256 不符' >&2
    exit 1
fi
mkdir -p "$out"
out=$(CDPATH= cd -- "$out" && pwd)
unzip -qo "$archive" -d "$out"
if command -v taskpolicy >/dev/null 2>&1; then
    set -- taskpolicy -c utility
else
    set --
fi
CFLAGS="-fPIC -I. ${CFLAGS:-}" "$@" make -C "$out/esp-opus" -f Makefile.unix -j2 FIXED_POINT=1 lib
case $(uname -s) in
    Darwin)
        "$@" cc ${LDFLAGS:-} -dynamiclib -Wl,-force_load,"$out/esp-opus/libopus.a" -lm -o "$out/libopus.dylib"
        ;;
    Linux)
        "$@" cc ${LDFLAGS:-} -shared -Wl,--whole-archive "$out/esp-opus/libopus.a" -Wl,--no-whole-archive -lm -o "$out/libopus.so"
        ;;
    *) echo '只支持 macOS 和 Linux' >&2; exit 1 ;;
esac
echo 'Opus 共享库已编好（输出目录中的 libopus.dylib 或 libopus.so）'
