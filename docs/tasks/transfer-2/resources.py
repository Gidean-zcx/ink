"""比较同一基线和本次固件；只输出尺寸，不输出本机路径。"""
import gzip
import json
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[3]
base, current = (Path(p) for p in sys.argv[1:3])

def size_table(build):
    text = (build / 'size.txt').read_text()
    result = {}
    for label in ('Flash', 'External RAM', 'DIRAM', 'LP SRAM'):
        match = re.search(r'│\s*' + label + r'\s*│\s*(\d+)', text)
        if match:
            result[label] = int(match[1])
    return result

before = size_table(base)
after = size_table(current)
a = (base / 'ink-s31.bin').stat().st_size
b = (current / 'ink-s31.bin').stat().st_size
page = root / 'ports/esp32s31/vendor/read_pico_transfer/upload.html'
old_pages = sorted((base / 'esp-idf/main').glob('ap_fileman_*.html.gz'))
result = {
    'firmware_bytes': {'baseline': a, 'current': b, 'delta': b - a, 'within_40_KiB': b - a <= 40960},
    'static_bytes': {k: {'baseline': before[k], 'current': after[k], 'delta': after[k] - before[k]} for k in before if k in after},
    'browser_bytes': {'current_raw': page.stat().st_size, 'current_gzip': len(gzip.compress(page.read_bytes(), compresslevel=9, mtime=0)),
                      'baseline_embedded_gzip': {p.name: p.stat().st_size for p in old_pages}},
    'runtime_measured_on_device': False,
}
print(json.dumps(result, ensure_ascii=False, indent=2))
