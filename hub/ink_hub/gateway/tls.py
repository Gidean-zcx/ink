"""Stable local TLS identity; the device pins the public key, not the certificate."""
import hashlib
import shutil
import socket
import ssl
import subprocess
from pathlib import Path


def loopback(address):
    return address in ("127.0.0.1", "::1")


def proto_allowed(proto, remote, secure, development):
    return proto == 2 or (proto == 1 and development and loopback(remote) and not secure)


def verification_code(pin):
    alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"
    bits = int(pin[:15], 16)
    return "".join(alphabet[(bits >> shift) & 31] for shift in range(55, -1, -5))


def identity(directory, port=8444):
    if not shutil.which("openssl"):
        raise RuntimeError("局域网入口需要系统 openssl，请先安装")
    root = Path(directory) / "tls"
    root.mkdir(parents=True, exist_ok=True, mode=0o700)
    cert, key = root / "cert.pem", root / "key.pem"
    if cert.exists() != key.exists():
        raise RuntimeError("TLS 证书或私钥缺失，请恢复原文件，不能自动更换公钥")
    def run(*args, data=None):
        try:
            return subprocess.run(["openssl", *map(str, args)], input=data, capture_output=True, check=True).stdout
        except subprocess.CalledProcessError:
            raise RuntimeError("TLS 证书处理失败") from None
    if not key.exists():
        key.touch(mode=0o600)
        run("req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "3650",
            "-subj", "/CN=ink-hub", "-keyout", key, "-out", cert)
    key.chmod(0o600)
    public = run("x509", "-in", cert, "-pubkey", "-noout")
    pin = hashlib.sha256(run("pkey", "-pubin", "-outform", "DER", data=public)).hexdigest()
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(cert, key)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        try:
            sock.connect(("192.0.2.1", 9))  # Route lookup only; no packet is sent.
            address = sock.getsockname()[0]
        except OSError:
            address = "127.0.0.1"
    return {"url": f"wss://{address}:{port}/v1/device", "pin": pin,
            "code": verification_code(pin), "context": context}


def print_identity(info):
    print("设备地址：" + info["url"], flush=True)
    print("公钥 SHA-256：" + info["pin"], flush=True)
    print("核对码：" + info["code"], flush=True)
