import csv
import json
from collections import Counter
from pathlib import Path

root = Path('/home/workspace/reader')
lib = root / 'docs/research/interaction-library'
batches = lib / 'batches'

# Preserve the initial snapshot once, before merging later batches.
for name in ('evidence.jsonl', 'coverage.csv'):
    baseline = batches / ('initial-' + name)
    if not baseline.exists():
        baseline.write_bytes((lib / name).read_bytes())

def read_csv(path):
    with path.open(newline='') as f:
        return list(csv.DictReader(f))

def merge(groups, key):
    result = {}
    for file, records in groups:
        for item in records:
            item_id = item[key]
            if item_id in result and result[item_id] != item:
                raise ValueError(f'Conflicting {item_id} in {file}')
            result[item_id] = item
    return sorted(result.values(), key=lambda x: (x['product_id'], x[key]))

evidence = merge([(p, [json.loads(l) for l in p.read_text().splitlines() if l.strip()])
                  for p in sorted(batches.glob('*-evidence.jsonl'))], 'evidence_id')
flows = merge([(p, read_csv(p)) for p in sorted(batches.glob('*-coverage.csv'))], 'flow_id')
products = read_csv(lib / 'catalog.csv')

def write_csv(path, records):
    with path.open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)

for p in products:
    cases = sorted((lib / 'cases').glob(p['product_id'] + '-*.md'))
    if len(cases) > 1:
        raise ValueError(f'Multiple primary cases for {p["product_id"]}: {cases}')
    subset = [f for f in flows if f['product_id'] == p['product_id']]
    if cases and subset:
        p['detailed_case'] = str(cases[0].relative_to(lib))
        p['case_status'] = 'partial_' + max(f['depth'] for f in subset)
        p['inventory_status'] = 'partial_function_domains'
        # The old seed question stays in the initial backlog; current gaps come
        # from the actual reviewed flow records instead of silently stale text.
        gaps = list(dict.fromkeys(f['gaps'] for f in subset if f['gaps']))
        p['next_gap'] = '；'.join(gaps[:2]) + '；完整功能与其余分支见案例，未宣告完成。'
    elif cases:
        p['detailed_case']=''
        p['case_status']='draft_not_indexed'
    p['feature_total'] = 'unknown'

(lib / 'evidence.jsonl').write_text(''.join(json.dumps(e, ensure_ascii=False) + '\n' for e in evidence))
write_csv(lib / 'coverage.csv', flows)
write_csv(lib / 'catalog.csv', products)

depths = Counter(f['depth'] for f in flows)
visuals = sum(bool(e.get('visual_observed')) and e.get('visual_kind')!='browser_sequence' for e in evidence)
operated = sum(f['hands_on']=='true' for f in flows)
case_count = sum(bool(p['detailed_case']) for p in products)
lines = ['# 案例与覆盖索引', '', '更新：2026-10-27。以下数字描述登记量，不表示全产品完成率。', '',
         f'当前 {len(products)} 组主目录、{case_count} 个部分案例、{len(flows)} 条流程、{len(evidence)} 条证据记录（{len({e["url"] for e in evidence})} 个不同 URL）。', '',
         '深度：' + '、'.join(f'{k} {depths.get(k,0)}' for k in ['D1','D2','D3','D4']) +
         f'。{visuals} 条证据含已查看的静态图/页；{operated} 条任务有自动化公开网页操作记录（具体环境见案例），尚无手机或电子纸实机操作记录。', '',
         '每项功能的缺口、版本范围和失败分支以案例与 coverage.csv 为准；目录之外已点名系列仍见 analysis-status.md。', '',
         '| 对象 | 案例 | 已登记任务 | 当前证据深度 |', '| --- | --- | --- | --- |']
for p in products:
    subset = [f for f in flows if f['product_id'] == p['product_id']]
    ds = Counter(f['depth'] for f in subset)
    link = f'[查看案例]({p["detailed_case"]})' if p['detailed_case'] else '尚无交互案例'
    desc = '、'.join(f'{k}: {v}' for k, v in sorted(ds.items())) or '目录线索'
    lines.append(f'| {p["product_id"]} {p["name"]} | {link} | {len(subset)} | {desc} |')
(lib / 'case-index.md').write_text('\n'.join(lines) + '\n')
print(json.dumps(dict(products=len(products), cases=case_count, flows=len(flows), depths=depths,
                     evidence_records=len(evidence), distinct_urls=len({e['url'] for e in evidence}),
                     visual_evidence=visuals), ensure_ascii=False))
