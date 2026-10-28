# M019 本轮执行记录

查阅日：2026-10-26。只改M019案例及本批两文件；不改master或旧批次。

已执行命令：

- `cat AGENTS.md docs/research/interaction-library/cases/M-019-shanbay.md`，并用`rg`定位M019/E-SBY旧证据与coverage；读取退出0。E-SBCD写入前未发现已用编号，检查脚本再对全部其他批次查重。
- `timeout 45s python3 -u work/shanbay-controls-deepening/fetch-cms.py`，退出0；按既有帮助前端公开CMS格式读取10篇，均HTTP 200。不调用学习、账户、提交接口。
- `timeout 45s python3 -u work/shanbay-controls-deepening/fetch-selected.py`，退出0；四份大陆App Store HTML为200。指南第7/8图返回200但Content-Type为text/html，正文为浏览器校验脚本，属于未取得图像。未执行脚本或再请求，返回内容改名为blocked-response.html。
- `python3 work/shanbay-controls-deepening/write-batch.py`，退出0；写12E、2新F及8条旧coverage建议。
- `python3 work/shanbay-controls-deepening/check-batch.py`，退出0；检查编号/引用/CSV LF/哈希/旧批次未改及建议字段范围。
- `git diff --check -- docs/research/interaction-library/cases/M-019-shanbay.md docs/research/interaction-library/batches/shanbay-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/shanbay-controls-deepening-coverage.csv`，退出0。仓库这些资料为未跟踪状态，因此以Python显式文件检查为主要依据，不将空git diff当成内容验证。

web工具检索/打开官方帮助、指南、四个大陆商店页；搜索误评/撤销/误触/跳过/重录等未取得可核定的现版控制说明。未采纳社区经验或隐私数据列表推导按钮动作。没有helper/worker调用、安装、登录、购买、账户操作或视频下载。

版本日期由本地HTML相应版本条目的time datetime确认；定位摘录见apple-version-locators.json。CMS assign_date不作更新时间。所有新E的visual_observed=false，未创建静态界面artifact。

旧F更新仅建议，不实际写旧批次：existing-coverage-updates.csv；逐字段差异见coverage-changed-fields.json。8行均只改evidence_ids、known_branches、gaps，不改depth。
