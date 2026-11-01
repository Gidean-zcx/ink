from pathlib import Path
from html.parser import HTMLParser
from html import unescape
from urllib.request import urlopen, Request
from urllib.error import HTTPError
from urllib.parse import urljoin
import hashlib, json, re

W=Path('work/sony-dpt-controls-deepening'); W.mkdir(exist_ok=True)
BASE='https://helpguide.sony.net/dpt/rp1/v1/en/'
def sha(b): return hashlib.sha256(b).hexdigest()
class Article(HTMLParser):
    def __init__(self): super().__init__(); self.depth=0; self.out=[]
    def handle_starttag(self,t,a):
        d=dict(a)
        if t=='div' and (self.depth or d.get('class')=='article'): self.depth+=1
        if self.depth and t in ('p','li','h1','h2','h3','br','tr'): self.out.append('\n')
    def handle_endtag(self,t):
        if t=='div' and self.depth: self.depth-=1
        if self.depth and t in ('p','li','h1','h2','h3','tr'): self.out.append('\n')
    def handle_data(self,s):
        if self.depth: self.out.append(s)
log=[]
def get(url,key):
    row={'url':url,'checked_on':'2026-10-27','method':'ordinary_public_GET'}
    try:
        with urlopen(Request(url,headers={'User-Agent':'Mozilla/5.0'}),timeout=30) as r:
            data=r.read(); row.update(status=r.status,final_url=r.url)
        p=W/(key+'.html');p.write_bytes(data);row.update(raw_work_path=str(p),raw_sha256=sha(data))
        s=data.decode('utf-8-sig');a=Article();a.feed(s)
        text='\n'.join(x.strip() for x in ''.join(a.out).splitlines() if x.strip())
        p=W/(key+'.txt');p.write_text(text);row.update(text_work_path=str(p),text_sha256=sha(p.read_bytes()))
    except HTTPError as e: row.update(status=e.code,error=str(e));s=''
    except Exception as e: row.update(status=None,error=str(e));s=''
    log.append(row);(W/'fetch-log.json').write_text(json.dumps(log,ensure_ascii=False,indent=2))
    return s
s=get(BASE+'index.html','index')
nav=s.split('<nav',1)[1].split('</nav>',1)[0]
items=[]
for m in re.finditer(r'<a href="(contents/[^"]+)"[^>]*>(.*?)</a>',nav,re.S):
    title=unescape(re.sub('<[^>]+>','',m[2])).strip()
    items.append({'title':title,'url':urljoin(BASE,m[1]),'key':Path(m[1]).stem})
(W/'html-catalog.json').write_text(json.dumps(items,ensure_ascii=False,indent=2))
selected_titles={
'Document screen','Transferring a document from a computer','Transferring a document to a computer',
'Changing the organization of documents/folders on the Digital Paper','Adding/editing a note template',
'Handwriting on a document','Adding a highlight','Using the eraser','Displaying the annotation list',
'Limiting edits to annotations','Using the PDF form','Creating a new note','Adding/deleting a page from a note',
'Selecting defaults for the note template','Calibrating the pen','Displaying a document on the computer',
'Displaying the screen of the Digital Paper on the computer via Digital Paper App',
'Changing the settings on the Digital Paper','Changing the Digital Paper settings using the Digital Paper App',
'Setting the screen lock','Disabling the screen lock via FeliCa card/FeliCa-compatible device',
'Connecting via Wi-Fi/Connecting using a USB cable','Connecting via Bluetooth',
'Connecting a second and subsequent Digital Paper','Updating the software','Setting up the Digital Paper App',
'The screen is not appearing','Cannot edit or delete annotations','Cannot add annotations',
'Creating a side-note on an open document','Copying/cutting and moving handwritten items',
'Syncing the Digital Paper with a computer','Zooming in on a document','Display page thumbnails',
'Searching for a string of text within a document'}
seen=set()
for x in items:
    if x['title'] in selected_titles and x['url'] not in seen:
        seen.add(x['url']);get(x['url'],x['key'])
print(json.dumps({'catalog_rows':len(items),'unique_catalog_urls':len({x['url'] for x in items}),
    'requests':len(log),'statuses':[r.get('status') for r in log]},ensure_ascii=False))
