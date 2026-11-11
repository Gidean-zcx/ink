# CloudZao 固件仓库调研

查阅日期：2026-11-11。基线：`0c9b1790d6e79e1beff8337479d72e780389e5c6`。

范围：只读公开网页、接口、源码和本项目；不下载或运行固件二进制，不登录、不接设备。结论分为【实测】（本次只读请求或命令）、【文档】（上游说明）、【实现】（源码核查）、【估算】（建议及工作量）。网页或源码具备某功能不代表本项目已经实机验证。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 固件全清单与三类建议 | 清单完成，建议核查中 | 52条、156版本、20组筛选一致 | `python3 docs/tasks/registry-research-1/collect_registry.py --output work/registry-research-1/recheck.json` |
| 2 可复用条目许可证 | 进行中 | 待逐项核对许可证原文 | 待补 |
| 3 隐私扫描 | 待检查 | 不保留登录态、令牌或签名参数 | 待补 |
| 4 修改范围 | 进行中 | 仅新增本任务目录文件；产品代码 0 行 | `git diff --name-only` |

## 固件完整清单

【实测】匿名GET返回 **52条固件、156个版本**：初始四块板51条，另有后台板号36的一条。每行的功能是对公开说明的转述，属于【文档】；不把说明当运行测试。表中“测”表示接口当前首项为测试通道，版本不是按字符串自行排序。完整历史版本、通道、更新时间和各筛选命中ID在[脱敏快照](registry-snapshot.json)。

### Metalio E-Ink4 Plus

| 编号／固件／公开作者 | 归属；精选 | 当前版本／历史数 | 更新（+08:00） | 说明与功能【文档】 | 源码／许可核查 | 逐行接口出处【实测】 |
| --- | --- | --- | --- | --- | --- | --- |
| F01 Metalio E-Ink4 Plus／CLOUDZAO | 官方；是 | 2.0.78／16 | 2026-11-10 19:46 | 官方综合固件；2.0.78说明新增图文提问、阅读排版与EPUB命名空间兼容 | S01（MIT；依赖分列） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=32b9b8d0790fe483d8b24a9909c5bdac&page=1&limit=100&sourceType=all) |
| F02 CrossLink Pixel 4.7／风眼 | 社区；是 | v4.3.0T／9 | 2026-11-08 22:06 | 像素阅读主题；书籍元数据联网补全、排版预设、灰阶封面 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=92c82de9caa4249c4270e76bb10290c8&page=1&limit=100&sourceType=all) |
| F04 Mogo E4p／常暗 | 社区；是 | v1.0.10／16 | 2026-11-11 07:56 | 作者称源自kiikoread Pico；最近阅读、迷你书库、锁屏小组件 | [官网](https://mogo.mantake.shop)；S04上游；Mogo改动未公开 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=5580fff7f2e8b382283557fa37080944&page=1&limit=100&sourceType=all) |
| F06 RickyOS／Ricky AI Studio | 社区；是 | v1.2.14／4 | 2026-11-02 23:35 | 仅写安装官网；当前版本无功能更新说明 | [官网](https://rickyaistudio.com/)；未取得源码许可 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=f5c35ac9c523aac3bd307090a0fded68&page=1&limit=100&sourceType=all) |
| F07 kiikoread · Metalio EINK 4 Plus／Kiiko厂长 | 社区；是 | rc110／3 | 2026-11-05 22:15 | Plus移植；EPUB/TXT、排版、统计、锁屏、字体与传书 | S05（混合许可） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=b9df44db099418cd836cb32d74941b05&page=1&limit=100&sourceType=all) |
| F08 Crosslink2 无名4.7／whimco | 社区；是 | 4.2.9／8 | 2026-11-09 09:25 | 经典阅读主题；TTF、USB、热点、坚果云与数据胶囊 | [官网](https://www.crosslink-reader.top)；未取得源码许可 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=d932f06a5906b6a283c38c37d031a8f3&page=1&limit=100&sourceType=all) |
| F11 ThingOS／flowflic | 社区；是 | v0.1.7／6 | 2026-11-11 12:31 | 本地/微信读书、进度与批注、录音音乐、ima投递、闹钟 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=b9909c6fbbb253f105cc65befbb89ee5&page=1&limit=100&sourceType=all) |
| F12 lnkboot／wenbos058 | 社区；是 | v1.0.6／7 | 2026-11-03 15:36 | 多固件切换启动器；未给具体实现 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=abaa819e9eec5e0c390ffb66cf63d7f8&page=1&limit=100&sourceType=all) |
| F17 固件口袋／开哥 | 社区；是 | v1.0.8-plus.15／1 | 2026-11-05 12:55 | 多固件容器；TF卡资源、应用返回规范、睡眠与异常返回 | S07（源码ZIP未展开） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=d339eb28f76779495b909d1129ce994f&page=1&limit=100&sourceType=all) |
| F18 PageRest／风城 | 社区；是 | v0.7.103／5 | 2026-11-04 22:01 | 阅读、书架、每日手帐、句子锁屏；按键和阅读热区设置 | [官网](https://pagerest.gofree.kdns.fr)；未取得源码许可 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=71addc365d3f31a53ed5dacca7855582&page=1&limit=100&sourceType=all) |
| F23 boboos-前往官网 https://bobo.sh.creativone.cn／树莓和派 | 社区；是 | v0.2.3.1／3 | 2026-11-07 23:02 | BoBoOS；官网账号初始化、应用/字体库与AI余额 | [官网](https://bobo.sh.creativone.cn)；未取得源码许可 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=722e1e8f7ab139c77e5d3cbe540f73f8&page=1&limit=100&sourceType=all) |
| F28 森林里的朋友——简单的认识／开哥 | 社区；否 | v0.1.0／1 | 2026-11-05 13:04 | 儿童认知/冒险/电子宠物；称可放固件口袋 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=83ecfee765f538299e850d41b0509674&page=1&limit=100&sourceType=all) |
| F30 ModernClassicOS／wenbos058 | 社区；是 | v0.1.1／2 | 2026-11-11 13:01 | 转盘式音乐/有声书；断线恢复、M4A/M4B快跳与索引 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=759b008fdff7a846affd925f928dee90&page=1&limit=100&sourceType=all) |
| F35 录音机／开哥 | 社区；否 | v0.1.0／1 | 2026-10-30 10:39 | AI键录音、锁屏录音、TF卡保存和回放 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=0946fcb2f2dddbf7021d56a65e3a4338&page=1&limit=100&sourceType=all) |
| F39 Crosspoint (自用非官方）／郁闷 | 社区；否 | v0.1.0（测）／1 | 2026-10-30 18:38 | 作者自称CrossPoint非官方Plus移植；称支持有限、网页刷未测 | 仅S16为上游；移植改动未公开 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=7e591796e0c3c512e9376e5b259778ee&page=1&limit=100&sourceType=all) |
| F40 亮证／开哥 | 社区；否 | v0.1.0／1 | 2026-10-31 22:14 | 二维码凭证展示/核销；含LGPL驱动合规交付说明 | S07（源码ZIP未展开） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=2b8842ea27543007d9512228e7e0029b&page=1&limit=100&sourceType=all) |
| F41 浏览器／开哥 | 社区；否 | v0.1.0／1 | 2026-10-30 10:39 | Plus浏览器；称可放固件口袋 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=8fa583addfc017c28cd3451dbe830227&page=1&limit=100&sourceType=all) |
| F42 天气查询／开哥 | 社区；否 | v0.1.0／1 | 2026-10-30 10:39 | Plus按IP天气；称可放固件口袋 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=e806162c097a7cacfba6d1a15e4a1a27&page=1&limit=100&sourceType=all) |
| F43 五子棋／开哥 | 社区；否 | v0.1.0／1 | 2026-10-30 10:37 | Plus五子棋；人机与双人 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=15905a3185d83b9892dc250d413a8199&page=1&limit=100&sourceType=all) |
| F45 点读机／开哥 | 社区；否 | v0.1.0／1 | 2026-10-31 13:01 | 点读演示；AI键返回口袋；无内容识别机制说明 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=3f4432cb94f3ac6426c732bec3009370&page=1&limit=100&sourceType=all) |
| F47 kiikoread xiugai／zhu_yue2008 | 社区；否 | rc109.1（测）／2 | 2026-11-02 14:16 | kiikoread改版；夜间、音乐、FTP、TXT目录；减封面省内存 | S11（混合许可） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=180b786c0fcee631ae05234a46f8b8f5&page=1&limit=100&sourceType=all) |
| F48 dpReader／dephia3188 | 社区；否 | v1.2.1／1 | 2026-11-01 12:22 | kiikoread越南语衍生；版本备注为字体与加载优化 | S12（混合许可） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=7a938d02f2d778d9da494a4cc67e4816&page=1&limit=100&sourceType=all) |
| F49 记事本／开哥 | 社区；否 | v0.1.0／1 | 2026-10-30 10:38 | Plus TXT记事；称可放固件口袋 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=d3aa1246e9a9f567f8e34e4f854828a0&page=1&limit=100&sourceType=all) |
| F50 听趣S1-E4P／taorperson | 社区；否 | v0.1.1（测）／2 | 2026-11-08 16:52 | 官方基底；阅读记录/批注、实验在线阅读/KO同步、局域网剪贴板 | S13（MIT；逆向部分另核） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=144740d278df42d7cf22863139326a9a&page=1&limit=100&sourceType=all) |

### Metalio E-Ink4

| 编号／固件／公开作者 | 归属；精选 | 当前版本／历史数 | 更新（+08:00） | 说明与功能【文档】 | 源码／许可核查 | 逐行接口出处【实测】 |
| --- | --- | --- | --- | --- | --- | --- |
| F03 Metalio E-Ink4／CLOUDZAO | 官方；是 | 2.0.65／12 | 2026-11-05 21:21 | 官方综合固件；阅读、电脑助理、传文件、充电与显示设置 | S02（MIT） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=64795eef17390ae50771ef0e564baac0&page=1&limit=100&sourceType=all) |
| F05 Crossslink_2.0（Metalio E-Ink4）／周默の周末 | 社区；否 | 10.10 修复BUG最新版（测）／9 | 2026-10-10 08:52 | 作者称为Whimco版；语音控制、自定义AI键、旋转与同步 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=f6d56eb1d84e723014012b0d87e4fcb8&page=1&limit=100&sourceType=all) |
| F09 Crosslink2 无名3.97寸／whimco | 社区；是 | v1.2.10／2 | 2026-10-30 15:38 | 同系列S3版；另修PNG关机图画质 | [官网](https://crosslink-reader.top)；未取得源码许可 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=de8a2e4c73ff82fd5bacc9ce81dcacc2&page=1&limit=100&sourceType=all) |
| F10 CrossLink Pixel／风眼 | 社区；是 | v4.3.0T／2 | 2026-11-10 00:00 | 像素阅读；字体、云盘、排版预设与书籍元数据 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=4fea71b49b8bf6fa4a22dc1798423be4&page=1&limit=100&sourceType=all) |
| F14 CrossMux_Metalio E-ink4 (倾斜翻页增强版）／Eric | 社区；是 | v1.6.5（测）／1 | 2026-10-17 17:56 | CrossPoint衍生的CrossMux移植；增加倾斜翻页 | 仅S15为上游；移植改动未公开 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=d343fb172427c7418e0d90f6361f757c&page=1&limit=100&sourceType=all) |
| F15 墨集／paouke | 社区；是 | v0.1.3／3 | 2026-10-14 18:20 | 作者称Fork自Launcher；应用安装、校验、市场、返回启动器 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=630bc329e6ca929d99907209e05824b6&page=1&limit=100&sourceType=all) |
| F19 Metalio E-Ink4微信读书适配自用／卡卡罗特 | 社区；是 | v0.1.1（测）／2 | 2026-10-14 00:19 | 官方固件改版；作者称移植CrossMux的微信读书与OPDS | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=115a7819e14f902deda0f085e5a502db&page=1&limit=100&sourceType=all) |
| F20 Metalio-WhiteAI／Michael | 社区；否 | v2.0.51（测）／1 | 2026-09-23 13:54 | AI首页、工具调用；公开源码与当前仓库版可能不同 | S08（根许可未找到） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=8751908357ff9fa272b7678fcaef17e6&page=1&limit=100&sourceType=all) |
| F21 固件口袋／开哥 | 社区；否 | 1.0.7-s3.20／2 | 2026-11-05 12:55 | S3版固件容器；独立固件须按协议适配 | S07（源码ZIP未展开） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=c08c65e5a4ec74f2025efb64c9a56594&page=1&limit=100&sourceType=all) |
| F22 布丁／paouke | 社区；是 | v0.1.0（测）／1 | 2026-10-14 18:26 | 本地/网络音乐、电台、TXT、录音、电话；注明短信发不出 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=f4576587b25c738295eb196468c20307&page=1&limit=100&sourceType=all) |
| F24 Eink4Boy／paouke | 社区；是 | v0.1.0／1 | 2026-10-14 22:03 | Game Boy模拟；存档、双触点、游戏音乐；说明称残影仍调试 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=b3f2d73a185865867bc5cea29caa18e0&page=1&limit=100&sourceType=all) |
| F25 BoBoOS--AIOS 用AI重塑你的生活 建议前往官网https://bobo.sh.creativone.cn完成刷写／树莓和派 | 社区；否 | v0.1.0（测）／1 | 2026-11-07 23:01 | 同系列S3版；要求官网完成初始化 | [官网](https://bobo.sh.creativone.cn)；未取得源码许可 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=10833b0d56572c7d2ad525122dea3d1c&page=1&limit=100&sourceType=all) |
| F27 黑猩猩测试——黑猩猩发起对人类的智力挑战／开哥 | 社区；否 | v0.1.0／1 | 2026-10-12 11:02 | 数字位置记忆小游戏；不是学习效果验证 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=2c1297cb3a6c846e7d1345b78039221e&page=1&limit=100&sourceType=all) |
| F29 五子棋／开哥 | 社区；否 | v0.0.2／3 | 2026-11-05 10:43 | 人机/双人五子棋；适配固件口袋 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=bd72665970facddcc7beff3b9fc13d72&page=1&limit=100&sourceType=all) |
| F31 小智畅享版／数字昵称（已隐去） | 社区；否 | v0.1.0／1 | 2026-10-06 21:31 | 小智改版；待机日历/壁纸、U盘传文件 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=2c220821474d3b922f6aa2367037edc1&page=1&limit=100&sourceType=all) |
| F32 记事本／开哥 | 社区；否 | v0.1.1／2 | 2026-11-05 10:42 | TXT记事；适配固件口袋 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=70f1b605d2619ed18663264729e7d933&page=1&limit=100&sourceType=all) |
| F34 天气查询／开哥 | 社区；否 | v0.1.1／2 | 2026-11-05 10:43 | 按IP查询天气；适配固件口袋 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=80cb722919f9f1439f5f8d72703ed04e&page=1&limit=100&sourceType=all) |
| F36 贪吃蛇／开哥 | 社区；否 | v0.1.0／1 | 2026-10-19 21:42 | 贪吃蛇；未提供实现细节 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=1c8740d464481571da51c61bad9880fd&page=1&limit=100&sourceType=all) |
| F37 ardmos／paouke | 社区；是 | v0.1.0／1 | 2026-10-14 20:00 | Arduboy模拟；横竖屏、双触点、SD/USB/网页导入 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=a743751c5a6ad3d2f2d6066a223770cd&page=1&limit=100&sourceType=all) |
| F38 浏览器／开哥 | 社区；否 | v0.1.1／2 | 2026-11-05 10:42 | 浏览器；未说明页面能力和兼容范围 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=095cb50f66803ac98b4ba2b1f149067d&page=1&limit=100&sourceType=all) |
| F44 测试固件／zzz | 社区；否 | v0.1.0／1 | 2026-11-05 17:34 | 名称为测试固件；无说明 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=05e5348f7de0541d608b2304a4aef839&page=1&limit=100&sourceType=all) |
| F51 森林里的朋友——简单的认识／开哥 | 社区；否 | v0.1.0／1 | 2026-11-05 13:04 | S3儿童认知/冒险/电子宠物；称可放固件口袋 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=d7828a3721330a505de95599560e2af7&page=1&limit=100&sourceType=all) |

### Metalio Claw4

| 编号／固件／公开作者 | 归属；精选 | 当前版本／历史数 | 更新（+08:00） | 说明与功能【文档】 | 源码／许可核查 | 逐行接口出处【实测】 |
| --- | --- | --- | --- | --- | --- | --- |
| F13 Metalio Claw4／zzz | 官方；是 | 2.0.5.2／2 | 2026-11-05 09:52 | Claw4官方固件；列表仅称稳定版 | S03（MIT） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=810ce64e8e7e58a21843dcd0cfafcee6&page=1&limit=100&sourceType=all) |
| F16 AgentUI／AlayaElla | 社区；是 | v0.2.0／2 | 2026-09-18 01:46 | P4触屏；电脑AI遥控、相机/文件/电话；SD卡外部应用 | S06（MIT；依赖分列） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=4a86a7934a9fc5c2b96295a92e7966e0&page=1&limit=100&sourceType=all) |
| F26 Metalio_Claw4_2.0.51_gba_full／zuochen.luo | 社区；否 | v0.1.0（测）／1 | 2026-09-03 16:56 | Claw4官方基底加GBA；BLE手柄、触屏补键；已知菜单残留 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=bfcb9610aefbedd1b9c3f718cccd5fcc&page=1&limit=100&sourceType=all) |
| F33 retro-go-gba／黄埔大道西 | 社区；是 | 0.4.13／3 | 2026-10-16 03:30 | retro-go移植；GBA与虚拟键；帧率只是作者说明 | S09（GPL-2.0及混合） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=4d45ca7e57e235b067a0753877232d0a&page=1&limit=100&sourceType=all) |

### Metalio 1.54

| 编号／固件／公开作者 | 归属；精选 | 当前版本／历史数 | 更新（+08:00） | 说明与功能【文档】 | 源码／许可核查 | 逐行接口出处【实测】 |
| --- | --- | --- | --- | --- | --- | --- |
| F46 默认版本／数字昵称（已隐去） | 社区；否 | v1.8.2／1 | 2026-08-20 19:08 | 列表说明为空；仅版本备注称优化对话响应 | S10（根许可未找到） | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=d4d65b5755d72436962217644c87cc80&page=1&limit=100&sourceType=all) |

### AI玩具盒

| 编号／固件／公开作者 | 归属；精选 | 当前版本／历史数 | 更新（+08:00） | 说明与功能【文档】 | 源码／许可核查 | 逐行接口出处【实测】 |
| --- | --- | --- | --- | --- | --- | --- |
| F52 测试／zzz | 官方；否 | 1.0.1／1 | 2026-08-29 18:13 | 不在初始四板列表的AI玩具盒测试记录；说明为空 | 接口未给源码；许可未核实 | [接口](https://admin.metalio.world/xiaozhi/api/firmwares?firmwareId=25dc29688ee3b504bbab9df18e77eea1&page=1&limit=100&sourceType=all) |


## 可以直接用或复用的

待逐项核对来源、许可证、C 接入和模块边界。

## 我们应该补齐的功能

待与原厂盘点和当前代码、backlog 去重。

## 值得参考的设计

待核对软总线、原子能力、AI 开放性与复杂度。

## 建议先做的三到五件事

待完成证据核查后提出；建议不等于用户决定。

## 没做到的

- 交互式决策板经公开网页工具读取失败；没有推测用户选择，未修改决策记录。
- 构建、全量测试、内存检查、S31 构建及 gcccheck 尚未运行。

## 和设计不一致的

待核对；不修改设计以迁就现状。

## 改过的已有测试

无。

## 要用户定的

待补。

## 顺手发现

待补。

## 新增的机制

无。产品代码、测试和构建文件均不修改。

## 要改的文档

待补；共享文档及既有调研文件不在本任务修改范围。

## 复现与验证

待补。

## 里程碑

- 已读取任务书及总规则；工作树初始干净，独立分支 HEAD 与指定基线一致。

- 【实测】公开API与所有板号的四种筛选已核对，52个固件ID无重复；分页到第二页为空，已保存脱敏快照。
