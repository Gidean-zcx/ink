# 本批命令与访问记录

工作目录均为 `/home/workspace/reader`；查阅日2026-10-27。以下记录实际执行的关键命令/工具及退出情况，不把HTTP状态当shell退出码。没有运行全项目检查或旧批脚本。

## 阅读与查重

- `cat AGENTS.md README.md docs/requirements.md`：exit0；长输出有截断，后续单独读取requirements前105行。
- `cat docs/research/interaction-library/cases/M-018-bbdc.md`：exit0。
- `rg --files` 后按 `bbdc|M-018|chinese-vocab-deepening` 筛已有材料；读取旧case、34项目录、两轮research-log/captures/source-notes，并用内联Python读取主表9E/5F：exit0。初始命令体没有独立脚本，不伪称已保存完整shell会话；基线实际记录在baseline.json/case-before.md。
- `rg -n '3548342|4425622|579227156|ae20220700000|testdaily.cn/3783' docs/research/interaction-library/evidence.jsonl`：无匹配；没有用它代替全库ID检查。E-BBLC新前缀和同URL最终由precheck检查。
- `cat work/bbdc-learning-controls-deepening/write-batch.py`、`cat work/bbdc-learning-controls-deepening/existing-coverage-updates.json docs/research/interaction-library/batches/bbdc-learning-controls-deepening-evidence.jsonl`：exit0。
- `rg -n 'visual_status|third_party_firsthand|source_type|additional_artifacts' docs/research/interaction-library/template.md scripts work/bbdc-learning-controls-deepening/baseline.json`：exit2，原因是不存在scripts目录；只读，无文件更改。随后 `rg --files -g '*check*' -g '*valid*' -g '!work/**' -g '!node_modules/**'`：exit0，没有运行检出的旧文件。
- `rg -n 'static_reference_only|visual_status' docs/research/interaction-library/README.md docs/research/interaction-library/template.md docs/research/interaction-library/coverage.csv`：exit0；确认沿用现有枚举，长输出有截断。

## 公开取证

web.search_query共9条字符串（3次调用），并不表示9篇候选：
1. 不背单词 记错了 拼写 回忆 学习 官方 教程
2. 不背单词 产品体验 回忆 知道 不知道 复习
3. 不背单词 学习 提示 重新 拼写 site:woshipm.com
4. "不背单词" "拼写" "提示" "产品分析"
5. "不背单词App" "不知道" "官方"
6. "不背单词" "记错了" "评测"
7. 不背单词 体验报告 "拼写" "完成" -site:php.cn -site:reddit.com
8. 不背单词 "Learn" "Review" "提示" 教程 -site:php.cn
9. 不背单词 "记错了" site:jingyan.baidu.com

6个候选的URL、原件、HTTP结果和排除原因见candidate-register.json。web.open搜狐候选返回内部400/Timeout fetching，无shell退出码，未改路重试。初批C01/C03/C04普通GET由内联Python执行，进程exit0；输入URL、最终URL、字节数、SHA保存在fetch-results.json。

- `python3 work/bbdc-learning-controls-deepening/fetch-selected.py`：exit0。C05/C06 HTML及C04三张必要静态图均HTTP200；每请求记录见fetch-selected-results.json。
- `python3 work/bbdc-learning-controls-deepening/fetch-detail-images.py`：exit0。C05两图HTTP200；C06首图HTTP403后停止该域，错误响应保留但未作为图片查看；见fetch-detail-images-results.json。
- `PYTHONPATH=work/research/tools/pymupdf python3 -`：内联PyMuPDF脚本提取6页论文文字并仅渲染索引3/第4页，exit0，出现fitz名称弃用FutureWarning。没有安装库；派生文件raw/c03-paper2022.txt及c03-paper2022-p4.png。
- `view_image` 实际查看raw中的c04-version-table.jpg、c04-flow.jpg、c04-study-ui.jpg、c05-first-word.jpg、c05-spelling.jpg、c03-paper2022-p4.png，共6次工具成功；无shell退出码。前5图正式归档，论文页仅work排除用。没有看少数派图片，没有音视频或产品操作。
- `nl -ba work/bbdc-learning-controls-deepening/raw/c04-review2021.txt | sed -n '90,107p;182,211p'`：exit0。
- `nl -ba work/bbdc-learning-controls-deepening/raw/c05-agirls2017.txt | sed -n '90,109p'`：exit0。
- `nl -ba work/bbdc-learning-controls-deepening/raw/c06-sspai2023.txt | sed -n '38,77p'`：exit0。

## 写入与预检

- apply_patch及内联Python只修改本case和本work。正式新文件由 `python3 work/bbdc-learning-controls-deepening/write-batch.py` 生成；冻结前执行3次均exit0（后两次保留旧缺口措辞、修正目录对应关系）。它不会写旧批/主表，但冻结后不要重跑。
- `python3 work/bbdc-learning-controls-deepening/precheck.py`：3次exit0/PASS；只读，约60行，检查3E/0F、2旧F8字段before、原件/摘记/审计SHA、链接ID/LF。仅合并前可运行，合并后before已变，不能重跑。
- `git diff --check -- docs/research/interaction-library/cases/M-018-bbdc.md docs/research/interaction-library/batches/bbdc-learning-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/bbdc-learning-controls-deepening-coverage.csv docs/research/interaction-library/batches/bbdc-learning-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/bbdc-learning-controls-deepening`：exit0。文件在本仓库为untracked，此命令不能代替原件和内容检查。
- `diff -u work/bbdc-learning-controls-deepening/case-before.md docs/research/interaction-library/cases/M-018-bbdc.md`：exit1，正常表示有文档差异；查看了完整差异，非脚本失败。
- `sha256sum docs/research/interaction-library/cases/M-018-bbdc.md docs/research/interaction-library/artifacts/bbdc-learning-controls-deepening/source-notes.md docs/research/interaction-library/artifacts/bbdc-learning-controls-deepening/static-observation-log.md work/bbdc-learning-controls-deepening/existing-coverage-updates.json work/bbdc-learning-controls-deepening/raw/c03-paper2022-p4.png`：exit0；最终文件以frozen-manifest.sha256为准。

状态：只改M018 case、新同名批次/正式artifacts/work；旧F/E建议未应用。无账号、安装、外部提交或产品状态变化。
