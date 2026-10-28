# 本轮命令与访问记录

工作目录均为 `/home/workspace/reader`。只读检索/阅读命令退出0；`rg ... || true`用于允许“没有匹配”，不把空结果说成来源失败。

## 本地阅读/检索（exit 0）

- `rg --files docs/research/interaction-library work | rg 'anki|Anki|M-020'`
- `cat AGENTS.md`；`sed -n '1,110p' README.md`；`sed -n '1,100p' docs/requirements.md`；`rg -n 'G-001|G-002|R-016|H-004|H-005' docs/ideas.md`
- `cat docs/research/interaction-library/cases/M-020-anki.md`；检索主表和各批的 `F-M020` / `product_id=M-020`；旧23E和22F。
- `rg -n 'E-AKCD|anki-controls-deepening' docs/research/interaction-library work/anki-deepening || true`：E-AKCD属M065，弃用。
- `rg -l 'E-ANCD|anki-controls-deepening' docs/research/interaction-library || true`：新前缀/批次未占用；后用build/check全库JSON核重。
- `cat work/anki-deepening/llms.txt`；读取 `studying.md`、`deck-options.md`、`media.md`、`profiles.md`、`templates-intro.md`、`templates-generation.md`、`browsing.md`、`troubleshooting.md`。
- `rg -n -i 'undo|redo|auto advance|auto-advance|wait for audio|answer action|question action' work/anki-deepening/{studying,deck-options,browsing,preferences}.md`
- `rg -n '23.12|2.1.45|2.1.40|Release|releases|changes/' work/anki-deepening/llms.txt | head -n 45`
- `sed -n '1,80p' docs/research/interaction-library/template.md`；读取library README、batches README及旧批字段规范。
- 新原件正文按 `cat` / `sed` 阅读：`misc.md`、`errors.md`、`files.md`、`syncing.md`、`backups.md`、`profiles.md`、`media.md`、`deck-options.md`、`changes-2.1.40-49.md`、`changes-23.12-plus.md`、`missing-media.md`、`sync-profiles.md`、`scheduler-2021.md`。
- `rg -n -A12 -B4 'automatically disables|current behaviour|auto-advance|Offer to save|datetime=' work/anki-controls-deepening/{issue-4228.txt,release-23.12.txt}`；另核HTML的2023-12-25、2025-07-25日期。
- 用一次性Python只读扫描全库新URL及E-ANCD碰撞、统计Desktop页面；均退出0，结果见check脚本。

## 原件获取与本批生成

- `mkdir -p work/anki-controls-deepening`；写 `fetch.py`：exit0。
- `python work/anki-controls-deepening/fetch.py`：exit127，python命令不存在；未联网。
- `python3 work/anki-controls-deepening/fetch.py`：exit0，13个正常公开GET，全部200。
- 首次 `python3 work/anki-controls-deepening/fetch-extra.py`：exit1，bs4不存在；未联网。
- 用标准库HTMLParser替代bs4后，再运行同命令：exit0，3个正常公开GET，全部200；未安装依赖。
- `mkdir -p docs/research/interaction-library/artifacts/anki-controls-deepening`；写短摘记和本批build/update-case/check脚本：exit0。
- `python3 work/anki-controls-deepening/build.py`：exit0；7E、0F、36字段建议、6追加采集、60目录项。
- `python3 work/anki-controls-deepening/update-case.py`：exit0；后以一次性Python限定修改历史措辞/标题：exit0。
- `python3 work/anki-controls-deepening/check.py`：exit0；PASS，290项结构/引用/原件与正式材料散列/旧批未改检查。
- `git diff --check -- docs/research/interaction-library/cases/M-020-anki.md docs/research/interaction-library/batches/anki-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/anki-controls-deepening-coverage.csv docs/research/interaction-library/batches/anki-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/anki-controls-deepening/source-notes.md`：exit0。

## web工具辅助定位（不等同HTTP原件状态）

- open `https://docs.ankiweb.net/manual/misc`、`https://docs.ankiweb.net/manual/templates/errors`：工具Internal Error；Studying正文可读。正常.md GET独立成功，未遇401/403/429。
- search `site.docs.ankiweb.net undo review Ctrl Z Anki`（点号拼写导致宽检索，结果未用作证据）；`site:github.com/ankitects/anki/releases "23.12" "auto"`；`site:forums.ankiweb.net "auto advance" "stop" "dae"`。
- search `site:forums.ankiweb.net "auto advance" "dae" "pause"`；`site:docs.ankiweb.net "undo" "review"`；`site:github.com/ankitects/anki/releases "Auto advance" "stop"`。
- open/find发行23.12、维护者issue4228，成功正文定位；find scheduler Markdown工具Internal Error，但正常GET成功并已读。
- open `https://github.com/ankitects/anki/pull/2765`：公开讨论可读；只列排除材料，不以设计要求/评论/提交标题证明发布后的完整操作链，不下载媒体或读取文件diff。
- 第三方教程、AnkiDroid、翻译字符串等命中均未用于Desktop具体UI事实。
