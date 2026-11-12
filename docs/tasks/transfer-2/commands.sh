#!/usr/bin/env bash
# Commands used by REPORT.md; host only, no device operations.
set -euo pipefail
: "${INK_DEPS:?Set the read-only dependency checkout}"
py="$INK_DEPS/build-deps/pyenv/bin/python"
ctest="$INK_DEPS/build-deps/pyenv/bin/ctest"
run() { if command -v taskpolicy >/dev/null; then taskpolicy -c utility "$@"; else "$@"; fi; }
configure() {
    run cmake -S . -B "$1" -DINK_WERROR=ON \
        -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
        -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
        -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
        -DPython3_EXECUTABLE="$py" -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" "${@:2}"
}
case "${1:-}" in
main)
    configure build
    run cmake --build build -j2
    run "$ctest" --test-dir build --output-on-failure -j2
    ;;
host)
    run cmake -S ports/esp32s31/tests -B build-s31-host
    run cmake --build build-s31-host -j2
    run "$ctest" --test-dir build-s31-host --output-on-failure -j2
    run node docs/tasks/transfer-2/check-transfer.cjs build-s31-host/transfer2-fixtures
    run build-s31-host/test-s31-transfer build-s31-host/transfer2-fixtures
    ;;
s31)
    INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" run bash ports/esp32s31/build.sh
    ;;
asan)
    run cmake -S ports/esp32s31/tests -B build-s31-host-asan -DINK_SANITIZE=ON
    run cmake --build build-s31-host-asan -j2
    run "$ctest" --test-dir build-s31-host-asan --output-on-failure -j2
    configure build-asan -DINK_SANITIZE=ON
    run cmake --build build-asan --target ink_fonts ink_ui_fonts ink_icons ink_test_content sim-wireless-transfer sim-transfer-pages sim-ui-fix-a-transfer -j2
    run "$ctest" --test-dir build-asan --output-on-failure -j2 -R '^(integration-wireless-transfer|transfer-pages|ui-fix-a-transfer)$'
    ;;
visual)
    run "$py" tests/ui_fix_a/run.py --build build --case transfer --evidence docs/tasks/transfer-2/screenshots/after
    run "$py" docs/tasks/transfer-2/reverse-ui.py build
    ;;
*) printf '%s\n' 'Usage: commands.sh main|host|s31|asan|visual' >&2; exit 2 ;;
esac
