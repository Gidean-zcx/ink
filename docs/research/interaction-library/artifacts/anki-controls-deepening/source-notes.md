# M-020 Anki Desktop 控制与恢复补证

查阅 2026-10-27。仅公开文本取证，没有安装、启动 Anki、使用账户、执行同步或查看图片/视频。现行手册未标统一修订日；版本门槛与历史发行说明分别保留。原件、HTTP 状态、SHA-256 在 `work/anki-controls-deepening/fetch-log.json` 和 `fetch-extra-log.json`；工作目录不是长期原件归档。本文是短摘记，不是整页译文。

## 新来源

| E | 来源与定位 | 本轮用于什么／限制 |
| --- | --- | --- |
| E-ANCD-01 | [Checks and Errors](https://docs.ankiweb.net/manual/templates/errors.md)，开头、Basics、Specific Issues | 2.1.45+保存类型/导出时检查；从 Browse 找报错类型进入 Cards，修正字段引用与条件配对。空正面、单张/全部 cloze 空卡要区别；批量删除前核清单。未验证任意模板错误或取消后的保存。 |
| E-ANCD-02 | [Managing Files](https://docs.ankiweb.net/manual/files.md)，Checking Your Collection、User Data、Startup Options、Corrupt Collections | 数据库检查会重建标签；清闲置标签不随集合同步。安全启动同时停自动同步和插件，可用于保护另一端未受损副本。直接移动运行中的集合有损坏风险。未执行 SQLite 修复，也未把后台优化等同完整检查。 |
| E-ANCD-03 | [2.1.40–49 发行说明](https://docs.ankiweb.net/releases/changes/changes/2.1.40-49.md)，2.1.45 / Undo handling；2.1.46 | 2.1.45 于2021-07-30引入多数动作的多步撤销及重做；该类动作即时保存。Check Database及部分插件操作清空撤销历史；不是每个动作都能恢复。2.1.46修复Redo菜单动作名称显示；不外推当前菜单视觉。 |
| E-ANCD-04 | [v3 Scheduler](https://docs.ankiweb.net/faqs/the-2021-scheduler.md)，Enabling、Changes / Undo、Burying | v3允许回答与暂埋等交错后逐步撤销；Desktop23.10起只用v3。跨日学习卡可被暂埋，这比 Studying 的笼统“不暂埋学习卡”更细，限定调度器解释。未证明跳过中间历史的定点撤销或跨重启Redo。 |
| E-ANCD-05 | [Sound/image media files have gone missing](https://docs.ankiweb.net/faqs/sounds-or-images-have-gone-missing.md)，全文 | 查缺失列表→检查同名附件→从未清空回收站复原；共享包仍可取得时可导入隔离档案再复制媒体。删除可能传播到别端；客服可能恢复近期云端文件只是条件，不是承诺。 |
| E-ANCD-06 | [23.12 Release](https://github.com/ankitects/anki/releases/tag/23.12)，Deck options screen、Other | 原HTML日期2023-12-25；记录自动推进加入及入口，另记录关闭卡片布局页时提供保存机会。未读取代码变更或据此写确认按钮文案。 |
| E-ANCD-07 | [维护者议题 #4228](https://github.com/ankitects/anki/issues/4228)，dae，2025-07-25首帖 | 维护者解释失焦停自动推进是为了避免分心时意外评分。这是当时机制说明与变更请求；构建未给出，不能保证查阅日版本仍相同。其他用户评论不作为官方事实。 |

## 旧 E 复采而不重复编号

- E-AND-01：重新取得 `llms.txt`，字节哈希与旧件相同；Desktop Manual 的55个页面入口含平台父子项，不是55项完成任务。AnkiMobile、开发者目录不混入。
- E-ANK-02：重新取得 Syncing Markdown；关注单向覆盖前取消、逐顶层牌组导出再合并，及媒体同步独立。正文给出的 Network 设置名需按构建核对；不是UI观察。
- E-ANK-03：重新取得 Backups；自动备份/即时备份不含媒体。Deletion log按档案建立版本区分：**晚于25.09.2创建**的档案可带类型列，不能只看当前安装版本。
- E-AND-12、15、17：Deck Options、Media、Profiles复采；分别补自动推进证据缺口、缺失媒体恢复边界、逐牌组合并及降级失败路径。Media同页对工具垃圾箱和系统回收站的措辞仍不一致。

追加采集建议只在本批work中，未更改上述旧E。其他旧模板/浏览器文本是重新阅读已保存原件，不伪称本轮新抓取。

## 读取但不计作新任务/独立证据

Miscellanea已读菜单/调试控制台正文；未执行控制台，未建新F。Synchronizing multiple profiles FAQ与Profiles同结论，不为数量重复建立E。23.12+迁移页只指向GitHub发行说明。PR2765仅读公开讨论（2023年设计意图及迭代问题），未以讨论中的待修状态证明最终行为，未读取源码/下载附件。搜索命中的AnkiDroid、第三方教程、仓库翻译字符串没有用来填Desktop界面空白。

Web读取器对misc/errors失败，但普通公开Markdown GET均HTTP200；不是401/403/429后绕过。全部16次原件请求200。首次本地命令因`python`缺失退出127，改`python3`；第二脚本因bs4缺失退出1，改标准库HTMLParser，未安装依赖。

## 未解决

学习页Undo/Redo具体入口与快捷键、撤销后呈现哪一面、同步/退出后的撤销栈、自动推进停止/重进/音频组合、媒体垃圾箱期限与清空后恢复、档案删除确认与回收、错误模板删除范围与取消，仍需固定构建或更完整官方材料。旧页文字不能证明实际恢复成功。
