import importlib.util
import json
import os
import socket
import subprocess
import sys
from pathlib import Path

import pytest
import yaml

from ink_hub.console.setup import Setup
from ink_hub.ops.preflight import inspect, port_free

ROOT = Path(__file__).resolve().parents[2]


def test_preflight_reports_occupied_port(tmp_path):
    with socket.socket() as sock:
        sock.bind(("0.0.0.0", 0))
        sock.listen()
        port = sock.getsockname()[1]
        assert not port_free(port)
        result = inspect(tmp_path, (port,))
        assert result["ports"][str(port)] is False
        assert result["python_ok"]


def test_setup_resumes_but_never_enables_real_data(tmp_path):
    setup = Setup(tmp_path)
    setup.update({"step": 2})
    assert Setup(tmp_path).progress == {"step": 2, "mode": "example"}
    assert setup.path.stat().st_mode & 0o777 == 0o600
    with pytest.raises(ValueError):
        setup.update({"mode": "real"})
    with pytest.raises(ValueError):
        setup.update({"api_key": "EXAMPLE_NOT_A_REAL_KEY"})


async def test_setup_trusted_entry_code_once_and_no_secrets(make_hub):
    hub, client, url = await make_hub()
    assert "设置家庭服务器" in await (await client.get(url + "/setup")).text()
    headers = {"X-Setup-Code": hub.setup.code, "Origin": "http://example.com"}
    assert (await client.post(url + "/setup", json={"step": 1}, headers=headers)).status == 403
    headers.pop("Origin")
    assert (await client.post(url + "/setup", json={"api_key": "EXAMPLE_NOT_A_REAL_KEY"}, headers=headers)).status == 400
    result = await client.post(url + "/setup", json={"step": 1}, headers=headers)
    assert result.status == 200
    assert (await client.post(url + "/setup", json={"step": 2}, headers=headers)).status == 403
    assert "EXAMPLE_NOT_A_REAL_KEY" not in hub.setup.path.read_text()


def test_compose_worker_is_isolated():
    config = yaml.safe_load((ROOT / "deploy/compose.yaml").read_text())
    worker = config["services"]["worker"]
    assert worker["network_mode"] == "none" and worker["read_only"]
    assert worker["user"] != "0" and "environment" not in worker
    assert all("data" not in v and "secret" not in v for v in worker["volumes"])
    assert worker["cap_drop"] == ["ALL"]
    assert worker["mem_limit"] == "800m"
    assert ":latest" not in (ROOT / "deploy/Dockerfile").read_text()


def test_install_dry_run_has_no_side_effects(tmp_path):
    target = tmp_path / "example-install"
    result = subprocess.run(["sh", str(ROOT / "deploy/install.sh"), "--prefix", str(target), "--dry-run"], capture_output=True, text=True)
    assert result.returncode == 0 and not target.exists()
    assert "SSH" in result.stdout and "127.0.0.1" in result.stdout


def test_install_with_mocked_docker_and_existing_target(tmp_path):
    bindir = tmp_path / "bin"
    bindir.mkdir()
    docker = bindir / "docker"
    docker.write_text("#!/bin/sh\nprintf '%s\\n' \"$*\" >> \"$INK_TEST_CALLS\"\n")
    docker.chmod(0o755)
    log = tmp_path / "calls"
    env = {**os.environ, "PATH": str(bindir) + os.pathsep + os.environ["PATH"], "INK_TEST_CALLS": str(log)}
    target = tmp_path / "example-install"
    args = ["sh", str(ROOT / "deploy/install.sh"), "--prefix", str(target), "--apply"]
    result = subprocess.run(args, env=env, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert (target / "worker/main.py").exists() and (target / "requirements.lock").exists()
    lines = log.read_text().splitlines()
    config_index = next(i for i, line in enumerate(lines) if "config --quiet" in line)
    assert "build" in lines[config_index + 1]
    assert "--network none" in lines[config_index + 2]
    assert "up -d hub" in lines[config_index + 3]
    assert subprocess.run(args, env=env, capture_output=True).returncode == 4


def test_worker_probe_has_bounded_input(tmp_path):
    spec = importlib.util.spec_from_file_location("ink_worker", ROOT / "worker/main.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    source, output = tmp_path / "input", tmp_path / "output"
    source.mkdir()
    (source / "example.txt").write_text("synthetic example")
    assert module.run(source, output)["bytes"] == 17
    (source / "example.txt").write_bytes(b"x" * (1024 * 1024 + 1))
    with pytest.raises(ValueError):
        module.run(source, output)


def test_cli_setup_doctor_and_public_bind_refused(tmp_path):
    config = tmp_path / "config.yaml"
    config.write_text("step: 1\nmode: example\n")
    args = [sys.executable, "-m", "ink_hub"]
    result = subprocess.run(args + ["setup", "--data", str(tmp_path), "--config", str(config)], capture_output=True, text=True)
    assert result.returncode == 0 and json.loads(result.stdout)["step"] == 1
    result = subprocess.run(args + ["serve", "--host", "0.0.0.0"], capture_output=True, text=True)
    assert result.returncode == 2
