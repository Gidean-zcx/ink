"""Boot the real editor twice on one flash image; optional source-only negative controls."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, default=ROOT / "build")
parser.add_argument("--mutant", choices=("mapping", "clusters", "member", "draft", "page", "selection", "viewport",
                                        "closed-page", "context-value", "context-input", "context-keyboard", "stale-body"))
args = parser.parse_args()
build = args.build.resolve()
work = Path(tempfile.mkdtemp(prefix="text-long-", dir=build))
binary = build / "sim-text-long"
if args.mutant:
    service = {"selection": "services/text/text_caps.c", "viewport": "ui/textfield.c", "closed-page": "ui/textfield.c",
               "context-value": "ui/textfield.c", "context-input": "ui/ui_task.c", "context-keyboard": "ui/shell/keyboard.c"}.get(
                   args.mutant, "services/text/text_service.c")
    source = (ROOT / "system" / service).read_text()
    if args.mutant == "member":
        old = 'return !TS.session || (identity.authed && !strcmp(TS.owner.member, identity.member));'
        assert source.count(old) == 1
        source = source.replace(old, "return true;")
    elif args.mutant == "draft":
        old = "static void save_draft(void) {"
        assert source.count(old) == 1
        source = source.replace(old, old + "\n    return;")
    elif args.mutant == "page":
        old = '!strcmp(d->page, cfg->draft_page) &&'
        assert source.count(old) == 1
        source = source.replace(old, "true &&")
    elif args.mutant == "selection":
        old = "int n = ink_text_selected(text, INK_CLIP_MAX + 1);"
        assert source.count(old) == 1
        source = source.replace(old, 'int n = snprintf(text, INK_CLIP_MAX + 1, "%s", ink_text_value());')
    elif args.mutant == "viewport":
        old = '    else if (f->cfg.draft_page) snprintf(line, line_cap, "%.*s", (int)(char_at(v, f->page_end) - char_at(v, f->window)), char_at(v, f->window));\n'
        assert source.count(old) == 1
        source = source.replace(old, "")
    elif args.mutant in ("closed-page", "context-value", "context-input", "context-keyboard", "stale-body"):
        old, new = {
            "closed-page": ('if (!f->cfg.draft_page) f->window = 0;', 'f->window = 0;'),
            "context-value": ('ink_jw_strn(&w, start, (size_t)(char_at(f->text, f->page_end) - start));', 'ink_jw_str(&w, f->text);'),
            "context-input": ('ink_text_input_snapshot(text, cap)', 'ink_text_snapshot(text, cap)'),
            "context-keyboard": ('ink_text_draft_id() ? "" : ink_text_value()', 'ink_text_value()'),
            "stale-body": ('strcpy(d->value, TS.value);',
                           'ink_rec_t old; if (ink_db_get(TS.drafts, TS.draft_id, &old, d, sizeof *d) != 0) strcpy(d->value, TS.value);'),
        }[args.mutant]
        assert source.count(old) == 1
        source = source.replace(old, new)
        if args.mutant == "context-value": source = source.replace('    const char *start = char_at(f->text, f->window);\n', '')
    else:
        name = "ink_text_byte_offset" if args.mutant == "mapping" else "ink_text_cluster_end"
        pattern = r"size_t " + name + r"\([^)]*\) \{.*?\n\}"
        substitute = ("size_t ink_text_byte_offset(const char *text, size_t chars) { return chars < strlen(text) ? chars : strlen(text); }"
                      if args.mutant == "mapping" else "size_t ink_text_cluster_end(const char *text, size_t start) { return start < strlen(text) ? start + 1 : start; }")
        source, count = re.subn(pattern, substitute, source, count=1, flags=re.S)
        assert count == 1
    target = work / Path(service).name
    target.write_text(source)
    flags = {}
    for line in (build / "CMakeFiles/ink_system.dir/flags.make").read_text().splitlines():
        if " = " in line:
            key, value = line.split(" = ", 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / "CMakeFiles/sim-text-long.dir/link.txt").read_text())
    obj = work / "text_service.o"
    subprocess.run([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
                    "-I" + str(ROOT / "system/services/text"), "-c", str(target), "-o", str(obj)], cwd=build, check=True)
    index = next(i for i, word in enumerate(link) if word.endswith(".a"))
    link[index:index] = [str(obj)]
    binary = work / "sim-text-long"
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)

(work / "sd").mkdir()
ok = True
phases = ("indexes",) if args.mutant in ("mapping", "clusters") else ("write", "recover")
for phase in phases:
    out = work / phase
    result = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
                             "--assets", str(build / "assets"), "--content", str(build / "test-content"),
                             "--script", str(ROOT / "tests/text_long/editor.script"), "--budget", str(ROOT / "tests/ui_scene.budget.json"),
                             "--hub", str(ROOT / "tests/ui_shell.hub.json"), "--out", str(out), "--quiet", "--no-random-outages",
                             "--flash-image", str(work / "user.img"), "--sd-dir", str(work / "sd")],
                            cwd=ROOT, env={**os.environ, "INK_TEXT_PHASE": phase, "UBSAN_OPTIONS": "halt_on_error=1"},
                            capture_output=True, text=True, timeout=120)
    (work / (phase + ".log")).write_text(result.stdout + result.stderr)
    data = json.loads((out / "metrics.json").read_text()) if (out / "metrics.json").exists() else {}
    counters = data.get("metrics", {})
    complete = counters.get("test.finished") == 1
    if phase == "write": complete &= counters.get("test.cut") == 1 and counters.get("power.reboot") == 1
    passed = result.returncode == 0 and complete and counters.get("test.fail", 0) == 0
    ok &= passed
    print(phase, "PASS" if passed else "FAIL", "exit", result.returncode, "checks", counters.get("test.pass", 0),
          "failures", counters.get("test.fail", 0), flush=True)
    if not passed:
        for warning in data.get("warnings", []): print(warning)
        print(result.stderr[-2000:])
    if phase == "write" and passed and not args.mutant:
        compare = build / "text-long-compare/ImeLong"
        compare.mkdir(parents=True, exist_ok=True)
        for name in ("snap-scene.png", "metrics.json"): shutil.copyfile(out / name, compare / name)
    if not passed and phase == "write": break
if ok and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(work, ignore_errors=True)  # a pass leaves nothing behind; INK_KEEP_EVIDENCE=1 keeps it
else:
    print("Evidence:", work.relative_to(ROOT), flush=True)
raise SystemExit(0 if ok else 1)
