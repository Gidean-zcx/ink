#!/usr/bin/env python3
"""Reverse checks for auto-provision-1: take one safeguard out, the test that covers it must fail; then put it back.
Manual only, not in ctest (it rebuilds sources).

Run from the repository root: python3 tests/card_files/reverse.py [build] [mutation...]. Each mutation edits the source
text, rebuilds sim-card-files (for C sources), runs the named phases of tests/card_files/run.py (or serve_test.py), and
the original text is written back whatever happens."""
import subprocess
import sys
from pathlib import Path

ROOT = Path.cwd()
BUILD = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build"
CF, MOGO, SERVE = ROOT / "system/services/card_files/card_files.c", ROOT / "apps/mogo/mogo.c", ROOT / "tools/device/card_files_serve.py"
MUTATIONS = [
    ("no-hash-before-install", CF, "bool same = got == 1 && hash_is(i);\n        if (!same && got)", "bool same = got == 1;\n        if (!same && got)", ["F"]),
    ("no-chunk-crc", CF, " && ink_crc32(0, buf, len) == crc ? 0 : -1;", " ? 0 : -1;", ["B"]),
    ("resume-from-zero", CF, "(uint32_t)(have / CF_RESUME * CF_RESUME)", "0", ["E"]),
    ("no-restart-after-bad-resume", CF, "for (int again = 0; again < 2; again++)", "for (int again = 0; again < 1; again++)", ["E2"]),
    ("any-answer-is-serve", CF, 'if (n > 0 && !strncmp(CF.line, "INKCF1 SERVE ", 13) && !strcmp(CF.line + 13, INK_CARD_FILES_DIGEST)) return 0;',
     "if (n > 0) return 0;", ["G"]),
    ("no-reader-notice", MOGO, "else if (ink_mogo_font_missing) { /* not a blank page */", "else if (0 && ink_mogo_font_missing) {", ["A"]),
    ("done-ok-without-install", CF, "return CF.st[i] == E_OK ? D_OK : CF.stop ? D_IO : D_KEPT;", "return D_OK;", ["J"]),
    ("notice-holds-the-file", CF, " || ink_card_files_notice_snapshot(app, NULL, 0) > 0", "", ["I"]),
    ("reopen-by-redraw", MOGO, "static void ink_mogo_reopen(void) { ink_mogo_start(NULL,0,NULL); }", "static void ink_mogo_reopen(void) { ink_ui_redraw(); }", ["H"]),
    ("tool-ignores-get-hash", SERVE, '        if sha != e["sha256"]:\n            return f"{PREFIX}NO {idx} sha"\n', "", ["serve"]),
]


def run(phases):
    if phases == ["serve"]:
        cmd = [sys.executable, str(ROOT / "tests/card_files/serve_test.py")]
    else:
        cmd = [sys.executable, str(ROOT / "tests/card_files/run.py"), "--build", str(BUILD), "--only", *phases]
    r = subprocess.run(["taskpolicy", "-c", "utility", *cmd], capture_output=True, text=True, errors="replace")
    text = (r.stdout + r.stderr).splitlines()
    said = [l.strip() for l in text if "CHECK FAILED" in l] + [l.strip() for l in text if "Error" in l or l.startswith("FAIL")]
    return r.returncode != 0, (said or text[-2:])[:3]


def build():
    r = subprocess.run(["taskpolicy", "-c", "utility", "cmake", "--build", str(BUILD), "-j3", "--target", "sim-card-files"],
                       capture_output=True, text=True)
    if r.returncode:
        print(r.stdout[-2000:], r.stderr[-2000:])
    return r.returncode == 0


only, results = set(sys.argv[2:]), []
for name, path, old, new, phases in MUTATIONS:
    if only and name not in only:
        continue
    original = path.read_text()
    assert original.count(old) == 1, name
    try:
        path.write_text(original.replace(old, new))
        built = path.suffix == ".py" or build()
        failed, said = run(phases) if built else (False, ["did not build"])
    finally:
        path.write_text(original)
    results.append((name, failed, said))
    print(f"{name}: {'fails as required' if failed else 'STILL PASSES'} ({', '.join(phases)})", flush=True)
    for line in said:
        print("    " + line[:300], flush=True)
build()
sys.exit(0 if all(failed for _, failed, _ in results) else 1)
