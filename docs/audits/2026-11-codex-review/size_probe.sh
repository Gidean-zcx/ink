#!/usr/bin/env bash
# AUD-016 probe: run run.sh's build_size() with a fake idf.py; one project fails (exit 7).
# usage: probe.sh <path to run.sh>
set -u
RUN_SH="$1"
WORK=$(mktemp -d); export WORK
mkdir -p "$WORK/bin" "$WORK/working" "$WORK/broken"
cat > "$WORK/bin/idf.py" <<'X'
#!/bin/sh
case "$PWD" in */broken) exit 7;; esac
printf "mock size\n"
X
chmod +x "$WORK/bin/idf.py"
export PATH="$WORK/bin:$PATH"
# the function definitions (build_size, and whatever helpers run.sh defines) and the final summary,
# without the clone/install/export steps
VARS=$(grep -E "^[A-Z_]+=\"\"$" "$RUN_SH" || true)
FUNCS=$(awk '/^[a-z_]+ \(\) *\{/{f=1} f{print} f&&/^}/{f=0}' "$RUN_SH")
TAIL=$(awk '/^echo$/{f=1} f{print}' "$RUN_SH")
bash -c "set -euo pipefail; cd \"\$WORK\"; $VARS
$FUNCS
build_size working working
build_size broken broken
$TAIL"
echo "exit=$?"
rm -rf "$WORK"
