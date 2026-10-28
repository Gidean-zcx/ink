# M-058 Tolino：shine第五代T302，家庭书库与借阅状态

查阅2026-10-27。固定**黑白tolino shine第五代T302**，英文2024年12月指南与现行德/英FAQ分开。英站列5.12.232736，德文型号更新表列5.18.264769且有表内疑点；均非实机已安装构建。地区、联盟书店和图书馆资格分别适用，不混用shine3/4、color或vision的操作。

本案例含19项任务，其中设备及账户任务来自D2文档/静态页；**唯一D4是桌面匿名公开图书馆选择器**，不代表T302实机、账号登录或借阅已验证。[逐条目录](../batches/tolino-controls-deepening-inventory.csv)与[来源摘记](../artifacts/tolino-controls-deepening/source-notes.md)保留父项、重复问题和未深读条目，不是全功能完成率。

### 本批第三方证据补充（2026-10-28）

- 新增 E‑TLCD‑08：PIN 与忘记 PIN 的官方帮助条目；hands_on=false。
- 新增 E‑TLCD‑09：恢复出厂设置/专用 USB 步骤（更新页章节）；hands_on=false。
- 新增 E‑TLCD‑10：Onleihe/DRM/Reader code 与最低版本边界；hands_on=false。
  - 覆盖台账：F‑M058‑18 补 E‑TLCD‑08；F‑M058‑07 补 E‑TLCD‑09；F‑M058‑08 补 E‑TLCD‑10。

## 已有任务的分支补强

| 任务 | 条件、动作和数据变化 | 缺口与证据 |
| --- | --- | --- |
| F-M058-01 开书、回首页与睡眠续读 | 睡眠回原屏幕/页面，完整关机再开回首页；睡眠会关闭WiFi并可能中断同步。 | 未保存编辑、睡眠中断后重试与书签同步结果未核。E-TOL-01；D2 |
| F-M058-02 自有电子书导入与离线读取 | USB支持PDF/EPUB/TXT/MOBI/CBZ/CBR复制至Books并安全弹出；云上传EPUB/PDF，同步后点封面下载再离线读；封面可见不等于本机有文件。 | 重名、半文件、云配额/冲突及自传书的跨端进度未核。E-TOL-01、E-TOL-02、E-TOL-06；D2 |
| F-M058-03 正文查词/翻译 | 正文长按词查词/翻译；More Settings Language & Dictionaries→Installed Edit；加号增词典、取消勾选删后Save。 | 无命中提示、取消与回原句、词典包离线和生词练习未核。E-TOL-02；D2 |
| F-M058-04 蓝牙有声书播放 | 蓝牙Connected后My Books打开有声书；播放/停、前后30秒、时间条、章节、1x速度、音量及章/全书剩余时长；章节需下载；播放三点有Details/Dictionary；配对失败两端关开蓝牙重试。 | 断连/重启续播、收听进度同步、与正文锚点对齐及实际声输出未核。E-TOL-01、E-TOL-02、E-TLCD-02；D2 |
| F-M058-05 电子书TTS开启和停止 | 页中→三点Read Aloud，原菜单Stop；德文8.2限定Kobo EPUB/EPUB。 | 语言/音色、离线包、最低构建、PDF支持和读听定位未核；不得把Screen Reader条件套此入口。E-TOL-02、E-TLCD-02；D2 |
| F-M058-06 移除本机与删除云藏书 | 长按封面Remove选择本地或cloud；本地默认只去下载且封面保留，云删除影响多端；Manage books可多选/全选；误删云书需联系书店。 | 客服恢复不保证；自传书删除回收、断网传播、撤销及注释连带变化未核。E-TOL-01、E-TOL-02；D2 |
| F-M058-07 同步后恢复与重置 | 手动Sync now或关闭电子书自动同步，后者需两端在线；最多五设备是账户限制。普通强制关机不等于重置。退出账户需先同步未保存书签/笔记；重置清本机书/侧载文件/未同步注释及设置。手动恢复需接电、按至关机再长按至LED快闪结束，出现Restore后重设联网。 | 同步冲突、未联网登录、恢复完整性、刷机回退和侧载原件备份未核。E-TOL-01、E-TOL-02；D2 |
| F-M058-08 OverDrive借阅下载与授权错误 | OverDrive按原服务ByteBooks迁移；Onleihe当前T302从5.15.253009 More→Onleihe→选城市/馆→登录→搜书Borrow/OK→打开；LCP首次四位reader code。三点或长按封面Return；只直接借电子书。帮助称清Web-Browser-Daten可清最近馆，但范围不止选馆未证明。 | 两服务授权不可混用；Onleihe直接登录在桌面选馆器本次未实现，见F-M058-19；实际T302借书/到期/断网仍未核。E-TOL-04、E-TOL-05、E-TLCD-04、E-TLCD-05；D2 |
| F-M058-09 按型号和原版本更新固件 | 优先联网同步检查更新；USB依型号/当前版本选文件、电量>20%、连接确认、KoboRoot.tgz/update.tar放.kobo安全弹出后自动重启。英站仍5.12.232736，德文T302表列5.18.264769；FAQ的update.zip与专页不一，德文表亦有版本拼写/非递进目标疑点。 | 未下载固件；不把网站current当装机版本；升级链冲突、失败回退和数据完整性需官方/实机核实。E-TOL-02、E-TOL-03、E-TLCD-06；D2 |

## 本轮补入的操作任务

### F-M058-10 按书库或商店范围检索标题作者

首页放大镜→下拉选Shop或My Books→输入标题/作者→Go；后者搜索账户及本机内容。

D2；E-TOL-01。尚未核实：空结果提示、去重、全文检索、离线商店回退未核。

### F-M058-11 建立与维护主题收藏集合

My Books→Collections→三点New collection→命名选书；长按集合Edit→Add books加号加入/取消勾选移出；长按Rename/Delete；按名称/书数/创建日排序；文档称可在app/web看集合。

D2；E-TOL-02。尚未核实：重名、空集合、删除是否影响书、离线冲突及撤销未核。

### F-M058-12 目录跳转与书签回访

阅读页中→滑杆快速翻页，或底部目录→选章；点正文右上加书签可见标记；回访页中→底部纸页图标→Bookmarks，手册第三步措辞不清。

D2；E-TOL-01、E-TLCD-01。尚未核实：书签列表完整顺序、删除/撤销及跳转后回原处未核。

### F-M058-13 排版与系统大字分别调整

阅读页中→文字图标→字体下拉、字号滑杆或加减、行距/边距加减、对齐；点外部回阅读；设置应用后续书；PDF只缩放；系统Accessibility另设150/200%菜单大字。

D2；E-TOL-01、E-TOL-02。尚未核实：极端字号重排、阅读位置稳定性与格式例外未核。

### F-M058-14 家庭成员建立、按书分享与退出

More→Settings→Accounts→My Family View；昵称→设备交给邀请对象选书店并登录→昵称；合资格书长按Share选一个成员，可重复；Info查看访问者；各自进度笔记，完整书库互不可见；成员旁菜单移除，自己菜单离开撤销相互共享访问。

D2；E-TOL-02、E-TLCD-03。尚未核实：未实际建家庭；离线撤权时点、孩子独立账号资格、退出后注释保留和有声书分享当前控件未核。

### F-M058-15 连接同一用户的多个书店书库

Webreader My Accounts→Library link→选已有账户书店Link→用该店凭证确认；App先选国家/书店再同路径；成功后经cloud载入购买书。

D2；E-TOL-02、E-TOL-06。尚未核实：地区条件、解除关联、重名去重、不同DRM及认证失败恢复未核；不等同家庭档案。

### F-M058-16 蓝牙键盘辅助读屏与退出阅读

需BT音频和键盘；More Settings Accessibility Screen Reader开启或Alt+Windows/Command+S；箭头选书，空格下载/开书；Ctrl+T读/停，箭头翻页，Ctrl+Q回My Books。

D2；E-TOL-02。尚未核实：焦点顺序、断连恢复、首次下载反馈、语音资源离线和儿童操作均未核。

### F-M058-17 自制休眠图启用与撤销

USB连接确认→.kobo/screensaver放PNG/JPEG→安全弹出；Energy saving & PIN中当前书及全屏两项开启，先开一本书再睡眠；多图随机；移除图文件回书封面或关闭两项显示。

D2；E-TLCD-02。尚未核实：分辨率/裁切、锁屏信息遮挡、文件损坏与批量同步未核。

### F-M058-18 PIN设置、解锁与遗忘恢复

More Settings Energy saving & PIN→设四位码；Change PIN可关闭；忘记密码从PIN提示Log out并确认，退号后重新设置。

D2；E-TOL-02。尚未核实：退出会丢最后同步后阅读进度/批注；尝试次数、锁定时间、未联网恢复未核。

### F-M058-19 公开选馆器搜索、返回与最近项

匿名桌面：选择图书馆→无匹配词空列表→Hamburg两结果→选Bücherhallen Hamburg跳外站welcome仍需选馆→浏览器后退保留词但列表A字头→左上返回→刷新出现最近图书馆；没有登录。

D4；E-TLCD-04、E-TLCD-05、E-TLCD-07。尚未核实：只限桌面公开选择器；未借阅、未测跨会话/设备或清数据；无法判定外部重新选馆原因。

## 版本、服务与数据边界

1. 同账户最多五设备与家庭最多六人是不同维度。家庭各自账户、各自读书位置与笔记，内容共享不等于共享完整书库；自传书/PDF不因家庭关系自动获得分享能力。当前T302音频帮助不能被旧家庭FAQ“阅读器不能听书”覆盖。E-TOL-02、E-TLCD-03。
2. 普通TTS、辅助Screen Reader、有声书各有入口和条件。辅助读屏需蓝牙键盘+音频，不能据此认定所有TTS也必须键盘；德文普通TTS限定EPUB。听书位置与阅读位置未证明逐句对齐。E-TOL-01、02、E-TLCD-02。
3. 退出账户、PIN遗忘、菜单出厂恢复和电源键手动工厂恢复都可能丢未同步数据；普通强制关机是另一种行为。睡眠中断联网同步不能自动解释为数据已经备份。E-TOL-01、02。
4. 2026 OverDrive使用ByteBooks ID的迁移与既有Adobe例外只适用于该服务；Onleihe的LCP四位码、借期及图书馆登录另算。Tolino借阅帮助保留旧OverDrive Adobe段，不能取代服务现行说明。E-TOL-04、05、E-TLCD-05。
5. 更新表与FAQ文件名冲突尚未解决。德文T302表253099/253009不一致，5.13.241423到5.15.245253区间却给5.13.241423目标；不改名试刷、不推导完整升级链。E-TOL-02、03、E-TLCD-06。
6. p39实际查看发现书签入口图标存在；不能把文字提取漏图当成产品没按钮。原句“Next to the bookmark...tap Bookmarks”仍不清，保留列表操作缺口。E-TLCD-01。

## 公开选馆器：观察与帮助的差异

[操作审计](../artifacts/tolino-controls-deepening/browser-operation-audit.md)保存10次状态、6张必要图、命令与原件散列。搜索空列表没有可见无结果说明；选Hamburg后实际落在外站欢迎页仍需选馆。后退时输入Hamburg而列表从A字头开始；回首页刷新后显示最近选择，期间未登录。这个记录证明“最近选择”和“认证成功”可分离；不证明服务器保存、跨会话保留或所有图书馆都同样跳转。初始过渡帧和截图/DOM时间差单列，没把空白加载视作借阅失败。E-TLCD-07。

## 对本项目的参考与剩余缺口

以下为助手推断，未升级需求：儿童与成人可以共用内容，但个人进度、学习记录和共享权限宜分别建模；AI应能判断某个封面代表本地文件、云记录还是借阅授权，执行“清理”时应说明对象。读听切换需要稳定原文锚点，不能把多个播放器都能播放等同于支持无缝续听。最近选择、登录和版权码应作为不同可查询状态。

尚缺批注编辑/导出、书内全文搜索、删除撤销、音频/阅读互跳、云冲突、真实T302图书馆和升级恢复链；采购资格、地区和商业服务条款也未替本项目确认。下一轮可用无DRM自有短书在真实T302核导入→查词→回原句→TTS→睡眠，再按合法测试账户核分享撤权与未同步数据恢复；本轮未执行这些设备任务。

## 证据入口

- E-TOL-01：[tolino shine T302 英文用户手册](https://mytolino.de/wp-content/uploads/2024/12/DEC24_including-EU-Common-Charger-Directive_tolino-shine-T302-User-Guide-EN.pdf)；PDF页5—6、18、23、25—29、37—39、44—47。官方PDF下载后用现有PyMuPDF核读；正文流程，不是设备操作或视觉观察。
- E-TOL-02：[shine 第五代FAQ](https://mytolino.com/faq-group/tolino-shine-5-generation-en/page/2/)；2.1/2.3/2.4/2.6；6.1—6.2；9.7；3作目录边界。官方正文；未注明构建。FAQ更新文件名与软件更新专页不一致；不以FAQ推定旧固件都有TTS。
- E-TOL-03：[Tolino 软件更新专页](https://mytolino.com/software-updates-for-tolino-ereaders/)；shine color/shine5分组；手动更新该两型号例外段。官方正文；列表当前5.12.232736，下载选择还依已有版本。未下载/安装固件。
- E-TOL-04：[OverDrive Tolino设备页](https://help.overdrive.com/En-us/9983.html)；支持设备示例；Getting started；Last Updated。当前正文列shine5及浏览器下载；本轮末次打开显示2026-10-06。搜索索引曾保留需电脑ADE的旧句，未采用为当前结论。
- E-TOL-05：[OverDrive 在Tolino下载和错误恢复](https://help.overdrive.com/En-us/2305.htm)；First authorize；Then download；Resolving errors。2026-10-23正文含ByteBooks ID迁移及旧Adobe例外；仅限该服务，不推广到Onleihe或所有DRM商店。
- E-TOL-06：[Tolino webreader说明](https://mytolino.com/tolino-webreader/)；浏览器读书/内容和设备管理；地区提示。正文；本次页面显示所在国家不可用提示，未访问账户。不能据该提示推断用户真实地区资格。
- E-TOL-07：[shine第五代功能表](https://mytolino.com/wp-content/uploads/2024/04/ENG-2023_01-Datenblatt_tolino_shine.pdf)；PDF第1页：Included in box明确5th Generation；Functions。正文功能目录D1；PDF标题元数据沿用shine3，正文另有shine color笔误。仅取型号明确的第1页，不混用彩屏/笔功能。
- E-TLCD-01：[T302原手册p39书签图标与回访措辞](https://mytolino.de/wp-content/uploads/2024/12/DEC24_including-EU-Common-Charger-Directive_tolino-shine-T302-User-Guide-EN.pdf)；PDF50页；p2–4目录；p39实际查看；阅读章节见摘记。同E-TOL-01原件；图标存在，第三步英文措辞不清；不是设备操作。p32/36空白不作界面证据。
- E-TLCD-02：[当前德文T302 FAQ与休眠图/TTS条件](https://mytolino.de/faq-gruppe/tolino-shine-5-generation/)；1.3、5、6、7、8；全标题登记但部分正文深入阅读。不把其他代际条件继承到T302；8.2限定Kobo EPUB/EPUB；自制图需先开书。
- E-TLCD-03：[家庭分享：成员、选书、权限与离开](https://mytolino.de/faq-gruppe/faq-family-sharing/)；1.1–1.29。旧1.20音频限制与T302冲突；仅按适用范围采用。
- E-TLCD-04：[新Onleihe入口与最低版本](https://mytolino.de/onleihe-neu/)；新版入口、支持型号、5.15.253009、清数据及媒体范围。不把服务登录、最近图书馆和DRM授权合一。
- E-TLCD-05：[按代际的Onleihe借阅步骤](https://mytolino.de/anleitung-onleihe/)；A当前代I–IV；B旧代仅作范围区分。OverDrive旧Adobe段与2026服务现行文档有冲突；不以该段覆盖E-TOL-05。
- E-TLCD-06：[德文T302更新表与专用USB步骤](https://mytolino.de/software-updates-tolino-ereader/)；shine5/shine color版本表；该代专用USB步骤。列5.18.264769；表内253099/253009及非递进目标疑点；未下载或安装固件。
- E-TLCD-07：[公开图书馆搜索、跳转、后退与最近选择](https://mytolino.de/onleihe/)；01–10；6正式截图；7图已查看含1空白过渡帧。只验证公开选馆器有限任务；没有登录/借阅；外部落点与帮助理想流程不同，原因未知。

## 结构化竞分（2026-10-28）

- **对象与信息架构**
  - **内容对象**：本机书/云端条目/借阅授权（OverDrive/Onleihe）分离；“封面可见”≠“本机已有文件”。
  - **读听对象**：正文阅读/普通TTS/辅助读屏/蓝牙有声书分离；位置锚点不自动共享。
  - **账户对象**：单账户多设备、家庭分享、书店关联为不同层；PIN/退出/恢复为独立安全与维护对象。

- **关键任务流（D2/D4）**
  - D2：自传书（USB/云）→下载→阅读→查词/书签→移除本地/云。
  - D2：蓝牙配对→有声书播放/控制；TTS 开/停（限定格式）。
  - D4（公开网页）：Onleihe 选馆器 搜索→选择→外站跳转→后退→最近图书馆出现（不含登录/借阅）。

- **失败与恢复边界**
  - 同步冲突、重名/半文件、断连续播、读听锚点对齐未证；退出/重置丢失未同步数据。
  - 更新表/FAQ/USB步骤存在版本/文件名冲突；不推导统一升级链。

- **对 E‑Ink4 Plus 与 5–6岁+成人家庭的启发与风险**
  - 启发：显式对象边界（本地/云/借阅）与“最近选择≠已登录/已授权”；多播放器状态分离。
  - 风险：把家庭分享当成全库共享；把辅助读屏键盘依赖误推到普通TTS。

- **证据状态**
  - 深度：D2 主链 + 一条 D4（网页选馆器）；不声称设备登录/借阅 D4。
  - 参考：E-TOL-01～07、E-TLCD-01～07。
