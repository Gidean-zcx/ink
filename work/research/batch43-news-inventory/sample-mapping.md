# TIME K–1 2026-05-08公开样刊与Oceans教师指南映射

日期：2026-10-27。仅本地只读核对；未访问样刊、未运行浏览器、未修改 `docs/`。

## 结论

目标样刊：

`https://reader.emagazines.com/issue/time-for-kids-k-1/may-8-2026`

当前正式资料中**没有登记该 URL**，也没有对应的 E、F 或 inventory 行。它不能直接并入现有“植物单篇文章”操作证据，也不能使用 M-048 The Week Junior 的样刊编号；如果根代理取得独立整期操作，应在 M-047 下新增独立来源和任务。

与其内容邻近的 May 2026 Oceans 教师指南已经完整登记，可直接复用 **E-TFK-04 / F-M047-02**，不应再造同一PDF的新 E 或新教学任务。

## 已有教师指南映射

| 项目 | 应复用值 |
| --- | --- |
| 稳定证据 ID | `E-TFK-04` |
| 稳定任务 ID | `F-M047-02` |
| 产品 | `M-047 TIME for Kids` |
| E 持有批次 | `docs/research/interaction-library/batches/news-evidence.jsonl` |
| F 持有批次 | `docs/research/interaction-library/batches/news-coverage.csv` |
| 案例位置 | `docs/research/interaction-library/cases/M-047-time-for-kids.md`，F02及来源段 |
| 官方URL | `https://www.timeforkids.com/wp-content/themes/tfk/build/assets/pdf/educators/teachers-guide-gk1.pdf` |
| 版本范围 | `May 2026 K-1 Oceans; 12-page PDF` |
| 已读范围 | PDF pp.1–3；静态查看第2、3页 |
| 原PDF临时件 | `work/news-interactions/tfk-teacher.pdf` |
| 原PDF SHA-256 | `849e57011fd382fa8582b4ecf998589d5097877c5bd1fa9f7d0883041ce132a3` |
| 正式静态页 | `artifacts/news/tfk-teacher-p2.png`、`artifacts/news/tfk-teacher-p3.png` |
| 正式页散列 | p2 `2023276351b7f6672a5551227a4cdccea8bf6035c5b0fbb558a54e2b7d99f5c3`；p3 `2c04c6a7a61f604076a9350b910dc14ae16b5942ef6a21e6f182d2a184361897` |
| coverage深度 | D2、`hands_on=false`、`visual_status=static_reference_only` |
| 证据边界 | 成人组织的纸面/课堂任务，不是网站自动评分或整期阅读器操作 |

E-TFK-04 当前字段可直接沿用：`source_type=official_teacher_guide`、`access_mode=downloaded_pdf_text_and_static_pages`、`visual_observed=true`、`capture_state=selected_static_excerpts_archived_other_pages_temporary`。若新样刊操作确认与指南同刊相关，只在新任务的 `evidence_ids` 中同时引用 E-TFK-04；不要修改它为 D4 或 `hands_on=true`。

## 目标样刊在旧目录中的状态

对完整正式库、brief和 Batch43 work 报告进行定点字符串搜索：

- 未出现不带token的目标 URL `reader.emagazines.com/issue/time-for-kids-k-1/may-8-2026`。
- 当前 M-047 的 `time-vocabulary-deepening-inventory.csv` 有83行，覆盖TIME主站年级/主题、旧G3–4文章控件与纸面测验；没有eMagazines整期样刊节点。
- 当前 M-047 只有 F-M047-01～05。F01是TIME主站K–1单篇/目录/Science操作；F02是Oceans教师指南；F03词卡；F04单篇朗读；F05纸面测验。没有“整期版面阅读器”任务。
- 资料库存在 eMagazines 阅读器操作范例，但属于 **M-048 The Week Junior**，其 F/E、刊物、URL和内容均不能复用给M-047。只能参考审计文件结构。

## 新增独立整期阅读流程时的编号建议

在当前稳定编号之后，新的用户任务应使用：

- **建议 F：`F-M047-06`**，任务应明确为TIME K–1整期样刊的版面阅读、跳页/搜索/保存/退出恢复等实际覆盖范围。
- **建议 E：使用新的 TIME 专属前缀和未占用编号**，由根代理在写批次前最终确定。不要复用 `E-TKCD-*` 或 `E-TKVD-*` 中已有ID，也不要借用 M-048 的 `E-WJCD-*`。
- 若一次操作同时补旧 F01/F04 的导航或音频子范围，可以把新 E 追加到旧F；只有整期阅读器这个新任务登记一次F06，不能按每个按钮拆多个F。

## 正式增量文件与格式

建议新建同名前缀批次对，例如：

- `docs/research/interaction-library/batches/time-sample-reader-deepening-evidence.jsonl`
- `docs/research/interaction-library/batches/time-sample-reader-deepening-coverage.csv`

若需要逐个登记整期阅读器控件/状态，可另建同名前缀 `-inventory.csv`；inventory不参与F/E总数。

### coverage.csv 11字段

`flow_id,product_id,task,depth,evidence_ids,known_branches,gaps,visual_status,hands_on,case_path,checked_on`

若取得可复查公开网页操作，F06应为：

- `product_id=M-047`
- `depth=D4`
- `visual_status=observed_browser_sequence`
- `hands_on=true`
- `case_path=cases/M-047-time-for-kids.md`
- `evidence_ids`含新操作E；若与Oceans指南确有同刊内容映射，可再含`E-TFK-04`

### evidence.jsonl 基础字段

至少保持：

`evidence_id, product_id, title, url, source_type, access_mode, checked_on, source_updated_on, version_scope, app_build, firmware_version, locator, note, hands_on, visual_observed, archived_artifact, sha256, capture_state, supersedes`

D4操作E还必须包括：

- `hands_on=true`
- `visual_observed=true`
- `visual_kind=browser_sequence`
- `operation_method=automated_public_browser`
- `operation_audit_path`及正确`operation_audit_sha256`
- `artifact_hashes`包含正式 `observed-states.json`、`trace-actions.json`，并登记所有保留截图/日志散列

操作审计正文必须同时包含新E编号和F-M047-06，写明环境、入口、逐步动作、成功/失败、未测范围及外部状态变化。若URL最终带访问token，应区分“发现入口URL”和“实际成功访问URL”，不要从根消息里的示例token复制成未验证原址；正式记录使用实际操作取得且可解释的URL。

## 整合时需更新的现有文件

由批次合并与人工正文分别处理：

1. `docs/research/interaction-library/cases/M-047-time-for-kids.md`：加入F06任务、整期与单篇/教师指南的关系、失败与恢复边界。
2. 新 `*-evidence.jsonl` / `*-coverage.csv`：正式持有新E/F。
3. 若建目录：新 `*-inventory.csv`，登记入口、阅读器控件、状态、E/F、观察状态和缺口。
4. `docs/research/interaction-library/evidence.jsonl`、`coverage.csv`：由 `merge_batches.py` 生成，不能手工与批次脱节。
5. `docs/research/interaction-library/case-index.md`、`catalog.csv`：由 merge 根据新增F/E重算；M047任务数应从5变6，D4从3变4（假设只新增这一条D4且不改其他深度）。
6. `docs/research/interaction-library/reference-points.csv`：仅当整期样刊产生可复用的具体状态/恢复证据时添加RP；15字段必须完整，F/E/附件存在。
7. `docs/research/interaction-library/design-reference-guide.md`：若新增RP或新增“整期阅读/退出恢复”设计入口，更新相应问题链及RP总数。
8. `docs/research/interaction-library/analysis-status.md`、`README.md`、`batches/README.md`、对应新brief：更新本批范围、数量和限制；不能把一次公开样刊操作写成全TIME或儿童实测。

最后运行 `merge_batches.py` 一次，再运行 `check_docs.py`；本次只读盘点没有执行它们。

## 执行记录

1. 定点搜索目标样刊URL、May 2026、E-TFK-04及M047 F：

   ```bash
   rg -n -i 'reader\\.emagazines\\.com/issue/time-for-kids-k-1/may-8-2026|may 8,? 2026|may oceans|teachers-guide-gk1|E-TFK-04|F-M047' docs/research/interaction-library docs/briefs work/research/batch43-news-inventory
   ```

   Exit 0。只命中教师指南和既有F；未命中目标样刊URL。

2. 定点检索 M047 的TIME/eMagazines URL、案例、inventory和批次行：`rg` exit 0。
3. 读取 `evidence.jsonl` 中E-TFK-04、`news-evidence.jsonl`、`news-coverage.csv`、master coverage、M047案例及`time-vocabulary-deepening-inventory.csv`：exit 0。
4. 核对 E-TFK-04 正式/临时文件存在并计算散列：exit 0。
5. 读取 `data-dictionary.md`、`batches/README.md`、`check_docs.py` 的F/E/D4格式约束：exit 0。
6. 写入本报告并计算SHA-256：exit 0。

## 未知与状态变化

- 本轮没有访问样刊，无法确认无token URL是否直接可用、刊号/页数、控件、音频、保存或恢复行为。
- 尚不能仅凭日期断言样刊与E-TFK-04教师指南逐页一一对应；需要根代理从样刊封面/目录/内容实际核对。
- 新E前缀及最终F任务措辞由根代理在操作证据完成后决定。
- 状态变化仅新增本报告；未修改 `docs/`，未运行浏览器、merge或integration。
