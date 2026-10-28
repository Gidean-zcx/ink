# 第十八期：关闭页面之后，内容和状态发生了什么

2026-10-27。本轮补 TIME for Kids 的实际网页操作、Astell&Kern SR35 的控制流程和有道普通2.0的资料缺口。研究仍未覆盖每个产品的每个功能；下列观察、文档结论与本项目建议分别说明。

## 词卡关掉了，朗读可能还在继续

在 TIME for Kids 一篇2018年G3–4文章中，点击预标词会展开含释义、图片、例句和音频的卡片。匿名桌面浏览器的两组操作记录显示：播放portrait的词音后点X，词卡隐藏，底层媒体仍在推进；重新打开时接着播放。点击音频暂停后才停住；本次刷新并重新打开则回到起点。Escape和卡片外点击也没有关闭本次词卡。[公开文章](https://www.timeforkids.com/g34/food-for-thought/?rl=en-760)

这些是实际界面与媒体状态观察，没有听音，也不是5–6岁儿童或墨水屏触控测试。网页同时记录到加载与媒体中止错误，尚未定位原因，不能把一次能播放当作整条播放链可靠。截图、动作和状态对照见[操作审计](../research/interaction-library/artifacts/time-vocabulary-deepening/browser-operation-audit.md)。

对本项目的启发：AI说“已经关掉”之前，需要辨别关闭界面、暂停声音、结束任务和清除进度各自改变什么。哪些动作应该联动，仍待产品讨论。

## 找到了题库链接，还要确认题目依据

同篇文章链接的纸面测验有10道选择题和一道开放延伸题，其中一题涉及的人名没有出现在本轮取得的网页正文中。测验要求依据当期封面故事，题头刊期与网页发布日期也不同；因此不能断言试卷错误，但只导入当前网页还不足以回答全部题目。答案另需教师指南，本轮未取得。[配套测验](https://www.timeforkids.com/wp-content/uploads/2018/02/edition-3-4-cover-quiz-022318.pdf)

这对“AI帮我找资料并导入题库”是直接参考：文章、完整刊期、题目、答案和解析应保留各自来源与对应关系。取得链接、取得完整材料、可以开展练习，是不同阶段。这是研究建议，没有预先决定题库架构。当前网页也没有因此被认定具有自动评分、错题本或掌握记录。

## 设置可见、处理完成、实际生效也要分开

SR35手册将ReplayGain的选曲校准、完成标记和启用开关分开；型号支持页所链的共用FAQ另说明Line Out时不生效。EQ设置则留下一个具体冲突：正文要求点SAVE，配图却显示Rename EQ和CHANGE。资料库保留差异，没有自行补出“保存成功”的操作。[官方手册](https://cdn.astellnkern.com/bbs/download/202305/20230503165340_8b122109-9c25-4ef8-8ecc-69290747288f.pdf)印刷41、62–63页；[ReplayGain FAQ](https://www.iriver.jp/support/faq_1104.php)。

本轮只读文档、查看静态图，没有设备操作或听感验证。其参考价值在于：AI需要知道准备数据、执行处理和打开效果开关之间的区别，不能仅看到“已开启”就报告目标效果已经实现。

## 已整理成可查回的任务与缺口

| 后续设计问题 | 资料入口 | 当前证据边界 |
| --- | --- | --- |
| 点词、听词、关闭后如何继续？ | [TIME案例](../research/interaction-library/cases/M-047-time-for-kids.md)，F-M047-03 | D4仅本篇词卡分支；分级切换、课堂分配与词本练习未完成。 |
| 年级、主题、分页是否保持上下文？ | 同案例F-M047-01，83行[目录](../research/interaction-library/batches/time-vocabulary-deepening-inventory.csv) | 实际进入K–1/Science和第二页并返回；37主题只列目录，未逐项体验。 |
| 收到题目后能否直接开始练习？ | 同案例F-M047-05 | D2纸面材料检查；不是在线作答或自动评分。 |
| 音频设置如何准备、保存、共享和生效？ | [SR35案例](../research/interaction-library/cases/M-065-astell-kern.md)，F-M065-17/18及202行[目录](../research/interaction-library/batches/ak-directory-deepening-inventory.csv) | 旧手册与V1.22基线分开；EQ保存、校准失败和恢复仍有缺口。 |
| 扫词之后如何校订、收藏、复习和恢复？ | [有道普通2.0案例](../research/interaction-library/cases/M-025-youdao-pen.md)，20行[覆盖台账](../research/interaction-library/batches/youdao-controls-deepening-inventory.csv) | 本轮未取得更强流程证据，未新增任务或升级深度；8条探索记录另存。 |

本批新增3条文档任务，补回6条既有任务；其中一条仅词卡分支从声明升级为实际网页操作。完整数量见[覆盖索引](../research/interaction-library/case-index.md)，所有65组仍为部分覆盖。目录项含父子项和重复来源，不是全部功能的分母。正式设计时从[横向问题](../research/interaction-library/comparison.md)进入任务，再查证据、适用版本和未解决分支，避免直接把“竞品支持”变成我们的设计结论。
