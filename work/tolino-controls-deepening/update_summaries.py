from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library'
def prepend_table(path,rows):
 s=path.read_text();marker='| --- | --- | --- |\n';assert marker in s;s=s.replace(marker,marker+rows,1);path.write_text(s)
brief=R/'docs/briefs/013-household-recording-and-audio-roles.md'
brief.write_text('''# 第十三期：家庭内容、练习录音与音频角色

2026-10-27。本批继续把功能名拆成可复查的任务、条件和数据变化。研究对象固定Tolino shine第五代T302、CASIO E-W100/E-W220和FiiO JM21；仍未决定本项目账户方案、系统架构或具体界面。

## 具体细节如何影响后续设计

| 设计问题 | 竞品中的具体依据 | 需要保留的区分 |
| --- | --- | --- |
| 家里共享一本书，是否也共享学习进度？ | Tolino家庭帮助说明各自账户保有各自阅读位置和笔记，不能互看整个书库；按书、按成员授权，离开家庭会撤销相互分享。[F-M058-14](../research/interaction-library/cases/M-058-tolino.md) | 内容所有权、访问权限和个人记录分开研究。自传书/PDF不适用该商业分享机制；不能直接当成本项目账户要求。 |
| 孩子跟读后，AI还能找回刚才的录音吗？ | CASIO临时发音对比结束即删录音；“我的演讲”另有五个保存位置，覆盖与放弃各有确认。[F-M024-14/15](../research/interaction-library/cases/M-024-casio.md) | 临时练习、保存作品和播放断点是不同对象。若希望AI回访，必须先确认记录是否存在；对比播放也不等于发音评分。 |
| “帮我播放”到底是控制哪台设备？ | JM21的USB-DAC、蓝牙接收、AirPlay和本地播放有不同音频来源，遥控又是另一种关系；退出模式不保证恢复原曲位置。[F-M034-04/07/11/12/15](../research/interaction-library/cases/M-034-fiio.md) | 输入来源、输出设备、控制端、模式和断点分别查询。不要把配对成功、遥控成功或文件导入当成已经出声。 |
| 选择了服务，就代表已经登录吗？ | 实际匿名操作Tolino公开选馆器：选择Hamburg后落到外站欢迎页仍需选馆；回首页刷新却已有最近选择，期间未登录。[F-M058-19及审计](../research/interaction-library/artifacts/tolino-controls-deepening/browser-operation-audit.md) | 最近项、身份认证、借阅资格和版权码分别判断；AI不能用最近项代替登录成功证据。只观察桌面网页，未验证T302。 |

对5–6岁孩子，值得继续研究的是“练习后能否回听自己的表达”“家长分享内容但孩子保有自己的进度”；没有据这些说明书证明游戏动力或学习效果。对成人，“接着上次”要能指明恢复的是哪份内容、哪个任务和哪段状态。以上均为助手建议，未自动升级为需求。

## 原件审读与操作带来的纠正

- Tolino英文更新页仍列5.12.232736，德文T302表已列5.18.264769且表内有版本拼写/目标疑点；文件名也与FAQ不同。保留出处与适用范围，未替厂商补出一条升级路线。
- CASIO的设置初始化有用户名、语言和时间例外；异常复位又是另一动作。说明书未保证学习簿会保留，不能据“复位”二字承诺恢复数据。背诵卡删除表的标签和说明范围冲突也保留。
- FiiO 3.3.4发布说明已重做播放页和均衡器；旧手册图不能作为该版界面。外部M3U从Folder打开，导入内部Playlist后是否回写原文件仍未知。
- Tolino网页无匹配搜索只留下空列表；浏览器后退时本次保留搜索词但列表恢复A字头。截图与DOM先后采集造成的过渡差异明确列在审计中，未拼成虚构的成功借书链。

本批有8张正式手册摘页和6张实际浏览器截图。设备任务仍为文档/静态证据；网页仅一个有限任务。新增27条证据、23任务，补21个旧任务的83字段；3个FiiO重复来源已合回原E，未用重抓充数。491条新目录记录包含父项、重复语言、内容目录及非功能章节，不是491项完成的功能。

## 后续如何重复查阅

从[横向设计问题](../research/interaction-library/comparison.md)进入案例F任务，再查E记录、页码/原件摘记和观察方式，最后检查缺口。逐条[目录](../research/interaction-library/inventories/README.md)独立说明哪些条目只有标题，哪些已读正文或形成任务；同一任务的新分支补回原编号。

[准确覆盖量](../research/interaction-library/case-index.md)现为65组产品、705任务，但65组仍全部部分覆盖。未完成每个产品的每个功能，也没有设备触感、刷新、音频打断或学习效果的实测依据。接下来继续按核心场景和明确缺口取证，不能以文档数量作为完成标准。
''')
p=R/'README.md';s=p.read_text();marker='## 项目资料\n\n';s=s.replace(marker,marker+'- [第十三期：家庭内容、练习录音与音频角色](/home/workspace/reader/docs/briefs/013-household-recording-and-audio-roles.md)：最新简报；家庭进度、临时/保存录音、音频方向及公开选馆器实际分支。\n',1).replace('：最新简报；reMarkable跨端副本','：reMarkable跨端副本');p.write_text(s)
p=L/'README.md';s=p.read_text().replace('及Duolingo匿名法语课程。','、Duolingo匿名法语课程及Tolino公开图书馆选择器。').replace('及[课程审计](artifacts/duolingo-controls-deepening/browser-operation-audit.md)。','、[课程审计](artifacts/duolingo-controls-deepening/browser-operation-audit.md)及[选馆审计](artifacts/tolino-controls-deepening/browser-operation-audit.md)。');s+='\n本批入口：[第十三期简报](../../briefs/013-household-recording-and-audio-roles.md)。Tolino、CASIO和FiiO均保留型号/版本、目录、旧任务修订与数据边界；只有公开选馆器新增有限D4。\n';p.write_text(s)
prepend_table(L/'inventories/README.md','| [M058-T302手册与双语/家庭FAQ](../batches/tolino-controls-deepening-inventory.csv) | 38手册目录、57英文FAQ标题、60德文标题、30家庭标题，共185行 | 德文仅部分正文深读；父项/重复语言不是功能数；具体任务与缺口分开 |\n| [M024-E-W手册及型号内容表](../batches/casio-controls-deepening-inventory.csv) | 113书签、E-W100的28及E-W220的68内容项，共209行 | 包含父章节/非功能项/重复内容；同手册不代表两机内容相同 |\n| [M034-JM21手册及FAQ](../batches/fiio-controls-deepening-inventory.csv) | 45印刷目录、29正文小节、23型号FAQ，共97行 | 旧手册与Music3.3.4分开；扫描、WiFi传歌和完整队列仍缺 |\n')
p=L/'comparison.md';s=p.read_text()
for marker,row in [
('## 学习与反馈','| 练习录音结束后还能找回吗？ | [CASIO F-M024-14/15](cases/M-024-casio.md) 临时发音对比结束即删，演讲另有五槽保存/覆盖确认。 | 记录是否存在、保存作品、声音对比与能力评分分开；AI回访需明确数据生命周期。 | G-001/002、R-014/019/021 |\n'),
('## 读听与中断恢复','| 接收、遥控和本地播放能否用同一个“播放成功”？ | [FiiO F-M034-04/07/11/12/15](cases/M-034-fiio.md) 音频输入、输出、遥控与模式退出条件不同，原曲恢复未知。 | 按来源、控制端、出口、模式和断点分别验证；蓝牙接收不是内容导入。 | R-018、G-002 |\n')]:
 pos=s.index(marker);end=s.index('| --- | --- | --- | --- |\n',pos)+len('| --- | --- | --- | --- |\n');s=s[:end]+row+s[end:]
s+='''
## 本批补充：家庭权限与服务状态

| 设计问题 | 事实依据 | 本项目仍需决定/验证 |
| --- | --- | --- |
| 共享内容是否共享进度？ | [Tolino F-M058-14](cases/M-058-tolino.md) 家庭各自账户保存进度/笔记，按书授权；退出撤销分享，离线失效时点未知。 | 家庭内容、个人记录、授权和本地副本分开；不能据竞品规则决定本项目账号。 |
| 最近使用是否代表登录成功？ | [Tolino F-M058-19](cases/M-058-tolino.md) 本次匿名选馆后未登录，刷新已有最近项；外站仍需选馆。 | AI查询当前身份/权限，不能从推荐/最近列表推断操作已经成功。 |
| 原件提取缺图是否证明没有控件？ | Tolino p39实际有书签图标；CASIO p171删除范围文字矛盾在原图仍存在。 | 文本提取缺失与原文冲突分别处理；先核图，再保留确切未知。 |
''';p.write_text(s)
p=L/'backlog.md';s=p.read_text()+'''
## 2026-10-27：家庭、录音与音频角色续查

- Tolino T302：已补双语/家庭目录、收藏/跨书店/PIN/辅助读屏及Onleihe版本；公开选馆器只有桌面有限操作。仍缺批注编辑/导出、完整书签回访、TTS离线/锚点、离线共享撤权、同步冲突、真实借阅与更新链。
- CASIO E-W100/E-W220：目录与内容按型号分开，手写、便笺、随记、文学、临时/保存录音、复位已补；仍缺其他内置训练循环、满额反馈、误删恢复、学习簿复位保留与外部迁移。
- FiiO JM21：补M3U内部导入、USB-DAC/蓝牙/AirPlay接收、LRC、升级；仍缺当前Music3.3.4扫描过滤、传歌、完整队列和模式切换续播。旧图不代替当前UI，USB方向措辞和DLNA版本冲突保留。

本批目录491行含重复和父项，不构成全功能分母。新有限操作不使设备案例变成已实测完成；核心学习/新闻/全局AI任务继续按既有优先级补证。
''';p.write_text(s)
p=L/'analysis-status.md';s=p.read_text();marker='本轮之前的工作属于';pos=s.index(marker);s=s[:pos]+'| 家庭书库、录音生命周期与音频角色深化（10月27日） | Tolino7E/10新F/185目录、10网页状态6图；CASIO8E/7新F/209目录；FiiO去重复查后12E/6新F/97目录；补21旧F的83字段 | 新增1条有限D4仅桌面选馆；8张手册摘页；设备、登录恢复和学习效果未实测，全部部分覆盖 |\n\n'+s[pos:];# The row must stay in the table, before its following blank.
s=s.replace('\n\n| 家庭书库','\n| 家庭书库');p.write_text(s)
p=R/'docs/ideas.md';p.write_text(p.read_text()+'''\n2026-10-27继续I-021/I-022：Tolino补家庭进度/书库关联、PIN/重置与借阅服务条件，并实际观察桌面公开选馆器；CASIO区分临时跟读和保存演讲，FiiO区分音频接收/控制/本地文件。旧来源复抓沿用编号，原任务补分支；详见[第十三期简报](/home/workspace/reader/docs/briefs/013-household-recording-and-audio-roles.md)。全部65组仍部分覆盖，未认可账户方案、语音架构或启动全系统设计。\n''')
print('updated brief and eight research entry documents')
