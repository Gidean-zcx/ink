from pathlib import Path
import csv, hashlib, json, shutil

root = Path('docs/research/interaction-library')
out = root / 'batches'
art = root / 'artifacts/extended-readers'
art.mkdir(parents=True, exist_ok=True)
evidence, coverage = [], []
D = '2026-10-26'

def E(id, p, title, url, loc, note, scope, typ='official_help', mode='opened_text', fw=None, updated=None, visual=False, artifact=None):
    evidence.append(dict(evidence_id=id, product_id=p, title=title, url=url, checked_on=D, source_type=typ, access_mode=mode, version_scope=scope, app_build=None, firmware_version=fw, source_updated_on=updated, locator=loc, note=note, hands_on=False, visual_observed=visual, archived_artifact=artifact, sha256=hashlib.sha256((root/artifact).read_bytes()).hexdigest() if artifact else None, capture_state='selected_static_image' if artifact else 'bibliography_and_short_note_only', supersedes=None))

def F(p, n, task, depth, ids, branches, gaps, name, visual='not_observed'):
    coverage.append(dict(flow_id=f'F-M{p}-{n:02}', product_id=f'M-{p}', task=task, depth=depth, evidence_ids=ids, known_branches=branches, gaps=gaps, visual_status=visual, hands_on='false', case_path='cases/'+name, checked_on=D))

def case(name, body, p):
    sources='\n\n## 证据\n\n'+'\n'.join(f"- {x['evidence_id']}：[{x['title']}]({x['url']})；{x['locator']}。{x['note']}" for x in evidence if x['product_id']==p)+'\n'
    (root/'cases'/name).write_text(body.strip()+sources)

p='M-053'; name='M-053-ireader-hardware.md'; scope='iReader Ocean4 long-battery 2025; SmartOS build unknown; China certified Youzan shop'
shop='https://detail.youzan.com/show/goods?alias=1y43umgr1e67n0i&from_source=gbox_seo'
E('E-IHW-01',p,'Ocean4 长续航版2025认证店商品页',shop,'页首型号；店铺微信认证及官方旗舰店标识；详情图13—15、31—33（1起算）','正文及静态图；认证标识是归属依据，仍属商品陈述，不是实际性能验证。',scope,'certified_brand_store','opened_text_and_static_images',visual=True)
urls=json.loads(Path('work/extended-readers/ireader-images.json').read_text())
for n,i,title,loc,note in [(2,15,'Ocean4 扫描PDF重排示意图','详情第16图；顶栏重排与重排方式弹层；前后示意','实际查看静态原图；没有观察触发、进度、完成的连续过程。'),(3,19,'Ocean4 全局手势区域示意图','详情第20图；七个手势区域','实际查看静态原图；图示区域不证明十二种功能的默认绑定。'),(4,25,'Ocean4 物理翻页键图','详情第26图；右侧两键与可自定功能说明','实际查看静态原图；未确认长短按、改键入口或全局生效范围。')]:
    fname=f'ireader-ocean4-2025-detail-{i+1:02}.jpg'
    shutil.copyfile(Path(f'work/extended-readers/ihw-{i:02}.jpg'),art/fname)
    E(f'E-IHW-{n:02}',p,title,urls[i],loc,note,scope,'certified_brand_store_image','static_image_observed',visual=True,artifact='artifacts/extended-readers/'+fname)
E('E-IHW-05',p,'掌阅官方账号使用技巧视频条目','https://www.bilibili.com/video/BV1eR4y1s7JR/','2021-12-10；账号掌阅iReader官方账号；条目说明','仅正文/视频元信息，未观看连续视频；年代和型号不足以证明2025机型操作。','iReader official video listing 2021; model unspecified','official_video_listing','listing_only',updated='2021-12-10')
for n,t,ids,b,g in [(1,'从书架打开并继续阅读','E-IHW-01;E-IHW-04','物理翻页和重力旋转为商品功能','书架导入、返回、断电位置、字体变更保存'),(2,'扫描PDF OCR与重排','E-IHW-01;E-IHW-02','宣传离线转换；图上有重排选项','触发到完成、取消、误识别修改、原文定位'),(3,'全局手势与实体键改绑','E-IHW-03;E-IHW-04','七区域；键可绑定返回首页/全局刷新','设置入口、默认动作、冲突与恢复默认'),(4,'AI阅读辅助','E-IHW-01','摘要、核心内容、思维导图列为能力','来源追溯、提示修改、重试、收费与离线边界'),(5,'朗读与录音','E-IHW-01','AI朗读与扬声器/麦克风列为能力','开始/停止、读听位置、离线音色、录音保存和导出')]:
    F('053',n,t,'D1',ids,b,g,name,'static_product_images_only')
case(name,r'''
# M-053 掌阅硬件：Ocean4 长续航版2025，操作主链仍待补

查阅及静态图观察日2026-10-26。固定 **Ocean4 长续航版2025**、中国市场、SmartOS构建未知；不混用Ocean4 Turbo、Ocean5、Smart或Tango的按键、手写与App能力。Smart/Tango仍为目录待查对象，本案例不表示整个系列已覆盖。未登录、安装或操作设备，全部流程D1；静态宣传图不能升D3。

来源为有赞页面显示“微信认证”的掌阅iReader官方旗舰店；这是平台内店铺归属标识，不代表独立测试。已检查详情图，正式归档仅保留三张直接支撑控件与布局的图。2021官方视频只读到条目信息，未将其作为2025款教程。E-IHW-01、05。

## 已见功能目录与流程缺口

| F 编号 | 前置与入口证据 | 已见反馈/数据边界 | 待核完整操作 |
| --- | --- | --- | --- |
| F-M053-01 本机阅读 | 商品说明阅读、排版及重力旋转；图中右侧两枚翻页键 | 可见阅读画面；不能确认真实翻页耗时 | 导入→书架→开书→翻页→返回→续读尚未闭合；E-IHW-01、04 |
| F-M053-02 PDF重排 | 静态图有顶栏“重排”及方式弹层 | 宣称扫描PDF可免费离线OCR重排；前后对照为示意 | 无开始/处理中/取消/失败/恢复证据；E-IHW-02 |
| F-M053-03 输入映射 | 图示七个手势区域；实体键说明可自定首页/全刷 | 有配置能力，未见绑定后的即时反馈 | 十二种功能的清单、设置页、长短按及应用内冲突未知；E-IHW-03、04 |
| F-M053-04 AI阅读 | 商品列摘要、核心整理、思维导图 | 功能发现；生成物的可编辑性未核 | 入口、原文引用、修订、失败重试、网络和套餐未知；E-IHW-01 |
| F-M053-05 听读与录音 | 商品列AI朗读及扬声器、麦克风 | 不能据硬件存在推断录音转写或笔记联动 | 播放停止、定时、断网续播、保存及导出未知；E-IHW-01 |

功能目录还见PDF裁边/漂白及图文重排、字体排版、前光、重力旋转、刷新优化。没有取得同型号官方完整帮助，因此查词、生词本、划线导出、云同步、外部App、手写是否可用均不能据未出现推断“不支持”。所有速度、续航、清残影比例仍是商店条件下陈述，不作为已测效果。E-IHW-01。

观察定位：[PDF重排图](/home/workspace/reader/docs/research/interaction-library/artifacts/extended-readers/ireader-ocean4-2025-detail-16.jpg)、[手势图](/home/workspace/reader/docs/research/interaction-library/artifacts/extended-readers/ireader-ocean4-2025-detail-20.jpg)、[两枚实体键图](/home/workspace/reader/docs/research/interaction-library/artifacts/extended-readers/ireader-ocean4-2025-detail-26.jpg)。这些图足以定位可见控件，不足以证明“点该控件必得图中结果”。

## 异常、数据与本项目推断

本轮没有可确认的空书架、错误格式、导入重名、OCR低置信度、误划线撤销、下载中断、离线AI、账号切换、同步冲突或重置恢复方案。PDF原文件、重排显示、OCR文本与AI总结之间是否独立保存也未核定；不要把“离线OCR”扩张为“全部AI离线”。

对G-001/G-002及Plus的推断：应把原页、识别文本、重排视图和生成摘要当作可区分对象，保留退回原页与纠错动作。全局手势和实体键若由成人改绑，儿童需要稳定的返回方式和可理解的当前动作提示；该判断不是对Ocean4误触率的结论。成人读PDF可从“原图和易读视图互查”受益，5–6岁自主读听更需明确暂停和回原处。未发现足以核定奖励/复习机制的操作资料，不能把AI摘要当作学习效果。

最小后续核查：取得该款及明确SmartOS构建的帮助页或连续演示，用一份合法扫描PDF记录导入、重排、误识别、取消、重启续读，再核两枚键和手势的设置与恢复默认。另为Smart/Tango各固定型号后另立分支。
''',p)

p='M-054';name='M-054-moaan.md';scope='MOAAN inkPalm5; Android 8.1; English manufacturer-style leaflet mirrored by ManualsPlus; firmware unknown'
moapdf='https://manuals.plus/m/1013a4ce313dbb06c0c9da76eea48db9f3c0faeabd9d3fb023a90aabf32faa17_optim.pdf'
E('E-MOA-01',p,'inkPalm5 单页说明书保存件',moapdf,'PDF第1页：Basic parameters、Function introduction、Exception handling','PDF正文可读；本地下载403，未看到页面图像；第三方保存，原厂站未能交叉取得。',scope,'manufacturer_manual_mirror','opened_pdf_text')
E('E-MOA-02',p,'inkPalm5 手册镜像目录','https://manuals.plus/moaan/inkpalm5-mini-e-book-reader-manual','型号与PDF链接；镜像发布日期','仅确认保存件入口；2024-07-25是镜像日期，不当成产品/固件发布日期，也非第二份独立操作证据。',scope,'manual_mirror_index','opened_text',updated='2024-07-25')
for n,t,d,ids,b,g in [(1,'开机、待机和关机','D2','E-MOA-01','短按待机；长按关机需弹窗确认','唤醒后书页/未保存笔记恢复'),(2,'侧键与Logo触控','D1','E-MOA-01','侧键默认音量，可改翻页；Logo默认返回','具体改键菜单、长按英文full swipe中文原义'),(3,'连接网络与蓝牙','D2','E-MOA-01','设备设置中的网络设置；WiFi/蓝牙分支','连接成功提示、断开重连、配对失败'),(4,'卡死后重新开机','D2','E-MOA-01','12秒释放后5秒开机；无法开机查电量/售后','数据是否保留、强制重启与恢复出厂边界'),(5,'书籍导入到查词/继续阅读','D1','E-MOA-01;E-MOA-02','型号明确为Android8.1口袋机','完整主链、App兼容、听读、同步及导出')]: F('054',n,t,d,ids,b,g,name)
case(name,r'''
# M-054 墨案：inkPalm5 的基础控制与恢复，阅读软件主链缺口

查阅2026-10-26。固定说明书所称 **inkPalm5、5.2英寸、Android 8.1**；系统构建、在售地区和账户策略未知。不并入inkPalm Plus、后续Mini或大屏W系列，也不采用社区Android升级结果作为原厂能力。资料为第三方站保存的厂商样式单页手册；原厂网站本轮连接失败，未核到同内容原厂下载，可信度低于官方托管手册。镜像2024年日期不是固件日期。E-MOA-01、02。

只有公开PDF文本核读，未看到PDF渲染图；无实机、无连续视频。基础开关机/联网/卡死恢复达到文档D2，阅读主任务仍D1。没有核定全功能目录。

## 功能目录及操作

| F 编号 | 前置/动作 | 反馈及结束 | 分支和缺口 |
| --- | --- | --- | --- |
| F-M054-01 电源状态 | 电源键长按5秒开机；开机短按待机；长按2秒后点弹窗关机 | 操作区分短按待机与确认关机 | 未说明阅读位置与未保存编辑的恢复；D2，E-MOA-01 |
| F-M054-02 输入映射 | 侧键默认调音量，设备设置可改翻页；Logo轻触默认返回 | 可停用或更改触控；长按写作full swipe，中文原义待核 | 未给完整菜单及保存反馈，不升D2；E-MOA-01 |
| F-M054-03 联网/外设 | 设备设置→网络设置→WiFi开启并选网络；蓝牙分支开启并配对新设备 | 文档给连接任务，未描述成功图标 | 密码错误、外设配对失败、取消、自动重连未知；D2，E-MOA-01 |
| F-M054-04 卡死恢复 | 电源长按12秒后释放，再长按5秒开机 | 仍打不开时先检查电量，再联系售后 | 文档没有承诺数据不丢失，也未给恢复出厂动作；D2，E-MOA-01 |
| F-M054-05 阅读任务 | 仅确认设备定位为电子书阅读器 | 没有书架→导入→开书→查词→返回的操作依据 | 应用安装、格式、笔记、听读、导出、同步均待查；D1，E-MOA-01、02 |

手册还列WiFi、蓝牙、充电指示与OTA变化提醒：白灯充电闪烁、充满常亮。它没有证明任意Android App均可正常运行，也未给目标App的墨水屏适配策略。没有可靠资料可说明学习奖励、错题或间隔复习；这些能力不能从Android系统名推导。

## 分支与数据恢复缺口

空书架、文件名冲突、导入半成品、查词无命中、误操作撤销、删书恢复、离线可读范围、休眠续播和云端冲突都未知。卡死恢复只回答怎样再次尝试开机，未回答原文位置、笔记、第三方App状态保存在哪里。手册镜像对长按Logo的翻译有歧义，不能把它直接写成已确认的全局刷新键。

对G-001/G-002的推断：口袋机把音量和翻页复用在同一按键上，可减少键数，但跨阅读/听书时需要可见的当前功能。儿童的“返回”要能预测，成人改键也应有恢复默认路径。Plus若采用类似复用，应把按键映射、应用状态、阅读位置作为独立数据对象；仅有卡死重启不能替代中断恢复设计。以上是本项目分析，不是对墨案体验或算力的实测判断。

最小补证：先取得原厂中文说明书与固定构建截图，解决Logo长按语义；再用同一份短文记录导入、查词、返回、待机、断网和重启。后续Mini与大屏机必须各自确认系统、实体输入和阅读引擎后再扩展。
''',p)

p='M-055';name='M-055-hisense.md'
E('E-HIS-01',p,'海信商城 A9 羽墨6GB+128GB商品页','https://mall.hisense.com/items/4769','型号标题；下架状态；详情图链接','官方商城正文可读；详情图取回403、独立HTML请求超时，未观察图像。下架不推导停止售后/固件服务。','Hisense A9 6GB+128GB China SKU; firmware unknown','official_store')
E('E-HIS-02',p,'海信论坛用户：A9（HLTE556N）反馈和建议','https://fans.hisense.com/thread-206872-1-14.html','2023-04-17第2楼；用户海信手机忠实粉；自报NK OS L2037.6.06.01.00','早次打开正文读到步骤；本轮复访改为IP/账号访问限制。普通用户报告，不是海信确认；未存完整正文或执行复现。','Hisense A9 HLTE556N; user-reported historical build; not generalized to current firmware','first_person_forum_report','opened_text_then_access_restricted',fw='NK OS L2037.6.06.01.00 (as reported)',updated='2023-04-17')
for n,t,d,ids,b,g in [(1,'A9阅读主任务及通信/音频范围','D1','E-HIS-01','阅读手机型号与容量固定；官方页面现下架','导入开书、查词划线、音频、实体键、同步恢复'),(2,'两侧手势下系统面板误传到背景','D2','E-HIS-02','用户自报2023构建；底部右/中两路','官方确认、修复版本、可靠规避、阅读位置影响')]:F('055',n,t,d,ids,b,g,name)
case(name,r'''
# M-055 海信：A9 历史系统手势问题与阅读主链待查

查阅2026-10-26。官方商城范围固定 **A9 羽墨6GB+128GB**；另取一位用户自报 **A9（HLTE556N）、NK OS L2037.6.06.01.00** 的2023年反馈作为独立历史分支，保留帖子原写法，不擅自改成其他系统名称。没有把TOUCH、TOUCH Lite、HiReader、HiReader Pro的纯阅读/音频设置移植到A9。

官方页面当前显示下架，只能说明该销售条目状态；不能据此断定产品停止维护。商品详情图未成功取回，没有视觉观察。论坛早次正文可读，复访遭访问限制，未绕过。无实机或视频；D2仅表示有用户所述的复现动作，**不表示官方已确认缺陷或现版本仍存在**。E-HIS-01、02。

## 功能目录与流程

| F 编号 | 入口/动作 | 已有反馈 | 数据及结束边界 |
| --- | --- | --- | --- |
| F-M055-01 阅读与系统任务 | 官方销售条目定位墨水屏阅读手机，列全网通及Hi-Fi | 只有功能/型号发现，D1 | 无开书、翻页、查词、生词、划线、听书、来电打断、继续阅读的闭合链；E-HIS-01 |
| F-M055-02 全局手势与背景滚动 | 用户设“设置→系统和更新→系统导航→两侧滑动手势”；在可滚动设置页从底右上滑 | 用户称墨水屏控制面板出现时，背景页也滚动；底中上滑回首页也报告同类滚动 | D2历史复现描述；未核实际输入传播、是否影响阅读页和修复版本；E-HIS-02 |

已知目录只足以区分阅读手机、系统导航、墨水屏面板和音频定位。书籍导入、阅读引擎、实体键映射、应用商店、OCR、词典、语音输入、笔记导出、云服务、AI及奖励机制均未取得同型号足够操作帮助。它们为待核功能族，不能写成已经支持的菜单清单，也不能写成不存在。

## 异常、恢复与研究价值

手势问题有触发条件与用户看到的结果，没有可信的恢复步骤。不能自行建议切键制就一定修复；也不能把滚动误触写成数据丢失。来电、蓝牙切换、后台回收、锁屏、断网、重启、取消导入、删书和同步冲突的状态保留均未核。本次资料不足以评估系统层和第三方阅读App间的持久化约定。

对G-001/G-002的推断：全局面板、AI浮层、音量控制必须明确接管哪一层输入；界面出现不等于下层已停止接收手势。对5–6岁自主读听，打开辅助面板后原句不应无意移走；成人长文也需要回到明确锚点。要检验的对象包括前台页位置、浮层状态、触摸事件归属与后台任务，不是仅截图比较按钮是否存在。该结论来自交互风险分析，不是关于A9普遍故障率的断言。

最小补证：取得A9原厂手册/具体构建官方教程，先记录导入→开书→查词→返回→锁屏续读；再依原帖动作观察手势面板是否移动原文。TOUCH和HiReader需各固定型号另建范围，当前案例不能替代。
''',p)

p='M-057';name='M-057-meebook.md';scope='Meebook M7; international English quick-start mirror; system build unspecified'
E('E-MEE-01',p,'深圳皓晴 Meebook 官网','https://www.haoqingtech.com/','厂商联系方式及en1.png型号宣传','正文和首页长图已观察；当前展示P6/P78 Pro/P10 Pro，不能用其规格证明M7。','Manufacturer site; displayed models are not M7','official_product','opened_text_and_static_image',visual=True)
E('E-MEE-02',p,'MEEBOOKM7 FCC用户手册提交记录','https://fccid.io/2A53EMEEBOOKM7/User-Manual/User-Manual-6662213','Document ID6662213；申请人、型号与标题元数据','监管记录镜像；2023-07-13提交，原文档标题为P6说明书，不能据此排除共享模板；原PDF未取回。','FCC ID 2A53EMEEBOOKM7; metadata only','regulatory_filing_mirror','metadata_only',updated='2023-07-13')
E('E-MEE-03',p,'MEEBOOK M7 Quick Start Manual 保存件','https://www.manualslib.com/guide/4006924/meebook-m7-manual.html','Basic operating guidance 1—4、7；Data backups','镜像转录正文；非厂商现行帮助。图示未取回；含条件化笔支持段，未据此认定M7支持笔。',scope,'manufacturer_manual_mirror')
E('E-MEE-04',p,'eReader.co.th M7 历史固件日志','https://ereader.co.th/firmware/','Meebook M7小节：V1.5.2；V1.5.3-2023092114','销售商公布的版本日志；与同页P6/M6/P78分开。未核OEM原始日志或升级效果。','Meebook M7 historical firmware; Thai retailer-published changelog','retailer_changelog','opened_text',fw='MEEBOOK-V1.5.2; MEEBOOK-V1.5.3-2023092114')
for n,t,d,ids,b,g in [(1,'USB载入本地书籍','D2','E-MEE-03','Books根目录；文件管理中找回','支持格式/同名处理/中断/书架索引'),(2,'局域网传书','D2','E-MEE-03','应用页WiFi传书；同路由器','身份验证、成功反馈、重试去重与取消'),(3,'字典与字体导入后使用','D2','E-MEE-03','Dict和fonts为独立目录；本地书Aa选择字体','字典格式/查词上下文/生词复习；字体失败回退'),(4,'翻页键长按和第三方应用适配','D2','E-MEE-04','V1.5.2改长按；应用图标长按优化滚动','不同App实际可用性、默认键值、冲突恢复'),(5,'读书与同步/导出','D1','E-MEE-03;E-MEE-04','日志仅称JD Bookstore阅读记录同步','原生读书进度、笔记导出、全部App统一同步'),(6,'后台管理与恢复','D1','E-MEE-03;E-MEE-04','任务管理/后台管理；历史多项修复声明','清理任务后状态、刷机/重置恢复、修复复现')]:F('057',n,t,d,ids,b,g,name)
case(name,r'''
# M-057 Meebook：M7 导入路径与历史按键适配

查阅2026-10-26。固定 **M7**。快速手册未注明系统构建；另独立标识销售商日志中的 **V1.5.2、V1.5.3-2023092114**，不称为2026最新固件，也不把P6/M6/P78/P10 Pro的功能合并过来。无实机、连续视频或第三方App安装。

厂商官网当前大图主要展示其他型号。FCC记录镜像确认MEEBOOKM7及皓晴申请人，但提交手册元数据标题为“P6说明书”；原PDF未取回。因此操作依据采用明确标成M7的ManualsLib转录保存件，同时保留共享模板风险。带“支持笔版本”条件的段落不能证明M7有笔。只有官网产品静态图被查看，M7手册图未成功取得；没有M7视觉流程。E-MEE-01～03。

## 功能目录与操作

| F 编号 | 前置/动作 | 反馈及数据 | 返回/异常和等级 |
| --- | --- | --- | --- |
| F-M057-01 USB传书 | 电脑连接后复制书到根目录Books；设备打开文件管理 | 可找回并分类所载文件 | 打开到阅读页的细节未给；D2传书子任务，E-MEE-03 |
| F-M057-02 WiFi传书 | 同一路由器下，应用页打开WiFi传书，电脑访问地址或扫码 | 文档给本地传输入口 | 完成提示、重名覆盖、半文件清理未说明；D2，E-MEE-03 |
| F-M057-03 词典与字体 | 自备词典放Dict，词典App检查；字体放fonts，本地书Aa选字体并确认 | 两类资源分开加载 | 查词→原句→生词→练习未闭合；D2仅资源导入/字体应用，E-MEE-03 |
| F-M057-04 翻页键适配 | V1.5.2：用户设置→设备设置改长按；长按第三方App图标进入优化设置滚动 | 日志另称V1.5.3新增长按翻页键竖屏180°旋转 | D2配置入口；未核选项/默认值/实际App反馈，E-MEE-04 |
| F-M057-05 阅读记录与扩展 | 手册列应用安装；V1.5.3日志列京东读书阅读记录同步 | 仅能归到指定App | 原生引擎和其他App的数据不能推定统一同步；D1，E-MEE-03、04 |
| F-M057-06 后台和中断 | 手册给顶部任务管理及应用页后台管理入口 | 可清理不需的任务；未承诺保留编辑 | 日志“修复”不等于本轮复现；D1，E-MEE-03、04 |

目录另涉及刷新策略、应用商店和保修备份；没有核定全部阅读格式、笔记、听书、AI与学习奖励。转录把A2说明和“支持笔版本”字样接在一起，无法确认条件归属，因此不写成确定M7刷新菜单。维修说明要求事先备份，未提供统一备份按钮或恢复保证。E-MEE-03。

## 版本和失败边界

销售商V1.5.2日志列过KOReader翻页键、复制书打不开、microSD书目不全、偶发WiFi失败等修复事项；这是历史问题线索，不是当前缺陷清单，也不是修复有效的实测证明。V1.5.3的京东读书同步不能升级为全部阅读App/本地笔记同步。E-MEE-04。

USB/WiFi重复导入、传输取消、字典格式不兼容、字体切换失败回退、误删除、离线词典、应用更新后按键变化、重启位置及跨端冲突均未有充分证据。可以明确追踪Books文件、Dict资源、fonts资源、按键配置、逐App优化和指定App进度这些对象，但尚不能判断其统一导出方式。

对G-001/G-002的推断：开放系统需要把“系统键映射”和“应用实际接收动作”分别测试；同一翻页键在本机读书和外部App中的结果不能靠兼容口号保证。Plus的AI若要管理导入资源，应能解释文件已传入、索引已出现和可成功打开的区别；对儿童，这三种状态最好减少手工目录操作，对成人则保留可诊断的文件位置。未将应用商店存在推论为适合5–6岁独立使用。

最小补证：原厂M7指南/明确构建；同一短书USB和WiFi各导入一次，观察重名、中断和打开；对原生阅读器及一个第三方App记录两键短按/长按、旋转、退出及重启。后续新增型号单独列系统与键位。
''',p)

p='M-058';name='M-058-tolino.md';scope='tolino shine 5th generation T302; English manual Dec2024 plus current help snapshot; not shine color or older models'
tolpdf='https://mytolino.de/wp-content/uploads/2024/12/DEC24_including-EU-Common-Charger-Directive_tolino-shine-T302-User-Guide-EN.pdf'
E('E-TOL-01',p,'tolino shine T302 英文用户手册',tolpdf,'PDF页5—6、18、23、25—29、37—39、44—47','官方PDF下载后用现有PyMuPDF核读；正文流程，不是设备操作或视觉观察。',scope,'official_manual','downloaded_pdf_text',updated='2024-12')
faq='https://mytolino.com/faq-group/tolino-shine-5-generation-en/page/2/'
E('E-TOL-02',p,'shine 第五代FAQ',faq,'2.1/2.3/2.4/2.6；6.1—6.2；9.7；3作目录边界','官方正文；未注明构建。FAQ更新文件名与软件更新专页不一致；不以FAQ推定旧固件都有TTS。',scope)
E('E-TOL-03',p,'Tolino 软件更新专页','https://mytolino.com/software-updates-for-tolino-ereaders/','shine color/shine5分组；手动更新该两型号例外段','官方正文；列表当前5.12.232736，下载选择还依已有版本。未下载/安装固件。','shine 5 current update catalog on 2026-10-26; firmware not tested','official_update_help','opened_text',fw='5.12.232736 (listed current; not tested)')
E('E-TOL-04',p,'OverDrive Tolino设备页','https://help.overdrive.com/En-us/9983.html','支持设备示例；Getting started；Last Updated','当前正文列shine5及浏览器下载；本轮末次打开显示2026-10-06。搜索索引曾保留需电脑ADE的旧句，未采用为当前结论。','OverDrive current Tolino service documentation; minimum device firmware unspecified','official_service_help','opened_text',updated='2026-10-06')
E('E-TOL-05',p,'OverDrive 在Tolino下载和错误恢复','https://help.overdrive.com/En-us/2305.htm','First authorize；Then download；Resolving errors','2026-10-23正文含ByteBooks ID迁移及旧Adobe例外；仅限该服务，不推广到Onleihe或所有DRM商店。','OverDrive Tolino library loans; new ID policy with pre-2026-06-23 authorization exception','official_service_help','opened_text',updated='2026-10-23')
E('E-TOL-06',p,'Tolino webreader说明','https://mytolino.com/tolino-webreader/','浏览器读书/内容和设备管理；地区提示','正文；本次页面显示所在国家不可用提示，未访问账户。不能据该提示推断用户真实地区资格。','Webreader via participating booksellers; account/region dependent','official_product')
E('E-TOL-07',p,'shine第五代功能表','https://mytolino.com/wp-content/uploads/2024/04/ENG-2023_01-Datenblatt_tolino_shine.pdf','PDF第1页：Included in box明确5th Generation；Functions','正文功能目录D1；PDF标题元数据沿用shine3，正文另有shine color笔误。仅取型号明确的第1页，不混用彩屏/笔功能。',scope,'official_factsheet','opened_pdf_text')
for n,t,ids,b,g in [(1,'开书、回首页与睡眠续读','E-TOL-01','睡眠回原处；完整关机回首页','未保存编辑、后台同步中断后的回补'),(2,'自有电子书导入与离线读取','E-TOL-02;E-TOL-06','USB Books目录或云上传后同步/下载','重名、未完整文件、云与本机删除冲突'),(3,'正文查词/翻译','E-TOL-02','长按词；初次需下载词典','无命中、选错、返回定位、原句生词和练习'),(4,'蓝牙有声书播放','E-TOL-01','Connected反馈；30秒跳转；配对失败重试','重启/断联续播、缓存丢失、时间和书页映射'),(5,'电子书TTS开启和停止','E-TOL-02','阅读菜单三点开始/停止','最低固件、语言、离线音色、外设与后台限制'),(6,'移除本机与删除云藏书','E-TOL-02','本机保留云封面；云删除影响所有端','撤销窗口、客服恢复成功条件'),(7,'同步后恢复与重置','E-TOL-01','先同步；强制关机与出厂重置不同','断电写入、侧载文件备份、冲突处理'),(8,'OverDrive借阅下载与授权错误','E-TOL-04;E-TOL-05','新ByteBooks ID；旧授权例外；到期/账号不一致','T302所需最低固件、不同图书馆具体菜单'),(9,'按型号和原版本更新固件','E-TOL-02;E-TOL-03','专页KoboRoot.tgz/update.tar；FAQ写update.zip','两官方页差异、失败回滚、断电恢复')]:F('058',n,t,'D2',ids,b,g,name)
case(name,r'''
# M-058 Tolino：shine 第五代T302，阅读、借阅与恢复

查阅2026-10-26。固定 **tolino shine第五代T302黑白款**；2024年12月英文手册与本轮官网FAQ分开。更新专页列当前5.12.232736，**不代表手册中每项功能已在该构建实测，也不确定TTS最低版本**。不混用shine3/4、shine color或vision color的控件、笔和屏幕。面向欧洲Tolino联盟书店，书店账户及地区条件另行适用。E-TOL-01～04。

无实机、账号、连续视频或界面观察；D2均为公开步骤重建。产品表的PDF元数据沿用shine3且正文有一处color笔误，本轮按第一页明确的第五代型号取功能目录；不以搜索标题认型号。E-TOL-07。

## 功能目录

已查功能族：本地书、商店书、云端库、导入、书内检索、查词/翻译、划线/笔记、书签、排版、收藏集合、家庭分享、图书馆借阅、有声书、TTS、休眠及更新恢复。全量功能及各国书店差异未穷尽。划线、笔记和家庭分享目前只核能力目录，不能视为完整操作。听书进度与阅读进度被分别描述，不证明逐句对齐。E-TOL-07。

## 操作流程

| F 编号 | 入口/操作 | 反馈和结束/分支 | 尚未核实 |
| --- | --- | --- | --- |
| F-M058-01 开书及续读 | 点封面开书；点页中→回首页；背部电源短按睡眠/唤醒 | 睡眠回原处，完整关机后回首页 | D2；E-TOL-01。未保存编辑及同步恢复 |
| F-M058-02 导入及离线 | USB复制到Books并安全弹出；或webreader上传EPUB/PDF→机上同步→点封面下载 | 已下载书可离线读 | D2；E-TOL-02、06。重复文件与半文件 |
| F-M058-03 查词 | 正文长按词→上下文菜单查词/翻译；初次下载词典 | 解释出现；生词练习没有证据 | D2；E-TOL-02。无命中、取消、回原句 |
| F-M058-04 听有声书 | More→设置→蓝牙连接→开启并选外设；显示Connected后从My Books选有声书 | 播放/暂停，前后30秒或选章；章节可能需先下载 | D2；E-TOL-01。重启续播未知 |
| F-M058-05 文本朗读 | 页中→三点→Read Aloud；同菜单停止 | 开始/停止TTS | D2；E-TOL-02。语言、离线及最低构建未知 |
| F-M058-06 删除范围 | 长按封面→移除→选本机或云 | 本机删除可重下；云删除影响各端，误删联系书店 | D2；E-TOL-02。客服能否恢复未知 |
| F-M058-07 同步与恢复 | 首页同步→立即同步；卡死长按电源至关机再开；出厂重置在More→设置→设备信息→高级 | 重置后须重新设置/同步；本机书及未同步注释会丢失 | D2；E-TOL-01。侧载文件和冲突恢复未知 |
| F-M058-08 图书馆借阅 | 先在Libby或图书馆站借书；阅读器浏览器登录该馆→Loans→下载 | 文件进库，到期自动归还；先满足DRM账户授权 | D2；E-TOL-04、05。T302最低固件未注明 |
| F-M058-09 更新 | 按型号和原版本选文件；电量>20%，USB连接确认→将KoboRoot.tgz或update.tar放.kobo→安全弹出 | 专页称自动重启更新；通常优先WiFi更新 | D2；E-TOL-03。与FAQ文件名冲突，见下 |

## 条件、失败与文档冲突

蓝牙配对失败可在两端关开蓝牙重试；睡眠可能中断同步。重启不是出厂重置，不能用“reset”笼统描述两者。原PDF的普通文字排版与PDF缩放有区别，未证明扫描PDF有OCR重排。E-TOL-01。

**2026借阅变化**：OverDrive现行正文要求新下载使用ByteBooks ID，即使设备仍显示Adobe字段；2026-06-23前已授权情况另列例外。保留旧下载访问时要求同邮箱，新账号不能无条件替代原授权。身份未就绪需重填有效ID，跨设备已履约错误查ID一致，到期错误需重借重下。仅归入OverDrive，不扩展至Onleihe或全部商店。E-TOL-05。

**索引与正文不同**：本轮搜索摘要仍出现shine5须电脑ADE的旧说明；当前设备正文已列shine5可用浏览器。末次正文更新标记为2026-10-06，应以打开的正文和具体服务说明为依据；最低设备版本仍未知。E-TOL-04。

**官网内部不同**：型号FAQ 9.7写update.zip；更新专页却为该代写KoboRoot.tgz/update.tar。本案例保留差异，以更具体的更新专页记录步骤，不自行改名尝试，也不宣称已解决兼容问题。E-TOL-02、03。

Webreader公开页显示本次访问地区不可用提示，故云端路径是有条件流程，不保证所有地区可开通。无障碍Screen Reader另有外设条件，不与普通TTS、有声书播放器合为一个功能。TTS语音包、笔记导出、删注释撤销、多端同时编辑、导入中断、更新失败回退及账户切换数据保留尚未完整核实。E-TOL-02、06。

## 数据、激励与本项目推断

应区分原始文件、本机下载、云端藏书、授权/借期、阅读与收听位置、注释、集合、字典资源和外设配对。收藏集合与家庭分享是组织机制，不能代替学习成效；本轮没有核到间隔复习/答题奖励完整流程。

对G-001/G-002及Plus的推断：AI若能“删除一本书”，必须区分释放本机空间和删除云藏书；“恢复”应先区分重启、重新下载、重新授权和重置。儿童读听需要一个稳定的原文锚点，成人借阅则需要清楚呈现权限和借期。版本/地区/格式条件应属于可查询的数据，避免AI根据旧帮助自动执行错误动作。上述为设计启发，不宣称本项目已经获得Tolino云或DRM接入。

最小观察：T302记录实际构建，用自有无DRM短书走导入→查词→回原句→TTS→睡眠续读；用已授权测试账户核云删除范围与同步，再按可用图书馆验证2026授权。更新文件冲突应先由官方确认，不在真实数据设备上试错。本轮均未执行。
''',p)

(out/'extended-readers-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in evidence))
with (out/'extended-readers-coverage.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(coverage[0]));w.writeheader();w.writerows(coverage)
print('WROTE',len(evidence),'evidence',len(coverage),'flows',len({x['product_id'] for x in evidence}),'cases; 3 selected static images')
