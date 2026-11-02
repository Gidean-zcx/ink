#!/usr/bin/env bash
# 为 ESP32-S31 编译三个工程，输出静态内存占用（IRAM/DRAM/Flash），供底座设计 §3.2 自上而下列账使用。
#   1) ESP-IDF hello_world（系统基线）
#   2) ESP-IDF wifi station（加上 Wi-Fi/lwIP 的基线）
#   3) 上游 CloudZao Metalio E-INK4-Plus 固件（固定提交）
# 用法：在 macOS 或 Linux 上执行 `bash tools/s31-size/run.sh`，结果写到 .s31-size/*.txt 并汇总打印。
# 需要：git、python3、可访问 GitHub 与 PyPI 的网络。首次运行会下载约 1 GB 的 ESP-IDF 与工具链。
set -euo pipefail
WORK="${WORK:-$PWD/.s31-size}"
IDF_VER="${IDF_VER:-v6.1}"
UPSTREAM_COMMIT="${UPSTREAM_COMMIT:-690a023c66862af103a3187464373ee7c5efcd14}"
mkdir -p "$WORK" && cd "$WORK"

[ -d esp-idf ] || git clone --depth 1 --branch "$IDF_VER" --recurse-submodules --shallow-submodules https://github.com/espressif/esp-idf
./esp-idf/install.sh esp32s31
# shellcheck disable=SC1091
. ./esp-idf/export.sh

build_size () {  # $1=名称 $2=工程目录
  echo "== $1"
  if (cd "$2" && idf.py --preview set-target esp32s31 >"$WORK/$1.build.log" 2>&1 \
        && idf.py build >>"$WORK/$1.build.log" 2>&1 \
        && idf.py size >"$WORK/$1.size.txt" 2>&1 \
        && idf.py size-components >"$WORK/$1.components.txt" 2>&1); then
    echo "   ok"
  else
    echo "   failed，见 $WORK/$1.build.log"
  fi
}

rm -rf hello wifi
cp -r esp-idf/examples/get-started/hello_world hello && build_size hello hello
cp -r esp-idf/examples/wifi/getting_started/station wifi && build_size wifi wifi

[ -d upstream ] || git clone https://github.com/CloudZao/Metalio-E-INK4-Plus upstream
(cd upstream && git fetch -q --depth 1 origin "$UPSTREAM_COMMIT" && git checkout -q "$UPSTREAM_COMMIT")
# 上游 README 要求手动给 IDF 打的补丁（若已打过会跳过）
for p in upstream/patches/esp-idf/*.patch; do
  [ -f "$p" ] && (cd esp-idf && git apply --check "../$p" 2>/dev/null && git apply "../$p" && echo "applied $p") || true
done
build_size upstream upstream

echo
echo "===== 静态内存汇总 ====="
for f in "$WORK"/*.size.txt; do echo "--- $(basename "$f")"; cat "$f"; done
