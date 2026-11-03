"""Update audit records and the matching report sections from JSON on stdin."""
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent
data = json.load(sys.stdin)
for name in ("verification", "findings"):
    path = ROOT / (name + ".json")
    rows = {r["id"]: r for r in json.loads(path.read_text())}
    rows.update({r["id"]: r for r in data.get(name, [])})
    path.write_text(json.dumps(sorted(rows.values(), key=lambda r: r["id"]), ensure_ascii=False, indent=2) + "\n")
report = (ROOT / "report.md").read_text()
v = json.loads((ROOT / "verification.json").read_text())
table = "| 编号 | 结论 | 证据 | 说明 |\n| --- | --- | --- | --- |\n"
for r in v:
    table += "| " + " | ".join(str(r[k]).replace("|", "\\|").replace("\n", " ") for k in ("id", "verdict", "evidence_type", "note")) + " |\n"
findings = []
for r in json.loads((ROOT / "findings.json").read_text()):
    findings.append(f'### {r["id"]}｜{r["severity"]}｜{r["title"]}\n\n'
                    f'**范围：**{r["area"]}。**证据：**【{r["evidence_type"]}】。\n\n'
                    '**位置：**' + '；'.join('`' + p + '`' for p in r['locations']) + '。\n\n'
                    f'**核实与复现：**{r["reproduce"]}\n\n'
                    f'**影响：**{r["impact"]}\n\n'
                    f'**建议：**{r["recommendation"]}\n')
for name, body in (("verification", table), ("findings", '\n'.join(findings) or '尚在核查；没有记录不表示没有问题。\n')):
    a, b = f'<!-- {name}:start -->', f'<!-- {name}:end -->'
    left, rest = report.split(a)
    _, right = rest.split(b)
    report = left + a + '\n' + body + b + right
(ROOT / 'report.md').write_text(report)
