import json
from pathlib import Path
import sys
rows = [line.split('\t') for line in Path(__file__).with_name('google_sentences.tsv').read_text().splitlines()]
Path(sys.argv[1]).write_text('static const struct { const char *key, *word; } SENTENCES[] = {\n' + ',\n'.join('{' + ','.join(json.dumps(x, ensure_ascii=False) for x in row) + '}' for row in rows) + '\n};\n')
