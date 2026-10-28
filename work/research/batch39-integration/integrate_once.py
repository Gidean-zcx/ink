import csv
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path('/home/workspace/reader')
LIB = ROOT / 'docs/research/interaction-library'
B = LIB / 'batches'
RAW = ROOT / 'work/research/batch39-viwoods'
OUT = LIB / 'artifacts/viwoods-chinese-workflows'
DATE = '2026-10-27'
URL = 'https://cdn.shopify.com/s/files/1/0729/7368/0674/files/User_Manual_CN-0305.pdf?v=1789609108'
EID = 'E-VWCN-01'
scope = 'AiPaper中文历史手册；封面修订2025-06-11；未标整机固件，不视为V3.15实测'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p, obj): p.write_text(json.dumps(obj, ensure_ascii=False, indent=2)+'\n')
def csvwrite(p, rows):
    with p.open('w', newline='') as f:
        w=csv.DictWriter(f, fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)

assert sha(ROOT/'docs/requirements.md') == 'e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
assert not (B/'viwoods-chinese-workflows-evidence.jsonl').exists(), 'already integrated'
assert sha(RAW/'manual-cn.pdf') == '7ca80582c8397a42263774528e8dabcd1356076fb76997e6600702db79ca3dc5'
observed = [1,2,13,23,26,27,40,41,42,43,44,45,51,52,55,57,58,59,60,63,64,65,66,67,69,70,71,73,74]
archive = [1,2,23,26,27,42,43,52,55,57,58,60,63,64,65,66,67,71,73]
for n in archive: shutil.copy2(RAW/f'cn-{n:02}.png',OUT/f'cn-{n:02}.png')
pages = json.loads((RAW/'manual-cn-pages.json').read_text())
assert len(pages)==75
audit = {'evidence_id':EID,'checked_on':DATE,'source_url':URL,'source_revision':'2025-06-11',
 'pdf_metadata_creation_modification':'2026-09-17T01:30:05Z; not revision or firmware',
 'source_pdf_sha256':sha(RAW/'manual-cn.pdf'),'text_read_file_pages':list(range(1,76)),
 'visual_observed_file_pages':observed,'archived_file_pages':archive,'hands_on':False,
 'visual_kind':'selected_static_manual_pages','page_numbering':'one-based PDF file pages',
 'note':'All 75 extracted text pages read; only listed 29 images viewed. 19 representative images archived; no continuous video, account, mail, sync or device operations.',
 'observations':[{'file_page':n,'raw_work_path':str((RAW/f'cn-{n:02}.png').relative_to(ROOT)),
   'raw_sha256':sha(RAW/f'cn-{n:02}.png'),'archived_artifact':f'artifacts/viwoods-chinese-workflows/cn-{n:02}.png' if n in archive else None} for n in observed]}
dump(OUT/'observed-pages.json',audit)
chapters={1:'封面',2:'目录',3:'快速入门',16:'WLAN Transfer',17:'功能介绍',18:'输入法',19:'Paper',29:'Learning',37:'Meeting',40:'Daily',46:'Picking',51:'文件加密',52:'Apps',53:'AI',56:'Viwoods Files APK',60:'第三方云集成',61:'Mailbox',68:'ViTransfer',72:'Share',73:'Memo',75:'联系页'}
page_flows={23:'19',26:'18',27:'18',40:'12;14',41:'14',42:'14',43:'14',44:'15',45:'07',51:'11',52:'13',55:'03;04',57:'16',58:'16',59:'16;17',60:'17',61:'21',62:'21',63:'21',64:'21',65:'22',66:'22',67:'22',68:'10',69:'10',70:'10',71:'10',73:'23',74:'20;23'}
rows=[]
for p in pages:
 n=p['file_page'];chapter=chapters[max(k for k in chapters if k<=n)]
 header=' / '.join([s.strip() for s in p['text'].splitlines() if s.strip()][:2])[:140]
 ids=';'.join('F-M007-'+x for x in page_flows.get(n,'').split(';') if x)
 rows.append({'entry_id':f'I-VWCN-{n:03}','product_id':'M-007','source_evidence_id':EID,'source_file_page':n,
  'chapter':chapter,'page_heading':header,'text_status':'read','visual_status':'static_page_observed' if n in observed else 'not_observed',
  'flow_ids':ids,'coverage_status':'selected_branches_documented' if ids else 'text_read_not_full_task_analysis',
  'artifact_path':f'artifacts/viwoods-chinese-workflows/cn-{n:02}.png' if n in archive else '',
  'scope_note':'页台账不是功能数；未映射F不表示不支持；静态图非操作；'+scope,'checked_on':DATE})
csvwrite(B/'viwoods-chinese-workflows-inventory.csv',rows)
ev={'evidence_id':EID,'product_id':'M-007','title':'AiPaper中文历史手册：AI事件、邮件预览与第三方云周期',
 'url':URL,'checked_on':DATE,'source_type':'official_manual','access_mode':'anonymous_public_http200_pdf_text_read_and_selected_static_pages_viewed',
 'version_scope':scope+'；官网同时供Mini链接，但本案例不扩展其硬件能力','app_build':None,'firmware_version':None,
 'source_updated_on':'2025-06-11','locator':'75文件页；AI日历42–43、Files57–60、邮箱61–67、ViTransfer68–71、Memo73–74',
 'note':'封面修订日期与2026-09-17 PDF元数据分开；邮箱图显示1.9.74仅为应用版本。75页文本、29页图实际查看；19摘页归档；没有实操。',
 'hands_on':False,'visual_observed':True,'visual_kind':'selected_static_manual_pages',
 'archived_artifact':'artifacts/viwoods-chinese-workflows/source-notes.md','sha256':sha(OUT/'source-notes.md'),
 'capture_state':'short_notes_selected_page_images_and_visual_audit_archived','supersedes':None,
 'raw_work_path':str((RAW/'manual-cn.pdf').relative_to(ROOT)),'raw_sha256':sha(RAW/'manual-cn.pdf'),
 'raw_bytes':(RAW/'manual-cn.pdf').stat().st_size,'http_status':200,
 'visual_audit_path':'artifacts/viwoods-chinese-workflows/observed-pages.json','visual_audit_sha256':sha(OUT/'observed-pages.json'),
 'artifact_hashes':{str(p.relative_to(LIB)):sha(p) for p in sorted(OUT.iterdir()) if p.is_file()}}
(B/'viwoods-chinese-workflows-evidence.jsonl').write_text(json.dumps(ev,ensure_ascii=False)+'\n')

# Append reread provenance to original E records, retaining their first acquisition.
captures={
 'E-VIW-11':{'access_mode':'official_manual_index_opened_with_web_tool','locator':'AiPaper 中文链接；同URL亦列Mini，Reader另链接；页区AiPaper至Reader C',
  'note':'官方入口复核；不声称下载新版英文。中文PDF单列E-VWCN-01。','archived_artifact':'artifacts/viwoods-chinese-workflows/source-notes.md'},
 'E-SHCD-03':{'access_mode':'local_existing_extracted_text_reread','locator':'review.txt 231–235、247–251；排序对象/歌单搜索/格式化入口',
  'raw_work_path':'work/shanling-controls-deepening/extracted/review.txt','note':'仅旧原件复读，无新HTTP或图像；作者自报V2.1。','archived_artifact':'artifacts/audio-source-recheck/source-notes.md'},
 'E-AK-10':{'access_mode':'local_existing_extracted_text_reread','locator':'M3U支持与路径不匹配段；未补导入过程',
  'raw_work_path':'work/ak-directory-deepening/raw/m3u.txt','note':'无新网络请求；不以内部歌单或媒体扫描补M3U导入。','archived_artifact':'artifacts/audio-source-recheck/source-notes.md'},
 'E-AKCD-01':{'access_mode':'local_existing_manual_extraction_reread','locator':'印刷38–40/文件39–41；印刷57/文件58',
  'raw_work_path':'work/ak-directory-deepening/manual-pages.json','note':'文字复读，无新图像；内部歌单与数据库重扫不能证明M3U导入。','archived_artifact':'artifacts/audio-source-recheck/source-notes.md'}}
found=set()
for p in B.glob('*-evidence.jsonl'):
 data=[json.loads(l) for l in p.read_text().splitlines() if l.strip()];changed=False
 for e in data:
  if e['evidence_id'] in captures:
   c=dict(captures[e['evidence_id']]);c.update(checked_on=DATE,hands_on=False,visual_observed=False,url=e['url'])
   c['sha256']=sha(LIB/c['archived_artifact'])
   if c.get('raw_work_path'): c['raw_sha256']=sha(ROOT/c['raw_work_path'])
   e.setdefault('additional_captures',[]).append(c);found.add(e['evidence_id']);changed=True
 if changed:p.write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in data))
assert found==set(captures),found

updates={
 'F-M007-03':dict(known_branches='历史自定义命令名/指令→Confirm；长按删除。中文2025-06-11修订p55仍有正文5条/截图4条冲突，不定统一上限。'),
 'F-M007-10':dict(task='ViTransfer收发、跨设备传递与局域网入口',known_branches='中文历史p69–71：网页Import/手机+→设备Receive；设备Export选文件→Send；A导出→同账号网页选文件和目标B→Sync→B Receive。中文用my.viwoods.com；保留旧英文图mytest差异。独立WLAN入口需同局域网。',gaps='重名/断线/取消/满盘、收发完成与文件完整性、当前构建未知；人工传递不证明双向合并/历史备份。'),
 'F-M007-13':dict(known_branches='官网网络/Google服务故障恢复；旧英文和中文p52均有App长按清理/卸载/添加主页，正文清缓存与截图Clear Data仍冲突，未执行。'),
 'F-M007-14':dict(task='手动、语音或圈选创建并确认Daily日历事件',known_branches='旧手动创建/编辑保持；中文p42长按新建加号→语音→可编辑标题/全天/起止/备注→Confirm→事件示意；p43套索选区→AI创建事件走同一预览。Cancel可见未执行；事件不是手写待办。',gaps='多日期/相对时间消歧、识别错误修改结果、取消/重复提交、时区/冲突、真实写盘/同步/提醒/删除恢复未知；不是V3.15实测。'),
 'F-M007-16':dict(known_branches='中文历史p57 My Notes长选Export→PDF/note和目标→确认；p58 My Files长选Import→My Notes/CloudDrive目标，标题仍写exported而按钮Import；p59云文件Import→Learning目录。note仅AiPaper且各应用格式不互通。旧英文本地目录开发中与该中文图分开。'),
 'F-M007-17':dict(task='设置第三方云的同步账号、周期、对象与格式',known_branches='中文p60：首页短按同步、长按云界面→Settings；账号/AutoSync/每7天、3天、24小时/六应用目录/PDF与note。正文note默认不可取消、PDF可选；画面选7天不证明默认。与V3.15原生云分开。',gaps='周期计时起点、休眠/断网补跑、进度/失败/取消、删除传播、云满/账号切换、历史版本恢复未知；设置开关不证明成功备份。'),
 'F-M007-18':dict(known_branches='旧编辑/撤销/返回入口保留；中文p26圈选笔迹只在同应用笔记间复制，p27文字复制可跨模块粘贴，空剪贴板粘贴入口灰。两指返回位于撤销节，不作页面导航。'),
 'F-M007-19':dict(known_branches='中文p23：新快捷入口使用既有普通目录名可建立关联；删除快捷入口使目录普通化，删关联目录移除快捷入口。旧模板名称/Done步骤保持。',gaps='多个同名目录的冲突、目录内容去向、撤销/当前数量限制未知；关联规则不等于已证明删除无损。'),
 'F-M063-07':dict(known_branches='既有V2.1作者报告：Artist页人形入口切Artist/Album Artist，所选艺术家下专辑按字母排列；System Settings文字列格式化卡。没有格式化确认与结果。',gaps='当前构建、其他排序字段/保持、格式化文件系统/确认/取消/恢复；复合任务仍D1，不把格式化当刷新曲库。'),
 'F-M063-10':dict(known_branches='作者自报V2.1：A:\\音频引用、_explaylist_data及Import playlist；歌单按创建顺序，M3U导入批内按字母、后添在末尾。称可导出再导入变更次序但未操作；歌单内可搜曲/专辑/艺术家，歌单名搜索缺失为作者报告。'),
 'F-M065-09':dict(known_branches='共用FAQ说明M3U支持与Android路径不匹配；复读SR35印刷38–40内部歌单管理、57媒体数据库重扫，均没有外部M3U导入过程，仍D1。')}
found=set()
for p in B.glob('*-coverage.csv'):
 with p.open(newline='') as f:data=list(csv.DictReader(f))
 changed=False
 for r in data:
  fid=r['flow_id']
  if fid in updates:
   r.update(updates[fid]);r['checked_on']=DATE
   extra=[EID] if fid.startswith('F-M007') else (['E-SHCD-03'] if fid=='F-M063-07' else (['E-AKCD-01'] if fid=='F-M065-09' else []))
   r['evidence_ids']=';'.join(dict.fromkeys(r['evidence_ids'].split(';')+extra))
   found.add(fid);changed=True
 if changed:csvwrite(p,data)
assert found==set(updates),found
new=[]
for n,task,known,gaps in [
 (21,'设置邮箱账号并切换/筛选邮件','中文历史61–64：Google/Outlook或手动IMAP/POP3→SMTP；账号下拉/最多5个；文件夹/日期过滤/翻页，设置20/60/100/200/1000为单文件夹显示数。','实际授权/同步/搜索、清缓存与删账号范围、重新登录恢复、当前构建未知；截图1.9.74是邮箱应用号。'),
 (22,'手写或文本写邮件，经AI预览后回到编辑器','中文65–67：新建填写To/CC/BCC/主题、模式/附件；手写→AI生成→独立预览→Copy to the mailbox→编辑器→另有Send。签名可从写信/设置入口编辑。','AI前已填收件地址；不证明自动选收件人。取消/替换范围、草稿持久化、签名保存、发送失败与送达回执未知。'),
 (23,'为Memo便笺设置提醒','中文73–74：打开便笺→时钟→提醒开关/日期/时刻→Cancel或Confirm；长选工具栏另有提醒入口。','保存结果、到点/锁屏/后台/离线、删除和恢复后的提醒未知；图中确认不等于实机已触发。')]:
 new.append(dict(flow_id=f'F-M007-{n}',product_id='M-007',task=task,depth='D2',evidence_ids=EID,
  known_branches=known,gaps=gaps,visual_status='static_manual_pages_only',hands_on='false',case_path='cases/M-007-viwoods.md',checked_on=DATE))
csvwrite(B/'viwoods-chinese-workflows-coverage.csv',new)

# A small cross-product retrieval layer, explicitly partial; no new F/E counts.
refs=[]
def ref(n,tag,flows,page,before,action,result,unknown,persist='未观察持久保存'):
 refs.append(dict(reference_id=f'RP-M007-{n:02}',product_id='M-007',question_tags=tag,
  flow_ids=flows,evidence_ids=EID,source_locator=f'中文历史手册PDF文件{page}页',
  artifact_path=f'artifacts/viwoods-chinese-workflows/cn-{page:02}.png',
  precondition=before,action=action,visible_result=result,evidence_basis='manual_diagram_static',
  persistence_result=persist,unknown_branches=unknown,version_scope=scope,checked_on=DATE))
ref(1,'AI;日程;预览;确认','F-M007-14',42,'Daily日历页','长按加号→说事件→松开','编辑页展示标题/全天/起止/备注，确认后日历事件示意','识别失败;相对日期;取消;真实写盘;同步')
ref(2,'AI;选区;日程;消歧','F-M007-14',43,'四类笔记之一已有圈选内容','套索→AI创建事件','可编辑事件字段和取消/确认','多时间消歧;选区完整性;原稿回写;重复提交')
ref(3,'AI;邮件;预览;取消','F-M007-22',66,'编辑器已有收件地址与手写内容','AI生成邮件','独立预览与复制回邮箱/取消入口','取消结果;收件人选择;生成失败;覆盖范围')
ref(4,'AI;邮件;回写;发送','F-M007-22',66,'AI预览已有草稿','复制回邮箱','回到编辑器显示主题/正文，发送仍是另一步','自动草稿保存;失败重试;送达;退出恢复')
ref(5,'同步;周期;备份','F-M007-17',60,'第三方云账号已授权','云入口长按→设置','自动开关/7天3天24小时/目录/格式','周期起点;休眠;断网;删除传播;云满;历史恢复')
ref(6,'导入;文件;完成','F-M007-16',58,'My Files有本地文件','长选→Import→目标目录','目标窗口与确认入口；标题导出/按钮导入有冲突','真正导入结果;取消;重名;满盘')
ref(7,'传输;多设备;接收','F-M007-10',71,'同账号A/B与网页','A导出→网页选文件和B→同步→B Receive','手册给出分步传递示意','自动合并;部分成功;重试;原件关系;恢复')
ref(8,'提醒;确认;后台','F-M007-23',73,'已有Memo便笺','时钟入口→开关/日期/时刻','取消/确认入口可见','保存;锁屏;到点;离线;删除后取消')
ref(9,'复制;对象;跨应用','F-M007-18',26,'套索已选笔迹','复制笔迹','说明限同应用笔记间','跨应用;粘贴失败;来源关联')
ref(10,'复制;对象;跨应用','F-M007-18',27,'可选择的文字','选文字→复制→目标长按粘贴','说明能跨模块；空剪贴板入口灰','格式保真;覆盖;保存;撤销')
ref(11,'删除;缓存;账号','F-M007-13',52,'Apps列表','长按应用','正文清缓存与按钮清数据冲突','影响内容;登录保留;取消;卸载恢复')
ref(12,'账号;邮件;筛选','F-M007-21',64,'邮箱已配置','账号下拉/日期/文件夹/分页设置','可见账号和列表范围控制','账号删除范围;筛选保持;缓存恢复;实际同步')
weekscope='The Week Junior UK第340期公开样刊；2026-10-27桌面浏览器/390×844模拟；非原生App或实体设备'
for n,tag,flows,loc,before,action,result,persist,gap in [
 (1,'阅读;书签;重启','F-M048-07','状态14–16及第二次重启','手动保存14页后回封面','同profile关闭并重启浏览器','页面回14','本次同profile跨进程页码恢复','账户;跨设备;刷机;音频秒点'),
 (2,'音频;暂停;关闭;续播','F-M048-12','状态22–24','2:52暂停且1.5x','关闭外层窗口后同页再开','自动续放到原位置附近，1.5x保留','iframe仍在，不证明落盘；暂停意图未保持','持久化;新进程;真实声音/速度'),
 (3,'音频;重启;失败','F-M048-12','状态25–32','暂停后关闭播放器/外层','同profile第二次浏览器重启','页码14，耳机入口未见，定位超时','音频恢复结果未知','不能确诊丢断点;原因;重试后状态'),
 (4,'阅读;窄屏;跨设备','F-M048-05;F-M048-07','状态33–36','空上下文390×844触摸模拟','打开样刊→菜单跳14','从封面开始；跳14后仍纸刊整页','不属于同账号同步恢复','实体触摸;流式排版;电子纸性能')]:
 refs.append(dict(reference_id=f'RP-M048-{n:02}',product_id='M-048',question_tags=tag,flow_ids=flows,
  evidence_ids='E-WJCD-04',source_locator=loc,artifact_path='artifacts/week-reader-recovery/browser-operation-audit.md',
  precondition=before,action=action,visible_result=result,evidence_basis='automated_public_browser',persistence_result=persist,
  unknown_branches=gap,version_scope=weekscope,checked_on=DATE))
assert not (LIB/'reference-points.csv').exists()
csvwrite(LIB/'reference-points.csv',refs)
dump(ROOT/'work/research/batch39-integration/integration-result.json',{'new_evidence':[EID],'new_flows':[r['flow_id'] for r in new],
 'updated_flows':sorted(found),'appended_captures':sorted(captures),'manual_inventory_rows':len(rows),'reference_points':len(refs),
 'static_pages_observed':len(observed),'static_pages_archived':len(archive)})
print('Integrated 1 new E, 3 new D2 F, 11 existing F, 4 existing E captures; 75-page inventory and 16 partial retrieval points.')
