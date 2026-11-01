from pathlib import Path
import json,csv,re
root=Path('docs/research/interaction-library'); out=root/'batches'
items=[
('01','Kobo Libra Colour','17971574287895-Kobo-Libra-Colour','Model number; User Guide','N428 型号及翻页键/手写屏；没有检视图片。'),
('02','Kobo eReader User Guide','360019123773-Kobo-eReader-User-Guide','About this guide through Security, Accessibility and Troubleshoot','官方指南目录，分清已重建任务和只见入口的功能域。'),
('03','Support Documents — Kobo Libra Colour N428','https://www.kobo.com/sg/en/p/support-documents','Kobo Libra Colour: English PDF, EAA EPUB, View Online','只确认官方文件入口；PDF 和 EAA EPUB 各直取一次均 HTTP 403/exit 1，预览无可抽取正文。未读到完整型号手册。'),
('04','Add non-protected PDF and ePub files using your computer','360024775093-Add-non-protected-PDF-and-ePub-files-to-your-Kobo-eReader-using-your-computer','A few things to remember; import steps; find added file','通用操作；正文 micro-USB 是旧文案，N428 USB-C 另由 E-KBD-20 支持。'),
('05','Add books using Google Drive','15335985512983-Add-books-to-your-eReader-using-Google-Drive','Before you start; Steps 2–5; View files; Disconnect','Libra Colour 明列兼容；软件门槛 4.37；传入本机与撤销共享分开。'),
('06','Add books using Dropbox','360033830114-Add-books-to-your-eReader-using-Dropbox','Before you start; Steps 2–5; View files and missing files','Libra Colour 明列兼容；软件门槛 4.18；目录与刷新故障分支。'),
('07','Turn off Dropbox sharing on your eReader','360033882694-Turn-off-Dropbox-sharing-on-your-eReader','Turn off Dropbox Sharing; Disconnect accounts','退出共享与下载副本清除是不同状态。'),
('08','Add eBooks with Adobe Digital Editions','360017814074-Add-eBooks-with-Adobe-Digital-Editions','Important; Steps 2–4; Having trouble signing in','现行官方正文直接要求 ByteBooks 身份迁移；不是从其他阅读器类推。未安装/登录。'),
('09','Buy a book on your Kobo eReader','360017676913-Buy-a-book-on-your-Kobo-eReader','Purchase steps; confirmation; missing purchase sync','阅读器购书文档，实际国家/付款资格未知；没有交易。'),
('10','About the Libby app','4477058367895-About-the-Libby-app','6.0 note; Add library card; Borrow; Limitations; offline FAQ','6.0+ 重新加卡；此页绑定入口与 E-KBD-11 不同，保留版本缺口。'),
('11','Borrow eBooks from the public library using your Kobo eReader','360017677053-Borrow-eBooks-from-the-public-library-using-your-Kobo-eReader','Compatibility; Add library card; Borrow; Holds; Filter','N428 兼容、参与地区和馆；借阅和候补步骤。未执行借书。'),
('12','Turn pages on your eReader','360017481194-Turn-pages-on-your-eReader','Touch screen; page turn buttons','通用触控步骤；实体键段型号列表未列 N428，不直接确认其反转和长按细节。'),
('13','Skip chapters and pages','360017639953-Skip-chapters-and-pages-on-your-Kobo-eReader','Skip chapters; Browse; Page Preview tips','预览与真正跳转及返回原页是不同状态。'),
('14','Turn pages, magnify, and rotate PDFs','360017513414-Turn-pages-magnify-and-rotate-PDFs-using-your-Kobo-eReader','PDF limitations; turning; zoom; landscape','泛指普通书只能竖屏的旧句与 N428 旋转专页不符；不采用该泛化。'),
('15','Adjust font size and style','360017639913-Adjust-font-size-and-change-the-font-style-on-your-Kobo-eReader','Steps 1–3; remembered preferences; PDF exception','排版偏好与固定版式 PDF 分开。'),
('16','Lock screen orientation','360019141453-Lock-your-eReader-s-screen-orientation','Supported models; lock and Auto rotate','型号列表含 Libra Colour；优先于旧 PDF 页的竖屏泛化。'),
('17','Prevent accidental page turns','21346752946839-Prevent-accidental-page-turns-while-reading','Adjust page turn settings','文档页面 3/3 与 Swiping only；具体固件未知。'),
('18','Reduce the rainbow effect','21355003278615-Reduce-the-rainbow-effect-when-viewing-colour-content','Steps 1–5; performance and image caveat','Libra Colour 明列兼容；不将彩色处理当作全刷频率。'),
('19','Adjust screen brightness','360017481234-Adjust-your-Kobo-eReader-s-screen-brightness','Use ComfortLight while you read; menu','仅用于通用亮度动作；没有把旧 ComfortLight PRO 型号列表补写为 N428 支持。'),
('20','Write notes in your book with Kobo Stylus','1500001927562-Write-notes-in-your-book-with-Kobo-Stylus','Supported file types; write/colour; view markup; delete; PDF export','书内 EPUB 笔迹、非保护 PDF 与独立笔记本出口分别记录。'),
('21','Use your Kobo eReader as a notebook','360062226733-Use-your-Kobo-eReader-as-a-notebook','Create; Basic; Advanced; Refresh; backup; computer export; remaining chapter inventory','正文；限制摘述主链，其他章节只列待查。页面笔色段含 Clara Colour 旧误项，不据此扩型号。'),
('22','Use gestures to edit handwriting','1500001833721-Use-gestures-to-edit-handwriting-on-your-eReader','Advanced-only note; conversion; replace word; join/split','转换后人工编辑与保存有明确动作；不推断识别效果。'),
('23','Listen to audiobooks on your Kobo eReader','4406292712471-Listen-to-audiobooks-on-your-Kobo-eReader','Supported models; Download; Bluetooth; playback controls; chapters','N428 音频专页；未找到整本缓存后断网/断连续播保证。'),
('24','Delete a book from your eReader','360017640033-Delete-a-book-from-your-eReader','Steps 1–8; Remove download vs Remove from My Books','账户内容范围；不能用于承诺外部导入书或笔迹恢复。'),
('25','Move items out of your account archive','360017765873-Move-items-out-of-your-account-archive','Steps 1–8; post-sync result','账户书目回库的确认及关闭步骤。'),
('26','Repair your Kobo account','360017605394-Repair-your-Kobo-account-on-your-Kobo-eReader','Repair steps; after repair; unresolved branches','修复后仍要搜索检查；不是已完成恢复的实测。'),
('27','Reset your Kobo eReader','360017605114-Reset-your-Kobo-eReader','Before reset; effects; reset steps; manual reset link','一般菜单重置；与型号双键恢复分开。'),
('28','Manual reset Kobo Libra Colour','18259230340887-Manual-reset-Kobo-Libra-Colour','Before start; Kobo Libra Colour manual reset steps','只用于 N428；原文常亮/闪烁描述存在歧义，未操作。'),
('29','Kobo eReader Accessibility Features Support','32246787707799-Kobo-eReader-Accessibility-Features-Support','EU update; Read Aloud; July 2026 Version 5.18 limitations; enable/revert','文内发布/限制时间不等于网页修改时间；条件分支，不泛化全球常规固件。'),
('30','Sync your Kobo eReader over Wi Fi','360017513334-Sync-your-Kobo-eReader-over-Wi-Fi','Syncing; before start; steps; after sync','正文含过时不支持音频句；仅采同步流程，音频以 E-KBD-23 为准。'),
('31','Use Instapaper with your Kobo eReader','33359968957463-Use-Instapaper-with-your-Kobo-eReader','Steps 1–3; Current Features; icons','现行集成，未给最低固件；与无障碍分支限制并列。'),
('32','Fix common Kobo eReader problems','360017605134-Fix-common-Kobo-eReader-problems','If you have issues viewing your book; missing books; reset caveat','本条取得搜索索引全文，采用重开书路径；未把通用强制恢复时序套到 N428。'),
('33','Download all your books onto your Kobo eReader','360017605034-Download-all-your-books-onto-your-Kobo-eReader','Download all steps; offline reading purpose','用于账户书离线准备；不外推音频每一章和第三方文件恢复。'),
('34','File formats your Kobo eReader and Kobo Books app support','360017763713-File-formats-your-Kobo-eReader-and-Kobo-Books-app-support','Kobo eReader and Kobo Desktop file support; DRM notes','通用兼容清单；不把文件打开能力等同于所有阅读功能。')]
records=[]
for no,title,url,locator,note in items:
 if not url.startswith('https://'):url='https://help.kobo.com/hc/en-us/articles/'+url
 scope='Kobo Libra Colour N428 research boundary; generic Kobo eReader help where model/build not stated; English; account region unknown'
 fw=None
 if no=='05':scope+='; Google Drive requires firmware >=4.37';fw='minimum 4.37 (help requirement, not observed build)'
 if no=='06':scope+='; Dropbox requires firmware >=4.18';fw='minimum 4.18 (help requirement, not observed build)'
 if no=='10':scope+='; library-card migration following version 6.0 or later';fw='6.0+ migration note; installed build unknown'
 if no=='29':scope='Libra Colour among supported models; EU optional accessibility update released 2025-07-29; limitations labelled July 2026 / 5.18; not global standard-firmware claim';fw='5.18 limitation table; installed build unknown'
 records.append(dict(evidence_id='E-KBD-'+no,product_id='M-002',title=title,url=url,checked_on='2026-10-26',source_type='official_support_index' if no in ('02','03') else 'official_help',access_mode='official_index_text' if no=='32' else 'opened_text',version_scope=scope,app_build=None,firmware_version=fw,source_updated_on=None,locator=locator,note=note,hands_on=False,visual_observed=False,archived_artifact=None,sha256=None,capture_state='bibliography_and_short_note_only',supersedes=None))
(out/'kobo-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in records))
flows=[
('04','USB 导入自有 EPUB/PDF','04;20','非保护文件、USB检测与本地范围','导入中断、重复和损坏文件'),
('05','Google Drive 传书及撤权','05','目录、刷新、下载与解除共享','代码过期、同名替换、失败重试'),
('06','Dropbox 传书及退出共享','06;07','缺文件检查、退出共享保留副本','并发变化与部分下载恢复'),
('07','ADE 外部 DRM 书授权与导入','08','ByteBooks身份迁移与授权错误','授权额度和迁移失败'),
('08','阅读器购书到打开','09','已存账单、优惠资格、购后未显','支付失败、取消与退款'),
('09','借书、候补与到期','10;11','地区与馆资格、候补、到期、6.0迁移','绑定入口差异、续借、多卡、到期批注'),
('10','翻页、预览跳转与返回','01;12;13;29','退出预览和回原页、软件分支','N428键反转与快翻具体动作'),
('11','PDF 缩放与导航','14;16','放大和未放大翻页、旋转','缩放保持和扫描PDF处理'),
('12','排版、方向与防误触','15;16;17','PDF例外、方向锁定、触控模式','设置页码与按钮互相影响'),
('13','亮度与彩色处理','18;19','彩虹纹处理的显示取舍','全刷频率入口、刷新实际表现'),
('14','书内手写、重排与PDF取回','20;29','格式、权限、重排、永久删除','跨页锚点、笔迹冲突'),
('15','Basic 笔记创建与结束','21','撤销、翻页、残影刷新','掉电最后笔画和整本删除恢复'),
('16','Advanced 转文字与校订','21;22','Advanced限定、转换后编辑','多语言、离线、撤销边界'),
('17','独立笔记导出与同步设置','21','导出格式和云备份开关','失败恢复、冲突、可编辑备份'),
('18','下载并用蓝牙听书','23','下载状态、配对失败、未缓存章节','断网换章、断连与重启续播'),
('19','删除下载、账户移除与恢复','24;25','清缓存与云归档不同','本地导入书与个人积累恢复'),
('20','缺书或显示异常的修复','26;32','修复后检查、重开书','取消、进度、错误码、修复范围'),
('21','菜单重置及N428手动恢复','27;28','数据清除及型号双键流程','灯时序歧义、恢复中断'),
('22','欧盟可选无障碍与Read Aloud','29','更新和回退的重置及功能限制','语种、语音缓存、5.18与6.0对应'),
('23','网页文章进入阅读器','31;29','主列表范围、刷新、归档返回','最低版本和断网状态'),
('24','同步与离线备书','30;33;23','未知Wi-Fi和离线下载准备','断点续传、完整缓存与同步冲突')]
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
with (out/'kobo-deepening-coverage.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields);w.writeheader()
 for no,task,ev,branches,gaps in flows:
  w.writerow(dict(flow_id='F-M002-'+no,product_id='M-002',task=task,depth='D2',evidence_ids=';'.join('E-KBD-'+x for x in ev.split(';')),known_branches=branches,gaps=gaps,visual_status='not_observed',hands_on='false',case_path='cases/M-002-kobo.md',checked_on='2026-10-26'))
print('Wrote',len(records),'new evidence,',len(flows),'new coverage rows')
