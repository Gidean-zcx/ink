from pathlib import Path
import csv,json,hashlib,shutil,re
import pymupdf
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library';work=root/'work/office-aipaper-deepening'
art=lib/'artifacts/office-aipaper-deepening';art.mkdir(parents=True,exist_ok=True)
date='2026-10-26'
xurl=json.loads((work/'x2-download.json').read_text())['url'];vurl=json.loads((work/'viwoods-download.json').read_text())['url']
E=[]
def evidence(id,pid,title,url,locator,note,pdf,scope,page=None):
 raw=work/(pdf+'.pdf');r=dict(evidence_id=id,product_id=pid,title=title,url=url,checked_on=date,source_type='official_manual',access_mode='public_http200_pdf_text_read' if page is None else 'official_pdf_page_render_actually_viewed',version_scope=scope,app_build=None,firmware_version=None,source_updated_on=None,locator=locator,note=note,hands_on=False,visual_observed=page is not None,archived_artifact=None,sha256=None,capture_state='bibliography_and_short_note_only',supersedes=None,raw_work_path=str(raw.relative_to(root)),raw_sha256=hashlib.sha256(raw.read_bytes()).hexdigest())
 if page:
  src=work/f'{pdf}-file-{page:02}.png';dst=art/src.name;shutil.copyfile(src,dst);r.update(archived_artifact=str(dst.relative_to(lib)),sha256=hashlib.sha256(dst.read_bytes()).hexdigest(),capture_state='local_pdf_page_render_actually_viewed')
 E.append(r)
xs='China iFLYTEK Office X2; historical manual, no stated firmware revision; PDF metadata 2020-11-05 is not publication proof'
vs='Viwoods AiPaper English historical 70-page manual; no stated firmware build; PDF metadata 2025-04-30, not V3.15 manual'
evidence('E-IXCD-01','M-009','X2官方54页完整手册',xurl,'PDF54文件页；封面、目录i–iii、正文1–50','原PDF公开HTTP200；全文提取阅读，封面已看。元数据和示意图均指向历史材料，不据此认定发布日期或当前固件。官方服务中心另有X2使用指南，但该链接404，未将其说成PDF回链。','X2-manual',xs)
for i,page,title,note in [(2,18,'记录进行中布局','印刷14；录音状态、完成、多人/专注模式与书写区静态可见，未点按。'),(3,21,'记录完成后回听与编辑','印刷17；播放、转写编辑、继续录制、替换、查看全文。正文描述点文字定位原声；图不是连续操作。'),(4,22,'关键词回溯与断网补转写','印刷18；搜索结果图与离线录音上传后再更新转写的文字步骤。离线录制不等于本地识别。'),(5,23,'笔记加密改变云副本','印刷19正文：加密阻止上传，已同步副本将删除；未验证服务实际执行与删除范围。'),(6,39,'听书页静态控制','印刷35；主播、语速、章节、播放、音量、定时入口；退出/后台断点未知。'),(7,48,'账号与自动上传条件','印刷44；注销在本文语境为退出账号，历史笔记隐藏至重新登录；自动上传在编辑退出后触发。')]:
 evidence(f'E-IXCD-{i:02}','M-009',title,xurl,f'印刷{page-4}；PDF文件{page}',note,'X2-manual',xs,page)
evidence('E-VICD-01','M-007','AiPaper官网所链原版手册',vurl,'官方User Manuals→AiPaper English；70文件页；正文1–69','web因文件过大未解析，普通HTTP取得原件。SHA256等于先前device.report镜像URL标识；这是原件可取得的补证，不称新修订或独立交叉验证。文本编码异常处只凭实际查看图片判断。','Viwoods-AiPaper-manual',vs)
for i,page,title,note in [(2,2,'AiPaper手册17章目录','实际看目录，仅标手册章节范围，不当17项独立功能或当前完整目录。'),(3,38,'Daily月历和任务面板','印刷37；任务勾选框、Notes、事件与笔记反馈标记可见，不能推定完成、跨日迁移的结果。'),(4,39,'Daily事件创建编辑','印刷38；新建事件的名称、全天、起止、备注、Cancel/Confirm；已有事件编辑/删除入口。未触发取消。'),(5,40,'Daily外部日历同步','印刷39；Outlook/Google绑定与手动同步入口；解除绑定仅保留本地创建事件是手册规则，未实测。'),(6,53,'第三方云登录与导入','印刷52；首个账号默认同步账号，可更改；长按文件Import并选设备目录。'),(7,54,'云导出格式与回导限制','印刷53；PDF/note格式和目标目录；note仅AiPaper可打开编辑且各应用格式不互通。不是所有文件均可恢复原生编辑状态。'),(8,55,'第三方云同步设置','印刷54；主页短按同步、长按云界面，Settings选择账号/目标目录/格式；没有双向冲突或删除传播规则。')]:
 evidence(f'E-VICD-{i:02}','M-007',title,vurl,f'印刷{page-1}；PDF文件{page}',note,'Viwoods-AiPaper-manual',vs,page)
(lib/'batches/office-aipaper-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in E))
# Each tuple: number, task, preconditions, actions, state/branch, gap, evidence, depth.
xflows=[
(9,'新建写作并用标题找回页面','历史手册；桌面笔记','写作→手写；标题旁目录→本页插入标题；双击修改、长按删除，点标题跳页；✔保存返回。','首页标题不能删；菜单搜索覆盖文本及手写转写结果，可定位页。','自动保存时机、识别错字、删标题是否影响链接、误删页恢复。','E-IXCD-01','D2'),
(10,'输入文字并修正识别结果','写作→插入→文本','默认语音键盘，可暂停/继续；切手写后选候选字纠错。笔势圈选后键盘替换，划线删除、尖角插入等。','自由笔迹、识别文字及键盘输入是不同内容状态；圈选删除和换行有不同结果。','误判笔势撤销、光标焦点、离线语音和网络故障。','E-IXCD-01','D2'),
(11,'写作中插入原声再继续录制','写作页；云转写需网络','菜单→录音出现录音条→完成；播放后可拖进度或前后10秒；继续录制；云端转写后查看文字。长按删除录音块。','完成录音与保存整篇分开；与会议记录模式出口不同。','删除块是否同步删原音/转写、翻页后继续录制指向哪段、跨app中断。','E-IXCD-01;E-IFLYO-01','D2'),
(12,'记录会议、回听纠错并补转断网部分','历史X2手册；开始实时转写需网络','桌面记录即开始录音转写，可标记/手写/隐藏转写；完成后点文字或笔迹回听，转写编辑/替换纠错，继续录制追加。断网录音继续，完成后WiFi下点离线录音转写，上传处理完再点更新转写内容。','多人模式仅智能识别可用；语言及词库选择持久保存。专注模式隐藏转写。断网补转依赖云上传，不证明离线本地识别。','修改后时间对齐、重复更新覆盖、上传失败重试、强退恢复、说话人改名精确入口。','E-IXCD-01;E-IXCD-02;E-IXCD-03;E-IXCD-04','D2'),
(13,'为笔记加密并恢复访问','写作或记录；账号/短信恢复条件','菜单→笔记加密，首次设独立笔记密码；打开时密码或已设指纹验证；忘记密码用短信验证码重置；密码管理可关闭加密。','手册规定停止上传且删除已有云副本；笔记密码不同于锁屏密码。','离线时云删除何时执行、删除哪些副本、取消加密后重传、验证码失效及分享链接是否撤销。','E-IXCD-01;E-IXCD-05','D2'),
(14,'阅读、跳转与保留段落标记','已入库的电子书','点中部显示导航；进度跳页/目录跳章；书签按钮添加再点删除。长按段落→划重点或摘录笔记；更多→笔记列表回看/删除。','PDF进度显示缩略图，重排格式松开滑块后跳转；摘录显示下划线，点回手写笔记。','临时跳转返回栈、重排后锚点、删书与批注关系。手册左右滑动方向表述需实机确认。','E-IXCD-01','D2'),
(15,'从阅读切入听书并回到原文','书籍支持朗读；实际格式/网络条件未核定','中部导航→听书；选择主播/语速、音量、定时；调本章进度、跳章、暂停；返回到阅读页。','手册说明听书模式不能批注，需先暂停；返回阅读没有证明停止音频或保存断点。','当前语音/离线包、锁屏及退出后续播、计时选项、睡眠/来电打断。','E-IXCD-01;E-IXCD-06','D2'),
(16,'调整PDF视图与批注能力','PDF；历史手册','更多→裁切→框选→确认作用整本；放大镜后双指缩放，100%或关闭退出；右上切省电/性能。','省电模式不支持批注；PDF对比度/加粗不同于可重排格式的字号。','取消裁切/复原、原PDF是否改写、批注锚点、模式切换保存与真实残影。','E-IXCD-01','D2'),
(17,'切换账号与处理离线笔记','设置→账号；历史手册','注销退出当前账号，原账号笔记隐藏；登录后可选择是否合并登录前离线数据，重登原账号恢复可见。','菜单称注销但文意是登出；旧账号设置恢复出厂模式，不等于整机恢复出厂或删除账号。','两账号同名数据冲突、合并后归属、共享文件隔离、云端退出状态。','E-IXCD-01;E-IXCD-07','D2'),
(18,'编写邮件、留草稿和处理附件','邮件账号已配置；历史授权步骤','写邮件→手写或键盘输入；未发送退出保存草稿，点废弃丢弃内容。收到附件可下载后预览，附件管理长按删除；邮件删除进入已删除目录。','草稿、废弃、删邮件与删下载附件分属不同对象；已删除目录支持彻底删除。','发送失败/重试重复、服务器与设备删除传播、恢复邮件入口；QQ/网易/iCloud当前授权流程未核。','E-IXCD-01','D2'),
(19,'按日期找回笔记并写日程','桌面日历','点日历格子查看当天笔记；在日程便签区书写，翻页继续。','日历是笔记日期索引及手写日程，不证明已具备结构化提醒或待办完成状态。','跨日移动、提醒、同步到外部日历、误删恢复。','E-IXCD-01','D2'),
(20,'投屏分享当前页面','电脑工具同LAN；或电视支持Miracast','电脑工具查IP；设备设置→会议投影→选择/输入IP。电视打开无线显示后，设备会议投影→Miracast选目标连接。','两个投屏路径前提不同；仅显示输出，不等于电脑可控制设备。','停止投屏、切网重连、录音音频是否输出、密码笔记遮蔽。','E-IXCD-01','D2'),
(21,'恢复出厂设置','通用设置→设备及存储信息；已设密码则验证','手册列恢复出厂入口并说明清除账号、系统及应用数据。','恢复出厂与退出账号的保留范围不同；没有给出备份恢复完整流程。','确认页和取消路径、TF卡/云副本范围、账号重登后的恢复完整性。','E-IXCD-01','D1'),
(22,'检查固件或应用更新','历史X2；足够存储及电量高于50%','检查更新显示版本；WiFi默认自动下载，移动数据须手动开始下载。','下载升级包不等于已完成安装；固件和应用更新都在手册描述范围。','安装确认、重启、失败回滚、刷机后数据、当前服务可达性。','E-IXCD-01','D2'),
(23,'新闻与手机网文进入收藏（D1）','历史腾讯新闻/读写客功能','手册列频道配置、听新闻、收藏至网文；手机系统分享给读写客再于设备阅读。','跨模块内容流转有据；手册所谓永久保存不当服务承诺。','频道编辑、朗读控制、采集正文范围、失效网页、去重/来源保留。','E-IXCD-01','D1'),
(24,'安装和管理应用（D1）','历史应用中心','手册列应用市场下载、安装、升级、卸载能力，未给操作步骤。','能力存在不证明任意APK兼容，也不证明AI可读取/操作第三方app。','当前商店列表、权限、安装失败、卸载数据保留和重装恢复。','E-IXCD-01','D1')]
vflows=[
(14,'新建及修改Daily日历事件','历史手册；Daily日历页','点＋→填写名称、全天或起止时间、备注→Confirm；点已有事件进入详情，可编辑/删除。','日历事件与手写任务/笔记不同；Cancel入口可见，未执行。','时段冲突、时区、提醒、重复事件、删除恢复和当前构建变化。','E-VICD-01;E-VICD-03;E-VICD-04','D2'),
(15,'绑定日历并处理解除绑定','历史手册；Outlook或Google账号','Daily设置→选择账号授权；页面有手动同步入口。手册称事件增删改自动同步，解绑后设备只保留本地创建事件。','企业Outlook需管理员授权；失败建议重试。此解绑规则不代表原生Viwoods Cloud数据规则。','授权回调、冲突优先级、离线删除传播、解绑前提示及远端是否保留。','E-VICD-01;E-VICD-05','D2'),
(16,'从第三方云导入或导出可编辑笔记','历史Viwoods Files APK；授权云盘','Cloud Drive登录；长按云文件→Import→设备目录确认。My Files长按→Export，选择PDF或note及云目录。','首个账号默认同步目标；note仅AiPaper能打开编辑，且各应用note格式不能互导，需回到对应应用。','PDF与note信息损失、同名覆盖、媒体是否随包、导出失败、重新安装后的完整恢复。','E-VICD-01;E-VICD-06;E-VICD-07','D2'),
(17,'选择第三方云的同步对象与格式','历史手册；已授权第三方云账号','主页云同步图标短按执行；长按进入云界面→Settings，选择账号、目标文件夹和PDF/note格式。','Google Drive/OneDrive/Dropbox与V3.15新增原生云分别研究；同步按钮不证明双向合并或独立历史备份。','调度/自动触发、进度与失败、取消、删除传播、目录多选含义与账号切换。','E-VICD-01;E-VICD-08','D2')]
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
new=[]
for pid,seq,case in [('M-009',xflows,'M-009-iflytek-office.md'),('M-007',vflows,'M-007-viwoods.md')]:
 for n,t,pre,act,state,gap,eids,depth in seq:
  new.append(dict(flow_id=f'F-{pid.replace("-","")}-{n:02}',product_id=pid,task=t,depth=depth,evidence_ids=eids,known_branches=state,gaps=gap,visual_status='static_manual_pages_only' if len(eids.split(';'))>1 and any(x['visual_observed'] and x['evidence_id'] in eids.split(';') for x in E) else 'not_observed',hands_on='false',case_path='cases/'+case,checked_on=date))
with (lib/'batches/office-aipaper-deepening-coverage.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields,lineterminator='\n');w.writeheader();w.writerows(new)
# Save rendered case rows separately; root integrates existing content once.
for name,pid,seq in [('x2','M009',xflows),('viwoods','M007',vflows)]:
 (work/(name+'-new-rows.md')).write_text(''.join('| '+' | '.join([f'F-{pid}-{n:02}',t,pre,act,state,eids.replace(';','；'),gap])+' |\n' for n,t,pre,act,state,gap,eids,depth in seq))
# Historical manual contents. These are chapters/subsections with overlap, never a count of distinct functions.
invfields=['entry_id','product_id','scope','entry_type','title','upstream_id','url','evidence_id','review_status','related_flow_ids','checked_on']
xdoc=pymupdf.open(work/'X2-manual.pdf');entries=[];chapter=''
mapflows={'指纹电源按键':[],'桌面':[],'AIUI 智能语音交互':[],'写作':[9,10,11],'创建一篇新写作':[9],'自由书写':[9],'子标题':[9],'键盘输入文本':[10],'插入图片':[],'录音':[11],'转文字':[9],'分享':[4,5,6],'页面管理':[9],'新增页':[9],'更多功能':[9,13],'记录':[12],'创建记录':[12],'记录过程':[12],'查看记录':[12],'上传及分享':[4,6],'笔记隐私保护':[13],'首次开启笔记加密':[13],'查看加密笔记与忘记密码':[13],'笔记密码管理':[13],'邮件应用':[18],'登录邮箱':[18],'QQ 邮箱设置':[18],'网易163/126 邮箱设置':[18],'iCloud 邮箱设置':[18],'邮件操作':[18],'查看收信':[18],'添加星标':[],'删除邮件':[18],'写邮件':[18],'搜索邮件':[],'通讯录':[],'附件管理':[18],'邮箱设置':[18],'了解阅读':[1,2,14,15,16],'书架':[1,14,15,16],'书城':[],'文档':[1,2],'导入Office 文档':[1],'阅读文档':[],'文档批注与导出':[],'腾讯新闻':[23],'网文':[23],'网盘':[2],'日历':[19],'应用中心':[24],'账号设置':[7,17],'无线设置':[],'通用设置':[21,22],'配套同屏工具':[20],'Miracast 同屏':[20]}
for p in range(1,4):
 for line in xdoc[p].get_text().splitlines():
  line=line.strip()
  if re.match(r'第[一二三四五六七]章$',line):chapter=line;continue
  m=re.match(r'(.+?)\.{4,}\s*(\d+)\s*$',line)
  if not m:continue
  rawtitle=m[1].strip();title=re.sub(r'^\d+(?:\.\d+)?\.?\s*','',rawtitle);num=int(m[2]);is_chap=not re.match(r'^\d',rawtitle)
  entries.append(dict(entry_id=f'I-M009-X{len(entries)+1:02}',product_id='M-009',scope='X2历史官方手册；固件未标；与2023 AI升级/现行政策分开',entry_type='chapter' if is_chap else 'section',title=(chapter+' '+title if is_chap else title),upstream_id=f'目录 {rawtitle}; printed {num}; file {num+4}',url=xurl+f'#page={num+4}',evidence_id='E-IXCD-01',review_status='manual_text_read_no_runtime_observation',related_flow_ids=';'.join(f'F-M009-{n:02}' for n in mapflows.get(title,[])),checked_on=date))
with (lib/'inventories/M009-X2-manual-inventory.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=invfields,lineterminator='\n');w.writeheader();w.writerows(entries)
vchap=[('Quick Start',2,[1,13]),('Feature Overview',15,[]),('Input Method',16,[]),('Paper',17,[7]),('Learning',27,[2,9]),('Meeting',34,[]),('Daily',37,[14,15]),('Picking',41,[8]),('File Encryption',46,[]),('Apps',47,[]),('AI',48,[3,4]),('Viwoods Files APK',51,[16]),('Third-Party Cloud Integration',54,[17]),('Mailbox',55,[]),('ViTransfer',62,[10]),('Share',66,[]),('Memo',67,[])]
ventries=[]
for i,(title,p,fs) in enumerate(vchap,1):
 ventries.append(dict(entry_id=f'I-M007-V{i:02}',product_id='M-007',scope='AiPaper官网历史70页英文手册；非V3.15全功能目录',entry_type='chapter',title=title,upstream_id=f'{i}; printed {p}; file {p+1}',url=vurl+f'#page={p+1}',evidence_id='E-VICD-01;E-VICD-02',review_status='contents_image_read_selected_sections_only',related_flow_ids=';'.join(f'F-M007-{n:02}' for n in fs),checked_on=date))
with (lib/'inventories/M007-historical-manual-inventory.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=invfields,lineterminator='\n');w.writeheader();w.writerows(ventries)
print(json.dumps({'new_flows':len(new),'evidence':len(E),'static':sum(e['visual_observed'] for e in E),'x2_contents_rows':len(entries),'viwoods_contents_rows':len(ventries)}))
