# Batch48 M-008 汉王 N10 Pro二代：本地资料盘点

检查日期：2026-10-27。范围仅为本地只读盘点；未联网、未操作设备、未执行旧抓取/构建脚本、未改 `docs/` 正式文件。

## 当前结论

- M-008 固定为中国市场 **N10 Pro二代**。当前共 **6个F，全部D1**，没有可核对固件版本的完整连续操作链、D2或实机证据。
- `F-M008-01`～`04` 由 `batches/ai-notebooks-coverage.csv` 持有；`F-M008-05`～`06` 由 `batches/child-information-deepening-coverage.csv` 持有。`hanvon-controls-deepening-coverage.csv` 只有表头，上一批用它存新增任务（本次0个），对旧F仅形成更新建议后由根合并。
- 主证据表现有M-008证据12条：E-HANV-01～04、E-HND-01～04、E-HVCD-01～02、E-HVVC-01～02。只有E-HND-02/03、E-HVVC-01/02含静态视觉观察；全部 `hands_on=false`。
- 现有53行目录是“已取得来源与缺口目录”，不是机内完整菜单。其组成在旧交接中记录为：来源索引5、文章栏目15、报告控件3、入口父组1/子入口8、功能报告4、官方能力5、旧静态控件2、条件报告3、缺口7。

## 已经读过或看过的范围

| 范围 | 本地依据 | 已确认边界 |
| --- | --- | --- |
| 官方型号身份与能力宣传 | `work/ai-notebooks-research/hanvlist.html`、`hanv799.html`、`hanv399.html/.txt` | 列表、二代产品页、2025-07-18官方文章的读写/OCR/录音转写/纪要/传输能力；产品详情图未观察，宣传仅D1。 |
| 官方下载栏目 | `work/ai-notebooks-research/hanv-downloads.html` | 旧采集HTTP200；当时未见N10/PDF链接。只能说“旧索引未命中”，不能说手册不存在。 |
| 王二狗二代评测正文 | `work/child-information-deepening/hanvon-review.html`，提取件 `work/hanvon-controls-deepening/raw/old-review.txt` | 读过阅读、全局批注、分屏、聚合网盘等正文；作者体验、固件未知。 |
| 听书设置静态图 | `work/child-information-deepening/hanvon-tts.png` | 看过声音、音量、语速、定时、开始控件；未看启动、暂停、到期、锁屏、续播。 |
| 阅读分屏静态图 | `work/child-information-deepening/hanvon-split.png` | 看过类型、窗口位置、旋转方向、取消/确定；未看第二内容选择、执行结果、退出和保存。 |
| IT手机资讯二代报告 | `work/hanvon-controls-deepening/raw/itsjzx.html/.txt` | 正文读过：中央圆键/另外八键、电源键、B6笔键、离线转写条件、微信绑定、应用安装声明；没有完整菜单、失败或恢复链。 |
| 新浪二代报告 | `work/hanvon-controls-deepening/raw/sina-review.html/.txt` | 正文读过：灵动条及首页/笔记/书架/书城/语音转写/应用/自定义键/设置入口；未证明切换后的页面/后台状态。 |
| 语音与纪要静态画面 | `work/hanvon-voice-controls-deepening/` 下两篇HTML、文本、元素表及11张已查看图 | 正式保留6张：语言/分角色/离线与转在线/暂停结束/连接等待/纪要结果及重生成分享。离散图片不是一次连续操作；3张讯飞主体图已排除，不能迁入汉王。 |

现有F：

1. `F-M008-01` 读文档后批注并分享：只确认功能名、输出PDF/图片和局部阅读工具；选择、保存、原稿/副本、撤销、再编辑、错误处理均缺。
2. `F-M008-02` 扫描PDF转适读内容：只确认OCR重排和字号/行距能力；入口、识别修订、联网、原文件保留和恢复版式均缺。
3. `F-M008-03` 录音→分角色→纪要：局部静态控件最多；仍缺资源下载、修正说话人、结束/锁屏保存、失败重试、重生成覆盖、原音定位、分享格式和重新打开。
4. `F-M008-04` 多端流转同一材料：只确认渠道及微信/网盘绑定前提；传输方向、落点、冲突、删除传播、取消/回滚、配额与备份语义均缺。
5. `F-M008-05` 阅读页准备听书：只有设置面板静态图。
6. `F-M008-06` 配置阅读分屏：只有设置弹窗和“分屏笔记”用途描述。

## 可直接复用的本地材料

### 正文与审计记录

- `docs/research/interaction-library/cases/M-008-hanvon.md`
- `docs/research/interaction-library/batches/hanvon-controls-deepening-inventory.csv`
- `docs/research/interaction-library/artifacts/hanvon-controls-deepening/source-notes.md`
- `docs/research/interaction-library/artifacts/hanvon-voice-controls-deepening/source-notes.md`
- `work/hanvon-controls-deepening/HANDOFF.md`、`captures.jsonl`、`access-failures.json`、`raw-manifest.json`
- `work/hanvon-voice-controls-deepening/captures.json`、`image-captures.json`、`image-captures-supplement.json`

### 图片

- 听书：`work/child-information-deepening/hanvon-tts.png`，SHA256 `a5e662...f97af7`。
- 分屏：`work/child-information-deepening/hanvon-split.png`，SHA256 `ce29e4...83c1d`。
- 汉王语音/纪要正式可用画面：`docs/research/interaction-library/artifacts/hanvon-voice-controls-deepening/` 下 `sina-18.jpg`、`sina-19.jpg`、`sina-21.jpg`、`sina-26.jpg`、`pconline-14.jpg`、`pconline-17.jpg`。work原图及逐图哈希在对应capture JSON。
- work还保留已看但排除/未正式采用的 `sina-17/24/25/27`、`pconline-16`，须按source-notes的归属限制使用。

### 手册与视频

- **没有已核定适用于N10 Pro二代的完整本地手册。** 正文仅提到包装内有纸质/快速入门手册。
- E-HND-04是第三方228页“N10pro说明书”公开预览，代际未核，未下载受限内容，不可用于二代流程。
- **没有已下载或已连续观察的本机视频/GIF。** 新浪正文含GIF入口，但现有记录没有观察动态过程；2024 N10恢复视频属于邻近型号，已经明确排除。

## 明确受限或失败的来源（避免重复碰撞）

- 知乎 `https://zhuanlan.zhihu.com/p/1905680180795639326`：普通匿名GET返回403，650B，已停止；失败体在 `work/hanvon-controls-deepening/raw/zhihu-compare.html`，SHA256 `29e1d2...dc61`。不要重试或绕过。
- 什么值得买 `https://post.smzdm.com/p/ak3z4588/`：HTTP200但仅1697B验证码脚本，提取正文0B；已停止，未运行TencentCaptcha或POST。失败体SHA256 `ff9c58...41b7`。不要重复验证码路径。
- 汉王官网：旧web工具路径返回405，但普通requests曾得HTTP200；本次未联网复试。后续应复用现有HTML，除非根代理有不同官方入口。
- 官方程序/驱动页：旧采集未见本机手册链接；该结果不能证明不存在。不要把同一页的无命中当新增证据。
- E-HND-04付费/受限预览：只保留排除记录，不下载受限正文。
- 邻近型号：N10二代、N10 2024、初代N10 Pro、N10 Pro三代、M10以及2024 N10恢复视频均不可直接补M-008流程。

## 尚未展开的目录

库存中明确 `not_established` 的7项：

1. 首次设置、关于设备、当前固件、完整机内手册。
2. 阅读目录、书签、页码跳转、搜索结果返回原页及阅读进度保存。
3. TTS启动/暂停、后台、锁屏、音频打断、定时到期和断点续播。
4. 任务管理、后台清理和刷新模式的入口及作用范围。
5. 笔记/批注导出副本、对象身份、再编辑、删除和撤销。
6. 备份、自动同步、跨端删除传播、账户切换及冲突处理。
7. 更新、App重装、刷机/重置后的数据范围与恢复。

此外现有报告只给名称、尚未形成任务链的目录包括：首页、书城、应用、设置、自定义键；实体键/笔键配置；应用市场/浏览器安装与卸载；AI助手如何读取当前文档、引用来源、修改要求、写回及确认成功。语音链需优先补同版本的“结束→回听→改说话人/原文→重生成→分享→重新打开”，才能判断对象身份与持久性。

## 本次命令与状态

以下命令均在 `/home/workspace/reader` 执行，exit 0：

- `pwd && rg --files | rg '(^|/)(README\\.md|AGENTS\\.md|M-008|hanvon|Hanvon|汉王|coverage|evidence|catalog|inventory)' | sed -n '1,240p'`
- Python读取指定案例/清单/证据文件大小；`rg -n 'M-008|E-HAN|F-M008|hanvon|汉王' ...`；`find work ...`（组合输出过长被工具截断，之后拆分复核）。
- Python逐行读取53行inventory，统计批次coverage持有关系和主证据表M-008条目。
- `sed`读取M-008案例及两份source-notes；`find`列本地Hanvon资料；`rg -n -i`检查失败/限制词（组合输出过长，后续直接读取HANDOFF与JSON确认）。
- Python分目录列文件；`sed -n '1,240p' work/hanvon-controls-deepening/HANDOFF.md`；`cat` captures/fetch/image日志。
- `cat work/ai-notebooks-research/fetch-report.json`；Python读取access-failures/raw/formal manifest；`sha256sum`核对案例、inventory和8张核心图片。
- `mkdir -p work/research/batch48-integration` 与本报告写入命令。

状态变化：仅新增 `work/research/batch48-integration/hanvon-inventory.md`；没有改动正式资料、代码、设备或外部状态。未知仍包括固件/应用构建、账户套餐、完整机内目录及全部实际运行结果。
