"""Exercise both UTF-16 byte orders with Chinese headings and an astral scalar."""
from pathlib import Path
import subprocess
import sys
import tempfile

first = "第一章 河边\n" + "星光照着河水，读书的人记录见闻。\n" * 1900 + "星光😀\n"
text = first + "第二章 灯下\n" + "星光映着书页。\n" * 600
with tempfile.TemporaryDirectory(prefix="mogo-utf16-") as directory:
    folder = Path(directory)
    expected = folder / "expected.txt"
    expected.write_bytes(text.encode("utf-8"))
    for encoding, bom in (("utf-16-le", b"\xff\xfe"), ("utf-16-be", b"\xfe\xff")):
        book = folder / f"{encoding}.txt"
        book.write_bytes(bom + text.encode(encoding))
        subprocess.run([sys.argv[1], str(book), str(expected), str(2 + len(first.encode(encoding)))], check=True)
