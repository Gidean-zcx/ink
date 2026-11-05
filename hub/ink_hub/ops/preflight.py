import platform
import shutil
import socket
import subprocess
import sys
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
    directory.mkdir(parents=True, exist_ok=True)
    docker = shutil.which("docker")
    compose = False
    if docker:
        compose = subprocess.run([docker, "compose", "version"], capture_output=True, timeout=10).returncode == 0
    return {"os": platform.system(), "arch": platform.machine(), "python": platform.python_version(),
            "python_ok": sys.version_info[:3] == (3, 11, 15), "docker": bool(docker), "compose": compose,
            "ports": {str(p): port_free(p) for p in ports}, "disk_free_bytes": shutil.disk_usage(directory).free,
            "entry": "本机入口；远程机器请用 SSH 转发。未验证公网、安全组、证书和续期。"}
