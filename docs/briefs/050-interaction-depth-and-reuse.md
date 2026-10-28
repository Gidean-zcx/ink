# 第五十期：交互细节与可复查依据

2026-10-27。当前还没有深入到每个产品的每个功能。65组产品全部部分覆盖，多数任务依据操作文档或固定源码重建；公开网页的有限实际操作不能代表原生App、手机或电子纸设备。资料数量不作为完成率。

本轮补两个例子，说明功能清单之外还需要研究什么：

| 表面功能 | 补到的交互差别 | 后续设计要核对的问题 |
| --- | --- | --- |
| AI记笔记、设待办 | 多应用默认目标会受最近访问影响；指定应用、追加对象和撤销条件有区别，后台Live动作不能沿用前台Undo | “记下来”到底写到哪，结果怎样回读，重复请求与取消会留下什么 |
| 手写、剪贴、撤销 | QUADERNO Gen.2写入即保存，但剪贴只可粘贴一次，翻页清空撤销；校准时抬笔才显示线 | 保存、临时状态、历史和界面反馈分别是什么，不能靠一个“已保存”概括 |

分别见[Gemini逐项摘记](../research/interaction-library/artifacts/gemini-notes-reminders/source-notes.md)与[QUADERNO明确代际的原页定位](../research/interaction-library/artifacts/quaderno-gen2-ink/source-notes.md)。本轮均为正文重建，没有实际执行；日历帮助限流、完整传出/恢复等缺口继续保留。

复用入口也作了实质调整：[交互依据表](../research/interaction-library/reference-points.csv)随行提供关联任务深度、是否操作、来源性质、取得方式及操作审计链接；它们从F/E自动回填。任务达到D4并不使该任务关联的每条文档也成为实操证据，取用时仍需核对本行引用的E。资料字段中的旧四值说明已改成自由描述，避免把未列出的源码类型误判为无效。

以后按[设计问题](../research/interaction-library/design-reference-guide.md)查细节，按[产品目录](../research/interaction-library/case-index.md)查覆盖，按原页或操作审计查依据；简报用于阅读结论。先前与本次候选机制仍是研究参考，没有升级为需求，也没有进入全系统设计。
