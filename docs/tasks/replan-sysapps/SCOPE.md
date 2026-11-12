# 扩大检索、淘汰理由与闭源追溯

结论：范围已扩至成熟Android应用、腕表/单色设备、ESP-IDF/LVGL示例、4G墨水屏手机和远程浏览器。较弱的演示没有因为省移植时间而压过成熟应用。未证实源码归属的社区固件只作待补证的参照。

## 范围核查

| 指定范围 | 本次读到的东西/证据 | 如何进入结论 |
| --- | --- | --- |
| 乐鑫ESP-Brookesia和应用 | [B01](SOURCES.md#b01)：当前v0.8树的Settings、Files、Shell；[B03](SOURCES.md#b03)：固定v0.4.2 P4 phone示例main | 新旧分开，不把“Phone框架”当完整电话应用。旧例程安装simple/complex/SquareLine；当前有具体文件/设置实现，也有S31版本列，但未在本项目构建验证 |
| ESP-BOX | [B02](SOURCES.md#b02)：chatgpt_demo的settings.c | 能证明示例配置存取；不能当成熟电话/短信/联系人应用，因此不列前二 |
| ESP32-P4官方示例 | [B04](SOURCES.md#b04)：esp-iot-solution的video_lcd_display README/app_video.c | 是摄像头到LCD的数据流程，非成熟相机交互；对本任务只作边界对照，不转成Plus底层驱动建议 |
| LVGL官方demos/widgets | [L01](SOURCES.md#l01)：calendar示例；另检查固定树的demos/widgets目录清单 | 控件/演示不包含事件存储、重复、同步、编辑等完整日程业务，故不因原生C就称更成熟；没有宣称逐个运行全部demos |
| PebbleOS | [M04](SOURCES.md#m04)：coredevices当前树的设置、启动器/简览、闹钟列表；另取pebble-dev旧树清单 | 以当前coredevices公开实现为主；黑白菜单/简览很有价值，按键UI不能直接当触屏UI。两棵树不是两份独立成熟候选 |
| InfiniTime | [M01](SOURCES.md#m01)：闹钟、秒表、Timer控制器和页面 | 时钟第二候选；分秒输入/计次可读到实现，但没有完整世界时钟证据 |
| ZSWatch | [M02](SOURCES.md#m02)：计算器、天气、录音、设置 | 天气/录音/简单计算器进入比较。Zephyr任务/Zbus并不自动兼容本项目软总线；spinner、闪动和圆屏布局须重做 |
| Watchy | [M05](SOURCES.md#m05)：Watchy.cpp实体按键菜单与刷新 | 适合低刷新手表的静态菜单参照；默认没有更完整的电话类应用，不能作为整机系统替换结论。没有采用其深睡/按键唤醒参数 |
| Bangle.js应用 | [M03](SOURCES.md#m03)：weather、calendar、sched、noteify；另读stopwatch代码 | 小屏天气的新旧数据表达、笔记/提醒组合有价值；JS运行时和Storage文件不搬。多个小应用不是同一份完整时钟/日程应用 |
| 带4G/SIM的ESP32手机 | [E02](SOURCES.md#e02)：LilyPhone的phone_store和ui_phone1；[E03](SOURCES.md#e03)：Phone-OS的ComposeScreen/ContactBook | 两个独立项目均有实际源码。优先LilyPhone单色界面与姓名/会话一致性；Phone-OS为补充，没有把其自述硬件验证算本项目验证。SIM800/T-Call的驱动例程不等同成熟手机应用，未以其替换Plus通信链 |
| 墨水屏手机 | LilyPhone和Phone-OS都面向T-Deck Pro系；[E02/E03](SOURCES.md#e02) | 本次找到比“彩色手机皮肤”更直接的低刷新交互参照，板卡/屏/键盘/模组均与Plus不同，不复制底层参数 |
| Flipper Zero应用 | [X01](SOURCES.md#x01)：Archive的browser view、files helper及scene；另读clock settings | 作为成熟单色文件操作参照；没有伪称其具有蜂窝电话、相册或完整日程 |
| AgentUI（Claw4） | [E01](SOURCES.md#e01)：电话、相机、图库、文件、主页；另读外部calculator例程 | 相机/图库C++进入候选，文件列表补充。电话源码只有较窄的拨号/挂断视图，ATD受理与接通有明确区分；没有找到同等完整的短信/联系人应用，未给它虚排“电话类全套第一” |
| AOSP/成熟手机应用 | [A01～A04](SOURCES.md#a01)、[F01～F11](SOURCES.md#f01)、[C01](SOURCES.md#c01)、[W01](SOURCES.md#w01)、[G01](SOURCES.md#g01)、[N01](SOURCES.md#n01)、[X02](SOURCES.md#x02)、[H01](SOURCES.md#h01) | 阅读功能模型和关键页面/回调；Android/Flutter/TypeScript功能完整度可更好，实际复用分为行为参照、算法翻译、服务器代码和设备原生片段，没有以语言不相同直接淘汰 |
| 远程网页 | [R04 Neko](SOURCES.md#r04)、[R03 Browservice](SOURCES.md#r03)、[R01 noVNC](SOURCES.md#r01)、[R02 Guacamole](SOURCES.md#r02) | 分清完整远程浏览器与远程图像/输入组件，服务器运行和设备执行分开；未启动容器、浏览器节点或真实会话 |

## 布丁和开哥：查了哪些，能下什么结论

【实测】2026-11-12重新只读GET [官方固件目录接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all)，返回52条。只提交与本题有关的9条筛选字段：[registry-evidence.json](registry-evidence.json)，未保留用户标识、头像或固件下载地址，也未请求固件二进制。条目ID用于从同一公开接口定位，不是设备标识。

| 对象 | 官方原文短摘/当前证据 | 开源来源追溯与结论 |
| --- | --- | --- |
| 布丁/paouke，E-Ink4 | 原文：“短信目前无法发送，诊断中”；v0.1.0为test；条目与版本的githubUrl/giteeUrl都空 | 结合旧调研及本次按“布丁+Metalio/paouke/源码”检索，没有建立作者到公开代码的可验证链接。**不确定其电话/短信/天气等来自哪个上游**，不能因名称与TinyGSM/LVGL类似就归因，更不能排成成熟短信首选。并非已经证明永远闭源 |
| 开哥记事本（S3/Plus两条） | 原文短摘：“一个很简单的txt记事本”；目录无源码链接 | 功能声明只能证明产品定位，不能证明自动保存、搜索、撤销/恢复或源码可复用；本次未找到逐应用可识别的公开实现 |
| 开哥天气查询（S3/Plus两条） | 原文：“根据 IP 查询天气”；目录无源码链接 | 没有天气源/错误/旧数据/多城市实现证据；没有证据能胜过Breezy/Bangle公开实现 |
| 开哥浏览器（S3/Plus两条） | 原文：“网上冲浪，即可体验！”；目录无源码链接 | HTML能力、输入、TLS、页面范围、是否远程渲染均不确定；未找到能把它归因到Browservice/noVNC/某个浏览器内核的代码证据 |
| 开哥固件口袋（S3/Plus两条） | [K01](SOURCES.md#k01)与固定树可见 `firmware-pocket-1.0.7-s3.20-source.zip`、`firmware-pocket-plus-1.0.7-plus.16-source.zip`，以及重链接包/协议 | **有公开口袋源码交付包，不等于以上每个小应用源码已公开。** 本次尝试只取ZIP目录，HTTP Range返回501，未下载整包，未据此虚报已审计包内代码；要确定各小应用归属仍缺独立源码/包内清单/作者说明。本任务没有联系作者或发消息 |

检索不能穷尽私人群、未索引仓库和历史交付；所以结论是“当前未获得可验证来源”，不是“闭源一定无法复用”。已经找到的可替代开源来源分别是：Fossify/Joplin（笔记）、Breezy/Bangle/ZSWatch（天气）、Neko/Browservice/noVNC（远程网页）；这是**功能替代源**，并非证明它们就是社区固件的原始上游。

## 额外边界

- 【实现】原厂同硬件驱动应复用，与上层应用成熟度排序是两回事。本任务未改/移动任何已有上游只读副本；也未拿原厂阅读/传书替代本题系统应用比较。
- 【实现】Brookesia当前树与几年前的手机demo结构已经不同；固定提交防止后续用新README替旧代码补功能。其“支持S31”是上游版本表，不能写成本项目端口已验证。
- 【实现】Browservice停止维护是维护现状证据；“需要维护浏览器才能持续使用”是取舍，不是许可证筛选。
- 【实现】[BrowserBox当前入口R05](SOURCES.md#r05)被核查时主要为文档/交付入口，未取得可定位的当前浏览器实现源码；只作扩展检索线索，不列为可直接搬代码的推荐。没有购买、激活或调用其服务。
- 所有旧调研只作起点；本次新增的是上述跨平台代码核查、固定行号、基线纠错和逐应用建议。没有把72项勾选清单当作这些候选都已经移植完成。
