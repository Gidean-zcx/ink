# M-002 Kobo：查词、选区、批注与导出

查阅：2026-10-26。范围：Kobo eReader 官方通用帮助；彩色标注说明涉及 Libra Colour/Clara Colour，导出涉及 Kobo 网站。具体固件、账户地区未固定。以下为 **D2 文档重建，未操作设备、未观察连续界面**，不是整个 Libra Colour 的功能验收。完整功能数量未知。

## 流程与状态

| 流程 | 操作与反馈 | 保存/结束 | 证据 |
| --- | --- | --- | --- |
| F-M002-01 查词后继续阅读 | 在正文长按一个词，出现下划线；松手显示释义；轻点屏幕关闭。某些 PDF、借阅书或免费网络书可能不适用。 | 文档说明关闭词典的动作；是否计入生词本、是否保存查询历史未核实。 | E-KOB-01 |
| F-M002-02 标注与写笔记 | 长按词并松手出现选区手柄，拖动端点选文本；选区会自动高亮，可撤销高亮。选择 Add Note 后输入文字、Save。 | 兼容书籍的笔记可随账户同步；第三方书籍笔记仅在本机，删除该书也删除笔记。彩色机型可改高亮颜色。 | E-KOB-02、03 |
| F-M002-03 导出批注 | 先同步阅读设备；网站登录 → My Account → My Books → 对应书 Actions → Export Annotations → 选格式 → Generate File；到 Annotations 下载。 | 可选 PDF/HTML/TXT/Markdown；生成文件在账户保留 30 天。这是导出文件的保留期，不能解释为笔记 30 天后消失。 | E-KOB-04 |

## 容易被功能列表掩盖的边界

- “选中文字”同时可能改变持久数据：自动高亮后，取消选区与撤销高亮是不是同一动作，不能只从长按手势判断。文档给出 Undo Highlight；误触关闭后还能怎样撤销，待操作。
- 此处文本高亮方式不适用于 PDF；不能据此否定其他型号/方式的手写批注。
- 导出限定 Kobo 购买/Kobo Plus 书，并受出版方许可影响，不含手写标记。外部导入书、购买书和手写内容的数据出口并不相同。
- 断网保存、同步冲突、删除后的恢复窗口、跨设备具体延迟均未验证；“同账号同步”不等于备份。

## 对本项目的意义（推断）

关联 R-016、R-017、G-002 与 H-004：研究 AI 找书、读书、查词时，要同时记录书籍来源、格式、选区、批注和可导出范围。AI 获得文字选区不能自动等同于用户要求永久标注。删除书与删除个人积累需要单独讨论；这里没有决定本项目的删除策略。

## 后续观察脚本

用同一短段落准备可选中文本的自有 EPUB、PDF 和具备权限的 Kobo 书；逐一记录长按、拖动、取消、撤销、加笔记、断网重开及导出入口。删除测试只用专门测试书。必须固定机型/固件，并记录选区反馈、手指遮挡、跨页选择与刷新；当前均未执行。

## 证据

- E-KOB-01：[词典操作](https://help.kobo.com/hc/en-us/articles/360017639893-Define-words-with-the-dictionary-on-your-Kobo-eReader)，操作步骤与兼容例外。
- E-KOB-02：[文本高亮](https://help.kobo.com/hc/en-us/articles/360017481334-Highlight-text-on-your-Kobo-eReader)，选区、自动高亮、撤销与颜色。
- E-KOB-03：[添加笔记](https://help.kobo.com/hc/en-us/articles/360017639993-Add-notes-to-your-eBook)，保存及第三方书籍限制。
- E-KOB-04：[批注导出](https://help.kobo.com/hc/en-us/articles/29991333812631-Export-annotations-from-your-books)，网页流程、格式、资格与保留期。

获取方式与版本字段见 [证据索引](/home/workspace/reader/docs/research/interaction-library/evidence.jsonl)；未覆盖购书、借阅、云盘、字号排版、完整导航与系统设置。
