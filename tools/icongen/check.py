#!/usr/bin/env python3
"""Check required icon declarations and generate compile-time name references."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
STRING = r'"(?:\\.|[^"\\])*"'
TOKEN = re.compile(STRING + r"|'(?:\\.|[^'\\])*'|[(){}\[\],;?:]|\w+|\S")
REFERENCE = re.compile(r'\bINK_UI_ICON\s*\(\s*([A-Za-z_]\w*)\s*\)')


def without_comments(text):
    return re.sub(STRING + r"|'(?:\\.|[^'\\])*'|/\*.*?\*/|//[^\n]*",
                  lambda m: ' ' * len(m[0]) if m[0].startswith(('/',)) else m[0], text, flags=re.S)


def parts(text, start, closing=')'):
    """Split C arguments/initializers without splitting nested expressions."""
    stack, begin, values = [closing], start, []
    for token in TOKEN.finditer(text, start):
        c = token[0]
        if c in ('(', '{', '['): stack.append({'(': ')', '{': '}', '[': ']'}[c])
        elif c in (')', '}', ']'):
            if c != stack.pop(): return [], token.end()
            if not stack: return values + [text[begin:token.start()].strip()], token.end()
        elif c == ',' and len(stack) == 1:
            values.append(text[begin:token.start()].strip()); begin = token.end()
    return [], len(text)


def choices(expr):
    expr = expr.strip()
    if expr.startswith('('):
        values, end = parts(expr, 1)
        if end == len(expr) and len(values) == 1: return choices(values[0])
    depth, question, nested = 0, None, 0
    for token in TOKEN.finditer(expr):
        c = token[0]
        if c in ('(', '{', '['): depth += 1
        elif c in (')', '}', ']'): depth -= 1
        elif not depth:
            if c == '?':
                if question is None: question = token.end()
                else: nested += 1
            elif c == ':' and question is not None:
                if nested: nested -= 1
                else: return choices(expr[question:token.start()]) + choices(expr[token.end():])
    return [expr]


def literal_names(expr):
    leaves = choices(expr)
    if leaves != [expr.strip()]: return [name for leaf in leaves for name in literal_names(leaf)]
    expr = expr.strip()
    # Conditions were discarded above; concatenated C strings are one name.
    if re.fullmatch(r'(?:' + STRING + r'\s*)+', expr):
        return [''.join(json.loads(m[0]) for m in re.finditer(STRING, expr))]
    if expr.startswith('{'):
        return [json.loads(m[0]) for m in re.finditer(STRING, expr)]
    return []


def icon_parameters(sources):
    sinks = {'ink_ui_icon': 1, 'ink_ui_circle': 1, 'ink_ui_list_row': 1}
    # Forwarding helpers declare their icon argument, just like the components.
    for text in sources:
        for m in re.finditer(r'\b(\w+)\s*\(([^;{}]*)\)\s*\{', text):
            args, _ = parts('(' + m[2] + ')', 1)
            for i, arg in enumerate(args):
                if re.fullmatch(r'const\s+char\s*\*\s*icon', arg): sinks[m[1]] = i
    return sinks


def check_sources(sources, names):
    clean = {path: without_comments(text) for path, text in sources.items()}
    sinks = icon_parameters(clean.values())
    # Follow local aliases and all writes conservatively, rather than trusting
    # the last textual assignment. These declarations also cover icon arrays.
    aliases = {}
    for text in clean.values():
        for m in re.finditer(r'\bchar\s*\*\s*(?:const\s+)?(\w+)\s*(?:\[[^]]*\]\s*)*=\s*', text):
            end = text.find(';', m.end())
            aliases.setdefault(m[1], []).append(text[m.end():end])
    for text in clean.values():
        for m in re.finditer(r'\b(\w+)\s*=(?!=)\s*([^;]+);', text):
            if m[1] in aliases: aliases[m[1]].append(m[2])

    def resolve(expr, seen=()):
        values = literal_names(expr)
        for leaf in choices(expr):
            for token in TOKEN.finditer(leaf):
                name = token[0]
                if name in aliases and name not in seen:
                    values += [value for rhs in aliases[name] for value in resolve(rhs, (*seen, name))]
        return values

    structs = {}
    for text in clean.values():
        for m in re.finditer(r'typedef\s+struct\s*\{([^{}]+)\}\s*(\w+)\s*;', text):
            fields = re.findall(r'\b(\w+)\s*(?:\[[^]]*\])?\s*[,;]', m[1])
            if 'icon' in fields: structs[m[2]] = fields.index('icon')

    def aggregate(expr, index):
        fields, _ = parts(expr, 1, '}')
        if fields and fields[0].startswith('{'):
            return [name for row in fields for name in aggregate(row, index)]
        if len(fields) > index and not fields[index].startswith('.'):
            return resolve(fields[index])
        return []

    errors = []
    for path, text in clean.items():
        required = [(m[1], m.start()) for m in REFERENCE.finditer(text)]
        for function, index in sinks.items():
            for m in re.finditer(r'\b' + re.escape(function) + r'\s*\(', text):
                args, _ = parts(text, m.end())
                if len(args) > index and not re.match(r'const\s+char\s*\*', args[index]):
                    required += [(name, m.start()) for name in resolve(args[index])]
        # Raw spellings in icon tables/aliases are checked too. The generated
        # INK_UI_ICON references also make a misspelling a compiler error.
        for m in re.finditer(r'\bchar\s*\*\s*(?:const\s+)?(\w+)\s*(?:\[[^]]*\]\s*)*=\s*', text):
            if 'icon' not in m[1].lower(): continue
            end = text.find(';', m.end())
            required += [(name, m.start()) for name in resolve(text[m.end():end])]
        for m in re.finditer(r'(?<=[{,])\s*\.icon\s*=\s*', text):
            args, _ = parts(text, m.end(), '}')
            if args: required += [(name, m.start()) for name in resolve(args[0])]
        for struct, index in structs.items():
            pattern = (r'\b' + re.escape(struct) + r'\s+\w+\s*(?:\[[^]]*\]\s*)*=\s*\{|'
                       r'\(\s*' + re.escape(struct) + r'\s*\)\s*\{')
            for m in re.finditer(pattern, text):
                _, end = parts(text, m.end(), '}')
                required += [(name, m.start()) for name in aggregate(text[m.end() - 1:end], index)]
        for name, at in required:
            if name not in names:
                errors.append(f'{path}:{text.count(chr(10), 0, at) + 1}: unknown required icon {name}')
    return sorted(set(errors))


def header(names):
    lines = ['/* Generated from the checked icon manifest. */', '#ifndef INK_UI_ICON_NAMES_H',
             '#define INK_UI_ICON_NAMES_H', '#define INK_UI_ICON(name) INK_UI_ICON_##name']
    lines += [f'#define INK_UI_ICON_{name} "{name}"' for name in names]
    return '\n'.join(lines + ['#endif', ''])


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--header', type=Path)
    a = ap.parse_args()
    folder = ROOT / 'assets/icons'
    manifest = json.loads((folder / 'manifest.json').read_text())
    errors = []
    for name, digest in manifest['files'].items():
        file = folder / name
        if not file.exists() or hashlib.sha256(file.read_bytes()).hexdigest() != digest:
            errors.append(f'missing or changed geometry: {name}')
    paths = [p for parent in ('system/ui', 'system/include', 'apps')
             for p in (ROOT / parent).rglob('*') if p.suffix in ('.c', '.h')]
    errors += check_sources({p.relative_to(ROOT): p.read_text() for p in paths}, set(manifest['names']))
    for message in errors: print(message, file=sys.stderr)
    print(f'icons: {len(manifest["names"])} names, {len(manifest["files"])} SVG files, {len(errors)} errors')
    if not errors and a.header:
        output = header(manifest['names'])
        a.header.parent.mkdir(parents=True, exist_ok=True)
        if not a.header.exists() or a.header.read_text() != output: a.header.write_text(output)
    return bool(errors)


if __name__ == '__main__': sys.exit(main())
