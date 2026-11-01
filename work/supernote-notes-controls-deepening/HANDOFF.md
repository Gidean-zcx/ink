# M006 Supernote 核心笔记补强交接

2026-10-27。本批只研究既有 A5 X2 Manta / A6 X2 Nomad 英文 X2 V3.28.42 手册及2026-10-26保存的X2日志。**0新E、5新F（11–15）、3旧F/10字段建议、2旧E追加采集建议**。没有网络请求，没有重试旧失败路线，没有产品/账户状态变化。文件仅M006案例、新批/摘记/必要页图及本work目录改变；旧批/master/global均未写入。

## 正式文件

- `docs/research/interaction-library/cases/M-006-supernote.md`
- `docs/research/interaction-library/batches/supernote-notes-controls-deepening-coverage.csv`：5行，全部D2/false/static_reference_only。
- 同目录 `supernote-notes-controls-deepening-evidence.jsonl`：0字节，0新来源；不是缺失写入。
- 同目录 `supernote-notes-controls-deepening-inventory.csv`：140个旧书签层级行，29行本轮正文复读；另111行继承旧状态。父子项/前后说明均包含，不能按140项功能已完成解读。
- `docs/research/interaction-library/artifacts/supernote-notes-controls-deepening/source-notes.md`：独立简短摘意及视觉审计。
- 同artifacts目录 `x2-v3.28.42-p65.png`、`p66.png`、`p75.png`、`p83.png`、`p153.png`：均经view_image实际查看；页码=PDF序号。一页多图仅并列示意。

## 合并建议（仅建议，旧文件未写）

- `work/supernote-notes-controls-deepening/existing-coverage-updates.json`：每条包含source_batch、逐字段before/after、replacement_row。
  - F-M006-01：known_branches / gaps / checked_on，共3字段；标题区域不能直接书写、关键词输入/最近项。
  - F-M006-08：known_branches / gaps / checked_on，共3字段；空查/输入对象与文本框搜索入口未展开。
  - F-M006-09：evidence_ids / known_branches / gaps / checked_on，共4字段；3.28.42隐藏层TXT/DOCX导出修复声明。
- `work/supernote-notes-controls-deepening/existing-evidence-additional-captures.json`：E-SNCD-02、10各追加1个local_reread capture，含before_exists/before/after/append。**没有新HTTP抓取**，同一原件/URL复用旧编号。
- E-SNCD-02原PDF与E-SNCD-10原日志的主字段、旧图片及旧140项CSV均未改。新capture的正式摘记为library相对路径，raw_work_path为repo相对路径。旧M006 E没有visual_audit_path引用case；没有旧审计hash迁移需求。
- 本轮没有新E，故不存在需填supersedes的新行；原18E仍各保留原supersedes字段。

## 最有价值的原件核查入口

1. `https://ib.supernote.com/x2/X2_V3.28.42_EN.pdf`，复读副本 `work/supernote-notes-controls-deepening/raw/X2_V3.28.42_EN.pdf`。
   SHA256 `9c7b24be22b8f06727381564412f192273e50e2157a2aa5b4350d12b6c3037d2`，等于旧E-SNCD-02。p28：当前页最多20次Undo，排除增删层、模板变化；p59实时识别笔记无层；p64–67模板与层；p74–76文本框；p81–84整页复制移动；p150/152–155日历同步条件与事件维护。目录p63/正文p64模板页差异仍保留。
2. `https://support.supernote.com/changelog-for-manta-and-nomad`，副本 `raw/10-changelog.html`。
   SHA256 `28ce34428094d1f9778d5071ccb79a4bfdc7ea946170099001cc9e61823b1030`，等于旧E-SNCD-10；页面更新2026-10-23，原采集2026-10-26。
   - `raw/10-changelog.txt`原文行254–258：Chauvet3.25.39（2025-11-14）Text Box优化，指点框外退出、笔长按/套索编辑。
   - 行221–224：3.28.42（2026-05-22）Undo/Redo按钮、隐藏层TXT/DOCX、擦空另一层后主层链接、模板后关机保存卡住的历史修复声明。
   - 行280–286：3.24.38文本框/笔记内与全局文字搜索。手册135–137仍未重建这项新搜索入口。
   - 3.29.42重复列部分修复，不作为第二份独立交叉验证。

p65模板图可见源页与目标页计数但不证明分配规则；p66层面板可见显隐、加删；p75文本框编辑；p83复制目标及Cancel/OK；p153New Event/New Task分开、事件表单时间。所有视觉事实仅上述静态页；没有视频、设备、网页交互或成功保存观察。

## 实际命令、退出与文件状态

工作目录均为 `/home/workspace/reader`。

- `rg --files work docs/research/interaction-library | rg 'supernote|M-006'`；`cat docs/research/interaction-library/cases/M-006-supernote.md`；`rg -n 'Supernote|Manta|Nomad' README.md docs/requirements.md docs/ideas.md`：定位及读取成功。起初误猜 `batches/supernote-controls-deepening-inventory.csv`，cat不存在，组合命令exit1；改读实际artifacts路径，未修改任何文件。
- `cat AGENTS.md`、`cat README.md`、`cat docs/requirements.md`、`sed -n '1,130p' docs/research/interaction-library/template.md`，exit0；读取ideas相关行、既有case/18E/10F与140目录。一次大输出被工具截断，之后用定段及结构化输出复读所需内容。
- `cat work/supernote-controls-deepening/HANDOFF.md`；Python读取http-snapshots.json；旧来源均为200记录，无新HTTP；exit0。后随 `rg -n 'E-SNNC|E-SNND|supernote-notes-controls-deepening' ...` 无命中，组合exit1是预期查重空结果。
- `sed -n '1430,1835p' work/supernote-controls-deepening/manual.txt`；`sed -n '1834,2095p' ...`；`sed -n '3345,3450p' ...`；`sed -n '3916,4135p' ...`；`sed -n '3508,3600p' ...`，exit0。另从manual-pages.json读取28/58/59/150/155；后接误猜旧Khan建议文件名的sed失败，组合exit2；不涉及Supernote原件或任何写入。
- `sed -n '169,326p' work/supernote-controls-deepening/10-changelog.txt`；`rg -n 'text box|Text|template|layer|calendar|undo|Redo|Clipboard|paste|copy|move' ...`，exit0；只读已存日志。
- Python内联基线脚本：copy2当前case到case-before.md；读取所有旧batch中M006记录到coverage-before/evidence-before；保存baseline-paths；copy2原PDF/日志到本raw。exit0。新目录此前不存在；新前缀E-SNNC无占用，但实际0新E。未复用任何旧构建脚本。
- `PYTHONPATH=work/research/tools/pymupdf python3`内联只读PDF/渲染脚本：get_text提取39页至raw/selected-pages.txt，get_pixmap(Matrix(1.25,1.25))渲染65/66/75/83/153。exit0，无安装；随后5次tools.view_image均成功，无shell退出码。
- `python3 work/supernote-notes-controls-deepening/build.py`：两次均exit0；第二次仅明确inventory各条实际观察页码。生成本批正式文件与增量建议。冻结后不要重跑。
- Python difflib比较case-before和当前case：exit0；已人工审阅新增5F与旧3F的变更。
- `python3 work/supernote-notes-controls-deepening/check.py`：exit0，536 PASS。只读核对旧M006 18E/10F未变、全部before/replacement、F/E引用、目录状态、原件/图/独立审计hash及LF；不能在root合并后再跑。
- `git diff --check -- docs/research/interaction-library/cases/M-006-supernote.md docs/research/interaction-library/batches/supernote-notes-controls-deepening-coverage.csv docs/research/interaction-library/batches/supernote-notes-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/supernote-notes-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/supernote-notes-controls-deepening`：exit0。正式文件可能尚未跟踪，专属check已另外验证内容，不能仅据git检查。

最终带manifest的只读核验命令为 `python3 work/supernote-notes-controls-deepening/check.py --manifest`；实际最终输出另存 `check-results.json`。manifest登记本批全部正式文件、原件及建议/脚本/HANDOFF，排除manifest自身和check-results以避免循环。散列逐文件以manifest为准。

## 资料边界与后续

只据V3.28.42原件和有日期的日志，未重新查2026-10-27线上版本；3.31.45全新界面与实际构建不明。当前页20步撤销不证明跨页/重启持久；删除页/层/事件恢复、模板页数分配、剪切未粘贴中断、文本框保存回执/识别准确度、事件与笔迹双向关联、日历离线冲突均未取得完整链。贴纸、笔迹复制和纯文本剪贴已读正文，但未为凑数量单独新增F；阅读、邮件、应用、锁定等大量目录仍未展开。没有把日志修复当复现通过，也没有把同步当可恢复备份。

若继续，优先同版本测试笔记的模板错选、图层删除、整页跨类型搬移、日历事件删除/离线同步；当前只列下一类证据，不执行设备或账户操作。root负责重要结论复核与旧F/E合并。
