"""Original, deterministic test books; no downloaded book text."""
from pathlib import Path
import io
import struct
import zipfile
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parent / 'fixtures'

def book(name, version=2, toc=True, long=False):
    title = '长章练习' if long else '无目录散记' if not toc else '星图日记' if version == 3 else '山间来信'
    author = '测试作者乙' if version == 3 else '测试作者甲'
    paths = ['正文/一.xhtml', '正文/二.xhtml'] if version == 3 else ['c1.xhtml', 'c2.xhtml']
    entries = [('第一章 山路', paths[0]), ('第二章 海边', paths[1]), ('中途码头', paths[1]+'#dock')]
    paragraphs = ''.join(f'<p>山路段落{i}。清晨，我们沿着石阶上山，记录风声与树影。</p>' for i in range(40))
    if long:
        paragraphs = '<p>长章开头。</p>' + '<p>我们在长长的山路上留下脚印，也把星光记进日记。</p>' * 45000
    ch1 = '<h1 id="start">第一章 山路</h1>' + paragraphs
    ch2 = '<h1>第二章 海边</h1><p>海边的晨风吹过纸页，远处传来船笛。</p><h2 id="dock">中途码头</h2><p>码头灯亮了，我们在这里写下新的约定。</p>'
    target = io.BytesIO()
    with zipfile.ZipFile(target, 'w', compression=zipfile.ZIP_DEFLATED) as z:
        def put(path, text, stored=False):
            item = zipfile.ZipInfo(path, (2020,1,1,0,0,0)); item.compress_type = zipfile.ZIP_STORED if stored else zipfile.ZIP_DEFLATED
            z.writestr(item, text.encode('utf-8'))
        put('mimetype', 'application/epub+zip', True)
        put('META-INF/container.xml', '<container><rootfiles><rootfile full-path="OPS/book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
        navpath = 'navigation/contents.xhtml' if version == 3 else 'contents.ncx'
        manifest = ''.join(f'<item id="c{i+1}" href="{p}" media-type="application/xhtml+xml"/>' for i,p in enumerate(paths))
        if toc: manifest += f'<item id="toc" href="{navpath}" media-type="'+('application/xhtml+xml" properties="nav' if version == 3 else 'application/x-dtbncx+xml')+'"/>'
        put('OPS/book.opf', f'<package version="{version}.0" xmlns:dc="http://purl.org/dc/elements/1.1/"><metadata><dc:title>{title}</dc:title><dc:creator>{author}</dc:creator></metadata><manifest>{manifest}</manifest><spine toc="toc"><itemref idref="c1"/><itemref idref="c2"/></spine></package>')
        for path, text in zip(paths, [ch1, ch2]): put('OPS/'+path, '<html xmlns="http://www.w3.org/1999/xhtml"><head><title>测试章节</title></head><body>'+text+'<p><img src="drawing.png"/></p></body></html>')
        if toc:
            if version == 3:
                links = ''.join(f'<li><a href="../{p}">{t}</a></li>' for t,p in entries)
                put('OPS/'+navpath, f'<html xmlns:epub="http://www.idpf.org/2007/ops"><body><nav epub:type="toc"><ol>{links}</ol></nav></body></html>')
            else:
                links = ''.join(f'<navPoint id="p{i}"><navLabel><text>{t}</text></navLabel><content src="{p}"/></navPoint>' for i,(t,p) in enumerate(entries))
                put('OPS/'+navpath, f'<ncx><navMap>{links}</navMap></ncx>')
    data = target.getvalue(); (ROOT/name).write_bytes(data)
    return data

if __name__ == '__main__':
    ROOT.mkdir(exist_ok=True)
    data = book('epub2.epub'); book('epub3.epub', 3); book('no-toc.epub', toc=False); book('long.epub', long=True)
    (ROOT/'corrupt.epub').write_bytes(b'PK\x03\x04BROKEN TEST ZIP')
    with zipfile.ZipFile(ROOT/'not-epub.epub','w') as z: z.writestr('mimetype','not an epub')
    encrypted = bytearray(data)
    for signature, offset in ((b'PK\x03\x04',6),(b'PK\x01\x02',8)):
        start = 0
        while (start := encrypted.find(signature,start)) >= 0:
            struct.pack_into('<H',encrypted,start+offset,struct.unpack_from('<H',encrypted,start+offset)[0]|1); start += 4
    (ROOT/'encrypted.epub').write_bytes(encrypted)
    (ROOT/'chapters.txt').write_text('第一章 早晨\n我们写下一段原创文字。\n第二章 夜晚\n星光落在纸上。\n', encoding='utf-8')
    (ROOT/'gb18030.txt').write_bytes('第一章 春天\n春天的山路很安静。\n'.encode('gb18030'))
