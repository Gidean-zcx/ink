# Duolingo匿名网页课程操作审计

取证：2026-10-27（北京时间）；原始UTC日志为10月26日17时段。关联 **F-M031-16 / E-DUCD-25**（匿名准备）、**F-M031-17 / E-DUCD-26**（首课交互与结束尝试）、**F-M031-18 / E-DUCD-27**（退出分支）。

## 环境与方法

临时Playwright驱动真实公开网页；Linux、Chromium156.0.8078.4、1280×950、en-US、全新浏览器上下文。主页HTTP200。按正常可见控件选择法语、来源Other、动机Spend time productively、零基础、每日5分钟、Start from scratch；这些是研究用选择，不是用户真实档案。未提交姓名、邮箱、年龄或支付数据。匿名使用仍可能建立服务端状态，不能称无外部状态变化。

脚本通过正常点击、悬停及等待操作，没有改页面业务变量、强制点击或直接调用答题API。读取DOM用于定位和保存所见；未实际听音、录音或下载视频。没有登录、发反馈/邀请、购买或执行删除。动作日志是带时间的交互命令记录，不是Playwright原生trace或连续录屏；每条命令可含多个动作和截图，命令条数不等于点击数。

## 所见状态与解释

| 状态 | 动作与结果 | 限制 |
| --- | --- | --- |
| 01–12 | 从主页进入课程选择；法语→来源/动机/基础→5分钟目标→从零开始 | 未测试不同课程、回退或跳过；欢迎过场中的文本可在DOM重复 |
| 13–16 | 两次动机引导后载入首题，coffee三张图卡、5颗心 | 13文件名含lesson但仍是引导；15为Loading，不记进入成功 |
| 17–21 | coffee故意选thé，提交前仍5心；Check后4心、正确答案café与扣心提示；下一题tea选thé显示正确 | 未耗尽红心；19截在弹层动画中，不能单独当已关闭 |
| 22–24 | 翻译Un thé；悬停词义节点见tea；选A/tea提交后正确，并显示连对2次 | DOM词bank保留已选词，画面已移到回答区；未把DOM重复当多次作答 |
| 25–28 | Un café题点X，出现丢失进度警告；Keep learning回同一未答题；答A/coffee正确 | 这段只取消退出，没有确认放弃，不证明跨会话保存 |
| 29–44 | 继续翻译及词义选项；每次正确反馈后Continue；两次鼓励过场，红心仍4 | 未评价难度自适应；没有点击Too easy/Too difficult/Report，不发送评价 |
| 45–47 | 听力题点Can't listen now，提示15分钟后恢复听力；红心仍4，随后换文字翻译题 | 未实际听音，也未等待15分钟核恢复；只证明提示与本次替代题 |
| 48–50 | 最后一项文字回答显示正确、连对11；Continue后短暂空白，随后回/learn再次询问基础 | 未看到课末错题重做或XP结算；DOM中的每日任务仍0/10。原因未定位，不能称课程成功完成 |
| 51–53 | 再选零基础、从零开始，重新进入课程 | 重新显示coffee图卡，选项换序，红心保持4；未恢复至上一段最后题，不能因此推定所有历史数据已删除。 |
| 54–57 | 第二次进入先正确回答coffee，下一题tea点X→End session，返回/learn的基础选择层；红心仍4 | 实际确认退出，未在退出后第三次进入核恢复；不把匿名引导当正常账户主页 |

正式保存必要截图；所有取证状态的短文本、URL、可见控件信息见[观察状态](browser/observed-states.json)，命令见[动作日志](browser/trace-actions.json)。原HTML和其余截图仅在work，原件散列另有manifest。DOM可能含被遮住或过场文本，图像结论只依据实际查看的帧。

## 失败与尚不能判断的部分

首次启动在主页截图等待字体时6秒超时，进程退出；重启新上下文后20秒截图上限正常。该次失败发生在课程操作前。日志另有大小写匹配及找错词义节点的定位超时，改用对应可见控件后继续；这些是工具定位错误，不当产品故障。帮助脚本误把FAQ展开当导航的错误单独留在帮助取证记录中。

本次没有记录所有网络/控制台事件，不能断言最终跳回学习页是保存失败、会话过期或服务异常；也不能用课程参与行为推定实际学习成效。截图和随后DOM读取有短暂时间差，Loading、过场和遮罩状态已标明。第一段首题故意答错，随后11次显示正确，一次听力暂缓；题次不用于推定固定课长。

主课与Adventures完全分开；本审计不提升旧F-M031-01～04的深度。原生App、儿童档案、课程更新、家庭共享、离线、跨端与账户删除均未操作。

## 正式截图索引

- [02-get-started](browser/02-get-started.png)
- [11-daily-goal](browser/11-daily-goal.png)
- [16-lesson-ready](browser/16-lesson-ready.png)
- [18-wrong-choice-checked](browser/18-wrong-choice-checked.png)
- [23-word-hint](browser/23-word-hint.png)
- [24-translation-correct](browser/24-translation-correct.png)
- [26-quit-confirm](browser/26-quit-confirm.png)
- [27-keep-learning](browser/27-keep-learning.png)
- [45-next](browser/45-next.png)
- [46-cannot-listen](browser/46-cannot-listen.png)
- [50-after-lesson-wait](browser/50-after-lesson-wait.png)
- [53-second-entry-ready](browser/53-second-entry-ready.png)
- [56-confirm-exit](browser/56-confirm-exit.png)
- [57-exited](browser/57-exited.png)
