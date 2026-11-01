from pathlib import Path
import json,csv,hashlib,shutil,re
from collections import Counter
ROOT=Path(__file__).resolve().parents[2];W=ROOT/'work/viwoods-controls-deepening';L=ROOT/'docs/research/interaction-library';B=L/'batches';A=L/'artifacts/viwoods-controls-deepening';A.mkdir(exist_ok=False)
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def csvout(p,rows,fields):
 with p.open('w',newline='') as f:
  wr=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');wr.writeheader();wr.writerows(rows)
old_e={x['evidence_id']:x for x in json.loads((W/'evidence-before.json').read_text())}; old_f={x['flow_id']:x for x in json.loads((W/'coverage-before.json').read_text())}
case=L/'cases/M-007-viwoods.md';assert sha(case)==json.loads((W/'baseline.json').read_text())['case_sha256']
pdf=ROOT/'work/office-aipaper-deepening/Viwoods-AiPaper-manual.pdf';pdfurl=old_e['E-VICD-01']['url'];pdfsha=sha(pdf)
archive_pages=[8,19,22,23,24,25,30,31,32,47,48,51,69]
page_e={p:f'E-VWCD-{i+2:02}' for i,p in enumerate(archive_pages)}
old_pages={2:'E-VICD-02',38:'E-VICD-03',39:'E-VICD-04',40:'E-VICD-05',53:'E-VICD-06',54:'E-VICD-07',55:'E-VICD-08'}
observed=[]
for p in range(2,70):
 q=(ROOT/('work/office-aipaper-deepening/Viwoods-AiPaper-manual-file-'+f'{p:02}.png')) if p in old_pages else W/'raw'/f'manual-file-{p:02}.png'
 assert q.exists()
 if p in page_e:shutil.copyfile(q,A/q.name)
 observed.append({'file_page':p,'printed_page':p-1,'actual_view':'view_image','observed_on':'2026-10-27','hands_on':False,'continuous_sequence':False,'raw_work_path':str(q.relative_to(ROOT)),'raw_sha256':sha(q),'evidence_id':page_e.get(p,old_pages.get(p,'E-VICD-01')),'archived_artifact':str((A/q.name).relative_to(L)) if p in page_e else old_e[old_pages[p]]['archived_artifact'] if p in old_pages else None})
dump(A/'observed-pages.json',observed)
notes='''# M007 AiPaper：历史手册控件与版本边界

2026-10-27；固定10.65英寸AiPaper。资料基线V3.15，实际设备构建未知。无登录、安装、设备或账户动作；hands_on=false，静态示意不能拼为本项目连续操作。

## 现行入口核对

官网User Manuals仍把AiPaper English链到旧E-VICD-01同一URL；本轮普通GET200的17,098,207字节PDF SHA256仍为90bd3c06b4bb4425ad62aef69dd2b553ab622ead68b01a668cea4ab3b5786676。70文件页、印刷1–69；元数据2025-04-30不当固件号。不是一份新V3.15手册。官网Update导航仍到V3.15；正常搜索未取得更高AiPaper版本正文，不能证明不存在其他发布渠道。Mini/Reader/Reader C不纳入案例。

本轮4个官方HTML正文正常GET200：手册入口、V3.15、V3.12、FAQ；前三个旧URL沿用旧E，V3.12为E-VWCD-01。此处“前三个旧URL”指手册入口/V3.15/FAQ，V3.12是新增URL。FAQ仅取AiPaper条目，不套Reader的系统旋转。

## 读取与图像观察

实际逐页查看PDF文件2–69（印刷1–68），共68页：61个本轮渲染页，7个旧已归档摘页复读。封面文件1和联系页文件70未做图像观察。逐页路径/散列/新旧E见observed-pages.json；正式新增13张关键摘页，其余渲染仅存work。未观察官网HTML中的图像、视频或设备。

旧PDF部分字体抽取错码。work中的manual-locator-hints.json对一个特定字体作字符偏移，仅用作定位候选；不把这种变换当可靠全文修复。功能和控件登记最终核对实际页面，乱码未据猜测补按钮。截图中的日期、页数、已选值、输入内容均只是示例；未转录文学正文、歌词或示例邮件全文。

## 关键页与语义边界

| 文件页 / 印刷页 | 观察与限制 |
| --- | --- |
| 8 / 7 | WLAN Transfer同局域网地址、Import/Download、传输列表；图内提示传输期间不要锁屏或关闭此功能。没有失败恢复结果。它与ViTransfer账号收件不同。 |
| 18–23 / 17–22 | Paper列表编辑、标签和快捷模板关联文件夹。删快捷入口与删关联文件夹是不同动作；旧图最多8模板未推为V3.15限制。Paper删除到回收站，但没有读到其期限。 |
| 24–26 / 23–25 | 手写页返回Paper、增页、套索/缩放/旋转、文本编辑、复制粘贴、Undo/Redo。两指说明用back一词，所在标题与图标为Undo/Redo；不解释成离开页面。撤销深度/跨退出保留未知。 |
| 29–34 / 28–33 | Learning有独立Back、选区、批注撤销、阅读笔记及进度；Back后的选区/AI对话锚点仍未知。分享选区TXT、汇总批注页PDF与原文可编辑性分开。PDF/EPUB显示图分别有当前书/全部书选择，选中值不是已证默认。 |
| 41 / 40 | Daily关联可按笔记名打开建立关联的页，是手册明说的返回范围；不扩大为任意AI返回锚点。 |
| 47 / 46 | 历史Paper/Meeting密码分开、本地管理及忘密不能解锁；V3.15发布页另写共用笔记密码、Settings→Security Settings。历史规则不能覆盖新版账号/同步；迁移与找回仍未知。 |
| 48 / 47 | Apps正文称清缓存，截图菜单为Clear Data；未确定清理范围，不能保证笔记/登录态保留。卸载和Add to Home入口可见，未触发。 |
| 51 / 50 | AI命令正文最多5条，截图创建窗最多4条，冲突保留。历史说明24小时内可续聊，不等于24小时后自动删档；AI commands不自动保存一语与自定义命令配置之间语义未解释。 |
| 52 / 51 | Local标为仍在开发，只限旧手册，不能改写成现版不支持本地文件。 |
| 68–69 / 67–68 | Memo提醒时间、长按编辑、Delete及Recycle Bin；最多30天规则仅在Memo章。恢复/永久删除为正文声明，具体恢复按钮及确认/失败页未见。 |

## 本批目录与流程的边界

目录登记历史各章/页上的控件和本轮官方版本增量。父章/页/控件有包含关系，不当独立任务数量。旧任务只补同目标分支；新增Paper编辑、快捷模板与关联目录、Memo删除去向三个目标。页组织方面，旧手册只见多页入口/增页；V3.15明确跨笔记移页和整本复制，但缺具体拖动/目标确认路径，保留D1增量，不能用Picking的Move Left/Right当Paper页序操作。

权限方面，V3.15 Advanced Settings的多用户管理只列功能和入口，没有逐权限定义或切换后的可见性。用户账号、笔记密码、锁屏密码、第三方云授权和Mailbox账号是不同对象。所有恢复/回滚/离线同步结论受此范围限制。
'''
(A/'source-notes.md').write_text(notes)
ns=sha(A/'source-notes.md'); obs_sha=sha(A/'observed-pages.json')
captures=json.loads((W/'captures.json').read_text());capmap={x['key']:x for x in captures}
new=[]
def base(eid,title,url,kind,scope,locator,note):
 return {'evidence_id':eid,'product_id':'M-007','title':title,'url':url,'checked_on':'2026-10-27','source_type':kind,'access_mode':'anonymous_public_HTML_GET_text_read','version_scope':scope,'app_build':None,'firmware_version':None,'source_updated_on':None,'locator':locator,'note':note,'hands_on':False,'visual_observed':False,'supersedes':None,'archived_artifact':'artifacts/viwoods-controls-deepening/source-notes.md','sha256':ns}
x=base('E-VWCD-01','AiPaper V3.12：App Display作用域及历史增量',capmap['update312']['url'],'official_release_notes','AiPaper original; historical V3.12 notes, not a new release after V3.15','App Display / Settings / File Management / Knowledge Base','独立App刷新开关可选自身配置或跟随系统；未核保存/重启行为。性能表述不当测量。')
x.update({k:v for k,v in capmap['update312'].items() if k not in ('key','url','checked_on')});new.append(x)
titles={8:'WLAN传输的网络与锁屏条件',19:'Paper列表编辑和删除去向',22:'快捷笔记与关联文件夹的双向关系',23:'快捷模板编辑入口',24:'Paper手写工具栏与增页',25:'套索编辑与撤销手势',30:'Learning选区工具及输出对象',31:'Learning返回与批注工具栏',32:'Learning阅读笔记汇出和显示范围',47:'历史笔记加密与忘密边界',48:'Apps清理文图差异',51:'AI命令上限文图差异与历史',69:'Memo删除与回收期限'}
for p in archive_pages:
 eid=page_e[p];q=A/f'manual-file-{p:02}.png';x=base(eid,titles[p],pdfurl,'official_manual_static_page','AiPaper historical 70-page English manual; build unspecified; not V3.15 menu verification',f'PDF文件{p}；印刷{p-1}',f'实际看过静态页；与E-VICD-01同一原件，非独立交叉验证；见独立摘记。')
 x.update(access_mode='local_official_PDF_render_actually_viewed',visual_observed=True,derived_from='E-VICD-01',raw_work_path=str(pdf.relative_to(ROOT)),raw_sha256=pdfsha,archived_artifact=str(q.relative_to(L)),sha256=sha(q),visual_audit_path='artifacts/viwoods-controls-deepening/source-notes.md',visual_audit_sha256=ns,observed_on='2026-10-27');new.append(x)
(B/'viwoods-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in new))
additional=[]
for key,eid in [('manual-index','E-VIW-11'),('update315','E-VIW-02'),('faq','E-VIW-10')]:
 cap={k:v for k,v in capmap[key].items() if k!='key'};cap.update(access_mode='anonymous_public_HTML_GET_text_read',hands_on=False,visual_observed=False,archived_artifact='artifacts/viwoods-controls-deepening/source-notes.md',sha256=ns)
 additional.append({'evidence_id':eid,'expected_url':old_e[eid]['url'],'before_additional_captures':old_e[eid].get('additional_captures',[]),'append_additional_captures':[cap]})
cap=json.loads((W/'pdf-capture.json').read_text());cap.update(access_mode='anonymous_public_PDF_GET_same_bytes_then_local_static_page_review',hands_on=False,visual_observed=True,observed_file_pages=list(range(2,70)),observed_printed_pages='1–68',visual_audit_path='artifacts/viwoods-controls-deepening/observed-pages.json',visual_audit_sha256=obs_sha,archived_artifact='artifacts/viwoods-controls-deepening/source-notes.md',sha256=ns,note='同一旧原件复取，目录/功能页逐页观察；不是现行V3.15手册。')
additional.append({'evidence_id':'E-VICD-01','expected_url':pdfurl,'before_additional_captures':old_e['E-VICD-01'].get('additional_captures',[]),'append_additional_captures':[cap]})
dump(W/'old-evidence-additional-captures.json',additional)
dump(W/'old-evidence-field-updates.json',[])

updates=[]
def change(n,known,gaps,es,visual=True):
 fid=f'F-M007-{n:02}';before=old_f[fid];after=dict(before);after.update(known_branches=known,gaps=gaps,evidence_ids=';'.join(dict.fromkeys(before['evidence_ids'].split(';')+es)),checked_on='2026-10-27')
 if visual:after['visual_status']='static_manual_pages_only'
 updates.append({'flow_id':fid,'before':before,'after':after,'changed_fields':{k:{'before':before[k],'after':v} for k,v in after.items() if before[k]!=v}})
change(1,'历史手册区分右上下拉控制中心与左上下拉全刷；V3.12 App Display新增独立刷新开关，可用本App设置或跟随系统。不是性能实测。','V3.15具体开关页、保存/重启保持、关闭进程前自动保存和音频恢复未核；不能继续将“是否有逐App选择”全列未知。',['E-VICD-01','E-VWCD-01'])
change(2,'历史PDF/EPUB长按或Selection后，AI带入选区；同栏有翻译、词典、复制、TXT分享、高亮归入Reading Notes。Back入口独立存在，未证AI关闭后的返回锚点。','扫描件OCR、长选区截断、对话关闭后选区/页/滚动保持仍未知；不能据Back图标认定一键返回原选区。',['E-VWCD-08','E-VWCD-09'])
change(3,'历史配置窗有命令名称/指令与Confirm；正文最多5条而同页截图写4条，二者冲突；长按删除为文字规则。','V3.15数量与编辑/删除确认、改指令后旧结果关系未知；不能把历史5条上限写成统一事实。',['E-VWCD-13'])
change(4,'历史手册长按AI录入最多15秒、松开自动提交；文本输入为短按。TXT/HTML导出；历史说明24小时内可续聊，不能推出之后自动删除。','误松提交、取消/失败/重试、当代会话保留期未知；“AI commands不自动保存”的具体对象未解释，不能与自定义命令保存混同。',['E-VICD-01','E-VWCD-13'])
change(7,'标记/大纲沿用V3.14；旧Paper标签可长按改名/删除并从侧栏筛选；Daily关联明确按笔记名打开关联建立时的页。V3.15新增大纲子层、整本复制与跨笔记移页只是功能增量。','移页后标记/大纲/日历锚点、同名条目、撤销/恢复未核；没有把Picking移页按钮当作Paper移页流程。',['E-VICD-01','E-VIW-02'])
change(8,'保留Picking→Knowledge Base版本区分；旧Picking标注开关启用时锁当前页，停用后标注冻结可再启，Save为另一入口。V3.12重组PickNotes/VisualNotes并改为先注释再裁剪保存。','URL/书页来源、取消/关闭后持久化及OCR错误、去重未知；不同版本不能拼同一连续流程。',['E-VICD-01','E-VWCD-01'])
change(9,'历史页/章导航之外，Reading Notes可选一/多/全部批注页汇为PDF；Learning书库长按可改名/删书。当前书/全部书显示范围有选择入口，图中选中值不当默认。','跳转返回历史、AI返回、删书后标记/回收期限、导出后原生可编辑性仍未知；Memo30天不得套书库。',['E-VICD-01','E-VWCD-09','E-VWCD-10'])
change(10,'ViTransfer是登录账号后显式Import/Receive；独立WLAN Transfer为同局域网URL，并提示传输期间不要锁屏或关闭此功能。历史手册导入正文my.viwoods.com与图中mytest域名不一致，未访问测试域名。','重名/断线/取消/满盘、锁屏失败后的续传、当前构建适配未知；不把局域网传输与账号云同步混成一项自动机制。',['E-VICD-01','E-VWCD-02'])
change(11,'V3.15原生云与第三方云/ViTransfer分开；账号内容独立。V3.15称Paper/Meeting共用笔记密码，旧手册分别管理。Advanced Settings多用户权限仅功能声明。','完整同步/权限菜单、密码迁移/找回、离线删除、云满、账号切换后本地可见性与授权范围仍未知。',['E-VIW-10','E-VWCD-11'])
change(13,'保留WLAN/Google故障恢复路径；历史Play Store禁用说明清缓存，Apps页正文clear cache但图为Clear Data，不同入口不能等同；未执行清理/卸载。','清理具体数据、登录态和笔记保留、卸载/重装恢复、AI请求续跑/收费重试仍未知；不能保证清理无损。',['E-VICD-01','E-VWCD-12'])
dump(W/'old-coverage-updates.json',updates)
fields=list(next(iter(old_f.values())).keys());nf=[]
def flow(n,task,known,gaps,ids):
 nf.append(dict(flow_id=f'F-M007-{n:02}',product_id='M-007',task=task,depth='D2',evidence_ids=';'.join(ids),known_branches=known,gaps=gaps,visual_status='static_manual_pages_only',hands_on='false',case_path='cases/M-007-viwoods.md',checked_on='2026-10-27'))
flow(18,'在Paper编辑选区、撤销并返回笔记列表','旧手册：套索圈选后缩放/旋转/剪切/复制/删选区；两指back置于Undo/Redo节，三指redo。末页左划显Add Page；返回Paper按钮独立。','撤销层数/重开保留、改模板/层与原笔迹关系、保存时点未知；不将back解释成离开页面。',[page_e[24],page_e[25],'E-VICD-01'])
flow(19,'创建Quick Note模板并理解关联文件夹','Quick Note模板编辑→Add New→名称/模板→Done示意；生成同名关联文件夹，使用对应模板的笔记归入该目录。删快捷入口改为普通目录，删关联目录移除快捷入口。','同名冲突、删除目录内文件去向、撤销和当前V3.15上限未知；删除快捷入口不等于删除笔记。',[page_e[22],page_e[23],'E-VICD-01'])
flow(20,'删除Memo并核查回收去向','历史Memo长按→Delete；Recycle Bin入口可见，正文称最近删除最多保留30天，可恢复或永久删除。','未见恢复按钮/确认步骤、到期触发条件、恢复位置/提醒重建、V3.15云传播；不套Paper/Meeting/Learning或系统文件。',[page_e[69]])
csvout(B/'viwoods-controls-deepening-coverage.csv',nf,fields)

# A page/control ledger, not an automatically generated claim of completed tasks.
idata=json.loads((W/'inventory-data.json').read_text());inventory=[]
chapters=[(1,'Quick Start',3,15),(2,'Feature Overview',16,16),(3,'Input Method',17,17),(4,'Paper',18,27),(5,'Learning',28,34),(6,'Meeting',35,37),(7,'Daily',38,41),(8,'Picking',42,46),(9,'File Encryption',47,47),(10,'Apps',48,48),(11,'AI',49,51),(12,'Viwoods Files APK',52,54),(13,'Third-Party Cloud Integration',55,55),(14,'Mailbox',56,62),(15,'ViTransfer',63,66),(16,'Share',67,67),(17,'Memo',68,69)]
flowmap={**{p:'F-M007-01' for p in range(3,11)},13:'F-M007-18',15:'F-M007-13',18:'F-M007-18',19:'F-M007-18',20:'F-M007-07',21:'F-M007-19',22:'F-M007-19',23:'F-M007-19',24:'F-M007-18',25:'F-M007-18',26:'F-M007-18',27:'F-M007-18',28:'F-M007-10',29:'F-M007-09',30:'F-M007-02',31:'F-M007-09',32:'F-M007-09',33:'F-M007-09',34:'F-M007-09',37:'F-M007-07',38:'F-M007-12',39:'F-M007-14',40:'F-M007-15',41:'F-M007-07',42:'F-M007-08',43:'F-M007-08',44:'F-M007-08',45:'F-M007-08',46:'F-M007-08',47:'F-M007-11',48:'F-M007-13',49:'F-M007-04',50:'F-M007-02',51:'F-M007-03;F-M007-04',52:'F-M007-16',53:'F-M007-16',54:'F-M007-16',55:'F-M007-17',63:'F-M007-10',64:'F-M007-10',65:'F-M007-10',66:'F-M007-10',68:'F-M007-12',69:'F-M007-20'}
def inv(parent,level,chapter,item,p,eids,flows,status,gap,url=pdfurl,scope='AiPaper historical manual; firmware build unknown'):
 iid=f'I-VWCD-{len(inventory)+1:03}';inventory.append(dict(inventory_id=iid,parent_id=parent,level=level,source_family=chapter,item=item,file_page=str(p) if p else '',printed_page=str(p-1) if p else '',locator=f'PDF文件{p}/印刷{p-1}' if p else chapter,source_url=url,evidence_ids=eids,flow_ids=flows,coverage_status=status,model_version_scope=scope,gap=gap));return iid
for num,name,start,end in chapters:
 pid=inv('',1,'历史手册17章',f'{num}. {name}',2,'E-VICD-02','','directory_only','父章非独立任务；以下逐页控件不是实操')
 for p in range(start,end+1):
  title,items=idata[str(p)];eid=page_e.get(p,old_pages.get(p,'E-VICD-01'));fid=flowmap.get(p,'')
  pp=inv(pid,2,name,title,p,eid,fid,'static_page_read','页范围；同页子项不重复计任务')
  for item in items.split('|'):
   inv(pp,3,name,item,p,eid,fid,'static_control_or_document_text_read','入口/文字已核；未实际执行。关联F仅为任务域，未闭合项以case/F缺口为准')
for family,eid,items in [
 ('V3.12增量','E-VWCD-01','Knowledge Base/PickNotes/VisualNotes|多层文件夹|Grid/List|图上手写|先注释后裁剪|Quick Refresh声明|App Display独立/跟随系统|Light Color Removal|调节+/-|Factory Reset解绑选项|AiNotes目录|AI模型切换'),
 ('V3.15增量','E-VIW-02','原生云默认关闭/10GB|按App同步|Learning选书同步|网页预览下载/传入|账号内容分离|共用笔记密码|Paper大纲子层|整本复制|跨笔记移页|PDF目标目录|Knowledge Base保存路径|截图浮钮|自动图像名|Memo多页增删|Memo手写转待办|透明背景|手写命名|更多背景|列表排序|本地/第三方云导入目录|Daily note格式修复|PDF分享到Paper/Learning|笔颜色|Advanced Settings权限|Google Tasks'),
 ('现行FAQ抽核','E-VIW-10','AiPaper Learning横屏|Reader系统旋转排除|Paper组织声明|原生云默认/账号条件')]:
  url='https://viwoods.com/pages/system-update-3-12' if eid=='E-VWCD-01' else old_e[eid]['url'];pid=inv('',1,family,family,None,eid,'','official_text_read','不是全系统现行菜单；未看HTML图片',url,'AiPaper; named release or current FAQ, device build unknown')
  for item in items.split('|'):inv(pid,2,family,item,None,eid,'','release_claim_or_help_text','具体入口/结果不足时保持D1，不能以旧图填新版步骤',url,'AiPaper; release-specific delta')
csvout(B/'viwoods-controls-deepening-inventory.csv',inventory,list(inventory[0]))
dump(W/'inventory-summary.json',{'new_e':len(new),'new_f':len(nf),'inventory_rows':len(inventory),'levels':dict(Counter(str(x['level']) for x in inventory)),'old_f':len(updates),'old_f_fields':sum(len(x['changed_fields']) for x in updates),'old_e_additional_captures':len(additional),'observed_pages':68,'new_archived_pages':13})

s=(W/'case-before.md').read_text()
s=s.replace('查阅2026-10-26。固定','初查2026-10-26；2026-10-27复核手册入口/版本页并逐页看历史控件。固定',1)
s=s.replace('当前官网V3.15、V3.14发布记录为主要依据','官网Update导航本轮仍指V3.15；V3.15、V3.14发布记录为版本基线',1)
s=s.replace('本次沿官网AiPaper英文链接取得70页原PDF并查看部分静态页，归档7页','2026-10-26沿官网AiPaper英文链接取得70页原PDF并归档7页；2026-10-27重新GET得到相同散列，逐页实看文件2–69并新增13个关键摘页',1)
s=s.replace('本次按实际查看的目录建立[17章台账]','此前按实际查看的目录建立[17章台账]',1)
s=s.replace('## 任务流程','本轮新增[逐章/页/控件台账](../batches/viwoods-controls-deepening-inventory.csv)，含历史功能页与现行官方增量；父子目录不作任务数。原PDF文件2为印刷1，正文文件3–69为印刷2–68；封面及联系页未做图像观察。全部静态观察索引见[观察审计](../artifacts/viwoods-controls-deepening/observed-pages.json)。字体偏移转换仅定位，正式控件逐页看图核对。\n\n## 任务流程',1)
newrows={
1:'| F-M007-01 | 从正文进入系统控制 | 历史手册/发布说明，app打开 | 右上下拉控制中心；左上向下为全刷，Home长按为任务管理。V3.12 App Display可选独立刷新或跟随系统。 | 显示配置与关闭进程分开；不是性能或持久化实测。 | E-VIW-08/09；E-VICD-01；E-VWCD-01 | V3.15具体页面、保存/重启保持、关闭前落盘与音频恢复。 |',
2:'| F-M007-02 | 从书内选段问AI | 历史Learning，PDF/EPUB可选文字；AI需网络 | 长按或Selection→AI带入选区；同栏可翻译/词典/复制/TXT分享/高亮。 | 高亮归入Reading Notes；Back独立入口不证明对话返回锚点。 | E-VIW-06/12；E-VWCD-08/09 | 扫描件、长文本截断、关闭AI后的选区/页/滚动保持。 |',
3:'| F-M007-03 | 设置重复使用的AI指令 | 历史AI配置 | 命令名称/指令→Confirm；长按可删除。 | 原手册正文5条、截图4条冲突，不设统一上限。 | E-VIW-06；E-VWCD-13 | V3.15限制/编辑入口、删除确认及改指令对旧结果影响。 |',
4:'| F-M007-04 | 语音提问并带走结果 | 历史入口；联网 | 长按AI最长15秒，松开自动提交；短按键盘；TXT/HTML导出。 | 历史24小时内可续聊不等于之后自动删档；短语音不同于长会议录音。 | E-VIW-06；E-VICD-01；E-VWCD-13 | 误松取消/重试、现版历史保留；“AI commands不自动保存”对象含义未解。 |',
7:'| F-M007-07 | 从长笔记找回标记位置 | Paper内容存在；版本分别列明 | V3.14圈选标记/大纲；旧标签侧栏检索、长按改名/删除；Daily按关联笔记名回建立关联时的页。 | V3.15新增大纲子层、整本复制、跨笔记移页为功能增量，缺操作确认链。 | E-VIW-03/04/02；E-VICD-01 | 重排/移页后锚点、同名、撤销恢复；不套Picking移页菜单。 |',
8:'| F-M007-08 | 提取其他app内容并继续AI | 旧Picking与新Knowledge Base分开 | 旧Picking下拉/浮钮等入口沿用；历史标注模式启用锁页、停用冻结可再启，Save另列。V3.12重组PickNotes/VisualNotes、先注释后裁剪保存。 | V3.14裁剪外取消/OCR动作与V3.13来源命名按各版记录。 | E-VIW-07/04/05；E-VICD-01；E-VWCD-01 | 原URL/页码、取消持久化、AI图像/OCR输入、重复去重。 |',
9:'| F-M007-09 | 从阅读导航回到证据 | Learning已有书，历史手册 | 底部五分一区进度/跳章；Reading Notes选一/多/全部批注页汇PDF；书库长按Rename/Delete。 | 当前书/全部书显示范围可选；图中已选值非默认。V3.13离线英语词典另保留。 | E-VIW-12/05；E-VICD-01；E-VWCD-09/10 | 跳转历史、删书标注/回收期限、PDF导出后原生可编辑性；不套Memo30天。 |',
10:'| F-M007-10 | 传入资料：账号收件与局域网入口 | ViTransfer同账号；WLAN Transfer另需同局域网 | ViTransfer电脑Import/手机+→设备Receive。局域网入口用页面地址传输，图内提示期间勿锁屏/关闭功能。 | 两入口分别取证；导入正文my.viwoods.com与旧图mytest域名不同，未访问测试域名。 | E-VIW-12；E-VICD-01；E-VWCD-02 | 重名、断线/取消/满盘、失败续传；不当自动双向云。 |',
11:'| F-M007-11 | 选择需要同步的内容（D1） | AiPaper V3.15；账号与网络 | 原生云默认关闭，启用10GB；按App控制，Learning选书上传。 | 账号内容分离；V3.15 Paper/Meeting共用笔记密码，区别旧独立密码；Advanced Settings权限仅功能声明。 | E-VIW-02/10；E-VWCD-11 | 同步/权限完整菜单、密码迁移/找回、离线删除、云满与切号本地可见性。 |',
13:'| F-M007-13 | 网络/Google服务故障恢复 | 功能联网/授权异常 | 沿官网WLAN遗忘重连、Play Store服务恢复。旧手册还见App长按清理/卸载入口。 | Apps正文称清缓存、截图Clear Data，范围冲突；没有执行清理。 | E-VIW-09；E-VICD-01；E-VWCD-12 | 登录/笔记保留、卸载重装、AI续跑及计费重试均未知，不保证清理无损。 |'
}
for n,row in newrows.items():
 pattern=rf'^\| F-M007-{n:02} \|.*$';s,count=re.subn(pattern,lambda m:row,s,flags=re.M);assert count==1,(n,count)
s=s.replace('\n\n| F-M007-14','\n| F-M007-14')
extra='''
## Paper编辑、快捷目录与Memo删除

以下三个新任务仅由历史手册重建，D2不代表实操或V3.15当前菜单验证。

| F | 操作与条件 | 数据/退出边界 | 证据/缺口 |
| --- | --- | --- | --- |
| F-M007-18 Paper编辑选区并撤销 | 套索圈选后调整/剪切/复制/删除；两指说明用back，位于Undo/Redo节，三指redo。末页左划可显Add Page。 | 返回Paper是独立按钮；不把撤销当离开页面。 | E-VWCD-06/07、E-VICD-01；撤销层数、保存时点、层/模板关系与重开后保持未知。 |
| F-M007-19 快捷模板与关联目录 | Quick Note模板编辑→新增→名称/模板→Done示意；同名目录关联，笔记按模板归类。 | 删快捷入口使目录变普通；删关联目录会移除快捷入口。不能推定这两种删除都删笔记。 | E-VWCD-04/05、E-VICD-01；同名冲突、目录内容去向、撤销/当前上限未知。 |
| F-M007-20 Memo删除与回收 | 长按Memo→Delete；Recycle Bin入口可见，正文称最多保留30天、可恢复或永久删。 | 期限只限历史Memo，不套Paper、Meeting、Learning或系统文件。 | E-VWCD-14；恢复具体按钮/确认、位置/提醒重建、到期与云删除规则未知。 |

Paper文件删除到回收站有独立说明，但该页没有期限；Learning长按删书只有入口。V3.15跨笔记移页/整本复制缺明确目标选择与完成回执，仍只作功能增量。多用户权限、锁屏密码、笔记密码、云账号及邮箱授权分别研究，不能推成统一数据隔离保证。

'''
s=s.replace('## 版本差异、错误与数据边界',extra+'## 版本差异、错误与数据边界',1)
s=s.replace('回收期未统一核实。','历史Memo最多30天见F20；其他对象期限仍未核。',1)
s=s.replace('## 实际观察与项目关系','历史手册File Encryption（印刷46）称Paper/Meeting密码各自管理且本地保存；V3.15发布页改为共用笔记密码、Settings→Security Settings。保留两期，迁移/找回未核。Apps清缓存/Clear Data、AI命令5/4条的文图差异亦保留，详见[本批独立摘记](../artifacts/viwoods-controls-deepening/source-notes.md)。\n\n## 实际观察与项目关系',1)
s=s.replace('全部查于2026-10-26。','初批查于2026-10-26；本轮手册/版本/FAQ复读与静态观察为2026-10-27。',1)
s+='\n### 2026-10-27控件补证\n\nE-VWCD-01为[官方V3.12增量](https://viwoods.com/pages/system-update-3-12)，E-VWCD-02—14为同一历史PDF的13张衍生静态摘页，不是13份独立来源。原件和观察边界见[独立摘记](../artifacts/viwoods-controls-deepening/source-notes.md)，逐页/编号见[观察审计](../artifacts/viwoods-controls-deepening/observed-pages.json)。官网英文手册本次仍为旧字节；未证更高固件，不把网页当前日期当旧手册修订。\n'
case.write_text(s)
formal=[case,B/'viwoods-controls-deepening-evidence.jsonl',B/'viwoods-controls-deepening-coverage.csv',B/'viwoods-controls-deepening-inventory.csv']+sorted(A.iterdir())
dump(W/'formal-manifest.json',[{'path':str(p.relative_to(ROOT)),'sha256':sha(p)} for p in formal])
print((W/'inventory-summary.json').read_text())
