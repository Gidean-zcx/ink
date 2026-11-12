#!/usr/bin/env python3
"""将已有 Noto CJK SC 的 CFF 轮廓转成 Mogo 可从卡读取的 TrueType 子集。"""
import argparse
import hashlib
import json
from pathlib import Path

from fontTools import __version__, subset
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont


def repertoire():
    """GB2312 的 6763 个汉字，加拉丁、中文标点及竖排标点。"""
    chars = set(range(0x20, 0x100))
    for lead in range(0xB0, 0xF8):
        for trail in range(0xA1, 0xFF):
            try:
                chars.add(ord(bytes((lead, trail)).decode("gb2312")))
            except UnicodeDecodeError:
                pass
    for start, end in ((0x2000, 0x2070), (0x3000, 0x3040),
                       (0xFE10, 0xFE20), (0xFE30, 0xFE50), (0xFF00, 0xFFF0)):
        chars.update(range(start, end))
    chars.add(0xFFFD)
    return chars


def convert(source, output, face=2):
    if source.resolve() == output.resolve():
        raise ValueError("输出不能覆盖原字体")
    with TTFont(source, fontNumber=face, recalcTimestamp=False) as font:
        if "CFF " not in font or "glyf" in font:
            raise ValueError("输入必须是已有的 Noto CJK SC CFF 字体")
        if not any("Noto Sans CJK SC" in item.toUnicode()
                   for item in font["name"].names if item.nameID == 1):
            raise ValueError("请选择 Noto Sans CJK SC 字面（TTC 通常为 face 2）")
        wanted = repertoire()
        available = wanted.intersection(font.getBestCmap())
        options = subset.Options()
        options.notdef_glyph = True
        options.notdef_outline = True
        options.recommended_glyphs = True
        options.hinting = False
        options.recalc_timestamp = False
        options.name_IDs = ["*"]
        options.name_languages = ["*"]
        # Keep vertical alternates and their original metrics through subsetting.
        sub = subset.Subsetter(options=options)
        sub.populate(unicodes=sorted(available))
        sub.subset(font)
        glyph_set = font.getGlyphSet()
        glyphs = {}
        for name in font.getGlyphOrder():
            pen = TTGlyphPen(None)
            glyph_set[name].draw(Cu2QuPen(pen, max_err=1.0, reverse_direction=True))
            glyphs[name] = pen.glyph()
        del font["CFF "]
        font.sfntVersion = "\x00\x01\x00\x00"
        builder = FontBuilder(font=font)
        builder.isTTF = True
        builder.setupGlyf(glyphs)
        builder.setupMaxp()
        font["post"].formatType = 3.0
        # A modified font gets a new family, preserving copyright and OFL records.
        names = {1: "Ink Reader Sans SC", 2: "Regular", 3: "InkReaderSansSC-1.0",
                 4: "Ink Reader Sans SC Regular", 6: "InkReaderSansSC-Regular",
                 16: "Ink Reader Sans SC", 17: "Regular"}
        for record in font["name"].names:
            if record.nameID in names:
                record.string = names[record.nameID].encode(record.getEncoding())
        output.parent.mkdir(parents=True, exist_ok=True)
        font.save(output)
    with TTFont(output, recalcTimestamp=False) as result:
        cmap = result.getBestCmap()
        if not {ord(c) for c in "中文阅读目录书签设置，。Aa09"}.issubset(cmap):
            raise ValueError("生成字体缺少基本字形")
        if len(output.read_bytes()) > 5 * 1024 * 1024:
            raise ValueError("生成字体超过单文件 5 MiB 限制")
        summary = {"bytes": output.stat().st_size, "codepoints": len(cmap),
                   "han": sum(0x4E00 <= cp <= 0x9FFF for cp in cmap),
                   "glyphs": len(result.getGlyphOrder()),
                   "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                   "sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
                   "fonttools": __version__, "cu2qu_max_error_font_units": 1.0,
                   "missing": "未收录的字符由 Mogo 绘制 glyph 0（原字体的缺字方框）"}
    license_source = Path(__file__).parent / "appfw-spike/licenses/NotoSansCJK-OFL.txt"
    output.with_suffix(".OFL.txt").write_bytes(license_source.read_bytes())
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="现有 Noto TTC 或 SC OTF；不下载字体")
    parser.add_argument("output", type=Path, help="内容目录或待复制到存储卡的 TTF 路径")
    parser.add_argument("--face", type=int, default=2)
    args = parser.parse_args()
    print(json.dumps(convert(args.source, args.output, args.face), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
