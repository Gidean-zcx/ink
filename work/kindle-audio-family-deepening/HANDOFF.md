# 第二十六批已合并：Kindle / Kompakt / Khan Kids

2026-10-27。本批已完成资料整理与合并，整体目标仍active，全部65组部分覆盖。没有进入产品设计、设备操作或账户变更。

## 一次性脚本状态

根acquire.py、build_batch.py、merge_reviewed.py、update_summaries.py全部已执行一次且exit0，禁止重跑。两helper的before检查已在合并前PASS，禁止合并后重跑。根before内联检查也已完成。

已合并39新E/19新D2、17旧F54字段、4旧E追加、7审计字段；新看静态图13幅，其中根11、Khan2；无新D4。主表831F/1382E，737D2/79D1/15D4，1219不同URL，176静态E/18网页操作E。requirements不变。

根formal-manifest.json含17正式文件；helpers分别work/mudita-app-controls-deepening/HANDOFF.md和work/khan-library-controls-deepening/HANDOFF.md。精确before/after、旧批备份链和散列见applied-reviewed-updates.json；verify_batch.py可作合并后核查，不是before检查。最近verification-result.json为PASS：132冻结文件、66链接CSV/6578行引用有效。

## 来源与范围

根新公开GET清单及SHA在captures.json，4HTML+11PNG全部200；guide SHA与旧E-KDCD-02相同。新E-KDAF-01–03为AI/儿童档案/TTS帮助；04–10为同指南派生静态图。observe通过工具view_image实际查看，无连续过程。旧指南段落解析在guide-sections.json，独立短摘记在正式artifacts/kindle-audio-family-deepening/source-notes.md。

根检查了Mudita的录制/回放/定位及Khan删除/报告/中文指南的官方正文；Khan两图也复看。官方地区/型号冲突、5%可播和整书离线、TTS/读屏/有声书、Done/X的不同作用均保留，恢复和学习效果未补造。

根主要准确命令：

```sh
python3 work/kindle-audio-family-deepening/acquire.py
python3 work/kindle-audio-family-deepening/build_batch.py
python3 work/mudita-app-controls-deepening/check.py
python3 work/khan-library-controls-deepening/check.py
python3 work/kindle-audio-family-deepening/merge_reviewed.py
python3 work/research/interaction-depth/merge_batches.py
python3 work/kindle-audio-family-deepening/update_summaries.py
python3 work/kindle-audio-family-deepening/verify_batch.py
python3 work/research/interaction-depth/check_docs.py
git diff --check
```

上述各程序本身均exit0。一次组合shell末尾ls查询Khan尚未生成的HANDOFF时exit2，后续待正式交接取得；无重试失败研究路线。首次verify目录计数漏掉不含inventory字样的一份index，修正为正式目录全部链接后为66/6578。收尾apply_patch及定向Python仅改文档措辞，没有修改冻结来源。

## 入口与下一轮

本期docs/briefs/026-audio-controls-and-research-reuse.md；新docs/research/interaction-library/design-reference-guide.md按设计问题指向F/E。新使用入口和简报已由helpers只读复核，并采纳两处范围收窄。最新文件/链接结果见本work的final-doc-check.json。

整体继续研究，勿宣告逐产品每个功能都已拆完。优先核心失败/恢复和未读正文，不重复扩写已经明确的目录；无实机结果时按文档/静态证据限定。除必要检查外，无需重跑本批采集、生成和合并。
