"""Verify conversion, original Mogo file rasterization and the CFF reverse control."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--face", type=int, default=2)
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    manifest = json.loads((ROOT / "third_party/mogo/font-upstream-files.json").read_text())
    for entry in manifest["files"]:
        data = (ROOT / "third_party/mogo" / entry["path"]).read_bytes()
        assert hashlib.sha256(data).hexdigest() == entry.get("adapted_sha256", entry["sha256"]), entry["path"]
    # The regular project Python needs Pillow; the optional conversion tool needs fontTools.
    candidates = list(dict.fromkeys([sys.executable, shutil.which("python3")]))
    for candidate in candidates:
        if candidate and subprocess.run([candidate, "-c", "import fontTools"],
                                        capture_output=True).returncode == 0:
            if candidate != sys.executable:
                return subprocess.run([candidate, __file__, *sys.argv[1:]]).returncode
            break
    else:
        print("SKIP: fontTools is unavailable; install it in the tool Python to test conversion")
        return 77
    from fontTools.ttLib import TTFont
    spec = importlib.util.spec_from_file_location("mogo_font", ROOT / "tools/mogo_font.py")
    tool = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(tool)
    args.work.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="mogo-font-", dir=args.work))
    try:
        converted = work / "InkReaderSansSC-Regular.ttf"
        summary = tool.convert(args.source, converted, args.face)
        assert summary["han"] == 6763
        assert summary["bytes"] < 5 * 1024 * 1024
        with TTFont(converted, checkChecksums=2) as font:
            assert font.sfntVersion == "\x00\x01\x00\x00"
            assert "CFF " not in font and {"glyf", "loca", "maxp"}.issubset(font.keys())
            assert font["glyf"][".notdef"].numberOfContours > 0
            for glyph in font["glyf"].glyphs.values():
                glyph.expand(font["glyf"])
                assert not any(flag & 0x80 for flag in getattr(glyph, "flags", ()))
            assert any("Ink Reader Sans SC" in r.toUnicode() for r in font["name"].names)
            assert any(r.nameID == 13 and "SIL" in r.toUnicode() for r in font["name"].names)
        subprocess.run([str(args.engine), str(converted)], check=True)
        raw = work / "unconverted.otf"
        with TTFont(args.source, fontNumber=args.face, recalcTimestamp=False) as font:
            font.save(raw)
        negative = subprocess.run([str(args.engine), str(raw)], check=False)
        assert negative.returncode == 3, negative.returncode
        assert converted.with_suffix(".OFL.txt").is_file()
        print(json.dumps(summary, ensure_ascii=False, sort_keys=True))
        print("reverse control: same source face without conversion failed with exit 3")
    finally:
        if not os.environ.get("INK_KEEP_EVIDENCE"):
            shutil.rmtree(work)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
