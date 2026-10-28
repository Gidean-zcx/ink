"""Batch45: fixed PocketBook manual, bounded task records. Run once."""
import csv
import hashlib
import json
import shutil
from pathlib import Path

R = Path('/home/workspace/reader')
L = R / 'docs/research/interaction-library'
W = R / 'work/research/batch45-integration'
A = L / 'artifacts/pocketbook-reading-media'
B = L / 'batches'
assert not A.exists(), 'Already integrated; do not rerun'
A.mkdir()
DATE = '2026-10-27'
EID = 'E-PBRC-01'
RAW = R / 'work/research/interaction-depth/pocketbook-6.10.2854.pdf'
RAW_SHA = '87bf56b461cd298b7c53421c5b21f72f1498e4c060d4eea89844545667030101'
SCOPE = 'PB743K3 / InkPad Color 3; official manual path 6.10.2854; installed build unknown'
URL = 'https://support.pocketbook-int.com/fw/743K3/u/6.10.2854/manual/User_Manual_InkPad_Color_3_EN.pdf'


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def dump(p, obj):
    p.write_text(json.dumps(obj, ensure_ascii=False, indent=2) + '\n')


def csvout(p, rows):
    with p.open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]), lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)


assert sha(RAW) == RAW_SHA
assert EID not in (L / 'evidence.jsonl').read_text()
pages = sorted(set(range(11, 26)) | set(range(30, 33)) | {41, 42} |
               set(range(46, 52)) | set(range(56, 62)) | set(range(63, 67)) |
               set(range(69, 72)) | set(range(78, 87)))
assert len(pages) == 48
observations = {
    16: ('屏幕键盘', '静态英文QWERTY布局含globe、?123、Shift、退格、回车与隐藏图标；特殊字符下拉未展开。正文说空格显示所选语言，图中却写space；不自行消除差异，也未观察切换后输入保留。'),
    31: ('音乐播放控制', 'Mediatheka/Playlist/Now Playing三标签；Exit箭头指向Home图标，Collapse指向下划线。Normal有选中示例，另有Repeat once和Shuffle；静态选中不证明默认。未观察最小化或退出后的音频。'),
    48: ('阅读菜单与定位', '顶端返回、目录、搜索、书签，底部设置/旋转/笔记/词典/Voice；滑条36/72。左下箭头旁13含义未由此页文字解释，不猜成历史位置。没有打开搜索或完成跳页。'),
    49: ('边缘前光手势及目录', '边缘拖动示意和正文；自动调节已启用时会询问是否关闭自动来自文字，未出现实际询问弹窗。不能把图示当手势执行或左右边缘默认映射验证。'),
    57: ('排版与计页单位', 'Page/Font/Screen设置片段含字体大小、连字符、状态栏、剩余页与内置计页开关；示例12pt等值不当默认，未观察重算或保存。'),
    80: ('开机图与关机图说明', '正文嵌有编辑/返回/加号等图标，非完整Logo选择器画面；Current Page出现在Boot列表，Power-off列表续在81页。screensaver措辞不证明普通睡眠同一机制。'),
    82: ('阅读触摸区域映射', '两幅静态面板示意区域与动作菜单；顶端Cancel和勾号，菜单Move to next page为选中示例。正文要求勾号确认；未观察取消结果、实际触摸或断电保存。'),
}
obs = []
for n, (title, note) in observations.items():
    src = R / f'work/research/batch45-pocketbook-depth/manual-p{n}.png'
    dst = A / src.name
    shutil.copyfile(src, dst)
    obs.append(dict(evidence_id=EID, pdf_page=n, printed_page=n, title=title,
                    observed_on=DATE, actual_view='view_image', hands_on=False,
                    continuous_sequence=False, raw_work_path=str(src.relative_to(R)),
                    archived_artifact=str(dst.relative_to(L)), sha256=sha(dst), note=note))
dump(A / 'observed-pages.json', obs)

notes = '''# PocketBook：阅读、音乐与内容准备的证据摘记

2026-10-27。范围固定PB743K3 / InkPad Color 3，官方6.10.2854路径95页英文手册。仅复读已存原件，没有重新核定最新固件、登录、付款、发送、配对、导入或实机操作。原PDF SHA256为87bf56b461cd298b7c53421c5b21f72f1498e4c060d4eea89844545667030101；原件位置由E-PBK-01记录。实际安装构建未知，PDF元数据时间不当发布日期。

本轮正文复读文件/印刷11–25、30–32、41–42、46–51、56–61、63–66、69–71、78–86，共48页。仅16、31、48、49、57、80、82共7页实际查看图像。页数相同；所有摘页来自同一原件，不是7个独立来源，不构成连续操作。新增E-PBRC-01仅作为此组静态摘页记录，原文依据仍用E-PBK-01。

## 任务、结果和缺口

| 页码 | 资料支持的任务细节（转述） | 必须保留的边界 |
| --- | --- | --- |
| 11–17、84–85 | 开机可进主页或上次书；任务管理可关闭单应用。键盘长按特殊字符，globe切换布局，系统最多配置3种布局；密码输入可切可见性。PC连接提供PC link/Charge模式，设置另有每次询问。 | 关闭应用、锁屏、关机不等同；充电与连接模式措辞不足以推定所有供电组合。键盘文字与16页space图示不同。没有中文输入、撤销、草稿或后台保存实测。 |
| 18–19、69 | 需网应用可连接已知网络；未知网络显示选择，受保护网络需密码。通知快捷项按已知连接条件开关/选网；隐藏网络输入SSID、安全方式/凭据后返回该节再Connect；More可设DHCP/手动/代理。 | Scanning与网络列表不证明已连通互联网；密码错、门户、断网重连、超时/取消和凭据保留未给完整结果。 |
| 22–23 | Store需网络及登录；个人区含已购、愿望、语言与商店选择。Buy后进入支付选项；已购内容在My Purchases，可稍后下载。下载结束出现在Library/Downloads和Recent Events。 | 购买权益与本地文件分开；未登录/付款，没见取消、失败、退款、地区权限与DRM结果。 |
| 24–25、59 | Browser在地址栏输入后Enter；可滚动、缩放及从菜单查看历史/下载/书签，设置有清历史/cookie及JS/图片开关。阅读外链进入浏览器后用其菜单Exit返回阅读；书内链接模式另有左上退出。 | 没有实际访问或清除；未说明外链返回时的精确位置、页面状态、下载中断与空白错误。不能把退出浏览器等同完整恢复。 |
| 30–32 | Music Player菜单加文件/目录建列表，重排、保存或清空；播放面板含循环/正常/随机、前后曲、播放/暂停及音量。长按可看信息、加队列或移除。31图区分Exit与Collapse。 | 32页文字为移出列表，菜单图为Remove from the device，物理删除范围仍矛盾。队列/歌单/底层文件、后台播放和断点分别未知。手册提MP3，同型号RN提FLAC但未归属具体播放器，不宣告格式上限。 |
| 41–42、71 | Dropbox需账户与Wi-Fi；授权创建本机同步目录及云端Application下的Dropbox PocketBook目录，手册称仅该云目录可访问。加入对应目录的文件联网同步；可改/新建本机Sync Folder，显式Synchronize Now。授权时自动启用Autosync，也可关闭。 | 此服务不同于PocketBook Cloud阅读进度同步，不是全Dropbox访问或整机备份。未核双向删除、冲突、部分成功、退出账户后的副本与重装恢复。 |
| 46–50、63–65 | 从最近/书库开书，手册称重开回上次停处；排版计页时有省略号且部分功能不可用。阅读菜单提供滑条及数字跳页；目录可展开层级，含章节/书签/笔记/书信息；右上折角添加书签，再点并确认删除。书库可分组/排序及切简略、详细、列表视图，长按有标为已读/合集/收藏等入口。 | “标为已读”是菜单动作，不证明真实完成或理解；列表整理与源文件修改分别待核。未验证跳页失效、搜索结果、误删撤销、重排后定位或重启持久化；书库长按入口不各计完整任务。 |
| 56–59 | 文本排版含字号/行距/边距等页设置、字体和屏幕项。内置原书页码可一页对应多个屏幕；关掉后按屏计数，首次/重设时重算。PDF/DjVu含整页、连续滚动、分栏和裁边；Reflow仅PDF，手册警告表格/图像可能不显示。可切阅读引擎，PDF还可调对比/亮度/gamma。 | 计数变化不表示读更多；裁边是显示操作，不据此宣称修改或删除文件内容。没有不同文档精度、引擎迁移笔记、设置回退或持久化记录。 |
| 59–61、71、85 | USB须设备开机并选PC link，电脑以可移动存储复制后安全移除。ACSM是下载链接材料，拷入后打开、输入Adobe凭据激活再取得整书到Digital Edition目录；电脑ADE路径也要求先授权。取消Adobe授权后保护文档不可用。 | 不是复制ACSM即取得可读整书。59页microUSB与本型号Type-C矛盾；Adobe网页旧Flash流程不当现行操作指南。61页Service删除授权与71页Accounts and Synchronization路径不一；不可用不等于文件被删除。未执行授权/复制。 |
| 17–18、49、78–79 | 通知栏开关/滑条及Menu长按调前光；SMARTlight手册默认按时间自动。阅读边缘拖动遇自动模式会先询问关闭；自定义时段图逐时设亮度/色温，退出图表确认即应用，可再激活/编辑/复制/改名/删。 | 时间准确性是自动调节前提；询问来自文字不是实看弹窗。83页边缘可改绑音量/字号，不能假定所有当前配置都调光。没有照度、色温、续航或护眼效果测量。 |
| 80–83 | Boot Logo和Power-off Logo分别配置；自定义选图后Set，随机列表最多10图。Current Page只列在Boot；关机另有none。阅读点按区域→选动作→勾号确认；四方向滑动及两侧拖动可改。全刷按翻页数配置。 | 文中又称screensaver，不扩大为睡眠/锁屏规则。82页有Cancel但未见取消结果。全刷质量/耗电是说明，非高刷或无残影证明。 |

## 实看静态页

每页的准确观察及未知内容见[逐页记录](observed-pages.json)。可直接打开：[键盘16](manual-p16.png)、[音乐31](manual-p31.png)、[阅读48](manual-p48.png)、[调光49](manual-p49.png)、[计页57](manual-p57.png)、[Logo80](manual-p80.png)、[触摸映射82](manual-p82.png)。图示值均不自动当默认；图文差异保留，不以研究者推测补齐。

## 后续复用

任务由[案例F17–26及原F01/04](../../cases/M-003-pocketbook.md)承接；系统输入控件补回原F01，不把每个按钮当新功能。RP-M003-01～10按条件、结果、保存证据和未知分支索引。全部为D2文档重建；需要实机补同一文件/配置下的取消、断网、退出、重启和恢复。没有验证Plus能够直接实现，也未将本批机制升级为产品需求。
'''
(A / 'source-notes.md').write_text(notes)
capture = dict(checked_on=DATE, access_mode='existing_local_pdf_body_and_selected_static_pages',
               raw_work_path=str(RAW.relative_to(R)), raw_sha256=RAW_SHA,
               text_pages=pages, text_pages_count=len(pages),
               locator='PDF/printed pp11–25,30–32,41–42,46–51,56–61,63–66,69–71,78–86; static pp16,31,48,49,57,80,82',
               visual_observed=True, visual_pages=list(observations), hands_on=False,
               continuous_sequence=False, archived_artifact=str((A / 'source-notes.md').relative_to(L)),
               sha256=sha(A / 'source-notes.md'),
               visual_audit_path=str((A / 'observed-pages.json').relative_to(L)),
               note='既存同一原件复读；不新增独立来源URL，不代表全部95页或所有操作分支完成。')
new_e = dict(evidence_id=EID, product_id='M-003', title='阅读、音乐和系统输入的七页静态手册摘页',
             url=URL, source_type='official_manual_figure_bundle', checked_on=DATE,
             source_updated_on=None, version_scope=SCOPE, app_build=None,
             firmware_version='6.10.2854 document baseline; installed unknown',
             access_mode='local_pdf_render_actually_viewed', locator='PDF/printed pp16,31,48,49,57,80,82',
             note='七页同源静态图；键盘图文差异、退出/最小化、页码单位、自动调光询问仅文字、Logo场景与映射确认。不是操作或七个独立来源。',
             hands_on=False, visual_observed=True,
             archived_artifact=capture['archived_artifact'], sha256=capture['sha256'],
             capture_state='static_pages_and_bounded_notes_archived', supersedes=None,
             derived_from='E-PBK-01', raw_work_path=capture['raw_work_path'], raw_sha256=RAW_SHA,
             visual_audit_path=capture['visual_audit_path'],
             artifact_hashes={o['archived_artifact']:o['sha256'] for o in obs})
new_e['artifact_hashes'][capture['visual_audit_path']] = sha(A / 'observed-pages.json')
(B / 'pocketbook-reading-media-evidence.jsonl').write_text(json.dumps(new_e, ensure_ascii=False) + '\n')
original_owner = B / 'reader-evidence.jsonl'
existing = [json.loads(x) for x in original_owner.read_text().splitlines()]
for e in existing:
    if e['evidence_id'] == 'E-PBK-01':
        old = json.loads(json.dumps(e))
        e.setdefault('additional_captures', []).append(capture)
        dump(W / 'old-evidence-update.json', dict(before=old, after=e))
original_owner.write_text(''.join(json.dumps(x, ensure_ascii=False) + '\n' for x in existing))

flows = []
details = []


def task(n, title, pre, steps, gap, pp, visual=False, extra=''):
    ids = 'E-PBK-01' + (';' + EID if visual else '') + (';' + extra if extra else '')
    flows.append(dict(flow_id=f'F-M003-{n:02d}', product_id='M-003', task=title, depth='D2',
                      evidence_ids=ids, known_branches=pre + '；' + steps, gaps=gap,
                      visual_status='static_manual_pages_only' if visual else 'not_observed',
                      hands_on='false', case_path='cases/M-003-pocketbook.md', checked_on=DATE))
    details.append((n, title, pre, steps, gap, pp, ids))


task(17, '建立音乐列表并控制播放', '本地音乐文件，Music Player',
     '应用菜单加文件/文件夹，重排/保存/清空列表；播放面板切循环/正常/随机、前后曲、播放暂停及音量。长按可看信息/加队列/移除；31图分别标Exit与Collapse。',
     '队列/歌单/原文件不混同；32页移出列表正文与Remove from the device菜单矛盾；后台、重开断点、删除确认及FLAC播放器归属未核。', '30–32', True, 'E-PBK-05;E-PBCD-07')
task(18, '选择或配置Wi-Fi网络', '需要网络的应用或通知栏/Settings → Wi-Fi',
     '已知网络可自动连接，未知显示选择；安全网络输入密码。More含DHCP/手动/代理。隐藏网络输入SSID、安全方式及凭据，返回该节Connect；Scanning是扫描状态。',
     '网络列表/配置不等于互联网可用；错密、门户、取消、超时、重连及凭据保存未核。', '18–19、69')
task(19, '从商店取得并下载图书', '网络、Store账户及相应购买条件',
     '商店授权后选书/Buy进入支付选项；已购内容列于My Purchases，可稍后下载。文档称下载完成后出现于Library/Downloads和Recent Events；个人区可改语言/商店或退出。',
     '未登录付款；购买权益不当文件落地。取消、支付/下载失败、退款、地区/DRM与再下载条件未知。', '22–23')
task(20, '浏览网页并从书中外链返回', 'Browser或阅读中的外部链接，网页需网络',
     '地址栏输入/Enter，链接/拖动滚动/缩放；菜单含历史、下载、收藏及清历史/cookie、JS/图片设置。阅读外链打开浏览器后，菜单Exit回阅读；书内链接模式另从左上退出。',
     '未实际打开或清除；返回书中精确位置、空白错误、下载取消/保存及隐私数据范围未知。', '24–25、59')
task(21, '在书库找书、定位并整理页书签', '最近书或Library中已有可读书',
     '分组/排序/视图找书；打开后菜单滑条或数字跳页，目录含章节/书签/笔记且层级可展开。右上折角加书签，再点并确认删除。重开回停处为文档描述；计页中省略号且部分功能暂不可用。',
     '搜索仅见入口；标为已读不证明完成。误删撤销、跳页错误、重排后的定位、退出/重启持久化及源文件变化未核。', '46–50、63–65', True)
task(22, '调整排版、计页和阅读引擎', '阅读菜单 → Settings或书库长按Open with',
     '文本可设字号/行距/边距/字体等；内置原书页码可一页多屏，关掉后按屏计数并在首次/更改时重算。PDF/DjVu可整页/滚动/分栏/裁边，PDF另可Reflow及图像调整；可切阅读引擎。',
     'Reflow仅PDF，表格/图像可能不显示；页数不等于学习量，裁边不当删除原文。未测格式准确性、引擎切换后的笔记定位、设置恢复或耗时。', '56–59', True)
task(23, '通过USB或ACSM准备可读图书', 'USB路径需开机/电脑；ACSM路径另需网络与Adobe授权',
     '连电脑选PC link，出现可移动存储后复制并安全移除。ACSM拷入打开，按授权表输入凭据，取得整书到Digital Edition目录；电脑ADE也需授权。删除Adobe授权后保护文档不可用。',
     'ACSM不是整书；手册microUSB与本型号Type-C、移除授权两路径差异保留。旧Adobe网页步骤不当现行指南；未实操，复制错误/空间不足/重复与撤销未知。', '59–61、71、85')
task(24, '设置Dropbox目录并同步文件', 'Dropbox账户与Wi-Fi',
     'Accounts and Synchronization → Dropbox登录；创建本机同步目录及云端应用目录，只能访问后者。文件加入对应目录后联网同步；可改/新建Sync Folder及Synchronize Now。授权时Autosync自动开，可关。',
     '不是全Dropbox或整机备份，也不等于PocketBook Cloud阅读位置同步。删除传播、冲突、部分失败、换目录/退出账户及重装恢复未核。', '41–42、71')
task(25, '切换前光自动/手动并定时调节', '通知面板、阅读边缘或Personalize → Frontlight',
     '通知滑条/开关及Menu长按；SMARTlight按时间自动需时钟准确。阅读边缘调节遇自动模式先询问关闭；时段图选时间并设亮度/色温，退出确认后应用，可激活/编辑/复制/改名/删。',
     '询问仅正文；边缘可改绑音量/字号，当前映射未核。拒绝询问、跨模式覆盖、重启恢复及亮度/续航测量缺失。', '17–18、49、78–79、83', True)
task(26, '分别配置开机图与关机图', 'Personalize → Logos；自定义图在设备',
     'Boot Logo或Power-off Logo选默认/封面/自定义/随机；自定义选图后Set，随机最多10图。Current Page只在Boot列表，关机另有none；Gallery也可设图。',
     '手册screensaver措辞不扩大到普通睡眠；未看实际启动/关机、图片失效、选择取消、循环顺序或配置保存。', '80–81', True)
assert len(flows) == 10
csvout(B / 'pocketbook-reading-media-coverage.csv', flows)
dump(W / 'new-task-details.json', details)

flow_updates = []
for path in B.glob('*-coverage.csv'):
    if path.name == 'pocketbook-reading-media-coverage.csv':
        continue
    rows = list(csv.DictReader(path.open()))
    changed = False
    for row in rows:
        if row['flow_id'] == 'F-M003-01':
            before = dict(row)
            row['known_branches'] += ' 点按区选动作后勾号确认；边缘拖动可改光/色温/音量/字号。键盘长按特殊字符、globe切布局，最多配置3种；密码可切可见性。'
            row['gaps'] = '82页Cancel仅入口，未见撤销/保存结果；16页空格语言标签图文不同。当前映射、输入保留、后台保留/关闭影响、配置失败及HID适配未知；读屏手势独立。'
            row['evidence_ids'] += ';' + EID
            row['visual_status'] = 'static_manual_pages_only'
            row['checked_on'] = DATE
        elif row['flow_id'] == 'F-M003-04':
            before = dict(row)
            row['gaps'] += ' Dropbox的指定目录文件同步见F24，不能借其Autosync推定本Cloud的备份、删除或进度范围。'
            row['checked_on'] = DATE
        else:
            continue
        changed = True
        flow_updates.append(dict(owner=str(path.relative_to(R)), before=before, after=dict(row)))
    if changed:
        csvout(path, rows)
assert len(flow_updates) == 2
dump(W / 'old-coverage-updates.json', flow_updates)

invpath = B / 'pocketbook-app-controls-deepening-inventory.csv'
inv = list(csv.DictReader(invpath.open()))
inv_before = json.loads(json.dumps(inv))
changes = {
    'Charging Battery': ('11', '充电与USB模式文字复读；未单列完整任务', '', '设备开机；已有开机连接PC提供PC link/Charge', '供电模式措辞不推所有组合；充电时长/失败/续航未测'),
    'Device management': ('12', 'D2对应任务/仍有缺口', '01', '开/关/锁/10–12秒重启；任务管理关闭单应用', '锁屏/关闭/关机不同；后台和未存内容影响未知'),
    'Gestures': ('15', '系统控件已读；不是新增独立任务', '01', '点按/长按/滑动/快速划动/捏合', '普通手势不套读屏；实际触发/冲突未知'),
    'Working with On-Screen Keyboard': ('16–17、84', '系统输入局部任务补证', '01', '长按特殊字符；模式切换；globe/最多3布局；密码眼睛', 'space图与语言标签文字不同；输入/取消/草稿保留未知'),
    'Brightness': ('17–18、49、78–79', 'D2对应任务/仍有缺口', '25', '通知调光/Menu长按；自动/手动；时段图确认应用', '时间前提；拒绝切手动、恢复/功耗未核'),
    'Connecting to the Internet': ('18–19、69', 'D2对应任务/仍有缺口', '18', '已知自动/未知选网；安全密码；隐藏SSID/安全方式/Connect；More', '错密/门户/取消/重连和真实联网结果未核'),
    'Library [Applications]': ('22、63–65', 'D2对应任务/仍有缺口', '21', '进入书库；分类/排序/视图和长按菜单', '入口不等于完整合集/删除/收藏链'),
    'Store': ('22–23', 'D2对应任务/仍有缺口', '19', '授权/Buy/支付选项；My Purchases/下载/Library-Downloads/Recent Events', '未购买；权益、下载和可读状态分别未知'),
    'Browser': ('24–25、59', 'D2对应任务/仍有缺口', '20', '地址/Enter；缩放/滚动；历史/下载/收藏/清除；Exit回阅读', '精确返回点/网页错误/清除确认和保存未核'),
    'Music Player': ('30–32', 'D2对应任务/仍有缺口', '17', '文件/目录建列表，排序保存清空；播放模式/音量/曲目；Exit/Collapse', '移出列表与Remove from the device矛盾；背景与恢复未知'),
    'Services': ('41–42、70–71', '功能域局部补读；非一个完整功能', '06;24', '邮件接收与Dropbox指定目录同步分开', '不能据一服务推全服务恢复'),
    'Dropbox PocketBook': ('41–42、71', 'D2对应任务/仍有缺口', '24', '授权建目录/自动同步；手动同步/改Sync Folder；仅指定云目录', '删除传播/冲突/换目录/退出副本和恢复未知'),
    'Reading Books': ('46–50', 'D2对应任务/仍有缺口', '21', '最近/书库打开；重开位置文档描述；计页中部分不可用', '无实测恢复或完整搜索'),
    'Gestures in Reading Mode': ('47、81–83', 'D2对应原系统任务/仍有缺口', '01', '默认示意与可配置区域/滑动/拖动；勾号确认', '当前配置/取消/持久化未核'),
    'Reading Menu': ('48', 'D2对应任务/仍有缺口', '21', '中央/Menu打开；滑条/数字跳页；搜索等仅入口', '左下13含义未释；搜索/误触/退出结果未知'),
    'Adjust SMARTlight and frontlight Brightness in reading mode': ('49、83', 'D2对应任务/仍有缺口', '25;01', '边缘调光；自动模式先询问关闭；边缘可另映射', '未见实际询问弹窗及拒绝分支'),
    'Contents': ('49–50', 'D2对应任务/仍有缺口', '21', '章节/书签/笔记/书信息；层级展开折叠/上下文', '重排/定位/异常未核；不把所有菜单各当功能完成'),
    'Bookmarks': ('50', 'D2对应任务/仍有缺口', '21', '右上折角添加；再点并确认删除；菜单亦可操作', '确认界面/取消/误删恢复/同步未核'),
    'Settings [Reading Books]': ('56–59', 'D2对应任务/仍有缺口', '22', '排版/原书页与屏数/重算；PDF模式/裁边/Reflow/引擎', 'PDF重排可能缺图表；保存/切引擎定位未知'),
    'Following Links': ('59', 'D2对应任务/仍有缺口', '20', '书内链接模式左上退出；外链浏览器Exit回阅读', '返回精确位置和失败网页未知'),
    'Loading Files': ('59–61', 'D2对应任务/仍有缺口', '23;04;06', 'USB PC link/复制/安全移除；ACSM/授权/整书下载', 'microUSB旧字样不改型号；旧Adobe网页流程不当当前指南'),
    'Library [Reading Books]': ('63–65', 'D2对应任务/仍有缺口', '21', '分组/排序/三视图；长按已读/合集/收藏/删除/上传等', '整理入口不代表持久化；已读不当掌握'),
    'Wi-Fi': ('18–19、69', 'D2对应任务/仍有缺口', '18', 'Network Access/扫描与列表；隐藏网络/参数/连接', '真实连通、错密/取消/重连未知'),
    'Accounts and Synchronization': ('70–71', '局部正文与对应任务；未全服务验证', '04;06;23;24', '邮件接收/Dropbox自动与手动/Adobe授权/Cloud范围分开', '删除传播/冲突/退出与恢复未知；Adobe移除路径有差异'),
    'Personalize': ('78–84', '部分D2及设置正文复读', '01;12;25;26', '时段图/Logos/点按勾号确认/滑动拖动/全刷/Startup/档案', '普通睡眠Logo规则未知；控件设置不证明性能/隔离'),
    'Languages': ('16–17、84', '系统输入局部任务补证', '01', '界面语言/最多3键盘布局；globe切换', '语言文字与space图不同；切换保存未核'),
    'Maintenance': ('85', 'D2对应任务/仍有缺口', '16;23', 'USB默认PC link/Charge/每次询问；配置密码/Factory Reset/Demo', 'USB连接角色不等于已复制；重置细项保留旧缺口'),
}
updated = []
for r in inv:
    if r['manual_item'] not in changes:
        continue
    pp, state, nums, controls, gap = changes[r['manual_item']]
    r.update(body_pages_reread=pp, checked_scope=state,
             flow_ids=';'.join(f'F-M003-{int(n):02d}' for n in nums.split(';') if n),
             evidence_ids='E-PBK-01' + (';' + EID if any(p in pp.split('、') for p in ['16–17','48','49','56–59','78–84','30–32']) else ''),
             controls_or_tasks=controls, gap=gap,
             read_basis='existing_local_manual_body_reread_with_bounded_static_pages', checked_on=DATE)
    updated.append(r['manual_item'])
missing = sorted(set(changes) - set(updated))
dump(W / 'inventory-update-audit.json', dict(updated_items=updated, unmatched_labels=missing,
                                            before=inv_before, after=inv))
assert not missing, missing
csvout(invpath, inv)
dump(W / 'integration-result.json', dict(new_flows=[f['flow_id'] for f in flows],
      new_evidence=[EID], new_distinct_urls=0, old_flows_changed=2,
      old_evidence_captures_added=1, inventory_rows_changed=len(updated),
      text_pages=pages, viewed_pages=list(observations), products_still_partial=True))
print(json.dumps(dict(new_flows=len(flows), new_evidence=1, text_pages=len(pages),
                     viewed_pages=7, inventory_rows_changed=len(updated)), ensure_ascii=False))
