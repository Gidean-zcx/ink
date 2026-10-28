from pathlib import Path
import json,csv,hashlib
root=Path('docs/research/interaction-library'); ev=[]; flows=[]
def E(prefix,n,p,title,url,locator,note,scope,mode='opened_text',typ='official_help',artifact=None,visual=False,updated=None,supersedes=None):
 a=root/artifact if artifact else None
 ev.append(dict(evidence_id=f'E-{prefix}-{n:02}',product_id=p,title=title,url=url,checked_on='2026-10-26',source_type=typ,access_mode=mode,version_scope=scope,app_build=None,firmware_version=None,source_updated_on=updated,locator=locator,note=note,hands_on=False,visual_observed=visual,archived_artifact=artifact,sha256=hashlib.sha256(a.read_bytes()).hexdigest() if a else None,capture_state='archived_public_artifact' if a else 'bibliography_and_short_note_only',supersedes=supersedes))
def F(p,n,task,depth,e,branches,gaps,file,vis='not_observed'):
 flows.append(dict(flow_id=f'F-{p.replace("-","")}-{n:02}',product_id=p,task=task,depth=depth,evidence_ids=e,known_branches=branches,gaps=gaps,visual_status=vis,hands_on='false',case_path='cases/'+file,checked_on='2026-10-26'))
def case(file,body,p):
 body=body.strip()+'\n\n## 证据\n\n'
 for x in ev:
  if x['product_id']==p:body+=f'- {x["evidence_id"]}：[{x["title"]}]({x["url"]})；{x["locator"]}。{x["note"]}\n'
 (root/'cases'/file).write_text(body)
q='https://help.quizlet.com/hc/';scope='Quizlet Web/iOS/Android; English help; exact builds unknown'
for n,title,path,loc,note in [
(1,'Creating study sets with Smart Assist','en-us/articles/39606772122509-Creating-study-sets-with-Smart-Assist','To use; Tips','正文；预览草稿可编辑/删/增，未验证生成结果。'),
(2,'Studying with Study Guides','en-us/articles/18312306436365-Studying-with-Study-Guides','Eligibility; platform creation; visibility','正文；地区年龄及语言限制，原始上传与生成卡组分开。'),
(3,'Creating sets by importing content','en-au/articles/360029977151-Creating-sets-by-importing-content','Import steps; delimiter rules','官方检索正文；网站文本导入流程。'),
(4,'Editing sets','en-au/articles/360030212131-Editing-sets','Web/mobile edit; delete; settings','官方检索正文；校订词义、语言及顺序。'),
(5,'Editing draft sets','en-us/articles/360029768912-Editing-draft-sets','Drafts; offline warnings; logout','正文；草稿不跨网站/App同步，丢弃不可恢复。'),
(6,'Studying with Learn','en-us/articles/360030986971-Studying-with-Learn','Start Learn; Options; Write/Spell','正文；目标、题型及重输错题。'),
(7,'Using grading options (US)','en-us/articles/360048313652-Using-grading-options-US','Relaxed/Moderate/Strict; Learn/Test settings','正文；美国帮助范围，评分宽松程度及语言条件。'),
(8,'Studying with Answer Streaks','en-us/articles/40011154960653-Studying-with-Answer-Streaks','How streaks work; I was correct','正文；首答连对及误判修正。'),
(9,'Studying with Spaced Repetition','en-us/articles/48324742264077-Studying-with-Spaced-Repetition','Web start; four ratings; options','正文；当前仅网站，四档自评与日新增量。'),
(10,'Studying offline with Quizlet mobile apps','en-ca/articles/360030565412-Studying-offline-with-Quizlet-mobile-apps','iOS/Android download; sync; grey titles','正文；最近八组自动缓存及恢复联网刷新。'),
(11,'Exporting your sets','en-us/articles/360034345672-Exporting-your-sets','Export procedure; ownership restrictions','官方检索正文；仅网站导出自建集，不导出图片。'),
(12,'Copying and editing a set','en-us/articles/360029781232-Copying-and-editing-a-set','Make a copy; premium restriction','官方检索正文；复制后成为另一个学习集。'),
(13,'Studying with Flashcards','en-ca/articles/360030988091-Studying-with-Flashcards','Flip; sorting; mobile X and progress','正文；手机会话进度不跨设备，与离线总页口径需区分。')]:
 E('QZLT',n,'M-021',title,q+path,loc,note,scope,'official_index_text' if n in (3,4,11,12) else 'opened_text')
c='https://support.casio.com/global/cn/dic/manual/E-W_series_full_cn/'
E('CASIO',1,'M-024','E-W100/E-W220 用户说明书（详细）','https://www.casio.com/content/dam/casio/global/support/manuals/exword/pdf/003-zh-cn/e/E-W_series_full_CN.pdf','2023年7月版 MA2307-A；PDF/印刷页37–38、91–92、132–135','下载PDF后用PyMuPDF读取；部分HTML 503的补充。','Chinese E-W100/E-W220; July 2023 manual','downloaded_pdf_text','official_manual',updated='2023-07')
for n,title,slug,loc,note in [
(2,'各部分名称','BONDSYuwseurno.html','键盘；触屏快键','正文；键盘图标另据手册静态图核对中文键名。'),
(3,'在HOME画面上查询单词','BONDSYsshtiyxa.html','步骤1–3；候选预览','正文；逐字更新、蓝色候选与释义预览。'),
(4,'画面的基本操作','BONDSYqvrqkoga.html','窗口；功能目录；点触目录','正文；解释窗与返回；已读功能目录，不等于全内容操作完成。'),
(5,'制作画有重点标记的单词本','BONDSYszfgvdrv.html','保存；显示条件；删除','正文；色标隐藏、CHECK可解除，单条/批量删除分支。'),
(6,'制作背诵卡','BONDSYxmqssath.html','制作；阅览并记住；听卡','正文并以PDF164–173页核对；两面手写、原文返回。'),
(7,'高考英语','BONDSYaxrdddxj.html','高中课标/高考词；中高考词汇同步练','正文；录音、拼写纠错与错题类别；并非所有词典共有。'),
(8,'通过查询记录查询曾经查过的单词','BONDSYmleaszga.html','查询记录画面；删除记录','正文；上限、当前/全部词典与删除范围。'),
(9,'可通过学习簿进行的操作','BONDSYjsguknov.html','四种功能；存储位置；入口','正文；学习簿仅保存在本机，非microSD。')]:
 E('CASIO',n,'M-024',title,c+slug,loc,note,'Chinese E-W100/E-W220; 2023 manual')
E('CASIO',10,'M-024','关于卡西欧电子辞典下载学习资源停止服务的通知','https://www.casio.com.cn/news/2026/05/20260507','对应机型；关闭说明；本地内容','正文；2026-12-31 17:00关闭下载，已存内容与内置辞书不受影响。','China E-W100/E-W220 and listed E-XA models',typ='official_service_notice',updated='2026-05-07')
E('CASIO',11,'M-024','拓展学习资源（下载版）的使用流程','https://support.casio.com/global/cn/dic/wlan/','更新时间；STEP1–3；停服说明','正文；先连接再下载再使用；未播放嵌入演示。','E-W/E-XA; updated 2026-05-07',updated='2026-05-07')
for n,page in [(12,42),(13,43)]:
 E('CASIO',n,'M-024',f'官方手册键盘静态图第{page}页','https://www.casio.com/content/dam/casio/global/support/manuals/exword/pdf/003-zh-cn/e/E-W_series_full_CN.pdf',f'PDF/印刷第{page}页；本轮2026-10-26静态观察','已用view_image查看渲染页；不是连续流程、不是实机。','Chinese E-W100/E-W220; 2023 manual','pdf_page_render_observed','official_manual',f'artifacts/dictionary/casio-ew-keyboard-p{page}.png',True)
yd='https://shared.ydstatic.com/market/souti/dict_pen_sync/dict/1.4.5/dist/'
E('YDP',1,'M-025','网易有道词典笔2.0历史产品页','https://c.youdao.com/dict/hardware/dictPen.html','功能文案；HTML中使用说明链接；app.2389c148.js','产品正文与公开JS；只确认功能声明，不采用速度和准确率宣传为实测。','Mainland Chinese historical 2.0; firmware unknown','public_html_and_static_js_text','official_product_page')
E('YDP',2,'M-025','词典笔小程序：功能说明',yd+'js/chunk-6b47e0f2.2e744c90.js','function_explain页面；登录与实时显示两个问题','产品页直链1.4.5前端帮助正文；portal包版本不是固件/App版本。','Historical 2.0 official linked companion portal 1.4.5','public_static_js_text','official_help','artifacts/dictionary/youdao-2-guide-1.4.5-help.js')
E('YDP',3,'M-025','词典笔小程序：学习面板公开模板',yd+'js/chunk-3734dbd6.bc9f5bab.js','扫词/词本/学习详情/连续使用文案','静态模板核查，未运行登录面板；不能据此验证真实统计或同步成功。','Historical 2.0 linked companion portal 1.4.5','public_static_js_text','official_frontend_template')
E('YDP',4,'M-025','词典笔小程序：设置公开模板',yd+'js/chunk-f5a293f4.1ba83d13.js','背词提醒；每周词单；解绑结果文案','静态模板声明功能；提醒实际送达和解绑数据范围未知。','Historical 2.0 linked companion portal 1.4.5','public_static_js_text','official_frontend_template')
E('YDP',5,'M-025','词典笔小程序入口',yd+'index_dict.html#/function_explain','产品页使用说明跳转目标；脚本清单','匿名获取HTML及公开资源；没有登录或触发绑定。','Historical 2.0 linked companion portal 1.4.5','public_html_text','official_help')
eu='https://docs.eudic.net/1/'
for n,title,path,loc,note in [
(1,'单词查询','shi-yong-zhi-yin/dan-ci-cha-xun','纠错；过滤；历史；相机；URL回调','正文；iOS回调最低6.3.0，其余未固定构建。'),
(2,'文件阅读','jin-jie-gong-neng/wen-jian-yue-du','平台导入；查词；标注；同步','旧帮助正文；不同步描述已被新版日志修正，不用作现行能力否定。'),
(3,'笔记和标注','shi-yong-zhi-yin/dan-ci-cha-xun/bi-ji-he-biao-zhu','添加/编辑；选区；集中回顾','正文；词条笔记与书籍批注分开。'),
(4,'例句卡片','jin-jie-gong-neng/ka-pian-fu-xi','听力原声例句→制作卡片；播放','正文；这是词典原声例句，不能等同导入书籍原句自动保留。'),
(5,'我的生词本','shi-yong-zhi-yin/wo-de-sheng-ci-ben','在线同步；分组；排序；播放','正文；数据同步入口与卡片播放条件。'),
(6,'单词学习','shi-yong-zhi-yin/untitled-3','开始学习；新词/复习测试；进度条','正文；自评、客观练习与已掌握。'),
(7,'回顾练习','shi-yong-zhi-yin/untitled-3/hui-gu-lian-xi','当日回顾；列表练习；每10词结果','正文；练习答题不改变熟悉度和正式学习进度。'),
(8,'单词学习常见问题','shi-yong-zhi-yin/untitled-3/untitled-2','误掌握重学；更换词本；提醒；Android备份','正文；历史算法规则不能泛化到日志新增FSRS配置。'),
(9,'算法及设置','shi-yong-zhi-yin/untitled-3/untitled','动态难度；高级设置；学习错误词','正文；动态间隔可关闭，自评算法为该帮助描述。'),
(10,'添加扩充词库','shi-yong-zhi-yin/tian-jia-kuo-chong-ci-ku','第三方格式；WiFi传输；离线发音库','正文；词库资源文件配套，不等于导入学习列表。'),
(11,'发音和跟读','shi-yong-zhi-yin/dan-ci-cha-xun/fa-yin-he-gen-du','英美音；发音库顺序；跟读评分','正文；逐音节反馈是产品描述，无准确率验证。'),
(13,'官方文档目录','llms.txt','基础及进阶功能目录','官方目录；未逐项完成所有功能研究。')]:
 E('EUD',n,'M-026',title,eu+path,loc,note,'Eudic mobile help; iOS/Android where stated; exact build unknown',typ='official_documentation_index' if n==13 else 'official_help')
E('EUD',12,'M-026','欧路词典官方更新日志','https://de.eudic.net/v4/en/app/history?appkey=eusoft_eudic_en','当前页：撤回；CSV/Anki；FSRS；阅读同步；AI功能','正文；页面模板反复显示2017而内容含2026，日期与对应平台/构建未可靠固定。','Eudic dictionary correct appkey; multi-platform updates mixed; date uncertain',typ='official_release_notes',supersedes='E-EUD-02: no-cross-device-reading-annotation claim only')
case('M-021-quizlet.md',r'''
# M-021 Quizlet：资料生成、校订、作答与复习

查阅 2026-10-26；范围为官方英文帮助覆盖的 Web、iOS、Android。未安装、登录或生成学习集，未连续观察界面；操作链为 D2 文档重建。构建与帮助发布日期多数未知；文中明确网站专属与移动端差异。免费、Plus/Unlimited、教师账户资格随功能区分，不采用旧宣传页的固定模式总数。

## 功能目录与范围

| 功能族 | 已核范围 | 未完成部分 |
| --- | --- | --- |
| 内容建立 | Smart Assist 草稿；Study Guides；结构化文本导入；编辑/复制/导出 | AI来源引用、幻觉校验、支持文件上限、扫描失败 |
| 学习与测试 | Flashcards、Learn、书写重答、评分宽松度、Answer Streaks、网站间隔复习 | Test完整会话、Practice Tests、Match/Blocks、Live/小组未逐项观察 |
| 内容与进度管理 | 草稿、发布、可见性、手机离线下载和同步限制 | 双端并发编辑、共享集变更对进度影响、误删恢复 |

## 操作流程

| F 编号 | 入口 / 前置 | 操作与反馈 | 状态 / 结束 | 缺口及证据 |
| --- | --- | --- | --- | --- |
| F-M021-01 AI生成并校订 | 登录→Create→Create a new flashcard set | 输入/粘贴资料、上传文件或给主题；Smart Assist产出预览草稿，可编辑、删卡、添卡；同一集可切手动创建 | 校订后发布成为学习集；生成预览与发布分开 | 生成失败、重生成覆盖、自动事实核查未证实。E-QZLT-01 |
| F-M021-02 Study Guide | 网站帮助路径 Generate→Study guide→上传/粘贴→Start transforming | 生成大纲、卡组等；More可改题名、可见性、查看原上传或删除 | Your library→Study guides 找回；生成卡组另设其可见性 | 免费次数有限；限定英/法/德及官方所列地区年龄，最低门槛14岁；中国大陆未在所列地区，实际资格未验证。E-QZLT-02 |
| F-M021-03 结构化导入 | 网站 Create→Flashcard set→Import | 粘贴术语/释义，按分隔符解析；Import后选两侧语言→Create | 文本成为卡对；语言选择影响后续语音/评分 | 词义可用逗号、Tab或横线分隔，行可分号或换行；异常行与重复合并未知。E-QZLT-03 |
| F-M021-04 校订、复制与导出 | 自有集 More→Edit；他人集 More→Make a copy | 编辑文字/语言、拖动顺序、交换两列；复制编辑后Create。自建集 More→Export→选排列→Copy text | 网站保存Done；导出是复制文本；他人集复制品有导出限制 | 高级内容复制可能需套餐；图片不导出；删除行恢复未知。E-QZLT-04、11、12 |
| F-M021-05 草稿中断与恢复 | 创建/编辑时自动保存；Library→Flashcard sets→In Progress | 重开草稿继续、Create发布或丢弃；未保存词旁有警告，断网时保留网页等待重连 | 草稿仅自己可见，不在网站与App间同步；移动端发布前退出账号会丢弃草稿 | 丢弃不可恢复；自动保存计数可见，重启进程后的边界未知。E-QZLT-05 |
| F-M021-06 翻卡自查 | 集→Flashcards | 点击翻面，网站箭头/键盘前后；手机滑动；Options可排序至Still learning/Know、设卡面及播放 | 看完全部卡完成一轮；手机X退出保存本设备会话位置 | 文档明确该进度不跨设备；语音缺失可能是语言不支持。E-QZLT-13 |
| F-M021-07 Learn与错题重答 | 登录→集→Learn→选学习目标 | Options选择题型、作答方向、星标词、声音；网站Write/Spell可进入书写，错误书面答案可设置重输正确答案 | 学习中产生回答记录；退出与跨日重排细节未知 | 免费仅有限学习轮次；不把会员资格混为永久免费。E-QZLT-06 |
| F-M021-08 评分与误判纠正 | Learn→Options→Grading options | 选择Relaxed/Moderate/Strict并保存；宽松允许语义改写，严格要求精确文本（大小写/标点例外） | 连对机制只计首答，五题起连胜；误判时I was correct可保持连胜，切题型重置连胜 | 评分选项受语言及至少3卡条件限制；修正是否重写长期调度未知。E-QZLT-07、08 |
| F-M021-09 间隔复习 | 网站集→Flashcards→Spaced repetition | 翻面后选Repeat/Hard/Okay/Easy，安排不同再现时间；直至没有到期卡 | Options可切学习方法、日新增量；再次打开保留所选方法 | 评级例示间隔不是效果保证；移动端该方法未支持于所查帮助，误评撤销未知。E-QZLT-09 |
| F-M021-10 离线准备与恢复 | iOS Profile→Your settings；Android Account→离线保存 | 自动缓存最近8集，也可单集下载；勾选表示保存。Library的Downloaded/Downloads进入；勾选→移除下载 | 离线Flashcards/Match及创建可用；重连刷新同步离线记录，编辑内容自动发布 | 灰标题未离线可用；下载可能需数分钟。会话位置同步参见F-06，不能据总页承诺全状态恢复。E-QZLT-10、13 |

完整主链以自有资料为例：F-01生成草稿→F-04人工核对词义/语言→发布→F-07作答→F-08处理评分/误判→F-09到期复习。各阶段是不同对象/模式；没有证据表明 AI 生成后无需校订，也未证实 Learn 错题自动无缝迁移到新的 Spaced Repetition 调度。

## 分支与限制

- “草稿自动保存”“发布集跨设备”“离线学习同步”“Flashcards会话位置”四种状态不可合并。离线帮助概述同步全部离线进度，Flashcards专页仍写手机会话进度不跨端；具体同步字段尚需实测。
- 发布集可见性与Study Guide可见性分别管理；不能根据指南私有推断其生成卡组也私有。共享内容编辑授权及公开默认值应按实际创建页复核。
- 未覆盖：空资料、上传超限、生成超时、题目无正确项、取消订阅后缓存、撤销回答后的历史审计、同步冲突、账户切换后的下载清理。未知不等于没有功能。

## 数据、激励与本项目关系

事实可区分原始上传、生成指南、卡组、卡对、草稿/发布状态、共享权限、星标、自评/答案、连胜和下载状态。连胜强调首次正确，允许“我答对了”修正评分；它不是知识掌握的独立测量。生成能力的年龄/地区条件也使其不能直接作为本项目5–6岁儿童自主账户路径。

关联 G-001/G-002、R-016/019/021、H-004/005：本项目可借鉴“资料→可审阅草稿→发布学习包”，把来源定位、校订理由和版本归属于每张卡；AI应分清修正题目、纠正评分、修改复习计划。个人新闻转学习素材可沿该链，但公开分享须有独立状态。以上均是推断，未选定Quizlet接口或算法。

最小后续观察：固定一个网站和一个手机构建，用三张自建卡分别验证断网草稿、误判更正、会话退出、离线同步、跨设备与间隔复习之间的数据边界；本轮未执行。
''','M-021')
for n,task,e,br,gap in [
(1,'AI资料生成与预览校订','01','手动/AI切换','生成失败及来源校验'),(2,'Study Guide生成与可见性','02','地区年龄、免费额度','生成卡组权限联动'),(3,'结构化文本导入','03','分隔符/语言','空行、重复和失败'),(4,'学习集编辑复制导出','04;11;12','自有/复制集、图片限制','恢复及高级内容权限'),(5,'草稿中断恢复','05','断网警告、退出账号丢弃','进程终止保存'),(6,'翻卡与会话返回','13','手机X保存、本地进度','跨设备断点'),(7,'Learn目标题型及书写纠错','06','免费限制、重输错误','结束及跨日调度'),(8,'评分选项与连胜误判','07;08','语言条件、I was correct、切题型','长期记忆状态修正'),(9,'网站间隔复习','09','四档自评、日新增量','误评撤销'),(10,'移动离线与同步','10;13','灰标题、下载移除、重连','精确同步字段')]:F('M-021',n,task,'D2',';'.join('E-QZLT-'+x for x in e.split(';')),br,gap,'M-021-quizlet.md')
case('M-024-casio.md',r'''
# M-024 CASIO EX-word：固定 E-W100 / E-W220 的查词与学习

查阅 2026-10-26。范围锁定中国版 **E-W100/E-W220**、2023年7月 MA2307-A 中文手册；未把日版XD、旧E-B或未来型号混入。固件未知，词典内容/语言组合依型号及具体词典不同。已读手册核心流程和2026停服公告；D2文档重建，键盘静态图已观察，但没有实机或连续演示，不能升D3。

**时效变化**：官方公告将于2026-12-31 17:00关闭该系列无线下载学习资源功能；本轮查阅尚在关闭前。内置词典与已下载本机/卡内资源不受影响。E-CASIO-10、11。

## 功能目录与实体输入

| 功能族 | 已核范围 | 边界 |
| --- | --- | --- |
| 查词 | HOME多词典、专用词典键、模糊查询、词条/例句跳查、快查窗、历史、发音 | 词典不同可用功能不同；全文/例句返回路径仍有缺口 |
| 学习簿 | 单词本色标、随记、便笺、手写背诵卡 | 深入核对单词本与背诵卡；随记/便笺只列目录 |
| 专门词汇训练 | 高中课标3000/高考3500的发音/拼写；同步练六类训练 | 内置课本与自制生词本不是同一套调度 |
| 内容管理/系统 | 我的书库、无线下载、本机/卡、恢复上次画面 | 任意PDF/ePub导入、文本加载器适配、学习数据导出未验证 |
| 辅助 | 手写、软键盘、字号、录音/演讲、日历计算器、投屏 | 功能目录已定位，未穷尽所有辞书/辅助操作 |

实体键在[手册42页静态图](/home/workspace/reader/docs/research/interaction-library/artifacts/dictionary/casio-ew-keyboard-p42.png)和[43页](/home/workspace/reader/docs/research/interaction-library/artifacts/dictionary/casio-ew-keyboard-p43.png)核对为：HOME、SHIFT、前一单词/后一单词（兼翻页）、音量±、发音、查询记录、退出、输入、方向、删除和字母键。**“输入”承担确认**；“退出”返回上一画面；屏右还有HOME/发音/退出/功能触屏快键。功能没有对应实体快键，播放时多数触屏快键无效但退出例外。E-CASIO-02、12、13。

## 操作流程

| F 编号 | 入口 / 操作 | 系统反馈及数据 | 返回 / 分支 | 证据与缺口 |
| --- | --- | --- | --- | --- |
| F-M024-01 查词主链 | HOME→英语→字母键逐字输入；候选已出现可提前选词；方向键选项+输入键确认，或点触词条 | 词典分组候选随输入更新，蓝底选中，旁边显示释义预览；选中后完整解释 | 退出返回；窗内输入键放大成解释画面，退出关窗；发音键用于听音 | E-CASIO-02～04、12、13。主链用contact示例；不同词典语音可用性另查 |
| F-M024-02 输入纠错与查阅切换 | 朗文词典输入模糊拼写→模糊查询→候选；找不到可用?/* | 返回近似拼写/原形候选，不保证推测原形正确 | 删除键修改输入；方向键滚动，SHIFT后翻页键切前/后词；解释页轻描可进入跳查菜单 | E-CASIO-01、02、04。无命中界面、全部跳查返回栈未观察 |
| F-M024-03 查询历史 | 词典初始页→查询记录键→点词 | 再按查询记录切全部/当前词典；总体最多1000条，满后淘汰最早记录 | 单条选择后删除；批量范围另确认；不是删除词典内容 | E-CASIO-08。部分词典/方式不记历史，撤销删除未知 |
| F-M024-04 原文标记变成生词本 | 词条解释页轻描文本→记号笔→颜色→目标单词本 | 保存该词条及色标并回解释页；工具→学习簿→单词本再找回 | 显示条件可按色/CHECK筛选、隐藏标记文字；点击隐藏区域显/隐切换，CHECK再点解除 | E-CASIO-05、09。最多12本×300条、每词10处；不是自动提取任意外部书原句 |
| F-M024-05 从词条制作背诵卡并练习 | 解释页上半部留参考→功能→背诵卡→手写正/反面→保存；学习簿选卡本 | 可存英语发音；设置“看/听”、正反面和顺序；点击遮面揭示答案，Check标记 | 上一个/下一个换卡；参照辞书原文回解释页，退出回卡/结束；仅含语音卡可进入“听” | E-CASIO-06、09。6本×100卡；不是算法判分；未查同步/导出 |
| F-M024-06 内置课本听说拼写 | 高中课标/高考词初始页→随机测试或单词表→类别/词→内置语音；输入键推进词义、发音评测、录音 | 约2秒录音后评测；拼写输入→输入键显示对错；输入键继续例句 | 无法识别→再来一次；拼写错按退出重新输入；“下一个”会记已完成测试 | E-CASIO-07。完成不必等同掌握；该路径不是单词本复习 |
| F-M024-07 同步练的错题复习 | 进入中高考词汇同步练→单词默写/听音释义等→选择答错的单词→范围 | 默写输入确定或四选回答，显示答案与对错；详解查释义，下一个继续 | 未学/已完成/答错分类可再选；垃圾桶删除学习进度 | E-CASIO-07。六类含入门、默写、听音、搭配、词性、辨义；误判修正/回滚未知 |
| F-M024-08 下载后本地学习 | 工具→我的书库→无线下载；初次选接入点并输入密钥→教育图书馆→下载 | 本机/存储卡的拓展资源可在我的书库查看 | 找不到接入点可重搜；再次连接可复用网络；服务关闭后已下载仍可用 | E-CASIO-01、10、11。下载中断/重复/失败回滚未核；未将TextLoader跨系列宣传视为E-W验证 |
| F-M024-09 中断恢复 | 打开翻盖自动开机，显示上次关机画面；合盖通常关机 | 另有电源键、无操作自动关机；词典键可开机直达其词典 | 语音播放器播放时合盖继续播放；学习簿存本机，不在microSD | E-CASIO-01、09。只证明画面恢复，不证明每种未保存编辑都持久化 |

## 分支、删除与未知

单词本的CHECK是可反复切换的标记。划过既有色标→记号笔→确认可删标记；删除单条词条直接执行，批量删除显示确认。它们与删除词汇测试进度是不同动作，手册没有证明统一撤销。E-CASIO-05、07。

内置词典/已存学习资源的本地可用性有明确公告支持；联网下载与本地学习分开。未找到账号云同步完整路径，不能据此断言绝无同步或自行宣称自动备份。学习簿保存在主机，插卡不能自动保护这部分数据。E-CASIO-09、10。

尚未覆盖满额后的具体提示、空卡本、手写识别纠错全链、断电写入、复位保留范围、文本导入导出、真实响应时延。用本型号手册而不是宣传页确定核心操作，仍不等同所有功能体验完成。

## 数据、激励与本项目关系

可区分查询历史、词典条目、色标/CHECK、手写卡、参考原文、录音评测、课本测试进度、下载资源和网络配置。遮挡回忆与错题分类是两种学习机制；“已完成测试”是流程状态，不能直接用作能力成就。

关联 G-001/G-002、R-016/019/021、H-004/005：本项目可让AI保留词条的原文定位，支持从记忆卡返回上下文；小屏按钮应清楚区分返回、重答、跳下一词。儿童若不能正确理解“完成/CHECK/掌握”，容易产生假进度；成人可用独立的错题集合减少重复翻找。联网资源退场但本地可继续的设计值得借鉴。以上为推断，不构成硬件键位或算法定案。

后续最小观察：E-W100同一固件，用一词从查找→遮挡→卡片→误标解除→合盖恢复，另验证同步练错题和删除进度；记录实体键与触屏是否一致。本轮未实测。
''','M-024')
for n,task,e,br,gap in [(1,'实体键查词与返回','02;03;04;12;13','候选提前选、窗口返回','词典差异'),(2,'拼写纠错与词条切换','01;02;04','模糊、通配符、SHIFT','无命中和跳查栈'),(3,'历史查询与删除','08','1000上限、当前/全部','撤销'),(4,'原文标记与生词本遮挡','05;09','色标/CHECK、容量','任意外部原句'),(5,'手写背诵卡创建练习','06;09','看/听、返回原文','导出/调度'),(6,'内置课本听说拼写','07','无法识别重录、退出重答','完成与掌握'),(7,'内置同步练错题','07','错题筛选、删除进度','误判修正'),(8,'联网下载与本地资源','01;10;11','网络重搜、2026停服','下载失败、导入'),(9,'合盖中断恢复','01;09','音频合盖继续','未保存内容')]:F('M-024',n,task,'D2',';'.join('E-CASIO-'+x for x in e.split(';')),br,gap,'M-024-casio.md','static_manual_pages_only' if n==1 else 'not_observed')
case('M-025-youdao-pen.md',r'''
# M-025 有道词典笔2.0：历史扫描入口与手机协同

查阅2026-10-26。范围只指大陆历史“网易有道词典笔2.0”产品页及其直接链接的词典App内小程序帮助，不混入2加强版、2专业版、海外Pen2、X系列。准确硬件编码、固件、App构建和这些历史服务当前可用性未知。公开页面资源路径含 **dict/1.4.5**，这是网页资源包版本，不能当作固件版本。

没有登录、绑定、扫描或观看演示视频。已匿名读取官方产品HTML与其公开脚本；绑定/实时显示帮助是D2文本流程，扫描主体和统计/提醒为D1功能声明。未找到型号明确、完整可读的官方硬件操作手册；故本案例不能称完整主任务已验证。

## 功能目录

| 功能族 | 公开证据 | 深度边界 |
| --- | --- | --- |
| 扫描查词、多行翻译 | 中英印刷文字、专业词典、真人英/美音 | E-YDP-01 产品页声明；扫描姿态、识别错误改字、返回上级未知 |
| 语音及设备 | 语音助手查词/古诗/天气、触屏、在线更新、蓝牙耳机 | E-YDP-01；实体键短长按及离线矩阵未获官方说明 |
| 手机协同 | 扫码登录、账号匹配、蓝牙查词实时显示、断连重搜 | E-YDP-02、05；有明确步骤，未运行 |
| 学习记录 | 扫词数、已入单词本、学习时长、连续使用、本周词单 | E-YDP-03 公开界面模板；统计定义/更新只按可读文案记录 |
| 提醒/解绑 | 背词提醒、每周词单提醒、解绑结果提示 | E-YDP-04 模板；不证明练习题型或解绑后的数据处置 |

## 操作链与状态

| F 编号 | 入口 / 前置 | 动作与公开反馈 | 返回 / 状态 | 证据与未知 |
| --- | --- | --- | --- | --- |
| F-M025-01 印刷内容扫描 | 手持2.0面对纸上中英印刷内容 | 产品页声明扫描后查词/译多行并可听英美音 | 识别文本、释义和译文在概念上不同；当前材料未给收藏及返回准确按钮 | D1，E-YDP-01；不把“一扫”扩写为已验证完整步骤，不采用宣传速度做时延 |
| F-M025-02 笔端账号登录 | 笔首页左上默认头像；手机词典App内词典笔小程序 | 笔端显示登录码→手机“扫码登录”扫描→成功后笔首页显示头像和用户名 | 账号关联可见；不是本轮实际绑定 | D2，E-YDP-02、05；扫码权限、过期码、换账号、解绑未同步数据未知 |
| F-M025-03 扫词实时显示到手机 | 两端同一账号；笔首页头像→账户→查词实时显示；蓝牙可搜索 | 手机小程序点实时显示→自动搜索并连接→底部显示已连接→笔扫词，手机同步展示释义 | 失败/断开先确认蓝牙与距离，再点“重新搜索”；笔离开实时显示页5分钟后自动断开，再走连接步骤 | D2，E-YDP-02；这只证明屏幕协同，不证明全部学习状态实时云同步 |
| F-M025-04 回顾学习与提醒 | 小程序公开学习面板/设置模板 | 模板有今日/本周扫词、入词本数、学习时长/连续使用、周小结及提醒开关；解绑有成功/失败文案 | 文案以每日使用超过30分钟记连续使用；周词单提醒标示周日22:00 | D1，E-YDP-03、04；真实入口完整链、练习/评分/提醒送达、时区与服务现状未知 |

## 分支与资料边界

目前官方帮助可确认蓝牙连接失败、自动断开及重连。识别失败、错词修改、空历史、重复收藏、离线查词/翻译/语音助手各自能力、重启断点、收藏批量导出、练习错题、答案撤销均未取得足够的2.0官方流程证据。**未知不等于不支持**。

产品页自带“使用说明”菜单指向官方ydstatic网页，再由该页引用帮助脚本；这是可追溯的历史关联。统计/设置代码仅用于读取公开UI文案，没有调用账号接口，也未把按钮存在当作真实成功状态。不同地区经销商FAQ与专业/加强版评测不用于填补普通2.0操作，因为版本与升级时间无法对齐。

## 数据、激励与本项目关系

官方材料能区分笔端登录码/账号身份、连接状态、扫描词、释义显示、入词本计数及时间统计；但扫描原句是否随词保留、入词本动作、练习记录字段尚不清楚。“使用超过30分钟”的连续记录衡量使用时间，不能证明学习收益。

关联 G-001/G-002、R-016/019/021、H-004/005：本项目若让AI把纸面生词转成练习，应独立保存识别原文、用户校正、来源位置、词条匹配及计划状态，不能把手机看见释义当成收藏成功或持久同步。儿童扫描后的短确认和纠错入口值得验证；成人新闻/纸书入口也需要保留原句。以上为本项目推断，不是该设备已实现的能力。

下一项最小核查：取得普通2.0大陆版本说明书/固件记录，先确认实体键，再观察“扫一句→纠错→选词→收藏→复习→返回原句”；若只能取得专业版，另立型号范围，不覆盖本记录。
''','M-025')
for n,task,d,e,br,gap in [(1,'印刷扫描查词与翻译','D1','01','中英、多行','实体键、纠错、收藏练习'),(2,'扫码登录绑定','D2','02;05','成功身份反馈','过期码与切换账号'),(3,'查词实时显示及重连','D2','02','蓝牙失败、5分钟断开','真正同步范围'),(4,'统计词单与提醒','D1','03;04','连续使用定义、解绑文案','实际功能、题型、服务现状')]:F('M-025',n,task,d,';'.join('E-YDP-'+x for x in e.split(';')),br,gap,'M-025-youdao-pen.md')
case('M-026-eudic.md',r'''
# M-026 欧路词典：查询、语境卡片与多种练习状态

查阅2026-10-26；以官方移动帮助为主，iOS/Android有差异；桌面/浏览器能力仅在明确来源处列出。构建未知，部分GitBook文档较旧；当前官方日志使用正确appkey=eusoft_eudic_en，但模板年份反复2017而正文包含2026更新，不能把模板日期或相邻文本当成发布日/版本对应。未安装、登录或实际练习；主要D2，新增能力仅D1日志确认，无连续视觉观察。

## 功能目录

| 功能族 | 已查内容 | 边界 |
| --- | --- | --- |
| 查词与输入 | 拼写纠正、通配符、词库筛选、历史、拍照/语音、跨App回调 | 图片/语音识别失败流程未核；不能以“离线查词”覆盖所有联网功能 |
| 保存语境 | 词条笔记/标注、原声例句卡、生词本、文件阅读查词 | 词典例句与导入书原句不同，任意原句自动关联链尚缺 |
| 学习 | 自评、新词/复习测试、回顾练习、跟读、已掌握重学、提醒 | 新版FSRS/撤回仅日志确认，不能套用旧算法全部规则 |
| 内容流转 | 第三方词库、文档、离线语音库、同步、CSV/Anki导出声明 | 导出生词格式已有，精确入口/冲突/重复导入未知 |
| 新版扩展 | AI问答、翻译引擎、视频学词、家长控制等日志能力 | 只列发现，未冒充全流程；完整功能数量未核定 |

## 操作流程

| F 编号 | 入口 / 前置 | 操作与反馈 | 数据 / 返回 | 证据与缺口 |
| --- | --- | --- | --- | --- |
| F-M026-01 查词与纠错 | 查词输入框；也可点相机/话筒 | 输错后选拼写纠正；右划侧栏选词库；?/*可模糊查词；相机有对焦取词/拍照翻译 | 历史可侧栏清空或单词左划删除；iOS6.3.0+ URL回调可把返回设到来源App | E-EUD-01。普通查询返回、误识别校订和清历史撤销未核 |
| F-M026-02 文档导入并查词 | iOS帐号→WiFi文件传输导入PDF/ePub/docx；Android帐号→文件阅读→导入PDF/ePub | 阅读长按词自动释义；Aa可改成单击查词；长按选区→颜色/批注 | 产生阅读标注；回原文的准确关闭动作未知；并不证明自动保留原句到词本 | E-EUD-02。新版批注同步见下文，不沿用旧页限制 |
| F-M026-03 词条笔记及例句卡 | 释义页左下菜单→我的笔记；或我的笔记标题旁编辑 | 可加/改笔记，选区颜色/批注；听力原声例句右下三点→制作卡片 | 生词笔记中集中看笔记/标注或例句卡；播放可设循环、间隔和原声 | E-EUD-03、04。卡内是选中的词典原声例句，不必等同用户阅读原句 |
| F-M026-04 生词组织、播放、同步 | 生词笔记→顶部切换→编辑分组；选择默认词本 | 按添加时间/字母/等级排序；播放图标轮播卡片；下拉或右上三点→在线同步 | 生词、词条笔记/标注可云端共享；学习页更换可选自建词本作复习课本 | E-EUD-05、08。查词当刻加词按钮、冲突和完成提示未明确；勿补画想象路径 |
| F-M026-05 正式学习和测试 | 学习页选课本→开始学习 | 卡片自评认识/模糊/不认识；复习可拼写、例句拼写、释义选择、听音；未过测试回卡片再学 | 绿/灰/白分别表示通过测试/学习中/未学；上滑或垃圾桶是人工“已掌握” | E-EUD-06、09。高级设置控制顺序、错词额外卡片与动态间隔；FSRS配置不能沿用旧间隔 |
| F-M026-06 补练与错误回顾 | 回顾今日学习，或单词列表→听单词/做练习 | 按进度/熟悉度筛选、选择题型；每10词结果列表可看释义、较多错误词、加生词本 | 此练习结果**不改变正式熟悉度/学习进度**；今日回顾不含今日尚未学的词 | E-EUD-07。重复练习不等同完成计划；新版生词笔记列表“做练习”见日志 |
| F-M026-07 误掌握恢复与提醒 | 单词复习→已掌握→单词左划→重新学习，或批量重学 | 被恢复词作为新词出现；学习设置可设记忆提醒时间 | Android帮助称首次学习备份，之后每7天，清50天前备份；不是实时云同步保证 | E-EUD-08。新版工具栏撤回仅日志确认，是否恢复原调度而非重学未知 |
| F-M026-08 跟读纠错 | 解释页点英/美国旗或音标听音→顶部跟读 | 录音后逐音节评分，可重新跟读；帐号→发音库管理调速度/排序 | 发音结果是独立于词义记忆的反馈 | E-EUD-11。噪声失败、录音保留、离线评分未核 |
| F-M026-09 词库和离线资源 | 帐号→WiFi文件传输；在电脑浏览器输入App给的网址并传文件 | 帐号→词典库管理看到词库；支持mdx/mdd、l2d、bgl；资源文件名称按词库要求对应 | 官方扩充词库与离线发音库各有下载入口；这是内容资源，不是生词学习队列 | E-EUD-10。破损文件、冲突替换和解析进度未核 |
| F-M026-10 新版导出/恢复能力 | 当前官方日志，未固定构建/平台 | 已公告CSV/Anki导出、生词笔记做练习、背词撤回、FSRS及阅读高亮笔记跨端同步 | 为能力目录与旧限制修正，不拼成实际操作链 | D1，E-EUD-12；导出菜单、覆盖规则、撤回范围、同步格式范围仍未知 |

## 已知分支与版本冲突

- 旧文件阅读帮助说批注不能跨设备同步，只能带文件迁移；新版日志明确新增阅读高亮笔记同步。**以“新版已公告支持、平台/格式/最低构建待核”修正旧限制**，仍保留旧帮助用于历史操作定位。不要与“词条笔记在线同步”混为一个对象。E-EUD-02、05、12。
- 旧学习帮助的人工“已掌握”不再排期，重学恢复为新词；自动达成掌握与人工跳过的再次出现规则不同。当前日志新增FSRS和撤回，所以不能把旧默认天数当作所有版本/配置统一算法。E-EUD-08、09、12。
- 文件阅读→查词→加词→自建词本练习有功能连接依据，但原句自动携带、确切加词按钮与回到原书位置未由本轮帮助闭合。已确认的语境链是“词典原声例句→制作卡片→播放回顾”。
- 离线词库/发音资源入口明确；在线百科、AI、语音识别、评分及同步的完整离线矩阵未知。进程中断、同步冲突、删除恢复、账号切换、批量重复词未获完整步骤。

## 数据、激励与本项目关系

资料区分词典资源、查询历史、词条笔记、阅读标注、原声例句卡、生词本分组、学习课本、熟悉度、练习错次、跟读评分与本地备份。正式学习、自由练习和“已掌握”是不同写状态动作；免费补练不污染计划是可借鉴的设计点。提醒与进度色能推动继续，但不能据此断言掌握效果。

关联 G-001/G-002、R-016/019/021、H-004/005：本项目AI需要读到每次作答的模式、来源句位置和是否影响计划，才能解释“为什么还要学”。从新闻/纸书进来的生词应保存用户原句，不能用通用例句替换而不说明；儿童的“跳过”与“我会了”宜区分，Plus输入可服务拼写与笔记。以上是本项目推断，不代表已选欧路算法或接口。

后续最小观察：固定iOS/Android构建及算法，导入一页自有文档，记录查词→加词→原句保存→错答→补练→撤回/重学→另一端同步；验证旧帮助与新日志对应范围。本轮未执行。
''','M-026')
for n,task,d,e,br,gap in [(1,'查询纠错与回调','D2','01','通配符、删历史、iOS回调','常规返回和撤销'),(2,'文档导入查词标注','D2','02;12','平台格式、点击模式','原句入词本闭环'),(3,'笔记与原声例句卡','D2','03;04','编辑、循环间隔','书籍原句关联'),(4,'生词分组播放同步','D2','05;08','分组、排序、自建词本','加词按钮和冲突'),(5,'正式学习与测试','D2','06;09;12','错题回卡片、配置算法','新版算法对应'),(6,'回顾练习与错误列表','D2','07','10词结果、不改正式进度','新列表入口版本'),(7,'重学恢复提醒与备份','D2','08;12','误掌握重学、Android周期备份','撤回调度细节'),(8,'发音与跟读反馈','D2','11','音色排序、重试','录音及离线'),(9,'词库与离线资源导入','D2','10','配套资源文件、离线发音','破损与覆盖'),(10,'新版导出撤回同步能力','D1','12','旧帮助限制修正','准确构建与操作路径')]:F('M-026',n,task,d,';'.join('E-EUD-'+x for x in e.split(';')),br,gap,'M-026-eudic.md')
(root/'batches'/'dictionary-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in ev))
with (root/'batches'/'dictionary-coverage.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(flows[0]));w.writeheader();w.writerows(flows)
print('Wrote',len(ev),'evidence and',len(flows),'flows')
