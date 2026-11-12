import argparse
import json
import logging
import os
import time
from pathlib import Path

from aiohttp import web

from ink_hub.console.setup import Setup
from ink_hub.gateway.server import Hub
from ink_hub.gateway.tls import identity, print_identity
from ink_hub.data.pairing import PairingStore
from ink_hub.ops.preflight import inspect


def main():
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    parser = argparse.ArgumentParser(description="家庭服务器，第 1 步仅使用示例资料")
    parser.add_argument("command", choices=("serve", "doctor", "setup", "status", "pin", "claim", "pair-file"))
    parser.add_argument("values", nargs="*")
    parser.add_argument("--data", default=os.environ.get("INK_HUB_DATA", "data"))
    parser.add_argument("--host", default=os.environ.get("INK_HUB_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("INK_HUB_PORT", "8443")))
    parser.add_argument("--dev-allow-proto1", action="store_true", default=os.environ.get("INK_HUB_DEV_ALLOW_PROTO1") == "1")
    parser.add_argument("--config")
    parser.add_argument("--lan", action="store_true")
    parser.add_argument("--lan-port", type=int, default=8444)
    parser.add_argument("--name", default="我的设备")
    parser.add_argument("--out")
    args = parser.parse_args()
    if args.host != "127.0.0.1":
        parser.error("第 1 步只监听 127.0.0.1；远程设置请使用 SSH 转发")
    if args.command == "pin":
        print_identity(identity(args.data, args.lan_port))
    elif args.command == "claim":
        from urllib.request import Request, build_opener, ProxyHandler
        from urllib.error import HTTPError
        if len(args.values) != 2:
            parser.error("请提供认领码和核对码")
        values = {"code": args.values[0], "check": args.values[1], "name": args.name}
        request = Request(f"http://127.0.0.1:{args.port}/claim", data=json.dumps(values).encode(),
                          headers={"Content-Type": "application/json"})
        try:
            with build_opener(ProxyHandler({})).open(request, timeout=5) as reply:
                print(reply.read().decode())
        except HTTPError:
            parser.error("认领失败，请核对认领码和核对码")
    elif args.command == "pair-file":
        if not args.out:
            parser.error("请提供 --out 路径")
        info = identity(args.data, args.lan_port)
        token = PairingStore(Path(args.data) / "hub.db", time.time).preauth()
        values = {"url": info["url"], "console": info["url"].replace("wss://", "https://").removesuffix("/v1/device"),
                  "pin": info["pin"], "preauth": token}
        with os.fdopen(os.open(args.out, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), "w") as output:
            json.dump(values, output, ensure_ascii=False)
        print("配对文件已生成，有效期 24 小时，请用后删除。")
    elif args.command == "doctor":
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
        info = identity(args.data, args.lan_port) if args.lan else None
        hub = Hub(args.data, dev_allow_proto1=args.dev_allow_proto1, lan=info)
        app = hub.application()
        if args.lan:
            print_identity(info)
            async def start_lan(application):
                runner = web.AppRunner(hub.tls_application(), access_log=None)
                await runner.setup()
                await web.TCPSite(runner, "0.0.0.0", args.lan_port, ssl_context=info["context"]).start()
                application.on_cleanup.append(lambda _: runner.cleanup())
            app.on_startup.append(start_lan)
        print(f"示例设置入口：http://127.0.0.1:{args.port}/setup", flush=True)
        print(f"本次设置码：{hub.setup.code}", flush=True)
        web.run_app(app, host=args.host, port=args.port, access_log=None,
                    print=lambda _: print("家庭服务器已就绪", flush=True))


if __name__ == "__main__":
    main()
