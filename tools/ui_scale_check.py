#!/usr/bin/env python3
"""Check UI size arguments, including local constants and arithmetic aliases.

This is a bounded C expression checker, not a compiler: runtime layout arithmetic
is checked at its constant leaves. Percentages and multiplicative layout ratios
are dimensionless. Font/icon/radius arguments use their own contract categories.
"""
import argparse
import ast
import copy
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
DECL = re.compile(r'\b(?:(?:const|static|unsigned|signed)\s+)*(?:int|long|short|float|double|u?int\d+_t|lv_coord_t)\s+(?:const\s+)?([a-zA-Z_]\w*)\s*(?=[=;,)])')
ARRAY = re.compile(r'\b(?:(?:const|static|unsigned|signed)\s+)*(?:int|u?int\d+_t|lv_coord_t)\s+(\w+)\s*\[[^]]*\]\s*=\s*\{([^{}]+)\}')
ASSIGN = re.compile(r'(?=(?<![\w.>])\b(\w+)\s*([+\-*/]?=)(?!=)\s*([^;{}]+);)')
RUNTIME = re.compile(r'ui-scale-runtime:\s*(dimension|font|icon|radius|stroke)\s+(\S+)\s+([^\n*]+)')


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
    # Track simple writes by lexical identity, including writes inside branches.
    stack, scopes = [], []
    for at, c in enumerate(text[:pos]):
        if c == '{': stack.append(at)
        elif c == '}' and stack: stack.pop()
        scopes.append(tuple(stack))
    current = tuple(stack)
    symbols = {k: str(v) for k, v in VALUES.items()}
    for m in re.finditer(r'^\s*#define\s+(\w+)[ \t]+([^\n]+)', text[:pos], re.M): symbols[m[1]] = m[2]
    declarations = list(DECL.finditer(text[:pos]))
    arrays = list(ARRAY.finditer(text[:pos]))
    events = [(m.start(), 'decl', m) for m in declarations] + [(m.start(), 'array', m) for m in arrays]
    events += [(m.start(), 'assign', m) for m in ASSIGN.finditer(text[:pos])
               if not any(d.start() <= m.start() < d.end() for d in arrays)]
    bindings = {}
    for at, kind, m in sorted(events):
        scope = scopes[at] if scopes else ()
        if scope and current and scope[0] != current[0]: continue
        if kind != 'assign':
            value = '[' + m[2] + ']' if kind == 'array' else ''
            bindings[m[1], scope] = value
            continue
        owners = [s for name, s in bindings if name == m[1] and scope[:len(s)] == s]
        if not owners:
            start = max(text.rfind(';', 0, at), text.rfind('{', 0, at), text.rfind('}', 0, at)) + 1
            if ',' not in text[start:at] or not DECL.search(text[start:at]): continue
            bindings[m[1], scope] = ''
            owners = [scope]
        owner = max(owners, key=len)
        previous = bindings[m[1], owner]
        # Stop at a declarator comma, but retain commas inside calls/subscripts.
        rhs = m[3]; depth = 0
        for i, c in enumerate(rhs):
            depth += (c in '([') - (c in ')]')
            if c == ',' and not depth: rhs = rhs[:i]; break
        value = rhs if m[2] == '=' else f'({previous}) {m[2][0]} ({rhs})'
        if value.strip() == m[1]: continue
        # Freeze aliases at the write, rather than at a later use.
        known = {}
        for (name, s), v in sorted(bindings.items(), key=lambda item: len(item[0][1])):
            if scope[:len(s)] == s and v: known[name] = v
        value = re.sub(r'\b\w+\b', lambda word: '(' + known[word[0]] + ')' if word[0] in known else word[0], value)
        if m[2] == '=' and current[:len(scope)] != scope and previous:
            value = f'[{previous}, {value}]'
        bindings[m[1], owner] = value
    for (name, scope), value in sorted(bindings.items(), key=lambda item: len(item[0][1])):
        if current[:len(scope)] == scope: symbols[name] = value
    for m in re.finditer(r'\benum\s*\{([^}]+)\}', text[:pos], re.S):
        scope = scopes[m.start()] if scopes else ()
        if current[:len(scope)] == scope:
            for k, v in re.findall(r'\b(\w+)\s*=\s*([^,]+)', m[1]): symbols[k] = v
    return symbols


def conditionals(source):
    # Size choices are all checked; condition operands are not pixel values.
    out, at = '', 0
    while at < len(source):
        if source[at] != '(':
            out += source[at]; at += 1; continue
        end, depth = at + 1, 1
        while end < len(source) and depth:
            depth += (source[end] == '(') - (source[end] == ')'); end += 1
        if depth: return source
        out += '(' + conditionals(source[at + 1:end - 1]) + ')'; at = end
    depth = 0
    for i, c in enumerate(out):
        depth += (c == '(') - (c == ')')
        if c == '?' and depth == 0:
            nested = 0
            for j in range(i + 1, len(out)):
                nested += (out[j] == '?') - (out[j] == ':')
                if out[j] == ':' and nested == -1:
                    return '(' + conditionals(out[i + 1:j]) + ') if True else (' + conditionals(out[j + 1:]) + ')'
    return out


def expression(source, symbols, seen=()):
    source = re.sub(r'\((?:const\s+)?(?:int|u?int\d+_t|unsigned|long|lv_coord_t)\)', '', source)
    source = re.sub(r'\b(0x[\da-fA-F]+|\d+)[uUlLfF]+\b', r'\1', source)
    try:
        node = ast.parse(conditionals(source.strip()), mode='eval').body
    except (SyntaxError, ValueError):
        return None
    def resolve(n):
        if isinstance(n, ast.Name) and n.id in symbols and n.id not in seen:
            value = copy.deepcopy(symbols[n.id]) if isinstance(symbols[n.id], ast.AST) else expression(symbols[n.id], symbols, (*seen, n.id)) or n
            if n.id in VALUES: value.ui_contract = True
            return value
        if isinstance(n, ast.Subscript):
            value = resolve(n.value)
            if isinstance(value, (ast.List, ast.Tuple)): return value
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
    node = expression(source, symbols)
    if node is None: return ['unresolved expression ' + source]
    def leaves(n, ratio=False):
        if isinstance(n, ast.Name) and ((category == 'dimension' and n.id == 'LV_SIZE_CONTENT') or
                                       (category == 'radius' and n.id == 'LV_RADIUS_CIRCLE')): return []
        if isinstance(n, (ast.List, ast.Tuple)):
            return [bad for item in n.elts for bad in leaves(item)]
        if isinstance(n, ast.IfExp): return leaves(n.body) + leaves(n.orelse)
        if category == 'dimension' and getattr(n, 'ui_contract', False): return []
        if ast.unparse(n) in symbols and getattr(symbols[ast.unparse(n)], 'ui_contract', False): return []
        if isinstance(n, ast.Call):
            name = ast.unparse(n.func)
            if name in ('ink_ui_px', 'ink_dp') and len(n.args) == 1: return leaves(n.args[0])
            if category == 'dimension' and name in ('LV_PCT', 'lv_pct') and len(n.args) == 1: return []
            if category == 'dimension' and (name in symbols and getattr(symbols[name], 'ui_contract', False) or
                name in ('lv_obj_get_width', 'lv_obj_get_height', 'ink_ui_width', 'ink_ui_height')): return []
            return ['unresolved call ' + name]
        contract = any(getattr(item, 'ui_contract', False) for item in ast.walk(n))
        v = evaluate(n)
        if v is not None and (category != 'dimension' or not contract):
            return [] if v in allowed or ratio else [v]
        if isinstance(n, ast.BinOp):
            # Layout count/ratio factors are not pixel operands. Fully constant
            # expressions were evaluated above, so 123 * 1 never escapes here.
            if isinstance(n.op, (ast.Mult, ast.Div, ast.FloorDiv, ast.Mod)):
                return leaves(n.left, ratio=True) + leaves(n.right, ratio=True)
            return leaves(n.left) + leaves(n.right)
        if ratio and isinstance(n, ast.Name): return []
        return ['unresolved value ' + ast.unparse(n)]
    return leaves(node)


def check(path):
    text = path.read_text(encoding='utf-8')
    exceptions = {float(n) for n, why in EXCEPTION.findall(text[:2400]) if len(why.strip()) >= 8}
    runtime = [(category, name, why) for category, names, why in RUNTIME.findall(text[:2400])
               if len(why.strip()) >= 8 for name in names.split(',')]
    clean, errors = clean_source(text), []
    for call in CALL.finditer(clean):
        args = arguments(clean, call.end())
        symbols = symbols_before(clean, call.start())
        for i, category in parameters(call[1], args):
            if i >= len(args): continue
            if re.match(r'^(?:int|unsigned|lv_coord_t)\s+\w+$', args[i]): continue  # Function declaration.
            trusted = dict(symbols)
            for kind, name, _ in runtime:
                if kind == category:
                    value = ast.Name(id=name, ctx=ast.Load()); value.ui_contract = True
                    if name not in trusted or not trusted[name]: trusted[name] = value
            for value in violations(args[i], category, trusted, CATEGORIES[category] | exceptions):
                line = clean.count('\n', 0, call.end()) + 1
                where = path.relative_to(ROOT) if path.is_relative_to(ROOT) else path
                errors.append(f'{where}:{line}: illegal UI {category} {value if isinstance(value, str) else format(value, "g")}')
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
