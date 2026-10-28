# 本轮命令与文件读取记录

全部在 /home/workspace/reader。工具读取和脚本只涉及公开文字及本批文档，不执行App。未运行任何旧Toutiao/Huawei等批次脚本。

## 执行命令（实际执行）

- `python3 work/ireader-controls-deepening/capture.py`：exit 0；普通GET 1次，HTTP200；captures.json给最终URL/字节数/散列。
- `python3 work/ireader-controls-deepening/build.py`：初次、F06措辞修订后、案例资料状态措辞修订后共3次，均exit 0；只写本case、本批及work。
- `python3 work/ireader-controls-deepening/check.py`：初次、F06修订后、案例资料状态措辞修订后共3次，均exit 0/245 PASS；第3次为冻结前结果。
- `sha256sum docs/research/interaction-library/cases/M-014-ireader.md docs/research/interaction-library/artifacts/ireader-controls-deepening/source-notes.md docs/research/interaction-library/batches/ireader-controls-deepening-inventory.csv work/ireader-controls-deepening/existing-coverage-updates.json work/ireader-controls-deepening/existing-evidence-additional-captures.json work/ireader-controls-deepening/existing-evidence-field-updates.json`：exit 0；该次在F06修正前，最终散列以manifest.sha256为准。
- 内联`python3`只用于保存case/6F/4E和旧批散列基线、提取HTML版本历史、生成本记录/清单；均exit 0。版本提取按TitledParagraph的primarySubtitle和secondarySubtitle，不运行网页脚本。

## 只读命令及范围（exit 0）

- `cat AGENTS.md`；`sed -n '1,45p' README.md`；`sed -n '1,68p' docs/requirements.md`；`rg -n '掌阅|阅读|导入|听书' docs/ideas.md | head -n 25`。
- `cat docs/research/interaction-library/cases/M-014-ireader.md`；`rg -n 'M-014|E-IRD' docs/research/interaction-library/coverage.csv docs/research/interaction-library/evidence.jsonl`；`rg -n 'E-IRCD|M-014-ireader.md|F-M014-' docs/research/interaction-library/batches | head -n 70`。
- `rg --files work docs/research/interaction-library/artifacts | rg 'ireader|ird-|M-014|huawei.*15939571|15939571'`；查原件/历史记录，未重抓挑战页。
- `cat work/chinese-reader-deepening/ireader.md`；`cat work/chinese-reader-deepening/ireader-2.5-search-result.txt`：属另一硬件案例的旧工作材料，只用于排除混型，不引为手机证据。
- `cat work/chinese-app-depth/fetch-log.json`；`head -c 1400 work/chinese-apps/ireader-home.html`；`cat work/chinese-apps/ireader-huawei.txt`；`sed -n '1,230p' work/chinese-app-depth/ireader-privacy.txt`：其中长输出截断，相关论断另由下列定点命令核实。
- `nl -ba work/research/extension-manuals/huawei-15939571.txt | sed -n '1,245p'`：完整FAQ正文；`sed -n '1,105p' work/chinese-app-depth/ireader-privacy.txt`：基本/扩展业务正文；`rg -n '导入|笔记|书童|更新|同步|朗读|备份|听书' work/chinese-app-depth/ireader-privacy.txt`。
- `sed -n '100,270p' work/ireader-controls-deepening/app-store.txt`；`nl -ba work/ireader-controls-deepening/app-store.txt | sed -n '100,144p'`；`rg -n '8\.6|8\.5|8\.4|书童|导入|听书|修复|AI|朗读|精读|笔记|版本|iOS|iPadOS|更新' work/ireader-controls-deepening/app-store.txt | head -n 100`。
- `sed -n '1,170p' work/toutiao-controls-deepening/capture.py`；`sed -n '1,100p' docs/research/interaction-library/batches/toutiao-controls-deepening-inventory.csv`；`cat work/toutiao-controls-deepening/existing-evidence-additional-captures.json`；`sed -n '1,145p' work/toutiao-controls-deepening/existing-coverage-updates.json`：只读格式参考，没有执行旧批代码。
- `nl -ba docs/briefs/022-ai-actions-and-resume-conditions.md | sed -n '1,200p'`：只读复核第33/41行掌阅范围，无修改。

## 失败与恢复

- 第一次基线内联命令误用`python`，shell报command not found（该子命令127）；同一工具请求后续读取成功，聚合exit显示0。随后立即用`python3`重新创建并验证基线；未安装解释器。
- 一次apply_patch试图同patch Delete/Add同一case，被校验拒绝：multiple operations target，未改文件。随后写work/case-after.md，由本批build复制到case。
- 公开web检索14条查询、一次AppStore open，查询详见acquisition-log.json；zhangyue.com返回robots禁止，未绕过。没有本轮HTTP401/403/429；旧主站script challenge只读原件，不再次请求。
