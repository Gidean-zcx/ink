from pathlib import Path
import json,csv,hashlib,re,yaml
root=Path('.');lib=root/'docs/research/interaction-library';b=root/'work/maimemo-controls-deepening';a=lib/'artifacts/maimemo-controls-deepening';a.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
fetch={x['name']:x for f in ['fetch-results.json','fetch-results2.json','fetch-results3.json'] for x in json.loads((b/f).read_text())}
meta=[
('02-basics','一些基础使用小技巧','四条正文：记忆模式、批选、章节、排序','F-M017-01','入口补证；完整拼写判断链未知'),
('04-pie','已选单词饼图','正文：红环、灰色勾圈、熟知度排序','F-M017-01','展示口径不等于独立测验结果'),
('05-makeup','补签卡条件与计算','正文两步及末段按钮条件','','统计修复规则；发卡、奖励与完整签到流程未知'),
('06-checkin-error','当天签到失败','正文：切换网络与客服入口','','排障建议，不是成功恢复记录'),
('07-filter','释义/助记/例句标签筛选','正文与 PS 三点','F-M017-08','仅文档；未观察标签状态'),
('08-super','超级释义功能介绍','一至七：入口、设置、显示优先级、导出、反馈','F-M017-08;F-M017-09','正文现有考点能力与后续计划措辞并存'),
('09-exam','超级释义考点义项','使用条件、开启方式、功能介绍、真题范围','F-M017-08','5.5.65+；平台未注明；考研范围'),
('10-pdf','导出 PDF','一、二、五、七、八、九','F-M017-09','生成、保存、退出与闪退分支；无实操'),
('11-open','开放平台','开发流程、接入授权、开始开发、限制','F-M017-07','开发者审核与终端用户授权分别处理'),
('16-api_bundle','墨墨开放 API v1 公开规范','info.description；paths /api/v1/memo/*；StudyProgress/StudyTodayItem/StudyRecord','F-M017-07','规范读取；没有生成凭据或调用用户接口'),
('14-ios-audio','iOS 单词不发音','1 系统设置；2 蓝牙耳机多设备连接','','iOS 专属排障；未实际听音'),
('15-harmony','墨墨背单词 HarmonyOS 日志','2025-04-22/07-08/2026-05-29/07-31/10-10 条目','F-M017-01;F-M017-02','历史新增/修复仅作版本边界，不拼接当前流程'),
('19-newload','为何不固定每日新学量','正文：新学与复习负荷','','调度解释；不能据建议断言不支持固定设置')]
notes=['# 墨墨软件补证取得记录','', '查阅：2026-10-27。只读官方 HTML 与公开 API 规范；没有 App、账户或接口操作，没有图片观察。此文件只保存短定位与来源，完整响应在 work；文件哈希证明取得字节，不证明功能可用。','']
ev=[]
for i,(n,title,loc,fs,note) in enumerate(meta,1):
 eid=f'E-MMCD-{i:02}'
 if n=='16-api_bundle':
  r=json.loads((b/'schema-fetch.json').read_text());r['status']=r['http_status'];r['name']=n
 else:r=fetch[n]
 typ='official_openapi_schema' if i==10 else 'official_release_notes' if i==12 else 'official_help_article'
 scope='中文软件帮助，精确构建/发布日期未注明；平台条件见正文'
 if i==7:scope='帮助明确最低 5.5.65；未注明平台，不套用 HarmonyOS 独立版本号'
 if i==10:scope='OpenAPI 3.1.0；info.version=v1；仅墨墨背单词标签，排除墨墨记忆卡；学习方法为公测'
 if i==11:scope='iOS；系统与 App 构建未注明'
 if i==12:scope='HarmonyOS 独立日志；顶部 5.5.19.0069（2026-10-10），历史条目逐项限定'
 row=dict(evidence_id=eid,product_id='M-017',title=title,url=r['url'],checked_on='2026-10-27',source_type=typ,access_mode='http_get_public_document',version_scope=scope,app_build='5.5.65+' if i==7 else None,firmware_version=None,source_updated_on=None,locator=loc,note=note,hands_on=False,visual_observed=False,visual_audit=None,archived_artifact='artifacts/maimemo-controls-deepening/source-notes.md',sha256=None,capture_state='HTTP 200 source saved; text only; no UI operation',raw_capture_kind='public_openapi_yaml' if i==10 else 'official_HTML',raw_work_path=r['raw_work_path'],raw_sha256=r['raw_sha256'],parsed_text_path=None if i==10 else str(b/(n+'-main.txt')),http_status=200,supersedes=None)
 if i==10: row.update(qualifies=['E-MMO-09'],discovery_url='https://open.maimemo.com/',discovery_raw_work_path=fetch['12-api-site']['raw_work_path'],discovery_raw_sha256=fetch['12-api-site']['raw_sha256'])
 ev.append(row)
 notes += [f'## {eid} {title}',f'- 来源：[{title}]({r["url"]})。',f'- 定位：{loc}。',f'- 原件：`{r["raw_work_path"]}`；SHA-256：`{r["raw_sha256"]}`。',f'- 关联：{fs or "功能目录/未完成分支，未新建流程"}。', '']
notes += ['## 复查但不重复登记 E','', '原 E-MMO-01/02/10/11 的自评、每日量、目录、iOS 日志已重新取得，保留旧编号；本轮原件及哈希见 work 的 fetch-results*.json。断签专页正文只有人工客服入口，未取得跨日恢复说明。API 主页只含规范组件，实际依据为其显式 apiDescriptionUrl 指向的 YAML；未读取 token URL 或开发者控制台。', '']
(a/'source-notes.md').write_text('\n'.join(notes));ah=sha(a/'source-notes.md')
for row in ev:row['sha256']=ah
(lib/'batches/maimemo-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in ev))
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
flows=[['F-M017-08','M-017','筛选和固定词条学习内容','D2','E-MMCD-05;E-MMCD-06;E-MMCD-07','标签筛选与固定选择；超级释义设置、缺词回退及考点查看','自创内容审核/冲突/删除恢复、报错结果和跨端偏好同步未知','not_observed','false','cases/M-017-maimemo.md','2026-10-27'],['F-M017-09','M-017','将词列表导出为 PDF','D2','E-MMCD-08;E-MMCD-06','选词→样式→确认生成→本地保存；退出与闪退分支','设备保存器、取消确认、断网/失败扣次判定及文件恢复未知','not_observed','false','cases/M-017-maimemo.md','2026-10-27']]
with (lib/'batches/maimemo-controls-deepening-coverage.csv').open('w',newline='') as f:
 w=csv.writer(f,lineterminator='\n');w.writerow(fields);w.writerows(flows)
# Inventory is the exact software sidebar slice plus release/open-platform entries; excluded device leaves retained explicitly.
links=json.loads((b/'01-start-links.json').read_text());app=links[3:33]
source_to_e={r['url'].rstrip('/'):r['evidence_id'] for r in ev}
old=[('Ok6Yw','E-MMO-01','F-M017-01'),('Pulqw','E-MMO-02','F-M017-01;F-M017-02'),('Y0iDw','E-MMO-03','F-M017-01;F-M017-02;F-M017-05'),('QKUlw','E-MMO-04','F-M017-03'),('TJDKw','E-MMO-05','F-M017-04'),('Nw46w','E-MMO-06','F-M017-04'),('KE8Zw','E-MMO-07','F-M017-05'),('YGRKw','E-MMO-08','F-M017-06')]
inv=[]
for j,r in enumerate(app,1):
 u='https://memodocs.maimemo.com'+r['url'];eid=source_to_e.get(u,'');fs='';status='directory_only';gap='入口/动作/结束与异常尚未分析'
 for s,e,f in old:
  if s in u:eid=e;fs=f;status='partial_documented';gap='见既有 F 的未核实栏'
 if eid.startswith('E-MMCD'):
  m=meta[int(eid.rsplit('-',1)[1])-1];fs=m[3];status='partial_documented';gap=m[4]
 if j in [1,2,4,15]:status='parent_index';gap='目录父项，不计完整功能'
 if r['url'] in ['/docs/support_wordmachine','/docs/OPLXw0KlkiaSUDkN69ScWREfnhc']:status='excluded_other_product';gap='M023 单词机，不能补本软件学习链'
 if 'XMenwad' in u:status='read_no_procedure';gap='正文只有人工客服；跨日续学未知'
 inv.append(dict(item_id=f'M017-H{j:02}',kind='help_node',title=r['title'],source_url=u,evidence_ids=eid or 'E-MMO-10',flow_ids=fs,coverage=status,gaps=gap,checked_on='2026-10-27'))
for key in ['product-log-memo-ios','product-log-memo-android','product-log-memo-harmonyos','product-log-memo-web','open','PNvOw2E1AivPtlkRlQgce1W1n2c','open-terms']:
 candidates=[r for r in links if r['url'].rstrip('/')=='/docs/'+key]
 if not candidates:continue
 r=candidates[0];u='https://memodocs.maimemo.com'+r['url'];eid=source_to_e.get(u,'E-MMO-11' if key.endswith('-ios') else 'E-MMO-09' if key.startswith('PNv') else 'E-MMO-10')
 inv.append(dict(item_id=f'M017-S{len(inv)-29:02}',kind='support_index',title=r['title'],source_url=u,evidence_ids=eid,flow_ids='F-M017-07' if key=='open' or key.startswith('PNv') else '',coverage='partial_documented' if eid!='E-MMO-10' else 'directory_only',gaps='平台/历史边界按案例；不由日志还原按钮连续过程',checked_on='2026-10-27'))
ops=json.loads((b/'api-operations.json').read_text())
for j,o in enumerate(ops,1):
 inv.append(dict(item_id=f'M017-A{j:02}',kind='api_operation',title=o['summary'],source_url='https://open.maimemo.com/api_bundle.yaml',evidence_ids='E-MMCD-10',flow_ids='F-M017-07',coverage='contract_read_not_called',gaps=('未列响应结构；' if not o['responses'] else '')+'失败码/回滚/幂等未知；'+o['method'].upper()+' '+o['path'],checked_on='2026-10-27'))
with (lib/'batches/maimemo-controls-deepening-inventory.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(inv[0]),lineterminator='\n');w.writeheader();w.writerows(inv)
# Exact old-field proposals, no mutation of prior batches or masters.
oldrows={r['flow_id']:r for r in json.loads((b/'existing-coverage-before.json').read_text())}
changes={
'F-M017-01':dict(evidence_ids='E-MMO-01;E-MMO-02;E-MMO-03;E-MMCD-01;E-MMCD-02;E-MMCD-12',known_branches='选词批选/章节/熟知度排序；英中与中英模式入口；当天首次自评和跨书共享；HarmonyOS 控制仅日志发现',gaps='显示答案与拼写判分重试、误评回滚、结束页操作、后台/跨日恢复未知；返回上词不等于撤销',checked_on='2026-10-27'),
'F-M017-02':dict(evidence_ids='E-MMO-02;E-MMO-03;E-MMO-11;E-MMCD-13;E-MMCD-12',known_branches='追加新词即时学；固定新学量是官方不建议而非能力禁止；跨平台版本有别',gaps='当前小组如何重算、固定值/最大值设置细节、积压跨日重排与分散压力确认步骤未知',checked_on='2026-10-27'),
'F-M017-07':dict(depth='D2',evidence_ids='E-MMO-09;E-MMCD-09;E-MMCD-10',known_branches='凭据/第三方授权分开；词 ID、词本和加入计划分开；规范返回成功数；公测/同步/当日初始化/频控限制',gaps='未调用接口；精确权限清单、错误响应/重试等待、幂等、撤销、删除恢复及硬件下发未知',checked_on='2026-10-27')}
proposals=[]
for fid,d in changes.items():
 for k,v in d.items():proposals.append(dict(flow_id=fid,field=k,before=oldrows[fid][k],after=v,reason='2026-10-27 官方软件帮助及公开契约补证；仅文档，保持稳定 F'))
with (b/'existing-coverage-updates.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=['flow_id','field','before','after','reason'],lineterminator='\n');w.writeheader();w.writerows(proposals)
print(json.dumps({'evidence':len(ev),'new_flows':len(flows),'inventory':len(inv),'old_field_changes':len(proposals),'notes_sha256':ah},ensure_ascii=False))
