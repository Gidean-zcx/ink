"""One ordinary anonymous GET for each of four public static help images."""
import hashlib
import json
import urllib.error
import urllib.request
from pathlib import Path

WORK = Path(__file__).resolve().parent
assert not (WORK / "image-fetch.json").exists(), "Do not repeat this capture"
images = [
    (10701497927316, 2, "25400385712276", "library-scan.jpg"),
    (10701578837268, 10, "40687025430292", "notes-page-manager.png"),
    (10701578837268, 13, "40687065448468", "notes-layers.png"),
    (25400769451540, 2, "40686663742996", "epub-more-settings.png"),
]
results = []
for article, image_number, attachment, filename in images:
    url = "https://booxsupport.zendesk.com/hc/article_attachments/" + attachment
    row = {"article_id": article, "image_number": image_number, "url": url,
           "checked_on": "2026-10-27", "method": "anonymous_ordinary_GET_once",
           "actual_view": False}
    try:
        with urllib.request.urlopen(url, timeout=30) as response:
            data = response.read()
            row.update(status=response.status, final_url=response.geturl(),
                       content_type=response.headers.get("Content-Type"))
        assert row["content_type"].startswith("image/"), row
        target = WORK / "raw" / filename
        target.write_bytes(data)
        row.update(raw_work_path="work/boox-reading-controls-deepening/raw/" + filename,
                   raw_sha256=hashlib.sha256(data).hexdigest(), bytes=len(data))
    except (urllib.error.HTTPError, urllib.error.URLError) as error:
        row.update(status=getattr(error, "code", None), error=str(error), retry=False)
    results.append(row)
    print(json.dumps(row, ensure_ascii=False))
    if row.get("status") in (401, 403, 412, 429):
        break
(WORK / "image-fetch.json").write_text(json.dumps(results, ensure_ascii=False, indent=2)+"\n")
