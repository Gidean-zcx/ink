"""Extract ten selected articles from an existing local public-guide capture."""
import hashlib
import json
import re
from html.parser import HTMLParser
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / "work/boox-reading-controls-deepening"
SOURCE = ROOT / "work/boox-controls-deepening/epaper-pc-articles.json"
IDS = [10701497927316,10701489683092,10701488996500,10701486858388,
       25400769451540,10701491126548,10701454503572,10701578837268,
       15196391289108,10701570125588]


class Extract(HTMLParser):
    def __init__(self):
        super().__init__()
        self.parts = []
        self.images = []
        self.links = []

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag in {"p", "div", "li", "h1", "h2", "h3", "h4", "br", "tr"}:
            self.parts.append("\n")
        if tag == "img":
            self.images.append(attrs)
            self.parts.append(" [image:%d] " % len(self.images))
        if tag == "a" and attrs.get("href"):
            self.links.append(attrs["href"])

    def handle_data(self, text):
        self.parts.append(text)

    def handle_endtag(self, tag):
        if tag in {"p", "div", "li", "h1", "h2", "h3", "h4", "tr"}:
            self.parts.append("\n")


articles = {x["id"]: x for x in json.loads(SOURCE.read_text())["articles"]}
index = []
for article_id in IDS:
    article = articles[article_id]
    parser = Extract()
    parser.feed(article["body"])
    text = re.sub(r"\n[ \t]*\n+", "\n", "".join(parser.parts)).strip()
    prefix = WORK / "raw" / str(article_id)
    prefix.with_suffix(".json").write_text(json.dumps(article, ensure_ascii=False, indent=2)+"\n")
    prefix.with_suffix(".txt").write_text(text+"\n")
    index.append({"article_id": article_id, "title": article["title"],
                  "url": article["html_url"], "edited_at": article.get("edited_at"),
                  "source_updated_on": article.get("updated_at"), "images": parser.images,
                  "links": parser.links, "text_path": str(prefix.with_suffix(".txt").relative_to(ROOT))})
(WORK / "article-index.json").write_text(json.dumps(index, ensure_ascii=False, indent=2)+"\n")
print(json.dumps({"articles":len(index), "network_requests":0,
                  "source_sha256":hashlib.sha256(SOURCE.read_bytes()).hexdigest()}))
