"""Real capability bus and the supplied device scripts on the physical-input simulator."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve()
work = Path(tempfile.mkdtemp(prefix="autotest-", dir=build))
try:
    content = work / "content" / "books"
    content.mkdir(parents=True)
    shutil.copyfile(root / "tools/device/autotest/reader.txt", content / "自动测试长文.txt")
    shutil.copytree(build / "test-content" / "fonts", content.parent / "fonts")  # the reader's font, as the card has it
    for scene in ("cap", "camera", "reader", "memo"):
        script = root / ("tests/device_autotest/cap.script" if scene == "cap" else f"tools/device/autotest/{scene}.script")
        out = work / scene
        result = subprocess.run([str(build / "sim-autotest"), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
                                 "--script", str(script), "--assets", str(build / "assets"), "--content", str(content.parent),
                                 "--out", str(out), "--no-random-outages"], capture_output=True, text=True, timeout=120,
                                env={**os.environ, "INK_AUTOTEST_SCENE": scene})
        log = result.stdout + result.stderr
        (work / f"{scene}.log").write_text(log)
        assert result.returncode == 0, log[-4000:]
        metrics = json.loads((out / "metrics.json").read_text())
        counts = metrics["metrics"]
        assert counts.get("test.finished") == 1 and not counts.get("test.fail", 0), log[-9000:]
        if scene == "cap":
            assert "autotest: 2 cap source=test result=0" in log, log[-4000:]
            assert log.count("cap rejected source=test") == 5, log[-4000:]
            assert "FAKE_PRIVATE_" not in log
        if scene == "reader":
            assert counts.get("reader.page_turns") == 20, counts.get("reader.page_turns")
        if scene == "camera":
            assert counts.get("action.user.camera.preview", 0) >= 5
        if scene == "memo":
            assert counts.get("input.touches") == 11, counts.get("input.touches")
        print(f"autotest {scene}: passed", flush=True)
    commands = json.loads((build / "compile_commands.json").read_text())
    assert not any(Path(e["file"]).resolve() == root / "system/autotest/device.c" for e in commands)
    print("normal host excludes the device runner")
finally:
    if not os.environ.get("INK_KEEP_EVIDENCE"):
        shutil.rmtree(work)
