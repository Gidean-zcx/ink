# 第九期：相同功能名称背后的条件与操作差别

日期：2026-10-26。继续竞品研究，没有进入全系统设计。

当前研究已经包含操作文档、版本记录、公开客户端代码和静态界面，但仍没有覆盖每个产品的每个功能。尤其要区分“文档可以重建步骤”“看到了一个页面”和“实际完成了任务”；资料量不能代替这个区别。

本期补强中文阅读设备、单词机和儿童信息产品，以下是对后续设计有用的发现。

| 用户场景 | 本轮找到的依据 | 尚不能认定的行为 |
| --- | --- | --- |
| AI 说“单词已准备好” | 百词斩历史教程涉及手机计划、设备确认、切换当前词书几个阶段；墨墨帮助把资源更新与学习数据同步分开。见 [M022](../research/interaction-library/cases/M-022-baicizhan-device.md)、[M023](../research/interaction-library/cases/M-023-maimemo-device.md)。 | 计划已保存不保证设备已下载，也不保证离线作答记录已上传；历史教程不能自动套给现款。 |
| 孩子想听一篇新闻 | TIME 页面公开播放器代码包含有条件的启动、暂停、关闭及错误重试。见 [F-M047-04](../research/interaction-library/cases/M-047-time-for-kids.md)。 | 尚未实际播音；服务是否给这篇文章启用全部控件、刷新后的断点及离线仍未知。 |
| 孩子边查百科边玩 | Britannica Early 的互动有说明、操作和收束；教师另可组织共读、互动、应用与解释。见 [F-M049-05/06](../research/interaction-library/cases/M-049-britannica.md)。 | 教师计分方案不是软件内置奖励；完成动画不能当作理解或长期掌握。 |
| 阅读时切到听书或双窗口 | 汉王同机型作者照片可定位听书设置与分屏选项。见 [F-M008-05/06](../research/interaction-library/cases/M-008-hanvon.md)。 | 单张照片不能证明按钮执行结果、中断恢复、取消效果或当前固件行为，因此仍为 D1。 |
| 用实体键控制当前页面 | 墨案 Plus 手册区分阅读页和其他页面的侧键动作；掌阅更新说明按机型限定操作范围。见 [M054](../research/interaction-library/cases/M-054-moaan.md)、[M053](../research/interaction-library/cases/M-053-ireader-hardware.md)。 | 同品牌各型号不能共用未经核定的按键表；当前页面和前置设置也会影响动作含义。 |

这些差别指向同一个待设计问题：**AI 需要知道当前对象、使用条件、已完成的阶段和仍未完成的阶段。** “有这个功能”不足以让 AI 回答“已经替你做好了”。这是对本项目的建议，没有据此选定账户体系、同步协议或应用框架。

儿童版也可以把“参与一次活动”和“解释一个问题”分开观察。后者可通过孩子举例、复述或作品讨论了解；具体如何评价、何时给予奖励、是否需要成人陪伴，仍应结合 5–6 岁实际使用验证。听故事和自主阅读不因此被自动改造成随时测验。

为了以后能反复使用，本轮继续沿用稳定 F 编号，把新证据补回原任务；另补 [Early 指南目录](../research/interaction-library/inventories/M049-early-guide-inventory.csv)，明确媒体等章节虽已读、操作链仍未完成。按需求查 [横向比较](../research/interaction-library/comparison.md)，按产品查 [案例索引](../research/interaction-library/case-index.md)，再沿 E 编号回到具体来源与版本。

完整功能目录、连续操作和实机记录仍有缺口，详见 [覆盖审计](../research/interaction-library/analysis-status.md)。本期没有给出全功能完成率，也没有把这些竞品在其他硬件上的能力当作 E-Ink4 Plus 已可实现的承诺。
