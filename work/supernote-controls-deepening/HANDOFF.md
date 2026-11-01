# M006 Supernote 补强交接

2026-10-26。11 新 E、4 新 F（F-M006-07–10）、6 旧 F 更新建议、140 目录书签条目、4 张已看静态图。全部 hands_on=false、无 D3/D4。只做文档与静态资料处理，未改产品或账号状态；没有修改 master／旧 batch／根文档。不要在 root 修改后重跑生成器。

## 文件

- docs/research/interaction-library/cases/M-006-supernote.md
- docs/research/interaction-library/batches/supernote-controls-deepening-evidence.jsonl
- docs/research/interaction-library/batches/supernote-controls-deepening-coverage.csv
- docs/research/interaction-library/artifacts/supernote-controls-deepening/functional-inventory.csv
- 同 artifacts 目录 x2-v3.28.42-p73.png、p84.png、p112.png、p160.png（全部实际 view_image）
- work/supernote-controls-deepening/old-coverage-updates.json：6 行各字段 before/after + replacement_row。旧 task 标题维持，更新 evidence_ids／known_branches／gaps；F02/03/04 的 visual_status 增为 static_reference_only。
- 同 work 目录：182 页 PDF、manual-pages.json、manual.txt、manual-index.html、9 份新帮助/日志 HTML 与文本、http-snapshots.json、manifest.json、check-results.json、构建/检查脚本。

## 关键复核

1. 官方索引 https://support.supernote.com/en_US/getting-started/download-manual 当前链接 https://ib.supernote.com/x2/X2_V3.28.42_EN.pdf ，正常 urllib HTTP200，19,158,941 bytes；SHA256 9c7b24be22b8f06727381564412f192273e50e2157a2aa5b4350d12b6c3037d2。
   - p9：仅 A5 X2 Manta / A6 X2 Nomad；手册V3.28.42不能等同设备最新3.31.45。
   - p98：Recognize as Digest → Create；p112静态菜单同时列Text/Digest/Task。旧E-SUP-03帮助 https://support.supernote.com/the-revamped-digest-app 却写Recognize as Text生成摘录。保留冲突，不自行判定是改名还是文案错误。
   - p73：逐步返回／一次回源及上半屏上滑；p160–161：文件副本、Digest备份、To-Do/Digest数据同步与非整机备份范围。
2. https://support.supernote.com/changelog-for-manta-and-nomad ，本地10-changelog.html/txt，HTTP200，SHA256 28ce34428094d1f9778d5071ccb79a4bfdc7ea946170099001cc9e61823b1030。
   - 3.31.45（2026-10-20）Adjusted／Optimized：云同步删除转云回收站、冲突检测处理、八种书写姿势。只具D1声明，恢复/冲突选择界面没有观察。
   - 3.25.39：笔记与目标移至microSD/OTG后链接保留；不能推断任意改名/删除。
   - 3.18.29：特定离线任务移动与原列表删除后重连丢任务修复，未做当前复现。

## 实际命令与结果

- `rg -n 'E-SNCD' docs/research/interaction-library`：exit1、无命中（首次冲突检查）。
- `sed -n '1,260p' docs/research/interaction-library/cases/M-006-supernote.md`、`sed -n '1,120p' AGENTS.md`、`sed -n '1,110p' docs/research/interaction-library/template.md`、`cat README.md docs/requirements.md`：exit0。另读取目录CSV、ideas相关行、现evidence/coverage。
- 原PDF通过Python urllib.request.urlopen(实际URL, timeout=60)下载，exit0/HTTP200；web点PDF先出现一次Internal Error，随后正常公开HTTP成功，无拒绝绕过。
- `python3 work/supernote-controls-deepening/fetch_sources.py`：exit0，9项全部HTTP200、无重试；确切URL与哈希见http-snapshots.json。
- PDF读取/渲染使用 `PYTHONPATH=work/research/tools/pymupdf python3` 内联脚本：pymupdf.open原PDF，get_text/get_toc；4页get_pixmap(Matrix(1.25,1.25))。exit0。没有安装软件。
- `PYTHONPATH=work/research/tools/pymupdf python3 work/supernote-controls-deepening/build_records.py`：exit0，输出11E/4F/6旧行建议/140目录/4静态图。
- `python3 work/supernote-controls-deepening/check_records.py`：exit0；字段、编号、前缀、引用、本地链接、哈希及观察标记PASS。
- `git diff --check -- docs/research/interaction-library/cases/M-006-supernote.md docs/research/interaction-library/batches/supernote-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/supernote-controls-deepening-coverage.csv docs/research/interaction-library/artifacts/supernote-controls-deepening`：exit0。这些正式资料目前为untracked，故此命令不替代自有检查。
- `git status --short -- docs/research/interaction-library/cases/M-006-supernote.md docs/research/interaction-library/batches/supernote-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/supernote-controls-deepening-coverage.csv docs/research/interaction-library/artifacts/supernote-controls-deepening work/supernote-controls-deepening`：exit0，显示正式文件未跟踪；work被忽略但文件存在。
- 曾有一次apply_patch因同时Delete/Add同路径而验证失败，未写入；后用quoted heredoc成功写case，exit0。没有部分删除遗留。

## 局限

无设备/账户/第三方云操作；没有完整最新工具栏或全局搜索界面。标题/星标删除后的笔迹、任务精确回句/删除恢复、OCR修改回写、导出结构往返、同名文件、断网重启、同步冲突选择及回收站保留/恢复再同步仍未知。目录登记不等于功能已重建；无远程视频、无实操性能结论。
