"""Embed the registered notices without duplicating or translating their legal text."""
import hashlib
import json
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
entries = json.loads((root / 'third_party/licenses/catalog.json').read_text())
rows, texts = [], []
for e in entries:
    raw = (root / e['file']).read_bytes()
    assert hashlib.sha256(raw).hexdigest() == e['sha256'], e['file']
    text = raw.decode('utf-8')
    if e.get('begin'):
        text = text.split(e['begin'], 1)[1].split(e['end'], 1)[0]
    if e['id'] == 'tlsf':
        text = '\n'.join(line.removeprefix('**').strip() for line in text.splitlines())
    quote = lambda s: json.dumps(s, ensure_ascii=False)
    rows.append('    {' + ', '.join(quote(s) for s in [e['id'], e['name'], e['version'] + ' · ' + e['license']]) +
                ', NULL, "shell.license", "list", NULL, NULL},')
    texts.append(quote('Source: ' + e['source'] + '\n\n' + text.strip() + '\n'))
out = Path(sys.argv[1])
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text('/* Generated from third_party/licenses/catalog.json. */\nstatic const row_t LICENSE_ROWS[] = {\n' +
               '\n'.join(rows) + '\n};\nstatic const char *const LICENSE_TEXTS[] = {\n' + ',\n'.join(texts) + '\n};\n')
