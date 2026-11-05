import argparse
import json
import os
from pathlib import Path

from aiohttp import web

from ink_hub.console.setup import Setup
from ink_hub.gateway.server import Hub
from ink_hub.ops.preflight import inspect


def main():
    parser = argparse.ArgumentParser(description="家庭服务器，第 1 步仅使用示例资料")
    parser.add_argument("command", choices=("serve", "doctor", "setup", "status"))
    parser.add_argument("--data", default=os.environ.get("INK_HUB_DATA", "data"))
    parser.add_argument("--host", default=os.environ.get("INK_HUB_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("INK_HUB_PORT", "8443")))
    parser.add_argument("--dev-allow-proto1", action="store_true", default=os.environ.get("INK_HUB_DEV_ALLOW_PROTO1") == "1")
    parser.add_argument("--config")
    args = parser.parse_args()
    if args.host != "127.0.0.1":
        parser.error("第 1 步只监听 127.0.0.1；远程设置请使用 SSH 转发")
    if any(os.environ.get("INK_HUB_" + key, "fake") != "fake" for key in ("MODEL", "ASR", "TTS")):
        parser.error("仅安装了示例厂商，不能接入真实服务")
    if args.command == "doctor":
        print(json.dumps(inspect(args.data), ensure_ascii=False))
    elif args.command == "setup":
        import yaml
        if not args.config:
            parser.error("请提供 --config file.yaml，只含 step 和 mode")
        values = yaml.safe_load(Path(args.config).read_text())
        try:
            print(json.dumps(Setup(args.data).update(values), ensure_ascii=False))
        except (ValueError, TypeError, AttributeError) as exc:
            parser.error(str(exc))
    elif args.command == "status":
        print(json.dumps(Setup(args.data).progress, ensure_ascii=False))
    else:
        hub = Hub(args.data, dev_allow_proto1=args.dev_allow_proto1)
        print(f"示例设置入口：http://127.0.0.1:{args.port}/setup", flush=True)
        print(f"本次设置码：{hub.setup.code}", flush=True)
        web.run_app(hub.application(), host=args.host, port=args.port, access_log=None, print=None)


if __name__ == "__main__":
    main()
