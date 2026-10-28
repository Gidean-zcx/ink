# 山灵M1 Plus：本次来源范围与静态观察

查阅2026-10-27，hands_on=false；没有设备或App操作，没有连续演示。

- 固定M1 Plus非Android MTouch，旧英文V2.1发布基线；中国站/download/134仍为V1.8，本次HTTP200。两页面不证明设备安装版本。M1快速指南V1.0为文档修订号，24页原件本地复读，没有再次下载。
- E-SLCD-01官网Eddict下载页本次列Android2.3.7、发布日期2026-09-11；其SyncLink名单不列M1 Plus，与M1产品介绍明示支持SyncLink并列保留，不能将名单缺项推成不支持。App页面的云下载/播放队列是App能力，不替换M1设备能力。
- E-SLCD-02为该下载页“说明书”链接指向的官方PDF，16页、末页V1.2；未标日期，也未声明对应Android2.3.7或某个iOS构建。p2自述不是单一型号手册；中文p9的定时/记忆等列表须结合英文p15：播放设置只影响App本地播放，不影响连接的山灵设备。
- SyncLink中文p6写设备控制入口、开控制端搜索并连接、成功提示，iOS只能控制端；英文p12另写必要时Music Scan，以及BT/Wi-Fi同时开启。后者不可直接移植到旧M1 Plus评测所报无线互斥的环境。共用图/文不是M1型号绑定测试。
- Wi-Fi传歌中文p5的“电脑传歌”和“手机传歌”步骤重复；英文p12才给电脑浏览器访问显示地址。均是App接收界面，不能据此填M1自身Wi-Fi传歌的地址、落盘、取消/失败和完成状态。
- Eddict SyncLink、DLNA的媒体渲染器选择、无线投屏是不同控制范围；p13把投屏对象写为streamer。没有证据将M1 Plus补作1:1投屏。
- HiFi Oasis旧E-SHCD-03复抓HTTP200；作者自报V2.1。仅用于型号菜单台账和既有F01/10补分支，未把商业评测升级为官方确认/实测，未查看其图片/视频。
- 普通GET对英文MTouch旧帮助/article/101376、Eddict旧介绍/article/101309、M1下载/download/118返回403，均停止；搜索索引先前可见不能当本次完整页面原件。未改地址绕过，也未下载App、固件或媒体。

## E-SLCD-03静态观察（源自E-SLCD-02）

实际view_image查看PDF页12、15（印刷页相同）。p12可见SyncLink Controller、Connected/Paired/Available分组，但示例已连接的是SCD3.3，列表有EH3/H5/M9 Plus，**不是M1 Plus截图**。右上Wi-Fi Transfer是App界面，示例局域网地址不是本设备可复用地址。p15可见Timed off/Resume mode等App设置，同时正文明确不影响连接设备；下方USB权限弹窗示例为UA7，不是M1 DAC连接验证。按钮、开关位置只有单帧证据，未触发成功/失败。

原件及SHA见work/shanling-controls-deepening/captures.json；正式短摘记/图片SHA见批次E与formal-manifest.json。M1旧图仍按E-SHCD-01旧观察保留，本次没有重复计为新观察。
