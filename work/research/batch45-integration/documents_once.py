"""Write case, question references and navigation after integrate_once.py."""
import csv
import json
from pathlib import Path

R = Path('/home/workspace/reader')
L = R / 'docs/research/interaction-library'
W = R / 'work/research/batch45-integration'
BRIEF = '045-reading-media-and-content-readiness.md'
brief_path = R / 'docs/briefs' / BRIEF
assert not brief_path.exists(), 'Do not rerun'
details = json.loads((W / 'new-task-details.json').read_text())
case = L / 'cases/M-003-pocketbook.md'
s = case.read_text()
s = s.replace('# M-003 PocketBook InkPad Color 3：笔记、读听与同步对象',
              '# M-003 PocketBook InkPad Color 3：阅读、音频与内容状态')
s = s.replace('2026-10-27复读既有手册补App与设置', '2026-10-27分批复读既有手册补App、设置、阅读与音乐')
old = '本例16条任务最高D2，无实机操作、连续视觉观察、登录、发送邮件或固件执行。前批实际查看27、33、55、62、68并重看13/32；本轮复读33页正文，仅实际看29、37、41、76、77共5页。均为静态手册，没有点击结果或耗时测量。通用FAQ仅作前批有限互证，不以其重启秒数、SD卡或其他型号步骤替代本册。'
new = '本例26条任务最高D2，无实机操作、连续视觉观察、登录、发送邮件或固件执行。此前实际查看13、27、29、32、33、37、41、55、62、68、76、77；第45批复读48页正文，另实际看16、31、48、49、57、80、82共7页。全部为静态手册，没有点击结果或耗时测量。本批范围和图文疑点见[独立摘记](../artifacts/pocketbook-reading-media/source-notes.md)；同源摘页不算独立互证。通用FAQ只作此前有限互证，不以其重启秒数、SD卡或其他型号步骤替代本册。'
assert old in s
s = s.replace(old, new)
old = '本轮为26项补正文页码、控件和任务范围；其中父章节与子项有包含关系，不是26条新任务。未复读项继承[前批目录](../artifacts/pocketbook-controls-deepening/functional-inventory.csv)的状态，未因目录完整就称全功能完成。'
new = '此前为26项补正文；第45批在同一目录就地更新27项，保留原名与页码，不新建重复目录。父章节与子项有包含关系，不是新增功能数。其余条目保留前次状态；最初[目录快照](../artifacts/pocketbook-controls-deepening/functional-inventory.csv)仅供追溯，当前判断用更新后的CSV。目录完整、正文读过与操作验证分别记录。'
assert old in s
s = s.replace(old, new)
rows = {
    '| 外观、安全、规格、充电 |': '| 外观、安全、规格、充电 | 5–11 | 充电与USB模式文字补读；安全/规格仍以章节和型号核对为主，不据参数宣称续航、刷新或耐久实测。 |',
    '| 电源、任务管理、主页、通知、手势、键盘 |': '| 电源、任务管理、主页、通知、手势、键盘 | 12–17、81–84 | F01补触摸映射确认、键盘输入/布局；Cancel、输入保存和后台关闭影响仍未验证。 |',
    '| 前光、网络、蓝牙 |': '| 前光、网络、蓝牙 | 17–20、69、78–79 | F09蓝牙；F18网络与隐藏SSID；F25自动/手动与时段调光；真实连通、切源及恢复未知。 |',
    '| Library/Store/Browser/Notes |': '| Library/Store/Browser/Notes | 21–27、59、63–65 | F03笔记；F19购买/下载、F20网页/外链返回、F21书库查找；未购买、清数据或操作账户。 |',
    '| Music Player/Audiobooks/Libby |': '| Music Player/Audiobooks/Libby | 30–39 | F07本地Audiobooks、F10馆借、F17音乐列表与播放分开；最小化/退出、删除和断点仍缺实证。 |',
    '| Dropbox、Send-to-PocketBook |': '| Dropbox、Send-to-PocketBook | 41–45、70–71 | F06邮件；F24指定目录同步及Autosync触发；不扩大为全Dropbox或整机备份。 |',
    '| 阅读导航、目录、书签、词典与笔记 |': '| 阅读导航、目录、书签、词典与笔记 | 46–54 | F02/03查词和笔记；F21跳页、层级目录、书签删除确认；计页期间部分功能不可用，搜索仅入口。 |',
    '| Voice、文本/PDF版式、链接与加载 |': '| Voice、文本/PDF版式、链接与加载 | 55–62 | F05 TTS；F20外链返回；F22页码单位/排版/引擎；F23 USB与ACSM授权/下载。PDF重排可能缺图表，未验证朗读顺序。 |',
    '| 书库分类、Cloud |': '| 书库分类、Cloud | 63–68、71–72 | F21分类/排序/视图及菜单；F04云端/本地状态与删除。标为已读不证明理解，整理菜单不当已完成全链。 |',
    '| PIN、Accessibility、Personalize |': '| PIN、Accessibility、Personalize | 72–84 | F11 PIN、F12档案、F13读屏；F25调光、F26开/关机图；F01触摸映射。普通睡眠图、隔离、持久化及刷新性能未知。 |',
}
lines = s.splitlines()
for i, line in enumerate(lines):
    for prefix, replacement in rows.items():
        if line.startswith(prefix):
            lines[i] = replacement
s = '\n'.join(lines) + '\n'
s = s.replace('本轮静态观察（2026-10-27）：', '此前App与设置批次的静态观察（2026-10-27）：')
s = s.replace('本轮未看其他新页面图像，正文阅读不升级为连续视觉证据。',
              '该次仅查看这5页图像；第45批另补下述7页。正文阅读不升级为连续视觉证据。')
new_section = '''## 阅读、音乐与内容准备补充（第45批）

以下仍是同册D2文档重建。每行有目标、前提、动作/所述结果与未闭合分支；没有实际购买、授权、复制、同步或删除。系统键盘与手势补回原F01，不把每个控件增加为一条功能。

| F编号与任务 | 前提与入口 | 操作与资料所述结果 | 状态边界、退出及缺口 | 来源/印刷页 |
| --- | --- | --- | --- | --- |
'''
for n, title, pre, steps, gap, pp, ids in details:
    new_section += f'| F-M003-{n:02d} {title} | {pre} | {steps} | {gap} | {ids.replace(";", "；")}；{pp} |' + '\n'
new_section += '''
原F01补充：阅读点按区选择动作后须勾号确认，82图另有Cancel但没有取消效果。四方向滑动、两侧拖动可配置，拖动可改前光/色温/语音音量/字号；当前映射不能由默认示意推断。屏幕键盘长按特殊字符、globe切布局，系统最多3布局；16页文字说空格显示所选语言，图中却为space，差异保留。输入的取消、草稿、重启恢复均未验证。[E-PBK-01；E-PBRC-01，16–17、81–84]

原F04的Cloud继续限定文件及阅读位置的文档范围；F24的Dropbox文件目录和Autosync规则不能移作Cloud的删除/进度或整机备份依据。F23移除Adobe授权后“不可用”也不等于删除本地文件。商店购买、链接材料、下载文件、已打开与已读状态必须分别核对，这是本项目G-002相关的候选分析问题，不是已选定接口方案。

本批七页静态摘页作为同一派生记录E-PBRC-01保存，详见[逐页观察](../artifacts/pocketbook-reading-media/observed-pages.json)。16页键盘、31页音乐、48页阅读菜单、49页调光、57页排版计页、80页Logo、82页触摸映射均来自同一手册；其中49页的自动调光询问仅为文字规则，80页主要为正文嵌图标，不能说看过完整弹窗或选择流程。

'''
anchor = '## 本地笔记、导出与再次编辑的边界'
assert anchor in s
s = s.replace(anchor, new_section + anchor)
case.write_text(s)

rp_path = L / 'reference-points.csv'
rp = list(csv.DictReader(rp_path.open()))
assert not any(r['product_id'] == 'M-003' for r in rp)
scope = 'PB743K3 InkPad Color 3；官方手册路径6.10.2854；实际安装版未知'
refdata = [
    ('音乐;后台;退出;恢复', '17', '30–32', 'manual-p31.png', 'Music Player已有列表和曲目', '选择播放控制、Collapse或Exit', '静态图区分最小化和退出，并列播放模式及音量；未执行', 'manual_static_controls_and_text', '列表有保存入口；后台音频与断点没有持久结果证据', '最小化后是否继续;退出/重启恢复;队列与文件删除范围'),
    ('阅读;书签;删除;确认', '21', '50', 'source-notes.md', '已有阅读页，可见右上折角标记', '再次点折角并确认删除', '手册文字明确需确认；未观察确认界面或删除结果', 'manual_text_only', '书签列于目录是文档描述；未核写盘、撤销与同步', '取消;误删恢复;换排版后位置;跨设备删除'),
    ('阅读;页码;进度;排版', '22', '57', 'manual-p57.png', '文本书已打开，进入阅读设置', '切换内置原书计页或调整排版', '文字区分原书页和屏数，一原书页可多屏；首次/更改后重算，静态开关非实际结果', 'manual_text_and_static_controls', '文字称之后再打开不每次重算；未验证重启或文件变化', '重算取消/失败;进度换算;笔记定位;引擎切换'),
    ('阅读;外链;浏览器;返回', '20', '24–25、59', 'source-notes.md', '阅读页外链进入内置Browser', '浏览器菜单Exit', '文档称返回阅读；未观察返回到哪一行/页', 'manual_text_only', '无原阅读位置、缩放或页面状态恢复记录', '网页失败;多级外链;下载中断;精确返回点'),
    ('内容导入;购买;下载;离线', '19', '22–23', 'source-notes.md', '网络、Store账户及有效购买条件', 'Buy/支付后从My Purchases下载', '文档将已购和下载分开；下载完成列于Library/Downloads与最近事件', 'manual_text_only', '仅有文档目录结果；未购买、检验文件或离线打开', '支付取消;重复下载;空间不足;DRM;地区权限'),
    ('内容导入;USB;DRM;授权', '23', '59–61、71、85', 'source-notes.md', '开机USB/PC link，或ACSM及Adobe授权条件', '复制后安全移除；ACSM打开授权再下载整书', '文档整书在Digital Edition目录；移除授权后保护文档不可用', 'manual_text_only', '未复制或授权；不可用不等于文件删除', '授权失败;旧网页步骤;两处移除路径差异;重复/损坏/恢复'),
    ('同步;目录;账户;备份', '24', '41–42、71', 'source-notes.md', 'Dropbox账户与Wi-Fi；指定同步目录', '授权后自动同步或Synchronize Now，可改Sync Folder', '文档授权时启用Autosync；仅访问云端Dropbox PocketBook应用目录', 'manual_text_only', '双端文件同步是文档声明；未验证冲突、删除或恢复', '改目录;退出账户;部分成功;双向删除;重装恢复'),
    ('前光;手势;自动;确认', '25;01', '17–18、49、78–79、83', 'manual-p49.png', '自动调节已启用，阅读边缘当前映射为调光', '边缘拖动；或编辑自定义时段图并退出确认', '文字说先询问是否关闭自动；图为边缘示意，未展示实际询问弹窗', 'manual_text_and_static_diagram', '时段图确认即应用为文档描述；未验证重启保留', '拒绝切手动;映射已改变;时间不准;跨模式覆盖'),
    ('手势;输入;配置;确认', '01', '81–83', 'manual-p82.png', 'Personalize中的阅读触摸区域配置', '选区域→选动作→勾号确认', '静态图有Cancel与勾号，正文说明确认；选中动作非实际默认', 'manual_text_and_static_controls', '未观察取消、配置写盘及实际手势命中', '取消结果;修改冲突;读屏模式;重启恢复'),
    ('显示;开机;关机;图片', '26', '80–81', 'manual-p80.png', 'Logos中的Boot或Power-off；本地图片', '自定义选图Set，或随机列表选最多10图', '文档Current Page仅在Boot列表，关机另有none；页面主要正文与图标', 'manual_text_and_embedded_icons', '未观察启动/关机或配置保存；不推普通睡眠图', '取消;图片失效;随机顺序;普通睡眠/锁屏规则'),
]
for i, data in enumerate(refdata, 1):
    tags, nums, pp, artifact, pre, action, result, basis, persistence, unknown = data
    rp.append(dict(reference_id=f'RP-M003-{i:02d}', product_id='M-003', question_tags=tags,
                   flow_ids=';'.join(f'F-M003-{int(n):02d}' for n in nums.split(';')),
                   evidence_ids='E-PBK-01' + (';E-PBRC-01' if artifact.endswith('.png') else ''),
                   source_locator=f'同册PDF/印刷{pp}页',
                   artifact_path='artifacts/pocketbook-reading-media/' + artifact,
                   precondition=pre, action=action, visible_result=result, evidence_basis=basis,
                   persistence_result=persistence, unknown_branches=unknown, version_scope=scope,
                   checked_on='2026-10-27'))
with rp_path.open('w', newline='') as f:
    writer = csv.DictWriter(f, fieldnames=list(rp[0]), lineterminator='\n')
    writer.writeheader(); writer.writerows(rp)
assert len(rp) == 78

brief_path.write_text('''# 第四十五期：阅读、音乐与内容准备

2026-10-27。继续研究功能交互，没有进入全系统设计。

**目前仍未深入到全部产品的每项功能和分支。** 早期主矩阵主要帮助发现产品；当前资料库已有文档流程、静态画面与少量实际网页操作，但65组产品都只完成了部分覆盖。手册写了什么、图中出现什么、操作后发生什么与重启后保留什么，必须分别取证。

这轮补PocketBook InkPad Color 3同一固定手册的阅读、音乐、网络、商店及文件准备流程。复读48页正文，实际看7页静态摘图；新增10项局部D2任务，补2项旧任务。没有设备操作。所有摘页同源，只新增1个派生证据记录、没有新增独立来源URL，避免用页数制造研究深度。

## 这次深入到了哪些会影响设计的细节

| 表面功能 | 能回查的具体区别 | 对本项目的候选问题 |
| --- | --- | --- |
| 音乐播放 | 最小化、退出是不同控件；移出列表的文字与菜单“从设备移除”存在矛盾 | “继续播放”“删掉这首”分别作用于什么对象，结果如何核对？ |
| 阅读进度 | 原书一页可能对应多个屏幕；调整排版会重算屏数 | AI报告进度、安排学习量时使用什么单位？ |
| 书签 | 再点页角标记后还需确认删除 | 确认、取消、误删恢复应分别留下什么结果？ |
| 返回阅读 | 外链进入内置浏览器，用其Exit返回；具体恢复位置未说明 | “回来继续读”需要保存哪层上下文，怎样验证？ |
| 图书导入 | 商店购买后还有下载；ACSM还需授权后取得整书 | 何时才能准确报告“已准备好，可以离线读”？ |
| 同步 | Dropbox仅覆盖指定应用目录，授权时启用自动同步 | 同步开关、文件到达、删除传播、备份恢复如何区分？ |
| 前光手势 | 自动模式下手动调节会先询问；边缘手势还可改绑音量或字号 | AI应读取当前配置和模式，不能只依据默认动作表 |

这些是[官方手册](https://support.pocketbook-int.com/fw/743K3/u/6.10.2854/manual/User_Manual_InkPad_Color_3_EN.pdf)的限定描述及图文核对，具体页码和图像见[独立摘记](../research/interaction-library/artifacts/pocketbook-reading-media/source-notes.md)。删除范围、保存与恢复没有实际证据时，资料库保留未知；没有替产品补出“应该会这样”的结论。

## 以后怎样重复使用

按问题进入[设计参考入口](../research/interaction-library/design-reference-guide.md)，按产品看[PocketBook案例](../research/interaction-library/cases/M-003-pocketbook.md)，或筛选[交互依据表](../research/interaction-library/reference-points.csv)中的RP-M003-01～10。每条都有前提、动作、结果依据、保存状态、未知分支、版本和原件定位。原65项手册目录就地更新27项，父章节、按钮与完整任务不混算。

本批中文背词/单词机与Meebook的限定检索没有补到新的核心操作链；旧评测、自动生成页面和型号未明的短视频线索不升级证据深度。PocketBook也仍缺真实输入、支付/授权、取消、断网、重启和跨端恢复。研究进度以这些具体缺口判断，不能以链接或文件数量宣布完成。

对成人，页码单位、资料就绪和精确返回值得继续比较；对5–6岁孩子，读听切换、误触与退出后接续仍需实际体验。上述均为研究启发，没有证明学习效果，也没有新增已确认需求或决定系统实现。
''')

latest = '- [第四十五期：阅读、音乐与内容准备](/home/workspace/reader/docs/briefs/' + BRIEF + ')：补页码单位、书签确认、音乐控制及购买/下载/授权/同步的边界；新增10条可检索依据。'
root_readme = R / 'README.md'
lines = root_readme.read_text().splitlines()
idx = next(i for i, v in enumerate(lines) if v.startswith('- [第四十四期：'))
lines[idx] = latest
root_readme.write_text('\n'.join(lines) + '\n')
brief_index = R / 'docs/briefs/README.md'
s = brief_index.read_text(); idx = s.index('- [第四十四期：')
brief_index.write_text(s[:idx] + latest + '\n' + s[idx:])
library_readme = L / 'README.md'
s = library_readme.read_text(); start = s.index('- [第四十四期：'); end = s.index('## 先说明当前研究做到了哪里')
s = s[:start] + latest + '\n- [全部研究简报](../../briefs/README.md)：按时间查发现与边界；后续设计优先用下方任务、证据及问题入口。\n\n' + s[end:]
library_readme.write_text(s)

appendices = {
    'analysis-status.md': '\n第45批以PB743K3固定手册补阅读、音乐及内容准备：48页正文复读、7页静态图、10新D2、2旧F、1派生E和1旧E采集；没有新增独立来源URL或产品实操。原65行目录就地更新27项，RP增10条到78条。中文背词/单词机与Meebook限定搜索没有新增核心链，不能升级D1。全部65组仍部分覆盖；见[简报45](../../briefs/045-reading-media-and-content-readiness.md)。\n',
    'backlog.md': '''
## 第45批：内容就绪与操作结果

- PocketBook原65行目录27项补到正文/任务，不再笼统称仅有宣传或标题；F17–26仍是文档重建。下一步应固定设备/版本，核音乐Collapse/Exit与删除范围、排版后的定位、书签删除取消、外链精确返回及重启保留。
- 内容导入：Store购买/下载、USB复制、ACSM授权/整书、Dropbox指定目录及Cloud阅读位置分别验证。需实际断网、失败、重复、删除传播与恢复；不能因手册说同步就当整机备份，不能将旧Adobe网站步骤当现行指引。
- 输入/显示：键盘空格标签图文不同；自动调光询问未有实看弹窗；触摸映射Cancel与确认后保存未核；Boot/Power-off的screensaver文案不扩大为普通睡眠。全刷参数不是60fps或无残影证据。
- 本批中文背词/单词机与Meebook限定检索未获得新核心操作链。旧52audio评测、旧视频转录、明确自动生成的聚合页和型号不明手册线索不新增F/E，不重复已受限路线；下一次应取得具体版本的完整教程/实际操作材料。详细排除范围留在本批work审计中。
''',
    'comparison.md': '''
## 阅读位置、内容就绪与文件范围（第45批）

[PocketBook F17–26](cases/M-003-pocketbook.md)补出文档层的条件：最小化/退出、原书页/屏数、书签确认、外链返回、购买/下载、ACSM/整书及指定目录同步分别记录。与[BOOX系统与内容](cases/M-004-boox-neoreader.md)的OPML/全文和云复制/同步边界、[TIME样刊](cases/M-047-time-for-kids.md)实际观察的页书签/音频接续/刷新重置可按同一问题比较，但证据等级不互相借用。

对G-002，候选研究问题是让AI获取当前对象、计量单位、有效配置和实际执行结果；对G-001，阅读页数、菜单标为已读与真正理解分别评估。以上没有确定数据模型、接口或学习策略。直接依据见RP-M003-01～10及[同册摘记](artifacts/pocketbook-reading-media/source-notes.md)。
''',
    'design-reference-guide.md': '''
## 查询内容是否真正准备好（第45批）

“找到了书”“购买成功”“已有文件”“能离线打开”要用不同依据。新增[PocketBook任务与缺口](cases/M-003-pocketbook.md)及RP-M003-01～10，可从以下问题进入：

| 问题 | 直接定位 | 证据边界 |
| --- | --- | --- |
| 换字号后页数变化是否表示进度增加 | [57页计页说明及控件](artifacts/pocketbook-reading-media/manual-p57.png)，RP03 | 文档区分原书页与屏数；未实际重排 |
| 关播放器会不会继续播放 | [31页Exit/Collapse](artifacts/pocketbook-reading-media/manual-p31.png)，RP01 | 仅控件，后台和断点仍需实测 |
| 已购或拿到ACSM是否已能读 | [22–23、59–61页摘记](artifacts/pocketbook-reading-media/source-notes.md)，RP05/06 | 文档还要求下载或授权；未购买/复制 |
| 同步是否覆盖所有数据、能否恢复 | [Dropbox目录及自动条件](artifacts/pocketbook-reading-media/source-notes.md)，RP07 | 仅指定文件目录，删除冲突/恢复未知 |
| 边缘手势和开机图是否总有同一效果 | [映射82页](artifacts/pocketbook-reading-media/manual-p82.png)与[Logo80页](artifacts/pocketbook-reading-media/manual-p80.png)，RP08–10 | 当前配置/自动模式/启动场景都有限定，普通睡眠不外推 |

正式引用应携带这些条件与缺口。RP是增量检索层，未覆盖的任务仍查产品目录与案例；不能将已建立78条检索依据称为全库逐页面状态已完成。
''',
    'inventories/README.md': '\n第45批[PocketBook当前65项目录](../batches/pocketbook-app-controls-deepening-inventory.csv)就地更新27项，补正文页码、操作和未知分支；新任务F17–26承接阅读/音乐/内容准备，原F01继续承接系统控件。未新建重复目录，正文读过、静态图看过与完整操作验证分开。\n',
    'batches/README.md': '\n第45批 `pocketbook-reading-media-*` 新增10D2和1组同源静态派生E，0新增独立URL；2旧F及E-PBK-01追加采集在原持有批次更新。48页正文复读、7页实看静态摘页、27原目录行更新，RP增10条。没有设备操作，见[简报45](../../../briefs/045-reading-media-and-content-readiness.md)。\n',
}
for rel, text in appendices.items():
    p = L / rel
    p.write_text(p.read_text().rstrip() + '\n' + text)
ideas = R / 'docs/ideas.md'
ideas.write_text(ideas.read_text().rstrip() + '\n\n2026-10-27继续I-021/I-022：PocketBook补阅读计页、书签确认、音乐与内容准备，分别保留购买/下载/授权/同步的对象及结果条件；同源七页静态图不升级为操作。新增10条按问题检索依据，见[第四十五期简报](/home/workspace/reader/docs/briefs/045-reading-media-and-content-readiness.md)。所有产品仍部分覆盖，没有新增已确认需求或进入全系统设计。\n')
print(json.dumps(dict(brief=str(brief_path.relative_to(R)), references_added=10,
                      total_references=len(rp), case_flows=26), ensure_ascii=False))
