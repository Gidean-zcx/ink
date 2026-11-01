# M-005 reMarkable Paper Pro：原稿、转换结果与跨端数据

## 范围

查阅：2026-10-26。对象为 **11.8英寸 reMarkable Paper Pro 与原生系统**，不是 reMarkable 2、Paper Pro Move 或第三方修改系统。以当前官方帮助及 **3.29 发布记录（2026年10月）**为版本参照；实际设备小构建号、手机/桌面app版本未知。早期 Paper Pro User Guide 仅补初次设置和休眠背景，不沿用其中旧促销套餐。当前订阅条件单独核查。[E-RMK-01、02、15]

**D2 文档重建，无实机、无连续视觉观察。** Salesforce 帮助直接打开有时返回 CSS Error；12篇帮助另用 requests 读取到官网完整 Article Body，其他教程采用官方公开索引正文。具体获取方式及页面日期写入 JSONL。官方PDF成功下载但未本地渲染/完整解析，不将其算视觉证据。

## 功能目录与范围边界

| 功能域 | 已查任务 | 待查 |
| --- | --- | --- |
| 首次/系统 | Marker配对提示、刷新教学、账户配对、文件主页/抽屉/快速设置 | 完整首次逐屏、密码错误/重置、休眠唤醒位置、后台中断 |
| 组织 | 文件/文件夹/类型筛选、标签、Quick sheets、全文/手写搜索 | 文件移动的当前完整步骤、批量重名、搜索中文效果 |
| 阅读/笔记 | PDF视图、高亮/图层、书写编辑、原稿与文字转换、快速浏览回原页 | EPUB排版新版细节、查词/翻译、TTS/音频/电话；本轮未确认而非“不支持” |
| 数据流 | app/USB/云盘导入、导出、同步诊断、Trash、Connect条件 | 多设备并发冲突、备份版本历史、重装后的选择性恢复 |
| 新增工作流 | 3.22+手写搜索、3.27+Convert and share、3.29回收站自动清除 | Meeting Notes、Miro/Slack、Methods所有模板、企业管理未深入 |

不宣称完整功能数已核定。刷新模式不照搬 BOOX：官方初次设置展示“屏幕刷新”的教学，已查快速设置是灯光/旋转等；尚未取得面向用户的多档刷新策略选择流程。

## 主要任务与数据状态

| F 编号 | 任务 | 前置状态 | 操作与官方反馈 | 数据变化与结束 | E 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
| F-M005-01 | 开始使用并配对云端 | Paper Pro；网络/电脑或手机可用 | 长按电源 → Next；可将Marker吸附右上或跳过笔设置，向导展示刷新。云配对从my.remarkable.com取得一次性码，输入设备后Continue。 | 本机可离线存文档；配对是启用云服务的另一步，不等于已付费订阅。 | E-RMK-01；E-RMK-03 | 当前3.29全部向导、离线跳过路径、验证码过期、初次同步中断。 |
| F-M005-02 | 切文件、快速设置及返回 | 文档/主页已开 | 左上Menu进My files/类型/标签/Trash；双指从顶下拉Drawer选最近/收藏。右上向下或点电池进入Quick settings调灯光。 | 最近列表是导航入口；亮度设置不改文档内容。 | E-RMK-04；E-RMK-05 | 手指命中/误触、打开另一文档后原页面恢复、睡眠与锁屏状态。 |
| F-M005-03 | 创建可持续编辑的笔记 | My files或文件夹 | “+”→Notebook，输入名→Create；笔记Layers → Template换背景。Quick sheet由“+”菜单或长按“+”进入。 | 生成原生笔记对象；PDF版式模板是独立导入文档，不等于Notebook背景。 | E-RMK-06；E-RMK-07 | 同名处理、模板更换范围、Quick sheet合并/迁移、离线Methods资源。 |
| F-M005-04 | 修改笔画并从误操作恢复 | 原生笔记有墨迹 | 工具栏选笔/粗细/色；Selection圈内容后移动/缩放/复制；Eraser删除，Undo/Redo回退。双指点撤销、三指点重做有官方说明。 | 笔画编辑不等于删页；工具栏可移动，部分工具随位置移进Document settings。 | E-RMK-08；E-RMK-09 | 撤销历史跨度、关文档后是否保留、误手势、原稿与图层间作用范围。 |
| F-M005-05 | 标注PDF并暂时隐藏标记 | PDF已导入 | 工具栏选Highlighter/Marker；Layers → New layer把标注分层，眼睛图标隐藏/显示。Adjust view可设置自定义留白边距。 | 隐藏层不是删除；PDF原内容与叠加笔迹不同。 | E-RMK-10 | 外部PDF软件的注释语义、隐藏层导出结果、扫描文本是否可选。 |
| F-M005-06 | 查旁页再回原位置 | 文档已开 | 底部上滑Quick browse，拖预览游标，松手打开目标页；点黑点回起点。捏合缩放，点倍率指示复位；PDF Adjust view作用于整份文档。 | 预览/跳页保留返回点，不需重找页码。 | E-RMK-04；E-RMK-06 | 关闭文档后起点保留、链接跳转历史、EPUB重排后返回锚点。 |
| F-M005-07 | 手写转文字并继续改 | Notebook含手写；Wi-Fi、已登录 | Document settings → Convert to text创建新文字页；Selection选手写→小写a转换则同页替换。转换后用屏幕键盘/Type Folio改字。 | 前者保留手写原页，后者移除被选墨迹；不是两种相同的转换入口。桌面/手机编辑需Connect。 | E-RMK-11 | 识别差错批量修正、取消/超时、选区转换撤销；PDF/EPUB适用性见下方冲突。 |
| F-M005-08 | 给证据页做标签并找回 | 有文档/页面 | 页内Document settings → Tags →选标签或New tag→Create；文件/文件夹长按→More→Tag。选中标签由白变黑，再点取消。 | 页标签与文件标签是两层对象；去标签不删内容。 | E-RMK-12 | 多标签组合检索、标签改名传播、误删标签恢复、同名大小写。 |
| F-M005-09 | 用手写内容找页 | Connect；系统3.22+；笔记已同步/索引 | 主页Search输入词，Handwriting筛选，点结果到相应页。初次索引完有提示；新笔记不会立即可搜。 | 已同步索引内容可离线搜；新内容需Wi-Fi；该手写索引只覆盖笔记/笔记页，不覆盖PDF/EPUB正文。 | E-RMK-13 | 中文检索/准确率、断网编辑多久入索引、结果失配恢复；文档所述等待非本轮计时。 |
| F-M005-10 | 导入、导出并保留可编辑原件 | 电脑/手机与设备；格式支持 | 桌面Import或拖入PDF/EPUB；USB接线后Settings → General → Storage → USB web interface，浏览器10.11.99.1传入/导出。桌面右键Export选PDF/PNG/SVG。 | USB支持PDF/EPUB/rmdoc；USB/web上限100MB，更大走app。普通输出格式不可自动等同原生可编辑笔记备份。 | E-RMK-14 | rmdoc往返保真、取消/断线、同名重复、PDF超大文件错误。 |
| F-M005-11 | 云盘资料导入为PDF或笔记 | 已绑定云盘；转换型需Connect | Menu → Import files →云盘，长按文件→Import→PDF或Convert to Notebook，选保存位置→Import here。 | 文件被导入本机；“云盘集成”不等同原位置自动双向编辑。转换成Notebook改变后续编辑能力。 | E-RMK-10；E-RMK-16 | 绑定/授权失效、下载离线、改后上传重名策略、原始.docx格式保真。 |
| F-M005-12 | 判断云同步为何没完成 | 已配对；有网络 | Settings → Cloud → Check sync看连接；桌面不一致可创建测试文件夹，核对设备/网页是否出现，确认同账号；换网络再查。 | 连接检查通过不证明每个对象已合并；设备与app可能分别出故障。 | E-RMK-17 | 并发写入冲突、离线删除传播、最终一致时间、同步进度的逐文件错误。 |
| F-M005-13 | 删除文件后恢复或永久清除 | 文件/文件夹已选中 | 长按→Delete后在Trash找回；Trash长按→Restore或Delete permanently；Empty Trash清空。3.29增加30天自动删除。 | 单独删页跳过Trash、不能靠文件恢复入口撤销；恢复时限随版本变化。 | E-RMK-04；E-RMK-02 | 删除确认、30天起算/离线计时、已有Trash迁移、云端清理传播、页删除即时Undo。 |
| F-M005-14 | 取消订阅后理解文件状态 | 个人账户；Connect状态变化 | 无订阅仍可设备端手写转换、云盘导入、Screen Share、邮件分享；无限云存储和多端编辑等属Connect。取消后查看具体文件是否仍在同步范围。 | 官方称云文件不因取消直接删除；50天未使用/同步的文件停止跨app更新，本机仍保留。 | E-RMK-15；E-RMK-16 | 两页对50天条件用“未编辑”与“未打开并同步”措辞不同，实际重启同步条件待核。 |
| F-M005-15 | 从手写资料生成网页再校订 | Connect；3.27+；Notebook | Share → Convert and share → Open QR-code，或在自己的邮箱取得链接；网页含转换文字与笔记图，可继续编辑/分享。 | 官方说明AI汇总和下一步；访问者可不注册查看，编辑需登录。原稿、识别文字与AI总结应区分。 | E-RMK-18 | 内容选择范围、删除/撤销链接、访问控制、AI错误回溯、地区/语言。 |

## 版本与异常

| 情形 | 证据支持与恢复边界 |
| --- | --- |
| 同是“转文字”却把笔迹替换了 | Document settings创建新页；Selection转换替换选中墨迹。先辨入口，不能笼统说“转换永远保留原稿”。[E-RMK-11] |
| PDF中的手写可否转换 | 转换帮助开头写可从ebook/PDF页开始，后文列“handwritten notes in notebooks”；Templates页仍写PDF不能转。本文只把Notebook列为已重建主流程，PDF内附加笔记页与原PDF页须分开测试。[E-RMK-07、11] |
| 已删除文件想很久以后恢复 | 旧导航帮助未写期限，新3.29明确Trash30天自动删除；不能继续给出无限期恢复承诺。删单页又是另一语义。[E-RMK-02、04] |
| 新笔记搜不到 | 手写索引需先同步/索引，非立即生成；旧资料“只能搜标题和打字”已不能代表3.22+Connect。[E-RMK-13] |
| 网络图标有连接却云功能失败 | 官方指出网络/服务地区可影响同步；检查同账户、Check sync及另一网络。本轮未验证中国大陆可用性，不保证转换/云盘在任意网络成功。[E-RMK-17] |
| 文档链接和本机文件的关系 | 导出文件、公开可访问链接、设备原稿是不同对象；链接功能的删除/权限步骤仍需查，不自动视为私人备份。[E-RMK-18] |

音频、听书、TTS、蜂窝/第三方通话及后台音频恢复未取得本产品官方操作链；不编造成不存在，也不把其他Android墨水屏的行为套入。空库、满盘、错误密码、强制重启、同步冲突、误删单页的可恢复性仍需设备级观察。

## 观察与项目关系（推断）

没有点击本机、测试手写、测耗时或实拍屏幕。官网下载的历史PDF未成功经web截图/本地渲染观察，不能计单帧；网页索引的图片描述也不替代查看。

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
