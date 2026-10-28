from pathlib import Path
from urllib.request import Request, urlopen
from urllib.error import HTTPError, URLError
from html.parser import HTMLParser
from hashlib import sha256
import json
import sys

BASE = Path(__file__).resolve().parent
RAW = BASE / 'raw'
RAW.mkdir(exist_ok=True)

class TextParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.parts = []
        self.skip = 0
    def handle_starttag(self, tag, attrs):
        if tag in ('script', 'style'):
            self.skip += 1
    def handle_endtag(self, tag):
        if tag in ('script', 'style'):
            self.skip = max(0, self.skip - 1)
    def handle_data(self, data):
        if not self.skip and data.strip():
            self.parts.append(data.strip())

for key, url in zip(sys.argv[1::2], sys.argv[2::2]):
    record = {'key': key, 'url': url, 'checked_on': '2026-10-27',
              'access_mode': 'anonymous_urllib_GET', 'state_change': 'local_research_capture_only'}
    try:
        with urlopen(Request(url, headers={'User-Agent': 'Mozilla/5.0'}), timeout=30) as response:
            data = response.read()
            record.update(status=response.status, final_url=response.url,
                          content_type=response.headers.get('Content-Type'))
    except HTTPError as error:
        data = error.read()
        record.update(status=error.code, final_url=error.url, stopped=True)
    except (URLError, TimeoutError) as error:
        record.update(error=str(error), stopped=True)
        data = b''
    if data:
        path = RAW / (key + '.html')
        path.write_bytes(data)
        record.update(raw_work_path=str(path.relative_to(BASE.parents[1])), raw_sha256=sha256(data).hexdigest())
        if record.get('status') == 200:
            parser = TextParser()
            parser.feed(data.decode('utf-8', errors='replace'))
            extracted = '\n'.join(parser.parts) + '\n'
            text_path = RAW / (key + '.txt')
            text_path.write_text(extracted)
            record.update(extracted_work_path=str(text_path.relative_to(BASE.parents[1])), extracted_sha256=sha256(extracted.encode()).hexdigest())
    with (BASE / 'captures.jsonl').open('a') as output:
        output.write(json.dumps(record, ensure_ascii=False) + '\n')
    print(json.dumps(record, ensure_ascii=False))
    if record.get('status') in (401, 403, 429) or 'error' in record:
        break
