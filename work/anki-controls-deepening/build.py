from pathlib import Path
import json,csv,re,hashlib
root=Path('/home/workspace/reader'); lib=root/'docs/research/interaction-library'; work=root/'work/anki-controls-deepening'; batch=lib/'batches'; art=lib/'artifacts/anki-controls-deepening/source-notes.md'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
logs=json.loads((work/'fetch-log.json').read_text())+json.loads((work/'fetch-extra-log.json').read_text()); cap={x['name']:x for x in logs}
old={}
for p in [lib/'evidence.jsonl',*sorted(batch.glob('*evidence.jsonl'))]:
 for line in p.read_text().splitlines():
  if line.strip():
   r=json.loads(line)
   if r.get('product_id')=='M-020':old.setdefault(r['evidence_id'],r)
new_specs=[
('01','模板检查与错误恢复','errors.md','official_manual','开头2.1.45+；Basics；Specific Issues','Anki Desktop current manual; note-type checks since 2.1.45; installed build unknown',None,'保存/导出前阻止部分模板错误；定位类型并编辑；空卡清理前核清单。'),
('02','集合文件、检查和安全启动','files.md','official_manual','Checking Your Collection；User Data；Startup Options；Corrupt Collections','Anki Desktop current manual; selected sections only; legacy platform guidance separated',None,'重建标签不跨端同步；安全启动停自动同步/插件；损坏首选备份，未执行修复。'),
('03','2.1.45多步撤销/重做历史变更','changes-2.1.40-49.md','official_release_notes','Changes in 2.1.45 / Undo handling；2.1.46','Desktop 2.1.45 released 2021-07-30; historical mechanism not proof of current UI','2021-07-30','多数动作可多步撤销/重做；Check Database及不兼容插件会清历史；即时保存限支持新撤销的动作。'),
('04','v3调度撤销与暂埋边界','scheduler-2021.md','official_help','Enabling；Changes / Undo；Burying','Desktop v3 supported since 2.1.45; default and only option since23.10',None,'回答/暂埋交错可逐步撤销；跨日学习卡可暂埋；未证明可定点删除任意复习历史。'),
('05','缺失声音/图片的恢复分支','missing-media.md','official_help','全文：Check Media、回收站、共享包、相同文件名','Anki Desktop official FAQ; update date and installed build unknown',None,'查缺失引用后复原同名文件；共享包仍可得时可隔离取回附件；删除同步与客服恢复有条件。'),
('06','23.12自动推进与布局保存变更','release-23.12.html','official_release_notes','Deck options screen；Other：Offer to save changes when card layout screen is closed','Anki Desktop23.12 release; not current behavior verification','2023-12-25','原HTML明确发布日期；新增自动推进入口与布局关闭保存机会，无按钮视觉观察。'),
('07','自动推进失焦停止的维护者说明','issue-4228.html','official_maintainer_issue','dae首帖，2025-07-25；正文机制理由','Historical maintainer explanation; July2025 desktop build unspecified; current behavior unknown','2025-07-25','失焦停止为避免意外评分；变更请求不能视为已实现；未用后续用户意见证明官方行为。')]
new=[]
for num,title,name,typ,loc,scope,date,note in new_specs:
 c=cap[name]; new.append(dict(evidence_id='E-ANCD-'+num,product_id='M-020',title=title,url=c['url'],checked_on='2026-10-27',source_type=typ,access_mode='public_http_markdown_body_read' if name.endswith('.md') else 'public_http_html_body_read',version_scope=scope,app_build=None,firmware_version=None,source_updated_on=None,release_date=date if num in ["03","06"] else None,issue_created_on=date if num=="07" else None,locator=loc,note=note,hands_on=False,visual_observed=False,archived_artifact=str(art.relative_to(lib)),sha256=sha(art),capture_state='short_source_note_with_raw_capture',raw_work_path=c['raw_work_path'],raw_sha256=c['raw_sha256'],supersedes=None))
(batch/'anki-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in new))
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
with (batch/'anki-controls-deepening-coverage.csv').open('w',newline='') as f:csv.DictWriter(f,fields,lineterminator='\n').writeheader()
# Six recaptures of existing pages, preserving their stable evidence IDs.
adds=[]
for eid,name,loc in [('E-AND-01','llms.txt','Manual完整页面目录，含平台父子项'),('E-ANK-02','syncing.md','Conflicts；Merging Conflicts；Media'),('E-ANK-03','backups.md','Creating；Restoring；Deletion log'),('E-AND-12','deck-options.md','Auto Advance；Timers'),('E-AND-15','media.md','Checking Media；Manually Adding Media'),('E-AND-17','profiles.md','Profiles；Profiles window')]:
 c=cap[name]; before=old[eid].get('additional_captures')
 a=dict(capture_kind='public_http_markdown_body_read',checked_on='2026-10-27',network_refetched=True,source_url=old[eid]['url'],capture_url=c['url'],http_status=c['status'],locator=loc,raw_work_path=c['raw_work_path'],raw_sha256=c['raw_sha256'],archived_artifact=str(art.relative_to(lib)),sha256=sha(art),hands_on=False,visual_observed=False)
 adds.append(dict(evidence_id=eid,field='additional_captures',before=before,after=(before or [])+[a],reason='同页HTML/Markdown沿用旧E；补本轮正文采集和哈希，不重复计来源'))
(work/'additional-captures-updates.json').write_text(json.dumps(adds,ensure_ascii=False,indent=2)+'\n')
# Stable-flow field suggestions, never write old batches or masters.
rows={}; rowfile={}
for p in batch.glob('*coverage.csv'):
 if p.name.startswith('anki-controls-deepening'):continue
 for r in csv.DictReader(p.open(newline='')):
  if r.get('product_id')=='M-020':rows[r['flow_id']]=r;rowfile[r['flow_id']]=str(p.relative_to(root))
updates={
'01':dict(evidence_ids='E-ANK-01;E-ANCD-03;E-ANCD-04',known_branches='评分、到期队列、当日完成；v3允许回答/暂埋交错逐步撤销；多数可撤销动作可Redo；2.1.45历史条件',gaps='学习页Undo/Redo具体入口、撤销后题面、跨同步/重启保存、定点回改旧评分及插件影响未知'),
'02':dict(evidence_ids='E-ANK-01;E-ANCD-03;E-ANCD-04',known_branches='暂埋/停用/删除对象不同；v3跨日学习卡可暂埋；2.1.45列删除笔记/暂埋可多步撤销',gaps='删除确认、当前快捷键、撤销栈跨退出、每种批处理保存粒度；删除日志恢复见F03'),
'03':dict(evidence_ids='E-ANK-02;E-ANK-03;E-ANCD-02',known_branches='单向同步覆盖；合并前取消自动同步、逐顶层组导出apkg再下载/导回；媒体独立合并；自动备份不含媒体；deleted.txt按档案建立版本区分类型列',gaps='实际两端结果、失败中断/重复导回、旧档案日志迁移、笔记日志恢复调度/媒体未知；Network设置名需固定构建'),
'07':dict(evidence_ids='E-AND-06;E-AND-07;E-ANCD-01;E-ANCD-03;E-ANCD-06',known_branches='2.1.45+保存类型/导出前阻止部分错误；Browse定位类型→Cards修引用/条件；区分单张与全部cloze空卡；23.12关闭布局页提供保存机会',gaps='保存/放弃按钮文案、所有CSS/JS错误、删除重复类型的精确对象、取消/Undo后全部恢复范围未知'),
'10':dict(evidence_ids='E-AND-09;E-AND-10;E-ANCD-03',known_branches='卡片/笔记模式及批量对象不同；Edit/Undo入口；2.1.45多数动作多步Undo/Redo，Check Database/不兼容插件可清历史',gaps='全部批动作撤销支持、重做入口、跨窗口/同步/退出后的撤销栈、查找替换误操作恢复'),
'15':dict(evidence_ids='E-AND-12;E-ANCD-06;E-ANCD-07',known_branches='23.12+非零停留时间→More/Auto Advance；计时显示/记录和自动动作分开；2025-07-25维护者说明失焦停止的历史防误评机制',gaps='当前手动停止/重进/跨日、自动评分动作组合、等待音频与焦点恢复；未把历史议题当现版保证'),
'18':dict(evidence_ids='E-AND-15;E-ANCD-05;E-ANK-02',known_branches='检查未引用/缺失；模板静态附件须区别；缺失文件从未清空回收站或仍可得共享包复原；删媒体可能传播、全量同步不覆盖媒体方向',gaps='工具垃圾箱/OS回收站措辞冲突、保留期、清空后恢复、重名替换与断网结果；恢复云端文件取决于尚存'),
'20':dict(evidence_ids='E-AND-17;E-AND-16',known_branches='档案集合/设置分开、插件共享；同账户多档案可覆盖；A逐牌组导出再导入B合并；旧版打不开时回新版执行Downgrade后重试',gaps='档案删除确认/回收、导出选项/重名重复、降级可支持的版本及失败恢复；不当权限隔离保证'),
'21':dict(evidence_ids='E-AND-18;E-AND-19;E-ANCD-02;E-ANCD-03',known_branches='Shift安全启动同时禁插件与自动同步；Check Database查损坏/重建标签；闲置标签清理不随集合同步；检查会清撤销历史的2.1.45边界',gaps='固定构建/插件兼容、损坏修复实际结果、检测遗漏、所有故障保存粒度；未执行SQLite修复')}
suggestions=[]
for num,vals in updates.items():
 fid='F-M020-'+num; vals['checked_on']='2026-10-27'
 for field,after in vals.items():
  before=rows[fid][field]
  if before!=after:suggestions.append(dict(flow_id=fid,source_batch=rowfile[fid],field=field,before=before,after=after,reason='2026-10-27补机制/恢复分支，原任务与深度不变；待root合并'))
(work/'existing-coverage-updates.json').write_text(json.dumps(suggestions,ensure_ascii=False,indent=2)+'\n')
with (work/'existing-coverage-updates.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,['flow_id','source_batch','field','before','after','reason'],lineterminator='\n');w.writeheader();w.writerows(suggestions)
# Exact top-level Manual directory, including platform container/subpages, plus bounded support evidence.
url_e={r['url'].removesuffix('.md').rstrip('/'):eid for eid,r in old.items()};url_e.update({r['url'].removesuffix('.md').rstrip('/'):r['evidence_id'] for r in new})
flow_by_page={
'studying':'01;02','editing':'06;08','preferences':'22','deck-options':'13;14;15','syncing':'03;18','profiles':'20','browsing':'10;11','filtered-decks':'12','searching':'10','exporting':'19;20','backups':'03','files':'03;21','stats':'17','media':'18','leeches':'16','addons':'21','troubleshooting':'21','templates/intro':'07','templates/fields':'09','templates/generation':'07;08','templates/errors':'07','importing/intro':'04;05','importing/text-files':'04','importing/packaged-decks':'05'}
gaps={
'intro':'介绍不是操作；正文未读','getting-started':'卡片/笔记基础目录已定位，正文未逐项分析','background':'学习理论正文未读；不能当任务效果验证','getting-help':'求助目录未展开，未提交问题','studying':'学习页撤销入口、题面恢复、后台保存未知','editing':'字段属性与所有输入/取消组合未逐项分析','preferences':'设置全部组合/恢复默认未闭合','deck-options':'非全参数研究；自动停止/音频组合和版本冲突未闭合','syncing':'失败/断网和实际两端恢复未测试','profiles':'删除确认/回收及降级范围未知','browsing':'全部批操作Undo支持与错误恢复未逐项确认','filtered-decks':'重建/取消和历史统计组合未逐项核实','searching':'检索语法未逐操作符测试','exporting':'中断/媒体重名/全部设置恢复未知','backups':'旧档案deleted.txt升级和日志调度恢复未知','files':'已读正文但未执行文件移动/修复；历史平台指引不套当前','stats':'每张图完整交互/跨时区口径未查','media':'回收措辞冲突、清空后恢复/期限未知','math':'公式输入/渲染/错误正文未读','leeches':'调整阈值后的所有队列/失败恢复未知','addons':'插件逐一兼容/卸载残留未知','troubleshooting':'只读步骤，未运行诊断/修复','sync-server':'未读正文或部署服务器','misc':'已读菜单快捷键与调试控制台；不运行，不建F','contrib':'贡献流程正文未读，非当前用户任务','resources':'外部资源列表正文未读','templates/styling':'样式/HTML正文未读，未推断触屏显示','templates/errors':'仅部分错误链；精确删除范围/确认与恢复未知','templates/intro':'预览非实测，全部布局保存分支未知','templates/fields':'未逐项分析TTS/特殊字段；输入比较非自动评分','templates/generation':'复杂条件模板未验证；空卡清理恢复有限','importing/intro':'导入总目录，失败/取消未知','importing/text-files':'编码异常/部分导入/媒体冲突未知','importing/packaged-decks':'所有类型合并/失败回滚未知'}
# old fetch log maps URLs to raw files and does not imply all subheadings complete.
oldcaps={}
for r in json.loads((root/'work/anki-deepening/fetch-log.json').read_text()):
 # inspect both standard shape names used by earlier script
 u=r.get('url')
 if u:oldcaps[u]=r
rawmap={c['url']:(c['raw_work_path'],c['raw_sha256']) for c in logs if c.get('status')==200}
for eid,r in old.items():
 if r.get('raw_work_path') and (root/r['raw_work_path']).exists():rawmap.setdefault(r['url'],(r['raw_work_path'],sha(root/r['raw_work_path'])))
rawmap.setdefault('https://docs.ankiweb.net/manual/studying.md',('work/anki-deepening/studying.md',sha(root/'work/anki-deepening/studying.md')))
inv=[];section='Desktop Manual'
manual=cap['llms.txt'];txt=(work/'llms.txt').read_text().split('## AnkiMobile')[0]
for line in txt.splitlines():
 if line.startswith('###'):section=line.lstrip('# ').strip()
 m=re.match(r'- \[([^]]+)\]\(([^)]+)\)',line)
 if not m:continue
 title,url=m.groups();slug=url.split('/manual/')[1].removesuffix('.md');eid=url_e.get(url.removesuffix('.md').rstrip('/'),''); nums=flow_by_page.get(slug,''); flows=';'.join('F-M020-'+x for x in nums.split(';') if x)
 raw=rawmap.get(url);status='body_read_partial' if raw else 'directory_only'
 if slug=='misc':raw=rawmap[url];status='body_read_no_task'
 gap=gaps.get(slug,'平台页面仅目录定位，安装/显示/输入/故障正文未读；不代表不支持')
 inv.append(dict(item_id=f'M020-P{len(inv)+1:02}',kind='manual_page',section=section,title=title,source_url=url,evidence_ids=eid or 'E-AND-01',flow_ids=flows,status=status,gaps=gap,raw_work_path=raw[0] if raw else '',raw_sha256=raw[1] if raw else '',checked_on='2026-10-27'))
for r in new:
 if '/manual/' in r['url']:continue
 inv.append(dict(item_id=f'M020-S{len([i for i in inv if i["kind"]=="supplementary_page"])+1:02}',kind='supplementary_page',section=r['source_type'],title=r['title'],source_url=r['url'],evidence_ids=r['evidence_id'],flow_ids={'E-ANCD-03':'F-M020-01;F-M020-02;F-M020-07;F-M020-10;F-M020-21','E-ANCD-04':'F-M020-01;F-M020-02','E-ANCD-05':'F-M020-18','E-ANCD-06':'F-M020-07;F-M020-15','E-ANCD-07':'F-M020-15'}[r['evidence_id']],status='body_read_partial',gaps=r['version_scope'],raw_work_path=r['raw_work_path'],raw_sha256=r['raw_sha256'],checked_on='2026-10-27'))
with (batch/'anki-controls-deepening-inventory.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,list(inv[0]),lineterminator='\n');w.writeheader();w.writerows(inv)
print('new_evidence',len(new),'new_flows',0,'suggested_fields',len(suggestions),'old_flows',len(updates),'additional_captures',len(adds),'manual_pages',sum(x['kind']=='manual_page' for x in inv),'all_inventory',len(inv),'manual_body_available',sum(x['kind']=='manual_page' and x['status']!='directory_only' for x in inv))
