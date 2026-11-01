# Quizlet 本轮取证记录

日期 2026-10-27。限定官方公开英文帮助；未登录、注册、生成、答题、提交、变更权限、删内容、加入课堂、购买、安装或下载远程视频。无 helper。只改指定 M021 case、新批次三文件和本工作目录；新正式图片/观察为零。

## 已检查材料与查询

- `AGENTS.md`、`README.md`、`docs/requirements.md`、`docs/ideas.md` 相关生成/学习条目；此前已读取资料库 README/template/batches 约定。
- 旧 `cases/M-021-quizlet.md`，dictionary-evidence.jsonl 的 E-QZLT-01～13、dictionary-coverage.csv 的 F01～10，主覆盖的十行只读快照；存 baseline.json 与 before 文件。
- 初始检索 `site:help.quizlet.com "Studying with Test"`、`site:help.quizlet.com "Deleting" "recover"`、`site:help.quizlet.com "progress" "reset" Learn`，随后从官方类目/章节链接读取正文。
- 针对误判缺口检索 `site:help.quizlet.com "Test" "I was correct"`、`site:help.quizlet.com "test" "wrong" "review"`，工具返回无检索结果。这不是不存在该能力的证明；最终仅保留官方可读正文明确的按钮。
- 本轮初始还读了 help 首页、Learn、Smart Assist；Smart Assist 内容与旧证据重复，不另增 E。Import、Study Guides、草稿正文复查保存于后述响应，但相同事实不增新 E。没有扩展到目录中所有新 AI/游戏/社交文章。

## 保存方式与一次失败

五个 `quizlet-response-0N.txt` 是 web 工具的公开正文响应字符串，包含来源 URL、章节和 L 行号；**不是原网站 HTML**。本轮用 apply_patch 保存工具文本，再由 prepare.py 拆出 20 个文章/目录的纯文本到 parsed/；source-map.json 记录原响应路径及 SHA-256。解析文本仅便于阅读，证据 raw_work_path/raw_sha256 指向原工具响应。

`fetch-probe.py` 对已由 web 工具成功读取的 Test URL 作一次普通 requests GET（timeout=(5,18)），返回 HTTP 403。已停止直接抓取路线，没有变换请求头、代理、镜像、公共 API 或账号接口重试，也没有保存/执行挑战内容。后续使用原本可用的公开 web 文本路线，不声称取得原 HTML。故 HTML 元数据、平台 tab DOM、修订时间和构建信息均未知；不把导航标签顺序当平台段顺序。

原件文本的准确文章定位见 evidence 的 locator 与 source-map.json；L 指工具行号。所有新 E 均 hands_on=false、visual_observed=false、archived_artifact=null。没有图片工具调用；没有静态图片升 D3。

## 核心边界

- Write/Spell 的“两次正确”是所述轮次结束条件；不是掌握测量。
- Reset 仅明确移除 Learn 进度；Android 范围与设置页不一致，未合并成统一承诺。
- Test 未完成离开不保存；不套到 AI Practice Tests。
- AI 试卷明确的编辑为标题/权限，逐题题干答案校订未知。
- Progress 跨端答题统计与 Flashcards 会话位置不是同一对象。
- 卡对删除、整集删除、隐藏动态、移除下载及生成指南删除不混用；整集不可恢复不证明其他对象也不可恢复。

## 命令/文件与退出

cwd 均 `/home/workspace/reader`。

- `sed -n '1,100p' AGENTS.md && sed -n '1,85p' README.md && sed -n '1,105p' docs/requirements.md && cat docs/research/interaction-library/cases/M-021-quizlet.md && rg -n 'E-QZCD|M-021' docs/research/interaction-library/batches/*evidence.jsonl docs/research/interaction-library/batches/*coverage.csv && rg -n 'Quizlet|资料.*生成|AI.*练习' docs/ideas.md`：exit 0。
- `cat docs/research/interaction-library/cases/M-021-quizlet.md && rg --files work | rg 'quizlet|dictionary.*(log|json|html|txt)$'`：exit 0。
- baseline 的内联 Python 写入：exit 0；紧接 `rg -n 'E-QZCD' docs/research/interaction-library --glob '*.jsonl' --glob '*.csv'` exit 1，表示无编号碰撞，不是错误。
- `python3 work/quizlet-controls-deepening/fetch-probe.py`：exit 0；HTTP 403 被记录，工具脚本退出 0 不表示已取得 HTML。
- `python3 work/quizlet-controls-deepening/prepare.py`：exit 0；20 个已保存官方页面，目录子项为23、27、12。
- `python3 work/quizlet-controls-deepening/write-batch.py`：exit 0；17 E、3 新 F、16 个旧字段建议、68 行目录。该脚本只运行一次；其后用 apply_patch 小幅修正 case，不重跑覆盖。
- `apply_patch` 修改 case、保存工具响应：均成功。
- 最终 `check-batch.py`、限定路径 `git diff --check` 的状态和输出 SHA 见 validation.json / HANDOFF.md。

## 交接数量

新 E-QZCD-01～17：4 目录、13 操作正文；E05 与旧 E-QZLT-04 为同一 article ID 的不同 locale，E17 与旧 E-QZLT-07 同 URL补章节，不能称17篇全新独立文献。F11～13 为普通 Test、AI Practice Test、Progress/星标子集复习；旧 F02/04/07/08 的16字段 before/after 留工作目录，由 root 决定合并。case 共13F/30E，全为D2文档；4父级+64入口中24项仅部分有任务依据，40项仍目录级，不是覆盖率或完成率。
