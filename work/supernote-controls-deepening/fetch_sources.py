from pathlib import Path
from urllib.request import urlopen
from html.parser import HTMLParser
from concurrent.futures import ThreadPoolExecutor
import hashlib, json

ROOT = Path('/home/workspace/reader/work/supernote-controls-deepening')
SOURCES = [
 ('03-todo', 'https://support.supernote.com/the-to-do-app'),
 ('04-search', 'https://support.supernote.com/en_US/organizing/global-search-using-categorization'),
 ('05-recognition', 'https://support.supernote.com/en_US/Tools-Features/handwriting-recognition'),
 ('06-edit', 'https://support.supernote.com/en_US/organizing/2126285-untitled-article'),
 ('07-toolbar', 'https://support.supernote.com/en_US/introduction-to-the-toolbar'),
 ('08-auto-sync', 'https://support.supernote.com/supernote-cloud-auto-sync?kb_language=en_US'),
 ('09-sync-trigger', 'https://support.supernote.com/en_US/faq/when-do-my-notes-get-automatically-synced-to-supernote-cloud-'),
 ('10-changelog', 'https://support.supernote.com/changelog-for-manta-and-nomad'),
 ('11-transfer', 'https://support.supernote.com/en_US/organizing/transfer-files'),
]
class Text(HTMLParser):
    def __init__(self):
        super().__init__(); self.parts=[]; self.skip=0
    def handle_starttag(self, tag, attrs):
        if tag in ('script', 'style'): self.skip += 1
    def handle_endtag(self, tag):
        if tag in ('script', 'style'): self.skip = max(0,self.skip-1)
    def handle_data(self, data):
        if not self.skip and data.strip(): self.parts.append(data.strip())
def get(item):
    name,url=item
    try:
        with urlopen(url, timeout=30) as response:
            data=response.read(); status=response.status; final=response.url
        path=ROOT/(name+'.html');path.write_bytes(data)
        parser=Text();parser.feed(data.decode('utf-8'))
        (ROOT/(name+'.txt')).write_text('\n'.join(parser.parts))
        return dict(name=name,url=url,final_url=final,status=status,bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),path=str(path.relative_to(ROOT.parent.parent)))
    except Exception as exc:
        return dict(name=name,url=url,error=str(exc),retry=False)
with ThreadPoolExecutor(max_workers=4) as pool: results=list(pool.map(get,SOURCES))
(ROOT/'http-snapshots.json').write_text(json.dumps(results,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(results,ensure_ascii=False,indent=2))
if any('error' in r for r in results): raise SystemExit(1)
