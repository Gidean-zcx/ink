"""Remove one F6 fix at a time; restore source and executable even on failure."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
ap = argparse.ArgumentParser()
ap.add_argument("--build", type=Path, required=True)
a = ap.parse_args()
build = a.build.resolve()
evidence = build / "activity-card-reverse"
evidence.mkdir(parents=True, exist_ok=True)
python = sys.executable
mutations = [
    ("media-card", "system/ui/shell/navigation.c", "if (!ink_activity_home_draw(N.content, y))", "if (true)", "home visibly renders"),
    ("call-return", "system/apps/phone/phone.c", 'if (!strcmp(id, "phone.return")) { ink_shell_close(); resume_call(); }',
     'if (!strcmp(id, "phone.return")) { ink_shell_close(); }', "return button reaches active call page"),
    ("remote-pause", "system/services/audio/media_service.c", 'static void need_session(ink_call_t *c, void (*act)(void)) {\n    if (!M.r)',
     'static void need_session(ink_call_t *c, void (*act)(void)) {\n    if ((c->src == INK_SRC_AI || c->src == INK_SRC_REMOTE) && ink_power_state() == INK_PWR_LOCKED)\n        fail(c, INK_RC_REJECTED, "wake the device first");\n    else if (!M.r)',
     "locked remote pause reaches the existing media capability"),
    ("media-title", "system/ui/shell/activity_card.c", "        char *ext = strrchr(A.title, '.');\n        if (ext && ext != A.title) *ext = 0;\n",
     "", "home exposes extension-free title and pause"),
    ("lock-mark", "system/ui/lock_mark.c", '"正在听，按电源键或 AI 键唤醒"', '"已锁定，按电源键或 AI 键唤醒"', "LockPlaying uses the listening mark"),
    ("job-cancel", "system/ui/shell/activity_card.c", 'ink_shell_bind(button, "job.cancel", r->args);',
     'ink_shell_bind(button, "job.list", r->args);', "touch invokes job.cancel and stops actual worker"),
    ("job-stage", "system/services/res/task.c", '    publish_running();\n}\n\nvoid ink_task_end',
     '}\n\nvoid ink_task_end', "open Tasks updates real stage without count change"),
]
commands = ["taskpolicy", "-c", "utility", "cmake", "--build", str(build), "--target", "sim-activity-card", "-j2"]
summary = []
try:
    for name, relative, old, new, expected in mutations:
        source = ROOT / relative
        original = source.read_text()
        assert original.count(old) == 1, name
        try:
            modified = original.replace(old, new, 1)
            if name == "remote-pause":
                modified = '#include "ink/sys/power.h"\n' + modified
            source.write_text(modified)
            compiled = subprocess.run(commands, cwd=ROOT, capture_output=True, text=True)
            (evidence / f"{name}-build.log").write_text(compiled.stdout + compiled.stderr)
            assert compiled.returncode == 0, name
            tested = subprocess.run(["taskpolicy", "-c", "utility", python, str(ROOT / "tests/activity_card.py"), "--build", str(build)],
                                    cwd=ROOT, capture_output=True, text=True)
            (evidence / f"{name}-test.log").write_text(tested.stdout + tested.stderr)
            metrics = json.loads((build / "test-out/activity-card/metrics.json").read_text())
            failures = [w for w in metrics.get("warnings", []) if "CHECK FAILED" in w]
            assert tested.returncode != 0 and any(expected in w for w in failures), (name, tested.returncode, failures)
            summary.append({"case": name, "build_exit": compiled.returncode, "test_exit": tested.returncode, "expected_failure": expected})
            print(f"{name}: removed fix fails at {expected}", flush=True)
        finally:
            source.write_text(original)
finally:
    result = subprocess.run(commands, cwd=ROOT, capture_output=True, text=True)
    (evidence / "restored-build.log").write_text(result.stdout + result.stderr)
    assert result.returncode == 0, "restore production executable"
(evidence / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
subprocess.run(["taskpolicy", "-c", "utility", python, str(ROOT / "tests/activity_card.py"), "--build", str(build)], cwd=ROOT, check=True)
