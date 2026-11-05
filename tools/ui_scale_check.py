#!/usr/bin/env python3
"""Reject uncontracted UI dimensions. Exceptions must name a value and a reason."""
import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
THEME = ROOT / "system/ui/ink/ui_theme.h"
# LVGL uses integer strokes: the common theme rounds the spec's 2.5 px to 3.
SPEC = {0, 1, 2, 2.5, 4, 8, 12, 16, 24, 26, 28, 30, 32, 36, 40, 44, 48,
        52, 56, 60, 64, 72, 84, 88, 96, 100, 104, 112, 124, 136, 160, 176, 186}
CALL = re.compile(r"\b(?:ink_dp|ink_ui_px|ink_ui_(?:box|label|icon|pill|circle|card|toggle|slider|segments|list_row|panel|pagebar|notice)|"
                  r"lv_obj_(?:set_(?:pos|size|width|height|x|y)|align(?:_to)?|set_style_(?:pad_\w+|radius|border_width|text_(?:letter|line)_space|translate_[xy])))\s*\(")
NUMBER = re.compile(r"(?<![\w.])\d+(?:\.\d+)?(?![\w.])")
EXCEPTION = re.compile(r"ui-scale-exception:\s*(\d+(?:\.\d+)?)\s+([^\n*]+)")


def check(path):
    text = path.read_text(encoding="utf-8")
    allowed = SPEC | {float(n) for n, why in EXCEPTION.findall(text[:2400]) if len(why.strip()) >= 8}
    # Remove strings/comments without changing line offsets.
    clean = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                   lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]), text, flags=re.S)
    errors = []
    for call in CALL.finditer(clean):
        end, depth = call.end(), 1
        while end < len(clean) and depth:
            depth += (clean[end] == '(') - (clean[end] == ')')
            end += 1
        args = clean[call.end():end - 1]
        # Percentages and constant divisors are layout ratios, not new pixel sizes.
        args = re.sub(r'(?:lv_pct|LV_PCT)\s*\([^)]*\)', '', args)
        args = re.sub(r'(?<=[/*])\s*\d+\b', '', args)
        args = re.sub(r'\b\d+\s*(?=[*/])', '', args)
        for number in NUMBER.finditer(args):
            if float(number[0]) not in allowed:
                line = clean.count('\n', 0, call.end()) + 1
                errors.append(f"{path.relative_to(ROOT) if path.is_relative_to(ROOT) else path}:{line}: illegal UI dimension {number[0]}")
    return errors


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('paths', nargs='*', type=pathlib.Path)
    a = ap.parse_args()
    paths = a.paths or [ROOT / 'system/ui', ROOT / 'apps/home']
    files = [f for p in paths for f in (sorted(p.rglob('*.c')) + sorted(p.rglob('*.h')) if p.is_dir() else [p])]
    errors = [e for f in files if f != THEME for e in check(f)]
    for e in errors:
        print(e, file=sys.stderr)
    print(f"ui-scale: {len(files)} files, {len(errors)} violations")
    return bool(errors)


if __name__ == '__main__':
    sys.exit(main())
