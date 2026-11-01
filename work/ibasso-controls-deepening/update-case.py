from pathlib import Path
p=Path('docs/research/interaction-library/cases/M-064-ibasso.md');s=Path('work/ibasso-controls-deepening/case-before.md').read_text()
s=s.replace('查阅2026-10-26。固定','查阅2026-10-26；补查2026-10-27。固定')
s=s.replace('中国官网最新条目为','2026-10-27中国官网页首条目为')
s=s.replace('目录中未配 F 的项目保留为已定位、未完成任务分析。','目录中未配 F 的项目保留为已定位、未完成任务分析。\n\n2026-10-27新增 [逐项台账](../batches/ibasso-controls-deepening-inventory.csv)：原目录13项（含父子项）、正文功能域41项、实际查看的静态菜单37项、中文版本声明37项及日本区域解释1项。五类合计129行有重叠，**不是129个功能完成或独立任务**。正文域以印刷页逐项关联；菜单项只表示看见入口。原PDF元数据生成/修改日均为2024-06-13、时区不同，不能充当发布日期或修订号。未取得比这份历史手册更新的完整指南。')
s=s.replace('USB复制→Mango Settings/Media Scan，图示可选内部/SD及Start；Music分类/搜索找曲，All Music的Position定位当前文件。','USB复制（历史Mac需MTP工具）→Mango Settings/Media Scan；Music左向右开分类/文件与底部搜索，关键词范围曲目/专辑/艺术家。All Music可按Title/Album/Artist/Time/Folder排序，Position定位当前文件；目录Return逐级返回。')
s=s.replace('图中有28%示意，非本轮进度测量。短时长/小文件过滤、M3U等见扫描页；空库、完成、取消、重复导入未知。 | E-IBA-01；E-IBA-02；E-IBCD-04 |','图中28%是印刷示例；短时长/小文件过滤文案冲突仍留。Time含义、ID3编辑、空结果/清搜索历史、完成/取消、重复和路径修复未知。 | E-IBA-01；E-IBA-02；E-IBCD-04；E-IBCD-01 |')
s=s.replace('原文疑点见下节；两种计时相互关系、结束动作/反馈须现版确认。 | E-IBA-02；E-IBCD-08 |','原文疑点见下节；日本MUSIN v1.04另明确Mango定时停止播放，只作区域解释。两计时关系、当前结束反馈/取消/重计仍缺。 | E-IBA-02；E-IBCD-08；E-IBDC-01 |')
s=s.replace('视图返回与切曲分别处理；Info还有加歌单/Delete入口，后者删除范围未知。','视图返回与切曲分别处理；Info Delete范围未知。Recent Play仅列50首最近曲目，年份标签只读ID3显示；均不证明时间断点、清历史或ID3编辑。')
s=s.replace('参数/开关作用域有文档；保存、重置、跨模式持久化未知。不能从参数界面推导音质收益。','GEQ五预设/自定义及10段；PEQ每设置最多6滤波器、8类型，Fc 33–16000Hz、Gain −20–20dB、Q 0.3–20。低通/高通/全通/Band/Notch类型的Gain不生效。越界反馈、保存/重置/持久化未证；不推音质收益。')
needle='## 重要修正与版本分支'
new='''## 本轮新增的应用与USB任务

以下只重建目标型号手册的 D2；没有安装、卸载、连接硬件或使用下载服务。三项均沿用旧 E-IBCD-01，新静态采集提交追加元数据，不把同一PDF重新编号。

| F 编号 | 前置、入口与动作 | 文档终点、分支与未知 |
| --- | --- | --- |
| F-M064-13 安装应用 | 印刷17给两条历史路径：打开预装 Google Play/Aptoide 检索后下载/安装；或桌面浏览器进入应用网站下载，再回桌面下载管理点击已下载应用安装。 | 文档到安装动作；图只有浏览器和空下载列表示例，无成功、授权或失败画面。当前商店可用性、登录要求、权限拒绝、取消、包兼容性和运行反馈未知。 |
| F-M064-14 卸载指定应用 | 印刷18：桌面 Settings→Apps→选择目标应用→Uninstall。静态并排图到 App info，另见 Open/Force stop、权限及存储入口。 | 文字把终点描述为删除App，图没有确认或完成状态。不能据此保证删除外部音频、歌单、账户、离线内容；预装限制、误删/取消和重装恢复未证。强停/清存储仅菜单，未造操作链。 |
| F-M064-15 为外接设备供电 | 印刷20：连接对端→下拉通知栏底部 USB 项→选连接用途→启用为所连设备充电。 | 文档描述设备开始获得供电；该页纯文字，未观察USB选项或充电反馈。关闭/拔线、低电量规则、设备兼容、电流、与文件传输/DAC并行关系未知。不能把“连接USB”当固定音频角色。 |

本轮实际查看印刷17、18、20、21、34、42（文件18、19、21、22、35、43）六页：分别为安装、卸载、供电纯文字、系统菜单、Mango侧栏、搜索页。它们不是连续操作。21页Backup/Reset仅可见入口，42页搜索历史/删除样式图标不证明清除规则；未依据通用Android补菜单或恢复流程。具体图注和散列见 [短摘记](../artifacts/ibasso-controls-deepening/source-notes.md)。

'''
s=s.replace(needle,new+needle)
anchor='另外两项静态观察纠正了此前缺口：'
insert='''2026-10-27补查日本MUSIN的2025-07-22一手公告：DX180 **v1.04** 段明确将Mango休眠计时改为“按计时停止播放”，并分列系统关机计时。iBasso经销名录将MUSIN列为日本Distributor；此为有身份依据的区域说明，不是中国站笔误的直接更正，也没有补出1.05的控制链。该稿DX340段的Mango OS不能套到DX180。中文v1.0（2024-06-26）标题与英文V1.01同日但写法不同，台账保留原名；1.02的应用固定/在线歌词/艺术家二级专辑页，以及1.05的ReplayGain/局域网/降级仍仅D1声明。[E-IBDC-01；E-IBCD-08；E-IBA-03]

'''
s=s.replace(anchor,insert+anchor)
s=s.replace('全部查于2026-10-26。官方PDF','旧材料查于2026-10-26，本轮复读及补查2026-10-27。官方PDF')
s+='''

| 本轮新来源 | 定位与取得边界 |
| --- | --- |
| E-IBDC-01 [MUSIN在PR TIMES发布的DX180 v1.04公告](https://prtimes.jp/main/html/rd/p/000000144.000139013.html) | 2025-07-22 17:00，DX180(v1.04)第1–2条；本地原HTML可核。品牌[经销名录](https://ibasso.com/where-to-buy/)仅用于确认MUSIN关系。相同机构站内新闻经web正文可读，但直接HTTP断开、无该URL本地原件；下游支持页444后停止。 |

本轮只新增1E/3F；原F01/06/09/10共14字段补证建议及E-IBCD-01/08、E-IBA-03的追加采集建议在 `work/ibasso-controls-deepening/`，由根合并旧批。未改旧批与主表。固件HTML、PDF元数据和图页原件SHA均登记；未下载APK/固件或执行外部产品动作。
'''
p.write_text(s);print('updated',p,len(s))
