from pathlib import Path
import json,csv,hashlib,shutil,sys
sys.path.insert(0,'work/research/tools/pymupdf')
import pymupdf
W=Path('work/casio-controls-deepening'); L=Path('docs/research/interaction-library'); B=L/'batches'; A=L/'artifacts/casio-controls-deepening'; A.mkdir(parents=True,exist_ok=True)
DATE='2026-10-27'; URL='https://www.casio.com/content/dam/casio/global/support/manuals/exword/pdf/003-zh-cn/e/E-W_series_full_CN.pdf'
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def dumpcsv(p,rows,fields):
 with p.open('w',newline='',encoding='utf-8') as f:
  w=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');w.writeheader();w.writerows(rows)
raw=W/'E-W_series_full_CN-MA2307-A.pdf';shutil.copyfile('work/dictionary-research/casio-full.pdf',raw);d=pymupdf.open(raw)
# Re-read only the saved manual, not its current online endpoint.
pages=sorted(set(list(range(32,40))+list(range(77,90))+list(range(108,124))+list(range(132,174))+list(range(175,183))+list(range(194,199))+list(range(220,232))+list(range(234,245))+list(range(249,252))))
(W/'selected-pages.txt').write_text('\n'.join(f'=== PDF/印刷页 {n} ===\n'+d[n-1].get_text() for n in pages))
for n in [87,117,121]:shutil.copyfile(W/f'page-{n}.png',A/f'manual-p{n}.png')
entries=[
('01','手写识别、候选纠错与输入限制','84–89；225、227、236、241、249–251',[87],'F-M024-10','识别候选和无词条分开；手动/自动模式；具体构建未知'),
('02','随记及学习簿编辑、删除与容量','140–144；153–158；164–173',[156,171],'F-M024-11','随记空白/词典页；卡片保存与删除补核；171页删除标签/范围矛盾保留'),
('03','词条便笺的保存、找回与分类','142–144；159–163',[161],'F-M024-12','便笺随词条；颜色文件夹及全色范围；不推断删除联动'),
('04','内置文学阅读定位、书签及型号条件','178–182；195–198',[197],'F-M024-13','世界文学两机均收录；对照名著仅E-W220；书签未外推至所有内容'),
('05','通用发音和临时录音对比','108–119；220–222；239–240',[116,117],'F-M024-14','退出对比删除录音；不是发音评分或持久演讲录音'),
('06','我的演讲：录制、保存、回放、删除','120–123',[121,123],'F-M024-15','五条录音；超规定时长15秒停；覆盖确认；不保存退出'),
('07','系统复位、设置初始化及异常恢复边界','34–36；224–231；234–244',[34],'F-M024-16','复位须先关机；设置初始化有例外；不证明学习簿保留或整机恢复'),
('08','完整PDF书签目录与两机收录表','1；4–8；178–182',[],'','113条PDF书签含父子项和非功能页；型号内容表另列，目录不等于完成任务')]
# Short notes and visual audit; original manual stays in work.
notes=['# CASIO E-W100/E-W220 本轮来源定位与静态观察\n',f'查阅 {DATE}。唯一原始出版物为[MA2307-A中文详细手册]({URL})，封面明确2023年7月；257页，PDF页与印刷页一致。复用2026-10-26已取得文件，未重新联网。现固件未知；不混日版XD、E-B或TextLoader。完整原件：`{raw.as_posix()}`；SHA-256 `{sha(raw)}`。PDF元数据创建2023-07-19、修改2023-07-24仅文件属性，不作固件日期。\n',
'下面是研究定位，不复制各章全文。全部为文档重建，图页仅静态检查；无实体设备、连续录像或实际按键结果。\n']
for eid,title,loc,vis,flow,note in entries:
 notes.append(f'## E-CACD-{eid} — {title}\n\n- 定位：PDF/印刷页 {loc}；关联 {flow or "功能目录"}。\n- 范围：{note}。\n- 原文取得：同一已保存官方PDF，以PyMuPDF提取文本；按键图标需结合下列静态观察。\n')
notes+=['## 静态图逐页分工\n\n2026-10-27已逐张用 `view_image` 查看以下十页。只正式保留87/117/121三页，以说明候选纠错和两种录音生命周期；其余以观察记录和原件页码定位。未查看的手册图片不计观察。\n',
'| 页 | 所见与用途 | E / F | 图件与SHA-256 |\n| --- | --- | --- | --- |\n']
seen={34:'背面RESET孔及附带笔示意；未操作复位',87:'点误识别字→候选区域示意；用于纠错入口，不代表现场识别',116:'录音停止为方块或输入键；正在录音条',117:'退出键停止播放，X关闭对比；文字说明关闭后删录音',121:'演讲结束的播放/重录/保存后退出/退出分支；停止可用输入或退出键',123:'演讲暂停后退出键结束；清单删除键进入删除录音数据',156:'随记缩略图、上一页下一页、退出键关页',161:'颜色/全色文件夹、便笺预览、X关闭便笺',197:'书签删除使用实体删除键；全部删除需确认',171:'卡片删除表末行标签指当前文件夹，说明却指全部；已看图确认不是文本提取错位'}
eby={34:'07 / 16',87:'01 / 10',116:'05 / 14',117:'05 / 14',121:'06 / 15',123:'06 / 15',156:'02 / 11',161:'03 / 12',197:'04 / 13',171:'02 / 05'}
for n,desc in seen.items():
 p=A/f'manual-p{n}.png' if n in [87,117,121] else W/f'page-{n}.png'
 link=f'[正式摘页](manual-p{n}.png)' if n in [87,117,121] else f'临时渲染 `{p.as_posix()}`（未正式归档图片）'
 notes.append(f'| {n} | {desc} | E-CACD-{eby[n].split(" / ")[0]} / F-M024-{eby[n].split(" / ")[1]} | {link}；`{sha(p)}` |\n')
notes += ['\n## 未决与口径\n\n满额仅核到分对象容量、数量显示、已有删除/覆盖操作；没有找到统一“满额”提示或自动淘汰规则，不能由容量推导强制覆盖。复位、设置初始化、合盖续用和已保存数据恢复分别处理。手册171页背诵卡删除表末行标签与描述范围不一致，保留冲突，不据此指定全局删除按钮。2026-12-31 17:00下载服务停止条件沿用E-CASIO-10/11，本轮未重查网络公告，不能据本地手册更新该日期。\n']
(A/'source-notes.md').write_text('\n'.join(notes))
evs=[]
for eid,title,loc,vis,flow,note in entries:
 ep=dict(evidence_id='E-CACD-'+eid,product_id='M-024',title=title,url=URL,checked_on=DATE,source_type='official_manual',access_mode='existing_downloaded_pdf_text_and_selected_static_pages' if vis else 'existing_downloaded_pdf_outline_and_text',version_scope='China E-W100/E-W220; MA2307-A July 2023; model/content conditions as stated; current firmware unknown',app_build=None,firmware_version=None,source_updated_on='2023-07',locator='PDF/印刷页 '+loc,note=note,hands_on=False,visual_observed=bool(vis),visual_kind='static_manual_pages' if vis else None,visual_audit_path='artifacts/casio-controls-deepening/source-notes.md' if vis else None,visual_audit=('已查看静态页 '+','.join(map(str,vis))+'；非连续操作') if vis else None,archived_artifact='artifacts/casio-controls-deepening/source-notes.md',sha256=sha(A/'source-notes.md'),capture_state='archived_short_notes_and_selected_static_pages',raw_work_path=raw.as_posix(),raw_sha256=sha(raw),original_acquisition_date='2026-10-26',rechecked_saved_file_on=DATE,network_refetched=False,supersedes=None)
 formal=[f'artifacts/casio-controls-deepening/manual-p{n}.png' for n in vis if n in [87,117,121]]
 ep['rendered_artifacts']=formal;ep['artifact_hashes']={p:sha(L/p) for p in formal};ep['observed_pages']=vis
 evs.append(ep)
(B/'casio-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in evs))
flows=[
(10,'手写识别、候选纠错后查词','E-CACD-01','手动/自动识别、候选无正确字则删写、识别成功但词典无资料、触屏位置校准','精度时延未测；无候选时显示与草稿中断保存未知'),
(11,'创建、找回和修改手写随记','E-CACD-02','空白页/词典页两入口、缩略图、编辑保存、单页与批量删除、功能入口不能改名','6本各50页；满额提示、未保存退出、撤销与外部恢复未知'),
(12,'给词条加便笺并按颜色找回','E-CACD-03','保存回词条、颜色/全色筛选、修改、单条/文件夹/全部删除、与单词本组合','一词条一张、共300；满额提示、无保存关闭、跨对象删除联动未知'),
(13,'内置文学阅读、书签和阅读位置','E-CACD-04','按作者/作品/词量进入、最近十本与删除、书签保存跳转/删除、首尾和上次页面','仅两机世界文学；E-W220对照名著另支；未保存退出、字体变化定位稳定性与外部书未知'),
(14,'听词条并录音对比后结束','E-CACD-05','真人语音不可用、2分钟自动停止、输入键停止、重录、对比结束删临时录音、拔耳机停播','不是评分；意外掉电数据/合盖对比录音中断未知'),
(15,'录制并保存演讲后回放清理','E-CACD-06','1至5分钟设置、超时15秒停止、重录/放弃确认、五槽覆盖确认、位置拖动、播放结束回开头','删除范围与确认细节未给；满槽其他提示、误删/覆盖恢复、导出未知'),
(16,'设备异常后复位并重新完成设置','E-CACD-07','先关机再RESET、语言/日期设置可略过日期、保护膜提示、设置初始化例外','关闭电源后复位是否保全学习簿未知；不是备份恢复或清除全部数据')]
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
rows=[dict(zip(fields,[f'F-M024-{i:02}', 'M-024',t,'D2',e,b,g,'static_manual_pages_only','false','cases/M-024-casio.md',DATE])) for i,t,e,b,g in flows]
dumpcsv(B/'casio-controls-deepening-coverage.csv',rows,fields)
# Outline and per-model content entries, explicitly not tasks.
outline=json.loads((W/'pdf-outline.json').read_text()); inv=[];parents={}
known={34:('F-M024-16','E-CACD-07'),35:('F-M024-16','E-CACD-07'),37:('F-M024-09','E-CASIO-01'),38:('F-M024-09','E-CASIO-01'),40:('F-M024-01','E-CASIO-02'),41:('F-M024-01','E-CASIO-02'),42:('F-M024-01','E-CASIO-12'),43:('F-M024-10','E-CACD-01'),49:('F-M024-01','E-CASIO-03'),51:('F-M024-01','E-CASIO-03'),53:('F-M024-01','E-CASIO-02'),54:('F-M024-01','E-CASIO-03'),57:('F-M024-01','E-CASIO-04'),58:('F-M024-04','E-CASIO-05'),59:('F-M024-02','E-CASIO-04'),60:('F-M024-01','E-CASIO-04'),65:('F-M024-01','E-CASIO-04'),74:('F-M024-03','E-CASIO-08'),77:('F-M024-02','E-CASIO-01'),84:('F-M024-10','E-CACD-01'),88:('F-M024-10','E-CACD-01'),91:('F-M024-02','E-CASIO-01'),108:('F-M024-14','E-CACD-05'),120:('F-M024-15','E-CACD-06'),122:('F-M024-15','E-CACD-06'),132:('F-M024-08','E-CASIO-01;E-CASIO-10;E-CASIO-11'),135:('F-M024-08','E-CASIO-01'),136:('F-M024-08','E-CASIO-01;E-CACD-07'),140:('F-M024-04;F-M024-05;F-M024-11;F-M024-12','E-CASIO-09'),142:('F-M024-11;F-M024-12','E-CACD-02;E-CACD-03'),145:('F-M024-04','E-CASIO-05'),153:('F-M024-11','E-CACD-02'),159:('F-M024-12','E-CACD-03'),164:('F-M024-05','E-CASIO-06'),175:('','E-CACD-08'),176:('','E-CACD-08'),177:('','E-CACD-08'),178:('','E-CACD-08'),220:('F-M024-14','E-CACD-05'),221:('F-M024-14','E-CACD-05'),223:('F-M024-10;F-M024-16','E-CACD-01;E-CACD-07'),224:('F-M024-10;F-M024-16','E-CACD-01;E-CACD-07'),231:('F-M024-16','E-CACD-07'),234:('F-M024-02','E-CACD-07'),237:('F-M024-08;F-M024-09;F-M024-14;F-M024-16','E-CACD-07'),242:('F-M024-08;F-M024-16','E-CACD-07'),249:('F-M024-10','E-CACD-01')}
for i,(level,title,page) in enumerate(outline,1):
 iid=f'M024-T{i:03}';parent=parents.get(level-1,'');parents[level]=iid
 f,e=known.get(page,('','E-CACD-08'));isgroup=level==1 or (i<len(outline) and outline[i][0]>level)
 status='parent_or_frontmatter' if level==1 else ('partial_task_documented' if f else 'located_not_task_analyzed')
 inv.append(dict(item_id=iid,kind='pdf_outline',parent_id=parent,level=level,title=title.strip(),model_scope='E-W100/E-W220；具体内容另表',source_url=URL,locator=f'PDF/印刷页 {page}',evidence_ids=e,flow_ids=f,status=status,gaps='父项/子项可能重叠，不作功能完成数；关联F仅覆盖其已写步骤' if isgroup else ('见关联F缺口；未实操' if f else '已取得对应PDF正文，尚未逐步分析'),checked_on=DATE))
# Exact order and categories transcribed from pages178–182, not inferred from marketing counts.
common=['英汉大词典（第二版）','汉英大词典（第三版）','牛津高阶英汉双解词典（第九版）','朗文当代高级英语辞典（第五版）','柯林斯COBUILD英汉双解词典','韦氏英语大学辞典']
practice=['牛津英语同义词学习词典','牛津英语搭配词典（英汉双解第二版）','牛津短语动词词典']
words=['高中英语课标3000词','英语高考3500词','中高考词汇同步练']
zh=['不列颠简明百科全书','现代汉语大词典','现代汉语学习词典','古代汉语词典（第二版）','新华成语词典（第二版）']
tools=['基本操作指南','我的演讲','日历/时钟','计算器','学习簿','我的书库']
modeldata={
'E-W100':[(178,'英语综合',common),(178,'实用英语',practice),(178,'词汇学习',words),(179,'视听练习',['Oxford iSpeaker']),(179,'写作素材',['初中英语三维词汇学习手册','高中英语三维词汇学习手册']),(179,'世界文学',['世界文学作品500篇']),(179,'汉语',zh),(179,'便捷查询',['汉语例句查询']),(179,'工具',tools)],
'E-W220':[(180,'英语综合',common+['新牛津英语大词典']),(180,'实用英语',['牛津同义词大词典']+practice[:2]+['牛津英语联想词典','牛津英语习语词典',practice[2],'牛津英美文化词典','英语搭配大辞典']),(180,'词汇学习',words+['新东方TOEFL词汇','新东方雅思词汇','新东方GRE词汇','新东方SAT词汇','新东方四级词汇','新东方六级词汇','高中英语三维词汇学习手册']),(181,'专业英语',['牛津英汉双解商务英语词典','牛津商务管理词典','牛津会计词典','牛津市场营销词典','牛津金融与银行词典','牛津生物学词典','元照英美法词典']),(181,'留学英语',['Collins - Presenting','Collins - Group Work','SAT阅读','SAT阅读背景：美国的历史','新东方TOEFL写作/口语','新东方雅思写作','新东方雅思口语','柯林斯多国语（英美中）']),(181,'文学・演说',['英语夜读15分钟・科林','英语夜读15分钟・经典','英语夜读15分钟・文化','英语夜读15分钟・时文','英语名演说・名台词集','世界文学作品500篇','译文经典名著【中英（法）对照】','上下五千年']),(182,'英语会话',['Oxford iSpeaker','发音练习','海外医疗会话集','独行侠英语口语手册-洽谈生意','独行侠英语口语手册-海外求学','英语口语900句']),(182,'汉语',zh+['唐诗三百首鉴赏辞典','宋词三百首鉴赏辞典']),(182,'便捷查询',['汉语例句查询']),(182,'工具',tools)]}
for model,groups in modeldata.items():
 count=0
 for page,category,items in groups:
  for title in items:
   count+=1;f='';e='E-CACD-08';status='model_index_only';gap='仅核收录；尚未分析该内容实际操作或功能支持矩阵'
   if title in ['世界文学作品500篇','译文经典名著【中英（法）对照】']:f='F-M024-13';e+=';E-CACD-04';status='partial_task_documented';gap='仅所写阅读/对照分支；书签不跨内容泛化'
   elif title in words:f='F-M024-07' if title==words[2] else 'F-M024-06';e+=';E-CASIO-07';status='prior_partial';gap='仅旧F训练分支；内容题库未全部分析'
   elif title=='我的演讲':f='F-M024-15';e+=';E-CACD-06';status='partial_task_documented';gap='删除确认/恢复与导出未知'
   elif title=='学习簿':f='F-M024-04;F-M024-05;F-M024-11;F-M024-12';e+=';E-CASIO-09';status='partial_task_documented';gap='四种对象容量已核，满额提示/备份恢复未知'
   elif title=='我的书库':f='F-M024-08';e+=';E-CASIO-01';status='prior_partial';gap='下载任务受2026停服时点约束；不证明任意文件导入'
   inv.append(dict(item_id=f'M024-{model}-{count:02}',kind='model_content_index',parent_id='M024-T088',level=3,title=title,model_scope=model+' / '+category,source_url=URL,locator=f'PDF/印刷页 {page}',evidence_ids=e,flow_ids=f,status=status,gaps=gap,checked_on=DATE))
 print(model,count)
dumpcsv(B/'casio-controls-deepening-inventory.csv',inv,list(inv[0]))
# Stable old tasks: suggestions only. Never write old batches or masters.
old=json.loads((W/'existing-coverage-before.json').read_text());old={r['flow_id']:r for r in old}
updates={
2:dict(evidence_ids=old['F-M024-02']['evidence_ids']+';E-CACD-07',known_branches='模糊/通配符及SHIFT；无命中检查连字符/空格与&；问号星号不能混用，HOME多词典部分不可用',gaps='完整无命中界面和跳查返回栈未观察；归一化规则依查询类型'),
4:dict(known_branches='色标/CHECK可切换；12本各300条、每词10处；单条直删与批量确认；便笺可同词组合',gaps='任意外部原句、满额提示、误删撤销和组合删除联动未知',evidence_ids=old['F-M024-04']['evidence_ids']+';E-CACD-03'),
5:dict(evidence_ids=old['F-M024-05']['evidence_ids']+';E-CACD-02',visual_status='static_manual_pages_only',known_branches='看/听与返回原文；本机6本各100卡；语音单独保存后仍须保存整卡；编辑保存及单卡/批量删除',gaps='满额/撤销/导出/调度未知；手册171页最后删除选项标签与说明范围冲突，未选定其实际行为'),
8:dict(evidence_ids=old['F-M024-08']['evidence_ids']+';E-CACD-07',known_branches='网络重搜及2026-12-31 17:00停下载；卡读失败先回书库关机、拔卡等约10秒重插；2至3次仍失败换卡',gaps='下载失败/导入仍未知；他机认证卡不等于可迁移备份；没有把换卡当数据恢复'),
9:dict(evidence_ids=old['F-M024-09']['evidence_ids']+';E-CACD-07',known_branches='语音播放器合盖续播且播放时不自动关机；无线连接时自动关机为6分钟；低电量可退到语言设置',gaps='未保存内容与异常掉电恢复未知；合盖续用不等于复位或备份恢复')}
changes=[]
for n,fields2 in updates.items():
 fid=f'F-M024-{n:02}';fields2['checked_on']=DATE
 for k,v in fields2.items():
  if old[fid][k]!=v:changes.append(dict(flow_id=fid,field=k,before=old[fid][k],after=v,reason='2026-10-27复核同一MA2307-A正文；补旧任务分支，不增加重复F或升深度'))
(W/'existing-coverage-updates.json').write_text(json.dumps(changes,ensure_ascii=False,indent=2)+'\n')
dumpcsv(W/'existing-coverage-updates.csv',changes,list(changes[0]))
print(json.dumps({'E':len(evs),'F':len(rows),'inventory':len(inv),'old_fields':len(changes)},ensure_ascii=False))
