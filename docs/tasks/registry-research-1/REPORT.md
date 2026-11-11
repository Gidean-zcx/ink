# CloudZao 固件仓库调研

查阅日期：2026-11-11。基线：`0c9b1790d6e79e1beff8337479d72e780389e5c6`。

范围：只读公开网页、接口、源码和本项目；不下载或运行固件二进制，不登录、不接设备。结论分为【实测】（本次只读请求或命令）、【文档】（上游说明）、【实现】（源码核查）、【估算】（建议及工作量）。网页或源码具备某功能不代表本项目已经实机验证。

**结论：有条件可行。** 适合吸收独立的文本处理、位置映射和字体工具，参考社区对阅读与内容导入的取舍；没有证据支持把某个完整固件直接并入 ink。原生镜像切换、P4 外部 ELF 应用、隐藏 AI 的阅读固件，均不能代替本项目的系统与应用框架。以下建议均未变成需求或实施决定。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 固件全清单与三类建议 | 清单完成，建议核查中 | 52条、156版本、20组筛选一致 | `python3 docs/tasks/registry-research-1/collect_registry.py --output work/registry-research-1/recheck.json` |
| 2 可复用条目许可证 | 进行中 | 待逐项核对许可证原文 | 待补 |
| 3 隐私扫描 | 待检查 | 不保留登录态、令牌或签名参数 | 待补 |
| 4 修改范围 | 进行中 | 仅新增本任务目录文件；产品代码 0 行 | `git diff --name-only` |

## 接口、筛选和完整性边界

【实现】[API脚本](https://burner.cloudzao.com/_next/static/chunks/17s3nznutp6_6.js)中的 `f()` 给出公开服务地址，`tS()` 构造固件列表请求；[页面脚本](https://burner.cloudzao.com/_next/static/chunks/4128lpwl_vb6d.js)把“精选”映射为 `sourceType=all&featured=true`，其余三项分别传 `official/community/all`。两份脚本均为压缩单行，定位应使用函数名、参数串及[网页哈希清单](web-evidence.json)，不能虚构展开后的行号。

【实测】实际列表为 [GET /xiaozhi/api/firmwares](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all)，不带 Cookie、Authorization 或其他凭据。第一页52条，第二页0条，全部ID唯一；四种筛选逐板与总表集合相等，22次成功请求的HTTP状态、业务码、数量及响应SHA-256均在[快照](registry-snapshot.json)。

| 板号／芯片 | 精选 | 官方 | 社区 | 全部 | 范围【实测】 |
| --- | ---: | ---: | ---: | ---: | --- |
| 44／esp32s31，E-Ink4 Plus | 12 | 1 | 23 | 24 | 我们的设备 |
| 38／esp32s3，E-Ink4 | 9 | 1 | 21 | 22 | SPI屏，不能等同Plus并口屏 |
| 33／esp32p4，Claw4 | 3 | 1 | 3 | 4 | P4触屏设备，不能移用其性能结论 |
| 35／esp32s3，1.54 | 0 | 0 | 1 | 1 | 无官方固件记录不等于无社区记录 |
| 36／AI玩具盒，接口额外记录 | 0 | 1 | 0 | 1 | 不在初始四板侧栏中；芯片未核实 |

“精选”是重叠标签，不能再加到官方和社区数量中。总计官方4、社区48，其中初始四板官方3、社区48。

【实测】板卡目录 `GET /xiaozhi/api/firmware-boards` 返回HTTP 200、业务码401，提示需登录；到此停止。静态HTML只带四块官方开发板，社区开发板列表是否另有空板无法核实。因此“完整”指**本次匿名固件总接口的全部52条及其涉及的5个板号**，不声称枚举了登录后、审核中、已下线或没有固件的开发板。静态页“官方0／社区0”是加载前状态，不能据此判断仓库为空。

## 固件完整清单

【实测】匿名GET返回 **52条固件、156个版本**：初始四块板51条，另有后台板号36的一条。每行的功能是对公开说明的转述，属于【文档】；不把说明当运行测试。表中“测”表示接口当前首项为测试通道，版本不是按字符串自行排序。完整历史版本、通道、更新时间和各筛选命中ID在[脱敏快照](registry-snapshot.json)。

### Metalio E-Ink4 Plus

| 编号／固件／公开作者 | 归属；精选 | 当前版本／历史数 | 更新（+08:00） | 说明与功能【文档】 | 源码／许可核查 | 逐行接口出处【实测】 |
| --- | --- | --- | --- | --- | --- | --- |
| F01 Metalio E-Ink4 Plus／CLOUDZAO | 官方；是 | 2.0.78／16 | 2026-11-10 19:46 | 官方综合固件；2.0.78说明新增图文提问、阅读排版与EPUB命名空间兼容 | S01（MIT；依赖分列） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F02 CrossLink Pixel 4.7／风眼 | 社区；是 | v4.3.0T／9 | 2026-11-08 22:06 | 像素阅读主题；书籍元数据联网补全、排版预设、灰阶封面 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F04 Mogo E4p／常暗 | 社区；是 | v1.0.10／16 | 2026-11-11 07:56 | 作者称源自kiikoread Pico；最近阅读、迷你书库、锁屏小组件 | [官网](https://mogo.mantake.shop)；S04上游；Mogo改动未公开 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F06 RickyOS／Ricky AI Studio | 社区；是 | v1.2.14／4 | 2026-11-02 23:35 | 仅写安装官网；当前版本无功能更新说明 | [官网](https://rickyaistudio.com/)；未取得源码许可 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F07 kiikoread · Metalio EINK 4 Plus／Kiiko厂长 | 社区；是 | rc110／3 | 2026-11-05 22:15 | Plus移植；EPUB/TXT、排版、统计、锁屏、字体与传书 | S05（混合许可） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F08 Crosslink2 无名4.7／whimco | 社区；是 | 4.2.9／8 | 2026-11-09 09:25 | 经典阅读主题；TTF、USB、热点、坚果云与数据胶囊 | [官网](https://www.crosslink-reader.top)；未取得源码许可 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F11 ThingOS／flowflic | 社区；是 | v0.1.7／6 | 2026-11-11 12:31 | 本地/微信读书、进度与批注、录音音乐、ima投递、闹钟 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F12 lnkboot／wenbos058 | 社区；是 | v1.0.6／7 | 2026-11-03 15:36 | 多固件切换启动器；未给具体实现 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F17 固件口袋／开哥 | 社区；是 | v1.0.8-plus.15／1 | 2026-11-05 12:55 | 多固件容器；TF卡资源、应用返回规范、睡眠与异常返回 | S07（源码ZIP未展开） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F18 PageRest／风城 | 社区；是 | v0.7.103／5 | 2026-11-04 22:01 | 阅读、书架、每日手帐、句子锁屏；按键和阅读热区设置 | [官网](https://pagerest.gofree.kdns.fr)；未取得源码许可 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F23 boboos-前往官网 https://bobo.sh.creativone.cn／树莓和派 | 社区；是 | v0.2.3.1／3 | 2026-11-07 23:02 | BoBoOS；官网账号初始化、应用/字体库与AI余额 | [官网](https://bobo.sh.creativone.cn)；未取得源码许可 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F28 森林里的朋友——简单的认识／开哥 | 社区；否 | v0.1.0／1 | 2026-11-05 13:04 | 儿童认知/冒险/电子宠物；称可放固件口袋 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F30 ModernClassicOS／wenbos058 | 社区；是 | v0.1.1／2 | 2026-11-11 13:01 | 转盘式音乐/有声书；断线恢复、M4A/M4B快跳与索引 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F35 录音机／开哥 | 社区；否 | v0.1.0／1 | 2026-10-30 10:39 | AI键录音、锁屏录音、TF卡保存和回放 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F39 Crosspoint (自用非官方）／郁闷 | 社区；否 | v0.1.0（测）／1 | 2026-10-30 18:38 | 作者自称CrossPoint非官方Plus移植；称支持有限、网页刷未测 | 仅S16为上游；移植改动未公开 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F40 亮证／开哥 | 社区；否 | v0.1.0／1 | 2026-10-31 22:14 | 二维码凭证展示/核销；含LGPL驱动合规交付说明 | S07（源码ZIP未展开） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F41 浏览器／开哥 | 社区；否 | v0.1.0／1 | 2026-10-30 10:39 | Plus浏览器；称可放固件口袋 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F42 天气查询／开哥 | 社区；否 | v0.1.0／1 | 2026-10-30 10:39 | Plus按IP天气；称可放固件口袋 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F43 五子棋／开哥 | 社区；否 | v0.1.0／1 | 2026-10-30 10:37 | Plus五子棋；人机与双人 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F45 点读机／开哥 | 社区；否 | v0.1.0／1 | 2026-10-31 13:01 | 点读演示；AI键返回口袋；无内容识别机制说明 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F47 kiikoread xiugai／zhu_yue2008 | 社区；否 | rc109.1（测）／2 | 2026-11-02 14:16 | kiikoread改版；夜间、音乐、FTP、TXT目录；减封面省内存 | S11（混合许可） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F48 dpReader／dephia3188 | 社区；否 | v1.2.1／1 | 2026-11-01 12:22 | kiikoread越南语衍生；版本备注为字体与加载优化 | S12（混合许可） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F49 记事本／开哥 | 社区；否 | v0.1.0／1 | 2026-10-30 10:38 | Plus TXT记事；称可放固件口袋 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F50 听趣S1-E4P／taorperson | 社区；否 | v0.1.1（测）／2 | 2026-11-08 16:52 | 官方基底；阅读记录/批注、实验在线阅读/KO同步、局域网剪贴板 | S13（MIT；逆向部分另核） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |

### Metalio E-Ink4

| 编号／固件／公开作者 | 归属；精选 | 当前版本／历史数 | 更新（+08:00） | 说明与功能【文档】 | 源码／许可核查 | 逐行接口出处【实测】 |
| --- | --- | --- | --- | --- | --- | --- |
| F03 Metalio E-Ink4／CLOUDZAO | 官方；是 | 2.0.65／12 | 2026-11-05 21:21 | 官方综合固件；阅读、电脑助理、传文件、充电与显示设置 | S02（MIT） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F05 Crossslink_2.0（Metalio E-Ink4）／周默の周末 | 社区；否 | 10.10 修复BUG最新版（测）／9 | 2026-10-10 08:52 | 作者称为Whimco版；语音控制、自定义AI键、旋转与同步 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F09 Crosslink2 无名3.97寸／whimco | 社区；是 | v1.2.10／2 | 2026-10-30 15:38 | 同系列S3版；另修PNG关机图画质 | [官网](https://crosslink-reader.top)；未取得源码许可 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F10 CrossLink Pixel／风眼 | 社区；是 | v4.3.0T／2 | 2026-11-10 00:00 | 像素阅读；字体、云盘、排版预设与书籍元数据 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F14 CrossMux_Metalio E-ink4 (倾斜翻页增强版）／Eric | 社区；是 | v1.6.5（测）／1 | 2026-10-17 17:56 | CrossPoint衍生的CrossMux移植；增加倾斜翻页 | 仅S15为上游；移植改动未公开 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F15 墨集／paouke | 社区；是 | v0.1.3／3 | 2026-10-14 18:20 | 作者称Fork自Launcher；应用安装、校验、市场、返回启动器 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F19 Metalio E-Ink4微信读书适配自用／卡卡罗特 | 社区；是 | v0.1.1（测）／2 | 2026-10-14 00:19 | 官方固件改版；作者称移植CrossMux的微信读书与OPDS | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F20 Metalio-WhiteAI／Michael | 社区；否 | v2.0.51（测）／1 | 2026-09-23 13:54 | AI首页、工具调用；公开源码与当前仓库版可能不同 | S08（根许可未找到） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F21 固件口袋／开哥 | 社区；否 | 1.0.7-s3.20／2 | 2026-11-05 12:55 | S3版固件容器；独立固件须按协议适配 | S07（源码ZIP未展开） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F22 布丁／paouke | 社区；是 | v0.1.0（测）／1 | 2026-10-14 18:26 | 本地/网络音乐、电台、TXT、录音、电话；注明短信发不出 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F24 Eink4Boy／paouke | 社区；是 | v0.1.0／1 | 2026-10-14 22:03 | Game Boy模拟；存档、双触点、游戏音乐；说明称残影仍调试 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F25 BoBoOS--AIOS 用AI重塑你的生活 建议前往官网https://bobo.sh.creativone.cn完成刷写／树莓和派 | 社区；否 | v0.1.0（测）／1 | 2026-11-07 23:01 | 同系列S3版；要求官网完成初始化 | [官网](https://bobo.sh.creativone.cn)；未取得源码许可 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F27 黑猩猩测试——黑猩猩发起对人类的智力挑战／开哥 | 社区；否 | v0.1.0／1 | 2026-10-12 11:02 | 数字位置记忆小游戏；不是学习效果验证 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F29 五子棋／开哥 | 社区；否 | v0.0.2／3 | 2026-11-05 10:43 | 人机/双人五子棋；适配固件口袋 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F31 小智畅享版／数字昵称（已隐去） | 社区；否 | v0.1.0／1 | 2026-10-06 21:31 | 小智改版；待机日历/壁纸、U盘传文件 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F32 记事本／开哥 | 社区；否 | v0.1.1／2 | 2026-11-05 10:42 | TXT记事；适配固件口袋 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F34 天气查询／开哥 | 社区；否 | v0.1.1／2 | 2026-11-05 10:43 | 按IP查询天气；适配固件口袋 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F36 贪吃蛇／开哥 | 社区；否 | v0.1.0／1 | 2026-10-19 21:42 | 贪吃蛇；未提供实现细节 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F37 ardmos／paouke | 社区；是 | v0.1.0／1 | 2026-10-14 20:00 | Arduboy模拟；横竖屏、双触点、SD/USB/网页导入 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F38 浏览器／开哥 | 社区；否 | v0.1.1／2 | 2026-11-05 10:42 | 浏览器；未说明页面能力和兼容范围 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F44 测试固件／zzz | 社区；否 | v0.1.0／1 | 2026-11-05 17:34 | 名称为测试固件；无说明 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F51 森林里的朋友——简单的认识／开哥 | 社区；否 | v0.1.0／1 | 2026-11-05 13:04 | S3儿童认知/冒险/电子宠物；称可放固件口袋 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |

### Metalio Claw4

| 编号／固件／公开作者 | 归属；精选 | 当前版本／历史数 | 更新（+08:00） | 说明与功能【文档】 | 源码／许可核查 | 逐行接口出处【实测】 |
| --- | --- | --- | --- | --- | --- | --- |
| F13 Metalio Claw4／zzz | 官方；是 | 2.0.5.2／2 | 2026-11-05 09:52 | Claw4官方固件；列表仅称稳定版 | S03（MIT） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F16 AgentUI／AlayaElla | 社区；是 | v0.2.0／2 | 2026-09-18 01:46 | P4触屏；电脑AI遥控、相机/文件/电话；SD卡外部应用 | S06（MIT；依赖分列） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F26 Metalio_Claw4_2.0.51_gba_full／zuochen.luo | 社区；否 | v0.1.0（测）／1 | 2026-09-03 16:56 | Claw4官方基底加GBA；BLE手柄、触屏补键；已知菜单残留 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |
| F33 retro-go-gba／黄埔大道西 | 社区；是 | 0.4.13／3 | 2026-10-16 03:30 | retro-go移植；GBA与虚拟键；帧率只是作者说明 | S09（GPL-2.0及混合） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |

### Metalio 1.54

| 编号／固件／公开作者 | 归属；精选 | 当前版本／历史数 | 更新（+08:00） | 说明与功能【文档】 | 源码／许可核查 | 逐行接口出处【实测】 |
| --- | --- | --- | --- | --- | --- | --- |
| F46 默认版本／数字昵称（已隐去） | 社区；否 | v1.8.2／1 | 2026-08-20 19:08 | 列表说明为空；仅版本备注称优化对话响应 | S10（根许可未找到） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |

### AI玩具盒

| 编号／固件／公开作者 | 归属；精选 | 当前版本／历史数 | 更新（+08:00） | 说明与功能【文档】 | 源码／许可核查 | 逐行接口出处【实测】 |
| --- | --- | --- | --- | --- | --- | --- |
| F52 测试／zzz | 官方；否 | 1.0.1／1 | 2026-08-29 18:13 | 不在初始四板列表的AI玩具盒测试记录；说明为空 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?page=1&limit=100&sourceType=all) |


## 可以直接用或复用的

“可复用”表示许可和代码边界具备候选条件，**本次没有移植、编译或运行第三方代码**。逐项读了下列许可原文，不摘录许可证正文。固定提交和文本哈希见[源码证据](source-audit.json)；不要按仓库首页的许可徽标概括整个固件。

| 编号／候选 | 事实与许可证原文【实现】 | 怎样接现有C系统、涉及模块【估算】 | 限制 |
| --- | --- | --- | --- |
| U1 kiikoread流式TXT处理 | S05 [`main/book/book_txt.c:1–18`](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/main/book/book_txt.c#L1)明确为Apache-2.0；已读[全文](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/licenses/Apache-2.0.txt)、[根范围声明](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/LICENSE)和第三方清单。源码按块读、保存原文偏移、按需存目录；不是整本装入内存 | 取编码扫描、原文偏移与目录识别部分；仍用`ink_fs_*`和既有资源分配、阅读能力，替换ESP堆和直接stdio依赖。落在`apps/reader/reader.c`及既有文本/排版边界；大块缓冲放PSRAM。原文位置独立于字号分页 | 不能连同索引缓存、GBK表、其他组件一并视为已审许可；文件级提取时逐个补查。保留版权、许可、NOTICE及修改说明；不必引入上游持久格式或缓存机制 |
| U2 Plus Noto Font的工具与无分配字形读取 | S07子项目[MIT原文及范围](https://github.com/Changkai9888/CloudZao-Projects/blob/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8/plus-noto-font/LICENSE)只覆盖这个子目录；字形数据另为[SIL OFL 1.1](https://github.com/Changkai9888/CloudZao-Projects/blob/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8/plus-noto-font/licenses/OFL.txt)，已读字体来源声明和保留名称要求 | 优先把电脑端字重/阈值处理方法接`tools/fontgen/`并输出我们已有字体格式。若比较现成读取器，C++11类需小型C包装接`system/text/ink_font.c`，只返回字形，不碰屏幕HAL | 当前NTF包不能直接给本项目字体读取器；灰度和1bpp也不等价。不为此默认引入第二种持久格式。未下载字体二进制；保留OFL、原版权及字体命名义务 |
| U3 dpReader的跨阅读器位置映射 | S12 [`firmware/shared/main/book/book_sync_position.c:1`](https://github.com/dephia31/dpreader31/blob/9ae58085c4805a321dc741055effb53183d0b335/firmware/shared/main/book/book_sync_position.c#L1)明确Apache-2.0；[根许可](https://github.com/dephia31/dpreader31/blob/9ae58085c4805a321dc741055effb53183d0b335/LICENSE)保留既有授权，不能把全仓库都称为非商用。实现分别返回精确位置、近似进度和精确性标志 | 可取其“原文位置→显示文本位置→外部位置”的映射方法；C入口需改为项目错误码、文件与资源接口。接阅读器导入导出边界，网络同步留给中枢。我们目前只有TXT，不是接上该文件就支持EPUB同步 | 所依赖的EPUB解析器、HTML映射器、XPointer实现本次未逐文件审完；不批准整组直接复制。先独立验证位置算法；不把百分比当精确书签 |
| U4 听趣S1的阅读服务请求与书架字段处理 | S13[MIT原文](https://github.com/TaoRone/Metalio-E-Ink4-Plus-TingQuS1/blob/89db82442e655340e6194db89d4d6c9e64031d93/LICENSE)；[`weread_service.cc:40、77、228`](https://github.com/TaoRone/Metalio-E-Ink4-Plus-TingQuS1/blob/89db82442e655340e6194db89d4d6c9e64031d93/main/weread/weread_service.cc#L40)可核对gateway信封和书架解析，未请求该服务 | 可作为`hub/ink_hub/`中内容提供方的协议样本；把结果转成现有设备协议，C侧只消费按成员归一化结果。无需把ESP HTTP、cJSON全局分配钩子、NVS和界面搬进设备 | MIT不授予服务使用权；账号、密钥、可用接口及条款要用户定。仅此文件的候选边界；`weread_ink.h:11–17`明确涉及逆向与设备身份伪装，不纳入直接复用 |
| U5 原厂TXT切章规则与电脑转换格式说明 | S01[MIT原文](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/LICENSE)，已核对[`chapter_detect.py`](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/tools/ebook/epdbook/chapter_detect.py)与[`SPEC.md:69`](https://github.com/CloudZao/Metalio-E-INK4-Plus/blob/874538eb54f7f7eb593cd40cd1ba6dc8497226e7/tools/ebook/SPEC.md#L69)；格式头为EBOK、版本1 | Python规则适合电脑/中枢预处理；C侧仍通过阅读器文档入口消费规范化文本或已选格式。先拿自写样本核目录、原文偏移、图片和中文，不把原厂整个Web站作为设备依赖 | D-15已选中枢转换扩展`.ebook`，但当前设备只读TXT；转换工具能生成文件不表示设备能读。不复制测试书、字体或插图；Web依赖另有许可，不被根MIT自动覆盖 |

### 许可证不兼容、附加条件与无法核实的项

- 【实现】S09 [retro-go的GPL-2.0原文](https://github.com/andjiang0083/retro-go-tab5/blob/4e8f0f2f4e75b2102016c433b34ee1f23e17e471/LICENSE)及混合模拟器核心不符合本任务“不用GPL”的规则。S15/S16的自有代码虽为MIT，[完整固件清单](https://github.com/0x1abin/crossmux/blob/a6fd569f7552db4ea1a6550ced7bc06568ae0e61/licenses/README.md)说明链接wolfSSL后的可执行文件按GPLv3；不能把整套固件按MIT拿来用，也不能由此反推每个独立MIT文件都禁用。
- 【实现】S04/S05/S11/S12分范围授权：明确标注的新增原创内容使用个人学习许可，既有Apache、MIT及其他组件权利保留。已读S05[个人学习许可全文](https://github.com/wegooo-cell/Metalio-S31-kiikoread/blob/83942eb45d13064a47aca7ff04c2faf8fd51f98a/licenses/kiikoread-Personal-Study-1.0.txt)。不得将整个新版当成Apache，也不得宣称历史Apache授权失效；本项目不默认接受受限部分。
- 【实现】EPDiy为LGPL，和GPL条目分开。D-124已经允许现有输出层按约定使用；这不等于已批准社区整机固件、全部波形或新驱动的许可。S05第三方清单还说明某DU14波形没有明确再分发授权，不能照抄。
- 【实测／文档】S07口袋和亮证的公开目录主要是源码ZIP、重链接包及协议；本次不下载这些包，不能验证包内代码、完整许可或二进制对应关系。只有`plus-noto-font/`取得了单独MIT/OFL原文，不能外推给整个项目集。
- 【实测】S08/S10/S14所读树未找到项目根许可；只看到源码或个别组件许可不构成全项目复用授权。其余没有公开源码的固件，许可均记“未核实”，不猜测是MIT、闭源或侵权。游戏ROM、封面、图标和样书也没有可复用结论。

## 源码与社区移植关系

【实测】读取16个相关公开仓库的完整Git树；75个实际取回的文本文件都重新计算Git blob SHA-1并与固定树匹配，另记SHA-256。只读源码和说明，未获取发布附件、固件、WASM、ELF或ROM。下表当前源码不等于仓库条目中那一个二进制版本。

| 证据号／固定源码 | 关系、改动与证据边界【文档／实现】 | 许可结论 |
| --- | --- | --- |
| S01 [Plus／874538eb](https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/874538eb54f7f7eb593cd40cd1ba6dc8497226e7) | README说明基于xiaozhi-esp32做Plus板级定制；本次看到的源码为任务给的v2.0.77树，不能用它证明列表2.0.78改动已经公开 | 根MIT，依赖与资产分查 |
| S02 [E-Ink4／d12f481f](https://github.com/CloudZao/Metalio-E-INK4/tree/d12f481f4a42420634bb56a928f3959a977367d4) | README_zn第338行说明小智架构的S3板级定制；与Plus是不同屏幕和端口 | 根MIT，未将全依赖认作MIT |
| S03 [Claw4／ca3aa3fa](https://github.com/CloudZao/MetalioClaw4/tree/ca3aa3fa027ff7dad2adf0c2d03c4f24aa838950) | README说明小智架构的P4板级系统；不是Plus墨水屏软件的性能证明 | 根MIT，依赖分查 |
| S04 [kiikoread Pico／ea371a7d](https://github.com/wegooo-cell/read-pico-reader/tree/ea371a7d5998880265aea09f9d44b26b619a8289) | README说明源自MindReset Read Pico；Mogo只声明基于它，未公开Mogo自身修改，不能核实二进制血缘 | 分范围、含个人学习许可 |
| S05 [kiikoread S31／83942eb4](https://github.com/wegooo-cell/Metalio-S31-kiikoread/tree/83942eb45d13064a47aca7ff04c2faf8fd51f98a) | 独立适配Plus V1.5/34Pin；TXT流式目录、EPUB、字体、睡眠与刷新适配。不能把Pico参数用于Plus | 分范围；U1按明确Apache文件核查 |
| S06 [AgentUI／172ec530](https://github.com/AlayaElla/MetalioClaw4-AgentUI/tree/172ec530ff65e11521585fe3b8611daa20a78aa4) | README直接指向Claw4和小智上游；源码有电脑桥接AI与外部应用；`.eapp`是USTAR里的P4 ELF，不是ink页面描述 | 根MIT；外部组件不自动包含 |
| S07 [CloudZao-Projects／90150a24](https://github.com/Changkai9888/CloudZao-Projects/tree/90150a24d1ed4ebde7389ddfd30e9e5e7b1235b8) | 口袋协议、字体组件、源码/重链接ZIP；开哥的多个小应用说明共用口袋，但没有逐应用展开源码 | 仅字体子项目MIT/OFL已核；其余不能外推 |
| S08 [WhiteAI／153d0967](https://github.com/446599/Metalio-WhiteAI/tree/153d0967e97e1cf308403252cfc2839d6d65fb08) | README为S3原始1bpp首页、无LVGL目标；工具文档有操作ID和结果状态；不是Plus直接移植 | 项目根许可未找到 |
| S09 [retro-go-tab5／4e8f0f2f](https://github.com/andjiang0083/retro-go-tab5/tree/4e8f0f2f4e75b2102016c433b34ee1f23e17e471) | README、Claw4发布说明和目录支持retro-go移植关系；F26是否来自同一分支没有证据 | GPL-2.0及各核心许可，不引入 |
| S10 [xingzhi-ai／55dd6dbb](https://github.com/WMnologo/xingzhi-ai/tree/55dd6dbb01b2bb50cf09ce4a7faed3a0f2481260) | README是星智聊天机器人；不能仅凭F46源码链接认定该1.54二进制的具体改动 | 根许可未找到 |
| S11 [xiugai／746ce316](https://github.com/oouu00/Metalio-S31-kiikoread-xiugai/tree/746ce316faf61ce5228eceee618eacb50a02a395) | GitHub父仓库为S05；MODIFICATIONS列rc107基线、书架快照、黑白目录和DU路径、字体内存处理；列表另称音乐/FTP，未逐功能运行核实 | 分范围，保留上游授权 |
| S12 [dpReader／9ae58085](https://github.com/dephia31/dpreader31/tree/9ae58085c4805a321dc741055effb53183d0b335) | README和许可承认kiikoread来源；增加越南语阅读、字体与KOReader位置映射。当前README版本与F48列表1.2.1不一致 | 分范围；U3仅核明确Apache文件 |
| S13 [听趣S1／89db8244](https://github.com/TaoRone/Metalio-E-Ink4-Plus-TingQuS1/tree/89db82442e655340e6194db89d4d6c9e64031d93) | 官方Plus基底；增加gateway书架、另一路逆向在线阅读、KO同步、局域网剪贴板；隐藏AI入口。源码区分这些路径，不能统称为已获官方支持 | 根MIT；逆向来源与服务权限另核 |
| S14 [CodexRemote／5f9bcad7](https://github.com/AlayaElla/CodexRemote/tree/5f9bcad783ea3274d01982e0f1826e6549d379ae) | F16版本记录链接的电脑端伴随项目；只读README作上下文，不是额外一条设备固件 | 根许可未找到，不复用代码 |
| S15 [CrossMux／a6fd569f](https://github.com/0x1abin/crossmux/tree/a6fd569f7552db4ea1a6550ced7bc06568ae0e61) | README明确为CrossPoint社区分支；支持Metalio E-Ink4目标。F14倾斜增强、F19功能移植只是作者说明，未取得对应修改树 | 自有MIT；组合固件GPLv3 |
| S16 [CrossPoint／0333cbbe](https://github.com/crosspoint-reader/crosspoint-reader/tree/0333cbbe4649f80afafe83061bf1afe9fd372fcf) | CrossMux上游。F39自称Plus移植，无该移植源码；名称相近不足以确认CrossLink系列改了它哪些代码 | 自有MIT；组合固件GPLv3 |

【实测】Ricky官网可读但没有源码入口；BoBo官网是账号、应用库、字体、设备管理及AI余额服务。Mogo和PageRest首页403，CrossLink所给官网DNS失败；均未绕过。F02/F05/F08/F09/F10等CrossLink条目、ThingOS、lnkboot、PageRest、Moji、Pudding、Eink4Boy、ModernClassicOS、ardmos及零散应用，目前只能转述接口说明。Moji自称Launcher分支、CrossPoint自用版自称移植，这些均没有被提升为代码验证结论。

## 导航附属工具

| 对象 | 查到什么【文档／实现】 | 与ink对照及建议【实现／估算】 |
| --- | --- | --- |
| [OTA接口文档](https://burner.cloudzao.com/zh/ota-docs) | POST `/xiaozhi/api/firmware-ota/check`，需所有者个人OTA密钥、Device-Id，输入板名、芯片、固件名和可选精确版本；`latest/test`控制通道。响应给版本、文件URL、大小等。只读文档，未调用POST | `system/services/update/update_service.c:186–245`验证INKFW1头、产品、版本和签名，后续核各段哈希；下载先落卡，安装后自检回退。原厂文档没有说明等价的包签名、资源事务和本项目回退契约，不能直接接裸URL替换它。若用户选原厂服务，连接器应在中枢获取元数据、构建可信ink包，C侧仍走现有更新能力；账号、所有者密钥及分区写入范围都要用户定 |
| OTA的两个容易误读处 | 文档说测试通道同时比较稳定版与测试版；精确指定版本可跨两者。公开下线不阻止所有者密钥获取；只有一块同名板时，芯片不符也仅记日志 | 有通道设计价值，但不能拿服务端筛选代替C侧产品/芯片/签名检查，也不能把网页隐藏当撤销。当前OTA端口和保护出厂槽的限制已在backlog，不作为新缺口重列 |
| [书籍工具](https://burner.cloudzao.com/zh/book-tools) → [eBook Studio](https://epub2ebook.metalio.world/) | 网页提供EPUB/PDF/MOBI/TXT转换为`.ebook`，预览、图片二值化和字体转换；标题称浏览器本地转换。原厂源码另有Python CLI/Web版本 | 本项目`apps/reader/reader.c:135`只列TXT，`MAX_BOOK_BYTES`为2MiB；没有兼容EBOK读取器。不能把“工具支持输入”写成“设备原生支持”。适合参考电脑预处理与设备排版分工；未上传书籍、字体或运行转换，未验证整站不发网请求 |
| [串口调试日志](https://burner.cloudzao.com/zh/serial-log) → [内嵌工具](https://burner.cloudzao.com/api/serial-log) | iframe授权Web Serial；源码/UI有串口配置、UTF-8/GB18030/HEX、暂停滚动、导出、快捷命令，并有退出下载模式按钮。读HTML/JS，没有执行`requestPort`或任何设备操作 | 我们已有`tools/device/pull_logs.py:34、79`离线日志环解码、缺口提示和核心转储处理，系统能导出日志；浏览器实时看日志是另一种便利入口，不代替这些能力。可参考“连接→看日志→导出”的单页，原厂工具未找到明确发布许可，不复制整页或命令状态控制 |
| [云造云](https://yun.cloudzao.com/) | SPA标题和脚本文案显示墨水屏工作台：账号/设备、书籍与字体、壁纸及内容传输 | 仅作原厂内容管理服务记录；未注册、登录、绑定设备、调用数据接口。原厂云账号/投递已有vendor-feature-audit记录，不在本次再列成新增需求 |

OTA和书籍工具这里是协议、说明与代码对照，不是服务端或真实设备兼容性测试。

## 我们应该补齐的功能

下表是**新增候选**，不是已定需求。已逐项与[原厂盘点](../vendor-feature-audit/REPORT.md)和[backlog原厂复查](../backlog.md#2026-11-10-起claude-子代理接手batch12-起)去重；原有EPUB/字体兼容、图库、旋转、蓝牙、传文件、日志、OTA和各硬件端口不重开任务。以下工作量均为【估算】：一名熟悉代码的开发者，不含账号等待、外部政策变化和实机验收。

| 新候选／依据 | 当前具体缺口【实现】 | 模块、原子能力与AI路径（建议） | 大致工作量／要用户定 |
| --- | --- | --- | --- |
| N1 统一外部书库入口，先自有OPDS，再考虑网盘；F19、CrossMux、F08 | 现有文件传输是一次授权上传，`apps/reader/reader.c:135`是本地TXT枚举；在阅读器、中枢内容实现和backlog未找到OPDS/WebDAV书目浏览、检索和导入流程 | 中枢内容提供方负责协议，设备只列书目、发导入动作、显示结果/失败事件；按钮和AI共用查询/导入能力，书籍结果按成员落到现有文件/数据服务。先补已有格式链路，目录入口不承诺万能格式 | 自有OPDS原型3～5人日；可靠导入另2～3日。是否接账号网盘、使用哪个外部服务都要用户定；本次只推荐候选 |
| N2 跨阅读器进度与批注互通；ThingOS、听趣、dpReader | `apps/reader/reader.c:69`使用成员位置文件、`RD.starts`是本次分页字节偏移；没有外部书籍身份、精确/近似位置结果和冲突选择。现有备份不是与其他阅读器双向同步 | 阅读位置/批注归阅读器或内容服务；提供按书读取/设定位置、批注查询/写入和同步结果事件；AI能读当前成员资料与发同步请求，凭据不开放。目录、版本、重排不能只按百分比等同。第一步只验证两端自写样本的位置转换 | 位置实验2～3日；完整单服务闭环约6～10日。用户先选KOReader、微信读书记录还是自建同步；账号、上传阅读记录/批注要用户定。不是默认实现所有服务 |
| N3 电脑给设备当前文本框输入普通长文本；听趣局域网剪贴板 | 已有`text.insert`、`clipboard.write`及会话/修订号检查（`text_caps.c:26、272、284`），能力schema单次文本上限300；没有电脑到当前编辑会话的便捷入口；S31传输监听仍是backlog已有端口缺口 | 在已有一次性授权和传输端口补齐后，用原文本能力完成粘贴，保持session/revision检查、成员隔离、密码字段不可代填和AI读取开关；输入只进入当前人指定的目标，先沿用限长与长文编辑路径，不直接搬入上游4KiB三槽库 | 普通文本原型1～2日，另依赖原有端口任务；是否开放电脑输入及允许哪些字段要用户定。推荐先普通文本，凭据不放公共剪贴板、不由AI填 |
| N4 有声书容器与章节跳转；ModernClassicOS | `media_service.c:350`进入`ogg_open_for`，现有进度/seek已有；未找到M4A/M4B/AAC容器与章节解析。不能将“音频播放存在”当成常见有声书格式已覆盖 | 推荐先电脑/中枢转成既有Ogg/Opus，再把章节映射到同一`media.play`/`media.seek`和状态快照；原生解码和索引留到测量后决定。只比较书籍定位时间、跳转延迟和预算，不照抄封闭固件的缓存实现 | 转换/章节样本验证1～2日；设备原生容器与定位约5～10日并要实机测量。来源与自有内容范围、电脑转换或设备原生方案要用户定；不涉及购买或调用服务 |

## 值得参考的设计

“值得参考”限于可指出的机制；没有跨固件的同机实测，不能宣称刷新、续航或交互速度比ink更好。

| 方面／具体出处 | 可取的设计和边界 | 软总线、AI与简单性审视（建议） |
| --- | --- | --- |
| 交互：PageRest热区关闭、底栏隐藏；CrossLink排版预设（版本说明） | 用少量可命名预设保存阅读偏好、让禁用热区确实不唤起菜单，减少来回调字号。封闭实现未测；不照搬页面布局 | 预设是设置，应用负责展示；人和AI均经同一设置能力，页面快照说明当前预设。先采用少量设置组合，不加另一套桌面或一串新主题 |
| 功耗：kiikoread S31锁屏/深睡、听趣页面声明保网 | 操作页面说明自己是否需要网络，退出后释放，比界面私自一直保活清楚。但kiikoread深睡不是我们可接电话待机的证据 | 功耗机制仍由既有电源服务统一裁决，用户可调项是能力/设置；唤醒和网络变化是事件。D-37保持4G可接听、原厂板级电源实现优先，不移用Pico参数，不新增后台保活任务 |
| 刷新：xiugai黑白目录/选中标记；F18含图章节夜间说明 | 静态界面用文字标记和黑白内容，减少不必要灰阶；照片仍保留灰阶。README性能描述不是实测 | 通过现有显示策略/设置表达；页面内容及当前模式给AI看。我们2026-11-11已改白底细边框，不把“去灰底”再列缺口；不引入水波动画或新波形 |
| 存储：kiikoread流式原文偏移、dpReader精确性标志 | 用稳定原文位置保持书签，显示分页可以重算；外部映射失败要明说近似，比假装准确有价值 | 阅读器拥有位置，数据服务按成员保存；读/跳转/映射结果是原子能力和结构化值。先用已有存储，不默认增加索引缓存文件、常驻扫描器 |
| 升级：Burner按板型/芯片/固件/通道选包；口袋统一返回约定 | 对用户讲清包属于谁、测试版、更新后如何回来；首次安装与日常升级分开。容器切换本身没有被本项目批准 | 包选择是中枢内容/发布机制，安装仍是现有更新能力，进度和失败给AI看。保留INKFW1验证、自检和分区边界，不把Web烧录或重启切换当应用安装 |
| AI：AgentUI连接/任务/录音发送状态；WhiteAI排队操作ID | 明确“已接收”“运行中”“完成”各自含义，帮助人判断是否需要再点；ink已有任务、活动卡和执行器，先核对结果语义即可 | AgentUI的宿主C函数表/位掩码不是AI能力表；原生ELF也没有因此获得成员隔离。沿用现有能力、事件、结构化任务结果，不复制额外队列。听趣为省资源隐藏AI不适合G-002 |

R-061结论：**协议连接器、阅读文档算法、字体工具可各自收口；不需要为了吸收功能再建立一个固件容器或通用运行时。** 需要开放给AI的是用户可做的读书、定位、导入、播放、普通输入等动作及其状态；账号凭据、密码和任意硬件写入不是给模型裸露的接口。原厂或社区UI直接调用网络/NVS的实现，不能照搬为我们的服务旁路。

## 建议先做的三到五件事

均为【估算／建议】，未排入正式backlog，也没有自行选定新增产品范围：

1. **先做阅读格式和位置的小型对照，1～2人日。** 用自写TXT、含图/目录的自制EPUB及转换结果，核清D-15和当前TXT入口的差距；同时量流式读取相对整本2MiB读入的收益。优先验证U1/U5，避免先换整个阅读器。
2. **补电脑输入普通文本的最小路径，1～2人日，依赖既有传输端口完成。** 长文本在墨水屏上输入成本高；已有会话、剪贴板和一次授权机制，不需要再建新同步服务。只在用户选定范围后实现N3。
3. **验证有声书电脑转换与章节定位，1～2人日。** 直接服务“继续上次听”的已有目标，先复用Opus播放和seek；测完再决定是否值得设备原生支持M4B，不能凭ModernClassicOS说明选择解码器。
4. **请用户选一个外部阅读闭环，再做3～5人日原型。** 推荐先自有OPDS目录，账号少、数据外发范围清楚；备选是KOReader进度或微信读书记录。服务授权、凭据和上传范围未定前，只做公开协议与合成样本，不注册、不购买，不默认开启同步。

## 没做到的

- 交互式决策板经公开网页工具读取失败；没有推测用户选择，未修改决策记录。
- 无源码、访问失败、只有源码ZIP、上游与列表版本不一致的部分，见逐行表及源码关系；未完成二进制与源码一致性、真实功能、速度、功耗或续航验证。没有把占位、作者说明或测试中功能写成已完成。
- OTA需登录后个人密钥，板卡目录业务401；未继续。所有阅读服务均未认证、未调用、未读真实书架或个人资料。
- 未实现任何候选功能；未读取设计稿、硬件备份或非`upstream`硬件目录；未接触设备。
- 普通/ASan全量最终结果待收尾；S31本次尚未编译，gcccheck待核适用范围。

## 和设计不一致的

- 【实现】D-15中枢扩展`.ebook`与当前设备TXT入口有差距，原厂书籍工具不能直接填上设备读取端；这是既有盘点项，本次只补准确格式、转换端与设备端的证据边界，不改设计迁就代码。
- 【实现】口袋/lnkboot/Moji通过独立镜像切换，AgentUI加载P4 ELF；和R-037、D-111的“C系统＋页面描述＋以后受限脚本”不同。不能把它们称为我们应用框架已可直接兼容，相关应用任务仍按原约定推进。
- 【文档／实现】听趣隐藏AI和待办不符合本项目G-002、R-047。其局域网三槽位持久剪贴板及直接设密钥的路径，也不符合我们按成员、当前输入会话、凭据不给AI的边界；只参考普通输入体验。
- 【实现】Burner个人所有者密钥、裸固件URL与本项目签名升级包/可信中枢的契约不同；未改现有更新服务，也未触碰受保护的出厂槽。

## 改过的已有测试

无。

## 要用户定的

- N1/N2先选自有书库、KOReader进度还是微信读书记录；允许哪些阅读记录/批注传到哪。涉及第三方账号、付费、凭据、家庭数据外发时必须明确选择。
- N3电脑输入范围；推荐普通文本，备选连同敏感配置但必须另行设计本人输入路径，不能由AI代填。
- N4电脑转换或设备原生容器；U2字体是否有真实观感收益。默认只保留现有格式、解码器与工具边界，不因社区有功能就增加格式或缓存。
- 是否接原厂OTA/云账号，仍是原backlog中的待选项，不因这份报告取得授权。

## 顺手发现

- 【实测】固件总表多出未在初始四板显示的板号36；主会话复查时不能只对四块板计数。52条包括测试项、重复名称的不同板型和部分旧记录，不是52个独立成熟系统。
- 【文档】Plus列表最新为2.0.78，而本次公开主分支与任务指定副本对应874538e/v2.0.77；新版本的功能说明不能冒充已逐行核过实现。
- 【文档／实现】WhiteAI README与工具说明的能力数量不同；本报告只采用可定位的操作/结果机制，不采用工具总数宣传。固件口袋条目版本名与备注中plus.16也有差异，表按接口版本字段原样记录。
- 【实测】动态站点的HTTP成功不一定代表业务成功；本次单独保留了板目录401，未用空列表替代权限错误。客户端虽支持`firmwareId`参数，服务端抽查没有按它过滤，F02/F44/F52三次均仍为52条、ID集合等于总表，不能把带此参数的链接当成单条固件详情。

## 新增的机制

无。产品代码、测试和构建文件均不修改。

## 要改的文档

- `docs/tasks/backlog.md`：主会话审核后择取N1～N4；不要重开原厂端口、通用格式和应用框架任务。
- `docs/system/ui-dependencies.md`：若采纳，补外部书目、阅读位置/批注、桌面输入和有声书章节的真实依赖。
- `docs/system/capabilities.md`：只有相应功能实际实现后才生成能力表；不能先写上建议的能力冒充实现。
- 需求、决策及既有调研资料由主会话按用户选择维护；本次均未修改。

## 复现与验证

### C1：公开清单和筛选

```bash
taskpolicy -c utility python3 docs/tasks/registry-research-1/collect_registry.py --output work/registry-research-1/recheck.json
```

【实测】退出0，原话：`Registry: 52 firmwares, 5 board IDs, 20 filters verified, 156 versions`。采集器只请求固定固件列表GET接口，移除用户ID、头像、资源/固件下载地址，纯数字作者昵称隐去；作者列保留的是公开展示署名。逐行出处是总列表接口，精确记录以快照的固件ID、板号和版本对应；不依赖未生效的`firmwareId`服务端过滤。快照不是之后永远不变的数量承诺，线上有增删时应重新核对。

### C2：源码及文件检查

【实测】75个取回的文本与Git树blob一致；16个固定提交与15处显式源码文件/行锚点核对通过。报告52个F编号无重复，快照156个版本数与每项`version_total`一致，20组筛选满足官方/社区不相交、并集等于全部、精选为子集。6个本地链接存在，JSON可解析。原始HTML、源文件和命令日志只留在被忽略的`work/registry-research-1/`，提交的是脱敏元数据与哈希，不是第三方正文。

独立复现源码哈希可用以下只读命令；文件名来自受控清单，不请求任何固件附件：

```bash
taskpolicy -c utility python3 - <<'PY'
import hashlib, json, urllib.parse, urllib.request
from pathlib import Path
data = json.loads(Path('docs/tasks/registry-research-1/source-audit.json').read_text())
for source in data['sources']:
    for item in source['checked_files']:
        url = 'https://raw.githubusercontent.com/' + source['repo'] + '/' + source['commit'] + '/' + urllib.parse.quote(item['path'])
        with urllib.request.urlopen(url, timeout=40) as response:
            raw = response.read(5 * 1024 * 1024 + 1)
        assert len(raw) == item['bytes'] and hashlib.sha256(raw).hexdigest() == item['sha256'], item['path']
print('75 source text hashes matched')
PY
git diff --check
```

### C3：普通构建、全量及内存检查

配置参数与总规则一致，额外指定已有中枢Python和编译命令导出；两套构建/测试依次运行，未同时启动。复现时先设置任务给定的`INK_DEPS`，在本检出执行：

```bash
for kind in normal asan; do
  dir=build
  sanitize=OFF
  if [ "$kind" = asan ]; then
    dir=build-asan
    sanitize=ON
  fi
  taskpolicy -c utility cmake -S . -B "$dir" -DINK_WERROR=ON -DINK_SANITIZE="$sanitize" -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON || exit 1
  taskpolicy -c utility cmake --build "$dir" -j2 || exit 1
  PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir "$dir" --output-on-failure -j2 || exit 1
done
```

- 【实测】普通配置、构建、CTest均退出0。全量原话：`100% tests passed out of 390`；`Total Test time (real) = 885.38 sec`。完整日志：`work/registry-research-1/ctest.log`。
- ASan/UBSan正在运行；最终原话、插桩范围及退出状态在收尾时补齐。
- 【实测】`taskpolicy -c utility python3 ../../tools/gcccheck.py build`退出0，原话`gcccheck: 0 compiled, 0 with findings`。没有改C文件，故未送入任何编译单元；这不是Linux/GCC编译通过证明。
- S31没有编译：本次仅调研文档与独立采集脚本，没有改产品代码、测试或构建文件；不能由主机390项通过推断真实S31兼容性。
- 没有产品修复，因此没有应做撤回修复的反向对照；原测试、预算和断言均未改，未跳过失败。

### C4：提交范围与隐私

最后对`git diff 0c9b1790d6e79e1beff8337479d72e780389e5c6..HEAD`执行总规则第6节原隐私表达式，另按JSON字段检查账户ID、凭据值、签名参数与作者脱敏。最终结果待提交后记录。只允许新增本任务目录，产品C/H、测试、构建及共享文档改动应为0。

## 里程碑

- 已读取任务书及总规则；工作树初始干净，独立分支 HEAD 与指定基线一致。
- 【实测】公开API与所有板号的四种筛选已核对，52个固件ID无重复；分页到第二页为空，已保存脱敏快照。
- 【实测】源码/许可边界、移植关系、导航工具及三类建议已完成；普通390项通过。内存检查及最终提交扫描收尾中。
