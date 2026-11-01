import csv, hashlib, json, shutil
from pathlib import Path

ROOT=Path('/home/workspace/reader')
LIB=ROOT/'docs/research/interaction-library'
WORK=ROOT/'work/research/batch44-boox-depth'
ART=LIB/'artifacts/boox-system-content-deepening'
assert not ART.exists(), 'One-time integration; do not rerun'
ART.mkdir(parents=True)
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def writecsv(p, rows):
    with p.open('w', newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
def readcsv(p): return list(csv.DictReader(p.open()))
def writejson(p, x): p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')

# Root-authored analysis; IDs refer to the already-read official body cache.
# Scope is a source qualification, never a device capability assertion.
entries=[
(10701146091284,1,'F-M004-14','Tab系列说明；本机适用未核','Settings → Desktop & Screensaver → Screensaver；单图/轮播可从本地、系统、云或推送选图。Memo显示当天日历备忘，Transparent保留当前画面，月历仅部分机型。含时钟的屏保会使自动关机不启用；关机图另设。','没有实际入睡/关机、优先级、更新功耗或失败反馈。图仅为城堡屏保和时钟备忘示例，不是选图或类型选择过程。'),
(10701108549140,2,'F-M004-02','Tab系列说明；本机适用未核','长按应用后移动至Dock或屏幕边缘；拖入/菜单归组，组内Remove from group移出。长按空白进入Edit；Tidy Up整理所有页。Widgets可添加、移除、跨页移动，部分可Resize；点空白退出编辑。','移出组/移除小组件不是卸载。图只是主页，无拖动/编辑前后状态；锁定布局后的错误反馈、撤销与重启保留未知。'),
(10701630316436,3,'F-M004-20','通用帮助；V4.1.2逐项未核','控制中心选Screenshot或Scrolling Screenshot；入口缺失可Edit拖入。导航球可配置截图/长截图。三指下滑需先启用；部分应用或网页会限制截图。','本页三指设置写System Navigation → Gestures；Navigate页写More Settings → Screenshots，保留路径差异。保存位置、长截图边界、限制时提示和取消未知。'),
(25403769498004,4,'F-M004-21','通用帮助；V4.1.2逐项未核','控制中心Screen Recorder，圆形开始、X结束；说明输出MP4至Storage/Movies/Captures，可点缩略图查看/分享。长按快捷项先调录制设置。静态图列音源、帧率、锁屏结束并保存、触点显示。','图中30fps是录制参数示例，不证明默认值或屏幕物理刷新率；锁屏保存只是选项文案，未实际完成录制或验证中断文件。'),
(10701620011540,5,'','排除：仅Tab Ultra/Ultra C/Ultra C Pro','本文明确限定三款Tab Ultra相机。流程为命名新扫描、拍摄或导入、编辑/替换/删除页，再生成、逐页OCR和导出Notes/PDF。','不是Note Air5 C或Palma 2扫描App的适用证据；不为本机新增任务。'),
(10701172864916,6,'F-M004-03;F-M004-14','通用帮助；字体导入仅部分机型','Settings → Display设置系统字体/字号、亮度、自动睡眠/关机及全刷频率；导入字体先Font → Import。全刷频率按点击/动作次数描述。','系统字号、应用DPI和单文档排版分开；点击次数不等于fps。具体默认值、即时生效、重启保留和本机字体导入未知。'),
(10701109999636,7,'F-M004-14','Tab系列说明；本机适用未核','控制中心音量条调音量；长按图标分功能调整。Mute说明只静音铃声和通知。静态设置图可见Media、Notification、Alarm、Call四条音量。','不推断媒体/闹钟同时静音；Call滑条不证明蜂窝硬件或通话链。没有实际听音、勿扰例外及蓝牙输出验证。'),
(10701622160916,8,'F-M004-22','通用帮助；三种投屏路径分别限定','Miracast路径要求兼容Windows 10/11及同网，控制中心搜索或手选接收端，正文称可查看/控制。BOOXDrop同LAN → Start Now → 接收端浏览器地址，支持旋转/全屏，配图注明暂不支持反向控制。AnyDesk另需双方安装与权限配置。','三条路径不可合并。未安装或连接；连接失败、断开与重连、输入控制和实际延迟未知。BOOXDrop仅显示画面不证明AI可读业务数据或执行动作。'),
(10701173983508,9,'','通用帮助；本机路径未核','Settings → More Settings → Language and Input选择语言/当前输入法。ONYX键盘设置可拆分键盘，通过中间箭头拖动位置。','未见实际输入、候选词、切换后焦点、撤销、词库/设置恢复，保留参考条目，不计完整输入任务。'),
(10701147047188,10,'','通用帮助；本机路径未核','Settings配置WLAN或Bluetooth；控制中心短按对应项开关，长按进入网络或设备选择。','短按切状态与长按选对象不同；密码错误、配对失败、休眠重连和取消分支未说明。'),
(10701137102100,11,'F-M004-03','文称V4.1起四种前灯模式；本机未操作','第一滑条调强度，第二调色温；可通过侧手势或Settings → Display → Brightness设置。','前灯调整不改变内容刷新模式；具体本机四模式值、自动变化与保存未知。'),
(20259982794772,12,'','待资格核对：原文仅Tab系列','Settings → More Settings → Mouse可调指针形状/大小/速度与滚动方向；推荐Fast/Ultrafast及参数只属文档建议。','不能据此确认Note Air5 C或Palma 2有该设置；没有指针实际轨迹、延迟或设备兼容性记录。'),
(10701256390548,13,'F-M004-03','通用帮助；彩色与黑白设备分支不同','控制中心EinkWise → Color Mode → Custom；设置按应用生效。Light Color Filter限黑白屏，Vividness/Color Brightness限彩屏；NeoReader另有单文件设置。','不把黑白/彩色选项合成一个设备菜单；重置、跨app切换生效与卸载后保留未知。'),
(15194669819796,14,'','仅部分机型；V3.3.2起；本机资格未核','Apps & Notifications → Work Profile → Enable，按新窗口继续，再开启所选app并等待安装；说明桌面出现独立数据的副本。','不是家庭成员档案证明；新窗口详细步骤、关闭/删除后的数据、通知隔离、备份恢复未知，不计本机已验证任务。'),
(10701363849108,15,'F-M004-24','通用NeoBrowser帮助；本机构建未核','菜单栏按预设改字号；右上菜单Page Visibility Enhanced开关增强显示。长按词 → Dictionary，弹窗可切词典；另有全屏阅读模式。','全屏不等于移除网页广告/推荐。未给全屏退出、弹窗关闭后的选区、网页保存、离线和模式持久化。'),
(10701356797332,16,'F-M004-23;F-M010-11','通用PushRead帮助；两机现固件未核','RSS → Directory → Add subscription选择公共源Subscribe；Custom先建组再加源OK。菜单Import from OPML选择本地文件。OPDS在独立页签按类似方式添加；Push接收稍后读网页。','订阅清单导入不是文章全文/书籍已下载。无效源、重名、部分导入、取消订阅、已读与离线保留未知；离线条件另按通用移动手册限定。'),
(10701353744916,17,'F-M004-25','现行通用帮助；不可把新版Events回填V4.1.2','按日期保存Memo；Today’s Note按该日最后编辑，Today’s Read按该日最后阅读，点条目回打开。现行页另给Events导入第三方日历账户，以及加事件/今日提醒入口。','阅读过/编辑过不是读完/新建。事件与提醒表单、确认、重复/时区、删除和日历冲突缺失；不把入口当已建立完整日程任务。'),
(10701398942356,18,'F-M004-04;F-M010-08','BOOX Assistant说明；同账号/服务器及所述同LAN条件','配套手机BOOXDrop → Send to BOOX，设备Receive查看；反向传输可Save To Phone。手机Push加文件后设备Push查看。还列手机扫码预览、Gallery屏保和PushRead。','扫码在手机侧，不是BOOX相机OCR；传输不等于书库收录。断线/重复/低空间/恢复未闭环。'),
(10701493804820,19,'F-M004-03;F-M004-16','NeoReader当前帮助；本机V4.1.2逐项未核','Display中调对比度、饱和度、图像锐化与淡化水印；Color Brightness影响彩色层而非黑色文字。Image Smoothing仅Regal模式。','显示处理不等于修改/删除原文件；保存范围、退出还原、灰阶和残影未实测。'),
(20261817911828,20,'','仅部分机型；现行NeoReader AI帮助','给出可重排文档矢量图、手写圈选区域、文字高亮三类提问上下文；菜单可选模型和预设字数。正文列登录/未登录每日生成字符额度。','没有模型调用或额度测试；不把该额度当稳定套餐承诺。阅读选区问答不证明任意页面语义、全库权限或跨app执行，本机资格未知。'),
(37475824326420,21,'','EMR笔分支；需核笔与机型','Settings → More Settings → Calibration按指引校准；说明磁性保护套/支架可能干扰，建议移开。','没有完整校准靶点、失败反馈及本机配笔资格；不将EMR校准与InkSpire设置合并。'),
(37475843783700,22,'','排除：本文InkSpire限定Tab X C','笔吸附侧边中部充电，可看弹窗电量或控制中心Stylus；可设置触觉反馈。','不倒推Note Air5 C或Palma 2具备该笔/充电/震动功能。'),
(10701627053460,23,'','键盘保护套与具体型号条件待核','Pogo连接后说明自动横屏；Settings → System Settings → Physical keyboard设置布局，多布局Ctrl+Space切换。触控板自定义明确仅Tab Ultra C Pro。','不把专用保护套路径套到任意蓝牙/USB键盘；本机配件兼容、断开恢复、输入法冲突未知。'),
(10701617416852,24,'','特定键盘说明；本机配件资格未核','连接弹窗或通知Shortcut查看按键表。Fn系列含前灯/全刷/音量/截图；Cmd+Enter或D回Home，Cmd+L锁屏，Alt+Fn+4关闭app。','回桌面、锁屏、关app作用不同；按键名称和组合不能外推所有键盘，文档未说明关闭是否保存。'),
(42939290151700,25,'','后续Lab功能；排除固定V4.1.2基线','所列地区/语言和网络条件满足后，Settings → Lab Features → Explore启用并接受提示，再New Talk或Notes → Create。关闭前需备份；说明关闭可能移除本机Talk Insights笔记，无备份可能无法恢复。','保留后续版本参考，不新增本机基线任务；没有实际同意条款、录音或删除，地区清单和当前可用性需采用前重核。'),
(10701171762452,None,'F-M004-14','Tab系列睡眠帮助；复用E-BXCD-14','短按电源睡眠，长按后Power off；Display设自动睡眠/关机，Power查电池。建议睡眠立即断开无线以省电。','省电建议不是听书场景结论；与移动通用手册的保留网络/蓝牙/音频条件分别阅读。'),
(10701262170644,None,'F-M004-03;F-M004-14','通用应用管理；复用E-BXCD-07','Apps & Notifications分别管理权限、通知、自启动、冻结和显示优化；冻结阻止全部后台操作，包括通知。Reset Tutorial与Reset all app optimization对象不同。','自启动不等于后台可靠播放。安装确认、权限拒绝、卸载后的数据、恢复结果仍未知。'),
(25213043150996,None,'F-M004-02','通用导航；复用E-BXCD-03','首页右滑Smart Assistant可搜索/增删小组件，桌面设置开关；它与AI Assistant不同。清通知、回Home、隐藏导航栏后上滑及侧手势开关分别处理。','截图设置路径与专篇不一致；没有逐屏退出/返回、冲突或状态持久化验证。'),
(10701308914964,None,'F-M004-14','安装入口复用E-BXCD-08；仍D1条件','部分机型预装Google Play；另可浏览器下载APK或传文件后安装。','未见实际安装提示、失败、取消及卸载恢复；正文已读不升级为完整安装流程。'),
(10701398359572,None,'F-M004-04','第三方云复用E-BOOX-24','Library → Cloud绑定账户，点文档下载并按账户/邮箱组织书架。上传从Storage长按 → Copy to Cloud Storage，选目标并点击文件上传。文档格式与图片/视频分流。','显式复制上传不能证明双向同步、删除传播、冲突合并或整机备份；同名覆盖和失败未知。'),
(10701348022676,None,'F-M004-04','目录中转；复用E-BXCD-01','BOOXDrop总页链接电脑和手机传输帮助；详细动作引用各自页面。','本页仅中转，不新增独立任务或来源计数。'),
]
cache={str(x['id']):x for x in json.loads((ROOT/'work/eink-systems-research/boox-articles.json').read_text())}
reuse={10701171762452:'E-BXCD-14',10701262170644:'E-BXCD-07',25213043150996:'E-BXCD-03',10701308914964:'E-BXCD-08',10701398359572:'E-BOOX-24',10701348022676:'E-BXCD-01'}
eid={a:(f'E-BXSD-{n:02}' if n else reuse[a]) for a,n,*_ in entries}
images={
'screensaver-types.jpg':(10701146091284,'screensaver-image-example.jpg','城堡画面；不是类型选择器或选图操作。'),
'screensaver-images.jpg':(10701146091284,'memo-screensaver-example.jpg','2022/11/01时钟及Today’s Memo；历史示意，不能证明当前日期或设置步骤。'),
'home.jpg':(10701108549140,'home-example.jpg','Library/Notes小组件、应用与Dock；不是编辑模式或拖动过程。'),
'screen-record.jpg':(25403769498004,'screen-record-settings.jpg','1872×1404、16Mbps、30fps、Internal sound source及锁屏结束保存/触点开关；示例值不当默认或屏幕刷新测量。'),
'volume.jpg':(10701109999636,'volume-channels.jpg','媒体、通知、闹钟、Call四滑条；勿扰显示off。Call不构成蜂窝通话证据。'),
'mirror-step.jpg':(10701622160916,'booxdrop-cast-conditions.jpg','同LAN地址/二维码，底部注明暂不支持反向控制；未建立连接。'),
'mirror-browser.jpg':(10701622160916,'booxdrop-cast-browser.jpg','浏览器中的笔记画面和旋转/全屏控件；不证明远程编辑。')}
captures=json.loads((WORK/'captures.json').read_text())['records']
image_records=[]
for old,(aid,new,obs) in images.items():
    src=WORK/'images'/old;dest=ART/new;shutil.copyfile(src,dest)
    cap=next(x for x in captures if Path(x['local_path']).name==old)
    assert sha(dest)==cap['sha256']
    image_records.append(dict(article_id=aid,evidence_id=eid[aid],artifact_path=str(dest.relative_to(LIB)),sha256=sha(dest),url=cap['url'],checked_on='2026-10-27',http_status=cap['status'],visual_observed=True,hands_on=False,observation=obs))
note=['# BOOX系统控制与内容入口：逐项依据','',
'查阅：2026-10-27。31篇正文复读2026-10-26既存官方公开帮助缓存；正文没有重新联网抓取。7幅官方静态附件于本次各GET一次并查看。无设备、App或投屏操作；无连续视觉过程。以下是D2局部文档依据或适用性排除，**不证明Note Air5 C V4.1.2及Palma 2当前固件支持整套操作**。','',
'公开分类有59篇文章，本次补读31篇，与此前局部研究合并；文章覆盖不等于逐控件、逐分支完成。标题/更新时间/原文body散列见[source-captures.json](source-captures.json)。原件仍在work，正式库保留短摘记和图像，不复制整站正文。','']
source_records=[]
for aid,n,flows,scope,fact,gap in entries:
    a=cache[str(aid)]
    note += [f'## {eid[aid]} · {a["title"]}', '',f'[官方原页]({a["html_url"]})；article {aid}；网页更新 {a["updated_at"]}。定位：正文各小节；具体字段见下述摘记。','',f'**适用范围：** {scope}。','',f'**文档所述：** {fact}','',f'**未证与限制：** {gap}','']
    source_records.append(dict(article_id=aid,title=a['title'],url=a['html_url'],source_updated_at=a['updated_at'],edited_at=a.get('edited_at'),evidence_id=eid[aid],flow_ids=flows,model_version_scope=scope,body_capture_on='2026-10-26',body_read_on='2026-10-27',body_sha256=hashlib.sha256(a['body'].encode()).hexdigest(),raw_work_path='work/eink-systems-research/boox-articles.json',hands_on=False))
note += ['## 静态图逐张观察','', '图中日期/参数仅属示例；图片是独立静态参考，不构成操作前后序列。','']
for x in image_records: note += [f'- [{Path(x["artifact_path"]).name}]({Path(x["artifact_path"]).name})：{x["evidence_id"]}；{x["observation"]}']
note += ['', '## E-PALMA-15 · 移动系列V4.0手册补读','',
'[原PDF](https://ebookreader.dk/images/upload/Onyx%20BOOX%20Palma%202%20Pro%20User%20Manual.pdf)共103文件页，封面为Mobile ePaper Series Universal V4.0，零售商文件名不是Palma 2专用证明。本次只将下列页段用于结论，不宣称整册逐页验证；正文读取不新增视觉E。','',
'| 文件页 | 可复用细节 | 限制 |','| --- | --- | --- |',
'| 42–45 | 查词后点空白退出；已装第三方词典可下拉切换。图像OCR后可选文标注/查词、从Contents/Bookmarks跳识别页，右上OCR可清内容。 | OCR清除确认、源文件修改及当前Palma 2适用未核；不继承Tab Ultra相机。 |',
'| 48–50 | 页书签可再点移除；Annotations可跳回原文/编辑/删除，导出TXT默认在Storage/note/同名文件。 | 原书删除、导出覆盖、取消和回导未知；本文手写只显示其他BOOX同步的结果。 |',
'| 77–80 | Push可由网页/手机送入，同ONYX账号；网页首次加载需联网，加载后可离线读。默认Web Mode，点中央菜单切Reading Mode/字号/全刷。 | 不证明完整图片/附件均缓存，未测试断网、失效内容或清缓存；没有已读/完成判定。 |',
'| 81–82 | 账户列表可切换，只保留账户信息，切换后需再登录；不同账号服务器随登录切换。 | 不是无缝家庭成员切换、内容隔离或备份恢复证明。 |',
'| 87–89 | 自启动依赖app具备监控系统能力；APK安装后需手动运行一次；成功后后台运行且不出现在最近任务。 | 不在最近任务不等于未运行；是否成功需看实际后台结果，锁屏/权限/冻结优先级未核。 |','',
'另发现第三方托管的德语“Palma 2”手册线索，但专用版本/译本出处未核，含Android 11等历史内容；未据此替换通用原件或增加本机结论。','',
'## 对本项目的研究启发（建议）','',
'G-002的“AI能操作”需核动作对象与真实结果，例如通知静音、媒体暂停、冻结app、隐藏控件、复制云文件分别影响不同状态。G-001的阅读记录应分开打开/最后阅读/读完/理解，不据日历或时长自动推断掌握。具体接口、账户与数据结构仍留待原型认可后设计。','']
(ART/'source-notes.md').write_text('\n'.join(note))
writejson(ART/'source-captures.json',dict(checked_on='2026-10-27',sources=source_records,images=image_records,body_capture_method='local_reread_of_2026-10-26_public_cache',device_operation=False))
notesrel=str((ART/'source-notes.md').relative_to(LIB));notesha=sha(ART/'source-notes.md')
es=[]
for aid,n,flows,scope,fact,gap in entries:
    if not n: continue
    a=cache[str(aid)];imgs=[x for x in image_records if x['article_id']==aid]
    es.append(dict(evidence_id=eid[aid],product_id='M-004',title=a['title'],url=a['html_url'],source_type='official_help',access_mode='local_saved_public_help_body_read_and_selected_static_attachments',checked_on='2026-10-27',source_updated_on=a['updated_at'],version_scope=scope+'；Note Air5 C V4.1.2为案例基线，当前帮助不等于该构建验证',app_build=None,firmware_version='case baseline V4.1.2; source varies',locator=f'Article {aid}; body sections; source-notes section {eid[aid]}',note=fact+' 限制：'+gap,hands_on=False,visual_observed=bool(imgs),archived_artifact=notesrel,sha256=notesha,capture_state='short_notes_and_selected_official_static_images_archived',supersedes=None,raw_work_path='work/eink-systems-research/boox-articles.json',raw_sha256=sha(ROOT/'work/eink-systems-research/boox-articles.json'),article_body_sha256=hashlib.sha256(a['body'].encode()).hexdigest(),artifact_hashes={x['artifact_path']:x['sha256'] for x in imgs}))
(LIB/'batches/boox-system-content-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in es))
# Append local rereads to owning records, retaining old captures and hashes.
targets=set(reuse.values())-{'E-BXCD-01'}|{'E-PALMA-15','E-PALMA-04','E-PALMA-13'}
for p in (LIB/'batches').glob('*-evidence.jsonl'):
    rows=[json.loads(x) for x in p.read_text().splitlines() if x];changed=False
    for e in rows:
        if e['evidence_id'] not in targets: continue
        capture=dict(checked_on='2026-10-27',access_mode='batch44_local_reread_no_body_network',archived_artifact=notesrel,sha256=notesha,hands_on=False,visual_observed=False,locator='Corresponding article section; E-PALMA-15 only PDF pages42–45/48–50/77–82/87–89')
        e.setdefault('additional_captures',[]).append(capture);changed=True
    if changed:p.write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in rows))
# Reuse the existing expanded directory, including qualification exclusions.
ip=LIB/'batches/boox-reading-controls-deepening-inventory.csv';rows=readcsv(ip)
em={str(a): (n,f,s,t,g) for a,n,f,s,t,g in entries}
for r in rows:
    if r['entry_kind']!='article' or r['article_id'] not in em:continue
    n,f,s,t,g=em[r['article_id']]
    r.update(flow_ids=f,evidence_ids=eid[int(r['article_id'])],body_read_on='2026-10-27',read_basis='batch44_existing_official_body_read',control_scope=s,gap=g)
    if '排除' in s:r['coverage']='other_model_excluded' if 'V4.1.2' not in s else 'outside_fixed_baseline'
    elif '资格' in s or '部分机型' in s or 'Tab系列' in s:r['coverage']='body_read_model_scope_unconfirmed'
    elif int(r['article_id'])==10701348022676:r['coverage']='body_read_link_hub_only'
    else:r['coverage']='body_read_partial_task_or_control_scope'
writecsv(ip,rows)
writejson(ROOT/'work/research/batch44-integration/evidence-integration.json',dict(new_evidence=len(es),read_articles=len(entries),static_images=len(image_records),static_evidence=sum(x['visual_observed'] for x in es),notes_sha256=notesha))
print(json.dumps(dict(new_evidence=len(es),read_articles=len(entries),static_images=len(image_records),static_evidence=sum(x['visual_observed'] for x in es))))
