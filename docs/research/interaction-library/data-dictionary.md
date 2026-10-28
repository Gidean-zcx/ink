# 资料字段与读取约定

2026-10-27。这里说明现有资料怎样读取、哪些约束已经检查；不是本产品的数据模型。配合[记录模板](template.md)、[批次约定](batches/README.md)和[设计参考入口](design-reference-guide.md)使用。

## 先分清四种记录

| 记录 | 稳定标识 | 说明 |
| --- | --- | --- |
| 产品或系列 | M 编号 | catalog.csv 中的研究对象；系列仍须核具体型号/平台，不能当一台设备。 |
| 功能/帮助目录项 | entry_id | 来源中发现的入口，可能包含父子项、重复语言与历史快照；不等于独立功能或已分析任务。 |
| 用户任务 | F 编号 | coverage.csv 的任务及已知分支；相同任务补证更新原编号。 |
| 来源证据 | E 编号 | evidence.jsonl 的来源、实际取得方式、范围与短摘；同一来源复读追加采集。 |

JSONL 每行一个对象；CSV 使用 UTF-8，多值编号以分号分隔。空值或 JSON null 表示未知/未取得，不能解释为竞品不支持。机器可读主表由批次合并，正文案例用于解释范围与冲突。

## 任务表 coverage.csv：当前全部 11 列

| 字段 | 类型/当前取值 | 含义 |
| --- | --- | --- |
| flow_id、product_id | 字符串 | 唯一任务与所属产品；必须能回到案例和目录。 |
| task | 字符串 | 用户要完成的任务，不能只写卖点。 |
| depth | D1 / D2 / D4；D3 为尚未取得的研究等级 | D1 功能发现或核心链不足；D2 有依据重建任务；D3 要连续视觉链；D4 要可复查的实际操作。并非完成率。 |
| evidence_ids | 分号分隔 E 编号 | 支持该任务的材料；引用存在不代表所有分支均被支持。 |
| known_branches、gaps | 字符串 | 已有依据及明确未知；型号、版本和局部升级范围不得省略。 |
| visual_status | 见下方约束 | 描述看到了什么，独立于任务深度。 |
| hands_on | CSV 字符串 true / false | 是否实际执行过产品 UI 动作；不能译作“人类亲自试用实体设备”。 |
| case_path | 相对资料库的路径 | 对应逐产品案例。 |
| checked_on | YYYY-MM-DD | 研究查阅日，不能替代软件版本或来源发表日。 |

当前非操作任务的 visual_status 为 not_observed、static_reference_only、static_official_images_only 或 static_manual_pages_only。当前实际操作仅有 automated_public_browser：任务必须同时为 D4、hands_on=true、visual_status=observed_browser_sequence，并关联包含本 F 编号的操作审计。手册看图、观看演示、拖动帮助视频的进度条都不属于产品 UI 操作。

## 证据表 evidence.jsonl

以下 17 个字段存在于当前每条记录。描述型字段保留来源细节，不能把它们误当统一枚举或自动评级依据。

| 字段 | 类型/含义 |
| --- | --- |
| evidence_id、product_id、title、url | 字符串；稳定编号、所属对象、标题、实际来源 HTTP(S) 地址。 |
| source_type | 字符串；材料性质，如官方操作帮助、宣传、源码或历史作者体验。现有值较细，尚未统一为受控词表。 |
| access_mode | 字符串；此次怎样取得，如搜索索引、HTML 正文、本地复读或浏览器操作。HTTP 200 本身不证明取得正文。 |
| checked_on、source_updated_on | 查阅日；来源日期或 null。索引显示日期、正文日期和体验日期须区分。 |
| version_scope、app_build | 适用型号/平台/版本范围；构建号或 null。不从文章日期推断构建。 |
| locator、note | 章节、页码、时间点或文本范围及必要短摘；保留冲突和不能支持的结论。 |
| hands_on、visual_observed | JSON 布尔值；实际 UI 动作与实际看图分别标记，下载图片不等于看过。 |
| archived_artifact、sha256 | 正式归档文件的相对路径及散列，或 null；散列证明文件一致，不证明内容正确。 |
| supersedes | 被明确取代的 E 编号或 null；后来的材料不自动取代旧版本事实。 |

条件字段：

- derived_from 说明派生材料的原 E；artifact_hashes 为多文件散列映射。派生摘帧不是独立来源交叉验证。
- additional_captures 追加原来源的复读或新快照，保留原取得方式；不得静默把最初只读索引改成最初已读正文。
- 如 `raw_encoding=gzip`，`raw_sha256` 对应实际保存的压缩文件，`decompressed_sha256` 对应解压后的HTTP原文；`extracted_work_path` / `extracted_sha256` 对应提取正文。核正文身份应解压并检查目标ID/版本，不能只核文件存在。
- 同一指南的欢迎/版权等非操作附页，可作为目录范围的附加采集；须保留各自 `source_url` 和 `role`，不当主页面正文或独立功能证据。
- raw_work_path 等临时原件路径相对项目根目录。work 可清理，不充当永久归档；正式短摘记、必要图像和审计在 artifacts。
- visual_kind 当前有多种静态描述值，未统一枚举；browser_sequence 专用于有操作审计的网页序列，sampled_video_frames 仍是离散抽帧。visual_observed=true 需要正式原件及散列，或独立 visual_audit_path / visual_audit_sha256。
- 当前 hands_on=true 的 E 必须有 operation_method=automated_public_browser、visual_kind=browser_sequence、visual_observed=true、operation_audit_path 及 operation_audit_sha256，并归档 observed-states.json 和 trace-actions.json。方法字段将来扩展到实体设备/远程操作时，应先扩展审计约定，不能借现有网页记录冒充。

## 已有检查与人工判断的边界

### 交互依据表 reference-points.csv（增量入口）

这是既有任务/证据的查阅层，以reference-points.csv当前行为准，属于增量依据，不是全部任务或完整状态机。`reference_id`（RP）稳定定位；`product_id`、`flow_ids`、`evidence_ids`回指M/F/E；`question_tags`用分号分隔问题标签。`source_locator`给具体PDF文件页或操作状态号，`artifact_path`相对资料库并直达原图/审计。

`precondition`、`action`、`visible_result`分别记录前置、动作和有依据的结果；`evidence_basis`是自由描述，包含手册示意、官方正文、静态图、固定源码及网页操作等，**不是限定四值的枚举**。保留历史原值，不能凭某个词自动评级。文字预期结果与静态画面中的结果均不能解读成实际成功执行。`persistence_result`独立说明是否有保存/恢复证据，`unknown_branches`保留取消/断网/重启等未验证条件；`version_scope`与`checked_on`保留适用范围和查阅时间。

原15列之后增加7列，合并任务/证据主表时自动回填，不能手工把其中false改成true：

| 派生字段 | 怎样筛选与解释 |
| --- | --- |
| task_depths、task_hands_on | 按F编号保存深度与布尔值的JSON对象；可筛含D2/D4的行。一个F可能有已操作与未操作分支，不能因此把本RP所引文档升级。 |
| source_types、access_modes | 按E编号保存来源性质、最初取得方式；沿用E的自由描述，不是自动质量评分。复读历史仍看E.additional_captures。 |
| evidence_hands_on、operation_methods | 按E编号保存是否实际操作及方法；not_recorded表示无方法记录，不能猜测为实机。判断本行的依据须优先看所引E，而非仅看F的最高深度。 |
| operation_audit_paths | 所引E中实际操作审计的去重路径，相对资料库；none_for_cited_evidence表示该行所引E没有实际操作审计。原artifact_path仍保留图片或摘记入口，两者用途不同。 |

例如筛`evidence_hands_on`包含true，再打开`operation_audit_paths`查看环境、动作与失败；筛文档或源码时仍核版本、原页/行号和未知分支。工具命令为`python3 work/research/interaction-depth/merge_batches.py`；结构检查不证明产品行为。

标签用于筛选，不保证全库标签穷尽。RP可多对一关联F/E；不加入F/E数量，不自动改变D等级。此表本轮另做ID、F/E、版本/方式和附件检查，现有通用检查器不判断这些文字是否支持行为结论；根代理仍需核对源页。

### 结构检查的能力

现有文档检查核 ID、引用、本地文件、归档散列、任务与案例一致性、主表与批次一致性，以及上述网页操作组合。它尚不自动判断来源质量、正文是否足够支持 D2、型号是否错配或任务是否完整；source_type/access_mode 的自由描述也未做统一枚举校验。检查通过只能说明这些结构约束通过。

因此取用顺序始终是：定位 F → 读 E 的原址与具体范围 → 看相应摘记/画面或操作审计 → 阅读 gaps → 决定哪些结论可引用。仅有宣传宣称不能支持详细任务；即便有官方步骤，也不等于已在设备上验证结果。

### 合成样本与产品实际操作

`local_fixture_run=true`表示运行公开组件及明确合成输入，`local_fixture_audit_path`和哈希指向范围/命令/结果记录；不意味着登录或操作真实产品，不能因此把`hands_on`置true或升D4。`visual_kind=synthetic_fixture_static_frames`表示本地样本代表图，计入“含静态图的证据记录”时也必须保留此类型，不能当产品界面实测。E-RWGR-01为首个实例；F-M015-33仍D2。
