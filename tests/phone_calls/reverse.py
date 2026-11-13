"""Remove the history reader's member binding, then restore and recheck it."""
from pathlib import Path
import os
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve()
source = root / "system/apps/phone/phone.c"
original = source.read_bytes()
# phone-3: the recent calls page's own read (the dial pad and the details read the same history elsewhere)
needle = b"P.db = ink_db_open_cached_member(&HISTORY, P.member);\n    if (!P.db) {\n        set_text(P.notice, P.log_failed"
assert original.count(needle) == 1


def rebuild():
    result = subprocess.run(["taskpolicy", "-c", "utility", "cmake", "--build", str(build),
                             "--target", "sim-phone-calls", "-j2"], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr


def check(output):
    return subprocess.run([sys.executable, str(root / "tests/phone_calls/run.py"),
                           str(build / "sim-phone-calls"), str(root), str(build / "assets"),
                           str(build / "test-content"), str(output)],
                          env=dict(os.environ, INK_KEEP_EVIDENCE="1"), capture_output=True, text=True)


try:
    source.write_bytes(original.replace(needle, needle.replace(b"P.member)", b'"owner")')))

    rebuild()
    negative = check(build / "test-out/phone-reverse")
    (build / "phone-reverse.log").write_text(negative.stdout + negative.stderr)
    assert negative.returncode != 0 and "member has exactly 0 history records" in negative.stderr, negative.stderr
    print("Removed member binding: FAIL at the child's empty-history assertion, as required")
finally:
    source.write_bytes(original)
    rebuild()
positive = check(build / "test-out/phone-calls")
(build / "phone-restored.log").write_text(positive.stdout + positive.stderr)
assert positive.returncode == 0, positive.stderr
print("Restored member binding: PASS")
