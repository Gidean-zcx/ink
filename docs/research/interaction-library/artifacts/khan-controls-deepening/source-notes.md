# Khan Kids：来源与画面核查

查阅：2026-10-27。帮助正文来自web工具成功读取；普通HTTP直取10页均403后停止。work保存web原返回文本（JSON编码字符串），不是成功抓取的HTML。公开section列表与博客200；前者未展开，未计完整功能目录。

| E编号 | 来源与位置 | 本次短摘记 |
| --- | --- | --- |
| E-KHCD-01 | [创作工具、录音与保存](https://khankids.zendesk.com/hc/en-us/articles/21188738836763-How-to-use-creative-tools-inside-the-Khan-Kids-app)；文档更新2026-02-26；Library/Create; blue pencil; microphone/stop/play; green check | 创作与回放、Gallery保存分开；数学故事和复述是活动范例，不是自动判分。 |
| E-KHCD-02 | [Gallery导出](https://khankids.zendesk.com/hc/en-us/articles/6477686235419-How-do-I-save-artwork-from-the-Gallery)；文档更新2026-02-27；steps 1-5 | Gallery底部启用导出，再点作品箭头到相册；未观察内嵌GIF，格式及失败提示未知。 |
| E-KHCD-03 | [图书馆顶层栏目](https://khankids.zendesk.com/hc/en-us/articles/4403657703821-Find-books-and-lessons-in-the-Khan-Kids-Library)；文档更新2026-02-27；Explore tabs; Books/Create/Math/Reading/Videos/Letters/Logic+ | 8个标签及部分级别选择；7月活动描述不当10月当前活动。 |
| E-KHCD-04 | [家长偏好设置](https://khankids.zendesk.com/hc/en-us/articles/360047566151-How-do-I-access-parental-controls)；文档更新2026-02-27；Parent Section; Preferences | 隐藏Videos/Create/Home与季节主题；Home隐藏不证明系统退出被禁用。家长版限定与教师页分别保留。 |
| E-KHCD-05 | [教师隐藏栏目FAQ](https://khankids.zendesk.com/hc/en-us/articles/13849896234395-FAQ-How-do-I-turn-off-videos-and-coloring)；文档更新2026-02-26；teacher avatar; Settings; Class Preferences | FAQ写OFF隐藏Create/Videos，与另一教师页的ON描述冲突。 |
| E-KHCD-06 | [教师区域访问与作业优先](https://khankids.zendesk.com/hc/en-us/articles/17031195532059--NEW-Manage-student-access-to-areas-of-the-app)；文档更新2026-02-26；Show students Assignments first/Create tab/Videos tab | 作业优先ON要求完成作业；另两开关写ON不可访问，与FAQ冲突，实际方向未知。 |
| E-KHCD-07 | [增加儿童档案](https://khankids.zendesk.com/hc/en-us/articles/360006538192-How-do-I-add-a-new-user)；文档更新2026-02-27；To create additional users | 同一家长账户可增孩子：New或头像、成人区、名字年龄头像、Next。删除和合并未知。 |
| E-KHCD-08 | [按账户类型退出](https://khankids.zendesk.com/hc/en-us/articles/360043669551-FAQ-How-do-I-sign-out)；文档更新2026-02-26；Parent Account; Class Account; Clever | 家庭、课堂及Clever入口不同；没有退出前同步完成或未上传数据的保证。 |
| E-KHCD-09 | [密码恢复与多设备冲突](https://khankids.zendesk.com/hc/en-us/articles/360007054191-How-do-I-set-up-reset-or-manage-a-password)；文档更新2026-02-20；recover/reset; change; password keeps resetting | 先单设备改密确认，再逐台登录；Kids不沿用主站密码；邻近重置入口可能误触。 |
| E-KHCD-10 | [学习反馈动画抽样帧](https://blog.khanacademy.org/supporting-english-language-acquisition-with-khan-academy-kids/)；文档更新2025-05-01；Step-by-Step Animations; A Visual Reward System | 两GIF的离散抽样，非完整题目、非App操作；首次答对粒子条件仍依据正文。 |

## 动画抽样边界（E-KHCD-10）

Chromium156.0.8078.4、1280×950、新建匿名上下文。正常打开公开博客，页面200。初次8帧有Cookie浮层；第二次通过页面Strictly Necessary Only关闭浮层后抽样8帧。两次共16帧均已实看。没有下载GIF、安装App、提交答案或实际听音；不是本项目儿童观察。

每个GIF取4个局部截图，截图间请求等待800毫秒，加截图耗时后间隔约1秒；具体采样相对时间见元数据。GIF在页面加载后已循环，记录时间不是媒体起点或逐帧时间码。两条循环不能拼接为一题的错误→提示→奖励链。

ThisIsHow示例可见“Sandy is …”及draw/drawing选项、提交勾和角色。抽样显示词在句中/选项间变化，不证明提示入口、触发阈值或点击动作。Sparkles示例可见三只熊、1/2/3、礼物车、角色姿态及两处粒子位置，不证明奖励车到达或完整结算。

正式保留第二次的4个必要帧；首轮遮挡图及其余帧留work。静态页不用于推断准确颜色、动效时长、响应速度或E-Ink显示效果。

- [ThisIsHow_LIGHT-1.png](ThisIsHow_LIGHT-1.png)
- [ThisIsHow_LIGHT-4.png](ThisIsHow_LIGHT-4.png)
- [Sparkles_FINAL-3.png](Sparkles_FINAL-3.png)
- [Sparkles_FINAL-4.png](Sparkles_FINAL-4.png)

原件及脚本在work/khan-controls-deepening；正式证据索引保存网页返回文本、博客原HTML及摘帧散列。缺口与具体任务见[M030案例](../../cases/M-030-khan-kids.md)。
