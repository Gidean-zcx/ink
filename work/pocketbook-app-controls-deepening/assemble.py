import csv,json,hashlib,shutil,re
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/pocketbook-app-controls-deepening';A=L/'artifacts/pocketbook-app-controls-deepening';A.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
def csvout(p,rows):
 with p.open('w',newline='') as f:
  d=csv.DictWriter(f,fieldnames=list(rows[0]),lineterminator='\n');d.writeheader();d.writerows(rows)
oldE=json.loads((W/'evidence-before.json').read_text());oldF=json.loads((W/'coverage-before.json').read_text());manual=next(x for x in oldE if x['evidence_id']=='E-PBK-01')
notes='''# PB743K3：App与设置补充的证据范围

2026-10-27。只复用PB743K3 / InkPad Color 3的6.10.2854路径英文95页手册。原PDF SHA256：87bf56b461cd298b7c53421c5b21f72f1498e4c060d4eea89844545667030101。未重新获取网页、更新固件或访问Libby/邮箱；实际安装构建未知。本轮没有外部账户、发送或设备动作。

正文复读文件/印刷28–30、34–41、69–90，共33页。仅29、37、41、76、77实际view_image查看，其他页为文字阅读；文件页等于印刷页。5张摘页来自同一原PDF，不是独立互证，也不能拼成连续操作。前批65条印刷目录保持原名/页码，本轮补控件与覆盖状态；未展开的游戏判分、取消和恢复仍缺。

## 重要限定

| 范围 | 已取得的依据 | 限制/疑点 |
| --- | --- | --- |
| Libby，34–39 | 借阅与预约、提前归还；通常下载电子书，不能下载则webview；音频为在线听。37图只有书目/Download/Place Hold示例。 | 预览不能借书；需要网络与有效馆卡。Return Early只在正文，不把37图当归还确认。到期对文件/笔记的处理未知。 |
| PIN，72–74 | 邮件确认后设置；错码提示，三次后Forgot PIN；恢复邮箱与客服条件；关闭二次确认。 | 邮件恢复不是离线解锁保证；手册用recover password措辞，具体重设界面未知。与85页Settings配置密码分开。 |
| 用户档案，83–84 | New Profile、Activate、勾号；复制/改名/删除；按档案保存若干阅读状态和预设网络。 | 未承诺文件隔离、每档案密码、云账号隔离或儿童权限；删除后的数据去向未说明。 |
| 无障碍，75–78 | Menu三击进入菜单；Power三击需启用Screen Reader Shortcut；焦点/激活/滑块/上下文/返回手势不同。 | 76图读屏off、快捷on仅示例，不能当默认；三击Power的关闭效果未说明；设备音频中断未核。 |
| Scribble，40–41 | Back是撤销上次动作；工具、New/Delete；保存到根目录My Pictures；可编辑图形注释。 | 41页New和保存句采用同形纸张图标，不能确定是否同一动作或新建自动保存。格式/覆盖/取消和回导仍未知。Coloring自动保存不套Scribble。 |
| 相框，28–30 | 创建照片列表、增图、Play、列表/图片上下文菜单、播放时点中部进入设置。 | Delete在文字中用于从列表移除，未说明底层图片文件；停止/退出/断点和电源策略未核。 |
| 维护，69、85–90 | Settings变化退出节应用；配置密码、恢复出厂、内部格式化有不同对象；版本/更新/排错另列。 | 不把设置总则套为Reset/Format延迟执行。更新表仍写card和SWUDATE.BIN，不能据此增加本型号卡槽或修正成保证成功的步骤。 |

全部为资料重建；没有测响应、耗电、离线馆借恢复、密码找回或删除效果。正式任务步骤见案例，原件只在work；档案隔离、AI可操作接口及Plus实现均未得到产品验证。
'''
np=A/'source-notes.md';np.write_text(notes)
meta=[(29,'相框列表与播放设置','＋、Play及齿轮图标；删除来自列表正文，未观察结果'),(37,'Libby书目及借还规则','静态图有Download/Place Hold；Return Early仅正文；没有执行借还'),(41,'Scribble撤销/保存与Coloring边界','Back为撤销；New与保存句同形纸张图标保留歧义'),(76,'Screen Reader开关与物理快捷键','读屏off/快捷on是示例；Power三击需启用快捷，不当默认'),(77,'无障碍焦点、激活及上下文手势','单点/双点/滑动/双点保持分开；未实际触发')]
es=[];obs=[]
for i,(n,title,note) in enumerate(meta,1):
 eid=f'E-PBACD-{i:02d}';src=W/f'raw/manual-p{n:02d}.png';dst=A/src.name;shutil.copyfile(src,dst)
 es.append(dict(evidence_id=eid,product_id='M-003',title=title,url=manual['url'],checked_on='2026-10-27',source_type='official_manual_figure',access_mode='local_pdf_render_actually_viewed',version_scope='PB743K3 InkPad Color 3 only; official manual path 6.10.2854; installed build unknown',app_build=None,firmware_version='6.10.2854 document baseline; installed unknown',source_updated_on=None,locator=f'PDF/printed page {n}',note=note,hands_on=False,visual_observed=True,archived_artifact=str(dst.relative_to(L)),sha256=sha(dst),capture_state='static_page_excerpt_archived',supersedes=None,derived_from='E-PBK-01',raw_work_path=str(src.relative_to(R)),raw_sha256=sha(src),visual_audit_path=str((A/'observed-pages.json').relative_to(L))))
 obs.append(dict(evidence_id=eid,pdf_page=n,printed_page=n,observed_on='2026-10-27',actual_view='view_image',hands_on=False,continuous_sequence=False,raw_work_path=str(src.relative_to(R)),raw_sha256=sha(src),archived_artifact=str(dst.relative_to(L)),sha256=sha(dst),note=note))
dump(A/'observed-pages.json',obs)
(L/'batches/pocketbook-app-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(e,ensure_ascii=False)+'\n' for e in es))
nf=[]
def f(n,t,e,b,g,v='not_observed'):
 nf.append(dict(flow_id=f'F-M003-{n:02d}',product_id='M-003',task=t,depth='D2',evidence_ids=e,known_branches=b,gaps=g,visual_status=v,hands_on='false',case_path='cases/M-003-pocketbook.md',checked_on='2026-10-27'))
f(10,'借阅、预约和归还Libby内容','E-PBK-01;E-PBACD-02','按馆名/地区查馆，馆卡登录或只预览；有副本Borrow/选期限，无副本Place Hold；Manage Loan/Return Early。电子书通常下载，失败可webview；音频在线。','预览不能借；授权/借期/配额依馆；下载中断、音频断点、到期后的文件/笔记及移除馆影响未知。','static_reference_only')
f(11,'设置PIN及找回设备访问','E-PBK-01','Device PIN Code启用先确认恢复邮箱；Wrong PIN，三错出现Forgot PIN；可改请求场景、Change PIN及Turn off确认。','邮件送达/失联、重设落地、离线恢复、失败次数后限速、与配置密码及用户档案关系未知。')
f(12,'创建、切换及整理用户档案','E-PBK-01','Personalize/User Profiles/New Profile命名确认；名称菜单Activate，活动档案勾号；头像/复制参数/改名/删除。','文档列阅读状态与预设网络；文件/云账号/密码隔离、复制对象和删除恢复不明，非已证儿童权限。')
f(13,'启用读屏并按焦点导航','E-PBK-01;E-PBACD-04;E-PBACD-05','Settings/Accessibility或Menu三击；Use Screen Reader；Power三击快捷需先开。点按聚焦、双点激活；滑动改焦点/滑块，双点保持上下文，两指返回/滚动。','开关示例非默认；快捷关闭、屏幕内容覆盖、音频路由/中断和误触恢复未知。','static_reference_only')
f(14,'在Scribble绘图、撤销及保存','E-PBK-01;E-PBACD-03','画笔/文字/字体/橡皮等工具；Back撤销上次操作；New/Delete；保存路径My Pictures；亦可编辑图形注释。','New与保存句同形图标；是否同一动作、自动保存/覆盖、文件格式、回导编辑与误删恢复未知。','static_reference_only')
f(15,'建立相框照片列表并播放','E-PBK-01;E-PBACD-01','Photo Frame/New Playlist/命名Continue/选图Create Playlist；＋增图、Play；长按列表改名/删/增图，长按图Delete；播放中点中央进入设置。','删除作用于列表与原图关系、停止/返回、锁屏/关机、中断断点和时间设置范围未知。','static_reference_only')
f(16,'区分设置重置、格式化与维护','E-PBK-01','Maintenance的配置密码和Factory Reset；About Device/Memory/Format Internal Memory。手册说重置清设置保留书/图片，格式化删非系统文件。','未见确认/取消/完成回执、笔记/档案细项、断电和恢复；不可将格式化当普通重置。')
csvout(L/'batches/pocketbook-app-controls-deepening-coverage.csv',nf)
changes={1:{'known_branches':oldF[0]['known_branches']+' Settings总则为退出节应用更改；阅读触摸/滑动/边缘拖动可配置；HID蓝牙按键映射有独立入口。','gaps':'当前默认布局/实际安装版、配置保存失败、HID适配、后台保留与关闭影响未知；读屏手势仅见F13。','checked_on':'2026-10-27'},3:{'evidence_ids':oldF[2]['evidence_ids']+';E-PBACD-03','known_branches':oldF[2]['known_branches']+' Scribble可编辑图形注释，但New/保存句同形图标有歧义，不补成导出文件回导链。','gaps':'导出格式/回导编辑、同名覆盖/取消/恢复仍未知；绘图My Pictures路径不证明Notes导出格式或自动保存。','checked_on':'2026-10-27'}}
ups=[]
for n,ch in changes.items():
 before=oldF[n-1];after={**before,**ch};ups.append({'flow_id':before['flow_id'],'before':before,'after':after,'changed_fields':{k:{'before':before[k],'after':v} for k,v in ch.items() if v!=before[k]}})
dump(W/'old-coverage-updates.json',ups)
caps=[{'evidence_id':'E-PBK-01','expected_url':manual['url'],'before_additional_captures':manual.get('additional_captures',[]),'append_additional_captures':[{'checked_on':'2026-10-27','access_mode':'existing_local_pdf_text_and_selected_static_pages_no_network','raw_work_path':manual['raw_work_path'],'raw_sha256':manual['raw_sha256'],'locator':'Printed/PDF pp28–30,34–41,69–90 text; actual static views pp29,37,41,76,77 only','text_pages_count':33,'visual_observed':True,'visual_pages':[29,37,41,76,77],'hands_on':False,'continuous_sequence':False,'archived_artifact':str(np.relative_to(L)),'sha256':sha(np),'visual_audit_path':str((A/'observed-pages.json').relative_to(L)),'note':'旧同一原件本地复读；非新修订或独立来源，未重新下载；同URL旧E-PBCD-01保持历史记录。'}]}]
dump(W/'old-evidence-additional-captures.json',caps);dump(W/'old-evidence-field-updates.json',[])
inv=json.loads((W/'inventory-before-normalized.json').read_text())
details={
'General':('28–30','正文功能域；子项分别登记','','日历/计算/Gallery/Photo Frame/RSS','无整个应用集合完成声明'),
'Calendar & Clock':('28','正文已读；未单列任务','','上/下月；返回本月','不含事件增改删或提醒流程'),
'Calculator':('28','功能声明；未单列任务','','数制/三角/对数/幂/逻辑','无按键、输入错误和结果保存流程'),
'Gallery':('28','正文已读；未单列任务','','Gallery文件夹图片；全屏触摸进编辑；旋转/信息/Power-off Logo；缩放选区','保存确认/退出及原图变化未核'),
'Photo Frame':('28–30','D2对应任务/仍有缺口','F-M003-15','New Playlist/Continue/Create；＋/Add Photos/Play；长按菜单；齿轮；日期/间隔/Shuffle/Display mode','列表删除不推定文件清除；退出、断点和电源规则缺'),
'PocketBook x Libby':('34–39','D2对应任务/仍有缺口','F-M003-10','查馆/国家筛选/卡登录或Preview；Borrow/Place Hold/Return Early；馆卡列表/换馆/移除馆','需网络；在线音频；配额/到期文件及删除馆副作用未核'),
'Games':('40–41','正文功能域；不是独立完成任务','','Klondike/Sudoku/Chess/Scribble/Coloring','玩法规则/错误反馈/局内恢复未给全链'),
'Klondike':('40','仅功能声明','','经典接龙','无牌局步骤或胜负反馈'),
'Sudoku':('40','局部输入控件已读；未单列任务','','选空格；底部选数字或候选标签','未给错误判定、提示/清空、完成奖励、退出恢复'),
'Chess':('40','仅功能声明','','对朋友/设备；设备10难度','棋子选择、非法步/悔棋、结束保存未核'),
'Scribble':('40–41','D2对应任务/仍有缺口','F-M003-14;F-M003-03','Palette/Text/Font/Pencil/Eraser；Back/New/Delete；My Pictures保存','New/保存同形图标；格式、退出自动保存、回导/恢复未核'),
'Coloring':('41','功能及保存声明；未单列任务','','给黑白图上色；文档称自动保存','位置、恢复编辑、撤销及保存失败未知；不套Scribble'),
'Settings [system]':('69','D2对应任务/仍有缺口','F-M003-01','通知面板Settings；退出节应用更改','不将总则套Reset/Format延迟执行'),
'Wi-Fi':('69','局部正文控件复读；未单列任务','','Network Access；隐藏网络；Available Networks/Scanning；连接参数','失败重试/隐藏网络完整步骤未本轮展开'),
'Bluetooth':('69–70','D2对应任务/仍有缺口','F-M003-09;F-M003-01','ON/OFF/Visible；HID按钮在Personalize/Key mapping新增并映射','默认不可见为手册描述；外设/音频适配未测'),
'Accounts and Synchronization':('70–72','局部正文复读；旧任务保留','F-M003-04;F-M003-06','Store/Send-to/Dropbox/Adobe/Cloud账户项；移除Adobe授权后保护文档不可用','授权移除可逆性/现存副本/云同步冲突未核'),
'PIN code':('72–74','D2对应任务/仍有缺口','F-M003-11','恢复邮箱确认；Turn on/Change/Request/Turn off；Wrong PIN/三错Forgot PIN','不保证离线找回；实际邮件/重设/客服流程未执行'),
'Accessibility':('74–78','D2对应任务/仍有缺口','F-M003-13','Menu三击；文本大小/粗体/OpenDyslexic；Screen Reader/Shortcut/Voice；焦点/激活/返回手势','快捷开关示例非默认；中断/触摸失败、关闭全链未知'),
'Personalize':('78–84','部分D2及设置正文复读','F-M003-01;F-M003-12','前光时段图/启用编辑复制改名删；DARKmode/插图反转；Logos；LED；tap/swipe/drag；全刷；Startup；User Profiles','界面大章不是单流程；配置冲突/性能/文件或账号隔离未知'),
'Languages':('84','局部设置已读；未单列任务','','界面语言；最多3键盘布局','切换确认/输入保留和缺语言未知'),
'Date/Time':('84','局部设置已读；未单列任务','','日期/时间/时区/格式；Synchronize Time/Auto Time Sync','网络失败/回退与内容时间影响未知'),
'Saving Power':('84','局部设置已读；未单列任务','','电量显示；Auto screen lock off/5/10分钟；Power off after 10/20/30/60分钟','只列空闲时长；不推定音频/TTS会阻止或受影响'),
'Maintenance':('85','D2对应任务/仍有缺口','F-M003-16','USB默认角色；配置密码；Factory Reset；Demo','配置密码不同于PIN；重置保留书/图片是文档范围非实测'),
'About Device':('85–86','D2对应任务/仍有缺口','F-M003-16','设备信息/Memory/格式化；版本/自动检查/下载设置','格式化除系统文件外删除；恢复/失败范围未核'),
'Firmware Update':('87–88','操作正文已读；未执行/未单列任务','','内存→网络检查；自动/手动更新步骤；版本错误项','保留Abouut、card、SWUDATE.BIN等文案；没有固件下载或刷机'),
'Troubleshooting':('89–90','错误正文复读；不作已恢复结果','','电量/坏书/编码/冻结应用/USB/满存储或文件系统导致保存失败','未实测；冻结按键图本轮未看，不凭抽取空白命名按键')}
for r in inv:
 r['body_pages_reread']='';r['controls_or_tasks']='';r['read_basis']='prior_inventory_scope_inherited';r['checked_on']='2026-10-27'
 if r['manual_item'] in details:
  pages,state,flows,ctrl,gap=details[r['manual_item']];r.update(body_pages_reread=pages,checked_scope=state,flow_ids=flows,controls_or_tasks=ctrl,gap=gap,read_basis='existing_local_manual_body_reread',evidence_ids='E-PBK-01')
 for name,eid in [('Photo Frame','E-PBACD-01'),('PocketBook x Libby','E-PBACD-02'),('Scribble','E-PBACD-03'),('Accessibility','E-PBACD-04;E-PBACD-05')]:
  if r['manual_item']==name:r['evidence_ids']+=';'+eid
csvout(L/'batches/pocketbook-app-controls-deepening-inventory.csv',inv)
dump(W/'inventory-summary.json',{'rows':65,'rows_with_current_body_detail':len(details),'text_pages_reread':33,'actual_view_pages':[29,37,41,76,77],'whole_product_complete':False})
print('new E',len(es),'new F',len(nf),'old F',len(ups),'fields',sum(len(x['changed_fields']) for x in ups),'inventory',len(inv),'detailed rows',len(details))
