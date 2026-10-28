# M021 Quizlet 本轮交接

2026-10-27。有限批次已完成；root 已告知核心原文抽核成立。本子任务不执行合并。

## 数量与文件

- 新 E-QZCD-01～17：4 目录 + 13 操作正文。E05/E17 为旧 article 的补充，不是17篇新独立材料。
- 新 F-M021-11 Test、F12 AI Practice Test、F13 Progress/星标；case 共13F/30E，全部D2文档重建，无图像/实操。
- 目录68行：4父项+64文章入口；24项有部分任务依据，40项目录级，不等于完成率。
- 改 `docs/research/interaction-library/cases/M-021-quizlet.md`。
- 新 `docs/research/interaction-library/batches/quizlet-controls-deepening-evidence.jsonl`、`quizlet-controls-deepening-coverage.csv`、`quizlet-controls-deepening-inventory.csv`。
- 旧 F02/F04/F07/F08 共16字段（evidence_ids、known_branches、gaps、checked_on）逐项 before/after：`work/quizlet-controls-deepening/existing-coverage-updates.csv`；LF正常。原等级/visual/hands_on不变。
- 旧 dictionary 两批次 SHA 不变；未修改 master、汇总、其他case或旧批次。新正式 artifact 0；文档和工作文件有变化，外部账户/内容状态没有变化。

## 最重要原文定位

| E | 可复核来源 | 工作原件及位置 |
| --- | --- | --- |
| 09 | https://help.quizlet.com/hc/en-us/articles/360030990531-Studying-with-Write-mode | response-03；To study/Tips，工具L19–80；每题两次正确、Don't know、I was correct；parsed/articles-360030990531.txt |
| 10 | https://help.quizlet.com/hc/en-us/articles/360030645752-Studying-with-Spell-mode | response-03；L21–42；错字提示、重播/慢速、每词两次正确、网站限定 |
| 11/12 | https://help.quizlet.com/hc/en-us/articles/360048314692-Setting-up-a-study-path 与 https://help.quizlet.com/hc/en-us/articles/360048806511-Resetting-study-path-progress | response-02 L21–57 与 response-01 L19–41；设置后须开始才保存；重学删除Learn进度；Android范围分歧保留 |
| 13 | https://help.quizlet.com/hc/en-us/articles/360030642972-Studying-with-Test | response-01；L21–59，重点L33/45/57未完离开不保存；不能套AI Practice Test |
| 16 | https://help.quizlet.com/hc/en-us/articles/25946589648013-Studying-with-Practice-Tests | response-02；L25–40网站生成/提交/More、L49–61移动/字符条件；没有逐题题干答案编辑步骤 |
| 14/15 | https://help.quizlet.com/hc/en-us/articles/360048803491-Using-Progress-for-targeted-studying 与 https://help.quizlet.com/hc/en-us/articles/360031172312-Studying-most-missed-terms-with-stars | response-01 L17–34 / response-02 L19–49；跨活动对错统计与星标子集，不等于Flashcards会话位置 |
| 06 | https://help.quizlet.com/hc/en-us/articles/360030171011-Deleting-a-set | response-01；L17–43，自建集可删且不可恢复；隐藏动态另由E07支持 |

这里 response-NN 为 `work/quizlet-controls-deepening/quizlet-response-NN.txt`，parsed 也在同工作目录。`source-map.json` 和各 E 的 raw_work_path/raw_sha256 给出准确文件映射。L 是工具文本行号，不是网站源码行号。

## 获取限制

原件是五份公开 web 工具正文响应，不是源HTML。直接 requests 对Test地址唯一探针HTTP403（fetch-probe.json），已停止，不换头/接口绕过。网页工具原本公开可读；之后只使用此文本路线。没有图片、视频、登录、生成、作答或其他外部变更。页面修订日、App构建和平台DOM未知；平台单独特性只采用正文明确限定，不按IOS/Web/Android导航顺序硬配。

普通Learn中断、Study path Android范围、Test漏答/重考/历史、AI试题校订和超时续做、源更新/删除级联仍未知。Smart Assist、Import和草稿重复正文不增新E。目录新功能保持未分析。

## 检查命令与退出

- `python3 work/quizlet-controls-deepening/fetch-probe.py`：0（记录HTTP403，不是取得HTML）。
- `python3 work/quizlet-controls-deepening/prepare.py`：0（20个已保存公开页面）。
- `python3 work/quizlet-controls-deepening/write-batch.py`：0（仅运行一次；之后小改case）。
- `python3 work/quizlet-controls-deepening/check-batch.py`：0，PASS。
- `git diff --check -- docs/research/interaction-library/cases/M-021-quizlet.md docs/research/interaction-library/batches/quizlet-controls-deepening-evidence.jsonl docs/research/interaction-library/batches/quizlet-controls-deepening-coverage.csv docs/research/interaction-library/batches/quizlet-controls-deepening-inventory.csv`：0。
- 限定四文件的 `git status --short`：0，四者均untracked；因此另做文件级UTF-8/LF、编号、引用、before值、旧批SHA、来源关键文字及本地链接检查。
- 其他读取/查询命令与最初无碰撞rg exit1见research-log.md。没有未处理的脚本失败。

## 原始工具响应 SHA-256

- `work/quizlet-controls-deepening/quizlet-response-01.txt`：`c095e56ef47ce51866c89e3faa1c3f813740aa2f11e477a20a4ce3564240f299`
- `work/quizlet-controls-deepening/quizlet-response-02.txt`：`63bc95df37bf44568db39fa73f4c0b5b6c95789c103c798518c64dbf922a21e1`
- `work/quizlet-controls-deepening/quizlet-response-03.txt`：`9fbb94049e94716324f279fe49eee72d0f0fddb817039498de06b52d98ce9e83`
- `work/quizlet-controls-deepening/quizlet-response-04.txt`：`48d897399f6ff01f47e643352a006a06f9b13ad4cda13e222109ef67c70854da`
- `work/quizlet-controls-deepening/quizlet-response-05.txt`：`67ae43e3fa5f44b3a0dcab19506cdda8ad0786bd7a1b9ebc295d31109c30469b`

## 交付 SHA-256

- `docs/research/interaction-library/cases/M-021-quizlet.md`：`308d8ecb128bfda07e0456ce7f90e3d8fd0f9cbdaadbdec3e9e8051aa2d81bde`
- `docs/research/interaction-library/batches/quizlet-controls-deepening-evidence.jsonl`：`2d7c28dfb077c23c7ca3579c298034b02ccdebb253fb51e4982e5c9c3687b655`
- `docs/research/interaction-library/batches/quizlet-controls-deepening-coverage.csv`：`2f517ddcccab486ed7ff89cb4d543327fc1321986bd0b231ee990efb31555f72`
- `docs/research/interaction-library/batches/quizlet-controls-deepening-inventory.csv`：`070e3b9dcd5ea6b2145d38d465ffab1dddde70bdba73830b76c1b7ed5e9297d1`
- `work/quizlet-controls-deepening/existing-coverage-updates.csv`：`7e6c3e03179fb6acd4d4704d2c3948dbbec95b5fab7ffdd9eddc16e91fb493bd`
- `work/quizlet-controls-deepening/research-log.md`：`9719122676dcd991efaa4f43e3d6a84caa89cc497ea503f765a5a8d22048d81d`
