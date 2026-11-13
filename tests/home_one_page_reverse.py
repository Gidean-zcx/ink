"""Reverse checks for home-1: take one part of the feature out, the tests must fail; then put it back.

Run from the repository root: python3 tests/home_one_page_reverse.py [ctest]. Manual only, not in ctest. Each mutation is applied to a copy of the source text, the two test programs are
rebuilt, the named tests run, and the original text is written back whatever happens."""
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path.cwd()
CTEST = sys.argv[1] if len(sys.argv) > 1 else "ctest"  # the ctest that goes with the build
HOME, TODAY = ROOT / "system/ui/shell/shell_home.c", ROOT / "system/ui/shell/today_edit.c"
MUTATIONS = [
    ("pages-turn-again", HOME,
     "bool ink_shell_home_swipe(ink_swipe_dir_t dir) { (void)dir; return false; }",
     "bool ink_shell_home_swipe(ink_swipe_dir_t dir) { ink_receipt_t rc; return dir == INK_SWIPE_LEFT && "
     "ink_cap_invoke(\"shell.apps\", NULL, INK_SRC_TOUCH, 0, &rc) == INK_RC_DONE; }",
     ["home-one-page"]),
    ("empty-card-drawn", HOME,
     "    ink_card_t failed = {0}; /* a failed read is said, not shown as nothing */",
     "    if (!seen.count) { ink_card_t none = {0}; snprintf(none.title, sizeof none.title, \"接下来\");\n"
     "        snprintf(none.text, sizeof none.text, \"还没有接下来的事\"); card_row(\"calendar\", &none, y, false); }\n"
     "    ink_card_t failed = {0}; /* a failed read is said, not shown as nothing */",
     ["ui-scene-Main"]),
    ("field-goes-elsewhere", HOME,
     "    ink_cap_invoke(\"shell.search\", args, INK_SRC_TOUCH, 0, &rc);",
     "    ink_cap_invoke(\"search.show\", args, INK_SRC_TOUCH, 0, &rc);",
     ["home-one-page"]),
    ("one-family-six", TODAY,
     "    T.saved = T.apps_saved = false; T.count = T.app_count = 0;\n    ink_db_t *db = ink_db_open_member(&TODAY_DB, ink_member_current()->id);",
     "    T.saved = T.apps_saved = false; T.count = T.app_count = 0;\n    ink_db_t *db = ink_db_open_member(&TODAY_DB, \"owner\");",
     ["home-one-page"]),
]


def build():
    r = subprocess.run(["taskpolicy", "-c", "utility", "cmake", "--build", "build", "-j3", "--target",
                        "sim-home-one-page", "sim-ui-scenes"], capture_output=True, text=True)
    if r.returncode:
        print(r.stdout[-3000:], r.stderr[-3000:])
    return r.returncode == 0


def run(tests):
    pattern = "^(" + "|".join(tests) + ")$"
    r = subprocess.run(["taskpolicy", "-c", "utility", CTEST, "--test-dir", "build", "-R", pattern],
                       capture_output=True, text=True)
    failed = "tests failed" in r.stdout and "0 tests failed" not in r.stdout
    warnings = []
    for t in tests:
        out = {"home-one-page": "home-one-page", "ui-scene-Main": "ui-scene-Main"}[t]
        m = ROOT / "build/test-out" / out / "metrics.json"
        if m.exists():
            warnings += [w.split(") ", 1)[-1][:120] for w in json.loads(m.read_bytes().decode("utf-8", "replace")).get("warnings", [])]
    return failed, warnings


results = []
for name, path, old, new, tests in MUTATIONS:
    original = path.read_text()
    assert original.count(old) == 1, (name, "anchor not found once")
    try:
        path.write_text(original.replace(old, new))
        assert build(), name
        failed, warnings = run(tests)
    finally:
        path.write_text(original)
    results.append({"mutation": name, "tests": tests, "failed_as_expected": failed, "first_failures": warnings[:3]})
    print(json.dumps(results[-1], ensure_ascii=False), flush=True)
assert build(), "restored build"
failed, warnings = run(["home-one-page", "ui-scene-Main"])
print(json.dumps({"restored": True, "tests_pass_again": not failed, "warnings": warnings}, ensure_ascii=False))
sys.exit(0 if all(r["failed_as_expected"] for r in results) and not failed else 1)
