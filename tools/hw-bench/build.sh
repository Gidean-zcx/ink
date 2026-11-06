#!/bin/bash
# Builds the bench image; "build.sh lab" builds the one that tries our own refresh (build-lab/).
# Compiles only: it does not touch the device.
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
B=build
LAB=""
if [ "$1" = lab ]; then
    B=build-lab
    LAB=--lab
    export INK_BENCH_LAB=1
    mkdir -p $B
fi
# The real screens, as the simulator's tests drew them, without its modelled residue: run ctest first.
# child- is what a child sees; the top text size has its own test.
OUT=$ROOT/ink/build/test-out
PICS=""
for n in Main Settings LockPage Confirm child-Main child-Today Today; do
    d=$OUT/ui-scene-$n/clean-scene.png
    [ -f "$d" ] && PICS="$PICS $n=$d"
done
"$ROOT/ink/build-deps/pyenv/bin/python" gen_images.py $LAB "$ROOT/ink/build-deps/fonts/NotoSansCJK-Regular.ttc" $B/bench_images.h $PICS
idf.py -B $B build
ls -l $B/ink-hw-bench.bin
