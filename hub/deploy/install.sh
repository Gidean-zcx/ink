#!/bin/sh
set -eu

prefix=./inkhub-local
apply=0
install_docker=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --prefix) prefix=$2; shift 2 ;;
    --apply) apply=1; shift ;;
    --install-docker) install_docker=1; shift ;;
    --dry-run) apply=0; shift ;;
    *) echo '参数无效：--prefix DIR [--apply] [--install-docker]' >&2; exit 2 ;;
  esac
done
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$apply" -eq 0 ]; then
  echo '预演：检查 Docker Compose；不抢占 80、443、8443；复制本地安装包；先构建再启动。'
  echo '入口仅监听本机。SSH 转发：ssh -L 8443:127.0.0.1:8443 user@example.com'
  exit 0
fi
if ! command -v docker >/dev/null 2>&1; then
  if [ "$install_docker" -ne 1 ]; then
    echo '没有 Docker，尚未安装。Debian/Ubuntu 可显式加 --install-docker；Mac 请先安装 Docker。' >&2
    exit 3
  fi
  if [ "$(uname -s)" != Linux ]; then
    echo '自动安装仅支持 Debian/Ubuntu。' >&2; exit 3
  fi
  # Never executed by the local test suite; the administrator opts in explicitly.
  sudo apt-get update
  sudo apt-get install -y docker.io docker-compose-v2
fi
docker compose version >/dev/null
if [ -e "$prefix" ]; then
  echo '目标目录已存在，请选择新目录；不会覆盖已有安装。' >&2; exit 4
fi
mkdir -p "$prefix/data" "$prefix/work/example/input" "$prefix/work/example/output"
cp "$root/deploy/compose.yaml" "$prefix/compose.yaml"
cp "$root/.env.example" "$prefix/.env.example"
cp -R "$root/ink_hub" "$root/worker" "$prefix/"
cp "$root/deploy/Dockerfile" "$root/requirements.lock" "$prefix/"
docker compose -f "$prefix/compose.yaml" config --quiet
docker compose -f "$prefix/compose.yaml" build
docker compose -f "$prefix/compose.yaml" up -d hub
echo '示例服务已启动。设置码请从本机 docker compose logs hub 读取。'
echo 'SSH 转发：ssh -L 8443:127.0.0.1:8443 user@example.com'
