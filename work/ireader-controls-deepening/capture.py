from pathlib import Path
from urllib.request import urlopen, Request
from urllib.error import HTTPError, URLError
from html.parser import HTMLParser
import hashlib, json

W = Path('/home/workspace/reader/work/ireader-controls-deepening')
class Text(HTMLParser):
    def __init__(self):
        super().__init__(); self.skip = 0; self.items = []
    def handle_starttag(self, tag, attrs):
        if tag in ('script', 'style'): self.skip += 1
    def handle_endtag(self, tag):
        if tag in ('script', 'style'): self.skip = max(0, self.skip - 1)
    def handle_data(self, value):
        if not self.skip and value.strip(): self.items.append(value.strip())

url = 'https://apps.apple.com/cn/app/id463150061'
record = {'url': url, 'checked_on': '2026-10-27', 'access_mode': 'ordinary_anonymous_GET'}
try:
    with urlopen(Request(url, headers={'User-Agent': 'Mozilla/5.0'}), timeout=30) as response:
        body = response.read()
        record.update(status=response.status, final_url=response.url, bytes=len(body))
    raw = W / 'app-store.html'; raw.write_bytes(body)
    parser = Text(); parser.feed(body.decode('utf-8'))
    raw.with_suffix('.txt').write_text('\n'.join(parser.items) + '\n')
    record.update(raw_work_path=str(raw.relative_to(W.parents[1])), raw_sha256=hashlib.sha256(body).hexdigest())
except (HTTPError, URLError, TimeoutError) as error:
    record.update(status=getattr(error, 'code', None), error=str(error), stopped=True)
(W / 'captures.json').write_text(json.dumps([record], ensure_ascii=False, indent=2) + '\n')
print(json.dumps(record, ensure_ascii=False))
