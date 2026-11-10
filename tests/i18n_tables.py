"""i18n-1: the build-time table check passes the committed tables and stops on every kind of mistake."""
import ast
import re
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / 'tools/i18n/i18n_gen.py'
SOURCES = [ROOT / 'system/ui/shell' / name for name in ('boot_flow.c', 'navigation.c', 'settings_pages.c')]


def run(tables, sources=(), header=None):
    cmd = [sys.executable, str(GEN), *map(str, tables), '--sources', *map(str, sources)]
    if header:
        cmd += ['--header', str(header)]
    return subprocess.run(cmd, capture_output=True, text=True)


tmp = Path(tempfile.mkdtemp())
good = run(sorted((ROOT / 'assets/i18n').glob('*.tsv')), SOURCES, tmp / 'i18n_table.h')
assert good.returncode == 0, good.stderr
header = (tmp / 'i18n_table.h').read_text(encoding='utf-8')
assert 'static const ink_i18n_pair_t I18N_EN[]' in header and '"English"' in header, header[:300]
table = header.split('I18N_EN[] = {')[1].split('};')[0]
keys = [ast.literal_eval(re.match(r'    \{("(?:[^"\\]|\\.)*")', line)[1])
        for line in table.splitlines() if line.startswith('    {"')]
assert len(keys) > 50, len(keys)
assert keys == sorted(keys, key=lambda k: k.encode()), 'keys are in strcmp order for the binary search'

CASES = {
    'placeholder': ('@name\tTest\n正在连接 %s\tConnecting\n', 'placeholders differ'),
    'order': ('@name\tTest\n%d月%s\t%s %d\n', 'placeholders differ'),
    'duplicate': ('@name\tTest\n加入\tJoin\n加入\tEnter\n', 'duplicate key'),
    'no-tab': ('@name\tTest\n加入 Join\n', 'exactly'),
    'two-tabs': ('@name\tTest\n加入\tJoin\tagain\n', 'exactly'),
    'empty': ('@name\tTest\n加入\t\n', 'exactly'),
    'no-name': ('加入\tJoin\n', 'missing "@name'),
}
for name, (text, why) in CASES.items():
    table = tmp / f'{name}.tsv'
    table.write_text(text, encoding='utf-8')
    bad = run([table])
    assert bad.returncode != 0 and why in bad.stderr, (name, bad.returncode, bad.stderr)
(tmp / 'utf8.tsv').write_bytes(b'@name\tTest\n\xe5\x8a\xa0\xff\tJoin\n')
assert run([tmp / 'utf8.tsv']).returncode != 0, 'invalid UTF-8 is refused'

# A page key with no translation stops the build too.
source = tmp / 'page.c'
source.write_text('void f(void) { g(TR("加入")); g(TR("这一句没有译文")); }\n', encoding='utf-8')
(tmp / 'small.tsv').write_text('@name\tTest\n加入\tJoin\n', encoding='utf-8')
missing = run([tmp / 'small.tsv'], [source])
assert missing.returncode != 0 and '这一句没有译文' in missing.stderr and "'加入'" not in missing.stderr, missing.stderr
# Every character a translation shows is in the UI font (the build's ui_fonts.py stops on any that is not).
import importlib.util
spec = importlib.util.spec_from_file_location('fontgen', ROOT / 'tools/fontgen/fontgen.py')
fontgen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fontgen)
covered = set(fontgen.charset('ascii,punct,pinyin,gb2312'))
for table in (ROOT / 'assets/i18n').glob('*.tsv'):
    outside = sorted({c for c in table.read_text(encoding='utf-8') if c >= ' ' and ord(c) not in covered})
    assert not outside, f'{table.name}: no glyph for {outside}'
assert ord('€') not in covered, 'a character outside the font is detectable'
print(f'PASS: committed tables ok ({good.stdout.strip()}); {len(CASES) + 2} broken tables refused')
