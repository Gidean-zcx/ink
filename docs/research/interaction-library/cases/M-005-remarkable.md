# M-005 reMarkable Paper Pro：原稿、转换结果与跨端数据

## 结构化竞分（2026-10-28）

- 对象/产物边界：
  - 原稿（手写页）、转换文字页、导出副本（PDF/PNG/SVG/链接网页）、云盘导入PDF与“Convert to Notebook”是不同对象（F‑M005‑07/10/11/15）。
  - Trash（30天自动清除）与“单页删除”分离；网页分享链接与设备原稿互不自动同步（F‑M005‑13/15）。
- 撤销/替换与恢复：
  - 选区转换存在“替换选中墨迹/粘贴到光标处”的文档差异，整页转换保留原稿（F‑M005‑07）。
  - Check sync 仅排障；连接正常不代表每个对象已合并（F‑M005‑12）。
- 对 E‑Ink4 Plus 与 5–6 岁+成人家庭的启发：
  - 把“原稿/转换/导出副本/链接/Trash/云同步”拆开呈现；“已显示分数/生成摘要”不等于正确或可恢复。

## 范围

查阅：2026-10-26；原件及交互补证：2026-10-27。对象为 **11.8英寸 reMarkable Paper Pro 与原生系统**，不是 reMarkable 2、Paper Pro Move 或第三方修改系统。以当前官方帮助及 **3.29 发布记录（2026年10月）**为版本参照；实际设备小构建号、手机/桌面app版本未知。早期 Paper Pro User Guide 仅补初次设置和休眠背景，不沿用其中旧促销套餐。当前订阅条件单独核查。[E-RMK-01、02、15]

**D2 文档重建，无实机、无连续视觉观察。** Salesforce 帮助直接打开有时返回 CSS Error；12篇帮助另用 requests 读取到官网完整 Article Body，其他教程采用官方公开索引正文。具体获取方式及页面日期写入 JSONL。原轮PDF仅取得；10月27日已解析69页文字、逐项整理印刷目录并查看p14/p20/p29，正式归档p20单帧。没有逐页完整研读或设备操作。

## 功能目录与范围边界

| 功能域 | 已查任务 | 待查 |
| --- | --- | --- |
| 首次/系统 | Marker配对提示、刷新教学、账户配对、文件主页/抽屉/快速设置 | 完整首次逐屏、密码见后续补证、休眠唤醒位置、后台中断 |
| 组织 | 文件/文件夹/类型筛选、标签、Quick sheets、全文/手写搜索 | 文件移动的当前完整步骤、批量重名、搜索中文效果 |
| 阅读/笔记 | PDF视图、高亮/图层、书写编辑、原稿与文字转换、快速浏览回原页 | EPUB排版新版细节、查词/翻译、TTS/音频/电话；本轮未确认而非“不支持” |
| 数据流 | app/USB/云盘导入、导出、同步诊断、Trash、Connect条件 | 多设备并发冲突、备份版本历史、重装后的选择性恢复 |
| 新增工作流 | 3.22+手写搜索、3.27+Convert and share、3.29回收站自动清除 | Meeting Notes见后续任务；Miro/Slack、Methods所有模板、企业管理仍未深入 |

不宣称完整功能数已核定。刷新模式不照搬 BOOX：官方初次设置展示“屏幕刷新”的教学，已查快速设置是灯光/旋转等；尚未取得面向用户的多档刷新策略选择流程。

## 主要任务与数据状态

| F 编号 | 任务 | 前置状态 | 操作与官方反馈 | 数据变化与结束 | E 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
| F-M005-01 | 开始使用并配对云端 | Paper Pro；网络/电脑或手机可用 | 长按电源 → Next；可将Marker吸附右上或跳过笔设置，向导展示刷新。云配对从my.remarkable.com取得一次性码，输入设备后Continue；旧指南p20明确可跳过。 | 本机可离线存文档；配对是启用云服务的另一步，不等于已付费订阅。 | E-RMK-01；E-RMK-03；E-RMCD-01 | 3.29全部首次逐屏、验证码过期实际反馈、初次同步中断；旧单设备账户限制未采。 |
| F-M005-02 | 切文件、快速设置及返回 | 文档/主页已开 | 左上Menu进My files/类型/标签/Trash；双指从顶下拉Drawer选最近/收藏。右上向下或点电池进入Quick settings调灯光。 | 最近列表是导航入口；亮度设置不改文档内容。 | E-RMK-04；E-RMK-05 | 手指命中/误触、打开另一文档后原页面恢复、睡眠与锁屏状态。 |
| F-M005-03 | 创建可持续编辑的笔记 | My files或文件夹 | “+”→Notebook，输入名→Create；笔记Layers → Template换背景。Quick sheet由“+”菜单或长按“+”进入。 | 生成原生笔记对象；PDF版式模板是独立导入文档，不等于Notebook背景。 | E-RMK-06；E-RMK-07 | 同名处理、模板更换范围、Quick sheet合并/迁移、离线Methods资源。 |
| F-M005-04 | 修改笔画并从误操作恢复 | 原生笔记有墨迹 | 工具栏选笔/粗细/色；Selection圈内容后移动/缩放/复制；Eraser删除，Undo/Redo回退。双指点撤销、三指点重做有官方说明。 | 剪切/复制的内容可跨页重复粘贴直至Clear clipboard；移动/缩放/旋转与翻转均属选区操作。笔画编辑不等于删页。 | E-RMK-08；E-RMK-09；E-RMCD-09 | 撤销历史跨度、关文档后是否保留、误手势、原稿与图层间作用范围。 |
| F-M005-05 | 标注PDF并暂时隐藏标记 | PDF已导入 | 工具栏选Highlighter/Marker；Layers → New layer把标注分层，眼睛图标隐藏/显示。Adjust view可设置自定义留白边距。 | 隐藏层不是删除；PDF原内容与叠加笔迹不同。 | E-RMK-10 | 外部PDF软件的注释语义、隐藏层导出结果、扫描文本是否可选。 |
| F-M005-06 | 查旁页再回原位置 | 文档已开 | 底部上滑Quick browse，拖预览游标，松手打开目标页；点黑点回起点。捏合缩放，点倍率指示复位；PDF Adjust view作用于整份文档。 | 预览/跳页保留返回点，不需重找页码。 | E-RMK-04；E-RMK-06 | 关闭文档后起点保留、链接跳转历史、EPUB重排后返回锚点。 |
| F-M005-07 | 手写转文字并继续改 | Notebook含手写；Wi-Fi、已登录 | Document settings → Convert to text创建新文字页；转字专页说Selection小写a自动同页替换；Selection帮助说指定光标再粘贴且可跨文档，冲突尚未实测。转换后用屏幕键盘/Type Folio改字。 | 整页转换保留原稿；转字专页称选区移除墨迹，实际粘贴机制仍有上述文档冲突。桌面/手机编辑需Connect。 | E-RMK-11；E-RMCD-09；E-RMCD-10 | 选区自动/手动粘贴冲突、替换撤销、取消/超时与批量纠错；PDF原页/附加笔记页适用性仍冲突。 |
| F-M005-08 | 给证据页做标签并找回 | 有文档/页面 | 页内Document settings → Tags →选标签或New tag→Create；文件/文件夹长按→More→Tag。选中标签由白变黑，再点取消。 | Tags总览长按可Rename→Save，Delete→Yes delete tag移除全部文件/页关联；移除某项标签与删除标签本身不同。 | E-RMK-12 | 多标签组合、改名跨端传播、删除标签恢复、同名大小写未核。 |
| F-M005-09 | 用手写内容找页 | Connect；系统3.22+；笔记已同步/索引 | 主页Search输入词，Handwriting筛选，点结果到相应页。初次索引完有提示；新笔记不会立即可搜。 | 已同步索引内容可离线搜；新内容需Wi-Fi；该手写索引只覆盖笔记/笔记页，不覆盖PDF/EPUB正文。 | E-RMK-13 | 中文检索/准确率、断网编辑多久入索引、结果失配恢复；文档所述等待非本轮计时。 |
| F-M005-10 | 导入、导出并保留可编辑原件 | 电脑/手机与设备；格式支持 | 桌面Import或拖入PDF/EPUB；USB接线后Settings → General → Storage → USB web interface，浏览器10.11.99.1传入/导出。桌面右键Export选PDF/PNG/SVG。 | USB支持PDF/EPUB/rmdoc；USB/web上限100MB，更大走app。网页可上传PDF/EPUB/JPG/PNG；Connect可将PDF/DOCX转Notebook。普通输出格式不可自动等同原生备份。 | E-RMK-14；E-RMCD-05；E-RMCD-06 | rmdoc往返保真、网页PDF转换版式/图层、取消断线、同名重复与超大文件错误。 |
| F-M005-11 | 云盘资料导入为PDF或笔记 | 已绑定云盘；转换型需Connect | Menu → Import files →云盘，长按文件→Import→PDF或Convert to Notebook，选保存位置→Import here。 | 文件被导入本机；“云盘集成”不等同原位置自动双向编辑。Convert to Notebook文档页列DOCX/GoogleDocs/TXT且会去表格、分栏、边距；PDF另见新版网页帮助。 | E-RMK-10；E-RMK-16；E-RMCD-03 | 绑定授权过期、离线下载、改后上传重名、表格内容可读性和原文件回溯。 |
| F-M005-12 | 判断云同步为何没完成 | 已配对；有网络 | Settings → Cloud → Check sync看连接；桌面不一致可创建测试文件夹，核对设备/网页是否出现，确认同账号；换网络再查。 | 连接检查通过不证明每个对象已合并；设备与app可能分别出故障。 | E-RMK-17 | 并发写入冲突、离线删除传播、最终一致时间、同步进度的逐文件错误。 |
| F-M005-13 | 删除文件后恢复或永久清除 | 文件/文件夹已选中 | 长按→Delete后在Trash找回；Trash长按→Restore或Delete permanently；Empty Trash清空。3.29增加30天自动删除。 | 单独删页跳过Trash、不能靠文件恢复入口撤销；恢复时限随版本变化；会议笔记在Trash时点对应日历事件也会恢复并打开。 | E-RMK-04；E-RMK-02；E-RMCD-02 | 30天起算/离线计时、云传播、单页Undo、会议事件恢复是否回原文件夹未核。 |
| F-M005-14 | 取消订阅后理解文件状态 | 个人账户；Connect状态变化 | 无订阅仍可设备端手写转换、云盘导入、Screen Share、邮件分享；无限云存储和多端编辑等属Connect。取消后查看具体文件是否仍在同步范围。 | 官方称云文件不因取消直接删除；50天未使用/同步的文件停止跨app更新，本机仍保留。 | E-RMK-15；E-RMK-16 | 两页对50天条件用“未编辑”与“未打开并同步”措辞不同，实际重启同步条件待核。 |
| F-M005-15 | 从手写资料生成网页再校订 | Connect；3.27+；Notebook | Share → Convert and share → Open QR-code，或在自己的邮箱取得链接；网页含转换文字与笔记图，可继续编辑/分享。 | 持链接者可查看/下载DOCX；只有所有者登录可编辑。默认6个月到期可改；设备后续修改不会更新网页。 | E-RMK-18 | 范围选择、即时撤销/删除链接、AI错误回溯与语言/地区、多个链接与网页编辑回流未知。 |

## 版本与异常

| 情形 | 证据支持与恢复边界 |
| --- | --- |
| 同是“转文字”却把笔迹替换了 | Document settings创建新页；Selection专页说替换选中墨迹，另一帮助说选择光标粘贴；保留该冲突，不能笼统说“转换永远保留原稿”。[E-RMK-11、E-RMCD-09、10] |
| PDF中的手写可否转换 | 转换帮助开头写可从ebook/PDF页开始，后文列“handwritten notes in notebooks”；Templates页仍写PDF不能转。本文只把Notebook列为已重建主流程，PDF内附加笔记页与原PDF页须分开测试。[E-RMK-07、11] |
| 已删除文件想很久以后恢复 | 旧导航帮助未写期限，新3.29明确Trash30天自动删除；不能继续给出无限期恢复承诺。删单页又是另一语义。[E-RMK-02、04] |
| 新笔记搜不到 | 手写索引需先同步/索引，非立即生成；旧资料“只能搜标题和打字”已不能代表3.22+Connect。[E-RMK-13] |
| 网络图标有连接却云功能失败 | 官方指出网络/服务地区可影响同步；检查同账户、Check sync及另一网络。本轮未验证中国大陆可用性，不保证转换/云盘在任意网络成功。[E-RMK-17] |
| 文档链接和本机文件的关系 | 导出文件、公开可访问链接、设备原稿是不同对象；链接功能的删除/权限步骤仍需查，不自动视为私人备份。[E-RMK-18] |

音频、听书、TTS、蜂窝/第三方通话及后台音频恢复未取得本产品官方操作链；不编造成不存在，也不把其他Android墨水屏的行为套入。空库、满盘、密码恢复与重置的实际结果、同步冲突、误删单页的可恢复性仍需设备级观察。

## 观察与项目关系（推断）

没有点击本机、测试手写、测耗时或实拍屏幕。10月27日补看原手册静态配对图；此前未渲染的限制已解除，但仍不是当前UI实操。

对G-002，AI要区分原手写、识别结果、总结、导出文件和分享链接，并报告动作是否替换原稿。“手写识别”“手写索引”“生成式总结”也不是同一技术：官方明确手写搜索不使用大语言模型。[E-RMK-13] 对Plus与成人学习，可借鉴旁页预览的返回点、保原稿转换、页标签与文档标签分离；大屏笔输入和完整色彩不能直接迁移。对5–6岁自主读听，手写工作流只能作组织/误操作恢复参考，不能替代R-018的音频断点研究。

最小后续观察：固定Paper Pro 3.29小构建，手写三行→两种转换→误撤销/删页→导出PDF和rmdoc→另一端重新打开；再离线改同一页制造同步冲突。记录原稿、文字页、Trash和远端版本，确认数据关系而非只看UI是否出现。

## 证据

均查于2026-10-26。动态帮助页面获取限制如范围所述；不将检索到的完整官方正文包装成实机测试。发布页仅简述与本例相关的版本差异，不重述全部更新历史。

| E 编号 | 来源 | 定位 |
| --- | --- | --- |
| E-RMK-01 | [Paper Pro 官方英文 User Guide](https://image.email.remarkable.com/lib/fe3511737364047c771479/m/1/13f08afe-723d-47df-8849-b850a77ec0de.pdf) | 1.1首次设置；官方索引正文，PDF下载成功；旧免费试用信息不采。 |
| E-RMK-02 | [Release notes overview](https://support.remarkable.com/articles/Knowledge/Release-notes-overview) | 3.29/2026-10 Trash30天；3.28与3.22等版本只作范围定位。 |
| E-RMK-03 | [Pair with the cloud](https://support.remarkable.com/articles/Knowledge/Pair-your-reMarkable-with-the-cloud) | 一次性码、离线/有限同步。 |
| E-RMK-04 | [Navigating on your reMarkable](https://support.remarkable.com/articles/Knowledge/Navigating-on-your-reMarkable) | Drawer、Quick browse、视图、Trash、单页删除。 |
| E-RMK-05 | [调节Paper Pro灯光](https://blog.remarkable.com/using-remarkable/setup-and-basics/how-to-adjust-the-reading-light-on-remarkable-paper-pro) | 右上/电池Quick settings；不采眼健康宣传。 |
| E-RMK-06 | [Start creating](https://support.remarkable.com/s/getting-started/step-3) | Notebook、Quick sheet、加页和快速浏览。 |
| E-RMK-07 | [Templates](https://support.remarkable.com/articles/Knowledge/Templates) | Notebook模板与导入PDF区别；PDF转换表述冲突保留。 |
| E-RMK-08 | [Toolbar](https://support.remarkable.com/articles/Knowledge/Toolbar) | 笔、选择、擦除、Undo/Redo、位置与Document settings。 |
| E-RMK-09 | [官方3.0–3.3手势说明](https://blog.remarkable.com/blog/software-updates-3-0-3-3-typed-text-comes-to-remarkable) | 双指撤销/三指重做；历史功能来源。 |
| E-RMK-10 | [Import & annotate](https://support.remarkable.com/s/getting-started/step-5) | 云盘导入选PDF/Notebook、图层及标注边距。 |
| E-RMK-11 | [Convert handwritten notes into text](https://support.remarkable.com/articles/Knowledge/Convert-handwritten-notes-into-text) | 两种转换的数据差异、联网/账号、编辑；官网Article Body已读，Last Published Date 04.09.2026。 |
| E-RMK-12 | [Tags](https://support.remarkable.com/articles/Knowledge/Tags) | 页标签、文件/文件夹标签、黑白状态。 |
| E-RMK-13 | [Handwriting search](https://support.remarkable.com/articles/Knowledge/Handwriting-search) | 3.22+Connect、索引、离线范围、非LLM。 |
| E-RMK-14 | [Importing and exporting files](https://support.remarkable.com/articles/Knowledge/importing-and-exporting-files) | USB/app/web格式与100MB；导出步骤。 |
| E-RMK-15 | [Connect当前套餐FAQ](https://remarkable.com/shop/connect) | 取消后文件保留、50天未编辑停止同步、当前试用条件。 |
| E-RMK-16 | [Using without a subscription](https://support.remarkable.com/articles/Knowledge/Using-reMarkable-without-a-subscription) | 无订阅功能、50天使用/同步措辞、本地保留。 |
| E-RMK-17 | [Help with sync and the cloud](https://support.remarkable.com/articles/Knowledge/Help-with-sync-and-the-reMarkable-cloud) | Check sync、同账号、设备/app分别排查、网络/地区条件。 |
| E-RMK-18 | [Convert and share](https://support.remarkable.com/articles/Knowledge/Create-a-link) | 3.27+Connect、Share/QR/邮件、访问者与编辑身份条件。 |

## 2026-10-27：目录、跨端副本及恢复补证

新增[逐条目录](../batches/remarkable-controls-deepening-inventory.csv)共125行：原指南90个章/节与帮助可见标题/空分类。父项与非功能安全章都保留，不是独立功能数；功能帮助当前只显示Apps/Tools/Features空分类。原手册没有完整应用工具栏目录，不能用其覆盖全软件。原件、复查记录和限制见[来源记录](../artifacts/remarkable-controls-deepening/source-notes.md)。

| F编号 | 任务/前提 | 操作及结果 | 数据与恢复 | E证据 | 待验证 |
| --- | --- | --- | --- | --- | --- |
| F-M005-16 | 从日历建立与回访会议笔记；Connect、3.27+、Google/Outlook主日历；一次绑定一个服务 | my.remarkable Integrations连接→My files日历→点事件创建含标题/时间/参与者/议程的Notebook；周期事件进入已有本添加一页；对应本在Trash时点事件恢复并打开 | 编辑笔记不写回日历；创建后保存日历信息副本。Outlook改周期可能产生新series ID和新本 | E-RMCD-02；E-RMK-02 | 帮助5场/10小时与3.29三天不同；取消会议、重新授权、离线、多人和重复创建未核 |
| F-M005-17 | 把网页文章送入可写或可读文档；Chrome扩展已安装/配对；默认Notebook需Connect、图片需3.27 | 点扩展一键发Notebook；右键选EPUB或Options改默认；另可浏览器Print存PDF后导入 | Notebook可编辑，EPUB正文不可直接改但可标注；PDF是另一份导入对象 | E-RMCD-04 | 网页剪裁/登录墙/图片缺失、重复发送、撤销和失败队列未知；EPUB设置段疑似混用打印步骤 |
| F-M005-18 | 用手机扫描多页并存PDF；手机App、网络；摄像头授权流程未给 | Create new→Scanned doc→对准纸张，蓝色提示扫描→继续加扫描或保存；生成前选过滤 | 扫描成为PDF并可同步；不证明OCR、文字层或原照片可回溯 | E-RMCD-07 | 裁剪/重拍/排序/拒绝权限/断网保存、取消和离线结果未知 |
| F-M005-19 | 编辑键入文字及勾选清单；Notebook；屏幕键盘或已连接Type Folio | T→Add/Edit text→宽/窄列；双/三/四击选词/段/本全文→拖柄→剪切/复制/加粗；格式菜单建checkbox，点或打勾后文本划线 | 键入文字与笔画对象分开；橡皮不擦键入文字；PDF需Add note page，不能直接在原PDF页打字 | E-RMCD-10 | 选区手势容错、撤销历史、跨页全选与键入清单状态对外接口未知 |
| F-M005-20 | 呈现文档并停止共享；已配对云；电脑/网页；网页需3.27+；双方网络可不同 | 网页先Screen Share，再设备三点→Share→Present；桌面走设备入口后Accept邀请；停止用Stop sharing your screen；桌面悬笔显示指针 | 每次显示一份文档，My files主页不投出；可切文档/方向。官方网络/防火墙条件不保证任意网络成功 | E-RMCD-11；E-RMCD-05 | 断线/重连、停止是否所有客户端立即生效、音频/延迟未知；灰阶通用措辞不套Paper Pro |
| F-M005-21 | 设置设备密码并在忘记后恢复；Paper Pro；4/6/8位；恢复码另行保管 | Settings Security启用→输入再确认；不匹配重输。错码逐渐延长等待至1小时；多次失败后Reset passcode→恢复码→新密码 | 恢复码路径重设访问凭据；没有恢复码则官方转恢复出厂，未同步内容不能凭账户找回 | E-RMCD-08；E-RMCD-12 | 错误次数阈值、恢复码丢失与轮换、等待后状态未知；不沿用RM1/2账户审批路径 |
| F-M005-22 | 恢复出厂后重建已同步资料；设备可访问；先检查Cloud同步；不可撤销 | Menu→Settings→General Account→Reset→Factory reset确认；重走设置后重新配对 | 清除设备文件/设置；仅已同步文件可回流，未同步内容永久丢失；不是含所有应用状态的整机镜像 | E-RMCD-12；E-RMCD-08 | 恢复耗时、选择性恢复、同步冲突、断电和固件降级未知；未执行重置 |

需要保留的版本条件：会议帮助10月20日写5场/未来10小时，10月23日3.29发布记录改为未来三天，三天内数量上限未知。旧手册的Storage同步入口与账户密码恢复不能覆盖当前Cloud和Paper Pro恢复码路径。Screen Share共享帮助的灰阶描述不套入彩色Paper Pro。网页导入支持PDF转笔记与较早Convert to Notebook格式列表按入口分别记录。

对G-002的候选启发：页面相似不代表数据联动——日历笔记是副本，分享网页不随设备修改，投屏是单文档呈现。AI执行前后需能够说明修改哪个对象、是否传播、如何恢复；这只是从竞品得到的设计问题，还未确定本项目接口或账户方案。对G-001，网页导入路径可帮助积累主题资料，但文章内容质量与转换可读性仍须评估，不能用成功上传替代学习收益。

补充证据链接及版本、原件散列见[来源记录](../artifacts/remarkable-controls-deepening/source-notes.md)与[原有来源复查](../artifacts/remarkable-controls-deepening/rechecked-existing-evidence.json)。所有新增任务均D2；只有原指南一张静态页，没有新增设备/网页产品实操。
