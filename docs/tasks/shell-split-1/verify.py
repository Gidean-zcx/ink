#!/usr/bin/env python3
"""Manual checks for the shell move; the baseline needs local Git history."""
import argparse
from collections import Counter
import hashlib
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))
import capabilities

FILES = ("navigation.c", "status_bar.c", "shell_home.c", "control_center.c", "settings_body.c", "today_edit.c")
SHELL = ROOT / "system/ui/shell"
FUNCTION = re.compile(r"^[\w][\w \t*]*?\b(\w+)\s*\([^;{}]*\)\s*\{", re.M)


def functions(source):
    found = {}
    for match in FUNCTION.finditer(source):
        depth = 0
        start = match.end() - 1
        for token in capabilities.LEX.finditer(source, start):
            if token.lastgroup in ("comment", "string", "char"):
                continue
            if token.group() == "{":
                depth += 1
            elif token.group() == "}":
                depth -= 1
                if depth == 0:
                    name = match[1]
                    assert name not in found, f"duplicate function: {name}"
                    found[name] = source[start:token.end()]
                    break
        else:
            raise AssertionError(f"unterminated function: {match[1]}")
    return found


def definitions(sources):
    entries = [entry for path, source in sources.items() for entry in capabilities.parse_source(path, source)]
    result = {(entry.kind, entry.id): entry.fields for entry in entries}
    assert len(result) == len(entries), "duplicate definitions"
    return result


def source_check(baseline):
    missing = [name for name in (*FILES, "shell_internal.h") if not (SHELL / name).is_file()]
    assert not missing, f"split files missing: {missing}"
    original = subprocess.run(["git", "show", f"{baseline}:system/ui/shell/navigation.c"], cwd=ROOT,
                              capture_output=True, text=True, check=True).stdout
    sources = {name: (SHELL / name).read_text() for name in FILES if (SHELL / name).exists()}
    if (SHELL / "shell_internal.h").exists():
        sources["shell_internal.h"] = (SHELL / "shell_internal.h").read_text()
    before = functions(original)
    after = {}
    for name, source in sources.items():
        items = functions(source)
        assert not after.keys() & items.keys(), f"duplicate functions in {name}"
        after.update(items)
    assert before.keys() == after.keys(), f"function set changed: {before.keys() ^ after.keys()}"
    changed = [name for name in before if before[name] != after[name]]
    assert not changed, f"function bodies changed: {changed}"
    old_defs, new_defs = definitions({"navigation.c": original}), definitions(sources)
    assert old_defs == new_defs, "capability / setting / state metadata changed"
    header = (SHELL / "shell_internal.h").read_text()
    old_state = re.search(r"static struct \{(.*?)\} N INK_PSRAM_BSS;", original, re.S)[1]
    new_state = re.search(r"struct ink_shell_state \{(.*?)\};", header, re.S)
    assert new_state and old_state == new_state[1], "shared state layout changed"
    negative = dict(after)
    negative["ink_shell_home_snapshot"] += " "
    assert before != negative, "negative control accepted a changed function body"
    negative = dict(new_defs)
    negative.pop(next(iter(negative)))
    assert old_defs != negative, "negative control accepted a missing definition"
    print(f"PASS: {len(before)} function bodies byte-identical; {len(old_defs)} definitions unchanged; shared state layout unchanged")
    print("PASS: negative controls reject a changed function body and a missing definition")
    old_lines = Counter(line for line in original.splitlines() if line.strip())
    new_lines = Counter(line for source in sources.values() for line in source.splitlines() if line.strip())
    glue = sum((new_lines - old_lines).values())
    for path in ("CMakeLists.txt", "ports/esp32s31/components/ink_system/CMakeLists.txt"):
        old = subprocess.run(["git", "show", f"{baseline}:{path}"], cwd=ROOT,
                             capture_output=True, text=True, check=True).stdout
        glue += sum((Counter((ROOT / path).read_text().splitlines()) - Counter(old.splitlines())).values())
    assert glue <= 80, f"glue budget exceeded: {glue}"
    print(f"PASS: {glue}/80 new or changed nonblank product lines after subtracting verbatim moves (including CMake and annotations)")


def images(root):
    from PIL import Image
    result = {}
    scenes = sorted(path for path in root.glob("ui-scene-*") if path.is_dir())
    assert scenes, f"no scenes in {root.name}"
    for scene in scenes:
        paths = sorted(scene.rglob("*.png"))
        assert paths, f"no screenshots in {scene.name}"
        for path in paths:
            with Image.open(path) as image:
                rgba = image.convert("RGBA")
                result[path.relative_to(root).as_posix()] = (rgba.size, hashlib.sha256(rgba.tobytes()).hexdigest())
    return result, len(scenes)


def image_check(before_path, after_path):
    before, scenes = images(before_path)
    after, _ = images(after_path)
    assert before.keys() == after.keys(), f"screenshot set changed: {before.keys() ^ after.keys()}"
    changed = [path for path in before if before[path] != after[path]]
    assert not changed, f"screenshots differ: {changed}"
    negative = dict(after)
    name = next(iter(negative))
    negative[name] = (after[name][0], "changed pixel")
    assert before != negative, "negative control accepted a changed image"
    print(f"PASS: {len(before)} screenshots from {scenes} scenes pixel-identical (RGBA SHA-256); 0 changed pixels")
    print("PASS: negative control rejects a changed image digest")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline")
    parser.add_argument("--images-before", type=Path)
    parser.add_argument("--images-after", type=Path)
    args = parser.parse_args()
    if args.baseline:
        source_check(args.baseline)
    if args.images_before and args.images_after:
        image_check(args.images_before, args.images_after)
    if not args.baseline and not (args.images_before and args.images_after):
        parser.error("provide --baseline or both image directories")


if __name__ == "__main__":
    main()
