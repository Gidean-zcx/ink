import csv, hashlib, json, re
from pathlib import Path
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library'
files=['M-059-sony-dpt.md','M-060-quaderno.md','M-061-huawei-paper.md','M-062-lenovo-paper.md']
scopes={
 'M-059':'DPT-RP1/DPT-CP1; official firmware 1.6 guide; actual installed build unknown',
 'M-060':'QUADERNO Gen.3C FMVDP43CA4/FMVDP53CA5; B5FL-2101-02 Z0 published 2026-04-01; firmware unknown',
 'M-061':'MatePad Paper HarmonyOS 2.1 help; Huawei Reading / iReader e-ink edition kept separate; per-source conditions in case',
 'M-062':'Lenovo Smart Paper SP101FU; 2023 UM927222 guide; HT514875 current indexed help with regional date differences; installed firmware unknown',
}
gaps={
 'M-059':['菜单实际触控与状态保持','新系统兼容性、失败恢复','快速列表外文档与恢复','不同PDF和返回位置','原文删除、重命名与跨软件关系','剪贴取消与重启实测','漏检和实际定位','多端并发/传输中断','取消配置实际回执','设置/历史/账号恢复范围'],
 'M-060':['显示时延、残影量化','连续翻页误停','成绩/复习计划无证据，模式恢复实测','回收站满与同名恢复实测','重复文件与中断','备份失败及范围验证','覆盖确认与中断恢复','冲突回执和重复重试'],
 'M-061':['逐app持久性和实际刷新','重连失败、重复导入和进度','跨app模式及笔迹保存','重排状态持久性与位置','时长不足/网络中断恢复','录音对象备份恢复','触发范围与误触','焦点/禁止手写模式实测','跨布局稳定定位','跨设备笔迹恢复'],
 'M-062':['实际手势阈值与误触','重复、中断与取消','删除录音后关系和精确定位','整本听书/跨端断点','退出后恢复位置','副本关系、授权失败','过期/满空间/冲突和恢复','重置后实际恢复范围'],
}
raw={
 'E-DPT-01':'sony-dpt.pdf','E-DPT-10':'sony-dpt.pdf',
 'E-QUA-01':'quaderno-gen3c.pdf','E-QUA-04':'quaderno-gen3c.pdf',
 'E-LNP-01':'lenovo-smart-paper.pdf','E-LNP-04':'lenovo-smart-paper.pdf','E-LNP-03':'lenovo-manual-api.json',
}
for i,n in enumerate(['15879064','15881450','15881453','15881454','15898323','15960354'],1):raw[f'E-HWP-{i:02}']='huawei-'+n+'.html'
raw['E-HWP-08']='huawei-15939571.html'
visual={
 'E-DPT-10':['artifacts/extension/sony-dpt-p13.png'],
 'E-QUA-04':['artifacts/extension/quaderno-gen3c-p68.png','artifacts/extension/quaderno-gen3c-p69.png'],
 'E-LNP-04':['artifacts/extension/lenovo-p8.png'],
}
evidence=[];flows=[]
for filename in files:
 body=(lib/'cases'/filename).read_text();pid=filename[:5]
 for eid,title,url,locator in re.findall(r'^- (E-[A-Z]+-\d{2})：\[([^\]]+)\]\(([^)]+)\)([^\n]*)',body,re.M):
  mode='opened_text'
  if eid in ['E-DPT-02','E-DPT-03','E-DPT-04','E-DPT-07','E-DPT-08','E-LNP-02']:mode='official_index_text'
  if eid in raw:mode='downloaded_public_pdf_text' if raw[eid].endswith('.pdf') else 'downloaded_public_html_or_json_text'
  record=dict(evidence_id=eid,product_id=pid,title=title,url=url,checked_on='2026-10-26',source_type='official_manual' if '.pdf' in url else 'official_help',access_mode=mode,
   version_scope=scopes[pid],app_build=None,firmware_version=None,source_updated_on={'E-QUA-01':'2026-04-01','E-QUA-04':'2026-04-01','E-LNP-01':'2023-05-19'}.get(eid),
   locator=locator.lstrip('，。 '),note='事实、范围、未核查分支见案例；无实机或外部账户操作。',hands_on=False,visual_observed=eid in visual,
   archived_artifact=None,sha256=None,capture_state='bibliography_and_short_note_only',supersedes=None)
  if eid in raw:
   p=root/'work/research/extension-manuals'/raw[eid];record.update(raw_work_path=str(p.relative_to(root)),raw_sha256=hashlib.sha256(p.read_bytes()).hexdigest())
  if eid in visual:
   assets=visual[eid];record.update(access_mode='local_pdf_page_render_viewed',archived_artifact=assets[0],sha256=hashlib.sha256((lib/assets[0]).read_bytes()).hexdigest(),rendered_artifacts=assets,artifact_hashes={a:hashlib.sha256((lib/a).read_bytes()).hexdigest() for a in assets},capture_state='selected_static_page_excerpt',derived_from=eid.rsplit('-',1)[0]+'-01')
  if eid=='E-HWP-07':record['access_limitation']='Web extracted body read; direct local request redirected to search page, not archived as article body.'
  if eid=='E-LNP-02':record['access_limitation']='Official indexed article read; direct web shell and local HTTP 403. Regional modified dates vary.'
  evidence.append(record)
 for line in body.splitlines():
  if not line.startswith('| F-M'):continue
  c=[s.strip() for s in line.strip('|').split('|')];fid,task=c[0].split(' ',1);idx=int(fid[-2:])-1
  refs=re.findall(r'E-[A-Z]+-\d{2}',c[-1])
  # Short-form references within the same evidence family (e.g. E-DPT-01、10).
  for short in re.findall(r'、(\d{2})(?:、|$)',c[-1]):
   refs.append(refs[0].rsplit('-',1)[0]+'-'+short)
  seen=any(e in visual for e in refs)
  flows.append(dict(flow_id=fid,product_id=pid,task=task,depth='D2',evidence_ids=';'.join(refs),known_branches=c[2],gaps=gaps[pid][idx],visual_status='static_manual_pages_only' if seen else 'not_observed',hands_on='false',case_path='cases/'+filename,checked_on='2026-10-26'))
(lib/'batches/extension-manuals-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in evidence))
with (lib/'batches/extension-manuals-coverage.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(flows[0]));w.writeheader();w.writerows(flows)
print(json.dumps(dict(cases=len(files),evidence=len(evidence),flows=len(flows),static_evidence=len(visual))))
