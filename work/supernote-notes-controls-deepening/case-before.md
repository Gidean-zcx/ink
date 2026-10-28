# M-006 Supernote Manta／Nomad：笔迹组织、任务回源与分类恢复

查阅：2026-10-26。固定对象为 **A5 X2 Manta 与 A6 X2 Nomad**，英文官方 X2 用户指南 **V3.28.42**（182 页，p9 明列两款）。官方当前下载页仍指向此版，发布日志已到 **Chauvet 3.31.45，2026-10-20**，页面更新 2026-10-23；操作基线与最新变化分开记录。不用 A5 X/A6 X 的 2.x 日志替换 X2 的 3.x 版本。无实机、账号、已安装构建或 Partner App 测试；地区、账户现行套餐及云容量未核定。[E-SNCD-01、02、10]

原 F01–06 保留并补证；新增 F07–10。主要为 D2 文档重建，日志的独立功能声明只作 D1 修正。四张实际查看的官方手册摘页是静态参考，`hands_on=false`；无 D3/D4、连续过程或耗时测量。历史帮助不自动覆盖两台设备的全部年代。

## 功能目录与覆盖

已逐项登记 [完整目录 CSV](/home/workspace/reader/docs/research/interaction-library/artifacts/supernote-controls-deepening/functional-inventory.csv)：140 个书签目录项，包括章、小节与非功能性前后页，**不是 140 项已分析任务**。记录 F/E 关联和未覆盖项。4.5 模板的 PDF 书签指向 p63，纸面目录和正文实际在 p64，保留差异。只对读过操作正文的重点小节标局部分析。全部目录依据 E-SNCD-02；下表为索引，不长篇重述整册手册。

| 功能域 | 章节／页 | 已关联 F／补充 E | 缺口 |
| --- | --- | --- | --- |
| 型号、首次设置、联网／账号 | 型号 p9；第1章 p12–16 | E-SNCD-01、02 | 首次使用、验证码和网络失败仅目录 |
| 屏幕／侧栏手势、工具栏、页码栏 | 第2章 p17–35 | F06；E-SUP-07、E-SNCD-10 | 手势仅局部；3.31 工具栏未逐入口观察 |
| 语言／时间、笔、键盘、睡眠与节电 | 3.1–3.5 p36–49 | F06 版本边界 | 其余仅目录；Manta 历史设置不泛化给 Nomad |
| Drive、应用、侧栏与蓝牙 | 3.6–3.9 p50–57 | F05；E-SNCD-08、09、11 | 第三方云仅入口，授权失效／蓝牙未研究 |
| 新建、识别、命名、封面、模板、图层 | 4.1–4.6 p58–67 | F08、09；E-SNCD-05 | 模板／图层完整链未研究 |
| 标题、关键词、链接 | 4.7–4.9 p67–73 | F01、02；E-SUP-01、02、E-SNCD-06 | 改名／移动后的所有关联、删除后笔迹待核 |
| 文本框、贴纸、笔迹与页面整理 | 4.10–4.13 p74–84 | 仅目录及文本搜索版本说明 | 未补成独立任务 |
| 笔记导出／分享 | 4.14 p84 | F09；E-SNCD-05、07 | 可编辑结构往返、取消／失败／重名 |
| 文档导入、字体、对比度、缩放、裁边 | 5.1–5.5 p84–90 | 导入通道见 F10／E-SNCD-11 | 阅读交互主体仅目录 |
| 文档目录／书签、批注、高亮／导出 | 5.6–5.9 p91–96 | F03 仅摘录相关 | PDF 菜单不等同笔记导出；其余未重建 |
| Digest 创建／组织／导入／导出／评论 | 第6章 p97–101 | F03、04；E-SUP-03–05 | 回源失效、合并去重尚缺 |
| TXT、Word、校对手势 | 第7–8章 p102–111 | F09 仅识别文字出口 | 文档再编辑与回写原笔迹未核 |
| To-Do | 第9章 p112–113 | F07；E-SNCD-03 | 删除恢复、精确笔划定位／联动未知 |
| Atelier、InkFlow、InkHub、NetVirtualDisk | 第10–13章 p113–124 | 仅目录 | 附加应用版本／完整流转未研究 |
| 文件／保护、直线、星标、剪贴板 | 14.1–14.5 p124–132 | F01、10 | 锁定／跨设备保护等未补任务 |
| Browse & Access、投屏、搜索、快捷／最近 | 14.6–14.10 p133–139 | F08、10；E-SNCD-04、11 | 投屏／快捷入口、无结果反馈待查 |
| 邮件、日历 | 第15–16章 p139–155 | 仅目录 | 授权／附件／事件／离线未研究 |
| USB／OTG、唤醒、更新、备份、重置／关机 | 第17章 p156–163 | F04、10 | 未做更新／重置；其余状态转移未研究 |
| 隐私、密码保护 | 第18章 p164–170 | 仅目录 | 锁屏恢复信息属新版本变化，未重建 |
| Supernote Cloud | 第19章 p171–174 | F05、10 | 云回收站恢复入口／保留期未知 |
| 安全、疑难解答、版权 | 第20–21章 p175–182 | 仅目录 | 模板／字体／卡死／残影排错未实测 |

## 既有任务补证

### F-M006-01 建立并维护可回找的手写结构

在主图层套索笔迹设 Heading；关键词由选区识别或 More → Add Keyword 输入；一笔画星标。Navigation Window 相应页签用于回页。[E-SUP-01] 标题可以再套索，移动、缩放、复制和改样式。[E-SNCD-06] 指南 p69 还列笔长按入口；p131 说明 **Erase Star 不擦除标题内的星标**，识别开关在 Pen Preferences。[E-SNCD-02] 历史帮助称删除标记不能撤销；删除后原笔迹如何保留、新工具栏是否改变规则未观察。全局回找见 F08。

### F-M006-02 用链接连接笔记并返回

套索笔迹或 More → Insert Link，选择样式及本文件页、其他文件或网页目标；点链接跳转。再次套索或笔长按可编辑／删除，样式与目标可改，对象可移动／缩放／复制。[E-SUP-02、E-SNCD-06]

指南 p73 区分两种返回：逐步返回按钮或**屏幕上半部单指上滑**；另一按钮一次回到最初页。创建和点击限于可见主图层。[E-SNCD-02，已看静态页] 3.25.39 日志补充移到 microSD／OTG 后链接保留，不据此承诺任意改名、目标删除或云端重建都不断链。[E-SNCD-10]

### F-M006-03 把内容收成摘录

文档括选或选文字生成 Digest；按指南 p98，笔记套索 → **Recognize as Digest → Create**，也可在 Digest 手动输入。按 Categories／Sources 查看，加手写或文字评论。**官方来源有文案冲突**：2025 帮助仍写 Recognize as Text 来生成摘录，指南 p112 菜单图则同时列出 Text／Digest 两项。此处按 V3.28.42 指南记录，不把旧帮助静默改字，也未确认是历史名称还是文案错误。[E-SUP-03、E-SNCD-02]

删除分类连同摘录删除，不只是取消标签。指南 p100–101 列 PDF／TXT 导出及 TXT 导入；Digest 自己导出的 TXT 可恢复来源、分类、作者元数据，普通外部 TXT 不具有同样保证。全部评论、笔迹与回源关系能否完整往返未核实。[E-SUP-03、E-SNCD-02]

### F-M006-04 备份与恢复摘录

Settings → System → Backup & Restore → Digest；备份到 Export 后另存外部，恢复时选备份并确认。[E-SUP-04] **2024 帮助称恢复清除备份后数据；3.24.38 起改为恢复前不先清空现有 Digest**。保留历史分支，不推断无重复合并。[E-SUP-05]

p160 将 Digest 备份恢复与 **My Account → Data Sync 的 Digest／To-Do 同步**分列。该页标题包含两类数据，但正文只把 Digest 放进备份入口，不能据标题声称 To-Do 有同样本地恢复包。重复恢复、跨版本合并、取消中断未知。[E-SNCD-02，已看静态页]

### F-M006-05 两台设备接续与同步冲突

保留原建议：同云账号，结束一端编辑并同步，再在另一端同步后继续，避免同时编辑同文件；旧帮助举 A5 X 与 Nomad，是协作建议而非 Manta 实测。[E-SUP-06]

补全入口：Settings → Drive → Supernote Cloud → 选文件夹 → Save，下拉执行同步。[E-SNCD-11] Auto Sync 需账号及对应云服务，在 Drive 开开关，下拉看进度；2024 FAQ 明确 **Back to Files** 是一个触发点，不能解释成每笔实时上传。[E-SNCD-08、09] **3.31.45 日志声称可检测／处理冲突文件，但未给版本选择及合并规则**。断网重连、两端删除竞态和冲突操作仍缺。[E-SNCD-10]

### F-M006-06 定制临时工具手势

历史入口 Settings → Display & Input → Gestures → Eraser & Lasso，配置屏幕或侧栏触发；p22 描述两指按住后用笔执行，松指回先前工具。[E-SUP-07、E-SNCD-02] **左右手是旧帮助／V3.28.42 表述；3.31.45 已改为八种书写姿势**。新姿势区域、误触、手掌拒触及延迟未观察。[E-SNCD-10]

## 本轮新增任务

### F-M006-07 手写变待办，再回来源笔记

前置有手写笔记；侧栏 To-Do 管理任务。套索选字 → 更多菜单 **New Task** → 识别出的名称／到期日 → Add。p112 静态图显示这些入口，未实际触发。任务带 Note 图标，官方帮助说点它回**来源笔记**，没有保证定位原句或高亮笔划。[E-SNCD-02、03]

任务可点选后改名、换清单、改日期或排序；勾选后有删除线并进 Completed，再点可撤销完成。无日期任务不列在 Planned；Inbox／All 与日期归类不同。长按可批量移动／删除，删除恢复、是否改变原笔迹未知。[E-SNCD-03] 同步是 F04 的 Data Sync，并非目录同步；3.18.29 修过“离线移任务并删原清单，重连同步后丢任务”，仅作历史故障记录，不保证现在任何离线序列无损。[E-SNCD-02、10]

### F-M006-08 分类搜索，回到命中的内容

下拉 → Search → Handwriting／File／Keyword／Star，输入内容或按类别支持的空检索执行。Handwriting 限实时识别笔记中正确识别的文字，点结果回命中页；Keyword 回标记页。File 查文件名（可不完整），回上次所读页。Star **按 File 聚合也回上次所读页，不一定是星标页**，需区分 Page／File 结果。[E-SNCD-04；p135–137 对应类别]

识别不是打开结果页才启动。帮助要求笔记保持打开，最后一次书写／触摸后约 **30 秒**开始后台识别，新操作会暂停；这是文档规则而非实测。结果可能是 Recognizing、已识别文本或提示多写一些；Queue 看状态，Re-recognize 重做当前页。识别错误、文字过少或仍在处理可能导致漏检，不等于原笔迹丢失。[E-SNCD-05] 3.24.38 又增加文本框／文字搜索；旧四分类帮助与手册不能证明最新界面只有四类。[E-SUP-05]

### F-M006-09 将笔记导出为交换文件

笔记 Export & Share → 选页 → PNG／PDF 或 TXT／DOCX；实时识别笔记也可从 Recognition Results 文字导出。标准笔记可导出 TXT／DOCX 时识别；实时识别笔记创建时选定的识别语言不能事后修改。[E-SNCD-05、07；p59–61、84]

p84 静态图可见 Cancel、Export、QR Code、选页入口，仅证明入口可见。PNG／PDF 二维码分享与 NOTE 发往 InkHub 是另外的发布路径，未执行，也不当作本地备份。导出后再编辑能否恢复标题、星标、链接、图层和原笔迹身份关联，没有完整承诺。输出目录、同名覆盖、存储不足／取消／断电恢复仍待查。[E-SNCD-02]

### F-M006-10 删除文件前留副本，区分备份与云回收

Files 长按文件／文件夹 → 多选 → Delete 是 p126 的入口；确认文案、撤销及本地回收站未查到。文件备份可 USB-C 连电脑，找到设备后复制文件／文件夹；macOS 帮助要求 MTP 软件。也可先同步云端，再下载独立副本。恢复是把保留的文件传回设备；原位置、外链及重复文件处理未验证。[E-SNCD-02、11]

无线通道：同 Wi-Fi 打开 Browse & Access，接收端输入设备显示地址，设备确认后进入文件夹，Upload 传入、点文件下载；不能由云同步推断离线能云恢复。[E-SNCD-11] **3.31.45** 记录：经 Supernote Cloud 同步删除的对应云副本进 Recycle Bin，可在账号站点关此功能；Google Drive 同步删除也进该云回收站。[E-SNCD-10] 这不是本地撤销。实际恢复按钮、保留期、永久删除、恢复后再次同步和关闭开关的精确后果未知。

p161 明确当时不提供系统／应用数据、个人设置、已装应用等整机备份；p162 重置会清除用户加的数据与账号。只记录范围，未执行重置，不把文件副本称整机镜像。[E-SNCD-02]

## 静态观察与剩余缺口

四页从官方 PDF 渲染并实际查看，印刷页码等于 PDF 序号；一页多个示意状态也**不构成连续过程**。PDF、HTTP 原件、文本及页图哈希在本批 manifest。

| 摘页 | 实际看见 | 不能推出 |
| --- | --- | --- |
| [p73](/home/workspace/reader/docs/research/interaction-library/artifacts/supernote-controls-deepening/x2-v3.28.42-p73.png) | 两种返回按钮、上半屏上滑说明 | 目标删除／历史持久性 |
| [p84](/home/workspace/reader/docs/research/interaction-library/artifacts/supernote-controls-deepening/x2-v3.28.42-p84.png) | 导出／分享／Cancel 入口 | 实际文件、耗时、取消结果 |
| [p112](/home/workspace/reader/docs/research/interaction-library/artifacts/supernote-controls-deepening/x2-v3.28.42-p112.png) | Recognize as Text／Digest／New Task 分开，Add／日期入口 | 成功后回落页面、双向改写笔迹 |
| [p160](/home/workspace/reader/docs/research/interaction-library/artifacts/supernote-controls-deepening/x2-v3.28.42-p160.png) | 文件副本、Digest 备份、To-Do／Digest 同步分列 | 单包涵盖设置、任务删后可撤销 |

下一最小观察：同一三页笔记建标记／链接／任务，分别改标题、删标记、移动来源文件，比较笔迹及回源定位；在 V3.31.45 记录工具栏与搜索分类。另用测试文件观察两机冲突和云回收恢复。OCR 修正是否回写笔迹、TXT／DOCX 再导入、重复恢复、任务删除、账户切换及重启中断未完成。

## 对本项目的启发（推断）

G-002／R-021 下应区分笔迹、识别文本、标记、任务及来源，回执说明回文件、页还是具体内容。R-016 成人专题学习可研究任务回源，但没有证据证明自动提高学习效果。5–6 岁儿童能否理解多种标记、识别结果和任务清单需另测，本案例没验证读听。

H-004／H-005 可借鉴分类公开恢复范围，区分外部副本、删除同步与整机恢复。Plus 可以研究这些关系与状态表达，本案例不证明同等电磁笔、OCR、内存、触控或云能力，也不把候选写成需求。

## 来源

均查阅 2026-10-26；新证据的获取方式、原件哈希在批次 JSONL。未登录或执行设备动作。

- E-SUP-01：[标题、关键词、星标](https://support.supernote.com/1759244-using-titles-keywords-and-stars)，2023-11-27。
- E-SUP-02：[笔记链接](https://support.supernote.com/en_US/inserting-links-to-notebooks)，2023-09-01。
- E-SUP-03：[重构后的 Digest](https://support.supernote.com/the-revamped-digest-app)，2025-08-25；生成入口文案与当前手册不同。
- E-SUP-04：[摘录备份／恢复旧帮助](https://support.supernote.com/en_US/Whats-New/back-up-and-restore-digests)，2024-06-06。
- E-SUP-05：[Manta／Nomad 日志](https://support.supernote.com/changelog-for-manta-and-nomad)，3.24.38 恢复修订；新快照 E-SNCD-10 同源。
- E-SUP-06：[两机同步](https://support.supernote.com/en_US/faq/how-to-sync-files-between-two-devices)，2024-05-14。
- E-SUP-07：[橡皮／套索手势](https://support.supernote.com/Whats-New/customize-and-use-the-eraser-lasso-gesture)，2024-03-07。
- E-SNCD-01：[官方手册索引](https://support.supernote.com/en_US/getting-started/download-manual)，更新 2026-07-01。
- E-SNCD-02：[X2 V3.28.42 英文指南](https://ib.supernote.com/x2/X2_V3.28.42_EN.pdf)，p2–9 目录／型号，重点页见上。
- E-SNCD-03：[To-Do](https://support.supernote.com/the-to-do-app)，2024-06-26。
- E-SNCD-04：[分类全局搜索](https://support.supernote.com/en_US/organizing/global-search-using-categorization)，2023-10-20。
- E-SNCD-05：[手写识别](https://support.supernote.com/en_US/Tools-Features/handwriting-recognition)，2026-02-09；页首 2.7.21 是历史沿革，X2 适用另由手册复核。
- E-SNCD-06：[再编辑标题／链接](https://support.supernote.com/en_US/organizing/2126285-untitled-article)，2023-10-09。
- E-SNCD-07：[工具栏](https://support.supernote.com/en_US/introduction-to-the-toolbar)，2023-11-01；只用于导出格式，不代表 3.31 布局。
- E-SNCD-08：[自动同步入口](https://support.supernote.com/supernote-cloud-auto-sync?kb_language=en_US)，2024-06-05。
- E-SNCD-09：[自动同步触发点](https://support.supernote.com/en_US/faq/when-do-my-notes-get-automatically-synced-to-supernote-cloud-)，2024-01-23。
- E-SNCD-10：[Manta／Nomad 日志版本快照](https://support.supernote.com/changelog-for-manta-and-nomad)，更新 2026-10-23；3.31.45、3.25.39、3.18.29 相关段。同 E-SUP-05，不算独立交叉验证。
- E-SNCD-11：[文件传输](https://support.supernote.com/en_US/organizing/transfer-files)，2026-07-15；USB、Browse & Access、Supernote Cloud 段。
