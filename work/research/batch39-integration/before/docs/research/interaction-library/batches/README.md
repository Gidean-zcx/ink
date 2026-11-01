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
