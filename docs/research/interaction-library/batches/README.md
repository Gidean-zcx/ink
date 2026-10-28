# 分批取证与主索引

2026-10-27补入汉王语音/纪要静态控件及百词斩历史官方教程抽样，见[第三十一期简报](/home/workspace/reader/docs/briefs/031-sync-readiness-and-ai-results.md)。沿用旧F，0新任务；静态照片和离散摘帧不计连续观察或产品操作。字段及已实现的检查范围见[读取约定](../data-dictionary.md)。

来源 URL 保留实际成功读取的协议与地址。若官网只有 HTTP 可读、HTTPS 失败，记录 HTTP 原址及失败情况，不为了格式统一改成未经验证的 HTTPS。此约定只针对公开研究来源，不代表产品通信或账户方案。

每个主题批次由同名 `*-evidence.jsonl` 和 `*-coverage.csv` 组成。`initial-*` 保存资料库建立时的首批记录，后续批次增量加入。资料库根目录的 `evidence.jsonl`、`coverage.csv` 是这些批次按稳定 ID 合并的检索表；相同 ID 出现不同内容时必须解决冲突，不能静默覆盖。

产品案例是行为分析正文，批次索引是来源与流程登记；二者应同步修改。新证据与旧证据存在时效差异时，用 `supersedes` 或 `qualifies` 指明被修正的记录，并在案例中保留适用范围。不将一条来源的不同静态图统计为独立研究来源。

同一任务补证时沿用原 F 编号，更新其所属批次行和案例，并在核查记录说明修订原因；新证据可来自后续批次。不得仅因取得新图片、增加适用平台或补出一个入口，就再次计为新任务。草稿中的重复编号不进入主索引，编号可以不连续，后续不复用废弃草稿编号。

路径约定：`case_path`、`detailed_case`、`archived_artifact`、`rendered_artifacts` 相对资料库根目录；`raw_work_path` 相对项目根目录。`work/` 可能清理，不能作为长期归档。正式静态证据放在 `artifacts/`，记录来源、页码、取得方式与 SHA-256；多图另记 `artifact_hashes`。

`visual_observed: true` 仅表示实际查看过所标的图片或页面；静态图片对应的任务仍可仅为 D1/D2。D3 需要连续过程，D4需要可复查的实际操作记录，并明确人工设备操作、远程操作或自动化浏览器方法。来源、证据记录、流程和产品案例数量各有含义，任何一个都不能单独代表完整产品已研究。

主索引当前由 `work/research/interaction-depth/merge_batches.py` 合并，结构与引用检查由同目录 `check_docs.py` 执行。两者只是本轮资料维护脚本，未引入产品工具链；批次、案例、主索引与正式图片本身不依赖临时脚本才能阅读。

公开网页操作新增字段约定：`operation_method=automated_public_browser`、`visual_kind=browser_sequence`，`operation_audit_path`相对资料库根目录，并记录`operation_audit_sha256`。正式审计必须包含环境、逐步动作、可见前后状态、失败和未测范围，保存动作日志、按序截图及散列。`hands_on=true`在此只表示实际执行UI动作，不表示实体设备试用。D4也不等于全分支通过。

静态图证据数排除`visual_kind=browser_sequence`；同一次网页运行支持的多个任务不当多次独立验证。仅在DOM中发现控件、对隐藏控件点击超时，均不能记为已成功操作。

第32批为 `learning-games-and-completion`：6E、5新F（4D2/1D4），5旧F补14字段，3旧E追加本地复读。24幅实际网页截图；同批[18个操作/目录节点](learning-games-and-completion-inventory.csv)不当独立功能或完成率。见[简报32](../../../briefs/032-learning-games-and-completion.md)。

## 第33批：家庭音频与语音范围

`family-audio-and-voice-controls`新增29E、20D2，补4旧F的12字段和4旧E采集；没有新图片、连续视觉或产品操作。Audible原22项仅目录补正文、Lists补打开正文，Yoto两篇既有完整正文、Tonie四排错正文和同ID地区采集。三份旧目录就地更新30行，Audible增加1个新入口，不复制总目录快照。原始工具文本/本地原件、一次构建记录在work/research/batch33-*；正式短摘记见三个artifacts主题目录。

本批全库885F（791D2/78D1/16D4）、1472E/1304不同URL，全部65组仍部分覆盖。入口见[简报33](../../../briefs/033-family-audio-and-voice-controls.md)；完成正文读取不等于完整功能或设备实测。

## 第34批：内容对象与资料分发

`anki-content-and-maintenance`新增30E、3D2，补5旧F和3旧E采集；原60行Anki目录更新30行，不另建目录快照。概念与父页不计独立功能；当前实际软件构建未知，没有安装、实操或看图。worker16页及root14页的获取日志分开，误报检测与纠正记录保留。见[简报34](../../../briefs/034-content-objects-and-study-materials.md)。全库888F（794D2/78D1/16D4）、1502E/1334不同URL，65组均部分覆盖。

第36批 `renderer-and-legacy-controls-*` 仅登记1新E/7新F；9旧F及9旧E就地更新原持有批次。旧原件/摘记不覆盖，同URL用additional_captures保存本次原页/媒体和范围。Readwise本地合成样本不是产品D4；X2和墨案静态图不当连续动作。

第37批 `adult-guidance-and-system-controls` 新增59E/14D2，10旧F及1旧E就地补证。Mudita111行更新46原index_only，Khan130行更新10原index_only并补112上游标题；未复制目录或升级产品实测。见[简报37](../../../briefs/037-adult-guidance-and-system-controls.md)。

第39批 `viwoods-chinese-workflows` 新增1个中文手册E、3个D2任务；11旧F及4旧E在原持有批次追加/修订。新75行台账按中文历史原件逐页列读取状态，不与旧英文页码混合，也不当75项功能。29页图已看、19页正式摘存；无新增实际操作。资料库根目录的`reference-points.csv`是16条增量查阅示例，不纳入F/E数量。见[简报39](../../../briefs/039-ai-preview-and-transfer.md)。

第42批 `duolingo-math-interactions`：4新E、1新D4数学练习任务，旧F-M031-16/18在原批次补数学范围；12个目录/控件节点不当12个功能。27图、83状态和58条命令保存为一个浏览器证据E，含未完成输入、定位失败、422与目标未提交边界。RP新增11条，Khan本地素材复查不新造F/E。见[简报42](../../../briefs/042-math-feedback-and-research-depth.md)。

第43批 `time-sample-reader-*`：1条新D4整期阅读任务、1条新操作E、15项有状态/缺口的目录；指南补读仍由原 `news-*` 批次持有。实际操作、静态任务和未操作入口分开，见[样刊审计](../artifacts/time-sample-reader/browser-operation-audit.md)。

第44批 `boox-system-content-deepening-*` 新增25E/7D2，31篇正文复读既存官方缓存，7图归于5个来源；6篇复用原来源/目录记录。10旧F及8旧E在原持有批次补证；127行BOOX目录、137行Palma目录只更新对应已读条目，不复制完整快照。无实际设备操作。见[简报44](../../../briefs/044-system-controls-and-evidence-scope.md)。

第45批 `pocketbook-reading-media-*` 新增10D2和1组同源静态派生E，0新增独立URL；2旧F及E-PBK-01追加采集在原持有批次更新。48页正文复读、7页实看静态摘页、27原目录行更新，RP增10条。没有设备操作，见[简报45](../../../briefs/045-reading-media-and-content-readiness.md)。

第46批 `sony-system-audio-*` 新增8D2、5E（4篇对应HTML和1组同源静态页），4旧F及E-SONY-15追加采集在原持有批次更新。59页正文复读，7页实看静态页，39原目录行更新含36原仅目录；不新增目录副本。RP增10条，无新增产品操作；见[简报46](../../../briefs/046-system-audio-and-content-state.md)。

第47批 `shortcuts-data-flow-*` 新增6D2与28篇27官方正文E；6旧F在原持有CSV更新。28原目录行补正文，原114节点不变；10RP，无新增静态图或产品操作。旧26同主题E保留；详见[简报47](../../../briefs/047-ai-data-flow-and-results.md)。

第48批 `maimemo-device-controls-*` 新增统计任务1D1、1个同源静态图组E和41标题目录；5旧F在原持有CSV更新，E-MMDVC-01追加正文采集，8RP。24图已看、21归档，无设备操作；正文/配图矛盾保留。见[简报48](../../../briefs/048-learning-controls-and-statistics.md)。

第49批 `antennapod-state-controls-*` 新增1D2/14固定源码E，4旧F与7旧E原位追加；原队列目录158→162，更新12项。8RP，没有产品执行或图像。见[简报49](../../../briefs/049-selection-confirmation-and-persistence.md)。

第52批 `quizlet-material-workflows` 新增8官方帮助E，5旧F及2旧目录E在原批次补证，无新增任务；原68行目录仅更新8项，增加7RP。总览/资格/步骤分别记深度；没有图像或实际操作。见[简报52](../../../briefs/052-study-material-interactions.md)。

第53批 `quizlet-audio-and-study` 新增8E/6D2，3旧F补证；原目录更新8新正文及3旧游戏状态，原68行不变，10RP。E-QZCD-03追加目录采集；E-KHD-04追加旧页复核/三图片403的范围，无新Khan任务或图像。见[简报53](../../../briefs/053-audio-learning-and-session-state.md)。

第54批 `quizlet-media-and-sharing` 新增23E、8D2/1D1，补2旧F及E-QZCD-01/02/04采集；原目录21条更新正文，另2条linked_article指向原文章行11/28，共70行，原目录64篇数量不变。新增13RP；背景说明不增任务，全部无新视觉/实际操作。见[简报54](../../../briefs/054-media-permissions-and-study-content.md)。

第55批 `shortcuts-entry-and-organization`：44新E/9D2、10旧F更新；原46目录条目补读，欢迎/版权关联目录证据的2次范围采集，其余44为操作/概念正文。原114行不增加；新增14RP、修订RP-M043-10的预览范围。HTML用gzip保存并分别核压缩/解压/提取正文SHA，无新视觉或实操。见[简报55](../../../briefs/055-global-entry-and-execution-context.md)。
