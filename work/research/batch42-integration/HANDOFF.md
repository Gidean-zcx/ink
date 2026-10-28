# Batch42 handoff

2026-10-27。正式文档已整合并由root核验。不要重跑 `integrate_once.py`；首个assert防止重复，但这是一次性迁移。

- Brief: docs/briefs/042-math-feedback-and-research-depth.md
- New batch: duolingo-math-interactions（4E、1新F-M031-20/D4）
- Old F-M031-16/18在duolingo-controls-deepening原coverage中各追加数学范围。
- 全库：65 partial、930F=834D2+77D1+19D4、1635E/1467URL、221静态E/20browser E。
- Check:249 Markdown/2016 local links/errors[]；RP49条（新增RP-M031-01..11）。
- 56正式文件变化：42new/14modified/0deleted；final-manifest.json保留全变更散列，requirements SHA不变e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087。
- 此前before采用稀疏快照：全部正式文件有hash，仅复制文本/元数据。不要再完整复制媒体；磁盘仅约百余MB。重复旧before媒体清理见duplicate-cleanup.json，正式资料未删除。

## 数学实际操作

公开匿名英语Web，Grade2研究选择。83状态/58命令/27正式截图，浏览器session80264已quit、exit0。
首段故意错答后换题，第二题选中未提交退出，直接回引导；重进新题/空进度。
第二段15首次题面含2错，17提交（2个课末Previous mistake回顾），20XP/88%。计算器与作答分离；选择/拼块/键入/两种数轴均有记录。一次快速拼块实际仅输入5+，没有0，原因未知；不能写成5+0被错判。待提交拼块可点回候选，另题2+3正确，回顾5+0正确。
成绩后到连续目标设置，71截图过场/选项仅DOM；未提交目标，以免重试前次Chess已403的设置路线。普通页面刷新课URL进入新题/5心，未答X回路径；路径刷新仍首节点勾选、1天/505宝石/5心/任务10/10。未跨进程或身份恢复。
正式网络URL对匿名用户/会话路径另脱敏，raw work保留原hash；380x200、35x404、21x204、3x302、1x422。422发生引导阶段用户PATCH，字段未知不称偏好已保存。无401/403/412/429。537requestfailed=520CSP+17aborted，不等于挑战或答题失败。三次定位超时如实保存。
静态图、文字DOM和真实操作分开；正文漏拼块/数轴值、含占位数字，原图已核，不能只读DOM猜答案。

## 其他来源及限制

三篇官方math/maths-engineering/music正文已读，HTTP200，各一次匿名GET，未看动图；标题/正文历史范围与2026页面分开。Music只范围发现，无核心链；Math Coach/跳主题只是当前FAQ宣称未操作。
Khan本地13图片标签无真实URL、无图；正式child-core-evidence-audit仅复查记录，不新造E/F。不重试前轮403栏目。洪恩/DragonBox仍缺当前同题错误→提示→重试→完成→重进。

所有产品仍partial，长期目标未完成，不能mark complete。没有修改需求、产品代码或进入全系统设计。后续按backlog实际缺口继续，不重复将旧图片/正文计新增功能。
