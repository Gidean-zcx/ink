#!/usr/bin/env python3
"""Check UI size arguments, including local constants and arithmetic aliases.

This is a bounded C expression checker, not a compiler: runtime layout arithmetic
is checked at its constant leaves. Percentages and multiplicative layout ratios
are dimensionless. Font/icon/radius arguments use their own contract categories.
"""
import argparse
import ast
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
THEME = ROOT / "system/ui/ink/ui_theme.h"
VALUES = {k: int(v) for k, v in re.findall(r'\b(INK_[A-Z_]+)\s*=\s*(\d+)\b', THEME.read_text())}
SPEC = {0, 1, 2, 2.5} | {v for k, v in VALUES.items() if not k.startswith('INK_CANVAS')}
CATEGORIES = {
    'font': {v for k, v in VALUES.items() if k.startswith('INK_FONT_')},
    'icon': {v for k, v in VALUES.items() if k.startswith('INK_ICON_')},
    'radius': {0, -1} | {v for k, v in VALUES.items() if k.startswith('INK_RADIUS_')},
    'stroke': {0, VALUES['INK_STROKE']},
    'dimension': SPEC | {-1},
}
STYLE_SIZE = r'(?:min_|max_)?(?:width|height)|pad_\w+|radius|border_width|outline_(?:width|pad)|text_(?:letter|line)_space|translate_[xy]'
CALL = re.compile(r"\b(ink_dp|ink_ui_px|ink_ui_font_px|ink_ui_line_height|ink_ui_(?:box|label|icon|pill|circle|card|toggle|slider|segments|list_row|panel|pagebar|notice)|"
                  r"lv_obj_(?:set_(?:pos|size|width|height|x|y|content_(?:width|height)|ext_click_area)|align(?:_to)?|set_style_(?:" + STYLE_SIZE + r"))|lv_style_set_(?:" + STYLE_SIZE + r"))\s*\(")
EXCEPTION = re.compile(r"ui-scale-exception:\s*(\d+(?:\.\d+)?)\s+([^\n*]+)")
DECL = re.compile(r'\b(?:(?:const|static|unsigned|signed)\s+)*(?:int|long|short|float|double|u?int\d+_t|lv_coord_t)\s+(?:const\s+)?([a-zA-Z_]\w*)\s*(?:=\s*([^;]+?))?\s*(?=[;,)])')


def clean_source(text):
    return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]), text, flags=re.S)


def arguments(text, start):
    depth, at, args = 1, start, []
    for end in range(start, len(text)):
        c = text[end]
        if c in '([': depth += 1
        elif c in ')]': depth -= 1
        if depth == 0:
            args.append(text[at:end].strip())
            return args
        if c == ',' and depth == 1:
            args.append(text[at:end].strip())
            at = end + 1
    return []


def parameters(name, args):
    if name in ('ink_dp', 'ink_ui_px'): return [(0, 'dimension')]
    if name in ('ink_ui_font_px', 'ink_ui_line_height'): return [(0, 'font')]
    kinds = {
        'ink_ui_box': {1: 'dimension', 2: 'dimension', 3: 'dimension', 4: 'dimension', 5: 'radius'},
        'ink_ui_label': {2: 'font', 4: 'dimension', 5: 'dimension', 6: 'dimension'},
        'ink_ui_icon': {2: 'icon', 4: 'dimension', 5: 'dimension'},
        'ink_ui_pill': dict.fromkeys(range(2, 6), 'dimension'),
        'ink_ui_circle': {2: 'dimension', 3: 'dimension', 4: 'dimension', 5: 'icon'},
        'ink_ui_card': dict.fromkeys(range(1, 5), 'dimension'),
        'ink_ui_toggle': dict.fromkeys(range(3, 6), 'dimension'),
        'ink_ui_slider': dict.fromkeys(range(3, 6), 'dimension'),
        'ink_ui_segments': dict.fromkeys(range(4, 7), 'dimension'),
        'ink_ui_list_row': dict.fromkeys(range(4, 7), 'dimension'),
        'ink_ui_panel': {1: 'dimension'}, 'ink_ui_pagebar': {3: 'dimension'}, 'ink_ui_notice': {2: 'dimension'},
    }
    if name in kinds: return kinds[name].items()
    if name.endswith('align_to'): return [(3, 'dimension'), (4, 'dimension')]
    if name.endswith('align'): return [(2, 'dimension'), (3, 'dimension')]
    if name.endswith('_radius'): return [(1, 'radius')]
    if name.endswith(('_border_width', '_outline_width')): return [(1, 'stroke')]
    if name.endswith(('set_pos', 'set_size')): return [(1, 'dimension'), (2, 'dimension')]
    return [(1, 'dimension')]


def symbols_before(text, pos):
    # Only enclosing lexical scopes can supply a name. A later shadow wins.
    stack, scopes = [], []
    for at, c in enumerate(text[:pos]):
        if c == '{': stack.append(at)
        elif c == '}' and stack: stack.pop()
        scopes.append(tuple(stack))
    current = tuple(stack)
    symbols = {k: str(v) for k, v in VALUES.items()}
    for m in re.finditer(r'^\s*#define\s+(\w+)[ \t]+([^\n]+)', text[:pos], re.M): symbols[m[1]] = m[2]
    for m in DECL.finditer(text[:pos]):
        scope = scopes[m.start()] if scopes else ()
        if current[:len(scope)] == scope: symbols[m[1]] = m[2] or ''
    for m in re.finditer(r'\benum\s*\{([^}]+)\}', text[:pos], re.S):
        scope = scopes[m.start()] if scopes else ()
        if current[:len(scope)] == scope:
            for k, v in re.findall(r'\b(\w+)\s*=\s*([^,]+)', m[1]): symbols[k] = v
    return symbols


def expression(source, symbols, seen=()):
    source = re.sub(r'\((?:const\s+)?(?:int|u?int\d+_t|unsigned|long|lv_coord_t)\)', '', source)
    source = re.sub(r'\b(0x[\da-fA-F]+|\d+)[uUlLfF]+\b', r'\1', source)
    try:
        node = ast.parse(source.strip(), mode='eval').body
    except (SyntaxError, ValueError):
        return None
    def resolve(n):
        if isinstance(n, ast.Name) and n.id in symbols and n.id not in seen:
            value = expression(symbols[n.id], symbols, (*seen, n.id)) or n
            if n.id in VALUES: value.ui_contract = True
            return value
        for field, value in ast.iter_fields(n):
            if isinstance(value, ast.AST): setattr(n, field, resolve(value))
            elif isinstance(value, list): setattr(n, field, [resolve(x) if isinstance(x, ast.AST) else x for x in value])
        return n
    return resolve(node)


def evaluate(node):
    if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)): return node.value
    if isinstance(node, ast.UnaryOp):
        v = evaluate(node.operand)
        if v is not None:
            if isinstance(node.op, ast.USub): return -v
            if isinstance(node.op, ast.UAdd): return v
    if isinstance(node, ast.BinOp):
        a, b = evaluate(node.left), evaluate(node.right)
        if a is None or b is None: return None
        if isinstance(node.op, ast.Add): return a + b
        if isinstance(node.op, ast.Sub): return a - b
        if isinstance(node.op, ast.Mult): return a * b
        if isinstance(node.op, (ast.Div, ast.FloorDiv)) and b: return int(a / b)
        if isinstance(node.op, ast.Mod) and b: return a - int(a / b) * b
    return None


def violations(source, category, symbols, allowed):
    source = source.strip()
    while source.startswith('(') and source.endswith(')'):
        depth = 0
        for i, c in enumerate(source):
            depth += (c == '(') - (c == ')')
            if depth == 0: break
        if i != len(source) - 1: break
        source = source[1:-1].strip()
    if re.fullmatch(r'(?:LV_PCT|lv_pct)\s*\(.*\)', source): return []
    if source.strip() in ('LV_SIZE_CONTENT', 'LV_RADIUS_CIRCLE'): return []
    # A scaling helper preserves the category of its argument.
    if source.startswith(('ink_ui_px(', 'ink_dp(')):
        inner = arguments(source, source.index('(') + 1)
        if len(inner) == 1: return violations(inner[0], category, symbols, allowed)
    # Check either branch of a simple C conditional independently.
    if '?' in source and ':' in source:
        yes, no = source.split('?', 1)[1].rsplit(':', 1)
        return violations(yes, category, symbols, allowed) + violations(no, category, symbols, allowed)
    node = expression(source, symbols)
    if node is None: return []  # Runtime C calls/array indexing: their nested size calls are checked separately.
    contract = any(getattr(n, 'ui_contract', False) for n in ast.walk(node))
    value = evaluate(node)
    if value is not None and (category != 'dimension' or not contract):
        return [] if value in allowed else [value]
    def leaves(n):
        if getattr(n, 'ui_contract', False): return []
        if isinstance(n, ast.BinOp):
            # Layout count/ratio factors are not pixel operands. Fully constant
            # expressions were evaluated above, so 123 * 1 never escapes here.
            if isinstance(n.op, (ast.Mult, ast.Div, ast.FloorDiv, ast.Mod)):
                result = []
                for child in (n.left, n.right):
                    if evaluate(child) is None: result += leaves(child)
                return result
            return leaves(n.left) + leaves(n.right)
        v = evaluate(n)
        return [v] if v is not None and v not in allowed and -v not in allowed else []
    return leaves(node)


def check(path):
    text = path.read_text(encoding='utf-8')
    exceptions = {float(n) for n, why in EXCEPTION.findall(text[:2400]) if len(why.strip()) >= 8}
    clean, errors = clean_source(text), []
    for call in CALL.finditer(clean):
        args = arguments(clean, call.end())
        symbols = symbols_before(clean, call.start())
        for i, category in parameters(call[1], args):
            if i >= len(args): continue
            for value in violations(args[i], category, symbols, CATEGORIES[category] | exceptions):
                line = clean.count('\n', 0, call.end()) + 1
                where = path.relative_to(ROOT) if path.is_relative_to(ROOT) else path
                errors.append(f'{where}:{line}: illegal UI {category} {value:g}')
    return sorted(set(errors))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('paths', nargs='*', type=pathlib.Path)
    a = ap.parse_args()
    paths = a.paths or [ROOT / 'system/ui', ROOT / 'apps/home']
    files = [f for p in paths for f in (sorted(p.rglob('*.c')) + sorted(p.rglob('*.h')) if p.is_dir() else [p])]
    errors = [e for f in files if f != THEME for e in check(f)]
    for e in errors: print(e, file=sys.stderr)
    print(f'ui-scale: {len(files)} files, {len(errors)} violations')
    return bool(errors)


if __name__ == '__main__': sys.exit(main())
