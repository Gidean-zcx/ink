import csv
import json
import re
from pathlib import Path

lib=Path('/home/workspace/reader/docs/research/interaction-library')
cases=['M-041-notebooklm.md','M-042-gemini.md','M-043-shortcuts.md']
scopes={
'M-041':'NotebookLM historical announcements 2025-11-06 / 2026-03-20; current help redirects to Gemini Notebook; desktop primary; build unknown',
'M-042':'Gemini Android; current official help; device/language/account restrictions; screen automation beta; build unknown',
'M-043':'Official Shortcuts 9.0 / iOS 26 guide path; no installed build or operation test',
}
gaps={
'M-041':['重复/超限实际反馈、资料准确性','原文变更后旧产物是否更新','矛盾来源处理、引用精度','逐题编辑、评分撤销、自动复习规则','多端冲突、删题恢复、精确断点','中文互动、断网、真实恢复位置','移除引用后可追溯性、删除恢复'],
'M-042':['屏幕外/历史数据覆盖','每个连接器全动作及部分失败','重复对象、确认与错误回执','拒绝权限后已执行动作','实际支持名单、部分修改与重试','不同设备接管、取消后的持久状态'],
'M-043':['运行副作用能否撤回','任意app页面/屏幕外数据','缺输入实际反馈','取消输入后的链状态','拒绝后的部分结果','进程终止、事务回滚、恢复入口'],
}
evidence=[];flows=[]
for filename in cases:
    p=lib/'cases'/filename;body=p.read_text();pid=filename[:5]
    for eid,title,url,locator in re.findall(r'^- (E-[A-Z]+-\d{2})：\[([^\]]+)\]\(([^)]+)\)([^\n]*)',body,re.M):
        mode='official_index_text' if eid in ('E-GEM-04','E-GEM-05') else 'opened_text'
        kind='official_release_notes' if eid in ('E-NBL-04','E-NBL-07') else 'official_help'
        record=dict(evidence_id=eid,product_id=pid,title=title,url=url,checked_on='2026-10-26',source_type=kind,access_mode=mode,
            version_scope=scopes[pid],app_build='Shortcuts guide 9.0; not installed app' if pid=='M-043' else None,
            firmware_version='iOS 26 guide; not tested OS' if pid=='M-043' else None,
            source_updated_on={'E-NBL-04':'2026-03-20','E-NBL-07':'2025-11-06'}.get(eid),
            locator=locator.lstrip('，。 '),note='范围与未核实分支见案例；无登录、生成或实际执行。',hands_on=False,
            visual_observed=False,archived_artifact=None,sha256=None,capture_state='bibliography_and_short_note_only',supersedes=None)
        if pid=='M-041':record['version_warning']='Evergreen help has redirects and newer capabilities; do not backdate to dated launch announcements.'
        if eid in ('E-GEM-04','E-GEM-05'):record['access_limitation']='Canonical direct open failed; official indexed body read; locale/index snapshots disagree on rollout scope.'
        evidence.append(record)
    for line in body.splitlines():
        if not line.startswith('| F-M'):continue
        cells=[s.strip() for s in line.strip('|').split('|')]
        fid,task=cells[0].split(' ',1)
        number=int(fid[-2:])
        refs=re.findall(r'E-[A-Z]+-\d{2}',cells[-1])
        flows.append(dict(flow_id=fid,product_id=pid,task=task,depth='D2',evidence_ids=';'.join(refs),known_branches=cells[2],
            gaps=gaps[pid][number-1],visual_status='not_observed',hands_on='false',case_path='cases/'+filename,checked_on='2026-10-26'))
(lib/'batches/ai-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in evidence))
with (lib/'batches/ai-coverage.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(flows[0]));w.writeheader();w.writerows(flows)
print(json.dumps(dict(cases=len(cases),evidence=len(evidence),flows=len(flows))))
