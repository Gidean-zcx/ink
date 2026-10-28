# 第三十期：学习阶段、实际状态与证据复查

2026-10-27。目前仍没有逐一研究透每个产品的每个功能。这轮继续补具体控件与分支，也修整资料入口，方便正式设计时找到“哪份证据支持哪项行为”，而非反复阅读历期简报。

**一组单词学完，到底完成了什么？** 不背单词2017年的静态界面，在辨识完成后明确提供“放弃拼写”和“开始拼写”两个选择。2021年的作者流程图与界面则补了选择答案、看答案及继续的局部路径。不同年代文字对重复次数的说法不同，不能拼成当前算法；历史图也没有交代退出后怎样恢复。可复查[任务 F01/02](../research/interaction-library/cases/M-018-bbdc.md)、[拼写选择图](../research/interaction-library/artifacts/bbdc-learning-controls-deepening/c05-spelling.jpg)和[来源范围](../research/interaction-library/artifacts/bbdc-learning-controls-deepening/source-notes.md)。对成人学习原型的启发是分别讨论辨识、回忆、拼写和主动结束，不能仅用“今日完成”概括掌握程度。这仍是建议。

**宣传片中的前后画面，是否同一次任务？** 小猿官网“智慧拍”短片能看见批改等入口、数学作业取景，以及另一段作文点评。数学和作文并非同一题的连续过程，也没有显示数学判错后的纠正、提示、重试与保存。因此只补相关分支的静态证据，原核心任务仍 D1。五幅必要摘帧、时间点、来源和取证失败/成功记录在[观察摘记](../research/interaction-library/artifacts/xiaoyuan-camera-preview/source-notes.md)，原[硬件案例](../research/interaction-library/cases/M-056-xiaoyuan-hardware.md)保留代际与固件缺口。

**选择某项设置，是否代表已经生效？** M7历史作者报告区分了下拉面板的刷新模式、应用级优化和进入应用后的实际模式；其不一致原因尚不明确。另有特定版本的 KOReader 按键问题报告和回应，不能由“问题已关闭”推断全部设备兼容。见[任务 F04/07](../research/interaction-library/cases/M-057-meebook.md)及[原文定位](../research/interaction-library/artifacts/meebook-reading-controls-deepening/source-notes.md)。它们为研究系统状态提供例子，不证明 Plus 的刷新效果、帧率或按键方案。

**播放器具备某项功能，需要什么条件？** HiBy R3II（2025）指南说明播放页上半部切换歌词，并给出 LRC 同目录、同名等条件；平台 FAQ 对无缝播放另有限制。返回页面的操作说明没有证明返回后声音或位置怎样变化。见[歌词任务 F11](../research/interaction-library/cases/M-035-hiby.md)与[指南/FAQ定位](../research/interaction-library/artifacts/hiby-library-controls-deepening/source-notes.md)。读听、播放状态和数据恢复应分别研究，不从功能名称推导体验。

这些问题共同服务两个目标：学习记录要能解释实际完成了哪一步；AI 要分清用户选择、请求已发出、当前有效状态和可恢复结果。当前只收集原型依据，没有据此确定系统架构或新增已确认需求。

资料整理方面，[按设计问题查证据](../research/interaction-library/design-reference-guide.md)补充直接来源摘记入口；[字段与读取约定](../research/interaction-library/data-dictionary.md)解释 F/E、版本、观察方式及实际操作的区别，并明确检查脚本尚不能判断的内容。重复补证沿用原任务编号，来源冲突和失败记录保留。

本批没有新增产品实操或连续视觉链。所有65组仍部分覆盖；当前登记量与逐项缺口见[覆盖索引](../research/interaction-library/case-index.md)。后续优先补同一任务的错误、结果和恢复过程，静态资料不足时明确留下缺口。
