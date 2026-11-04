"""Keep the two audit summaries synchronized as evidence is confirmed."""
import json
from pathlib import Path

DIR = Path(__file__).resolve().parent.parent


def finding(item):
    path = DIR / "findings.json"
    items = json.loads(path.read_text())
    items = [x for x in items if x["id"] != item["id"]] + [item]
    items.sort(key=lambda x: x["id"])
    path.write_text(json.dumps(items, ensure_ascii=False, indent=2) + "\n")
    sync()


def verification(id, verdict, evidence, note):
    path = DIR / "verification.json"
    items = json.loads(path.read_text())
    item = dict(id=id, verdict=verdict, evidence=evidence, note=note)
    items = [item if x["id"] == id else x for x in items]
    path.write_text(json.dumps(items, ensure_ascii=False, indent=2) + "\n")
    sync()


def sync():
    path = DIR / "report.md"
    s = path.read_text()
    ver = json.loads((DIR / "verification.json").read_text())
    a = s.index("| 编号 | 结论 |")
    b = s.index("\n## 新问题", a)
    table = "| 编号 | 结论 | 证据 | 说明 |\n| --- | --- | --- | --- |\n"
    for x in ver:
        table += "| " + " | ".join(x[k].replace("|", "\\|") for k in ("id", "verdict", "evidence", "note")) + " |\n"
    s = s[:a] + table + s[b:]
    a = s.index("\n## 新问题")
    b = s.index("\n## 分块核查记录", a)
    new = "\n## 新问题\n\n"
    for x in json.loads((DIR / "findings.json").read_text()):
        new += f'### {x["id"]}（{x["severity"]}）{x["title"]}\n\n'
        new += x["evidence_type"] + "。位置：" + "、".join(f"`{p}`" for p in x["locations"]) + "。\n\n"
        new += "影响与定级：" + x["impact"] + "\n\n"
        new += "复现证据：" + x["reproduce"] + "\n\n"
        new += "建议：" + x["recommendation"] + "\n\n"
    s = s[:a] + new + s[b:]
    path.write_text(s)
