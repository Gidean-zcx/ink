"""Reject unmarked Chinese literals in UI sources, including local helpers and tables.

This deliberately checks more than widget call names: a label often receives a variable filled
several functions earlier. Exemptions below describe non-display uses; none exempts a whole file.
No compiler, build directory, git history or third-party Python package is needed.
"""
import argparse
from collections import Counter
from pathlib import Path
import re
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/i18n'))
from i18n_gen import load, unescape
TOKEN = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:[^"\\]|\\.)*"|\'(?:[^\'\\]|\\.)*\'|[A-Za-z_]\w*|[^\s]', re.S)
HAN = re.compile(r'[\u3400-\u9fff\U00020000-\U0003134f]')

# Structural allowlist: capability/data/state titles describe APIs for AI, not widgets.
METADATA = ('ink_cap_t', 'ink_db_def_t', 'ink_state_def_t')
# Snapshot functions serialize page descriptions for AI only (task's explicit exclusion).
SNAPSHOTS = {
    'system/ui/shell/keyboard.c': {'semantic_snapshot'},
    'system/ui/shell/kid_pages.c': {'semantic_snapshot'},
    'system/ui/shell/member_pages.c': {'semantic_snapshot'},
    'system/ui/shell/shell_home.c': {'ink_shell_home_snapshot'},
    'system/ui/shell/control_center.c': {'control_snapshot', 'kid_control_snapshot'},
    'system/ui/shell/settings_body.c': {'page_snapshot'},
    'system/ui/shell/power_pages.c': {'semantic_snapshot'},
    'system/ui/shell/settings_family.c': {'snapshot'},
    'system/ui/ui_semantics.c': {'ink_ui_semantics_snapshot', 'ink_ui_semantics_summary'},
    'system/apps/files/files.c': {'snapshot'},
}
# Comparisons and prefix removal consume source-language protocol/status keys, not display text.
SOURCE_OPERATIONS = {'strcmp', 'strncmp', 'strstr', 'strlen'}
WIDGETS = {'ink_ui_label', 'ink_ui_line', 'ink_ui_pill', 'ink_ui_toggle', 'ink_ui_slider',
           'ink_ui_segments', 'ink_ui_notice', 'ink_ui_list_row', 'ink_ui_list_create', 'ink_shell_topbar'}
EXACT = {
    ('system/ui/shell/airplane_card.c', '飞行模式已打开，'): 'Remove the source-language receipt prefix before displaying its translated remainder.',
    ('system/ui/shell/ai_panel.c', '声音没能播放'): 'Keep the source-language state key; the caption is translated at display.',
    ('system/apps/camera/camera.c', '/family/锁屏拍的照片'): 'Existing persistent directory; translating it would change file identity.',
    ('system/apps/camera/camera.c', '/me/照片'): 'Existing persistent directory.',
    ('system/apps/camera/camera.c', '照片-%u.png'): 'Existing filename sequence, also parsed with sscanf.',
    ('system/apps/camera/camera.c', '%s/照片-%u.png'): 'Generated user filename; preserve the stored naming convention.',
    ('system/apps/gallery/gallery.c', '/family/锁屏拍的照片'): 'Read the same persistent camera directory.',
    ('system/ui/shell/keyboard.c', '中'): 'The input-method key for Chinese reads 中 in every interface language; the generic 中 in the table is the font size (Med).',
}


def tokens(text):
    return [m for m in TOKEN.finditer(text) if not m[0].startswith(('//', '/*'))]


def regions(ts, metadata_types=METADATA):
    """Find balanced declaration initializers and function bodies, ignoring quoted braces."""
    stack, pairs = [], {}
    for i, t in enumerate(ts):
        if t[0] in ('(', '{', '['):
            stack.append(i)
        elif t[0] in (')', '}', ']') and stack:
            start = stack.pop()
            pairs[start] = i
    metadata, functions = [], []
    for i, t in enumerate(ts):
        if t[0] in metadata_types:
            j = i + 1
            while j < len(ts) and ts[j][0] not in (';', '{'):
                j += 1
            if j in pairs and ts[j][0] == '{' and any(x[0] == '=' for x in ts[i:j]):
                metadata.append((j, pairs[j]))
        if t[0] == '(' and i and i in pairs:
            end = pairs[i]
            if end + 1 < len(ts) and ts[end + 1][0] == '{' and end + 1 in pairs:
                name = ts[i - 1][0]
                if name not in ('if', 'for', 'while', 'switch'):
                    functions.append((end + 1, pairs[end + 1], name))
    return metadata, functions


def literals(path, text):
    ts = tokens(text)
    meta, functions = regions(ts)
    ui = path.startswith(('system/ui/', 'system/apps/', 'apps/')) or path == 'system/include/ink/sys/ai.h'
    settings = regions(ts, ('ink_setting_def_t',))[0] if not ui else []
    calls, result = [], []
    for i, t in enumerate(ts):
        if t[0] == '(':
            calls.append(ts[i - 1][0] if i else '')
        elif t[0] == ')' and calls:
            calls.pop()
        if not t[0].startswith('"'):
            continue
        value = t[0][1:-1]
        value = re.sub(r'\\u([0-9a-fA-F]{4})|\\U([0-9a-fA-F]{8})', lambda m: chr(int(m[1] or m[2], 16)), value)
        if not HAN.search(value):
            continue
        if not ui and not any(a < i < b for a, b in settings):
            continue
        reason = None
        if any(c in ('TR', 'TR_NOOP') for c in calls):
            reason = 'marked'
        elif any(a < i < b for a, b in meta):
            reason = 'API metadata for AI'
        elif (any(a < i < b and n in SNAPSHOTS.get(path, set()) for a, b, n in functions)
              and not any(c in WIDGETS or c.startswith('lv_label_set_') for c in calls)):
            reason = 'AI-only page description'
        elif any(c in SOURCE_OPERATIONS for c in calls):
            reason = 'source-language comparison/parsing'
        elif (path, value) in EXACT:
            reason = EXACT[path, value]
        result.append((t.start(), t.end(), value, reason))
    return result


def sources(root):
    paths = {p for folder in ('system/ui', 'system/apps', 'apps')
             for p in (root / folder).rglob('*') if p.suffix in ('.c', '.h')}
    paths.add(root / 'system/include/ink/sys/ai.h')
    paths.update(p for p in (root / 'system').rglob('*.c') if 'ink_setting_def_t' in p.read_text(encoding='utf-8'))
    return sorted(paths)


def check(root):
    errors, counts = [], Counter()
    tables = []
    for table in sorted((root / 'assets/i18n').glob('*.tsv')):
        _, entries, problems = load(table)
        errors.extend(problems)
        tables.append((table.stem, entries or {}))
    for p in sources(root):
        text = p.read_text(encoding='utf-8')
        path = p.relative_to(root).as_posix()
        for start, _, value, reason in literals(path, text):
            counts[reason or 'unmarked'] += 1
            if not reason:
                line = text.count('\n', 0, start) + 1
                errors.append(f'{path}:{line}: unmarked Chinese {value!r}')
            elif reason == 'marked':
                key = unescape(value)
                line_start = text.rfind('\n', 0, start) + 1
                cut_note = path == 'system/include/ink/sys/ai.h' and 'INK_AI_ANSWER_CUT' in text[line_start:start]
                for language, entries in tables:
                    if key not in entries:
                        errors.append(f'{path}: {language} has no translation for {value!r}')
                    elif cut_note and len(entries[key].encode('utf-8')) > len(key.encode('utf-8')):
                        errors.append(f'{path}: {language} cut note exceeds its reserved source bytes')
    return errors, counts


def self_test():
    # Reverse controls use the same scanner as the repository check, without editing product files.
    for text in ('lv_label_set_text(o, "直接中文");', 'ink_ui_pill(\no,\n"多行中文", 0);',
                 'const char *rows[] = {"间接中文"};', 'f("相邻" "字面量");',
                 r'f("\u4e2d\u6587");'):
        assert any(not r for _, _, _, r in literals('system/ui/new.c', text)), text
    for text in ('// f("注释");\nf(TR("译文"));', 'f(TR(x ? "开" : "关"));',
                 'static const char *x[] = {TR_NOOP("稍后翻译")};',
                 'static const ink_cap_t c = {.title = "能力说明", .run = f};'):
        assert all(r for _, _, _, r in literals('system/ui/new.c', text)), text
    # An exemption cannot hide a second literal used by a widget on the same line.
    text = 'strcmp(x, "状态键"); lv_label_set_text(o, "漏译");'
    assert [v for _, _, v, r in literals('system/ui/new.c', text) if not r] == ['漏译']
    text = 'void f(const ink_cap_t *cap) { lv_label_set_text(o, "漏译"); }'
    assert any(not r for _, _, _, r in literals('system/ui/new.c', text))
    text = 'int snapshot(void) { ink_ui_label(o, "漏译", 0); }'
    assert any(not r for _, _, _, r in literals('system/ui/shell/settings_family.c', text))
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        header = root / 'system/include/ink/sys/ai.h'
        header.parent.mkdir(parents=True)
        header.write_text('#define INK_AI_ANSWER_CUT TR_NOOP("截断提示")\n', encoding='utf-8')
        table = root / 'assets/i18n/en.tsv'
        table.parent.mkdir(parents=True)
        table.write_text('@name\tTest\n截断提示\tToo long.\n', encoding='utf-8')
        assert not check(root)[0]
        table.write_text('@name\tTest\n截断提示\tThis notice exceeds the reserved bytes.\n', encoding='utf-8')
        assert any('reserved source bytes' in error for error in check(root)[0])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    args = parser.parse_args()
    self_test()
    errors, counts = check(args.root)
    for error in errors:
        print(error)
    print(f'i18n-literals: {len(errors)} unmarked Chinese literals; reverse controls PASS; {dict(counts)}')
    sys.exit(bool(errors))
