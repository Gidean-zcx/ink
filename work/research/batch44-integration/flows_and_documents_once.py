import csv,json,re,hashlib
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';B=L/'batches'
OUT=B/'boox-system-content-deepening-coverage.csv';assert not OUT.exists()
N='artifacts/boox-system-content-deepening/source-notes.md'
def read(p):return list(csv.DictReader(p.open()))
def write(p,rows):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
def append(p,s):p.write_text(p.read_text().rstrip()+'\n\n'+s.strip()+'\n')
data=[
('F-M004-20','M-004','截取当前画面或长网页（通用帮助参考）','E-BXSD-03;E-BXCD-03','控制中心/导航球/三指三入口；快捷项缺失可Edit补入；应用可限制截图','本机V4.1.2路径、权限/限制反馈、长截图范围、保存/取消未核','not_observed','支持该功能的构建、可截图页面；三指需启用','控制中心Screenshot/Scrolling Screenshot；缺项Edit拖入；导航球另配置','资料只给触发与限制，未给实际文件和完成提示'),
('F-M004-21','M-004','录屏后保存和查看输出（通用帮助参考）','E-BXSD-04','长按先配置；圆形开始/X结束；MP4目标目录、缩略图查看/分享；静态锁屏保存选项','本机构建、实际锁屏保存、权限拒绝/空间不足/取消、音源和文件可播放性未知','static_official_images_only','支持录屏的构建，拟录制页面已打开；未实际操作','控制中心Screen Recorder；圆形开始、X结束；长按入口先调音源/帧率等','说明存Storage/Movies/Captures为MP4；图片只显示锁屏停止并保存选项，不证明其执行'),
('F-M004-22','M-004','向另一屏幕展示设备内容（通用帮助参考）','E-BXSD-08','Miracast与BOOXDrop浏览器显示、第三方AnyDesk分别记录；BOOXDrop静态说明无反向控制','设备固件和接收端资格、失败/拒绝、断开重连、延迟及真实控制未验证','static_official_images_only','按路径满足Windows/Miracast或同LAN/浏览器；第三方路径另需安装授权','BOOXDrop控制中心→Start Now→接收端打开所示URL；旋转/全屏','图像/正文支持显示路径与限制；不将Miracast所称控制套入BOOXDrop'),
('F-M004-23','M-004','订阅来源并接收稍后读内容（通用帮助参考）','E-BXSD-16','公共源Subscribe、自建组加源、OPML导入及独立OPDS页签；Push网页分开','本机构建、无效源/重复/部分导入、取消订阅后内容、正文缓存和已读状态未知','not_observed','PushRead可用；自备有效源或本地OPML文件','RSS→Directory→Add subscription；公共源Subscribe或Custom建组加源；菜单Import from OPML','新增的是订阅来源；不证明全文、图片、书籍已下载或离线准备完成'),
('F-M004-24','M-004','网页阅读中调整显示并查词（通用帮助参考）','E-BXSD-15','菜单预设字号；Page Visibility Enhanced开关；长按词后Dictionary并切词典','本机版本、全屏退出、查词关闭/返回选区、断网/保存、模式持久化未知','not_observed','NeoBrowser打开可选文字网页；词典可用','右上菜单增强显示；长按词→Dictionary→选择词典','资料描述显示/词典弹窗；不证明网页去推荐、保存正文或进度恢复'),
('F-M004-25','M-004','按日期回看备忘和阅读痕迹（现行通用帮助参考）','E-BXSD-17;E-BXSD-01','日期Memo、最后编辑/最后阅读条目点击打开；现行Events/Reminder只记入口','V4.1.2逐项适用、保存/删除/恢复、日界线/时区、完成判定未知；日程表单未闭合','not_observed','Calendar Memo已有对应日期内容；本机现行版本未核','选择日期查Memo及Today’s Note/Read，点条目打开','记录依据是最后编辑/阅读日期，不是创建、读完或理解；事件同步不回填固定基线'),
('F-M010-11','M-010','把推送网页切为阅读模式并离线回看（通用V4.0参考）','E-PALMA-15;E-BXSD-16','同ONYX账号推送；首次联网加载后可离线；中央菜单切Web/Reading Mode、字号/全刷','Palma 2现固件适用、网页完整缓存/附件/失效、断网实测、取消订阅与持久进度未知','not_observed','Mobile ePaper通用V4.0说明；同ONYX账号且网页首次加载时联网','网页/手机推送到PushRead；打开加载后，中央菜单切Reading Mode','手册称加载后可离线；未实际断网验证，未证明所有资源已缓存')]
flows=[]
for fid,pid,task,e,branch,gap,visual,pre,action,result in data:
 flows.append(dict(flow_id=fid,product_id=pid,task=task,depth='D2',evidence_ids=e,known_branches=branch,gaps=gap,visual_status=visual,hands_on='false',case_path='cases/M-004-boox-neoreader.md' if pid=='M-004' else 'cases/M-010-palma.md',checked_on='2026-10-27'))
write(OUT,flows)
updates={
'F-M004-02':('E-BXSD-02','；Tab系列桌面说明另区分移出组/移除小组件/卸载，Smart Assistant非AI Assistant','；桌面布局操作仅通用Tab帮助，未核本机构建'),
'F-M004-03':('E-BXSD-06;E-BXSD-11;E-BXSD-13;E-BXSD-19','；按app颜色与单文档显示分开；黑白/彩色选项互斥，Image Smoothing仅Regal','；全刷动作次数、录制帧率与屏幕物理刷新不可混用'),
'F-M004-04':('E-BXSD-18;E-BOOX-24','；手机Receive/Push与第三方云显式复制上传分开','；同名覆盖/删除传播与双向同步未知'),
'F-M004-14':('E-BXSD-01;E-BXSD-06;E-BXSD-07','；Tab帮助Mute仅铃声/通知；时钟屏保影响自动关机；冻结仍阻通知','；静态音量通道不证明蜂窝通话；睡眠策略与媒体保留未实测'),
'F-M004-16':('E-BXSD-19','；当前Display帮助限定彩色层亮度与Regal图像平滑','；显示效果不证明改写源文件'),
'F-M010-02':('E-PALMA-15','；通用手册桌面/账户机制仍限原件版本','；Smart Assistant不是系统AI能力证明'),
'F-M010-05':('E-PALMA-15','；通用V4.0 p43点空白退出词典，p49–50给TXT导出至Storage/note同名文件','；通用说明不能确定本机当前出口；覆盖/取消未知'),
'F-M010-07':('E-PALMA-15','；通用V4.0 p88自启动需app自有能力且安装后手动运行一次；后台不出现在最近任务','；不在最近任务不证明未运行，后台成功未测试'),
'F-M010-08':('E-BXSD-18','；手机Send to BOOX/Receive、Save To Phone与远程Push分开','；手机扫码不是设备相机OCR'),
'F-M010-09':('E-PALMA-15','；通用V4.0账户列表只留账户信息，切换需再登录且服务器可能切换','；账户切换不是无缝家庭档案或数据恢复证明')}
for p in B.glob('*-coverage.csv'):
 rows=read(p);dirty=False
 for r in rows:
  if r['flow_id'] not in updates:continue
  ev,br,ga=updates[r['flow_id']];r['evidence_ids']=';'.join(dict.fromkeys(r['evidence_ids'].split(';')+ev.split(';')));r['known_branches']+=br;r['gaps']+=ga;r['checked_on']='2026-10-27';dirty=True
 if dirty:write(p,rows)
# Model-specific case notes; new task rows remain explicitly generic references.
p=L/'cases/M-004-boox-neoreader.md';s=p.read_text();s=s.replace('本轮复用该原件，完整阅读其中10篇正文','第28批复用该原件，完整阅读其中10篇正文').replace('本轮只新取4张官方静态附件并实际查看','该批只新取4张官方静态附件并实际查看')
s=s.replace('| General Settings | 11 | 账户／更新已查；语言输入、连接、睡眠、显示、前灯、音量、鼠标、投屏等仅部分条件或目录 |','| General Settings | 11 | 第44批补语言/连接/睡眠/显示/前灯/音量/投屏正文；鼠标资格单列，失败与本机状态仍缺 |')
s=s.replace('| Interface and Navigation | 2 | F02；桌面布局、Dock／小组件未完整研究 |','| Interface and Navigation | 2 | F02补桌面/Dock/小组件操作说明，原文限定Tab系列；本机验证未完成 |')
s=s.replace('| Optimize Speed and Display | 3 | F03；对比度／颜色完整参数未研究 |','| Optimize Speed and Display | 3 | F03补按app颜色、黑白/彩色条件；真实显示、参数默认和恢复仍缺 |')
s=s.replace('| Apps | 8 | 优化／管理条件见 F03、14；安装卸载仅入口 D1；浏览器、PushRead、Calendar Memo、Work Profile 未形成完整任务 |','| Apps | 8 | 第44批补浏览器/PushRead/日历局部任务；安装仍入口D1，Work Profile本机资格未知 |')
s=s.replace('| File Transfer | 3 | F04 及原第三方云分支；手机传输、异常重试未完整 |','| File Transfer | 3 | F04补手机和第三方云步骤；断线/覆盖/重试未知 |')
s=s.replace('| Stylus | 2 | 校准／InkSpire仅目录及适用性核查缺口 |','| Stylus | 2 | 正文已读；EMR配笔资格待核，InkSpire本文限定Tab X C |')
s=s.replace('| Keyboard Cover | 2 | 配件连接及快捷键未研究，不能从平板功能推配件条件 |','| Keyboard Cover | 2 | 已读连接/快捷键正文；本机配件资格、输入冲突和状态保留未知 |')
s=s.replace('| Screenshot and Screen Recording | 2 | 未实际录屏；本轮只读静态图 |','| Screenshot and Screen Recording | 2 | 第44批补截图/录屏文档任务与录制设置静态图；没有实际录制 |')
section=['## 系统与内容入口补查（第44批，2026-10-27）','','本次补读原目录31篇，详见[逐项来源与7幅静态图](../'+N+')；原127行目录就地更新。下面是**当前通用帮助的局部D2参考**，不是Note Air5 C V4.1.2逐项确认，也不把Tab系列示意图称作本机截图。读取正文不代表每个控件、失败和恢复均已研究。','','| F编号 | 任务与前提 | 动作/资料所述结果 | 未闭环 | 证据 |','| --- | --- | --- | --- | --- |']
for fid,pid,task,e,branch,gap,visual,pre,action,result in data:
 if pid=='M-004':section.append(f'| {fid} | {task}；{pre} | {action}；{result} | {gap} | {e.replace(";","；")} |')
section += ['', '**原任务补强。** F02的桌面移出组、移除小组件与卸载对象不同；Tab文档Smart Assistant是快捷信息页，不是AI Assistant。F03区分系统字体/前灯、按app颜色、单文档显示；黑白屏Light Color Filter不能套入彩屏菜单，Regal才有Image Smoothing。[E-BXSD-02/06/11/13/19；E-BXCD-03]','',
'F04补手机双向传输与云复制上传：手机Send to BOOX后设备Receive；反向Save To Phone。第三方云从Storage长按Copy to Cloud Storage选目标并上传，不当自动双向同步。F14保留睡眠与音频的不同条件：Tab帮助静音仅铃声/通知，冻结阻后台及通知；含时钟屏保使自动关机不启用。图中Call滑条不证明蜂窝通话，录屏30fps也不证明电子纸刷新率。[E-BXSD-18/01/07/04；E-BOOX-24；E-BXCD-07/14]','',
'**资格排除与仍待查。** Scan Documents仅列Tab Ultra三款；InkSpire仅Tab X C；Work Profile/AI Assistant仅部分机型；鼠标自定义仅Tab系列；键盘保护套需核配件。它们保留来源以便比较，不据此新增本机任务。NeoReader选区提问不证明跨app执行。后续Talk Insights保留“关闭前备份、关闭可能移除笔记”的参考，排除固定V4.1.2基线；正文更新时间本身不能证明固件归属。[E-BXSD-05/12/14/20–25]','',
'**供本项目讨论。** “静音”“后台运行”“已保存”“已读”必须带对象、范围、前提和结果。可借鉴这些差别设计AI动作确认与状态说明，但不据本文确定系统接口、账户制度、新闻形态或新增已确认需求。']
s=s.replace('## 证据\n','\n'.join(section)+'\n\n## 证据\n');p.write_text(s)
p=L/'cases/M-010-palma.md'
section=['## 第44批：移动通用手册的内容与后台条件','','仍限E-PALMA-15的Mobile ePaper Series通用V4.0，不是Palma 2专用当前手册。新增内容只用文件页42–45、48–50、77–82、87–89；没有新增手册视觉或实机观察。逐页短摘与限制见[补读依据](../'+N+')。','','| F编号 | 前提与操作 | 资料所述结果 | 未闭环 |','| --- | --- | --- | --- |']
for fid,pid,task,e,branch,gap,visual,pre,action,result in data:
 if pid=='M-010':section.append(f'| {fid} | {pre}；{action} | {result}。[E-PALMA-15；E-BXSD-16] | {gap} |')
section += ['', 'F05：通用手册p43规定点空白退出查词；p49–50列Annotations导出TXT至Storage/note/同名文件。这补充原通用帮助，不代表本机现在所有导出方式相同。p44–45的OCR说明针对文档图像，不当相机扫描流程；识别页可从Contents/Bookmarks跳回，清OCR的确认及对原书影响未知。[E-PALMA-15]','',
'F07：p88自启动要求app自身支持，并在安装APK后先手动运行一次；成功后后台运行而不出现在最近任务。不能用最近任务列表推断后台工作成功。F08手机传输同Receive/Push分开，手机扫码不当本机相机OCR。F09：p81–82账号列表只保留账户信息，切换需重新登录，服务器也可能切换；这不是已经形成家庭成员隔离与恢复方案。[E-PALMA-15；E-BXSD-18]','',
'PushRead的订阅、网页缓存和阅读位置是不同对象。后续讨论个人报纸及AI收集资料时，应逐项确认哪些内容真正离线可用、由谁读取、已读如何定义；这些是研究问题，未定新闻app或账户设计。']
s=p.read_text().replace('## 证据\n','\n'.join(section)+'\n\n## 证据\n');p.write_text(s)
# Only pages actually used above acquire a new read status; no whole-manual completion.
p=B/'palma-controls-deepening-inventory.csv';rows=read(p)
for x in rows:
 if not x['inventory_id'].startswith('I-PLCD-'):continue
 if not x['source_family'].startswith('V4.0'):continue
 m=re.search(r'PDF page (\d+)',x['locator'])
 if m and int(m.group(1)) in {42,43,44,45,48,49,50,77,78,79,80,81,82,87,88,89}:
  x['coverage_status']='selected_body_read_generic';x['gap']='第44批补读对应页段；通用V4.0，不是Palma 2现版验证；取消/失败/恢复仍缺'
  if 'PushRead' in x['item']:x['flow_ids']='F-M010-11'
write(p,rows)
# Direct design references: evidence statements and unknown branches remain separate.
rpdata=[
('M-004','静音;音频;动作范围','F-M004-14','E-BXSD-07','Volume步骤2/3；静态音量设置','volume-channels.jpg','Tab系列通用说明，当前声音渠道已辨明','点控制中心Mute或长按音量图标','文称静音铃声/通知；图显示媒体/通知/闹钟/Call独立音量','未验证媒体/闹钟静音结果或跨重启保存','本机资格、勿扰/蓝牙/闹钟例外'),
('M-004','冻结;后台;通知','F-M004-14','E-BXCD-07','App Management Settings / Freeze Settings',None,'第三方app的冻结设置','开启冻结','文称阻止包括通知在内的后台操作','未实际冻结或验证数据保存','与自启动/保活/睡眠优先级；解除后的恢复'),
('M-004','屏保;自动关机;功耗','F-M004-14','E-BXSD-01;E-BXCD-14','Screensaver类型和时钟注释','memo-screensaver-example.jpg','Tab系列通用说明，使用含时钟的屏保','选择含时钟屏保','文称自动关机不启用；图为时钟/当天Memo示例','没有实测进入屏保或关机','本机适用、优先级、刷新功耗'),
('M-004','录屏;锁屏;保存','F-M004-21','E-BXSD-04','Screen Recording设置图下半部','screen-record-settings.jpg','录屏可用，已辨明锁屏结束选项','长按录屏快捷项检查锁屏设置','静态文案称锁屏结束并保存；正文结束输出MP4','选项说明不等于实际文件已保存','权限拒绝、空间不足、中断文件可播放性'),
('M-004','帧率;刷新;显示参数','F-M004-21;F-M004-03','E-BXSD-04;E-BXSD-06','录制设置图Recording Framerate；Display全刷频率','screen-record-settings.jpg','比较录像参数与电子纸显示','查看录制帧率和全刷频率','一处是视频帧率，一处按点击/动作触发全刷','未测屏幕物理刷新、残影或60fps','不能用30fps示例值推断屏幕表现'),
('M-004','投屏;反向控制;AI执行','F-M004-22','E-BXSD-08','Screen Mirroring/BOOXDrop；图底部Note2','booxdrop-cast-conditions.jpg','BOOXDrop同LAN浏览器路径','Start Now后在接收端打开所示URL','图注明暂不支持反向控制；区别于正文Miracast路径','未连接或测试断开重连','设备资格、远程编辑、时延、网络失败'),
('M-004','订阅;导入;离线','F-M004-23','E-BXSD-16','PushRead/RSS/Import from OPML',None,'有本地OPML和可用PushRead','RSS菜单Import from OPML选择文件','说明导入订阅来源','不证明正文/图片/书籍已缓存','无效源、重复、部分成功、取消订阅后保留'),
('M-004','已读;阅读统计;日期','F-M004-25','E-BXSD-17','Calendar Memo/Today’s Note/Today’s Read',None,'对应日期有读过或编辑过内容','点日期下笔记或书条目','按最后编辑/阅读日期归类并打开','不是新建、读完或理解证据','本机固件、时区/日界线、删除和恢复'),
('M-004','云文件;复制;同步','F-M004-04','E-BOOX-24','Integrated Third-Party Cloud Storage上传步骤',None,'已绑定云账户，选中本地文档','长按→Copy to Cloud Storage→目标目录→点击文件上传','说明为显式上传文档','不能证明双向同步或整机备份','同名覆盖、断线重试、删除传播'),
('M-004','桌面;卸载;数据对象','F-M004-02','E-BXSD-02','Home Screen/Group Apps/Remove Widgets','home-example.jpg','Tab系列说明，已有分组或小组件','移出组或移除小组件','说明仅改变桌面组织；图只显示主页','不证明卸载应用或删除应用数据','本机适用、撤销、锁布局后反馈/保留'),
('M-010','稍后读;离线;阅读模式','F-M010-11','E-PALMA-15;E-BXSD-16','Mobile V4.0 PDF文件页77–80',None,'同ONYX账号；网页首次加载联网','推送并打开网页，中央菜单切Reading Mode','手册称首次加载后可离线读','未实际断网，完整资源/阅读位置未知','Palma 2现版资格；失效网页、清缓存、重启'),
('M-010','账户;家庭档案;恢复','F-M010-09','E-PALMA-15','Mobile V4.0 PDF文件页81–82',None,'通用手册中的多个ONYX账号','从账户列表切换','只保留账号信息，需要再登录；服务器可能随账号切换','不证明家庭进度隔离或数据备份','Palma 2当前入口、未同步内容和登出恢复')]
p=L/'reference-points.csv';rows=read(p);counts={}
for pid,tags,fs,es,loc,img,pre,act,res,persist,gap in rpdata:
 counts[pid]=counts.get(pid,0)+1
 rows.append(dict(reference_id=f'RP-{pid.replace("-","")}-{counts[pid]:02}',product_id=pid,question_tags=tags,flow_ids=fs,evidence_ids=es,source_locator=loc,artifact_path='artifacts/boox-system-content-deepening/'+img if img else N,precondition=pre,action=act,visible_result=res,evidence_basis='官方正文与已查看静态图；没有实际操作' if img else '官方文档重建；没有实际操作',persistence_result=persist,unknown_branches=gap,version_scope='通用帮助/移动V4.0；目标机型及实际构建未验证',checked_on='2026-10-27'))
write(p,rows)
(R/'work/research/batch44-integration/flows-integration.json').write_text(json.dumps(dict(new_flows=len(flows),updated_old_flows=len(updates),added_reference_points=len(rpdata),total_reference_points=len(rows)),ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(new_flows=len(flows),updated_old_flows=len(updates),reference_points=len(rows))))
