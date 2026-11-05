#!/usr/bin/env python3
"""Generate the contract UI font sizes/weights; keep unrestricted reading fonts."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
SIZES = {24: 32, 26: 36, 30: 44, 32: 52, 36: 48, 44: 60, 60: 72, 88: 100, 176: 186}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--regular', required=True)
    ap.add_argument('--face', type=int, default=2)
    ap.add_argument('--medium', required=True)
    ap.add_argument('--bold', required=True)
    ap.add_argument('--out', required=True, type=Path)
    a = ap.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    # UI strings plus the common GB2312 fallback. Dynamic names outside the UI subset
    # resolve to a same-size question mark; unrestricted body text uses sans-32.
    chars = set(''.join(chr(i) for i in range(32, 127)) + '□●·…“”《》——，。！？：；（）【】◀▶')
    import importlib.util
    spec = importlib.util.spec_from_file_location('fontgen', ROOT / 'tools/fontgen/fontgen.py')
    fontgen = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fontgen)
    for group in ['system/ui', 'apps', 'system/services', 'system/policy']:
        for source in (ROOT / group).rglob('*.c'):
            # Read only characters actually in the selected CJK face, not comments'
            # private-use markers or unsupported typographic symbols.
            chars.update(c for c in source.read_text() if '\u3400' <= c <= '\u9fff')
    coverage = fontgen.font_coverage(a.regular, a.face)
    chars = {c for c in chars if ord(c) in coverage}
    repertoire = a.out / 'ui-charset.txt'
    repertoire.write_text(''.join(sorted(chars)), encoding='utf-8')
    outfit = ROOT / 'third_party/fonts/Outfit.ttf'
    outputs = []
    for weight, font in enumerate([a.regular, a.medium, a.bold]):
        face = a.face if weight == 0 else (a.face if font.endswith('.ttc') else 0)
        for px, line in SIZES.items():
            digits = px >= 88
            out = a.out / f'{"outfit" if digits else "ui"}-{weight}-{px}.inkf'
            cmd = [sys.executable, str(ROOT / 'tools/fontgen/fontgen.py'), '--font', str(outfit) if digits else font,
                   '--face', str(0 if digits else face), '--px', str(px), '--bpp', '2', '--exact-line', str(line),
                   '--charset', 'digits' if digits else 'file:' + str(repertoire), '--require-coverage', '-o', str(out)]
            if digits:
                cmd += ['--weight', str([300, 400, 500][weight])]
            else:
                cmd += ['--digits-font', str(outfit), '--digits-weight', str([400, 500, 500][weight])]
            subprocess.run(cmd, check=True)
            outputs.append({'file': out.name, 'bytes': out.stat().st_size})
    total = sum(p.stat().st_size for p in a.out.glob('*.inkf'))
    (a.out / 'size-report.json').write_text(json.dumps({'ui_glyphs': len(chars), 'files': outputs, 'all_inkf_bytes': total,
                                                      'sp05_target_bytes': 4 * 1024 * 1024, 'over_target_bytes': max(0, total - 4 * 1024 * 1024)}, indent=2) + '\n')
    (a.out / 'ui-fonts.stamp').write_text('generated\n')
    print(f'UI font assets: {total} bytes; SP-05 target {4 * 1024 * 1024} bytes')


if __name__ == '__main__':
    main()
