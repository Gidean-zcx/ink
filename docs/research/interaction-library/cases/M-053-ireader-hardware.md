# M-053 掌阅硬件：Ocean、Smart、Tango 的型号分支与操作覆盖

查阅2026-10-26；无设备操作、登录、安装或升级。Ocean4 长续航版2025保留原静态观察；新增 Smart4 Pro 的历史官方更新说明与 Tango 2 系列静态图。**D2仅表示文字描述能定位特定任务，不表示连续观察或实测。**没有取得三条产品线的完整机内帮助，下面是功能任务目录及覆盖状态，不宣称全系统功能已穷尽。

## 型号、版本和证据范围

| 分支 | 固定范围 | 能确定及不能外推的边界 |
| --- | --- | --- |
| Ocean | Ocean4 长续航版2025，中国市场，SmartOS构建未知 | 原 E-IHW-01—04 的认证店正文/三张静态图；不混Ocean4 Turbo、Ocean5。2025-01更新名单只写“Ocean4长续航版”，**不能据此认定2025款已用2.5.0** |
| Smart | Smart4 Pro，官方2025-01-24所列 SmartOS 2.5.0 更新范围 | 名单还列Smart4，但本案例具体任务固定Smart4 Pro；完整build及当前最新版本未知，不并入SmartX3 Pro、Smart5/6。手机掌阅App构建未知；账号与套餐前提未完整说明。E-IHWD-01 |
| Tango | Tango 2（封面明确命名）；系列详情同时陈列多种外观，具体配置未逐项区分 | E-IHWD-02—04仅证实型号/外观和系列示意入口；共享图不能将每个图标都归到某个配置。原代Tango与Tango 2 Pro保留为待查分支，未套Smart4 Pro的2.5.0操作 |

E-IHWD-01来自“掌阅iReader官方账号”发布的B站**简介正文索引**，不是视频字幕或连续画面；直接正文返回412，未绕过。原始搜索返回留在 `work/chinese-reader-deepening/ireader-2.5-search-result.txt` 供核查。E-IHWD-05为2022年官方导出教程简介，机型不明；E-IHWD-06为2.4更新简介，缺完整适配名单，均只用于定位后续资料。

## 功能目录与覆盖

| 功能域 | 已查章节/入口 | 已关联 F/E | 当前缺口 |
| --- | --- | --- | --- |
| 首启、首页、全局返回 | Ocean商品图；Tango 2系列桌面示意 | F-M053-01/03；E-IHW-04、E-IHWD-03/04 | 首启引导、账户跳过、任务切换、返回层级均未闭合 |
| 书城、书架、文件、传入 | Smart2.5“PDF”“AI速读”；Tango图有文件管理 | F-M053-01/02/04；E-IHWD-01/04 | USB/WiFi/网盘导入的入口、重名、扫描、打开失败；不能把“导入PDF”当成传输教程 |
| 阅读、字体、目录、查词与划线 | Ocean排版/旋转；2.4简介提字体 | F-M053-01；E-IHW-01、E-IHWD-06 | 字体导入、词典/生词、划线回原文、阅读位置同步的同型号操作待查 |
| PDF、OCR、裁边、重排 | Ocean重排静态图；Smart2.5重排段落 | F-M053-02；E-IHW-02、E-IHWD-01 | 原页/识别文本/重排视图的数据关系、误识别修改及取消 |
| 笔记、标题、页码、插图编辑 | Smart2.5笔记目录/笔记优化段落 | F-M053-06；E-IHWD-01 | 新建到保存完整链未查；插图撤销/恢复只列目录，未新增流程 |
| 笔记同步、复制、导出 | Smart2.5云同步；2022导出教程简介 | F-M053-07；E-IHWD-01/05 | 同步开关、账号校验、配额、冲突、离线重试；历史导出渠道不能直接套本机 |
| AI、速读、思维导图 | Smart2.5部分书城书籍；Ocean宣传 | F-M053-04；E-IHW-01、E-IHWD-01 | 原文引用、改指令/重生成、编辑回写、费用及离线边界 |
| 听书、朗读、录音 | Ocean宣传；Tango系列桌面录音图标 | F-M053-05；E-IHW-01、E-IHWD-04 | 播放器、定时、音频路由、后台/休眠中断、转写/导出均未取得同型号步骤 |
| 实体键、手势、前光、刷新 | Ocean七区域和键图；Tango外观；2.5图像优化段落 | F-M053-03；E-IHW-03/04、E-IHWD-01/03 | 区域默认动作、手势退出、第三方App冲突、刷新回退；图像优化不能等同所有刷新模式 |
| 第三方应用、权限与网络 | Tango系列桌面“应用市场”；2.4简介有应用适配条目 | E-IHWD-04/06；未立F | 安装/卸载、权限拒绝、后台保活、网络异常未查；图标不证明任意App兼容 |
| 更新、删除、账户与恢复 | Smart2.5升级段落 | F-M053-08；E-IHWD-01 | 更新失败/回滚、账号切换、删书/删笔记恢复、重置后云/本地关系未知 |

## 既有任务补证与分支

| F 编号 | 前置与动作 | 反馈、数据及结束边界 | 深度和证据 |
| --- | --- | --- | --- |
| F-M053-01 本机阅读 | Ocean图示阅读、旋转与右侧双键；Tango系列图示书城和桌面 | 没有导入→开书→返回→续读链，也未测翻页时间 | D1；E-IHW-01/04、E-IHWD-04 |
| F-M053-02 PDF重排 | Ocean仍仅静态示意。**Smart4 Pro/2.5.0：已导入PDF→打开→重排→选在线或离线** | 结束在重排方式选择；未证明OCR成功、进度反馈、取消、重启后保存；“离线OCR”不能扩张为所有AI离线 | Smart分支文档D2、Ocean分支D1；E-IHW-02、E-IHWD-01 |
| F-M053-03 输入映射 | Ocean图示七区域与自定首页/全刷；Tango 2封面可见机身侧键 | 无绑定设置或恢复默认步骤；外观不证明短长按及应用行为 | D1；E-IHW-03/04、E-IHWD-03 |
| F-M053-04 AI阅读 | Ocean仍为能力声明。**Smart4 Pro/2.5.0：书城内合资格书→目录→速读/思维导图** | 官方仅说部分书提供内容；这里是查看入口，不证明任意导入文档可生成，也未核生成物是否回写原书 | Smart分支文档D2、Ocean分支D1；E-IHW-01、E-IHWD-01 |
| F-M053-05 听读与录音 | Ocean列AI朗读及音频硬件；Tango系列图有录音入口 | 未知录音与朗读是否共用控制；不能据麦克风/图标推出转写联动 | D1；E-IHW-01、E-IHWD-04 |

## 新增可定位任务

以下三项仅限 **Smart4 Pro/SmartOS2.5.0**，E-IHWD-01。系统反馈用文档所述终点表达，没有观察动画、耗时或结果质量。

| F 编号 | 前置/操作 | 反馈和数据状态 | 结束、异常与未核实 |
| --- | --- | --- | --- |
| F-M053-06 笔记定位，D2 | 已有笔记；更多→页面；或套索文字→H建标题，再按标题跳转；也可点底部页码输入 | 页列表/标题/页码提供三种定位；标题操作属于笔记结构变更 | 已到目标页；无标题、重名、识别错字、删除标题及跳转返回栈待查。插图编辑与撤销只在目录记录，不扩成已分析任务 |
| F-M053-07 查看同步笔记，D2 | 手机掌阅App：我的→我的书籍→笔记本；前提为已有同步笔记 | 终点是手机查看；文档没有提供从设备启动上传的完整链 | 两端账户匹配、传输中/离线/冲突、手机是否可改原稿未知；本地副本与云端删除联动未知 |
| F-M053-08 检查系统更新，D2 | 联网；设置→系统和更新→系统更新 | 文档说检测2.5.0；这里只分析历史版本的检查入口 | 不声称2026仍推送2.5.0；下载/电量门槛、安装确认、失败恢复、降级及用户数据保留未知 |

## 分支、观察与下一证据

空书架、重复导入、损坏文件、OCR低置信度、误划线撤销、离线AI、同步冲突、删后恢复与账户退出均缺同型号证据。Smart2.4中字体、云空间和应用相关功能名称有助于定位资料，但适配范围不清，不能用于填满三条机型线。2022官方教程提到笔记导出渠道；需要Smart4 Pro的实际导出菜单及目标文件验证后才能升级覆盖。

Tango 2详情第1/2图仅做**单帧静态观察**：第1图明确命名并画出侧键；第2图可见文件管理、换机助手2.0、录音、词典、应用市场等入口。图中多设备配置未逐项注明，不证明功能切换成功。原Ocean三张观察继续有效：[PDF](/home/workspace/reader/docs/research/interaction-library/artifacts/extended-readers/ireader-ocean4-2025-detail-16.jpg)、[手势](/home/workspace/reader/docs/research/interaction-library/artifacts/extended-readers/ireader-ocean4-2025-detail-20.jpg)、[按键](/home/workspace/reader/docs/research/interaction-library/artifacts/extended-readers/ireader-ocean4-2025-detail-26.jpg)。没有D3/D4。

下一证据按分支获取：Ocean4长续航版2025的“关于设备+机内帮助”确认构建；Smart4 Pro用已有短笔记验证同步状态、断网和删除；Tango原代、Tango 2与2 Pro分别取得按键/传书/应用/听读帮助。没有合法公开手册的部分保留待查，不从购物图或后代固件推定。

对G-001/G-002及Plus的推断：本项目应区分原页、OCR文本、重排视图、目录标题与AI生成物，并给出可预测的回原处。儿童读听需稳定返回与暂停，成人笔记需能知道本地/云端哪份已保存。可编辑、可导出和可恢复是不同问题；本次来源不能证明学习效果、奖励机制或性能。这些是项目分析，不是已确定需求。

## 证据

- E-IHW-01：[Ocean4长续航版2025认证店](https://detail.youzan.com/show/goods?alias=1y43umgr1e67n0i&from_source=gbox_seo)，店铺“微信认证”是归属标识，商品陈述非实测。
- E-IHW-02：[Ocean PDF静态图](https://img01.yzcdn.cn/upload_files/2025/04/23/Fpjy5-YSmrL39amP6rZ4tUqmxI9-.jpg)，详情16。
- E-IHW-03：[Ocean手势静态图](https://img01.yzcdn.cn/upload_files/2025/04/24/FsF-7MceezhW-9HxW8dzHaKROQEl.jpg)，详情20；原静态观察及归档保留。
- E-IHW-04：[Ocean侧键静态图](https://img01.yzcdn.cn/upload_files/2025/04/23/FqUYk6TYFG4-Ss7jT8HZQCbrvT0l.jpg)，详情26。
- E-IHW-05：[2021官方技巧条目](https://www.bilibili.com/video/BV1eR4y1s7JR/)，只读元信息，未用于2025机型操作。
- E-IHWD-01：[官方SmartOS2.5更新简介](https://www.bilibili.com/video/BV1v7f7YhEGn/)，2025-01-24；型号名单及六组更新段落，搜索索引正文；直接访问412。
- E-IHWD-02：[Tango 2系列认证店](https://detail.youzan.com/show/goods?alias=36489xybpq8pfjz&from_source=gbox_seo)，店铺身份及系列标题，不使用价格/库存作结论。
- E-IHWD-03：[Tango 2封面原图](https://img01.yzcdn.cn/upload_files/2025/10/15/Fgpc1ircRrX5ieoOC_Nvyk-drM4I.jpg)；[归档](/home/workspace/reader/docs/research/interaction-library/artifacts/chinese-reader-deepening/tango2-cover.jpg)。
- E-IHWD-04：[Tango系列桌面示意](https://img01.yzcdn.cn/upload_files/2025/10/15/FsUtGYW9nh-ONoxBPQUPH7bVCC_p.jpg)；[归档](/home/workspace/reader/docs/research/interaction-library/artifacts/chinese-reader-deepening/tango2-overview.jpg)。
- E-IHWD-05：[官方笔记导出教程简介](https://www.bilibili.com/video/BV1xt4y1K78M/)，2022-11-06；机型未知，只保留历史目录。
- E-IHWD-06：[官方SmartOS2.4更新简介](https://www.bilibili.com/video/BV1DQSKYKECB/)，2024-11-20；未取得适配名单，不据其升级型号流程。
