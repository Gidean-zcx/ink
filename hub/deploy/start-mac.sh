#!/usr/bin/env bash
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
export PYTHONPATH="$ROOT/hub${PYTHONPATH:+:$PYTHONPATH}"
exec "${INK_HUB_PYTHON:-python3}" - "$@" <<'PY'
import json
import os
import stat
import sys
from pathlib import Path

path = Path(os.environ.get("INK_HUB_ENV_FILE", str(Path.home() / "Documents/Workspace/ink-hw/private/hub.env")))
if stat.S_IMODE(path.stat().st_mode) & 0o177:
    print("密钥文件权限比 600 宽，请收紧权限。", file=sys.stderr)
with path.open() as source:
    for line in source:
        if line.startswith("DEEPSEEK_API_KEY="):
            os.environ["DEEPSEEK_API_KEY"] = line.rstrip("\r\n").split("=", 1)[1]
            break
if sys.argv[1:] == ["--check"]:
    from ink_hub.models import from_environment
    model = from_environment()
    print(json.dumps({"provider": model.provider if model else "fake", "base": model.base if model else "",
                      "model": model.model if model else ""}, ensure_ascii=False))
else:
    os.execv(sys.executable, [sys.executable, "-m", "ink_hub", "serve", "--lan", "--data",
                            os.environ.get("INK_HUB_DATA", "work/hub-data"), *sys.argv[1:]])
PY
