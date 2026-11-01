# M001 有限深化交接 — 2026-10-27

完成本批：新增 **E-KDCD-01–14（14E）**、**F-M001-09（1F）**；原8F保留且补证，案例现9F/23E，均为D2文档流程。没有登录、购买、发文件、安装、固件下载或设备操作；没有图片/连续视觉观察，没有 helper。没有改主索引、旧批次、汇总、README、requirements、ideas或其他case。

## 文件

- `docs/research/interaction-library/cases/M-001-kindle.md`
- `docs/research/interaction-library/batches/kindle-controls-deepening-evidence.jsonl`
- `docs/research/interaction-library/batches/kindle-controls-deepening-coverage.csv`
- `docs/research/interaction-library/batches/kindle-controls-deepening-inventory.csv`
- `work/kindle-controls-deepening/existing-coverage-updates.csv`：8个旧F、34字段；每行有before/after。原F04任务名拓为创建/整理笔记页和文件夹；F07增加2025+条件；其余编号和D2不变。
- `work/kindle-controls-deepening/check-results.json`：检查结果及正式文件哈希。
- 原件、提取正文、链接、取得记录、脚本均在该work目录；不发布整本指南副本。正式E的`archived_artifact/sha256`留空，原件信息用`raw_work_path/raw_sha256`。

目录58行含**9个父级标题、47个子目录项、2个正文子项**，表内有`entry_kind/parent_order`，不是58个独立功能，也不是全功能完成率。已有正文但没拆任务的功能仍登记缺口。

## 型号、日期与关键材料哈希

1. **E-KDCD-01 型号目录**：<https://digprjsurvey.amazon.com/csad/help/node/G7N7RPHV2SW8CKBW>。2024 Release链接到`kindle_scribe_2024`；旧E-KDL-01的`kindle_scribe`链接列在2022 Release下。
2. **E-KDCD-02 主样本**：<https://d1ergij2b6wmg5.cloudfront.net/kug/kindle_scribe_2024/v1/en-US/html/kug.html>。开头自称`1st generation, 2024 release`，保留原名不自行改代数。文件HTTP Last-Modified为`Mon, 05 Oct 2026 07:48:33 GMT`，这不是固件或功能上线日；`v1`只是路径。本轮未取得固定固件版本。
   - 原件`work/kindle-controls-deepening/02-scribe-2024.html`
   - SHA-256 `6122a2ee70cb5748992d93a301a72fc2c76608e3bf2c99dabd732a99595f8815`
3. **E-KDCD-10 云盘年份条件/导出对象**：<https://digprjsurvey.amazon.com/csad/help/node/TJE2UYmdw0ppUuR3Rs>，末尾将连接/分享到云盘列为2025及以后Scribe；中部说文字预览校订不进入可搜索手写PDF。
   - 原件`work/kindle-controls-deepening/10-share.html`
   - SHA-256 `c43bef903d02751ec9c0d8fe658b6de744242c11c23998fc9afadf481de5ccd4`
4. **E-KDCD-12 云盘生命周期**：<https://digprjsurvey.amazon.com/csad/help/node/TIh9JOMGAKr7bY4zqu>，FAQ 12–14重复/只读，20–22断开/重置。重复导入创建副本，原笔记保留；移除连接不删除已导入Kindle云库内容。
   - 原件`work/kindle-controls-deepening/12-cloud-drive.html`
   - SHA-256 `832bd182fb28ffeafa6daac2a72b9f66ffb85aeec6d18a847ac47fdf830e1b7f`
5. **E-KDCD-03 删除恢复**：<https://digprjsurvey.amazon.com/csad/help/node/TyR30Qzqu0hGzFbFVx?theme=light>，Remove download / Permanently delete：前者保留默认封面供重新下载，后者全设备删除且不可恢复笔记本。原件`03-delete-notebooks.html`。

所有14份原件哈希见evidence和fetch-results。对支持网页未给可靠更新日期者不填推测日期。

## F07 before/after 要点

全部5字段的逐字前后值见`existing-coverage-updates.csv`：

- `task`：从云盘导入再带回 → 从云盘导入再带回（2025+ Scribe）。
- `evidence_ids`：E-KDL-08 → E-KDL-08;E-KDCD-10;E-KDCD-12。
- `known_branches`：补同账号扫码连接、导入历史、重复副本、密码只读、断开/重置与云库保留。
- `gaps`：保留重复导出命名/覆盖、并发、失效授权/导入失败恢复；注明不套2024、USB不能回传。
- `checked_on`：2026-10-26 → 2026-10-27。

F01另有口径修正：原coverage写可标记掌握；本轮只确认指南Learning有统计，未见标记/撤销动作，不据此补按钮或间隔复习。

## 命令和退出

`cat AGENTS.md README.md docs/requirements.md`、读case/template、`rg`定位旧E/F及碰撞、Python基线/CSV读取均exit 0。公开web搜索与正文读取成功；未使用搜索中无关第三方资料补事实。

- `timeout 75s python3 -u work/kindle-controls-deepening/fetch-pages.py` → **exit 0**，初批13×HTTP 200。
- 补抓第14篇使用同脚本的`fetch(('14','download-collections',官方URL))`公开只读函数，在inline Python执行并追加`fetch-results.json` → **exit 0 / HTTP 200**。无401/403/429，没有失败端点重试。
- 初次requests按默认编码解码2024手册出现弯引号乱码，**原HTML字节未变**；用标准库HTMLParser从原件UTF-8离线重提取13份txt/links，exit 0，没有重新请求。脚本已固定UTF-8并加入main guard。
- `python3 work/kindle-controls-deepening/write-batch.py` → **exit 0**。
- `python3 work/kindle-controls-deepening/compose-case.py` → **exit 0**。
- `python3 work/kindle-controls-deepening/check-batch.py` → **exit 0 / PASS**。
- `git diff --check -- docs/research/interaction-library/cases/M-001-kindle.md docs/research/interaction-library/batches/kindle-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/kindle-controls-deepening-coverage.csv docs/research/interaction-library/batches/kindle-controls-deepening-inventory.csv` → **exit 0**。这4项当前untracked，故该命令不代替实际文件检查。

实际检查覆盖：E唯一及碰撞、F09连号及case9个任务、58项锚点真实存在与父子计数、所有引用E存在、相对链接存在、原件SHA-256、14次200、官方2025+条件在原文、8旧F/34字段before值匹配、LF且无字面量反斜杠r/n、旧reader-evidence/coverage哈希未变。全部正式文件哈希在check-results.json。

## 保留限制

在线手册不是固定固件；2024现行文档含后续更新能力，不倒推出厂。Downloaded筛选措辞在指南中有内部不一致，兼容表与Sync帮助的跨端手写范围仍矛盾。没有掌握误标撤销、删除确认/回收站、未同步笔迹保护、空间不足真实提示、云盘重复导出覆盖、下载失败及冲突全过程。目录中的音频/家庭/无障碍/设置/AI工具仍有资料可继续；本批不将其冒充已拆任务。
