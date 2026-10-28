# M-050 DragonBox Numbers 现有证据盘点

盘点日期：2026-10-27。范围仅限仓库现有材料；未联网、未安装、未执行采集脚本，也未修改正式资料。结论为：现有 8 个流程共 **D2 5 项、D1 3 项**，全部 `hands_on=false`；玩法结构已有静态手册、商店声明和历史作者体验支持，但没有当前构建的完整连续操作证据。

## 已核文件

- 协作与项目边界：`AGENTS.md`、`README.md`、`docs/requirements.md`。
- 案例及总表：`docs/research/interaction-library/cases/M-050-dragonbox.md`、`coverage.csv`、`evidence.jsonl`。
- 两份目录：`batches/dragonbox-controls-deepening-inventory.csv`（27 行）、`batches/dragonbox-gameplay-deepening-inventory.csv`（7 行）。
- 批次 coverage/evidence：`dragonbox-controls-deepening-coverage.csv`、`dragonbox-controls-deepening-evidence.jsonl`、`dragonbox-gameplay-deepening-evidence.jsonl`。
- 正式来源摘记与采集记录：`artifacts/dragonbox-controls-deepening/source-notes.md`、`artifacts/dragonbox-gameplay-deepening/{source-notes.md,acquisition.json,access-block.png}`。
- 工作原件与交接：`work/dragonbox-controls-deepening/`、`work/dragonbox-gameplay-deepening/`、`work/children-interactions/`、`work/children-deepening/` 中 DragonBox 文件。

## 各玩法／任务证据深度

| 流程 | 深度 | 已有证据 | 可确认到哪里 | 关键未知 |
| --- | --- | --- | --- | --- |
| F-M050-01 拼图循环 | D2 | 旧官方指南 p5/p6；Google Play 声明；2015 作者体验 | 取块、组合/划切、拖入轮廓、部分匹配后着色/描数得星、完图获币解锁在历史材料中有依据 | 当前构建入口；错放反馈；提示/撤销；最后落位至结算；奖励量、重玩收益、提交时机、恢复 |
| F-M050-02 Ladder | D2 | 旧指南 p7/p8；Google Play 声明；2015 作者体验 | 目标高度、炸弹、增长箭头、抵达后补全选关图在历史材料中有依据 | 当前碰撞、生命、失败/重试、结算、奖励保留、恢复 |
| F-M050-03 家庭档案 | D2 | 官方帮助正文/索引；外部 Kids/Kahoot 操作步骤 | 外部应用中的创建、编辑、删除入口及部分档案额度可描述 | Numbers 局内切档；最终删除确认、撤销/保留期；删除涵盖哪些 Numbers 数据；同步冲突 |
| F-M050-04 家长活动报告 | D2 | 官方 Dashboard 索引正文 | 周切换、累计主题进度、按日更新时间、免费与套餐字段边界 | 未看配图；Numbers 专属准确率/事件字段；实时性；是否能回到精确关内位置 |
| F-M050-05 拼图错放后求助与重试 | D1 | 2018 作者报告两个孩子放置受挫；旧指南仅给目标两端 | 只确认曾出现“放不到预期位置”的历史问题 | 数学规则不允许、触摸判定还是操作失误无法诊断；程序反馈、内置提示、撤销、重试到结算均未知 |
| F-M050-06 未完拼图退出与恢复 | D1 | 档案帮助、版本更新、Offline 标签只能作外围线索 | 尚无局内恢复事实 | 退出/重开入口、槽位保存粒度、金币提交、强退/断网、跨端冲突；不能套 Kahoot 主应用机制 |
| F-M050-07 Sandbox | D2 | 旧指南 p3/p4/p8；Google Play 声明 | 组合、分割、堆叠、比较；教材示例凑十、33−17；指南称可构建到 100；无固定胜利终点 | 完整菜单、对象来源、精确手势、弯箭头作用、丢弃/撤销/清空、退出存档；教材提问不是自动判分 |
| F-M050-08 Run | D1 | 当前商店声明；2018 作者体验；官方 Run 页面标题 | 历史作者称按数字决定跳高并收集星币；当前商店只声明快速判断与越障 | 完整入口、控件与映射、节奏、错误惩罚/生命、结算、重试、恢复；官方视频未播放 |

深度汇总与 `coverage.csv` 一致：F01/F02/F03/F04/F07 为 D2，F05/F06/F08 为 D1。没有 D3/D4，没有本项目实测。旧版指南、2015/2018 作者体验和当前 Kahoot 商店构建必须分开引用。

## 原件与视频采集状态

- 官方教学指南原件：`work/children-interactions/dragon-guide.pdf`，本地 `file` 显示 PDF 1.3、8 页，SHA-256 `def05eb635f225863d354b03913228a94896ca382aa50b4e3cf956239e451605`。案例称“9页”源自页码/提取记录，而本地 PDF 容器报告 8 pages；这是一个尚未解释的元数据差异，应在复用前核对，不自行改正式资料。
- 已正式归档并实际查看：指南 p4、p5；旧批次还查看过 p3、p6、p7。p8 已查看但只留在 `work/dragonbox-controls-deepening/guide-p8.png`，未正式重复归档。
- 全文文字层 p1–p9 均已读。图片观察并不覆盖每一页；p1/p2/p9 只有文字审阅记录。
- Google Play 原 HTML 已保存（HTTP 200，约 1.34 MB）并读正文；`googleplay-images.json`、`googleplay-links.json` 保存了图片/链接清单，但未保存对应商店截图或视频二进制，也没有逐图查看记录。
- 两篇历史作者文章原 HTML、纯文本均已保存且正文已读：GeekDad 2015、Games for Young Minds 2018。两文中的嵌入视频均未播放；其中 2018 文引用 GeekDad 同一拼图视频，不能算两段独立演示。
- 官方 Run YouTube 只完成一次匿名正常导航：页面 HTTP 200、登录请求 401、播放器 `readyState=0`、画面尺寸 0×0；只保存阻断页 HTML/文字/截图，没有 MP4、音频或产品画面。阻断截图只证明访问失败，不是玩法证据。
- Numbers 官网公开 HTML 曾定位官方演示 `bSr73I7Yi0w`，读取失败；未保存视频、字幕或连续画面。

## 尚未读／未看但已保存、仍可利用的来源

1. **Google Play 图片与视频链接清单**：`work/dragonbox-controls-deepening/googleplay-images.json`、`googleplay-links.json`。目前只是从已保存 HTML 提取出的远端资源描述；可先离线筛选 URL、标题和资源类型，再决定是否值得另行授权采集。它们本身不能作为已观察视觉证据。
2. **Google Play 原 HTML 中的媒体元数据**：`googleplay.html`。正文已读，但大量嵌入结构未做逐项视觉审计；可能含 Numbers 商店截图/预告片引用。当前没有本地图片/视频文件。
3. **历史文章中的嵌入视频线索**：两篇原 HTML 已保存、正文已读，嵌入视频未看。可从本地 HTML 离线提取视频 ID、上下文和是否重复，避免先走受限播放路线。
4. **指南未逐图观察页**：PDF 原件及 p1–p9 文字均保存；p1/p2/p9 未有逐图观察记录，p8 已看且保存于 work。其潜在价值较低，主要用于范围、版权页或产品数量声明，不太可能补齐错误/恢复链。
5. **Dashboard 配图**：inventory 明确“索引正文已读；未看配图”，但普通 GET 得到的是 403 页面壳，本地没有真实配图。因此它是“未看且未成功保存”，不能当作当前离线可用原件。

## 已失败／受限路线，避免再试

- Kahoot Learning apps 目录、旧档案帮助、Dashboard、订阅帮助的普通匿名 HTTP GET 均返回 403；已停止且未重试。同一路径不要再次普通 GET。现有可用内容来自官方公开索引文本 `search-profiles.txt`，必须继续标成索引证据。
- 官方 Run YouTube 已在现有 Chromium 做过一次匿名正常导航，要求登录验证；同时出现登录请求 401，媒体未就绪。不要重复匿名打开、改 embed、绕验证或登录尝试。
- Numbers 官网嵌入演示曾读取失败，没有字幕/画面；不要把链接存在当成看过，也不要重复相同读取方式。
- DragonBox 隐私页请求已落原始 HTML，但采集器因 `title` 参数冲突报错，HTTP 状态未保留；随后只做本地修复解析。不要为补状态重抓，也不要补造 HTTP 状态。
- gameplay 批次 `acquire_once.py`、`build_once.py` 已明确 APPLIED ONCE，交接要求不要重跑。
- 首次历史文章解析曾因缺少 bs4 退出 1；未安装依赖，后来用标准库解析成功。不要安装 bs4 或重复失败解析路线。
- Big Numbers 的 Progression Reset 属于另一产品；Kahoot 主应用离线续答、School 进度、Osmo 后半体验均不能拼接到 Numbers。

## 仍然未知

- 当前可安装 SKU/构建的真实菜单、版本一致性、订阅/单次购买分支。
- 四个玩法任一完整连续链及当前视觉、声音、触控容错。
- 错误、提示、撤销、失败、重试、结算、奖励与恢复的精确状态转换。
- 离线首次授权、局内存档粒度、跨端同步与删除数据范围。
- Plus 上的拖动/跑酷刷新与延迟适配；现有材料不构成目标硬件验证。

## 本次命令、退出状态与状态变化

1. `pwd && rg --files ... | sort`：exit 0；定位案例、两套批次文件及相关资料。
2. `cat AGENTS.md; sed ...; cat ...; rg ...; find ...`：exit 0；输出因体积被工具截断，随后改为分文件读取，没有据截断段落作结论。
3. `for f in ...; do cat "$f"; done`：exit 0；读取案例、coverage/inventory/evidence、来源摘记、HANDOFF、采集 JSON 与验证结果；输出局部截断，关键文件后续单独核查。
4. `python ...; find ...; file ...; sha256sum ...`：整条 shell 最终 exit 0，但首个子命令 `python` exit 127（环境无 `python`）；后续 `find/file/sha256sum` 成功。此命令存在部分失败，未将其误记为完整成功。
5. `python3 - <<'PY' ...; rg ...`：exit 0；用 27+7 行 inventory 精确列出 review status，并反查失败/未看记录。`rg` 命中巨大已保存 HTML，输出被截断；inventory 与正式摘记已足够形成结论。
6. `mkdir -p ...; cat > ...`：创建本报告，是本次唯一预期状态变化。

状态变化：仅新增 `work/research/batch49-integration/dragonbox-inventory.md`；未改 `docs/`、案例、coverage、evidence、inventory 或任何既有原件，未联网、未安装、未运行采集/构建脚本。

根代理后续核对：见dragonbox-pdf-check.json，PyMuPDF解析页树为9页，原SHA不变；file(1)的8页输出不足以推翻原页码。未新增视觉或玩法证据。
