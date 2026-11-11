# 参照来源和使用边界

查阅日期：2026-11-11。优先官方HIG、官方用户指南、已有竞品证据库；本次没有使用竞品App实机。页面表的“业界”是从下列来源概括的熟悉模式，**改法和尺寸是针对本项目的建议**，不冒称业界标准规定了本机像素或刷新率。

| 编号 | 官方参照／本次证据 | 用于本次审计的具体模式 |
| --- | --- | --- |
| R01 | [Apple HIG Buttons](https://developer.apple.com/design/human-interface-guidelines/buttons.md)、[Alerts](https://developer.apple.com/design/human-interface-guidelines/alerts.md)，正文已读 | 动作可辨、同组尺寸一致、主要动作与破坏性动作分清、忙时有明确反馈；本机采用无动画的原位文字反馈 |
| R02 | [HIG Text fields](https://developer.apple.com/design/human-interface-guidelines/text-fields.md)，正文已读 | 短字段和长正文分开、标签可持续辨识、适合内容的键盘、清除和错误反馈；不照搬灰色和pt数值 |
| R03 | [HIG Virtual keyboards](https://developer.apple.com/design/human-interface-guidelines/virtual-keyboards.md)，正文与布局描述已读 | 字母/数字/URL/电话/密码有合适布局；候选与听写属键盘辅助；C12的键宽倍数为本机建议，并非Apple规定 |
| R04 | [HIG Settings](https://developer.apple.com/design/human-interface-guidelines/settings.md)、[Segmented controls](https://developer.apple.com/design/human-interface-guidelines/segmented-controls.md)、[Pickers](https://developer.apple.com/design/human-interface-guidelines/pickers.md)，正文已读 | 常规设置分组、当前值明确、少量互斥项用分段，较多值用明确选择；本机用静态分页代替连续滚动 |
| R05 | [BOOX书库](https://booxsupport.zendesk.com/hc/en-us/articles/10701497927316-Choose-a-Book-Display-Mode)、[NeoReader重排文档设置（页面标V4.1）](https://booxsupport.zendesk.com/hc/en-us/articles/25400769451540-Adjust-EPUB-and-Similar)、[Kindle Scribe 2024官方指南](https://d1ergij2b6wmg5.cloudfront.net/kug/kindle_scribe_2024/v1/en-US/html/kug.html)，正文已读；另核[BOOX中央菜单](https://booxsupport.zendesk.com/hc/en-us/articles/10701492644372-Refresh-Settings)及本仓库 [BOOX案例](../../research/interaction-library/cases/M-004-boox-neoreader.md) | 书库、最近文件、阅读工具及字号/行距/边距；不将特定型号的手写/彩屏功能变成本机要求；继续读步数是给定起点的示例估算 |
| R06 | [HIG Notifications](https://developer.apple.com/design/human-interface-guidelines/notifications.md)，正文已读 | 明确来源、内容与行动，不以弹出频率代表价值；本机普通通知仍无横幅 |
| R07 | [HIG Playing audio](https://developer.apple.com/design/human-interface-guidelines/playing-audio.md)，正文已读 | 音量、输出路由、暂停/停止和打断语义一致；不引入新音频服务 |
| R08 | [Apple连接Wi-Fi](https://support.apple.com/en-ca/111107)，官方检索正文已读；通用旧指南地址直取429 | 从Wi-Fi列表选网络、需要时输密码、成功在原列表标记；不照搬彩色勾或自动联网策略 |
| R09 | [HIG Privacy](https://developer.apple.com/design/human-interface-guidelines/privacy.md)、[Alerts](https://developer.apple.com/design/human-interface-guidelines/alerts.md)，正文已读；本机额外合同为界面规范§7/§10和D-105 | 明确数据对象、用途、主体、结果和风险；“真实回执、不确定不重试、本人确认”是本项目合同，不是假借Apple要求 |
| R10 | [HIG Pickers](https://developer.apple.com/design/human-interface-guidelines/pickers.md)、[Apple时分秒自定义计时器（Watch）](https://support.apple.com/en-in/guide/watch/apdf448955b2/watchos)、[Mac Clock计时器](https://support.apple.com/en-az/guide/clock-mac/apdw3d5aebf9/mac)、[iPhone秒表](https://support.apple.com/en-ca/guide/iphone/iph96b1e460/27/ios/27) | 时长能设到秒，运行/暂停/结束动作分明、日期时间直接选择。iPhone旧timer页直取429，故不声称本次已操作或逐帧测量iPhone计时器；秒表取官方检索摘录 |
| R11 | [iPhone照片库](https://support.apple.com/guide/iphone/browse-your-photo-library-iph7d24753a5/27/ios/27)，官方检索正文已读、地区版直取429 | 缩略图网格、按时间查找、点图查看；本机采用分页，不增云图库或手势缩放要求 |
| R12 | [iPhone电话（iOS26经典/统一布局）](https://support.apple.com/guide/iphone/make-a-call-iph3c993cbc/26/ios/26)，官方检索正文已读 | 拨号盘/最近/联系人，详情直接呼叫；步骤因版本和多号码而变，不把一种布局叫唯一标准 |
| R13 | [iPhone信息：新建与回复](https://support.apple.com/en-euro/guide/iphone/iph82fb73ba3/ios)，官方检索正文和画面描述已读 | 收件人直接号码或姓名、对话内回复；位置按本任务共用基准采用顶部收件人和底部正文，未采用新版本工具栏位置为硬性要求 |
| R14 | [iPhone通讯录](https://support.apple.com/guide/iphone/add-and-use-contact-information-iph3e0ca2db/ios)，官方检索正文已读 | 独立应用、搜索、新建、详情直接电话/短信；备注字段要求来自 `comm-ux-2`，不是对官方文案的引述 |
| R15 | [iPhone相机：拍摄](https://support.apple.com/guide/iphone/take-great-photos-and-videos-iph9bbc8619e/27/ios/27)，官方检索正文已读 | 打开相机、可见取景、快门、回看；不据此给墨水屏指定帧率，也不把锁屏滑动入口移植到既定按键唤醒规则 |
| R16 | [iPhone Files整理文件/文件夹](https://support.apple.com/guide/iphone/organize-files-and-folders-iphab82e0798/27/ios/27)，官方检索正文已读；[Kindle指南的USB/导入/内容管理章节](https://d1ergij2b6wmg5.cloudfront.net/kug/kindle_scribe_2024/v1/en-US/html/kug.html)；本仓库界面规范§7.8～7.9 | 点按打开与长按管理分清、多选后批量操作、改名有明确入口；不把当前只读位置画成可移动。HIG Document launch直读失败，未用其未获取正文作依据 |
| R17 | [iPhone Safari](https://support.apple.com/guide/iphone/browse-the-web-iph1fbef4daa/ios)，官方检索正文与图注已读 | 可辨地址、前后导航、输入网址打开；本机远程渲染与会话恢复限制仍以本机代码为准 |
| R18 | [iPhone Notes](https://support.apple.com/guide/iphone/create-and-format-notes-iph1ac0b3a2/ios)，官方检索正文已读；具体无保存按钮、离线录音与提取建议由本机规范§7.6约束 | 新建即写、正文和清单可辨；本机不照搬Apple的Done保存路径或AI服务；日期/录音类型标记是本次找回效率建议 |
| R19 | [Material 3 Text fields](https://m3.material.io/components/text-fields/guidelines)仅返回需JavaScript；[Compose Material3官方变更说明](https://developer.android.com/jetpack/androidx/releases/compose-material3)检索可读 | 核到有label、outlined和secure字段类型；未拿未读到的M3像素/动效细则作证据。统一规格主要据已读HIG和本机合同，不假称完成M3逐控件实操 |
| R20 | [iPhone Calendar](https://support.apple.com/en-gb/guide/iphone/iph3d110f84/27/ios/27)，官方检索正文已读 | 事项详情可编辑内容与时间，开始/结束语义清楚；本机提醒和闹钟合并列表仍按本项目合同 |

交互式决策板 `claude.ai/artifact/RWnCX8AyAbyqArc7D9Wsft` 本次直读不可访问，没有读取到新的选择。本任务明确禁止改共享决策记录，故只用仓库现有决策与最新共同基准，未伪造同步。已读同批 `ui-input-2`、`comm-ux-2` 的任务边界；未访问私有浏览器会话或其他本机资料。
