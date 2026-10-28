# 第十九期：阅读队列、内容版本与 AI 动作

2026-10-27。本轮深化Readwise Reader、Kobo Libra Colour与不背单词。重点是成人阅读节奏、资料更新、内容组织与AI能执行什么；资料仍是公开文档和静态图，没有登录或设备操作。

## 连续阅读需要可选择的节奏

Readwise把“可能以后读”和“现在准备读”分开：Shortlist配置让新内容先进入Later，再选入近期清单；另一种Triage配置则从Inbox逐条处理。切换配置若会移动文档，界面先提示确认。值得注意的是，同名Shortlist也可能只是标签形成的视图，不能一律当成同一种存储位置。[配置指南](https://docs.readwise.io/reader/guides/workflows/library-configuration)

阅读中归档或移动文章，默认返回列表，也能配置自动进入下一篇。移动端可分别选择短文与长文工具栏；长文视图减少底栏动作，启用Ghostreader后可选Modern布局，提供AI入口。[导航帮助](https://docs.readwise.io/reader/docs/faqs/navigation)、[外观帮助](https://docs.readwise.io/reader/docs/faqs/appearance)

对成人版的启发是：收藏、近期阅读队列、阅读中的下一步可以分别设计。“连续”不必只有无限滚动一种形式。哪种节奏更利于专注和掌握仍需验证，这不是已确定的产品方案。

## AI更新资料之前，要知道原文和批注的关系

Reader保存的是文章当时的内容；同URL再次保存会去重，不保证刷新正文。官方给出的更新方法是删除后重存，并明确关联批注也会删除。因而“把新闻更新一下”可能影响已有知识记录。[解析帮助](https://docs.readwise.io/reader/docs/faqs/parsing)

PDF原版与文字视图也有独立显示条件：某一视图里的高亮可能不叠显在另一视图，但仍能在Notebook找到。不能仅按屏幕上是否看见来判断笔记是否丢失。[PDF帮助](https://docs.readwise.io/reader/docs/faqs/pdfs)

这些证据支持后续讨论内容版本、批注锚点与更新结果的表达；目前尚未决定存储架构、同步协议或迁移策略。

## 按钮和工具名称不能代替执行结果

Reader邮件中的Unsubscribe会屏蔽发件人，包括转发来的邮件，而不是退出原服务的邮件名单；想再次接收需要另走恢复途径。公开Bundle也不自动跟随视图增加内容，手动刷新会把新增文档加入此前已将该Bundle添加到Reader的用户账号。[邮件帮助](https://docs.readwise.io/reader/docs/faqs/email-newsletters)、[分享帮助](https://docs.readwise.io/reader/docs/faqs/sharing)

其原生Global Ghostreader与外部AI连接还需要分开研究：前者有工具权限和会话中的确认卡；后者的官方文档列出Reader与Readwise的工具用途，但本轮没有取得调用schema、授权结果或执行回包。推荐指南给出的“推荐并整理”提示不能当作已经验证的完整链。[Global指南](https://docs.readwise.io/reader/guides/ghostreader/global)、[工具范围](https://docs.readwise.io/tools/mcp)、[推荐工作流](https://docs.readwise.io/reader/guides/workflows/recommendations)

这与G-002直接相关：AI要理解动作对象、作用范围和实际结果。让AI看到按钮、让它发现工具、让工具成功改变数据，需要分别核对。

## 本轮可查回的材料

| 对象 | 新的参考依据 | 继续保留的缺口 |
| --- | --- | --- |
| [Reader案例](../research/interaction-library/cases/M-015-readwise.md) | 19篇正文、五张实际查看的静态图；阅读配置、保存/更新、制卡入口、PDF/字幕、邮件、分享包及外部AI范围 | 当前App实操、错误/取消/部分成功、问答卡后续练习、同步冲突；外部工具示例保持声明级别。 |
| [Kobo案例](../research/interaction-library/cases/M-002-kobo.md) | 合集创建/编辑/删除、页书签回查，目录/显示/听书补证；两张静态图 | 刷新下拉完整档位、合集冲突、书签重排锚点、音频中断与重启恢复。 |
| [不背单词案例](../research/interaction-library/cases/M-018-bbdc.md) | 核对版本说明、每组词量与政策记载的入口；资料覆盖逐项列出 | 完整同题纠错、评分/复习/退出恢复未取得，不凭更新日志宣布已修复或流程完成。 |

Kobo静态设置图虽然显示刷新选择值Chapter，但没有展开完整列表，也未标设备固件，不能当作N428默认值或实测效果。[原帮助](https://help.kobo.com/hc/en-us/articles/21355003278615-Reduce-the-rainbow-effect-when-viewing-colour-content)

所有资料继续通过[横向问题](../research/interaction-library/comparison.md)关联到稳定任务与证据，目录另有[逐项台账](../research/interaction-library/inventories/README.md)。全部已知产品仍是部分覆盖，未进入全系统设计。
