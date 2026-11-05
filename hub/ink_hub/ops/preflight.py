import platform
import shutil
import socket
import subprocess
import sys
import os
import time
from pathlib import Path


def port_free(port):
    with socket.socket() as sock:
        try:
            sock.bind(("0.0.0.0", port))
            return True
        except OSError:
            return False


def inspect(directory, ports=(80, 443, 8443)):
    directory = Path(directory)
    while not directory.exists():
        directory = directory.parent
    docker = shutil.which("docker")
    compose = False
    if docker:
        compose = subprocess.run([docker, "compose", "version"], capture_output=True, timeout=10).returncode == 0
    memory = None
    try:
        if platform.system() == "Darwin":
            memory = int(subprocess.check_output(["sysctl", "-n", "hw.memsize"], text=True, stderr=subprocess.DEVNULL, timeout=5))
        elif hasattr(os, "sysconf"):
            memory = os.sysconf("SC_PHYS_PAGES") * os.sysconf("SC_PAGE_SIZE")
    except (OSError, ValueError, subprocess.SubprocessError):
        pass  # Restricted hosts must report unknown rather than invent a capacity.
    return {"os": platform.system(), "arch": platform.machine(), "python": platform.python_version(),
            "python_ok": sys.version_info[:3] == (3, 11, 15), "docker": bool(docker), "compose": compose,
            "ports": {str(p): port_free(p) for p in ports}, "disk_free_bytes": shutil.disk_usage(directory).free,
            "cpu_count": os.cpu_count(), "memory_bytes": memory, "clock_plausible": time.time() >= 1767225600,
            "entry": "本机入口；远程机器请用 SSH 转发。未验证公网、安全组、证书和续期。"}


if __name__ == "__main__":
    import argparse
    import json
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", default=".")
    parser.add_argument("--check-install", action="store_true")
    args = parser.parse_args()
    report = inspect(args.directory)
    print(json.dumps(report, ensure_ascii=False))
    if args.check_install and (not report["ports"]["8443"] or not report["compose"]):
        print("8443 被占用或 Compose 不可用；请调整已有反向代理或换用本机 Python 开发模式。", file=sys.stderr)
        sys.exit(3)
