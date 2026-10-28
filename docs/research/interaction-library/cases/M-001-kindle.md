# M-001 Kindle／Scribe：阅读导航、词汇积累与笔记生命周期

本轮查阅：**2026-10-27**；保留 2026-10-26 的旧证据。主样本固定为 **Kindle Scribe 2024 Release**：官方型号目录对应 `kindle_scribe_2024` 指南，正文自称 “1st generation, 2024 release”。旧 E-KDL-01 的 `kindle_scribe` 链接则位于该目录的 2022 Release 项下。型号名以官方写法保留，不自行重排代数。（E-KDCD-01、E-KDCD-02）

两份都是现行在线指南；URL 的 `v1` 不代表固件。2024 指南本次 HTTP Last-Modified 为 2026-10-05，这是文件响应元数据，不是各功能发布日期，更不是 2024 出厂能力。精确固件、地区、书籍权限未知。其他 Scribe 通用帮助未逐项标最低固件；**2025+ 的云盘、Quick Notes、页复制/跨本移动分支另列，不能套到 2024 主样本**。

此前9F；本轮续补6个既有任务，新增F10–20，合计20F，均为限定范围的D2文档流程。没有设备、账号登录、买书、向服务传文件、固件下载或连续视觉观察。此前仅文字；本轮实际查看11幅官方静态插图，7组派生E，与3篇新帮助分别登记；原件哈希和独立摘记见文末。静态插图不是连续流程。

## 结构化竞分（2026-10-28）

- 对象边界（导出/本地副本/云条目/阅读点/音频点）：
  - 导出：Share 可生成“带可搜索文本的 PDF”与“Convert to text”两种不同产物；预览中的校订不写回可搜索 PDF。链接有效期与再次导出命名/覆盖未证（F‑M001‑05；E‑KDL‑05/E‑KDCD‑10）。
  - 本地副本 vs 云条目：Remove download 仅清设备缓存、留云端条目；Permanently delete 影响账户各端且不能恢复 Notebook（F‑M001‑06；E‑KDCD‑03/11）。
  - 云连接：Cloud Drive Import 为“建立副本”而非双向同步；断开连接不删除已导入 Kindle 云库中的对象（F‑M001‑07；E‑KDCD‑12/10）。
  - 阅读位置：Page Flip 预览与 Back to page 的临时历史不等于最远阅读点或跨端进度（F‑M001‑02；E‑KDCD‑02）。
  - 音频位置：音频书签/片段与文字阅读进度各自保存，读听切换/跨端同步未证一致（F‑M001‑12/02；E‑KDAF‑09/E‑KDCD‑02）。
- 关键任务流（样例）：
  - “写→导出”：在 Notebook 写入 → Share 选可搜索 PDF 或 Convert to text → 邮件/链接接收；两者为不同对象。
  - “清空间→保条目”：封面菜单 Remove download 清本机，再次联网可重下；不影响账户条目与他端。
  - “预览→回位”：Page Flip 查找另一处内容 → Back to page 回阅读点；与最远进度/跨端同步独立。
- 失败与恢复（文档证据）：
  - 未同步/空间不足：Notebook 低于100MB不可保存新对象；Sync 关闭时不云备份且不可云删除（F‑M001‑04/06）。
  - 云连接异常：不兼容/超限文件灰显；重复导入建额外副本，不覆盖旧文件（F‑M001‑07）。
- 对 E‑Ink4 Plus 与 5–6 岁+成人家庭的启发：
  - 把“已备份/已导出/已删除”拆成不同对象与结果；把“返回原位”与“跨端进度”区分；音频与文字的“继续”不是同一个锚点。

## 功能目录与覆盖

[逐目录覆盖表](../batches/kindle-controls-deepening-inventory.csv)登记 **56 个指南目录项及 2 个相关子项**，其中 56 项含 9 个一级父标题、47 个二级子项，另 2 项是正文子项；不能按数量解读为独立功能或完成率。均有原章节锚点，目录登记不升级为已完成任务。主目录较完整，但不等于穷尽每个菜单/固件差异。

| 功能域 | 关联范围 | 仍缺什么 |
| --- | --- | --- |
| 书库、合集、下载与移除 | 2024 指南、合集下载专页；F09 | 误删确认/撤销、合集同步冲突、下载失败 |
| 阅读导航与学词 | 2024 指南；F01、F02 | 误标掌握、复习安排、跨会话返回栈 |
| 批注、画布与PDF | 通用手写及兼容表；F03 | 最低固件、内容更新后笔迹定位、整画布误删 |
| 笔记页与文件夹 | 创建/移动/多页帮助；F04 | 空间不足实际提示、删除页恢复、跨本冲突 |
| 导出、同步、恢复 | 分享/同步/故障/删除帮助；F05、F06 | 未同步笔迹保护、批注跨端范围、重复导出 |
| 云盘、首页随手记 | 2025+ 帮助；F07、F08 | 2024 不借用此链；授权和离线编辑细节 |
| 笔记AI与页边栏 | F10及F03续补 | AI地区/语言冲突，断网/恢复及实际布局未知；不倒推到出厂 |
| 音频、蓝牙、无障碍、家庭、浏览器、系统设置 | F11–18及F06续补，正文与选图 | 核心文档链已补；播放打断/睡眠恢复、儿童档案冲突及实际动作仍缺 |
| 购书、借阅、家庭分发 | 指南部分章节可作下一来源 | 本轮未建立购买/借阅/家长退出全链 |

## 任务流程

| F 编号 | 前置、动作与反馈 | 状态、分支与缺口 | 证据 |
| --- | --- | --- | --- |
| F-M001-01 查词再复习 | 长按正文词 → 阅读菜单 Vocabulary Builder → Words/Books → Flashcards → 看原句、See Definition → Exit Flashcards 返回。 | Learning 有学习/掌握统计，但指南未给标记掌握或撤销按钮；不能把翻过卡片当掌握。关闭自动收词后新查询不入本；Word Wise 提示密度可调且受书籍/语言支持限制。复习调度未知。 | E-KDL-01、E-KDCD-01、E-KDCD-02 |
| F-M001-02 临时翻找再回原处 | 点阅读顶端或底部上划 → Page Flip，以缩略图/进度条预览 → Back to page/loc 回原阅读点；点预览页才跳转。 | 位置历史箭头在退出书籍后清空；预览、阅读点和持久进度分开。断电及跨会话返回未知。 | E-KDL-01、E-KDCD-02 |
| F-M001-03 在书中写批注 | 选 Active Canvas 工具 → 行间长按插入，或直接落笔后确认/取消；指针选框可改大小，垃圾桶删除后还需确认。便签 X 关闭自动保存，点图标可重开。 | 工具有 Undo/Redo、隐藏/重显；笔迹撤销不证明整画布删除可恢复。USB PDF、2022-11-11 前已传入库的 PDF 不支持该手写路径；漫画/儿童 EPUB 也不能泛化。画布语言方向有限制，固件与锚点恢复未知。 | E-KDL-02、E-KDL-03、E-KDCD-08、E-KDCD-09 |
| F-M001-04 创建并整理笔记页与文件夹 | Notebook 页 + 新建；整本封面 More → Move 选文件夹或 Notebook 顶层。顶栏多页图标进入概览 → Select/长按 → 勾选 → Add/Delete/Move/Share；Move 点页间位置，X 退出多选。 | 至少需 100MB 空闲才能保存新对象及笔迹，笔记关闭时自动保存。书与个人文档不能放进笔记文件夹。Add 在每个选中页后加页；**2025+** 才另列 Duplicate 与跨本移页。空间不足提示、删页确认/恢复未知。 | E-KDL-04、E-KDCD-05、E-KDCD-06、E-KDCD-07 |
| F-M001-05 导出与转换 | 打开笔记/文档 → 顶栏 Share → 注册邮箱快发或指定邮箱；可选 Convert to text。指定邮箱勾选附带可搜索 PDF，快发则另给该 PDF 链接。 | 便签附在导出文末，页内笔迹留原处。链接 7 天有效；转换预览中的校订**不进入可搜索手写 PDF**，二者是不同产物。不支持一次分享多本，Family 不自动共享这些个人笔记。发送失败重试、再次导出命名/覆盖未知。 | E-KDL-05、E-KDCD-10 |
| F-M001-06 同步并排查缺失 | 联网 → Quick Actions → Sync；缺失先核同账号其他设备删除。模板异常可重选、关闭重开，仍异常时移除本地再下载。封面 More 区分 Remove download / Permanently delete。 | 联网且开 Sync 才云备份；关闭时不能云下载/备份/永久删除。移除下载留下默认封面供重下载；永久删除影响所有设备且**不能恢复笔记本**。不能多选整本删除，与多选页面不同。已用Send to Alexa发送的附件副本仍留Alexa，需另行查看/删除；不等于所有外部副本一起清空。未同步笔迹保护及文件夹子对象删除范围未知。 | E-KDL-06、E-KDL-07、E-KDCD-03、E-KDCD-04、E-KDCD-11 |
| F-M001-07 从云盘导入再带回（2025+ Scribe） | Connections → Add connections → 手机扫码用同一 Amazon 账号连接；云盘文件 Import，长按可定制，历史图标查进度；文档/笔记 Share 回云盘。 | 不兼容/超限文件灰显；重复导入建额外副本，不改旧文件/笔记；密码文件只读。导出是 PDF 副本，不是双向同步；USB 文件不能走回传链。断开/重置/注销移除连接，已导入 Kindle 云库的文件保留。重复导出是否覆盖、授权失败恢复未知。 | E-KDL-08、E-KDCD-10、E-KDCD-12 |
| F-M001-08 首页随手记（2025+ Scribe） | 注册联网后，首页 Quick Notes 显示末次页；双箭头改展示大小，+ 在末尾加页，Move 可跨本移动/复制页面。Workspace 可找到同一笔记本。 | 这是既有 F08 的正文补证，不新增重复任务。未说明每次离线都不能编辑；离线冲突、末次页规则、删页撤销仍未知。 | E-KDL-09、E-KDCD-13 |
| F-M001-09 整理合集并下载或移除书库内容 | 2024 指南：Library → Create New Collection → 命名 → 勾选项目 → Done；合集菜单增删成员，Back/Home 退出。合集视图菜单另选 Download all items。 | 合集结构同步不等于内容下载。删除合集不删书；Remove download 保留有云副本的内容，USB 文件无此恢复。永久删除云端书需重新取得。确认/撤销与部分下载失败未知。 | E-KDCD-02、E-KDCD-14 |

## 条件、恢复和来源差异

| 问题 | 本轮依据与边界 |
| --- | --- |
| 主型号与新代分支 | 分享帮助末尾明确把云盘连接/分享限定 2025+；F07 据此收紧范围。Quick Notes 和多页帮助也分别写明年份。无需凭界面上的 Workspace 字样猜代际。（E-KDCD-07、10、13） |
| 云盘重复和断开 | F07 的“重新出现 Import”不代表此前失败。云盘 FAQ 区分导入副本、授权连接及 Kindle 云库；断开不等于删除已导入资料。账号不匹配需在外部浏览器改用设备注册账号重新连接，当前未实际授权。（E-KDCD-12） |
| 永久删除与移除下载 | F06 的笔记本删除结论来自专页；“默认封面仍在”是重下载入口，不是已保存一份本地内容。单页、整本、文件夹、云端书及合集不可合成一个 Delete 动作。（E-KDCD-03，本项目对象区分为分析） |
| 已删笔记仍出现 | 排查帮助建议刷新 Notebook 视图/重开/同步，也列再删后同步；本轮未执行。先分辨陈旧列表、其他设备已删及真实未同步数据，不能把所有缺失都当作能重下载。（E-KDCD-04、11） |
| 跨端手写 | 兼容表给部分 Android/iOS 的笔迹能力，Sync 帮助仍泛称手写只在 Scribe 间访问；保留内容/客户端/更新范围差异，不强行合并。（E-KDCD-09、11） |
| 下载筛选措辞 | 2024 指南的存储章节对 Downloaded 选中/取消有内部不一致；同段其他操作不能消除这处冲突。F09 不据此编出精确筛选状态。（E-KDCD-02） |
| 保存与导出 | 空间条件、Sync 备份、邮件生成、文字校订、可搜索 PDF 是不同状态；邮件链接过期不等于原笔记被删。（E-KDCD-05、10、11） |

## 本项目启发（推断）

对 G-002 / R-017，自动导入不能只回报“文件到了”：还要知道输入渠道、是否有云副本、能否书写、是否在同步中、权限和导出对象。书库合集与笔记文件夹的不同作用域，是 AI 需要明确的对象关系，不能只给一个通用移动接口。

对 G-001，查词、翻卡、自评与掌握应分别记录；指南缺具体掌握/撤销动作，就不替它补一个。对 R-018，预览位置、历史栈、当前阅读点、最远位置也不能压为单一进度。导出中可搜索 PDF 与校订后的文字分离，则提示 AI 应报告它究竟修改了哪一份产物。

家庭 5–6 岁读听可借鉴有边界的任务、恢复与笔迹反馈；本轮补家长PIN但有Scribe儿童档案排除冲突；完整家长退出及离线权限仍缺，也没有证明 Kindle 云服务能在 Plus 本地执行。

## 后续取证

现已取得型号化完整指南和互补官方帮助，音频/家庭/无障碍/系统与AI工具本轮已补正文任务；实际打断恢复、地区与型号冲突仍须验证。需要获授权设备或带构建的官方演示验证：同 PDF 经 USB/Send to Kindle 的工具差异，查词后误标与恢复，跨端笔迹冲突，空间不足时未保存内容，文件夹删除连带范围及下载失败回退。当前未操作，也不宣告全功能完成。

## 来源

旧 E-KDL-01–09 保留于原 reader 批次：2022 指南、通用手写/兼容/多页/分享/同步/排查、云盘、Quick Notes。原查阅日 2026-10-26；F07 年份范围已由本轮补证收紧，旧覆盖字段已由根代理核对原值并更新。

此前以下均为2026-10-27成功读取的官方文字；E-KDCD-01 是型号索引，E-KDCD-02 是用户指南，其余为帮助正文。帮助未给可靠更新日/最低固件；正式证据登记原件哈希，无图片观察。

- E-KDCD-01：[型号与指南对应目录](https://digprjsurvey.amazon.com/csad/help/node/G7N7RPHV2SW8CKBW)，Kindle Scribe - 2022 Release / Kindle Scribe - 2024 Release / 2025+ entries。
- E-KDCD-02：[Scribe 2024在线用户指南](https://d1ergij2b6wmg5.cloudfront.net/kug/kindle_scribe_2024/v1/en-US/html/kug.html)，TOC 56 entries; #h2_20 / #h2_23 / #h2_28 / #h2_48–50。
- E-KDCD-03：[删除笔记本与文件夹](https://digprjsurvey.amazon.com/csad/help/node/TyR30Qzqu0hGzFbFVx?theme=light)，Remove download / Permanently delete。
- E-KDCD-04：[笔记缺失与模板排查](https://digprjsurvey.amazon.com/csad/help/node/TyNFaxgPlqhHz1d4B4?theme=light)，Latest updates / Template / Page numbers。
- E-KDCD-05：[创建笔记本和文件夹](https://digprjsurvey.amazon.com/csad/help/node/TTkdXCk1A3rZ68MXt7)，Notebook icon / + / Storage requirement。
- E-KDCD-06：[移动笔记本](https://digprjsurvey.amazon.com/csad/help/node/TlI0Gva20JDq8Yfc4P)，Three-dot menu / Move / Notebook tab。
- E-KDCD-07：[多页整理与退出](https://digprjsurvey.amazon.com/csad/help/node/Tq6OTco5USdWqZ91ZH)，Select / Add / Move / Share / 2025+ / X。
- E-KDCD-08：[书中画布、便签和工具](https://digprjsurvey.amazon.com/csad/help/node/T0MT2p2D8t046zUxih)，Active canvas / Sticky note / Hide toolbar。
- E-KDCD-09：[个人文档笔记兼容表](https://digprjsurvey.amazon.com/csad/help/node/TNtSm13k3txFI4EvDV)，Opening note / Table 1。
- E-KDCD-10：[邮件导出与2025+连接条件](https://digprjsurvey.amazon.com/csad/help/node/TJE2UYmdw0ppUuR3Rs)，Share / Convert to text / 2025+ links。
- E-KDCD-11：[同步与云备份条件](https://digprjsurvey.amazon.com/csad/help/node/GDCAMDFMC2LZP6BR)，Sync / Sync off。
- E-KDCD-12：[云盘连接生命周期](https://digprjsurvey.amazon.com/csad/help/node/TIh9JOMGAKr7bY4zqu)，FAQ 4–5 / 8–15 / 18–22。
- E-KDCD-13：[2025+首页随手记](https://digprjsurvey.amazon.com/csad/help/node/T54Isf2DDrox4TckQR)，Requirements / Quick notes / Workspace。
- E-KDCD-14：[合集内项目下载](https://digprjsurvey.amazon.com/csad/help/node/GQNG5LCNAE6JYM45?searchNodes=GDRLC59WEFZTBPU7&theme=light)，Library / Sort By / Collections / Download all items。


## 本轮续补：声音、控制范围与恢复

[独立来源与逐图观察](../artifacts/kindle-audio-family-deepening/source-notes.md)保存具体定位、冲突和11幅原图；[控件目录](../batches/kindle-audio-family-deepening-inventory.csv)逐项反查任务。旧58项目录稳定编号回写，未读项保持未读。没有实机或音频测试。

| 新任务 | 已知动作与状态 | 未闭合部分 | 证据 |
| --- | --- | --- | --- |
| F-M001-10 笔记AI：选页、生成预览、插入或放弃 | 联网→AI摘要/整理→当前/全本/多选页→Start→加载→预览定制→插入首/末；开始前Cancel、预览X确认放弃；摘要≤25词失败、摘要15页/整理10页限制 | 地区/语言文档冲突；断网/超时/重试/重复插入/已插入恢复、正确性与最低固件未知 | E-KDCD-02、E-KDAF-01 |
| F-M001-11 Audible：下载、配对、播放及读听切换 | 书库下载→可播进度→Play配蓝牙；Rescan等排障；已下载区拖动、±30秒、章节选取即播、速度/输出切换；取消下载；匹配电子书回同位置 | 5%可播不等于整书离线；地区/内容权益；睡眠/断连/AI插话恢复、输出失败和匹配误差未测 | E-KDCD-02、E-KDAF-09 |
| F-M001-12 有声书位置加书签、附注及回听 | Add Bookmark保存当前音频位置→Add Note；书签列表选择即播，菜单Edit/Delete | 音频书签不等于文字锚点；删除确认/撤销、跨端同步、进度覆盖和离线恢复未知 | E-KDCD-02、E-KDAF-09 |
| F-M001-13 辅助TTS：启用、读听及导航打断 | 蓝牙/支持书籍→Aa More开启→底部播放/速度/退30秒；导航或批注自动暂停；开关持续开启直到关闭 | 语言/Enhanced Typesetting/来源有限；关菜单自动续播、中文、USB文档、睡眠/重启续读未证 | E-KDCD-02、E-KDAF-03、E-KDAF-10 |
| F-M001-14 VoiceView：配对、读屏教程与切换 | 首次开机等待/蓝牙配对/三按电源→最终声音反馈和教程；无声查配对；后续Accessibility切换、语速/音量/教程Exit | 读屏不是语音命令；具体手势/焦点回退/盲操作失败/读书断点未知；文档6秒未实测 | E-KDCD-02 |
| F-M001-15 设置家长限制并区分儿童档案 | 下拉→Settings→Your/My account→Parental control→设置并保存PIN；限制浏览器/商店/云内容/Goodreads | 指南家庭建档与英国专页Scribe排除冲突；不建立Scribe儿童建档成功链；限制逐项确认/解除失败未知 | E-KDCD-02、E-KDAF-02 |
| F-M001-16 浏览网页正文、书签与文件入口 | More→Browser→网址/链接，Back/History；Article Mode正文列；Bookmark this Page，列表勾选Remove；下载需确认；清历史/Cookies独立 | 网络失败、登录适配、重复文件、下载中断、删除撤销及跨会话位置未知；格式仅指南列项 | E-KDCD-02 |
| F-M001-17 阅读排版、主题保存与进度显示 | Aa字体/字号/粗细/布局；横屏才双栏；主题保存/管理；More选择位置/页/预计时间/隐藏；PDF专属对比和裁边 | 本机各书生效与系统UI字号分开；主题删除确认/撤销及跨设备未证；预计时间不等于理解进度 | E-KDCD-02、E-KDAF-06、E-KDAF-07 |
| F-M001-18 下拉快捷设置、全局显示与联网 | 下拉Quick Actions→飞行/亮度/冷暖/Sync/Settings；飞行关闭Wi-Fi和蓝牙；冷暖自动/手动日程；网络重扫/选网/密码/连接 | 只看到顶栏，未看快捷面板；系统全局与阅读排版分开；网络密码云保存删除不等于忘记本地网络；休眠/错误回退未知 | E-KDCD-02、E-KDAF-04 |
| F-M001-19 按内容类型放大阅读与返回 | 部分儿童书Text Pop-Up滑动换段、双击正常；漫画Panel View/部分先启用；图像放大、表格平移缩放与X返回 | 支持书籍各异；漫画退出/跨页状态及误触恢复未全列；点击区示意不证明这些模式实操或60fps | E-KDCD-02、E-KDAF-05 |
| F-M001-20 查看人物线索与前情回顾 | 长按人物/地点→X-Ray，时间轴超过最远阅读默认隐藏，可显示全部；库长按/书内菜单Recaps，系列前情与Story So Far分开 | 地区/英文/书籍资格；无条目不等于故障；剧透边界、回顾更新、返回位置、来源和AI生成方式未验证 | E-KDCD-02 |

既有任务的修订如下，保留原F编号：

| 任务 | 补充/修订 | 仍缺 |
| --- | --- | --- |
| F-M001-01 | 查词自动入本与关闭；词义卡/例句/Flashcards；Word Wise提示密度/多义词反馈及中英文提示独立 | 误标掌握/撤销与复习调度未知；中文提示不证明中文TTS或教学适龄 |
| F-M001-02 | 预览/点页跳转/Back原位；Go To历史退出书后清；脚注预览与跳到注释分开 | 静态三页与返回按钮；退出/断电/跨端历史恢复未知，不当连续视觉链 |
| F-M001-03 | 画布确认/取消及整块删除；便签X保存；页边栏宽/停靠/覆盖，关后便签显示；笔迹Undo/Redo与剪贴板分别 | 工具条静态非笔迹执行；整体删除/内容更新锚点与跨端恢复未知；页边栏关闭不等于删除 |
| F-M001-04 | 100MB与关闭保存；概览单点选择/再点打开，Back回原页，多选X回概览；模板选页/全本；2025+复制/跨本移页另列 | 空间不足实际提示、删除页确认/恢复、模板更换数据保护未知；不把2025+套2024 |
| F-M001-06 | 联网Sync云备份；移除下载/永久删除对象不同；Alexa附件单独副本；重启保内容，出厂重置清本地与设置后重注册 | 未同步笔迹、USB文件备份、跨端冲突及整机恢复未知；云下载不等于恢复本地全部状态 |
| F-M001-09 | 合集结构/下载分开，删合集不删书；音频筛选不删除；配套书音频合一条目，文档泛称删书连音频；Quick Archive/Manual Removal | 合并音频删除层级/确认/撤销未知；USB无云副本；合集冲突、下载失败与Downloaded措辞冲突保留 |

### 不能消除的来源差异

指南与英国帮助对Scribe儿童档案相冲突；F15只重建家长PIN，不推演儿童独立档案已经可用。AI地区/法语变体名单也不一致。文字中的Page Refresh设置路径前后不同。上述均保留来源范围，不能用插图存在就解决版本差异。

本项目启发仍是建议：AI需要识别当前对象、动作结果与恢复锚点。便签的X会保存，AI生成预览的X会放弃；音频书签点击会播放，翻页预览只是临时位置；同步云书、移除下载和清除本地设置不能被一个“已备份”概括。儿童是否能独立完成这些任务、中文听读质量及Plus可实现性另行验证。

新增文字来源：[AI笔记帮助](https://digprjsurvey.amazon.com/csad/help/node/TZNl60K13ehpn0s1O0)（E-KDAF-01）、[儿童档案与Scribe排除](https://digprjsurvey.amazon.co.uk/csad/help/node/G27XUUGLQ5TG84XC)（E-KDAF-02）、[辅助朗读帮助](https://digprjsurvey.amazon.co.uk/csad/help/node/TdHCSXXqsdhaVKt3Tc)（E-KDAF-03）。E-KDAF-04–10为指南插图派生证据，具体URL、哈希与观察均在独立摘记；同指南E-KDCD-02追加本次快照，不新增重复来源。
