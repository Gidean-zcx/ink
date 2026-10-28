from pathlib import Path
import json,csv,hashlib,re
lib=Path('docs/research/interaction-library');b=Path('work/eudic-controls-deepening');a=lib/'artifacts/eudic-controls-deepening';a.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
fetch={r['name']:r for x in ['initial','help','limits'] for r in json.loads((b/(x+'-results.json')).read_text())}
rows=[]
def evidence(i,name,title,typ,scope,loc,note):
 r=fetch[name];return dict(evidence_id=f'E-EUCD-{i:02}',product_id='M-026',title=title,url=r['url'],checked_on='2026-10-27',source_type=typ,access_mode='http_get_public_document',version_scope=scope,app_build=None,firmware_version=None,source_updated_on=None,locator=loc,note=note,hands_on=False,visual_observed=False,visual_audit=None,archived_artifact='artifacts/eudic-controls-deepening/source-notes.md',sha256=None,capture_state='text_or_metadata_only; no UI or image observation',raw_capture_kind='official_HTML',raw_work_path=r['raw_work_path'],raw_sha256=r['raw_sha256'],http_status=r['status'],supersedes=None)
rows.append(evidence(1,'06-product','欧路词典官网产品入口与新日志链接','official_product_page','官网展示应用版本26.9.1；平台未对应，不当所有端当前构建','应用版本；查看更新日志；AI宣传区','产品声明D1；AI校订/文档翻译未形成操作链'))
e=evidence(2,'13-log','官方词典日志目录及分端页面摘要','official_release_metadata','只读官网所链接Yuque公开HTML内的doc.description；iOS/Android/桌面分开；未取得正文','book.toc；三个子页doc.title/description/content_updated_at','只确认分端摘要与入口；不据元数据发布时间给功能定发布日')
e['access_mode']='http_html_embedded_public_metadata';e['raw_capture_kind']='HTML with public appData metadata; body_asl empty';e['qualifies']=['E-EUD-12'];e['additional_raw_sources']=[]
for name in ['15-ios-log','16-android-log','17-desktop-log']:
 r=fetch[name];d=json.loads((b/(name+'-appData.json')).read_text())['doc'];e['additional_raw_sources'].append({k:r[k] for k in ['url','raw_work_path','raw_sha256'] }|{'parsed_metadata_path':str(b/(name+'-appData.json')),'title':d['title'],'content_updated_at':d['content_updated_at'],'metadata_updated_at':d['updated_at'],'body_available':False})
rows.append(e)
p=b/'14-aiarticle-search-capture.txt';r=fetch['14-aiarticle'];rows.append(dict(evidence_id='E-EUCD-03',product_id='M-026',title='AI短文助记功能升级：官方托管稿搜索正文',url=r['url'],checked_on='2026-10-27',source_type='official_historical_tutorial_indexed_text',access_mode='web_search_result_body_then_direct_HTTP_429_stopped',version_scope='历史稿；平台/构建/发布日期未核定；只取明确欧路词典路径，排除每日英语听力路径',app_build=None,firmware_version=None,source_updated_on=None,locator='如何使用；PS学习页；生成后导出',note='搜索响应正文支持有限流程；人工保留工具显示文本，不是原HTML；URL查询时间非发布日期',hands_on=False,visual_observed=False,visual_audit=None,archived_artifact='artifacts/eudic-controls-deepening/source-notes.md',sha256=None,capture_state='indexed text acquired before HTTP 429; route stopped; no media fetched',raw_capture_kind='manually_retained_visible_web_tool_excerpt; not publisher original HTML',raw_work_path=str(p),raw_sha256=sha(p),http_status=429,supersedes=None))
e=evidence(4,'18-privacy','欧路软件 AI 数据处理声明','official_policy_statement','欧路软件跨产品政策；只能约束声明口径，不能证明词典每项功能界面或技术实现','五、Ai相关技术；最近更新日期2025年9月20日','请求处理、文档问答存留与AI历史不同；未据政策创造UI流程');e['source_updated_on']='2025-09-20';rows.append(e)
# Root requested stop on blocked sources: indexed AI article remains only an access limitation.
rows=[e for e in rows if e['evidence_id']!='E-EUCD-03']
# Short formal provenance: no full manual excerpts.
notes=['# 欧路词典本轮材料与边界','','查阅2026-10-27。没有App/账户/接口操作、媒体下载或图像观察。以下保存章节与原件哈希，不复制帮助正文；新E之外，对旧E重新取得的文本只补原件定位，不重复增证据编号。','']
for e in rows:
 notes += [f'## {e["evidence_id"]} {e["title"]}',f'- 来源：[{e["title"]}]({e["url"]})。',f'- 定位：{e["locator"]}。',f'- 取得：{e["access_mode"]}；{e["note"]}。',f'- 原件/记录：`{e["raw_work_path"]}`；SHA-256 `{e["raw_sha256"]}`。','']
 for r in e.get('additional_raw_sources',[]):notes += [f'- 子页：[{r["title"]}]({r["url"]})；只取得摘要。`{r["raw_work_path"]}`；SHA-256 `{r["raw_sha256"]}`。']
notes += ['## 旧 E 的本轮原件定位','','旧编号保持；Markdown是官方公开页面自己提供的替代格式。字节哈希只证明取得该文本，不能证明当前App一致。','', '| 旧E | 本轮原件 | SHA-256 |', '| --- | --- | --- |']
old_map={'01-index':'E-EUD-13','02-scheme':'E-EUD-18','03-library':'E-EUD-10','04-reader':'E-EUD-02','07-cross':'E-EUD-14','08-fulltext':'E-EUD-15','09-display':'E-EUD-16','10-keyboard':'E-EUD-17','11-learnfaq':'E-EUD-08','12-wordbook':'E-EUD-05'}
for name,eid in old_map.items():
 r=fetch[name];notes.append(f'| {eid} | [{name}]({r["url"]})；`{r["raw_work_path"]}` | `{r["raw_sha256"]}` |')
notes += ['', '## 失败、排除与下一取证','','- API文档入口 `http://my.eudic.net/OpenAPI/Doc_Index` HTTP 200只有登录链接和“开发指南”标题，无公开接口正文；未点击登录、请求凭据或调用API。原件 `work/eudic-controls-deepening/05-api.html`，SHA-256 `'+fetch['05-api']['raw_sha256']+'`。','- AI短文稿先由搜索返回正文，随后直接HTTP返回429即停止；保存的是已显示工具文本的人工留存。没有取到原HTML，没有改参数/换域绕过。URL中的时间串与索引抓取年龄都不作发布日期。','- Yuque首个页面web工具不可读，普通HTTP可取公开HTML，但各页body_asl均为空；只解析已响应的元数据，未调用Yuque内容接口。','- 排除每日英语听力、欧路翻译插件及其他语种App专属流程；未使用搜索中的第三方安装路径和未核身份网站。','- 下一步需官方现行词典App内帮助或完整对应版本教程，核对触屏加词、词库替换、备份恢复、AI结果校订与删除；本轮无这些操作依据。','']
(a/'source-notes.md').write_text('\n'.join(notes));h=sha(a/'source-notes.md')
for e in rows:e['sha256']=h
(lib/'batches/eudic-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in rows))
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
flows=[
['F-M026-11','M-026','跨软件选择文本并查词翻译','D2','E-EUD-14','iOS默认翻译18.4+与分享；Android浮窗权限、框选和展开；旧LightPeek限制','权限拒绝、弹窗关闭、回来源位置、误识别与词本原句保存未知','not_observed','false','cases/M-026-eudic.md','2026-10-27'],
['F-M026-12','M-026','建立词库全文索引后查询','D2','E-EUD-15','启用索引→后台建立→完成状态→全文搜索；显示词库开关','强退、索引失败/重建、更新删除资源后处理和空结果未知','not_observed','false','cases/M-026-eudic.md','2026-10-27'],
['F-M026-13','M-026','整理解释页词典与定位内容','D2','E-EUD-16','自动展开数量、排序隐藏、目录与解释内搜索、字体主题','误隐藏恢复、配置持久化/跨端、冲突、搜索空结果未知','not_observed','false','cases/M-026-eudic.md','2026-10-27'],
['F-M026-14','M-026','用生词生成AI短文并导出','D2','E-EUCD-03','历史官方搜索正文：生词选择→风格/字数→双语文章→导出；学习完成页另入口','原HTML429不可得；平台构建与现行权益、校订/重生成、失败扣量、保存位置和恢复未知','not_observed','false','cases/M-026-eudic.md','2026-10-27']]
flows=[r for r in flows if r[0]!='F-M026-14']
with (lib/'batches/eudic-controls-deepening-coverage.csv').open('w',newline='') as f:w=csv.writer(f,lineterminator='\n');w.writerow(fields);w.writerows(flows)
# All 22 official index pages; child settings kept separate from page counts.
toc=re.findall(r'^- \[([^]]+)\]\(([^)]+)\)',(b/'01-index.txt').read_text(),re.M)
mapping={'单词查询':('E-EUD-01','F-M026-01'),'跨软件取词':('E-EUD-14','F-M026-11'),'笔记和标注':('E-EUD-03','F-M026-03'),'发音和跟读':('E-EUD-11','F-M026-08'),'全文搜索':('E-EUD-15','F-M026-12'),'单词学习':('E-EUD-06','F-M026-05'),'算法及设置':('E-EUD-09','F-M026-05'),'回顾练习':('E-EUD-07','F-M026-06'),'常见问题':('E-EUD-08','F-M026-04;F-M026-05;F-M026-07'),'解释页面设置':('E-EUD-16','F-M026-01;F-M026-03;F-M026-13'),'添加扩充词库':('E-EUD-10','F-M026-09'),'我的生词本':('E-EUD-05','F-M026-04'),'文件阅读':('E-EUD-02','F-M026-02'),'例句卡片':('E-EUD-04','F-M026-03'),'外接键盘快捷键':('E-EUD-17','F-M026-03;F-M026-04'),'API & URL Scheme':('E-EUD-18','F-M026-01'),'VIP 功能介绍':('E-EUD-19','')}
fresh={r['url'] for r in fetch.values() if r.get('status')==200}
inv=[]
for i,(title,u) in enumerate(toc,1):
 eid,fs=mapping.get(title,('E-EUD-13',''));status='rechecked_partial' if u in fresh and title in mapping else 'prior_partial' if title in mapping else 'directory_only';gap='见对应F缺口；正文不等于现版验证' if fs else '未完成任务；目录/权益不证明具体操作'
 if title=='API & URL Scheme':gap='公开Scheme表可读；链接API文档只有登录壳，未读取接口契约'
 inv.append(dict(item_id=f'M026-P{i:02}',kind='help_page',title=title,source_url=u,evidence_ids=eid,flow_ids=fs,status=status,gaps=gap,checked_on='2026-10-27'))
children=[('解释页面设置',['折叠显示词典','排序和隐藏词库','词典内容批注','目录/解释内搜索','字体主题','单击查词与两种返回'],'E-EUD-16','F-M026-13;F-M026-01;F-M026-03'),('跨软件取词',['iOS跨软件翻译窗口','iOS默认翻译App','iOS共享取词','iPad分屏取词','Android浮动图标取词','Android LightPeek复制取词'],'E-EUD-14','F-M026-11')]
for parent,labels,eid,fs in children:
 u=dict(toc)[parent]
 for label in labels:inv.append(dict(item_id=f'M026-S{len(inv)-21:02}',kind='help_section',title=parent+' / '+label,source_url=u,evidence_ids=eid,flow_ids=fs,status='rechecked_partial',gaps='已定位正常步骤；异常/恢复见案例，不算独立完成任务',checked_on='2026-10-27'))
with (lib/'batches/eudic-controls-deepening-inventory.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=list(inv[0]),lineterminator='\n');w.writeheader();w.writerows(inv)
old={r['flow_id']:r for r in json.loads((b/'existing-coverage-before.json').read_text())}
changes={
'F-M026-01':dict(evidence_ids='E-EUD-01;E-EUD-16;E-EUD-18',known_branches='通配符、删历史、iOS回调；上一词与返回搜索分开；context查询仅传参',gaps='触屏误识别校订、清历史撤销、回调失败与context持久保存未知',checked_on='2026-10-27'),
'F-M026-03':dict(evidence_ids='E-EUD-03;E-EUD-04;E-EUD-16;E-EUD-17',known_branches='编辑、循环间隔；iOS外接键盘新建笔记，保存退出与不保存退出分开',gaps='触屏保存/撤销、书籍原句关联、跨端编辑冲突未知',checked_on='2026-10-27'),
'F-M026-04':dict(evidence_ids='E-EUD-05;E-EUD-08;E-EUD-17',known_branches='分组、排序、自建词本；iOS外接键盘解释页Command+A加入生词本',gaps='触屏加词按钮、重复词、同步完成与冲突；默认分组在快捷键路径的表现未知',checked_on='2026-10-27'),
'F-M026-05':dict(evidence_ids='E-EUD-06;E-EUD-08;E-EUD-09;E-EUD-12',known_branches='错题回卡片、配置算法；旧帮助新词配额不含复习，完成后追加新词及提前未来任务条件',gaps='当前FSRS/各平台构建对应、跨日恢复、现版任务追加反馈未知',checked_on='2026-10-27'),
'F-M026-07':dict(gaps='旧Android周期本地备份的恢复入口、覆盖冲突及新版撤回调度细节未知；不同于云同步',checked_on='2026-10-27'),
'F-M026-10':dict(evidence_ids='E-EUD-12;E-EUCD-01;E-EUCD-02',known_branches='旧帮助限制修正；官网新日志分端摘要有撤回/导出线索，未取得完整正文',gaps='摘要时间不等于功能发布日期；对应构建、菜单、撤回/恢复范围仍缺',checked_on='2026-10-27')}
proposals=[]
for fid,x in changes.items():
 for k,v in x.items():proposals.append(dict(flow_id=fid,field=k,before=old[fid][k],after=v,reason='2026-10-27补证或修正原行过期缺口；稳定任务不改编号'))
with (b/'existing-coverage-updates.csv').open('w',newline='') as f:w=csv.DictWriter(f,fieldnames=['flow_id','field','before','after','reason'],lineterminator='\n');w.writeheader();w.writerows(proposals)
(b/'existing-coverage-updates.json').write_text(json.dumps(proposals,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'new_E':len(rows),'new_F':len(flows),'inventory_pages':len(toc),'inventory_rows':len(inv),'old_fields':len(proposals)},ensure_ascii=False))
