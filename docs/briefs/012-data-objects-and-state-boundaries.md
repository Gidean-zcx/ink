# 第十二期：副本、索引与恢复状态

2026-10-27。继续落实逐功能交互研究。本批为文档重建及三张手册静态页；没有新增产品实际操作，也没有把竞品做法确定为本项目设计。

## 三项可直接带入后续设计的问题

| 设计时的问题 | 具体交互依据 | 应保留的区分 |
| --- | --- | --- |
| “改好了”会影响哪些地方？ | reMarkable的Convert and share网页不会随设备后续改动更新；只有所有者可编辑，持链接者可查看/下载。Meeting Notes复制日历信息，改笔记不写回日程。[案例F15/16](../research/interaction-library/cases/M-005-remarkable.md) | 原稿、转换文本、分享网页、日程是不同对象。AI需说明目标对象与传播结果；不能把有链接当成双向同步。 |
| “资料导入了”是否就能搜索和学习？ | 欧路先复制词库，再单独启用全文索引并等待完成；加入生词本、正式学习、同步是其他入口。[案例F04/09/12](../research/interaction-library/cases/M-026-eudic.md) | 文件存在、索引就绪、查到释义、进入学习计划分别报告。后台索引有文档依据，杀进程后续建尚无依据。 |
| 音频文件还在，为什么不能继续上次状态？ | Sony NW-A306移卡时手册分别列出播放位置/队列被清除与收藏/歌单登记信息消失；传回电脑保存音乐内容，不包含完整播放器状态。[案例F14/16](../research/interaction-library/cases/M-036-sony.md) | 内容备份与播放状态备份分别验收。重插是否恢复关联、逐书断点与普通音频打断仍需测试。 |

这些细节对应G-002的全局AI，也对应G-001：收集到内容并不等于形成可学习资料，播放或页面进度也不等于知识掌握。

## 纠正和条件

- reMarkable同日帮助对选区转文字分别描述自动替换与光标粘贴；会议列表帮助写未来10小时，新3.29日志写三天。两条证据与入口/版本条件均保留，不替文档补一个未经观察的统一行为。
- Sony播放页明确提示时间条可能不准确，旧“精确寻位”已收窄；USB音频输出与USB-DAC输入的音效条件不同。后台无声后回DAC屏只是官方建议的可能恢复。
- 欧路键盘Command+S保存笔记退出、Command+W放弃退出；这不证明触屏也有同样按钮。旧帮助的Android周期本地备份没有给出恢复流程，不能据此承诺重装后可恢复。

来源、版本和获取方式分别见[reMarkable原件记录](../research/interaction-library/artifacts/remarkable-controls-deepening/source-notes.md)、[Sony原件记录](../research/interaction-library/artifacts/sony-controls-deepening/source-notes.md)、[欧路原件记录](../research/interaction-library/artifacts/eudic-controls-deepening/source-notes.md)。

## 怎样反复使用这些资料

先从[横向问题表](../research/interaction-library/comparison.md)找设计问题，再打开案例的F任务，沿E编号查章节、原始来源、版本与证据方式，最后看该任务的缺口。功能目录单独维护，避免“列出入口”被误认为“研究完功能”。同一任务有新证据时补回原编号，原结论修订保留原因。

本批新增36条证据、16个任务，并补回20个已有任务；新增241条目录记录，含父章节及非功能项，不作为独立功能完成数。全库准确数量见[覆盖索引](../research/interaction-library/case-index.md)。65组产品仍全部为部分覆盖，不能说每个功能已经深入体验。下一步继续补缺失的完整目录、核心使用链及异常恢复；无设备或登录环境的部分明确停留在文档证据，不编造体验。
