"""Turn the fixed quality corpus into C data; never used to build the dictionary."""
import json
from pathlib import Path
import sys

rows = []
for line in Path(__file__).with_name("cases.tsv").read_text().splitlines():
    if not line or line.startswith("#"):
        continue
    key, word, category = line.split("\t")
    assert key.isascii() and key.isalpha() and key.islower() and len(key) <= 40
    rows.append("{" + ",".join(json.dumps(s, ensure_ascii=False) for s in (key, word, category)) + "}")
Path(sys.argv[1]).parent.mkdir(parents=True, exist_ok=True)
Path(sys.argv[1]).write_text("static const struct { const char *key, *word, *category; } CASES[] = {\n" +
                             ",\n".join(rows) + "\n};\n")
