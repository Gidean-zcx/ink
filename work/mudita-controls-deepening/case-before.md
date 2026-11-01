# M-012 Mudita Kompakt：通信、离线开关与受控扩展

查阅：2026-10-26。范围为 Kompakt、官方 Quick Start、Mudita Center 及已发布/预告版本资料；不是早期 Mudita Pure。已确认官方列出 K 1.6，但没有将全部现行帮助精确绑定到该固件。K 2.0 的 IFA 预览按预告记录，不当作已交付功能。无设备操作与连续视觉观察。

## 功能目录与版本边界

| 功能域 | 已有资料 | 本案例边界 |
| --- | --- | --- |
| 电话、联系人、SMS/MMS | Quick Start 的拨号、联系人、新号码、附件、删除 | 双卡选择、来电打断和送达状态尚缺完整证据 |
| 系统导航与声音 | 返回/主页/Control Center、音量、显示、隐藏应用、无障碍 | 多种音量与静音的精确优先级待观察 |
| Offline+ | 实体开关；手册说明断开连接及语音相关部件 | 不能与只静音或免打扰混同 |
| 音乐、电子书、图片 | Center 传入；1.4 更新含播放与阅读改进 | 读听位置、队列与所有格式细节待拆 |
| 其他工具 | 官方资料列地图、相机、笔记、录音、日历等 | 逐 app 主要任务和离线范围还未完整核定 |
| 可安装 app | 官方说明通过 Center 侧载兼容应用；2.0 预告改进管理 | 不保证所有 Android app 正常，不据预告宣称当前完整商店/后台管理 |
| 更新与数据管理 | 软件发布目录、Center 文件管理 | 系统升级、备份恢复和卸载数据的完整流程仍需补 |

## 具体流程

| F 编号 | 入口、操作与反馈 | 状态、结束或异常 | 证据 |
| --- | --- | --- | --- |
| F-M012-01 向联系人打电话 | Phone → Contacts → 对象 → Call；手册另有电源键结束通话的无障碍选项。 | 设置改变物理键作用；没有据四步成功路径推断待接通/无服务/占线处理。 | E-MUD-01、03 |
| F-M012-02 写短信与图片消息 | SMS → 新建入口 → 选联系人/新号码 → 输入 → Send；MMS 增加选图片。键盘未出现时点输入光标。 | 官方不同页面将新建入口称为 Pencil/Plus，需核对版本或模式。发送、已送达和对端已读不是同一状态，后两者未核实。 | E-MUD-04、05、06 |
| F-M012-03 删除对话 | SMS → Pencil → 选择会话 → Delete → 再次 Delete。 | 有二次删除步骤；作用于对话，单条删除/撤销/附件保留规则未知。 | E-MUD-07 |
| F-M012-04 调音量与进入控制中心 | Control Center → 铃铛下方音量条上下调整；手册同时给出独立系统导航入口。 | 总音量与铃声/闹钟等用途有分别设置；具体互相覆盖规则不能猜测。 | E-MUD-01、02 |
| F-M012-05 开启深度离线 | 机身开关切至 Offline+，需通信时再切回。 | 手册明确该状态不能紧急呼叫；不能称为“允许重要来电”的专注模式。切回注册网络的时间和失败反馈未测。 | E-MUD-01 |
| F-M012-06 从电脑准备书和音乐 | Center 连接设备 → Your Kompakt → Manage Files → 相应类型 → Add file → 选择 → 确认传入。 | 不支持的文件不能选/上传；完成传输与设备能成功解析仍应区分。电脑预处理不算设备端能力。 | E-MUD-08、09 |
| F-M012-07 从电脑删除文件 | Center → 相应类别 → 勾选文件 → Delete → Delete file。 | 图书/音乐文件删除与应用卸载不是同一操作；与笔记、播放历史的依赖未知。 | E-MUD-10 |

## 与“家庭 AI 手机”的关系（推断）

这种设备把通信、有限应用和离线控制放到可见入口，值得用来研究 G-001 的主动使用。但 Offline+ 同时影响网络与语音入口，与 R-012 常驻唤醒、R-013 来电可达之间存在产品取舍，不能只把实体开关形式搬过来。

侧载本身不足以满足 G-002：外来应用还需要状态、上下文和动作的接入约定。其 Android 底座不在本项目可选硬件内，不能据此认为 Plus 可直接安装 APK。

K 2.0 预告涉及兼容 app 管理、可选日历同步等，适合作为后续观察对象；当前正式分析必须与 K 1.x 发布记录分开。官方论坛的个别故障帖可提供测试线索，未在这里当作所有设备的确定行为。

## 下一步操作证据

固定固件、地区/运营商及 SIM：从阅读/音乐页接来电、拒接、返回；短信草稿退出重开、无网络发送/重试、图片失败；切换 Offline+ 后检查音频、录音、唤醒与网络恢复。另测新号码/联系人两个入口图标是否真有差异。尚未执行，不能以手册替代通话稳定性验证。

## 来源

- E-MUD-01：[官方英文简明手册](https://assets.ctfassets.net/isxmxtc67n72/3biBEzYwk8UjSapgvMB461/b6ade0485fba2625382f44aca0fd0b51/ENG_Mudita_Kompakt_Manual_Online_04.pdf)，第 2–4 页，来自官方 Quick Start 下载链接。
- E-MUD-02：[基础设置](https://www.mudita.com/products/phones/mudita-kompakt/quick-start/basic-settings-and-customization/)。
- E-MUD-03：[拨打电话](https://mudita.com/products/phones/mudita-kompakt/quick-start/calling/)。
- E-MUD-04：[联系人短信](https://mudita.com/products/phones/mudita-kompakt/quick-start/messaging/)。
- E-MUD-05：[新号码短信](https://mudita.com/products/phones/mudita-kompakt/quick-start/messaging/send-sms-to-new-number/)，官方索引正文。
- E-MUD-06：[MMS 附件流程](https://www.mudita.com/products/phones/mudita-kompakt/quick-start/messaging/send-mms-to-contact/)，官方索引正文。
- E-MUD-07：[删除会话](https://mudita.com/products/phones/mudita-kompakt/quick-start/messaging/delete-conversations/)。
- E-MUD-08：[传入电子书](https://support.mudita.com/en/support/solutions/articles/77000596832-how-to-transfer-e-books-to-kompakt-)，更新 2026-09-17。
- E-MUD-09：[传入音乐](https://mudita.com/products/phones/mudita-kompakt/quick-start/managing-files/transfer-music-to-kompakt/)，官方索引正文。
- E-MUD-10：[删除文件](https://www.mudita.com/products/phones/mudita-kompakt/quick-start/managing-files/delete-files-from-kompakt/)，官方索引正文。
- E-MUD-11：[1.4 发布说明](https://mudita.com/about/media/muditaos-k-1-4-0-released-with-many-improvements-for-kompakt/)。
- E-MUD-12：[2.0 预告](https://mudita.com/community/blog/muditaos-k-2-0-at-ifa-2026-a-first-look-at-whats-coming/?tags=e+ink+technology%2Cmudita+kompakt%2Cmudita+backstage)，不作为已发布操作依据。
- E-MUD-13：[官方对 Center 侧载的说明](https://mudita.com/community/blog/why-mudita-kompakt-is-considered-a-flexible-minimalist-phone/)，功能发现，未验证所有兼容性。
- E-MUD-14：[1.6 官方发布条目](https://support.mudita.com/en/support/solutions/articles/77000598634-muditaos-k-1-6)，已确认版本条目，未以未读取的附件推断具体操作。
