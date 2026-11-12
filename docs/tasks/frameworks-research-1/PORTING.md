# 嫁接边界与需要接的服务

**有条件可行。** 可保留完整的外部解码功能或完整页面工作流，同时把系统状态和副作用接回现有能力；不能把另一个 OS 的存储、线程、权限和硬件服务一起装进来。这里是调研建议，没有新建框架、线程、状态机、配置或文件格式。

代码事实以 [C01](SOURCES.md#c01-ink-接入合同与现有服务)、[I13](SOURCES.md#i13-ink-基线现状) 和本表直接链接为准；“需要”列均为后续实施建议【估算】，不宣称接入已经完成。

## 三类的选择

| 类别 | 推荐底子 | 整块保留的功能边界 | 逐页翻译/重绑 | 保留我们自己的 |
| --- | --- | --- | --- | --- |
| 输入法 | 功能标杆 libime/Rime；设备端先验证 Google 解码核心，前端 fcitx5-android + Mogo 九键 | 搜索、切分、选词、预测、用户词典作为同一来源的解码单元；Mogo 有界组词作备选对照 | Android 键盘、候选和符号页翻成 LVGL；Mogo 自有绘制接回现有显示层 | 焦点/会话、成员词频、密码规则、语音、AI 接话、输入能力与页面快照 |
| 通知栏/控制中心 | AOSP 行为为主，Pebble 动作与勿扰，Mudita/LilyPhone 低刷新列表 | Pebble 动作/回复规则、跨日勿扰计算；Brookesia 单个状态/导航控件是可提取单元 | 分组通知/展开动作、快捷控制、当前值和失败反馈；不可整块带 Android SystemUI | 成员通知箱、统一打断与能力、确认卡、显示/输入调度、任务生命周期 |
| 设置 | AOSP 总结构/搜索，Mudita 电话设置页 | Brookesia 设置行与 Wi-Fi 视图、Pebble 选项/时间段、InfiniTime 枚举视图作为可提取单元 | 目录、结果页、声音/前光/SIM/模式等完整工作流，读取真实服务状态 | 家人/PIN/孩子时间和权限、AI/服务器、备份更新、数据保留与活动记录 |

这些源文件都还依赖原框架；“整块”指保留其完整功能单元和算法，**不是本次已经证明可零改动编译**。若为提取一个视图必须引入另一套系统，按工作流翻译；不把“手写一张相似页面”报告成整应用移植。

## 能力、状态、设置、事件和内部机制

| 对象 | 现有入口/状态【实现】 | 迁入页面要接什么【估算】 | AI 边界与内部机制 |
| --- | --- | --- | --- |
| 组合输入/候选 | `text.state/insert/select/submit/undo/redo`，`keyboard.key/pick/candidates/edit`；I13 | 用户按键与 AI 都通过现有会话；候选需有稳定对象与版本、消耗的拼音长度，不能直接改 textarea 后再补服务 | 普通内容/候选可读可操作；解码搜索图、缓存、内存分配是内部机制，不为每个步骤新设能力 |
| 学习/成员切换 | 成员集合 `text.user_freq`，I13；`member.current`，C01 | 新核心的用户库按成员打开/关闭/提交，切换后不能沿用上一人的学习状态；现有学习是重排，外部核心新增词学习语义要对照 | 密码不学、不外发、不记日志；用户词库不是任意 AI 文件读写入口 |
| 语音/剪贴板 | `text.voice_start/finish/cancel`、`clipboard.read/write`，[text_caps.c:271](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/services/text/text_caps.c#L271-L284) | 保留录音中、转写结果、取消/失败等当前会话状态 | 凭据框不能被 AI 代输；语音、中枢请求和本地拼音结果不混标 |
| 通知列表/分组/动作 | `notify.post/list/read/dismiss/clear/channels/set_channel`；C01。[notify.h:24](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/include/ink/sys/notify.h#L24-L43) 已有 `group/tag/action/action_args/member` | 不再复制上游通知存储。用现有分组/替换字段；多个直接动作、内联回复是否扩展现有单 action 合同，须另列实施任务 | AI 只看当前授权成员的对象；显示、删除、回复走同一能力。清除摘要不等于删除短信源数据 |
| 通知变化 | `notify.changed/posted/channels_changed`，C01 | 定向更新受影响项，保证点按指向的通知没有被整页刷新替换 | 事件可供 AI 感知；上游高频 UI timer 不迁入 |
| 勿扰与打断 | 设置 `notify.dnd`；`interrupt.list/ack` 与 `interrupt.active`，[interrupt.c:317](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/services/notify/interrupt.c#L317-L322) | 勿扰例外/计划需要明确定义后接现有通知、时间、通信/闹钟服务，不在 UI 新建另一套优先级 | AI 可按既有规则设普通开关；模式状态与“是否真的可响铃”由系统返回，不凭控件颜色猜测 |
| 前光/声音/网络 | `settings.get/set`；`display.light/warmth`、`audio.volume`、`notify.dnd`；`net.set_airplane`；C01 | 状态栏/设置页/快捷区用同一设置与状态，接收失败和最终变化，保留暖光及儿童音量限制 | AI 与手指同路，驱动不暴露给页面；SIM/APN 密码不得出现在快照 |
| Wi-Fi/移动网络 | `wifi.scan/list/connect/forget`，[wifi.c:256](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/services/net/wifi.c#L256-L269)；`net.apn_set/roaming_set/usage`，[comm_service.c:804](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/services/comm/comm_service.c#L804-L809) | 保留真实扫描/连接/失败/只读等状态，不把上游 UI 演示数据当网络。双 SIM 选择和活动 SIM 要分清 | 普通设置按当前策略直接执行；本人输入凭据。不得把上游手机协议/AT 实现带到 UI 层 |
| 通知回复/来电动作 | `sms.send`、`call.answer/hangup` 与结果事件，[comm_service.c:695](https://github.com/Gidean-zcx/ink/blob/6171c2bf8d59238c95f2d548663f8b2ab3c6f43a/system/services/comm/comm_service.c#L695-L716) | 借 Pebble/ZSWatch 的动作呈现，实际通过自己的能力执行；配对手机通知不能未经映射就当成本地短信 | 来源、收件对象、成员、最终结果和失败都可追踪。用户主动请求遵守 D-105，不能把上游每次发送的确认流程强加进来 |
| 设置层级/搜索 | 设置行已绑定 `settings.set`，C01；现有设置服务为唯一状态来源 | 搜索索引读同一份定义/页面入口，结果带当前值与路径；实际完整搜索实现范围仍待任务定义 | 索引不能泄漏成人专属设置值；禁止 UI 绕过权限直接写 NVS/数据库 |
| 家人/PIN/确认卡 | `member.*`、设备端 PIN、既有策略与 UI；C01 | 全部保留。外部应用的用户/账户/联系人收藏不能替代本项目家人模型 | 页面及浮层可结构化读取，密码字符例外；AI 不能代替本人输密码或完成需要本人确认的身份动作 |

## 资源与验证门槛

设备条件取自[本次任务书](../frameworks-research-1.md)：684×1216、约 297 dpi、16 MB PSRAM、程序分区约余 400 KB。**后两项不是本轮重新测得的余量**。

1. 程序 `.text/.rodata` 与字典/图片/模型数据分开核算。把大词库放卡上不能解决解码器程序放不下，也不能把全部 PSRAM 当可供输入法独占的内存。不得借调研自行改分区。
2. 后续先编完整候选核心，记录 S31 固件增量和内部 RAM/PSRAM 峰值，再测完整句、简拼、九键长歧义输入、用户学习及成员切换；当前 Google 宿主机失败必须解决且 Mac/Linux 结果一致，才能进入应用集成。
3. 功能比较用固定输入：完整拼音跨词、未知姓名、已选前半句后改后半句、简拼、九键读音过滤、中英文夹杂、数字/符号切换、撤销和密码框。衡量首选命中、纠错操作数、端到端响应；不以“出现了一串汉字”算通过。这里只列后续验证对象，不声称执行过。
4. UI 后续核对大字号、成人/孩子模式、触点到真实反馈、后台事件下点按稳定、局部刷新残影；没有目标屏实测前不承诺刷新率、续航或识别延迟。

## 本轮架构审视

- **划分合理性**：建议继续保持输入引擎/键盘、通知模型/打断/展示、设置定义/执行/页面分开；可复用单元接现有软总线，不并行引入上游 OS 级框架。这是对迁入边界的判断，未改系统架构。
- **AI 原生**：所查上游都不足以直接满足本项目的能力表、成员来源和页面语义合同。页面换掉后，按钮与 AI 必须仍执行同一能力、返回相同结构化结果；不能用截图等价替代页面快照。
- **开放是否完整**：普通用户可见数据和动作应同时有快照与能力；内联回复、多动作通知、完整设置搜索与新的用户词学习尚需逐项定义接入。凭据本体和本人确认保留例外。没有把“内部算法步骤”泛化成一批新工具。
