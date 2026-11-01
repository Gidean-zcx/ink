# M-002 Kobo Libra Colour：内容进入、阅读、手写、听书与恢复

查阅：2026-10-26。本轮固定 **Kobo Libra Colour，型号 N428**；配套 Kobo 网站、Google Drive、Dropbox、ADE/图书馆只作为设备任务的外部环节。依据型号支持页、官方在线 User Guide 及互补帮助正文；未登录、未操作设备、未检视图片、未观察连续界面，全部流程仍为 **D2 文档重建**。实体设备固件、账户地区、出版物权限未知，不能称为整个型号的功能验收。[E-KBD-01、02]

官方支持目录提供 N428 PDF、新 EAA EPUB 和在线预览；PDF 与 EPUB 下载各遇 HTTP 403，预览页面无可抽取正文，因此**没有读到或归档型号完整手册**。本轮以在线指南目录建立功能域，再以帮助正文重建任务；完整功能总数仍未知。[E-KBD-03]

版本边界：云盘文档最低版本分别为 Dropbox 4.18、Google Drive 4.37；这是功能门槛，不是被测试固件。现行 Libby 页注明 6.0+ 升级后的借书证迁移。另有仅欧盟提供的可选无障碍更新，文中限制标为 July 2026 / 5.18；它与常规软件分开记录，不认定所有地区现行固件相同。网页未给可靠修订日期时保持未知，页脚年份不是版本。[E-KBD-05、06、10、29]

## 功能域目录与覆盖

按在线指南与帮助栏目归类，兼列有出处但尚未深入的主功能；不是把已有流程重新包装成“全部功能”。F 表中的菜单英文保留以方便后续逐项观察。

| 功能域 | 官方章节/来源 | 已分析 | 未分析或条件缺口 |
| --- | --- | --- | --- |
| 初始配置、供电与连接 | User Guide：Get started、Basics、Wi Fi；E-KBD-02 | 同步与重置 F-M002-21、24 | 初始扫码/账户注册、睡眠、充电、Wi-Fi 密码错误有帮助入口，未逐项拆解；本轮无续航估计。 |
| 文件兼容与本地传书 | USB、File formats、ADE；E-KBD-04、08、34 | F-M002-04、07 | 通用格式表另列 TXT、图像、CBZ/CBR 等；这些格式的实际导航、失败提示未分析。格式支持不意味着有同样的选区/排版能力。 |
| 云盘导入与解除连接 | Google Drive、Dropbox 五步及关闭共享；E-KBD-05～07 | F-M002-05、06 | 同名替换、双向删除、并发修改和大文件下载中断未知。 |
| 购书、预览、愿望单、订阅 | User Guide：Buy eBooks；购书正文 E-KBD-09 | F-M002-08 | 愿望单增删有指南入口，未重建；Kobo Plus、优惠退款与地区商店差异尚缺专门核查。 |
| 公共图书馆 | Libby / Borrow eBooks；E-KBD-10、11 | F-M002-09 | 借书证和参与馆资格成立后才有借阅；续借、多卡切换、借阅笔记到期后的状态未分析。 |
| 书库组织、搜索与下载 | User Guide：Find/Download；E-KBD-02、33 | 删除恢复 F-M002-19，离线准备 F-M002-24 | 书库排序、合集创建/批量移动、完成状态与检索空结果未重建。 |
| 阅读导航与误触 | Turn pages、Skip、Orientation、Prevent accidental turns；E-KBD-12、13、16、17 | F-M002-10、12 | 目录/书签有指南入口，未独立分析；翻页按键反转帮助未列 N428，不能直接当型号验证。 |
| 固定版式 PDF | PDF navigation；E-KBD-14 | F-M002-11 | 大页裁切、缩放保持、扫描 PDF OCR 未证实；不把手写当作文本高亮。 |
| 排版、照明与彩色处理 | Font、Brightness、Rainbow；E-KBD-15、18、19 | F-M002-12、13 | 深色模式、暖光定时有指南入口，未核定完整分支；常规阅读的全刷频率入口/档位仍未知。 |
| 词典、翻译、文字批注 | User Guide：Read；原 E-KOB-01～04 | 原 F-M002-01～03 | 下载词典、换语言、批注列表管理未深入；没有把查词历史推定成复习卡。 |
| 书内手写 | Write notes in your book；E-KBD-20 | F-M002-14 | EPUB 与 PDF 出口不同；笔尖维护、笔充电/配对故障有指南入口，未拆解。 |
| 独立笔记本 | Notebook：Basic/Advanced、Search、background/lasso、diagrams/math、folders、backup/export；E-KBD-21、22 | F-M002-15～17 | 模板、套索、搜索、图形数学与文件夹尚有正文可继续分析；识别中文、冲突合并与恢复副本是否可编辑未知。 |
| 有声书与蓝牙 | Listen / Find / Bluetooth；E-KBD-23、02 | F-M002-18 | 音频订阅权益、睡眠定时、侧载音频、断网换章、断连/重启续播未知。无障碍 Read Aloud 另见 F-M002-22。 |
| 稍后读文章 | Instapaper；E-KBD-31 | F-M002-23 | 文件夹与个人标注不随此集成进入；不能把它当完整 Instapaper 客户端。 |
| 数据清理、修复与恢复 | Delete、Archive、Repair、Reset、N428 manual reset；E-KBD-24～28、32 | F-M002-19～21 | 未同步个人数据、导入文件与账户书不能用同一恢复保证；恢复失败与数据完整性未知。 |
| 无障碍、安全与系统 | User Guide：Security/Accessibility/Repair；E-KBD-02、29 | 无障碍条件化 F-M002-22 | 锁屏 PIN、Large Print、OpenDyslexic、维修等有入口但未完整分析；不同软件分支的功能差异待设备核验。 |

## 已有查词与文本批注流程

## 流程与状态

| 流程 | 操作与反馈 | 保存/结束 | 证据 |
| --- | --- | --- | --- |
| F-M002-01 查词后继续阅读 | 在正文长按一个词，出现下划线；松手显示释义；轻点屏幕关闭。某些 PDF、借阅书或免费网络书可能不适用。 | 文档说明关闭词典的动作；是否计入生词本、是否保存查询历史未核实。 | E-KOB-01 |
| F-M002-02 标注与写笔记 | 长按词并松手出现选区手柄，拖动端点选文本；选区会自动高亮，可撤销高亮。选择 Add Note 后输入文字、Save。 | 兼容书籍的笔记可随账户同步；第三方书籍笔记仅在本机，删除该书也删除笔记。彩色机型可改高亮颜色。 | E-KOB-02、03 |
| F-M002-03 导出批注 | 先同步阅读设备；网站登录 → My Account → My Books → 对应书 Actions → Export Annotations → 选格式 → Generate File；到 Annotations 下载。 | 可选 PDF/HTML/TXT/Markdown；生成文件在账户保留 30 天。这是导出文件的保留期，不能解释为笔记 30 天后消失。 | E-KOB-04 |

## 新增主要任务

这些是文档给出的操作链；“保存”“成功”等描述指文档的状态，不代表本项目已经观察到。通用帮助未给固件时不得把菜单页码当作所有版本固定值。

| 流程 | 前置/入口、动作与反馈 | 保存、退出/异常与未知 | 证据 |
| --- | --- | --- | --- |
| F-M002-04 USB 导入自有书 | DRM-free EPUB/PDF；开机连电脑 → Computer detected 中 Connect → 电脑打开 KOBOeReader 盘 → 复制文件 → 安全弹出并拔线。设备显示 Importing content，随后 My Books 点书阅读。 | 本地导入不会因 Kobo 同步自动出现在其他设备/app。检测不到设备时可改为直连、检查线缆；中断导入、重复文件、损坏书恢复未知。通用导入文案仍写 micro-USB；N428 的 USB-C 依据手写导出页，不照抄旧接口。 | E-KBD-04、20 |
| F-M002-05 Google Drive 传书 | Wi-Fi、兼容软件、DRM-free 文件。More → My Google Drive → Get Started 获取码；外部浏览器到 kobo.com/googledrive 填码、批准共享并授权 Google。成功后向自动建立的 Rakuten Kobo 文件夹放书；设备 Sync now，或云盘列表点文件复制到 My Books，再打开。 | 不出现时列表菜单 Refresh/Download all，检查目录、文件名与网络。Google 设置 Manage Apps → Kobo → Disconnect from Drive 可撤权；**已下载本机副本仍在**。代码过期、拒绝授权后返回位置与覆盖策略未知。 | E-KBD-05 |
| F-M002-06 Dropbox 传书与退出共享 | Wi-Fi、4.18+；More → Settings → Accounts → Dropbox/Get Started 取码；浏览器 kobo.com/dropbox 输入、登录并 Allow，设备显示已连接。文件放 Apps/Rakuten Kobo → Sync now → My Books；More → My Dropbox 也可刷新后点文件下载。 | 列表缺失检查文件夹、特殊字符、Wi-Fi。关闭共享：More → Settings → Account → Dropbox/Sign out → 再确认；以后不再下载新文件，既有副本保留。该退出不等于退出 Kobo 主账户。 | E-KBD-06、07 |
| F-M002-07 外部 DRM 书经 ADE | 电脑已有 ADE，来源允许下载；按现行 Kobo 帮助以 ByteBooks ID 授权电脑，再连设备并 Connect → ADE 的 Devices 齿轮 → Authorize Device。将来源 .acsm 在 ADE 打开取得书，拖至设备 → 安全弹出；书应出现在 Library。 | 已有 Adobe 授权可用到再次要求登录；遇 E_ADEPT_RESET_PW_REQUIRED，帮助要求用原邮箱迁移 ByteBooks 后重登。ACSM 是授权入口，不能当普通 EPUB 直接复制。授权额度、离线过期和迁移失败未知；没有执行安装/迁移。 | E-KBD-08 |
| F-M002-08 从阅读器购书到打开 | Wi-Fi、有可用付款方式；Home → Discover → 搜标题/作者/ISBN 或选封面 → Buy → 必要时填账单信息 → Continue → 核对订单 → Confirm。购买完成书出现在 Home。 | 可 Keep Shopping 或回 Home；书未出现先 Sync now。保存过账单资料可跳过填写；优惠只限合格书。支付失败、取消扣款与退款未重建；此表仅研究入口，没有实际交易。 | E-KBD-09 |
| F-M002-09 借书、候补与到期 | N428 在内置借阅兼容列表；需支持地区、参与馆及有效借书证。现行帮助给出 Library Cards 或 Settings → Libby 两种绑定入口，按页面 QR/代码完成授权。Home → Discover → OverDrive/Libby → 选书 → More → Borrow；可借时确认成功，同步后入书库。 | 无副本可 Place a hold 并确认，Go to Holds/通知铃查看，轮到后 Borrow now；到期自动失去访问。多数已下载借书可在借期内离线读。借书须有对应 Kobo 版本，Libby 音频不能在阅读器播放。6.0+ 要重加借书证；入口差异、续借与到期批注未知。 | E-KBD-10、11 |
| F-M002-10 翻页、预览、跳转与回原位 | 普通阅读点/划左右边翻页；点中央出 Reading Menu，章节前后按钮跳章，拖导航条出现页面预览；点预览才进入该页。 | 点预览外退出；导航条黑点或 Back to page 返回先前位置。N428 确有实体翻页键，但通用页的键反转/长按快翻型号列表未列 N428，暂不确认其细节。无障碍 5.18 分支缺少预览/快翻，见 F-M002-22。 | E-KBD-01、12、13、29 |
| F-M002-11 PDF 放大后导航 | PDF 阅读点中央 → Navigation → 拖缩放条或捏合，拖页面平移。未放大时两侧点/划翻页；放大后先平移到页边再点，或用导航条翻页。 | Navigate → Rotate 可转 90°，再次点回原向。普通文本选择、词典和改字号不适用于此 PDF 交互；手写另看 F-M002-14。旧页“普通书只能竖屏”不适用于已明确支持旋转的 N428；缩放中断保持未知。 | E-KBD-14、16 |
| F-M002-12 排版、方向与防误触 | EPUB 等可重排书点中央 → Text 调字体、字号、行距、边距、对齐；偏好会用于其他书，PDF 不走此链。中央 → 旋转图标可锁横/竖，图标显示锁定，选 Auto rotate 恢复。易误翻时中央 → 齿轮 → 文档所示第 3 页 → Page forward and back by → Swiping only → X 回阅读。 | 误触设置改变触摸翻页方式，不是关闭所有触控。字形覆盖、跨书例外、按钮与触摸设置相互影响未知；具体设置页码依软件确认。 | E-KBD-15～17 |
| F-M002-13 亮度与彩色显示取舍 | 阅读时沿左边上下划改亮度，或 Home 亮度图标拖滑块。彩色图出现彩虹纹时：中央 → 齿轮 → 文档所示第 2 页 → Reduce rainbow effect 勾 On → X 回阅读。 | 官方提示此处理可能降低性能或使图模糊；没有效果/速度实测。**这不是全刷频率设置**。笔记本的清残影见 F-M002-15；常规阅读全刷入口和默认档位未找到可靠型号正文。 | E-KBD-18、19 |
| F-M002-14 书内手写、排版变更与 PDF 取回 | Stylus 在兼容 EPUB/KEPUB 或允许批注的非保护 PDF 上写画自动保存；中央 → Pen → 选色 → 手指点外部收菜单。改字号后笔迹可能缩成边栏标记；点标记 → 预览菜单 → Revert to these settings 可回原排版。 | EPUB 手写留本机，不随设备/app 转移或导出；PDF 笔迹存入文件，USB-C/Connect 后从 KOBOeReader 拷出。批注列表长按 → Remove markup → Continue 为永久删除。重置/登出会丢手写；无障碍分支缺少恢复排版选项。 | E-KBD-20、29 |
| F-M002-15 新建 Basic 笔记并结束 | Home → My Notebooks → New → 命名 → Basic → Save；写画自动保存。右上箭头展开菜单，可 Undo/Redo；写后左划建下一页。 | 展开菜单后点左上返回关闭。残影用菜单 Refresh page 清屏；与 Clear notebook 删除内容分开。突然掉电的最后笔画未知。 | E-KBD-21 |
| F-M002-16 Advanced 转文字并校订 | 选 Advanced 笔记后书写；菜单 Convert All 转文字。想改识别词：手指双点词使其放大 → 笔划掉旧词 → 写新词 → 双点句子保存。 | 仅 Advanced 有此编辑手势；向下划分隔、向上划连接，可编辑词间空格。识别错误可以人工改，不等于自动正确；多语言、离线识别、转换撤销边界未知。 | E-KBD-21、22 |
| F-M002-17 笔记本导出与同步开关 | 笔记旁菜单 → Export → Export to computer → 选格式 → Export；待 Notebook exported，USB/Connect 后从 Exported Annotations 复制文件。 | Basic 可导 PDF，Advanced 可导 DOCX/TXT；导出文字不能反向作为原笔记导回。Settings → Syncing and updates 的 Sync My Notebooks 控制云备份，默认开启；同步冲突、导出失败恢复未知。 | E-KBD-21 |
| F-M002-18 下载并用蓝牙听书 | Kobo 有声书已购入；My Books 点音频封面，下载后开播放器。下载图标表示仍需 Wi-Fi，Downloading 表示正在取文件。No Bluetooth Connected → 扫描 → 选耳机/音箱，成功显示 Connected；未发现先设配对模式再 Rescan。 | Play/Pause、前后 30 秒、章内时间轴和目录跳章；音量按钮与 1X 可调，倍率 0.5–3。暂停是已知结束动作，退出/重启续播未核实。跳到未缓存章仍需下载；未确认 N428 整本离线断连/换章表现。不可播放时可移除下载重取或修复账户。 | E-KBD-23 |
| F-M002-19 删除下载、移出账户与恢复 | My Books 长按封面 → Remove 弹窗。默认 Remove download 只删本机，封面仍在可再下载；选 Remove from My Books 会移出账户及各端可见书库。确认 Remove。 | 后者可在 Kobo.com 的 My Books → Archive → 书旁菜单 → Restore to Library → Yes, Move to Library → Close，再同步设备恢复书库项。此恢复针对账户内容；不保证恢复被删的外部文件和个人手写。 | E-KBD-24、25 |
| F-M002-20 缺书/坏书的修复 | 缺少账户书且有 Wi-Fi：More → Settings → Device information → Repair your Kobo account/Repair → Repair now；等待修复后到 My Books/搜索检验。页面显示异常时也可中央菜单 Back to Home，再重开书。 | 修复未解决再核对账户/Archive；登出不是无损刷新，个人数据边界先看 F-M002-14、17、21。修复进度、取消、失败错误码、修复本地导入数据库的范围未知。 | E-KBD-26、32 |
| F-M002-21 工厂重置与 N428 手动恢复 | 常规菜单：More → Settings → Device information → Advanced/Factory reset → Reset now，随后重新配置并同步。若仍不能解决，型号专页要求预充至少 1 小时、可用 Wi-Fi；关机至 Powered off → 按住一个翻页键，同时按背部电源键；按文档灯灭约 2 秒的节点释放两键，黑屏进入 restoring。 | 清本机书、导入文件、账户登录及设置；可重下载 Kobo 购买书，未同步批注不能据此保证恢复。专页灯“常亮/闪烁”描述不完全一致，须按型号完整原文并实机确认，不能概括成通用“长按若干秒”。恢复中断/重试代价未知；未实际重置。 | E-KBD-27、28 |
| F-M002-22 欧盟可选无障碍分支 | 仅官方列出的欧盟更新分支：More → Settings → Accessibility → SYSTEM UPDATE REQUIRED/Enable；需重设设备。分支就绪后，EPUB/KEPUB 中央菜单 → More → Read Aloud；同入口 Stop Read Aloud 结束。屏幕朗读另要求蓝牙音频设备及键盘。 | 此更新/回退均有重置代价；Device Information → Revert 回旧软件会失去新增能力。July 2026/5.18 限制表含页面预览、快翻、部分格式/亚洲语言及 Instapaper；不能推广成常规 N428 的永久缺失。具体语种、语音缓存、各版本与 6.0 的对应未知。 | E-KBD-29 |
| F-M002-23 网页文章进入阅读器 | 已有 Instapaper 帐户；More → My Articles → Link with Instapaper → 手机扫码/浏览器填码 → Authorize。外部保存文章并同步，回 My Articles 点开；缺文时菜单 Refresh。 | 返回箭头回文章列表；Archive 会关闭当前文并归档。只有主列表进入，放文件夹的文章需移回；标签、标注、视频/PDF 不由此集成提供。正文无最低固件，且无障碍 5.18 分支排除，不能认定所有 N428 都有入口。 | E-KBD-31、29 |
| F-M002-24 同步与离线准备 | Home → Sync → Sync now，连接 Wi-Fi 取账户书和更新并同步适用批注；未知网络会进入添加网络流程。需要预先备书时 My Books → 灰栏 More → Download all，等待下载。 | 书到 Home/My Books 可点开；通用帮助明确批量下载便于离线读。它不证明所有文件/音频章/侧载批注有云副本，也不承诺断点重试。同步页含过时“阅读器不支持音频”句，不采用该句；N428 音频以专页为准。 | E-KBD-30、33、23 |

## 容易被功能列表掩盖的边界

- “选中文字”同时可能改变持久数据：自动高亮后，取消选区与撤销高亮是不是同一动作，不能只从长按手势判断。文档给出 Undo Highlight；误触关闭后还能怎样撤销，待操作。
- 此处文本高亮方式不适用于 PDF；本轮补查的 N428 非保护 PDF 手写是另一条操作及数据链。
- 导出限定 Kobo 购买/Kobo Plus 书，并受出版方许可影响，不含手写标记。外部导入书、购买书和手写内容的数据出口并不相同。
- 账户书移入 Archive 后的回收路径已补齐，但它不能恢复所有个人积累。“下载副本”“账户书目”“EPUB 手写”“PDF 内笔迹”“独立笔记本”需要分开判断。

| 分支/失败 | 已知处理或条件 | 仍缺什么 |
| --- | --- | --- |
| 云端已有文件、本机列表空 | F-M002-05、06 的目录/文件名/网络检查与刷新；解除共享后保留已下载件。 | 空库占位、授权过期提醒、同名覆盖及部分下载回滚。 |
| 账户书消失 | F-M002-19、20 区分云端归档、本机缺下载、账户错误及修复。 | 被出版方下架、原购买资格变化和恢复历史。 |
| 借阅不可用 | F-M002-09 区分地区/馆/书目资格和副本已借完；到期不是永久购买书。 | 官方两个页面绑定入口不一致；地区按帮助列出的范围，不能补推未列地区支持。 |
| 手写因重排看不见 | F-M002-14 有标记预览及恢复原排版；软件分支可能缺该动作。 | 跨页锚点、重排后是否有笔迹错位、语言更换影响。 |
| 笔记误操作或残影 | F-M002-15 的撤销与刷新属于不同动作。 | 整本误删后的云端保留、版本历史与可编辑恢复未分析；不承诺回收站。 |
| 下载/播放中断 | F-M002-18 区分下载图标、下载中、未缓存章及蓝牙连接状态。 | 断网续传、蓝牙断连是否暂停、睡眠/掉电后的音频断点；不以手机 app 的离线说明代替 N428。 |
| 恢复出厂 | F-M002-21、22 区分修复账户、一般重置、型号双键恢复和无障碍分支回退。 | 现场LED时序、损坏数据库与恢复中断，无连续视觉或实际测试。 |

## 数据对象与学习动力

已能区分：来源文件及授权票据、账户书/归档项、设备下载副本、借阅资格/期限/候补、阅读位置与预览历史、文本批注、书内笔迹、笔记本及导出件、音频章节与下载/播放状态、云盘关联。以上各有 F/E 出处；它们之间不构成已验证的通用同步模型。

学习激励方面，本轮确认的是降低“把内容带到设备并继续读”的操作成本，以及人工批注、手写整理和纠错。没有证据证明其能提高学习成绩，亦未重建测验、间隔复习或儿童奖励链。官方无障碍页出现 Awards 作为分支限制，不足以推断常规固件奖励的具体规则。

## 对本项目的意义（推断）

关联 R-016、R-017、R-018、G-002 与 H-004，以下均为项目启发，尚未升级为需求：

- AI 找书/传书应显式暴露来源、权限、下载状态和“已入本机书库”的结果；仅完成云盘授权不能汇报导入成功。
- 阅读位置应区别实际跳转与预览，可让 AI 查一段资料后返回原处；文本选区不自动等同于用户要求永久标注。
- 原始笔迹、转换文字与导出件需要关联但保留区别；识别错误应能校订，避免将不可逆导出误当完整备份。
- 5–6 岁自主读听应能知道“这一章已经可离线播放”，而非只显示一张音频封面。是否实现读听对齐仍是本项目待讨论项。
- AI 删除/恢复操作应说明作用对象：只清设备缓存、移出账户书库、删除批注或重置整机；这里没有决定本项目策略，也不声称 Kobo 开放这些 AI 接口。

## 后续观察脚本

1. 固定 N428、账户地区、常规/无障碍分支及完整固件号；记录 Libby 绑定入口与翻页键设置，解决现行帮助差异。
2. 同一短文准备自有 EPUB、非保护 PDF 和有权限的商店书；观察导入、预览返回、长按选区、手写重排、断网重开及三种导出出口。
3. 用专门测试文件观察重复导入、USB 中断、云盘撤权和账号修复；分别核对设备副本与个人积累，不能靠书封面证明恢复完成。
4. 合法缓存整本音频，逐项记录断网换章、耳机断连、睡眠及重启续播；当前未执行。
5. 获取可读的型号完整手册或固定固件公开录屏，补齐全刷、合集、目录/书签、暖光定时及主功能未分析项。仅静态图仍不升级 D3。

## 证据

- E-KOB-01：[词典操作](https://help.kobo.com/hc/en-us/articles/360017639893-Define-words-with-the-dictionary-on-your-Kobo-eReader)，操作步骤与兼容例外。
- E-KOB-02：[文本高亮](https://help.kobo.com/hc/en-us/articles/360017481334-Highlight-text-on-your-Kobo-eReader)，选区、自动高亮、撤销与颜色。
- E-KOB-03：[添加笔记](https://help.kobo.com/hc/en-us/articles/360017639993-Add-notes-to-your-eBook)，保存及第三方书籍限制。
- E-KOB-04：[批注导出](https://help.kobo.com/hc/en-us/articles/29991333812631-Export-annotations-from-your-books)，网页流程、格式、资格与保留期。

新增 E-KBD 的完整 URL、章节、访问方式、版本与取得失败见 [Kobo 深化证据批次](/home/workspace/reader/docs/research/interaction-library/batches/kobo-deepening-evidence.jsonl)，F-M002-04～24 见 [新增覆盖表](/home/workspace/reader/docs/research/interaction-library/batches/kobo-deepening-coverage.csv)。均查阅于 2026-10-26，未归档图片；原批次及主索引不在本次改写范围。

主要正文入口：[N428 型号](https://help.kobo.com/hc/en-us/articles/17971574287895-Kobo-Libra-Colour)、[在线 User Guide](https://help.kobo.com/hc/en-us/articles/360019123773-Kobo-eReader-User-Guide)、[书内手写](https://help.kobo.com/hc/en-us/articles/1500001927562-Write-notes-in-your-book-with-Kobo-Stylus)、[笔记本](https://help.kobo.com/hc/en-us/articles/360062226733-Use-your-Kobo-eReader-as-a-notebook)、[N428 手动重置](https://help.kobo.com/hc/en-us/articles/18259230340887-Manual-reset-Kobo-Libra-Colour)、[无障碍分支](https://help.kobo.com/hc/en-us/articles/32246787707799-Kobo-eReader-Accessibility-Features-Support)。

变更说明：原案例仅有 3 个查词/批注流程。本轮保留原编号，新增 21 个任务与功能域缺口清单；深度仍是文档重建，未将范围扩大解释为实机、连续观察或完整功能验收。
