# M032 粉笔第29批交接（冻结）

完成本次有限批次：3新E（E-FBPC-01—03）、0新F，仍10F/5D1/5D2；原F04与F09合计11字段精确建议；E-FBCD-08一次本地复读追加采集；21项功能目录映射，其中9项是旧帮助目录入口，并非已完成任务。只新增1张实际看过的静态入口图，无实操、无连续观察。

## 根可直接复核的核心材料

1. E-FBPC-01原URL：https://jingyan.baidu.com/article/d2b1d102e627605c7e37d4e0.html 。作者wenan一个人的文案、页面更新2019-01-12，科目/地区/系统/构建未注。`raw/c05-wrong2019.txt`第63行是作者称答对移出，65行是错答看正确答案及解析，67/69行为考点来源/笔记。完整原HTML保存在同名.html；**不保证现行自动移出，不升级F04**。
2. E-FBPC-02原URL：https://jingyan.baidu.com/article/6dad5075034816e023e36ef8.html 。`raw/c06-quick2022.txt`41行页面更新2022-04-12；48—50行为作者自报荣耀30、magic UI2.0.0、6.16.25；53/55/57行入口/选项/交卷，59行明确未答完出现提示后选择确定。**没有弹窗原文、取消结果、提交后报告；科目/地区未注明，全部图片未看**。与原F09的2019国考iOS6.8.8图分开记录，不能合成一个会话。
3. E-FBPC-03原URL：https://jingyan.baidu.com/article/ca00d56c28bc50a89eebcff4.html 。`raw/c03-baidu.txt`41/47—49行是2022-12-21、作者自报小米11/MIUI12.0.0.1/8.0.1；52/54/58行是行测错题菜单、做题时间与题型。8.0.1未官方核定，不构建版本序列；未见筛选结果。

原HTML都是匿名普通GET200响应。上述“原创/真实经历”是页面作者自述，未独立认证；日期均为页面“更新”，不是实操或版本发布日期。正文摘记和未决明确保留。

## 图片分工与拒绝路径

仅查看E-FBPC-01方法1的入口图，`historical-menu-entry.jpg`：四方块被红框圈出、底部练习/课程/发现/我；主体被作者模糊，科目读不清。**没有看错答、移出、确认弹窗图**。随后方法4图片403，停止整个exp-picture.cdn.bcebos.com图片路线，既未改参数又未换路重试。失败响应101字节保存在work，其.jpg后缀不表示可用图片。

Bilibili cv33546391先有web工具抽取正文、后续工具open为空，普通GET412后停止。其余两个产品分析、套卷下载打印、智能组卷均只做正文范围排除，不升任务。8个候选完整地址/状态/SHA及采用理由见candidate-register.json；无更多候选。

## 修改/新增文件

正式7文件：
- docs/research/interaction-library/cases/M-032-fenbi.md
- docs/research/interaction-library/batches/fenbi-practice-controls-deepening-evidence.jsonl
- docs/research/interaction-library/batches/fenbi-practice-controls-deepening-coverage.csv（标准表头，0行）
- docs/research/interaction-library/batches/fenbi-practice-controls-deepening-inventory.csv
- docs/research/interaction-library/artifacts/fenbi-practice-controls-deepening/source-notes.md
- docs/research/interaction-library/artifacts/fenbi-practice-controls-deepening/static-observation-log.md
- docs/research/interaction-library/artifacts/fenbi-practice-controls-deepening/historical-menu-entry.jpg

工作文件都在work/fenbi-practice-controls-deepening/；完整清单与SHA见manifest.json。只改本案例和本批文件，未改master、旧批、README、requirements、ideas、brief或总索引；产品/账户状态未变。未重跑其他批脚本。

## 合并补丁

- `existing-coverage-updates.json`：数组；每项`flow_id/source_batch/changes{field:{before,after}}/replacement_row`。F04改task/evidence_ids/known_branches/gaps/visual_status/checked_on共6字段，仍D1；F09改task/evidence_ids/known_branches/gaps/checked_on共5字段，仍D2。F04的static_reference_only仅指菜单图，非错答反馈。
- `existing-evidence-additional-captures.json`：数组；E-FBCD-08的`additional_captures`精确before_exists/before/after/append，本地复读旧kb-50542.html，不称新网络快照；其他字段不动。
- `baseline.json`与`case-before.md`保留完整20E/10F和修改前案例SHA。旧E-FBCD-07的visual_audit引用独立static-observation-log.md，SHA未变，无case审计依赖迁移需求。

## 检查与冻结

合并前只读命令：`python3 work/fenbi-practice-controls-deepening/precheck.py --manifest`。检查源码不写文件、不联网；验证before、仅增采集、全部SHA、F/E引用、旧视觉审计、21目录与拒绝后停止。首次不带manifest为exit0/487 PASS；最终带manifest结果见precheck-result.json（由shell重定向记录）。根合并后不要再执行依赖before的检查或write-batch.py。

`git diff --check -- docs/research/interaction-library/cases/M-032-fenbi.md docs/research/interaction-library/batches/fenbi-practice-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/fenbi-practice-controls-deepening-coverage.csv docs/research/interaction-library/batches/fenbi-practice-controls-deepening-inventory.csv docs/research/interaction-library/artifacts/fenbi-practice-controls-deepening`：exit0。完整实际命令、两次早期本地脚本错误（KeyError与SyntaxError，修正后正常）及HTTP结果见commands.md；网络412/403是成功记录的停止结果，未绕过。

冻结关键SHA256：
- case：bc1a0278d87013d59c964e4c8e33355e5945e6a2f4f65f83f0ec4f6060d7f1c1
- source-notes：b4c7d9ee806473d7b10e6692f60b013e5555c94ef670f780331a6cbc0be154e3
- E-FBPC-01 HTML：fdc4e0225fa02fe2dfeff8bd11f03840544265918e75fc97f14ef4b3635e511c
- E-FBPC-02 HTML：edc91a8b95057d4d6ab839ea8db4cd9f0826146d03e2a7bc421961eb95d8bb2d
- E-FBPC-03 HTML：20cf76d540141672c1f5cbdf1903bbe66bfe509cdcd63c87467d74e8c7644e3c

现行行测改选/交卷确认取消/超时/后台断网草稿、清理恢复、题目修订后的记录与统计依然缺可核资料。本轮实际收益是区分历史作者叙述、现行功能声明和真正视觉范围，并保存可复核局部分支；没有验证学习成效或完整产品覆盖。下一最小取证仍是授权固定构建短卷及错题清理/恢复连续记录，未执行。
