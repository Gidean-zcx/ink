"""Bounded offline extraction of literal text objects from the existing DX180 PDF.
Not a general PDF renderer; ToUnicode mappings decoded, missing glyphs reported.
"""
from pathlib import Path
import re,zlib,json
p=Path('/home/workspace/reader/work/hifi-extension-research/DX180userguide.pdf')
b=p.read_bytes()
objs={int(m[1]):m[3] for m in re.finditer(rb'(\d+) (\d+) obj\s*(.*?)\s*endobj',b,re.S)}
catalog=next(v for v in objs.values() if b'/Type/Catalog' in v)
root=int(re.search(rb'/Pages\s+(\d+)\s+0\s+R',catalog)[1])
pages=[]
def walk(n):
    d=objs[n]
    if re.search(rb'/Type\s*/Page\b',d): pages.append((n,d));return
    kids=re.search(rb'/Kids\s*\[(.*?)\]',d,re.S)
    for x in re.findall(rb'(\d+)\s+0\s+R',kids[1]): walk(int(x))
walk(root)
def stream(n):
    d=objs[n]
    if b'stream' not in d: return b''
    head=d.split(b'stream',1)[0]
    length=int(re.search(rb'/Length\s+(\d+)',head)[1])
    start=re.search(rb'stream\r?\n',d).end()
    a=d[start:start+length]
    return zlib.decompress(a) if b'/FlateDecode' in d.split(b'stream',1)[0] else a
def literal(x):
    x=x[1:-1]
    x=re.sub(rb'\\([0-7]{1,3})',lambda m:bytes([int(m[1],8)]),x)
    x=re.sub(rb'\\([nrtbf()\\])',lambda m:{b'n':b'\n',b'r':b'\r',b't':b'\t',b'b':b'\b',b'f':b'\f'}.get(m[1],m[1]),x)
    return x.decode('cp1252',errors='replace')
cmap={}
for n,d in objs.items():
    m=re.search(rb'/ToUnicode\s+(\d+)\s+0\s+R',d)
    if m:
        cm=stream(int(m[1])); mp={}
        for bf in re.findall(rb'beginbfchar(.*?)endbfchar',cm,re.S):
            for a,c in re.findall(rb'<([0-9A-Fa-f]+)>\s*<([0-9A-Fa-f]+)>',bf):
                mp[int(a,16)]=bytes.fromhex(c.decode()).decode('utf-16-be')
        assert b'beginbfrange' not in cm,'Unhandled mapping type'
        cmap[n]=mp
out=[]
for i,(n,d) in enumerate(pages):
    cs=re.search(rb'/Contents\s*(\[.*?\]|\d+\s+0\s+R)',d,re.S)
    ids=[int(v) for v in re.findall(rb'(\d+)\s+0\s+R',cs[1])]
    raw=b'\n'.join(stream(k) for k in ids)
    fontdict=re.search(rb'/Font\s*<<(.*?)>>',d,re.S)
    fonts={k.decode():int(v) for k,v in re.findall(rb'/([^\s/]+)\s+(\d+)\s+0\s+R',fontdict[1])} if fontdict else {}
    blocks=[];hex_count=0; missing=0;cur=None
    for bt in re.findall(rb'\bBT\b(.*?)\bET\b',raw,re.S):
        chunks=[]
        for tok in re.finditer(rb'(?P<font>/([^\s/]+)\s+[\d.]+\s+Tf)|(?P<hex><[0-9A-Fa-f\s]+>)|(?P<lit>\((?:\\.|[^\\)])*\))',bt,re.S):
            if tok['font']: cur=re.search(rb'/([^\s/]+)',tok['font'])[1].decode()
            elif tok['lit']: chunks.append(literal(tok['lit']))
            elif tok['hex']:
                hex_count+=1; hx=re.sub(rb'\s+',b'',tok['hex'][1:-1]); mp=cmap.get(fonts.get(cur),{})
                txt=''
                for k in range(0,len(hx),4):
                    code=int(hx[k:k+4],16);txt+=mp.get(code,'[UNMAPPED]');missing+=code not in mp
                chunks.append(txt)
        if chunks: blocks.append(''.join(chunks))
    out.append(dict(pdf_page=i+1,printed_page=i,text='\n'.join(blocks),decoded_hex_strings=hex_count,missing_glyphs=missing))
dest=Path('/home/workspace/reader/work/hifi-controls-deepening/dx180-literal-text.json')
dest.write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n')
print('pages',len(pages),'text pages',sum(bool(x['text']) for x in out),'missing glyphs',sum(x['missing_glyphs'] for x in out))
for x in out[27:63]:
    print('\nPRINTED PAGE',x['printed_page'],'PDF',x['pdf_page'],'MISSING_GLYPHS',x['missing_glyphs'])
    print(x['text'])
