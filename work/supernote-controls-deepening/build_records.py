from pathlib import Path
import csv, json, re, hashlib
import pymupdf

ROOT=Path('/home/workspace/reader'); BASE=ROOT/'docs/research/interaction-library'
WORK=ROOT/'work/supernote-controls-deepening'; ART=BASE/'artifacts/supernote-controls-deepening'
DATE='2026-10-26'; CASE='cases/M-006-supernote.md'; PREFIX='E-SNCD-'
PDF='https://ib.supernote.com/x2/X2_V3.28.42_EN.pdf'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def relative(p): return str(p.relative_to(ROOT))
snap=json.loads((WORK/'http-snapshots.json').read_text())
descriptions=[
 ('官方当前手册索引', 'https://support.supernote.com/en_US/getting-started/download-manual','official_help','2026-07-01','Manta & Nomad download table; distinct A5 X/A6 X column','索引指向X2 V3.28.42；未把A5 X/A6 X的2.x版本混入。', 'manual-index.html'),
 ('Supernote X2 User Manual V3.28.42',PDF,'official_manual',None,'p2–8 TOC; p9 models; p22–23,50–53,59–61,67–73,84,97–101,112–113,124–128,130–131,135–137,159–162; four viewed pages 73/84/112/160','182页；140目录书签逐项索引。PDF书签4.5指向63而纸面目录及正文64。p98与旧Digest帮助入口文案不同；四摘页仅静态观察。','X2_V3.28.42_EN.pdf'),
 ('To-Do任务创建、管理与回源','', 'official_help','2024-06-26','Create a To-Do task; Edit To-Do tasks','Note图标官方只承诺返回来源笔记；删除恢复及双向改写未知。','03-todo.html'),
 ('分类全局搜索','', 'official_help','2023-10-20','Search by Handwriting/File/Keyword/Star','各类命中返回目标不同；Star按File结果打开最后所读页，不必是星标页。','04-search.html'),
 ('手写识别与后台状态','', 'official_help','2026-02-09','Create/view/export recognition; Important notes','最后触摸后30秒与暂停是文档规则非实测；历史2.7.21沿革不当X2版本号，X2同类操作另核手册。','05-recognition.html'),
 ('标题与链接再次编辑','', 'official_help','2023-10-09','Edit headings; Edit links','再套索编辑结构对象；2023入口与X2指南69/72页互相限定。','06-edit.html'),
 ('工具栏中的导出入口','', 'official_help','2023-11-01','Share/Export row','用来支持笔记PNG/PDF/TXT/DOCX出口；不证明3.31工具栏当前布局。','07-toolbar.html'),
 ('Supernote Cloud自动同步入口','', 'official_help','2024-06-05','Settings > Drive; Auto Sync; progress','账号、云服务与开关条件；下拉看进度，未实测同步。','08-auto-sync.html'),
 ('笔记自动同步触发点','', 'official_help','2024-01-23','Back to Files trigger','历史FAQ将返回Files列为触发点；不推断每笔实时上传或完整最新触发条件。','09-sync-trigger.html'),
 ('X2日志快照：当前云删除/冲突/姿势与历史链接/任务分支','', 'official_changelog','2026-10-23','Chauvet 3.31.45 (2026-10-20) Adjusted/Optimized; 3.25.39 link move; 3.18.29 offline To-Do fix','3.31.45云回收/冲突/八姿势是文档声明；未看对应操作。E-SUP-05同源，本条保留新快照，不作独立验证。','10-changelog.html'),
 ('文件传输：云、USB与局域网','', 'official_help','2026-07-15','Supernote Cloud; USB Type-C; Browse & Access','只读入口资料；未配对、上传、授权或安装MTP工具。','11-transfer.html'),
]
rows=[]
for i,(title,url,typ,updated,locator,note,filename) in enumerate(descriptions,1):
    if i>=3:url=snap[i-3]['url']
    path=WORK/filename
    rows.append(dict(evidence_id=f'{PREFIX}{i:02}',product_id='M-006',title=title,url=url,checked_on=DATE,source_type=typ,
      access_mode='public_http_pdf_text_and_static_pages' if i==2 else 'public_http_text',
      version_scope='A5 X2 Manta / A6 X2 Nomad; V3.28.42 manual baseline; dated help and later changelog qualified separately',
      app_build=None,firmware_version='Chauvet 3.31.45; historical 3.25.39 / 3.18.29' if i==10 else ('V3.28.42 manual' if i in (1,2) else None),
      source_updated_on=updated,locator=locator,note=note,hands_on=False,visual_observed=(i==2),
      archived_artifact=relative(path),sha256=sha(path),capture_state='original_pdf_and_four_viewed_static_pages' if i==2 else ('html_snapshot' if i==1 else 'html_snapshot_and_text'),supersedes=None))
(BASE/'batches/supernote-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows))

def flow(n,task,eids,branches,gaps,visual=False):
 return dict(flow_id=f'F-M006-{n:02}',product_id='M-006',task=task,depth='D2',evidence_ids=';'.join(eids),known_branches=branches,gaps=gaps,
   visual_status='static_reference_only' if visual else 'not_observed',hands_on='false',case_path=CASE,checked_on=DATE)
new=[
 flow(7,'手写变待办并回来源笔记',['E-SNCD-02','E-SNCD-03','E-SNCD-10'],'套索New Task/Add；Note图标回笔记；无日期不列Planned；完成可撤销；Data Sync独立于文件同步；3.18.29历史离线丢任务修复','原句/笔划定位、任务删除恢复、改任务是否影响原笔迹、最新断网序列未验证',True),
 flow(8,'按类别搜索并回到命中内容',['E-SNCD-02','E-SNCD-04','E-SNCD-05','E-SUP-05'],'Handwriting/Keyword回页，File回最后页；Star按File也不保证星标页；后台识别延后及触摸暂停为文档规则','3.24.38后文本搜索完整界面、无结果/误识别修正、实际耗时及重启队列未知'),
 flow(9,'选择笔记页导出交换文件',['E-SNCD-02','E-SNCD-05','E-SNCD-07'],'PNG/PDF与识别TXT/DOCX；标准笔记导出时识别；实时识别语言创建后固定；QR/InkHub另列','输出目录、重名/取消/存储不足、编辑后结构关系往返与原笔迹回写未知',True),
 flow(10,'删除文件前保留副本并区分云回收',['E-SNCD-02','E-SNCD-10','E-SNCD-11'],'Files多选Delete；USB/同WiFi传输和独立副本；3.31.45云同步删除进回收站；V3.28.42非整机备份','本地确认/撤销；云恢复按钮/保留期/永久删除/再次同步；恢复外链和重名规则未知',True),
]
with (BASE/'batches/supernote-controls-deepening-coverage.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(new[0]));w.writeheader();w.writerows(new)

with (BASE/'coverage.csv').open() as f: old={r['flow_id']:r for r in csv.DictReader(f) if r['product_id']=='M-006'}
updates=[
 flow(1,old['F-M006-01']['task'],['E-SUP-01','E-SNCD-02','E-SNCD-06'],'标题可再套索编辑；主图层限制；p131 Erase Star不擦标题内星标；历史帮助称标记删除不可撤销','删除标记后的原笔迹、当前版本恢复/星标识别误触未知'),
 flow(2,old['F-M006-02']['task'],['E-SUP-02','E-SNCD-02','E-SNCD-06','E-SNCD-10'],'p73逐步/一次返回与上半屏上滑；可见主图层；3.25.39移到microSD/OTG后链接保留规则','目标删除/任意改名/云重建稳定性，跳转历史持久化未知',True),
 flow(3,old['F-M006-03']['task'],['E-SUP-03','E-SNCD-02'],'帮助写Recognize as Text，指南p98写Recognize as Digest/Create且p112分列；删分类连删摘录；Digest TXT元数据恢复','入口历史名称或文案错误未确定；导出评论/笔迹/回源关系完整往返未知',True),
 flow(4,old['F-M006-04']['task'],['E-SUP-04','E-SUP-05','E-SNCD-02'],'2024恢复清除后续数据与3.24.38不先清空的版本分支；p160 Digest本地备份与Digest/To-Do Data Sync分开','To-Do同类本地恢复包不能由标题推断；重复恢复/合并去重/取消中断未知',True),
 flow(5,old['F-M006-05']['task'],['E-SUP-06','E-SNCD-08','E-SNCD-09','E-SNCD-10','E-SNCD-11'],'先一端同步再另一端；Drive选目录Save/手动同步；Auto Sync/Back to Files历史触发；3.31.45冲突处理声明','冲突选择及合并规则、最新完整触发条件、断网和两端删除竞态未知'),
 flow(6,old['F-M006-06']['task'],['E-SUP-07','E-SNCD-02','E-SNCD-10'],'两指按住临时工具、松指回原工具；左右手为历史/V3.28.42，3.31.45改八种姿势','新姿势有效区域、误触/手掌拒触/延迟未观察'),
]
suggest=[]
for after in updates:
 before=old[after['flow_id']]
 suggest.append(dict(flow_id=after['flow_id'],changes={k:dict(before=before[k],after=v) for k,v in after.items() if before[k]!=v},replacement_row=after))
(WORK/'old-coverage-updates.json').write_text(json.dumps(suggest,ensure_ascii=False,indent=2)+'\n')

doc=pymupdf.open(WORK/'X2_V3.28.42_EN.pdf'); toc=doc.get_toc()
links={
 '2.2':('F-M006-06','E-SUP-07;E-SNCD-10','姿势配置更新后的详细区域未覆盖'),
 '3.6':('F-M006-05','E-SNCD-08;E-SNCD-09;E-SNCD-11','第三方云与私有云只查目录/入口，不作完整任务'),
 '4.1':('F-M006-08;F-M006-09','E-SNCD-05','仅识别笔记类型与语言条件'),
 '4.2':('F-M006-08;F-M006-09','E-SNCD-05','识别错误更正及重启恢复未覆盖'),
 '4.7':('F-M006-01','E-SUP-01;E-SNCD-06','删除后原笔迹与恢复未知'),
 '4.8':('F-M006-01','E-SUP-01','关键词误识别/删除关联未知'),
 '4.9':('F-M006-02','E-SUP-02;E-SNCD-06;E-SNCD-10','文件删除/任意移动改名的边界未完'),
 '4.14':('F-M006-09','E-SNCD-05;E-SNCD-07','输出错误与结构往返未知'),
 '6.1':('F-M006-03','E-SUP-03','官方帮助Text与手册Digest文案冲突保留'),
 '6.2':('F-M006-03','E-SUP-03','回源失效及删除恢复未知'),
 '6.3':('F-M006-03;F-M006-04','E-SUP-03','仅TXT元数据导入；非完整备份恢复证明'),
 '6.4':('F-M006-03','E-SUP-03','评论/笔迹/来源往返未知'),
 '6.5':('F-M006-03','E-SUP-03','仅评论类型，完整编辑状态未完'),
 '9.1':('F-M006-07','E-SNCD-03','仅保证回笔记，不保证回原笔划'),
 '9.2':('F-M006-07','E-SNCD-03;E-SNCD-10','任务删除恢复及最新离线异常未知'),
 '14.1':('F-M006-10','E-SNCD-11','仅删除和文件副本，重命名/排序/视图未拆'),
 '14.4':('F-M006-01','E-SUP-01','Erase Star标题例外，误识别恢复未知'),
 '14.6':('F-M006-10','E-SNCD-11','无实传；中断/权限/同名未知'),
 '14.8':('F-M006-08','E-SNCD-04;E-SNCD-05','新版本文本类别界面与无结果反馈未覆盖'),
 '17.1':('F-M006-10','E-SNCD-11','仅文件备份通道，不作连接兼容实测'),
 '17.5':('F-M006-04;F-M006-10','E-SUP-04;E-SUP-05','按数据分类备份；完整还原/去重未知'),
 '17.6':('F-M006-10','','仅破坏范围边界；未实施重置或重建后续设置'),
 '19.2':('F-M006-05;F-M006-10','E-SNCD-08;E-SNCD-09;E-SNCD-10;E-SNCD-11','云端界面未访问，回收站恢复缺口'),
}
domains={1:'首次使用',2:'手势工具栏',3:'基本设置',4:'笔记',5:'阅读',6:'Digest',7:'TXT',8:'Word',9:'To-Do',10:'Atelier',11:'InkFlow',12:'InkHub',13:'NetVirtualDisk',14:'便利功能',15:'邮件',16:'日历',17:'系统基本操作',18:'隐私安全',19:'云服务',20:'安全说明',21:'FAQ排错'}
inventory=[]; current='前置说明'
for order,(level,title,page) in enumerate(toc,1):
 title=title.strip(); ch=re.match(r'Chapter\s+(\d+)',title)
 if ch:current=domains[int(ch[1])]
 m=re.match(r'(\d+\.\d+(?:\.\d+)?)',title); section=m[1] if m else ''
 related,extra,gap=links.get(section,('','','仅登记目录，未重建该项操作/反馈/异常'))
 status='partially_documented' if related else ('chapter_index_only' if ch else 'inventory_only')
 if title in ('Table of Contents','Applicable Models','Copyright'):status='non_flow_reference';gap='目录/型号/版权参考，不计功能任务'
 if title.startswith('Q：What should I do'):title='Q：What should I do when the device freezes or fails to boot?'
 if title.startswith('Q：How to eliminate'):title='Q：How to eliminate afterimages on note pages or display screen?'
 if title.startswith('Q：How to import'):title='Q：How to import document customized fonts'
 note='书签页码与印刷页码对应；'+('该起始页图已实际查看' if page in (73,84,112,160) else '未实际查看此起始页图')
 if section=='4.5':note='PDF书签63；纸面目录/正文64。63为封面设置尾页，未静默统一。'
 inventory.append(dict(order=order,level=level,section=section or title,manual_title=title,domain=current,pdf_bookmark_page=page,
  printed_start_page=64 if section=='4.5' else page,version_scope='A5 X2 Manta / A6 X2 Nomad; manual V3.28.42',coverage=status,
  flow_ids=related,evidence_ids='E-SNCD-02'+(';' + extra if extra else ''),gap=gap,page_note=note))
with (ART/'functional-inventory.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(inventory[0]));w.writeheader();w.writerows(inventory)
assets=[]
for n in [73,84,112,160]:
 p=ART/f'x2-v3.28.42-p{n}.png';assets.append(dict(path=relative(p),sha256=sha(p),source=PDF,pdf_page=n,printed_page=n,visual_observed=True,hands_on=False,evidence_id='E-SNCD-02',note='实际view_image单帧，非连续流程'))
manifest=dict(checked_on=DATE,model_scope=['A5 X2 Manta','A6 X2 Nomad'],manual=dict(url=PDF,path=relative(WORK/'X2_V3.28.42_EN.pdf'),http_status=200,bytes=(WORK/'X2_V3.28.42_EN.pdf').stat().st_size,sha256=sha(WORK/'X2_V3.28.42_EN.pdf'),pages=182,toc_rows=len(toc)),assets=assets,new_evidence=len(rows),new_flows=len(new),old_flow_updates=len(suggest),hands_on=False)
(WORK/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(evidence=len(rows),new_flows=len(new),old_updates=len(suggest),toc_rows=len(toc),images=len(assets)),ensure_ascii=False))
