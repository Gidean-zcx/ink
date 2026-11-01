import csv, hashlib, json
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch49-integration';S=R/'work/research/batch49-antennapod-state';A=L/'artifacts/antennapod-state-controls'
C='175e5ef8fe3e125a5014d70ef0c57cf85661c71b';DATE='2026-10-27';N='artifacts/antennapod-state-controls/source-notes.md'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def readcsv(p):return list(csv.DictReader(p.open()))
def writecsv(p,rs):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rs[0]),lineterminator='\n');w.writeheader();w.writerows(rs)
assert not (L/'batches/antennapod-state-controls-evidence.jsonl').exists()
specs=[
('EpisodeItemListAdapter.java','单集长按菜单与多选','80–135;185–230：长按、点击、多选入口及选中顺序'),
('AllEpisodesFragment.java','全部单集筛选与回读','31–132：入口、偏好、收藏范围、分页和空结果'),
('FeedItemlistFragment.java','单播客筛选与重载','390–397;510–540;576–581;658–690：入口资格、事件重载及错误路径'),
('TagSettingsDialog.java','标签编辑与共同标签','44–160：临时列表、取消/OK、共同集合与逐项提交'),
('LocalDeleteModal.java','本地文件删除警告','11–34：选择含本地源时警告、确认/取消'),
('FeedPreferences.java','标签数据表达','16–18;305–311：保留标签、集合及序列化'),
('PodDBAdapter.java','队列/标签/过滤的写入与查询','493–525;891–913;1011–1025;1119–1148：数据库对象及事务/异常边界'),
('ConfirmationDialog.java','通用确认与取消','23–52：确认回调、取消及取消监听'),
('AllEpisodesFilterDialog.java','全局筛选变化事件','10–32：选项变化事件，不另等Confirm'),
('FeedItemFilterDialog.java','单播客过滤写入入口','10–27：feed ID和每次选择写入请求'),
('EpisodesListFragment.java','列表生命周期与加载结果','84–107;200–205;388–426：事件注册、空列表、重载与错误'),
('FeedItemFilter.java','过滤对象与范围标记','30–101：状态常量、条件构造和读取，不涵盖全部matches实现'),
('DBReader.java','单集与单播客结果回读','129–148;223–264;330–355：读取条件、计数及单源状态范围'),
('FeedMultiSelectActionHandler.java','订阅批量标签入口','34–68;152–159：空选返回、标签操作分派及偏好集合')]
fetch={Path(x['path']).name:x for x in json.loads((S/'fetch-audit.json').read_text())};assert len(fetch)==14
new=[]
for i,(name,title,loc) in enumerate(specs,1):
 x=dict(fetch[name]);assert x['status']==200 and sha(R/x['path'])==x['sha256'];x.update(evidence_id=f'E-APST-{i:02d}',title=title,locator=loc);new.append(x)
oldroot=R/'work/antennapod-controls-deepening/src'
def oldfile(name,locator,queue=False):
 p=R/'work/antennapod-queue-controls-deepening/raw'/name if queue else next(oldroot.rglob(name))
 return {'raw_work_path':str(p.relative_to(R)),'raw_sha256':sha(p),'locator':locator}
old={
 'E-APCD-03':[oldfile('UserPreferences.java','885–892：全局过滤SharedPreferences读取/写入'),oldfile('strings.xml','314/417/440/826：本地删除、清队列、空筛选、共同标签文案')],
 'E-APCD-13':[oldfile('FeedMenuHandler.java','59–61：单订阅标签入口'),oldfile('SubscriptionFragment.java','192–229：批量标签入口及已有标签浏览')],
 'E-APCD-14':[oldfile('DBWriter.java','55–167;455–522;569–631;812–821;920–930：异步请求、媒体删除、队列、偏好与过滤写入')],
 'E-APQD-04':[oldfile('QueueFragment.java','137–172;299–312;438–487：队列事件、清空确认、多选结束',True)],
 'E-APQD-05':[oldfile('EpisodeMultiSelectActionHandler.java','39–56;152–169;212–231：本地删除警告分派、数量及移动',True)],
 'E-APQD-06':[oldfile('ItemFilterDialog.java','36–77;101–122：变化、确认、Reset；与新调用方连接',True)],
 'E-APQD-07':[oldfile('FeedItemFilterQuery.java','23–103：AND条件、状态回退及字段含义',True)]}
audit={'checked_on':DATE,'commit':C,'source_build':'3.12.3, versionCode 3120395; no installed APK','new_sources':new,'reused_sources':old,'method':'targeted static read; raw Git blob identities checked; no build/install/product execution or visual observation','source_index_path':'work/antennapod-controls-deepening/tree.json'}
dump(A/'source-audit.json',audit)
E=[]
for x in new:
 E.append(dict(evidence_id=x['evidence_id'],product_id='M-040',title='3.12.3 '+x['title'],url=x['source_url'],checked_on=DATE,source_type='official_source_code',access_mode='ordinary_HTTP_200_fixed_commit_and_targeted_static_read',version_scope='Android AntennaPod 3.12.3 source at '+C+'; not an executed APK',app_build='3.12.3 (versionCode 3120395); source only',firmware_version=None,source_updated_on=None,locator=x['locator'],note='定点静态源码；读到写入/事件不等于实际成功，相关范围与缺口见摘记。',hands_on=False,visual_observed=False,archived_artifact=N,sha256=sha(L/N),capture_state='source_notes_and_fetch_audit_archived',supersedes=None,raw_work_path=x['path'],raw_sha256=x['sha256'],verified_git_blob=x['git_blob_sha'],commit=C,artifact_hashes={'artifacts/antennapod-state-controls/source-audit.json':sha(A/'source-audit.json')}))
(L/'batches/antennapod-state-controls-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in E))
changes=[]
for eid,files in old.items():
 owners=[]
 for p in (L/'batches').glob('*-evidence.jsonl'):
  rows=[json.loads(l) for l in p.read_text().splitlines()]
  if any(r['evidence_id']==eid for r in rows):owners.append((p,rows))
 assert len(owners)==1
 p,rows=owners[0];row=next(r for r in rows if r['evidence_id']==eid);before=json.loads(json.dumps(row));row.setdefault('additional_captures',[]).append(dict(checked_on=DATE,access_mode='local_pinned_source_reread_no_refetch',source_files=files,locator='第49批：连接确认/取消与保存范围，见逐文件locator',archived_artifact=N,sha256=sha(L/N),hands_on=False,visual_observed=False))
 p.write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows));changes.append({'owner':str(p.relative_to(R)),'before':before,'after':row})
dump(W/'old-evidence-updates.json',changes)
flow_specs={
 'F-M040-02':('E-APST-01','长按上下文菜单后选Multi-select；选择模式点击切换，选中按列表顺序取出；原入队资格仍保留','多选旋转/数据更新后恢复、并发重复入队、DB错误与实际反馈未测'),
 'F-M040-03':('E-APST-07;E-APST-08','确认清空只删队列表，事件清UI；移顶先逆序插入保持传入顺序；setQueue事务捕获SQL异常仅日志，事件非落盘凭据','当前播放服务对清队列事件的全部反应、撤回、故障注入/回读、并发顺序与实机拖动未测'),
 'F-M040-05':('E-APST-05;E-APST-07','任一所选本地源触发警告，取消不调用整组删除；无本地源直接执行；数量是异步请求计数，物理失败与状态改变分开','实际取消/删除失败反馈、媒体残留、取消下载半文件、本地文件恢复及批量回滚未测'),
 'F-M040-13':('E-APST-04;E-APST-06;E-APST-07;E-APST-14','单/多订阅标签临时编辑，OK逐节目写入；批量仅改共同标签，保留独有标签；空/重复名不新增但不阻止OK提交；保留原last_tag浏览恢复条件','长按标签全局管理、并行写入/部分失败、跨端同步、删除恢复及异常终止保存未测')}
changes=[]
for fid,(eids,known,gaps) in flow_specs.items():
 owners=[]
 for p in (L/'batches').glob('*-coverage.csv'):
  rows=readcsv(p)
  if any(r['flow_id']==fid for r in rows):owners.append((p,rows))
 assert len(owners)==1
 p,rows=owners[0];r=next(r for r in rows if r['flow_id']==fid);before=dict(r)
 r['evidence_ids']=';'.join(dict.fromkeys((r['evidence_ids']+';'+eids+';E-APCD-14').split(';')))
 r.update(known_branches=known,gaps=gaps,checked_on=DATE)
 if fid.endswith('-13'):r['task']='整理订阅标签、按标签浏览并恢复选择'
 writecsv(p,rows);changes.append({'owner':str(p.relative_to(R)),'before':before,'after':dict(r)})
dump(W/'old-flow-updates.json',changes)
f=dict(changes[0]['after']);f.update(flow_id='F-M040-14',task='筛选全局或单播客单集并恢复可见范围',depth='D2',evidence_ids='E-APST-02;E-APST-03;E-APST-07;E-APST-09;E-APST-10;E-APST-11;E-APST-12;E-APST-13;E-APQD-06;E-APQD-07;E-APCD-03;E-APCD-14',known_branches='全局事件即存偏好并回第一页；单源即请求写feed过滤字段并事件重载；Confirm只关闭；Reset空集；全局空条件回已订阅、收藏扩全部状态；单源仍限feed',gaps='系统落盘、并发改变、进程中断/重装恢复、真实空结果/查询错误反馈未测；订阅筛选是另一未读控件',visual_status='not_observed',hands_on='false',checked_on=DATE)
writecsv(L/'batches/antennapod-state-controls-coverage.csv',[f])

ip=L/'batches/antennapod-queue-controls-deepening-inventory.csv';inv=readcsv(ip);assert len(inv)==158;invbefore=json.loads(json.dumps(inv))
inv_updates={
 'C-02':('F-M040-03','E-APST-07;E-APST-08','确认/取消链已接数据库队列表和CLEARED事件；无媒体删除调用','实机播放反应、撤回与写失败未测'),
 'C-03':('F-M040-03','E-APST-07','setQueue事务/SQL异常及调用后事件已读','实际回滚、并发及磁盘成功回读未测'),
 'C-04':('F-M040-03','E-APST-07','DBWriter移动及队列持久化调用已读','手势取消、失败UI与当前播放反应未测'),
 'C-07':('F-M040-02;F-M040-05','E-APST-01','单集adapter长按菜单→Multi-select，点击切选；后续沿原分派','旋转与数据变化中的选择保留未测'),
 'C-10':('F-M040-05','E-APST-05','已读本地源警告、确认/取消及无本地源直执行','物理删除/取消下载失败、残留与恢复未测'),
 'C-15':('F-M040-03','E-APST-07','移动到两端的顺序及DB写入已读；分享仍只传首项','外部分享与事务实际失败未测'),
 'C-16':('F-M040-02;F-M040-04;F-M040-05','E-APST-05;E-APST-07','确认数量来自调用方计数，并非异步完成回执','逐项完成、失败与回读未测'),
 'C-17':('F-M040-14','E-APST-02;E-APST-09;E-APST-10','补两调用方：全局偏好/单源DB即时请求，Confirm关闭','其他控件、真实落盘与重启未测'),
 'C-18':('F-M040-14','E-APST-07;E-APST-12;E-APST-13','查询和调用方范围已连接；字段不等于文件/实时状态','运行时查询/文件检查未测'),
 'C-19':('F-M040-14','E-APST-02;E-APST-13','全局空条件回已订阅、收藏扩全状态；单源限feed但允许全部源状态','实际零结果、删除/归档并发未知'),
 'C-20':('F-M040-13','E-APST-04;E-APST-14','补标签编辑入口/共同标签，原已有标签浏览保留','全局标签管理、同步和异常终止未测'),
 'H-47':('F-M040-13','E-APST-04','帮助入口现有固定源码补充，未把标签当独立队列','跨端/错误恢复未知')}
for r in inv:
 if r['inventory_id'] in inv_updates:
  fs,es,detail,gap=inv_updates[r['inventory_id']];r.update(flow_ids=fs,evidence_ids=';'.join(dict.fromkeys((r['evidence_ids']+';'+es).split(';'))),current_detail=detail,gap=gap,checked_on=DATE)
for n,title,fs,es,detail in [
 (28,'全局单集筛选入口和写入','F-M040-14','E-APST-02;E-APST-09;E-APST-11','选择即写偏好/重载；空结果提示，Confirm只关闭'),
 (29,'单播客过滤入口和数据库范围','F-M040-14','E-APST-03;E-APST-07;E-APST-10;E-APST-13','单源入口资格、feed字段和事件回读'),
 (30,'标签临时编辑及共同集合保存','F-M040-13','E-APST-04;E-APST-06;E-APST-14','OK前临时列表；取消无提交；共同标签替换、独有保留'),
 (31,'单集上下文菜单进入多选','F-M040-02;F-M040-05','E-APST-01','长按先菜单，Multi-select才启动；列表顺序取选中项')]:
  e=next(x for x in E if x['evidence_id']==es.split(';')[0]);inv.append(dict(inventory_id=f'C-{n:02d}',kind='targeted_source_branch',domain='State and commit',item=title,source_locator=e['locator'],source_url=e['url'],version_scope=e['version_scope'],coverage='static_branch_read_partial_not_whole_product',flow_ids=fs,evidence_ids=es,gap='无设备运行；错误/并发/持久恢复见关联任务',read_basis='targeted_pinned_source_read',checked_on=DATE,current_detail=detail))
writecsv(ip,inv);dump(W/'inventory-updates.json',{'before_count':158,'after_count':162,'updated_ids':list(inv_updates),'new_ids':['C-28','C-29','C-30','C-31'],'before_rows':[r for r in invbefore if r['inventory_id'] in inv_updates]})

rp=readcsv(L/'reference-points.csv');assert not any(x['product_id']=='M-040' for x in rp)
questions=[
('筛选;确认;取消;AI','F-M040-14','E-APST-02;E-APST-09;E-APQD-06','全部单集页面','改变筛选后关闭','变化即写偏好，Confirm仅关闭','实际落盘/重启未测；关闭没有补偿回调','不是其他筛选控件的通用规则'),
('筛选;存储;备份','F-M040-14','E-APST-03;E-APST-07;E-APST-10','可访问单播客过滤入口','修改单播客条件','写该feed字段，事件重载列表','与全局SharedPreferences不同；恢复未测','不要将全局和单源设置合并成一个备份对象'),
('标签;批量;确认;取消','F-M040-13','E-APST-04;E-APST-06;E-APST-14','多个订阅具有共同与独有标签','编辑共同标签并确定','保留独有标签，逐节目提交；取消无提交回调','未形成整批事务保证，实际落盘/并发未测','空输入/重复名不阻止确定提交已有列表'),
('删除;本地文件;取消','F-M040-05','E-APST-05;E-APQD-05;E-APCD-03','所选含本地源或仅普通下载','触发删除','含本地源先警告/确认；无本地源直执行','确认后才调用批量，实际删除仍未知','警告针对整组选中项；不能承诺本地原件可重下载'),
('结果;部分失败;AI','F-M040-05','E-APQD-05;E-APCD-14;E-APST-07','已有下载/本地媒体或进行中下载','批量删除/取消下载','提示数由调用方累加，不等待各项Future','物理失败可只记日志而继续状态路径','数量提示不能证明删净或恢复成功'),
('队列;删除范围;确认','F-M040-03','E-APST-07;E-APST-08;E-APQD-04;E-APCD-14','队列有项目','确认清空队列','清队列表并发事件；分支不调用媒体删除/STOP','数据库调用已追到，实际播放反应/错误未测','没有取得撤回旧队列链，不承诺一定续播'),
('批量;顺序;事务;执行结果','F-M040-03','E-APST-07;E-APCD-14;E-APQD-05','传入选中列表','移到顶端/底端','保留传入顺序；事务保存后有UI事件，SQL异常可仅日志','事件不构成写库成功证明；未故障注入','需回读真实队列，不从请求数或动画推成功'),
('空结果;来源范围;收藏','F-M040-14','E-APST-02;E-APST-12;E-APST-13;E-APQD-07','全局单集列表','清筛选或选收藏','空条件回已订阅；收藏扩源状态范围','按查询字段解释；实际结果未运行','全部/已下载/暂停等标签不等于全库/文件存在/实时播放')]
for i,(tags,fs,es,pre,action,result,persist,unknown) in enumerate(questions,1):
 rp.append(dict(reference_id=f'RP-M040-{i:02d}',product_id='M-040',question_tags=tags,flow_ids=fs,evidence_ids=es,source_locator='source-audit.json中相应文件/函数/行范围',artifact_path=N,precondition=pre,action=action,visible_result='静态源码描述：'+result,evidence_basis='pinned_source_static_read_not_executed',persistence_result=persist,unknown_branches=unknown,version_scope='AntennaPod Android 3.12.3 source '+C+'; no APK/device execution',checked_on=DATE))
writecsv(L/'reference-points.csv',rp)
dump(W/'integration-result.json',{'new_flows':1,'new_depth':'D2','updated_flows':4,'new_evidence':14,'existing_evidence_captures':7,'new_reference_points':8,'inventory_rows':162,'inventory_updated':12,'inventory_added':4,'new_visual_or_operation':False,'requirements_sha256':sha(R/'docs/requirements.md')})
print('Integrated 1 new D2, 4 prior tasks, 14 pinned sources, 7 source rereads, 8 reference points; no product execution.')
