from pathlib import Path
import csv,json,hashlib,re,shutil
ROOT=Path('/home/workspace/reader');L=ROOT/'docs/research/interaction-library';W=ROOT/'work/sony-dpt-controls-deepening'
B=L/'batches';A=L/'artifacts/sony-dpt-controls-deepening';A.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def jwrite(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n')
def csvwrite(p,rows,fields):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=fields);w.writeheader();w.writerows(rows)
catalog=json.loads((W/'html-catalog.json').read_text());logs=json.loads((W/'fetch-log.json').read_text())
oldE=[json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines()]
old_by_url={e['url']:e for e in oldE if e['product_id']=='M-059' and not e.get('derived_from')}
titles={x['url']:x['title'] for x in catalog};titles[logs[0]['url']]='1.6官方帮助首页与完整叶目录'
key_to_id={};new=[];caps=[]
scope='DPT-RP1/DPT-CP1; firmware 1.6 guide 4-597-942-19(1); installed build/current OS compatibility unverified'
for r in logs:
 assert r['status']==200 and Path(r['text_work_path']).stat().st_size>200
 key=Path(r['url']).stem
 cap={'checked_on':'2026-10-27','access_mode':'ordinary_public_html_body','capture_url':r['url'],
      **{k:r[k] for k in ['raw_work_path','raw_sha256','text_work_path','text_sha256']},
      'note':'普通GET200；首次article提取漏step-list，已离线按id_content修正；无产品操作。'}
 if r['url'] in old_by_url:
  e=old_by_url[r['url']];key_to_id[key]=e['evidence_id'];caps.append({'evidence_id':e['evidence_id'],'capture':cap});continue
 eid=f'E-DPCD-{len(new)+1:02}';key_to_id[key]=eid
 new.append(dict(evidence_id=eid,product_id='M-059',title=titles[r['url']],url=r['url'],checked_on='2026-10-27',
   source_type='official_help',access_mode='ordinary_public_html_body_with_pdf_cross_reference',version_scope=scope,
   app_build=None,firmware_version=None,source_updated_on=None,locator='id_content；固定1.6 PDF对应章节，页码见目录台账',
   note='公开HTML正文归档，功能依据与PDF章节互定位；不是当前安装构建或实机验证。',hands_on=False,
   visual_observed=False,archived_artifact=None,sha256=None,capture_state='short_note_archived_raw_temporary',supersedes=None,
   **{k:r[k] for k in ['raw_work_path','raw_sha256','text_work_path','text_sha256']}))
assert len(new)==33
for n in [15,56]:shutil.copyfile(W/f'guide-p{n}.png',A/f'guide-p{n}.png')
eid='E-DPCD-34';new.append(dict(evidence_id=eid,product_id='M-059',title='文档工具栏和整笔画擦除静态手册页',
 url=old_by_url['https://helpguide.sony.net/dpt/rp1/v1/en/print.pdf']['url'],checked_on='2026-10-27',source_type='official_manual',
 access_mode='existing_pdf_render_viewed',version_scope=scope,app_build=None,firmware_version=None,source_updated_on=None,
 locator='PDF p15、56实际查看并正式归档；p61为纯步骤页，已查看仅留work',note='p15显示跳转历史按钮与页码；p56显示笔键位置，底部对比图不完整，不据静态图声称已擦除。',
 hands_on=False,visual_observed=True,visual_kind='static_manual_pages',derived_from='E-DPT-01',supersedes=None,
 archived_artifact='artifacts/sony-dpt-controls-deepening/guide-p15.png',sha256=sha(A/'guide-p15.png'),
 additional_artifacts=[{'path':'artifacts/sony-dpt-controls-deepening/guide-p56.png','sha256':sha(A/'guide-p56.png')}],
 raw_work_path='work/research/extension-manuals/sony-dpt.pdf',raw_sha256=sha(ROOT/'work/research/extension-manuals/sony-dpt.pdf'),
 capture_state='selected_static_page_excerpt'))
def ids(*keys):return ';'.join(dict.fromkeys(['E-DPT-01']+[key_to_id[k] for k in keys]))
# Each tuple: number, task, steps, state/branches, gaps, HTML source keys, PDF pages.
data=[
(11,'传入PDF并传回电脑','配对连接→PC App选Folders及目标目录→从电脑拖文件/目录；传出时选设备条目→Transfer documents to computer→选电脑目录。','只传PDF、单文件≤1GB；All documents/搜索结果页不能作为传入位置；断开连接可能损坏数据，空间不足先查容量。','同名覆盖/取消/部分失败回执、当前系统兼容性和恢复未测。',['TP0001178322','TP0001178319'],[24,26,95]),
(12,'整理文件且识别同步关系变化','PC App选Folders→新建/右键改名，或Cut/Copy (within Digital Paper)→目标目录Paste；设备文档菜单Change Document Name / Copy可另存同目录。','PC App剪贴只在App内，不等于电脑文件剪贴；同步目录改名或移动会删除同步配置，继续同步须重设。删除入口已知，确认与回收未见。','同名冲突、误删恢复、原文改名对side-note影响不补推。',['TP0001178310'],[33,36]),
(13,'定位正文并回到跳转前页面','文档工具栏Search Text→输入→Search→点命中页；点外关闭。目录/Page Jump/文内链接跳转后可用前后历史按钮。','大小写不区分；手写与图片内文字不可搜。普通划页不生成跳转历史；无PDF书签时目录按钮不出现。','无命中界面、搜索跳转是否计入同一历史、跨书/重启历史未知。',['TP0001178330','TP0001178301'],[15,16,42,48]),
(14,'放大批注与缩略页跳转','双指张开放大、捏合缩小；或工具栏Zoom圈选区域；拖动看局部，Exit退回。菜单Page Thumbnails→点页恢复正常显示。','放大可写/擦/高亮但不可翻页；工具圈选放大不能再次改比例，须退出重选。缩略通常16页，并排状态9页。','手势误触、退出定位和实际延迟未测；不把pinch与圈选限制合成一条。',['TP0001273122','TP0001178328'],[43,44,45]),
(15,'写画、高亮、整笔画擦除与撤销','直接用笔写；按笔高亮键划文本；按橡皮键掠过笔迹后抬笔提交擦除；工具栏Undo/Redo可撤回。','写完即保存；擦中部分线会去掉整条线，不是像素擦除。换页清撤销。扫描图/图片文字/手写不能文本高亮；低笔电、正在保存或PC操作可阻止批注。','断电持久化、笔画识别边界、部分失败和跨重启撤销未测。',['TP0001178298','TP0001178297','TP0001178309','TP0001178285'],[54,55,56,57,91]),
(16,'从批注列表回访并处理不可编辑','文档菜单Annotation List→Handwriting/Highlight页签→点条目定位；点外关闭。Settings→System Settings→Limit Editing Annotations设置外来批注编辑限制。','DPT-S1便笺/书签不随关闭限制变可编辑；受保护文档/低空间只读、低笔电、PC正操作也是不同原因。条目多可能慢，无计时实测。','只读提示精确样式、解锁后的可编辑范围及导出互操作未测。',['TP0001178308','TP0001178307','TP0001178286'],[52,59,60,92]),
(17,'创建独立笔记并增删页','Home→Create New Note→选模板OK，生成Note目录中的模板名+日期PDF；All Notes打开。菜单Insert New Page在当前页前插页，末页向下一页划动追加。','PDF带笔记属性；即使原文打开，从Home创建也不形成关联。Delete Current Page后手册说不能恢复；双页视图不可增删页。','删页确认、满空间、恢复备份后特殊属性与删除撤销均未实测。',['TP0001178282','TP0001178283'],[63,64,68]),
(18,'添加PDF模板并选择默认模板','PC App Settings→Note template Add→Select file→选PDF→命名OK；已有项可Rename/Delete。设备System Settings→Note Template选固定模板或每次询问。','多页PDF只取第一页；密码保护PDF不能加入；模板不是原多页资料的完整导入。','删模板对既有笔记、重复命名/失败和多端同步未知。',['TP0001178312','TP0001178279'],[35,69,70]),
(19,'填写PDF表单','打开Adobe Acrobat制作的支持表单，手指/笔选复选框、单选、下拉或列表；文本框点入→键盘输入→Save。','支持五类表单对象；文本框明确保存按钮，不能由手写即时保存规则代替。','校验失败、取消、提交到服务器、签名/脚本及导出兼容未说明。',['TP0001690500'],[62,98]),
(20,'在电脑只读查看设备文档','已连接PC App中双击文档→系统PDF阅读器打开只读副本。','文件临时放在电脑，关闭Digital Paper App时删除；不是Transfer导出或独立备份。','阅读器仍开时删除反馈、缓存残留、另存和修改回传未知。',['TP0001178341'],[53]),
(21,'把设备画面显示到电脑','连接设备→启动PC App→Screen capture。','该入口显示设备屏幕并可保存图像；蓝牙比USB/Wi-Fi慢是手册定性说明。','保存图像控件/格式、刷新率、远程操作和停止步骤未由指南证明。',['TP0001690538'],[17,32]),
(22,'校准笔触位置','System Settings→Pen Calibration→四种握姿选一→Next→描虚线→Next→箭头对齐→Next→再次描线→Save。','第一次描线抬笔才显示；确认页仍偏移可Recalibrate返回校正并再验，不能把未落笔显示当笔失效。','取消、校准保存范围和儿童握姿适配未测。',['TP0001178306'],[61,85]),
(23,'设置锁屏并用已登记卡片解锁','System Settings→Screen Lock→Enable→设4–20字符密码；先有密码，再进NFC Card Lock Release Settings输入密码、Register NFC Card并贴近约2秒。','只登记一张；换卡先注销。唤醒可密码或登记FeliCa解锁；注销需密码。忘记密码的手册服务移除会清全部文档，费用/现今可用性未核。','错误次数、卡丢失后的实际回执、锁屏时PC访问范围未知。',['TP0001178332','TP0001178338'],[10,11,12]),
(24,'连接电脑并在休眠后恢复连接','首次PC App用USB按向导配对；以后USB或同网Wi-Fi连接，PC App显示Connected。蓝牙先设备进Pairing→电脑登记→设备Close。','Wi-Fi/蓝牙各有睡眠保持设置；Windows可自动蓝牙重连，Mac需系统操作。多设备用Tools→Switch Digital Paper→Add/Switch，不能同时传文件；Re-pair另机可能删除原同步设置。','首次向导每屏、锁屏下访问、当前OS支持和失败回滚未知；多设备数量与并发说明不相互替代。',['TP0001178315','TP0001273123','TP0001690498','TP0001178280','TP0001178281'],[19,20,21,22,23,73,74,76,77,88]),
(25,'检查更新与核对设备版本','USB连接→电脑联网开PC App→Help→Software update按向导；设备Settings→About This Device→Device Details核版本，PC App在About查看。','固件早于1.2先升级PC App再升设备；自动通知可配置，通知不是已升级。','当前可下载版本、向导细节、升级失败/回退和数据保留没有实测。',['TP0001178296','TP0001178281'],[73,74,75]),
(26,'按文件信息检索资料','设备Home→All Documents→Search→File Information输入词→Search→点文档，Return to All Documents退出；PC App File information→Filename,title,author→Search。','设备查文件名/标题/作者；PC还查文件夹名，结果双击文件夹展开、文档交电脑PDF阅读器；不是全文OCR检索。','匹配规则、无结果反馈、范围和跨语言检索未测。',[],[30,50]),
(27,'无响应时复位并辨认数据风险','屏幕不出现且电源/笔均无反应时，指南要求按实体RESET，再按电源开机；连接故障章节先列重连/关开机，最后才复位。','指南明确复位可能丢设置、批注和笔记。另一个Initialize Digital Paper菜单清存储、恢复默认并自动关机，不能与普通复位视为同一动作。','复位实际损失范围、初始化确认、自动备份和恢复均未验证。',['TP0001178304','TP0001178278'],[72,82,93])
]
fields=list(next(csv.DictReader((L/'coverage.csv').open())).keys());flows=[]
for n,task,steps,branches,gaps,keys,pages in data:
 evid=ids(*keys)
 visual='not_observed'
 if n in (13,15):evid+=';E-DPCD-34';visual='static_manual_pages_only'
 flows.append(dict(flow_id=f'F-M059-{n:02}',product_id='M-059',task=task,depth='D2',evidence_ids=evid,known_branches=branches,gaps=gaps,visual_status=visual,hands_on='false',case_path='cases/M-059-sony-dpt.md',checked_on='2026-10-27'))
updates=[];oldF={r['flow_id']:r for r in csv.DictReader((L/'coverage.csv').open()) if r['product_id']=='M-059'}
revisions={
2:('电脑转换为PDF→显示传输状态→完成后设备打开并进Received；单文件≤1GB，传输断连可能损坏。','当前OS兼容、同名冲突和失败恢复未测；旧打印OS与App总体OS列表不同。',ids()),
3:('先显示同文档两份；另一侧快速列表可选最近/同目录资料，列表外不能直接并排。可恢复最近组合；不能上下排或旋转。','相应资料不在列表时要先使其可访问；组合/页码跨重启保留未测。',ids()),
4:('有封面从第2页起，无封面从第1页；双页不可旋转、并排另一文档、建关联笔记、增删笔记页或跨两页连画。','PDF排版/退出页码和设置持久化未测。',ids()),
5:('每原文一份，原目录名加_Note；替换时旧笔记转独立。App移动原文或笔记、改笔记名保持关联；不能在笔记内再建side-note。','原文改名/删除、外部软件修改、复制和恢复后的关联未知。',ids('TP0001178284')),
6:('选区/粘贴模式不可翻页；Copy/Cut结束回文档后再选目标。只粘贴一次，新剪贴覆盖旧内容，重启丢失；换页清撤销。','取消选区/剪切后未粘贴、跨文档及故障恢复未测。',ids('TP0001273124')),
7:('设备书内标记按页合并；全库及PC按文档合并，PC可选择全库/文件夹范围。手写方式影响识别，不是通用手写全文检索。','漏检、匹配形状与命中定位未测；搜索结果退出不保证恢复所有滚动状态。',ids()),
8:('同PDF双改保留两份；只同步指定目录PDF，空PDF目录不参与，最多20目录。PC App可设自动同步间隔，作用于连接使用时。','并发/中断、删除与另一边编辑同时发生的规则未知；不可借QUADERNO规则补全。',ids('TP0001273119','TP0001178281')),
9:('Delete sync setting只去关系、不删两边文件；同步中不可配/删关系。同步目录改名或移动也会删除配置，继续使用须重设。','解除/移动后的回执、重新指定目录的冲突处置未测。',ids('TP0001273119','TP0001178310')),
10:('System storage全选传出是文件复制；每文件≤1GB且断连可能损坏。双击只读临时副本关闭App即删除，不能当完成备份。','设置/历史/关联笔记属性的回导与恢复范围未核。',ids('TP0001178319','TP0001178341'))}
for n,(branch,gap,evid) in revisions.items():
 old=oldF[f'F-M059-{n:02}'];before={k:old[k] for k in ['known_branches','gaps','evidence_ids','checked_on']}
 after=dict(known_branches=branch,gaps=gap,evidence_ids=';'.join(dict.fromkeys(old['evidence_ids'].split(';')+evid.split(';'))),checked_on='2026-10-27')
 updates.append({'flow_id':old['flow_id'],'before':before,'after':after})
pdfrel='work/research/extension-manuals/sony-dpt.pdf'
caps.append({'evidence_id':'E-DPT-01','capture':{'checked_on':'2026-10-27','access_mode':'existing_pdf_reread_and_catalog_reconstructed','capture_url':'https://helpguide.sony.net/dpt/rp1/v1/en/print.pdf','raw_work_path':pdfrel,'raw_sha256':sha(ROOT/pdfrel),'note':'按目录核读正文6–95页；法律/规格仅背景。新增目录与任务不表示当前实机测试；p15/56另静态证据，p61仅work。'}})
# Source notes: concise reusable interpretation; no wholesale manual reproduction.
notes=['# Sony DPT 1.6：本轮来源摘记','','查阅2026-10-27。固定RP1/CP1、指南4-597-942-19(1)；不是当前安装版本。原件留work，以下为研究者概括。',
'','## 获取与读取范围','','复用99页PDF，按目录核读操作和故障正文6–95页，法律/规格作背景。新GET36份HTML均200；最初只抽article漏步骤，离线改为id_content后全部有正文，原始HTML不变。PDF完整目录与网页73条叶入口对应；模板入口重复，72个唯一URL。网页正文抽检文档栏、锁屏、蓝牙、文件管理、橡皮、页管理、临时查看、side-note、更新和只读故障；其余以同版PDF核读为主，不能以抓到HTML充当新实操。',
'','## 静态观察','','实际查看p15、56、61。p15工具栏有历史前后箭头及其页码、页位置条、工具组、底部文件名；不是连续跳转。p56显示笔橡皮键位置，下方电子纸对比图不完整，整笔画擦除结论取文字规则。p61为校准文字步骤，无界面连拍，故只留work。正式归档p15/56。',
'','## 任务定位与状态含义','','| 任务 | 页码 | 本轮概括 |','| --- | --- | --- |']
for n,task,steps,branches,gaps,keys,pages in data:notes.append(f'| F-M059-{n:02} {task} | {", ".join(map(str,pages))} | {branches} |')
notes+=['','## 新网页来源','','| E | 标题 | 定位 |','| --- | --- | --- |']
for e in new[:-1]:notes.append(f'| {e["evidence_id"]} | [{e["title"]}]({e["url"]}) | id_content；对应PDF章节见目录台账 |')
notes+=['','## 限制','','没有运行Digital Paper App、连接设备、配对、上传、删除或复位。当前OS与服务供应、失败回执、速度、触感和备份回导仍未知。旧原文改名与同步删改冲突没有依据，不能用同源后继产品QUADERNO补齐。所有产品启发仍是建议。','']
(A/'source-notes.md').write_text('\n'.join(notes));noterel=str((A/'source-notes.md').relative_to(L));notesha=sha(A/'source-notes.md')
for e in new:
 if not e['visual_observed']:e['archived_artifact']=noterel;e['sha256']=notesha
 else:e['visual_audit_path']=noterel;e['visual_audit_sha256']=notesha
for c in caps:c['capture']['note_artifact']=noterel;c['capture']['note_sha256']=notesha
(B/'sony-dpt-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in new))
csvwrite(B/'sony-dpt-controls-deepening-coverage.csv',flows,fields)
jwrite(W/'existing-coverage-updates.json',updates);jwrite(W/'additional-captures-updates.json',caps)
# Leaf catalog; preserve repeat template entry rather than calling it another function.
toc=json.loads((W/'pdf-toc-links.json').read_text());pagebytitle={x['label']:x['page']+1 for x in toc if x.get('label') and 'page' in x}
page_to_flows={}
for n,*_,pages in data:
 for p in pages:page_to_flows.setdefault(p,[]).append(f'F-M059-{n:02}')
for n,pages in {1:[13,14],2:[25],3:[47],4:[46],5:[65,67],6:[58],7:[31,49,51],8:[28,29],9:[28,29,33],10:[27]}.items():
 for p in pages:page_to_flows.setdefault(p,[]).append(f'F-M059-{n:02}')
inventory=[]
for x in catalog:
 p=pagebytitle.get(x['title']);key=x['key'];e=key_to_id.get(key,'E-DPT-01')
 inventory.append(dict(entry_id=f'M059-DPCD-{len(inventory)+1:03}',entry_type='official_manual_leaf',title=x['title'],source_url=x['url'],locator=f'HTML目录；PDF p{p}' if p else 'HTML目录；PDF对应标题',evidence_ids=';'.join(dict.fromkeys(['E-DPT-01',e])),flow_ids=';'.join(page_to_flows.get(p,[])),review_status='同版PDF正文核读；HTML原件取得' if key in key_to_id else ('法律/规格背景，非功能闭环' if p and p>=96 else 'PDF正文核读，未另取HTML正文'),gap='功能/故障与重复入口混合；无实机；未映射不表示不支持'))
# Each declared setting, including parent items, is a separate inventory row, not a completed flow.
for key,pages in [('TP0001178278','71–72'),('TP0001178281','73–74')]:
 text=(W/(key+'.txt')).read_text().split('Setting items',1)[1]
 for m in re.finditer(r'^\[([^\]\n]+)\](.*)$',text,re.M):
  title=m[1]+m[2];inventory.append(dict(entry_id=f'M059-DPCD-{len(inventory)+1:03}',entry_type='declared_setting_including_parents',title=title.strip(),source_url=next(x['url'] for x in catalog if x['key']==key),locator=f'PDF p{pages}; Setting items',evidence_ids=key_to_id[key],flow_ids='',review_status='设置名称/作用文字已读，非逐控件实操',gap='父子层级/保存反馈/取消和异常未逐项完成'))
csvwrite(B/'sony-dpt-controls-deepening-inventory.csv',inventory,list(inventory[0]))
# Case retains earlier task table; add explicit revised rules without erasing history.
case=L/'cases/M-059-sony-dpt.md';s=case.read_text()
s=s.replace('查阅：2026-10-26。','查阅：2026-10-26；2026-10-27补官方目录及操作/恢复分支。',1)
s=s.replace('## 任务与状态','## 原有任务与本轮修订',1)
# Replace old table branch cells with verified updates; keep earlier steps and source column refreshed.
lines=s.splitlines()
for i,line in enumerate(lines):
 for u in updates:
  if line.startswith('| '+u['flow_id']+' '):
   parts=line.split('|');parts[3]=' '+u['after']['known_branches']+' ';parts[4]=' '+u['after']['evidence_ids'].replace(';','、')+' ';lines[i]='|'.join(parts)
s='\n'.join(lines)+'\n'
s=s.replace('## 静态观察与设计启发','## 新增任务：文件、笔迹与运行状态\n\n以下均为D2文档重建；入口、动作和数据后果有依据，表中缺口不补成事实。\n\n| F | 任务与操作 | 结果和限制 | 仍缺 |\n| --- | --- | --- | --- |\n'+'\n'.join(f'| F-M059-{n:02} {task} | {steps} | {branches} | {gaps} |' for n,task,steps,branches,gaps,keys,pages in data)+'\n\n各任务页码与E见[来源摘记](../artifacts/sony-dpt-controls-deepening/source-notes.md)和[逐项目录](../batches/sony-dpt-controls-deepening-inventory.csv)。\n\n## 静态观察与设计启发')
s=s.replace('未核实原文重命名、删除后关联的全套行为，未测试数据恢复、网络中断、重复导入、复杂 PDF、手写识别和全部设置。已知功能域不等于逐功能完成；当前全部流程为 D2。','原文重命名/删除后的关联、同步删改冲突、备份回导、首次配对每屏、全部设置的反馈与恢复仍未完成。已补官方73条叶入口（72唯一URL）和设置项，目录包含重复、父项和故障，不作为完整功能分母。新静态页仅支持布局；全部27条任务均D2，无设备或PC App操作。')
s+='\n## 2026-10-27补证的设计含义（建议）\n\n- 文档导航历史、手写撤销和文件备份各自有生命周期；AI应读取动作类型和有效条件。\n- 临时只读副本、永久传出文件、画面截图是三个对象；“电脑上看到了”不能作为已备份结果。\n- 笔记属性、原文关联、同步目录关系会受不同动作影响，不能用文件名或页面相似性代替。\n- 当前引用固定1.6手册；现今软件支持、实机数据保留及儿童可用性未由文档证明。\n'
case.write_text(s)
jwrite(W/'build-result.json',dict(new_evidence=len(new),new_flows=len(flows),old_tasks=len(updates),old_fields=sum(len(x['after']) for x in updates),old_captures=len(caps),inventory_rows=len(inventory),archived_images=2,viewed_pages=[15,56,61],actual_operations=0))
print((W/'build-result.json').read_text())
