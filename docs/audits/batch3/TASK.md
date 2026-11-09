# 一批合并后的独立质量复查：batch3

范围：`git diff 9b3d2da675199713e19af7a297a18458bae26426 de4de5229a9335caffeea82b581608c7d5cffc44`，也就是这一批合进来的任务： docs/tasks/search.md（报告 docs/tasks/search/REPORT.md）； docs/tasks/files-app-1.md（报告 docs/tasks/files-app-1/REPORT.md）； docs/tasks/camera-gallery.md（报告 docs/tasks/camera-gallery/REPORT.md）； docs/tasks/backup-pages.md（报告 docs/tasks/backup-pages/REPORT.md）； docs/tasks/ai-pages-2.md（报告 docs/tasks/ai-pages-2/REPORT.md）； docs/tasks/settings-b.md（报告 docs/tasks/settings-b/REPORT.md）；
这些任务由开发方写、主会话逐个审过并合并；这里是整批的一次独立复查，只做这一轮。

要看的：
1. 正确性：验收里写的行为是否真的成立；断电、重启、换成员、卡拔出时数据会不会丢、会不会串到别的成员。
2. 规则有没有放松：确认规则（D-105，需要人确认的操作）、孩子成员的限制、秘密字段（密码、验证码、PIN）不进快照和日志。
3. 按钮和 AI 是否走同一个能力，页面快照是否和屏幕一致（R-061、G-002）。
4. 资源：内部 RAM、栈、常驻任务、固定上限（能力表、订阅表、JSON 解析器）有没有被撑满或接近上限。
5. 这批任务之间的配合：合并时是否留下重复的实现、互相覆盖的改动、只在一处测试里成立的假设。
6. 可移植：CI 在 Linux 上用 gcc 编译并把警告当错误，本机的 clang 不报的写法也要指出。

时间：这一轮最多 1 小时，到点会被停掉，只做这一轮。先查最可能出严重、高问题的地方，查到一条写一条。

怎么做：
- 只读产品代码和测试，可以在构建目录里编译、跑测试、写一次性的实验；不改产品代码，不提交。
- 边做边把发现写进 docs/audits/batch3/report.md：每条写等级（严重、高、中、低）、文件和行号、怎么复现（命令和结果）、建议的最小修法。断了也要留得下已经写的内容。
- 等级只给有证据的；推测的写“待证实”。不要为了凑数报风格问题。
- 结束时在 report.md 末尾写一行计数：SEVERE=n HIGH=n MEDIUM=n LOW=n。
