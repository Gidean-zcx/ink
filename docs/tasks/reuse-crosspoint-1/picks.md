# 用户 2026-11-11 在功能清单页勾选的功能（71 项，另有 Arduboy 和 KOReader 同步明确不要）

| 编号 | 功能 | 说明 | 出自 | 我们现在 | 备注 |
| --- | --- | --- | --- | --- | --- |
| `ai_services` | 多家 AI 服务切换 | 百问、小智、云信、网易之间切换 | 官方（E-Ink4） | 部分有 | 我们走家庭中枢，模型在中枢那边换 |
| `antialias` | 抗锯齿强度 | 字体灰度边缘的强弱可选 | 官方 2.0.78 | 没有 |  |
| `ap_transfer` | 热点传书 | 设备开热点，手机连上传书 | 官方、CrossLink、PageRest | 没有 |  |
| `app_push` | 手机 App 推送 | 用原厂 App 把文件推到设备，分块下载 | 官方 2.0.65、2.0.76 | 没有 | 走原厂云和账号，待你定 |
| `appstore` | 应用市场 | 在线应用市场，受管安装和校验 | 墨集 | 部分有 | 有存储卡上的应用装卸，没有在线市场 |
| `audiobook` | 有声书章节跳转 | M4A、M4B 章节目录和快速跳转 | ModernClassicOS | 没有 |  |
| `autorotate` | 自动旋转 | 按设备方向自动转屏 | 官方（E-Ink4） | 没有 | 原厂对比里待你定 |
| `autoturn` | 定时自动翻页 | 隔一段时间自动翻下一页 | 官方（E-Ink4）、CrossLink | 没有 |  |
| `bookmark` | 书签 | 收藏书签、书签管理 | 官方 | 没有 |  |
| `bookstyle` | 关闭书内样式 | 忽略书里自带的样式，统一排版 | Mogo | 没有 |  |
| `browser` | 浏览器 | 上网看网页 | 浏览器 | 没有 |  |
| `bt_keyboard` | 蓝牙键盘 | 连蓝牙键盘打字 | 官方 2.0.76 | 没有 |  |
| `bt_reconnect` | 耳机断开后自动重连 | 断开后自动重连三次，失败转外放并从原位置继续 | ModernClassicOS | 部分有 | 断开后回到外放；自动重连没有 |
| `bt_speaker` | 当蓝牙音箱 | 接收手机放的音乐 | 官方（蓝牙模式 3） | 没有 | 原厂对比里待你定 |
| `capsule` | 数据胶囊网盘传书 | 从数据胶囊网盘拿书 | CrossLink 系列 | 没有 |  |
| `codex_remote` | 语音遥控电脑上的 AI | 用语音控制电脑端 ChatGPT 并同步进度 | AgentUI（Claw4） | 没有 |  |
| `comic` | 高清插图和漫画模式 | EPUB 插图高清显示，快速看漫画 | Crosslink2 | 没有 |  |
| `desk_pages` | 多页桌面 | 桌面多页、循环切换 | 官方（E-Ink4） | 有 |  |
| `detail` | 书籍详情页 | 书的简介、进度、信息 | 官方（E-Ink4） | 没有 |  |
| `epub` | EPUB 阅读 | 打开 EPUB 电子书，带图片和书内排版 | 官方、kiikoread、ThingOS、Mogo、PageRest、CrossLink 等 | 没有 | 现在只能读 TXT |
| `excerpt` | 书摘 | 收藏喜欢的句子 | PageRest、Mogo | 没有 |  |
| `files` | 文件管理 | 浏览、改名、移动、新建、编辑文本 | Mogo、小智畅享版 | 有 |  |
| `fm` | 收音机 | 收音机应用 | AgentUI（Claw4） | 没有 |  |
| `gameboy` | Game Boy 模拟器 | 内置游戏库，也能读卡上的游戏 | Eink4Boy | 没有 | 游戏卡带有版权问题，要单独商量 |
| `hidebar` | 隐藏状态栏和进度条 | 阅读页底部状态栏、页脚进度条可开关 | PageRest、Mogo | 没有 |  |
| `ima` | 录音上传 ima 知识库 | 录音文件传到 ima 做知识管理 | ThingOS | 没有 |  |
| `jianguo` | 坚果云网盘传书 | 从坚果云（WebDAV）拿书 | CrossLink 系列 | 没有 |  |
| `journal` | 阅读手账 | 每日手帐、像素风阅读手账 | PageRest、Mogo | 没有 |  |
| `keyboard` | 九宫格键盘 | 标准键盘和宫格键盘切换 | 官方（E-Ink4） | 部分有 | 有全键盘（中英），没有九宫格 |
| `kids` | 儿童认知和电子宠物 | 知识通关、冒险、电子宠物 | 森林里的朋友 | 部分有 | 有儿童模式和孩子应用名单，没有这类内容 |
| `landscape` | 横屏阅读 | 左右横屏阅读 | 官方、Mogo、CrossLink | 没有 |  |
| `lang` | 多语言 | 中英文等切换 | CrossLink | 有 | 中文和英文 |
| `light` | 前光亮度和冷暖 | 前光调亮度、调冷暖 | kiikoread、官方 | 有 |  |
| `lock_calendar` | 锁屏：日历和老黄历 | 万年历、老黄历锁屏 | Mogo | 没有 |  |
| `lock_clock` | 锁屏：时钟 | 简约时钟、待机时钟 | Mogo、ModernClassicOS、小智畅享版 | 部分有 | 有锁屏画面设置，样式还没做好 |
| `lock_pin` | 锁屏密码 | 休眠后解锁要密码 | 固件口袋 | 有 | 家人空间有密码，大人锁屏后可要求重新输入 |
| `lock_quote` | 锁屏：句子 | 把喜欢的句子留在锁屏上 | PageRest、Mogo | 没有 |  |
| `lock_ticket` | 锁屏：阅读小票 | 锁屏显示正在读的书、进度、书摘 | CrossLink、PageRest、Mogo | 没有 | 锁屏你说先不做 |
| `lock_todo` | 锁屏：待办 | 锁屏显示待办 | CrossLink Pixel | 没有 |  |
| `lock_widgets` | 锁屏组件 | 最近阅读、迷你书库组件 | Mogo | 没有 |  |
| `lunar` | 老黄历 | 农历、宜忌 | Mogo | 没有 |  |
| `minigames` | 小游戏 | 五子棋（人机、双人）、贪吃蛇、记忆测试 | 社区小游戏 | 没有 |  |
| `mobi` | MOBI 格式 | 直接打开 MOBI | Mogo | 没有 |  |
| `music` | 本地音乐播放器 | MP3、FLAC、WAV、OGG、M4A，浏览、切歌、音量、进度 | ModernClassicOS、ThingOS、布丁、kiikoread 改版 | 部分有 | 能放 Ogg Opus 并记住进度，没有音乐应用，也不支持其它格式 |
| `netease` | 网易云音乐 | 在线听歌 | 布丁 | 没有 |  |
| `netradio` | 网络电台 | 在线电台 | 布丁 | 没有 |  |
| `opds` | OPDS 书源 | 浏览、搜索、下载 OPDS 书库里的书 | 微信读书适配版、CrossMux | 没有 |  |
| `ota` | 联网升级 | 联网检查和安装新版本 | 官方、kiikoread | 部分有 | 经家庭中枢升级做了；真机要写出厂固件所在分区，待你定 |
| `phone_sms` | 电话和短信 | 打电话、发短信 | 布丁、AgentUI | 部分有 | 应用都有；真机 4G 已注册，通话声音正在调研，短信还没在真机试 |
| `recent` | 继续阅读、最近阅读 | 从上次读到的地方继续 | 官方、Mogo、PageRest | 有 |  |
| `recorder` | 录音机 | AI 键一键录音，锁屏也能录，存卡回放 | 录音机、ThingOS | 部分有 | 有录音和语音备忘，没有独立的录音机 |
| `scrape` | 联网补全书籍信息 | 自动下载封面、简介、评分 | CrossLink Pixel | 没有 |  |
| `sd_update` | TF 卡升级固件 | 把固件放卡里升级 | 官方（E-Ink4）、lnkboot、Eink4Boy | 没有 |  |
| `shelf` | 封面书架和分类 | 封面书架、分类、书籍下架、拟物双排书架 | CrossLink Pixel、Mogo、官方 | 部分有 | 有书架列表，没有封面和分类 |
| `stats` | 阅读统计 | 阅读时长、每日图表、本周时长、累计 | 官方（E-Ink4）、kiikoread | 没有 |  |
| `streak` | 连续阅读打卡 | 首页显示连续读了几天 | Mogo | 没有 |  |
| `themes` | 阅读主题 | 一键切换整套排版 | CrossLink Pixel | 没有 |  |
| `toc` | 目录和章节跳转 | 提取章节名，TXT 自动生成目录 | 官方、kiikoread、kiikoread 改版（网页上传时生成目录） | 没有 | 现在只能跳到第几页 |
| `todo` | 待办和今日事项 | 待办清单、今日事项 | 官方（像素视界）、CrossLink Pixel | 部分有 | 有备忘清单和提醒 |
| `typeset` | 阅读排版设置 | 字号、行距、页边距、字距、首行缩进、下划线位置 | 官方 2.0.74、2.0.78，kiikoread，CrossLink（字号到 50pt） | 部分有 | 只有全局字号，没有阅读专用的排版设置 |
| `usb_disk` | USB 传书（当 U 盘） | 插电脑后存储卡当 U 盘用 | 官方、CrossLink、Eink4Boy、ardmos、小智畅享版 | 系统有、真机没接 | 模拟器里有，真机这一层还没接 |
| `vertical` | 竖排阅读 | 古籍式竖排 | 官方（E-Ink4） | 没有 |  |
| `vibe` | 振动反馈和强度 | 触感振动，强度可调 | kiikoread、官方 | 有 |  |
| `wallpaper` | 锁屏和待机壁纸 | 自选壁纸，支持透明 PNG | 官方、CrossLink | 部分有 | 有壁纸设置项，壁纸图库待你定 |
| `weread_notes` | 微信读书：划线、想法、书评 | 看热门划线、想法和书评 | ThingOS、微信读书适配版 | 没有 |  |
| `weread_online` | 微信读书：在线读和搜书 | 在线阅读，搜不在书架上的书并加入书架 | ThingOS、听趣（测试中） | 没有 |  |
| `weread_outline` | 微信读书：读书大纲 | 生成书的大纲（要登录微信读书） | 听趣 | 没有 |  |
| `weread_progress` | 微信读书：进度和时长同步 | 阅读进度、阅读时长和手机端同步 | ThingOS | 没有 |  |
| `weread_shelf` | 微信读书：书架同步和离线读 | 扫码登录，同步个人书架，整本缓存成 EPUB 离线读 | 微信读书适配版（E-Ink4）、ThingOS、听趣 | 没有 | 要微信读书账号 |
| `weread_write` | 微信读书：自己划线写想法 | 本地和微读书籍划线、写想法，自动同步到云端 | ThingOS | 没有 |  |
| `wifi_transfer` | Wi-Fi 网页传书 | 手机或电脑打开网页传书、管文件、自动分类 | 官方、kiikoread、CrossLink | 系统有、真机没接 | 模拟器里有，真机这一层还没接 |
| `workbuddy` | 电脑助理 WorkBuddy | 扫码绑定电脑，语音交互，消息同步 | 官方（E-Ink4）2.0.65 | 没有 | 原厂对比里待你定 |
