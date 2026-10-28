from pathlib import Path
import csv,json,hashlib,shutil
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library';w=root/'work/ak-controls-deepening'
a=lib/'artifacts/ak-controls-deepening';a.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
case='''# M-065 Astell&Kern SR35：文件夹书签、网络下载与音频角色

## 范围与证据边界

查阅2026-10-26。固定 **SR35、V1.22发布基线**；实际构建、底层Android版本及第三方app构建未知。全球V1.22索引标2025-07-14，日区条目创建2025-07-08、更新2025-07-15，日期分别保留。日本型号支持页本次仍列V1.22，但不是本项目设备版本检查。[E-AK-03/04/12]

本轮从官网手册索引所链的官方CDN成功取得 **73文件页英文原PDF**，索引发布日期2023-05-02；文件元数据修改时间2023-03-27，不能当作固件号。此前“原PDF未取得”仅保留在旧证据历史，本例已不依赖镜像解释图标。手册明确设置会随固件变化，旧图不能证明V1.22界面原样保留。[E-AK-01/02；E-AKCD-01]

D1/D2文档研究。已实际查看印刷24、30、48、59、60页静态摘页；同页多图不算连续流程。没有设备操作、账号登录、APK/固件执行或耗时测量。共用FAQ若示范其他机型，不视作SR35照片或当前实机记录；原手册、镜像及摘页仍是一份来源。

## 功能目录与覆盖

表按 **印刷页**，PDF文件页=印刷页+1。完整目录已核对，操作章节按需读取；法律/认证页不展开。原案例把Open APP、AK Connect和故障页码混淆，本次统一为27、28–32及66。只列功能名/页码不等于已完成任务。

| 功能域 | 手册章节/官方补充 | F / E 对应 | 已查及尚未覆盖 |
| --- | --- | --- | --- |
| 附件、物理键、充电与电源 | 2–7；故障66 | F11/16；AKCD-01、AK-11 | 正常开关/关屏与强制关闭分开；熄屏误触和最后保存点未实测。 |
| 电脑、文件、存储卡 | 8–11；Folder37；System Information56 | F01；AKCD-01 | USB复制、电脑删除确认、卸载卡有文档；设备端copy/move/remove有入口，未闭合覆盖、撤销及旧引用修复。 |
| 升级、版本、数据重建 | Update12/56；System Reset57；V1.22发布 | F10；AKCD-01、AK-03/04 | Media Scan初始化、设置重置、Factory Data Reset为三个对象；确认/取消、保留清单未知。 |
| 全局与分类导航 | Basic13–16；Home17；Category18–19；Menu20 | F16；AKCD-01 | Home、边缘入口和Mini Player返回可定位；分类自定义、返回滚动位置未验证。 |
| 播放页、查找与排序 | Player21–23；Listening35–37 | F02/16；AKCD-01 | 歌词、进度、剩余时间、Next/Last及分类搜索有描述；坏标签、精确章节、时间书签仍缺。 |
| 通知、快捷设置与锁键 | Notification24–26；Panel53；V1.22 | F04/10；AKCD-04、AK-04/09 | 旧轮锁与新All Lock不能混为一张已观察界面；当前全图标、隐藏/满额、屏幕锁定时侧键待测。 |
| 应用与账号 | Open APP27；Update56；官方兼容表 | F06/07；AK-06/07、AKCD-01 | 默认应用更新与Open APP更新不是一条通路；登录/订阅、离线到期、卸载数据和后台抢音未知。 |
| 局域网库、扬声器与下载 | AK Connect28–32；Downloads56 | F12；AKCD-01/03 | 连库、流播、下载及中止入口已补；服务器发现失败、下载续传/重名和离线回查未知。 |
| FTP传入/传出 | File Drop33–34 | F05；AKCD-01、AK-05 | SR35型号手册独立说明凭据与匿名分支；权限失效、覆盖和会话退出未实测。 |
| 持久歌单、文件夹书签、收藏 | Playlist38–40；Bookmark48；Favorites49 | F02/03；AKCD-01/02 | 书签是最多5个文件夹入口，收藏为歌曲引用；第6个书签、删除源文件与恢复待查。 |
| ReplayGain、DSP、音量 | ReplayGain41；DAC/AMP51；EQ/Gapless/Line Out52；Balance53；Limit54；EQ62–63 | AKCD-01/11；无新增F | 各自作用域可定位；EQ导出文件不等于整机备份，校准取消/数据保存和第三方app范围未闭环。 |
| USB输入与数字输出 | USB DAC42；USB Audio43–44；Mode53/Audio54 | F14；AKCD-01/07/09 | 电脑输入与外接DAC输出方向不同；输入模式侧键限制有文档，断线/切回进度待测。USB-C耳机限制见官方FAQ，不泛称所有USB外设可用。 |
| CD附件、车载简化界面 | CD Ripping44–45；CD-Player46；Car47 | AKCD-01；无新增F | 目录与打断边界已查：抓轨期间禁播放/AK Connect，完成后后者恢复；未研究当前附件兼容或完整异常链。 |
| 无线连接与接收 | Wi-Fi58；Bluetooth59–61；Codec50 | F08/15；AKCD-05/06/08/10 | 发射/接收、断开/移除配对分开；共用Sink FAQ与型号手册入口差异保留。 |
| 计时、显示与个性化 | Display/Name54；Language/Input/Timer55 | F13；AKCD-01 | Sleep Timer、Auto Power Off、Screen Timeout效果不同；时长选项、重复计时及恢复未知。 |
| 重置、故障、其他 | System Reset57；Precautions64–65；Troubleshooting66；版权/认证67–71、规格72 | F10/11；AKCD-01 | 仅按章节索引；不把格式化/恢复出厂建议当作无损修复，不从规格推导实测表现。 |

表中 AK 是旧证据前缀，AKCD 是本批；没有借其他A&K机型补菜单。完整“现版全部功能已验证”仍未成立。

## 任务流程

旧F12原为三类能力混列，本次保留编号用于AK Connect下载；计时与USB DAC分别成为F13、F14，不重复统计原混合行。

| F 编号 | 任务/前置 | 入口、操作与文档/图示反馈 | 数据、退出与未核实 | E 证据 |
| --- | --- | --- | --- | --- |
| F-M065-01 | USB传入与安全结束；电脑/数据线 | Settings/USB Mode选Media Device→复制→完成后断线。移卡由通知栏Safely Remove或System Information/Unmount。 | 复制不证明全部索引成功；传输中断、同名覆盖、坏文件和移动后歌单修复未知。 | E-AK-02；E-AKCD-01 |
| F-M065-02 | 队列与持久歌单 | More选Play Next/Last；Playlist的+→命名/Add；歌单内选曲/Complete。移除成员需DELETE确认，手册明确不删存储音频。 | 歌单重命名/整表删除另有编辑入口；撤销、当前项移除后的播放、顺序持久化仍未知。不要把文件菜单Delete按同一规则解释。 | E-AK-02；E-AKCD-01 |
| F-M065-03 | 添加/移除常用文件夹书签（D2） | Folder分类找到文件夹→右侧⋮→Add Bookmark；印刷48图示Bookmarked，书签列于顶部。按已标记文件夹左侧书签图标移除。 | 最多5个；移除的是快捷入口，不是音频时间点。第6个如何拒绝/替换、移动目录后的路径和跨重启保留未核实。 | E-AK-02；E-AKCD-02 |
| F-M065-04 | 调整下拉快捷入口 | 顶缘下拉通知栏；Settings/Notification Panel拖放添加、移除、排序。 | SR35手册53可独立支持此入口；旧图24仅提供历史布局。当前All Lock图标、满额/重置未知。 | E-AK-09；E-AKCD-01；E-AKCD-04 |
| F-M065-05 | 无线传文件；同网设备 | 启用AK File Drop→FTP客户端输入SR35显示的地址/ID/密码→传入/传出；型号手册另说明匿名FTP仅可下载。 | 匿名下载指从SR35向客户端取文件；不是匿名上传授权。Windows具体网络位置流程仍来自SE180示例FAQ；覆盖、权限、断线及关闭会话未知。 | E-AK-05；E-AKCD-01 |
| F-M065-06 | 安装受支持服务app | APK复制至OpenService→断开电脑→Services→安装→打开。 | 安装包、app数据与服务账户不同；共用FAQ示范SP1000M。SR35当前权限弹窗、安装失败、卸载保留未知。 | E-AK-06；E-AK-07 |
| F-M065-07 | 应用兼容性条件（D1） | 依SR35所在分组查截至2025-05-01的已测试版本表。 | 曾测试不等于2026能登录/获离线授权；失败码、回退版本、服务订阅另需核查。 | E-AK-07 |
| F-M065-08 | 手机音频经SR35播放 | 型号手册图：Settings/Bluetooth内Bluetooth Sink Mode→手机搜SR35并连接→手机播放；开启Sink会断开原已连设备。 | 共用FAQ先开AK Connect的不同入口保留，不把它强加为型号前提。接收来源掌握编解码设置；手机来电/断开后的原曲与音量未知。 | E-AK-02；E-AK-08；E-AKCD-06；E-AKCD-08 |
| F-M065-09 | M3U路径兼容性（D1） | 官方FAQ提到Android路径差异。 | 未给完整导入/修复；编码、相对路径、重扫去重及批量修复未知。 | E-AK-10 |
| F-M065-10 | 升级与锁键版本核对 | V1.22发布要求备份、电量>50%、空间>500MB；Settings/Update/System update。旧手册Wheel Lock控制熄屏音量轮；V1.22扩展侧键/All Lock。 | 发布页说更新初始化用户设置；不能拿旧手册20%门槛替代版本专用条件。现版开关图、逐键行为、数据保留及更新中断回退未知。 | E-AK-03；E-AK-04；E-AKCD-01；E-AKCD-04 |
| F-M065-11 | 无响应恢复 | 电源约7秒强制关闭，再启动。 | 文档秒数，非实测；不是恢复出厂。写入期间后果、最后进度及冻结原因待查。 | E-AK-11 |
| F-M065-12 | 从AK Connect库下载（D2） | 同网并选定库；分类中点歌为流播，编辑图标→选曲→下载箭头为下载。Settings/Downloads可预设目录或每次选目录；完成由通知栏消息表明。点下载通知进入管理，选择暂停或删除项。 | 下载管理删除项的部分文件清理范围未明；断网续传、重名覆盖、下载后离线回查未知。选外部扬声器的连接取消入口在库/扬声器页，不能当下载取消。 | E-AK-02；E-AKCD-01；E-AKCD-03 |
| F-M065-13 | 设置睡眠停止与自动关机（D2文档） | Main Menu/Settings/Timer Settings，分别设置Sleep Timer、Auto Power Off、Screen Timeout。Sleep到时停止播放，关机按Auto Power Off；息屏为另一计时。 | 不概括成“睡眠计时到点立刻关机”。时长范围、确认/取消、操作是否重计、锁屏/重启保留和其他app适用范围未给出。 | E-AKCD-01 |
| F-M065-14 | 电脑音频进入SR35（D2文档） | Settings/USB Mode选DAC Input→USB连接电脑，手册描述其作为声卡。SR35官方驱动页列Windows10/11；Mac无需安装该驱动。音量由设备轮或播放程序控制，手册说Windows系统音量不可调。 | 模式内Previous/Play/Next不可用；未执行驱动。超过手册格式范围不可播；重采样、电脑声卡选择、热插拔、退出与原曲恢复未知。不是SR35向外接DAC输出。 | E-AKCD-01；E-AKCD-07 |
| F-M065-15 | 蓝牙发射、断开与移除配对（D2） | Settings/Bluetooth开启→在可用设备列表选耳机/音箱；点已连设备弹断开确认；设备右侧信息按钮→Remove移除配对。改变Bluetooth Codec后需重连，不支持时回落兼容编码。 | 断开连接与删除配对记录分开。图中CANCEL仅为可见选项，未触发；配对拒绝、断连续播/音量及外设多点切换未知。 | E-AKCD-01；E-AKCD-05；E-AKCD-06 |
| F-M065-16 | 返回播放页、定位时间与歌词（D2文档） | Mini Player上滑/点按展开，下滑/左上返回收起；进度条拖到曲内位置，点封面查看歌词，时长显示可切剩余时间。 | 这些是视图/单曲定位，不构成持久断点。无歌词反馈、精确跳转粒度、章节、切书/重启保存与返回原滚动位置未知。 | E-AKCD-01 |

## 版本、异常与数据边界

| 分支 | 可核查的依据 | 仍缺的观察/恢复 |
| --- | --- | --- |
| 第6个书签、失效文件夹 | 手册限制5个，已观察添加/移除示意 | 没有超额提示或自动替换证据；不虚构拒绝/排序规则。 |
| 下载暂停、删除与离线 | 手册提供通知进入管理及暂停/删除入口 | 暂停续传、删临时文件还是成品、同名文件和脱网能否直接播。 |
| Sleep与Auto Power Off | 印刷55先停止播放，再按自动关机规则 | Auto Power Off所称“用户和设备活动”是否包含每种后台事件；不能推断与iBasso历史计时相同。 |
| Wi-Fi登录页 | SR35关联官方FAQ仍排除需浏览器/额外认证的网络 | V1.22新增Chrome不能证明认证网已变为支持；没有登录测试。[E-AKCD-10] |
| 接收与输出限制 | 型号手册禁USB Audio使用时的Bluetooth；共用FAQ列BT Sink不支持SCMS-T | 未逐个服务/外设验证；USB Audio输出限制不能直接套到USB DAC输入。[E-AKCD-01/08] |
| USB-C外设 | SR35支持页所链FAQ明确警告USB-C耳机/无线路输入功放直连会有满音量、不可调问题 | 不因此把所有USB音频设备都写兼容；具体合适外接DAC仍需确认。[E-AKCD-09] |
| 播放应用与通知 | 通知栏同时承载快捷状态、下载/外设消息；USB DAC内侧键不控制播放 | 普通通知是否打断声音、其他app音频焦点、来电/Sink恢复无明确链，保持未知。 |
| 初始化、格式化与备份 | 印刷57区分数据库重扫、设置重置、全用户数据重置；EQ52有同型号文件共享 | 没有整机/歌单/进度云备份证明；不得把数据库重扫当无损恢复承诺。 |

更细的功能限制只按来源范围记录：ReplayGain共用FAQ说明Line Out时不工作；该项可定位，尚未建立校准失败/恢复任务。[E-AKCD-11] 缺少证据的AB、章节、逐书时间书签、持续后台及服务账号退出清理均未宣称不支持。

## 对本项目的分析与下一项核查

以下为推断：对G-002/R-021，SR35展示了不同对象的状态——文件夹快捷入口、歌单引用、下载队列、落地文件、当前播放页及实际声音出口。AI需报告改动的是哪一项，不能把通知消失当下载完成、把删书签当删文件，或把Sleep Timer当关机承诺。对R-018，单曲拖动与Favorites均不足以保证“继续上次的故事”。

对5–6岁儿童，可研究简单的播放/停止状态与成人维护入口；成人内容管理需要下载失败、路径失效及撤销反馈。Plus可以借鉴这些对象和状态的区分，但FTP、APK、外置DAC或编解码器能力不自动迁移，未新增产品需求。

下一最小核查：固定V1.22，建立5个及第6个文件夹书签，观察移动源目录；用一段长音频从局域网下载、暂停、断网、恢复并脱网播放；随后核对Sleep与Auto Power Off以及USB DAC退出的原曲位置。应分别记录真实文件、引用、队列、时点和声音出口。

## 证据与静态页

全部查于2026-10-26。原PDF存于 `/home/workspace/reader/work/ak-controls-deepening/SR35_instruction_book_ENG.pdf`；元数据只描述文件，未认定设备构建。所有摘页均实际查看，不把PDF内印刷的多张图视为操作录像。

| E 编号 | 来源/定位 | 方式和限制 |
| --- | --- | --- |
| E-AK-01 | [SR35官方手册索引](https://www.astellnkern.com/cn/support/download.php?boardid=Download_cn&category=&idx=60&mode=view&offset=70&sk=&sw=) | 2023-05-02；本次重开并沿原链接取PDF。 |
| E-AK-02 | [厂家手册镜像](https://device.report/m/55ddd354d3c3a8c43fceabf25ac74a7c41d903c65746e8e9228af4e834f2d0cc) | 旧文本；新原件补图标/页码，不作为独立来源。 |
| E-AK-03 | [V1.22全球发布记录旧链接](https://www.astellnkern.com/support/download_detail.jsp?bbsNo=426) | 旧官方索引正文；原网址转首页，未当本次成功直读。 |
| E-AK-04 | [V1.22日本发布记录](https://www.iriver.jp/support/file_1596.php) | 本次重读，日期及轮锁扩侧键；不含现版图。 |
| E-AK-05 | [AK File Drop Windows](https://www.iriver.jp/support/faq_1012.php) | 原SE180示范仍限定；SR35型号部分另由新原手册补。 |
| E-AK-06 | [Open APP Service](https://www.iriver.jp/support/faq_974.php) | 旧SP1000M示范。 |
| E-AK-07 | [已测试app版本表](https://www.iriver.jp/support/faq_1014.php) | 截至2025-05-01，非当前登录保证。 |
| E-AK-08 | [BT Sink共用FAQ](https://www.iriver.jp/support/faq_1011.php) | 本次重读；AK Connect前置与型号手册不同，图片未用于SR35观察。 |
| E-AK-09 | [通知栏设置FAQ](https://www.iriver.jp/support/faq_1103.php) | 本次重读；型号手册53提供独立型号定位。 |
| E-AK-10 | [M3U路径FAQ](https://www.iriver.jp/support/faq_1033.php) | 保留原D1限制。 |
| E-AK-11 | [强制关闭FAQ](https://www.iriver.jp/support/faq_1029.php) | 原文档证据，无实机操作。 |
| E-AK-12 | [SR35日本型号支持页](https://www.iriver.jp/support/product_235.php) | 本次核对手册、V1.22、驱动与共用FAQ关联。 |
| E-AKCD-01 | [官方英文原PDF](https://cdn.astellnkern.com/bbs/download/202305/20230503165340_8b122109-9c25-4ef8-8ecc-69290747288f.pdf) | HTTP200、73文件页；PyMuPDF目录和操作正文。印刷1目录、3–63操作、66故障；未证明V1.22原样。 |
| E-AKCD-02 | 同原PDF印刷48/文件49 | [书签摘页](/home/workspace/reader/docs/research/interaction-library/artifacts/ak-controls-deepening/sr35-printed-48.png)：Add Bookmark及Bookmarked示意。 |
| E-AKCD-03 | 同原PDF印刷30/文件31 | [网络下载摘页](/home/workspace/reader/docs/research/interaction-library/artifacts/ak-controls-deepening/sr35-printed-30.png)。 |
| E-AKCD-04 | 同原PDF印刷24/文件25 | [通知栏摘页](/home/workspace/reader/docs/research/interaction-library/artifacts/ak-controls-deepening/sr35-printed-24.png)：历史布局。 |
| E-AKCD-05 | 同原PDF印刷59/文件60 | [蓝牙连接摘页](/home/workspace/reader/docs/research/interaction-library/artifacts/ak-controls-deepening/sr35-printed-59.png)：断开确认示意，不是本次配对。 |
| E-AKCD-06 | 同原PDF印刷60/文件61 | [配对移除与Sink摘页](/home/workspace/reader/docs/research/interaction-library/artifacts/ak-controls-deepening/sr35-printed-60.png)。 |
| E-AKCD-07 | [SR35 Windows USB DAC驱动说明](https://www.iriver.jp/support/file_1504.php)，2023-05-10 | 只读说明；Windows10/11、Mac驱动条件；未下载或运行驱动。 |
| E-AKCD-08 | [BT Sink与SCMS-T](https://www.iriver.jp/support/faq_1046.php) | 型号支持页关联共用FAQ；无日期/构建，无当前服务验证。 |
| E-AKCD-09 | [USB-C耳机/无线路输入功放限制](https://www.iriver.jp/support/faq_1107.php) | 型号支持页关联共用FAQ；不当外接DAC通用兼容表。 |
| E-AKCD-10 | [Wi-Fi连接及认证条件](https://www.iriver.jp/support/faq_1060.php) | 只取与手册相符的认证网络限制；不据共用频段表重写SR35硬件规格。 |
| E-AKCD-11 | [ReplayGain限制](https://www.iriver.jp/support/faq_1104.php) | 型号支持页关联共用FAQ；Line Out分支；只读不测响度。 |
'''
(lib/'cases/M-065-astell-kern.md').write_text(case)
pdf='https://cdn.astellnkern.com/bbs/download/202305/20230503165340_8b122109-9c25-4ef8-8ecc-69290747288f.pdf'
E=[]
def ev(num,title,url,loc,note,artifact=None,source_type='official_manual',updated=None,access='opened_text'):
 art=None;digest=None
 if artifact:
  shutil.copy2(w/artifact,a/artifact);art='artifacts/ak-controls-deepening/'+artifact;digest=sha(a/artifact)
 E.append(dict(evidence_id=f'E-AKCD-{num:02d}',product_id='M-065',title=title,url=url,checked_on='2026-10-26',source_type=source_type,access_mode=access,version_scope='SR35 V1.22 release baseline; 2023 model manual or linked regional FAQ qualified; actual build unknown',app_build=None,firmware_version=None,source_updated_on=updated,locator=loc,note=note,hands_on=False,visual_observed=bool(artifact),archived_artifact=art,sha256=digest,capture_state='official_pdf_static_page_actually_viewed' if artifact else 'bibliography_and_short_note_only',supersedes=None))
ev(1,'SR35官方英文原手册与完整目录',pdf,'73文件页；印刷1目录、3–63操作、66故障；文件页=印刷页+1','官网2023-05-02索引链至CDN；HTTP200原件17114670字节，SHA256 '+sha(w/'SR35_instruction_book_ENG.pdf')+'。PDF元数据修改2023-03-27非固件号。修正旧案例Open APP/AK Connect/故障页码；未完整重述法律认证部分。',access='official_pdf_downloaded_and_pymupdf_text_read')
for num,page,title,note in [(2,48,'文件夹书签添加/移除','菜单Add Bookmark、Bookmarked图示及左侧书签移除；上限5个，超额未知。'),(3,30,'AK Connect流播与下载','编辑/选曲/下载箭头；通知完成、暂停/删除说明；印刷图非实际下载。'),(4,24,'SR35历史通知栏布局','旧轮锁时期图示；不能视作V1.22 All Lock已观察。'),(5,59,'蓝牙发射与断开确认','可见蓝牙/列表和Disconnect提示、CANCEL/OK；未触发按钮或配对。'),(6,60,'移除配对与Bluetooth Sink Mode','型号截图在Bluetooth页给出Sink开关；共用FAQ先AK Connect的差异保留。')]:
 ev(num,title,pdf,f'印刷{page}；PDF文件{page+1}',note+' 与全文同源，不单独扩充来源预算。',artifact=f'sr35-printed-{page:02d}.png',access='official_pdf_page_render_actually_viewed')
ev(7,'SR35 Windows USB DAC驱动条件','https://www.iriver.jp/support/file_1504.php','正文；添加2023-05-10','明确Windows10/11；Mac不需该驱动；只读页面，未下载或运行ZIP。','',source_type='official_regional_support',updated='2023-05-10')
ev(8,'BT Sink不支持SCMS-T','https://www.iriver.jp/support/faq_1046.php','正文；SR35 product_235关联FAQ','共用官方FAQ，无日期/固件；仅文档限制，未测试当前受保护内容。',source_type='official_regional_help')
ev(9,'USB-C耳机和无线路输入功放的连接限制','https://www.iriver.jp/support/faq_1107.php','正文；SR35支持页关联FAQ','该类连接的满音量/不可调警告；不当作所有便携DAC兼容性判断，也不套到电脑USB DAC输入。',source_type='official_regional_help')
ev(10,'Wi-Fi认证网络限制','https://www.iriver.jp/support/faq_1060.php','连接方法段：需浏览器或其他认证的网络','与SR35手册印刷58互证；共用频段表未用作SR35规格；V1.22新增Chrome不自动撤销此限制。页面图片未实际查看。',source_type='official_regional_help')
ev(11,'ReplayGain与Line Out限制','https://www.iriver.jp/support/faq_1104.php','正文：格式及Line Out限制','SR35支持页关联共用FAQ；只作功能目录分支，不把校准效果或失败恢复记作实测。',source_type='official_regional_help')
(lib/'batches/ak-controls-deepening-evidence.jsonl').write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in E))
fields=['flow_id','product_id','task','depth','evidence_ids','known_branches','gaps','visual_status','hands_on','case_path','checked_on']
def row(n,task,eids,branches,gaps,visual=False,depth='D2'):
 return dict(zip(fields,[f'F-M065-{n:02d}','M-065',task,depth,eids,branches,gaps,'static_reference_only' if visual else 'not_observed','false','cases/M-065-astell-kern.md','2026-10-26']))
new=[row(13,'设置睡眠停止与自动关机','E-AKCD-01','Timer Settings区分Sleep Timer、Auto Power Off、Screen Timeout；Sleep到时停止播放，关机依Auto Power Off。','时长/确认/取消、重计、锁屏/重启、第三方app范围未给出；非到时立即关机承诺。'),row(14,'电脑音频进入SR35','E-AKCD-01;E-AKCD-07','USB Mode/DAC Input；电脑声卡输入；Windows10/11驱动及Mac条件；模式内Previous/Play/Next不可用。','电脑声卡选择、退出/断线原曲恢复和音量；未装驱动/未实测；与USB Audio输出不同。'),row(15,'蓝牙发射、断开与移除配对','E-AKCD-01;E-AKCD-05;E-AKCD-06','列表配对；点已连设备确认断开；信息按钮/Remove清配对；改codec需重连及不支持时回落。','取消仅为按钮可见；配对拒绝、音量、断连续播、外设多点及现版差异。',True),row(16,'返回播放页、定位时间与歌词','E-AKCD-01','Mini Player展开/收起；进度条拖动、封面歌词、剩余时间。','无歌词反馈、定位精度、章节、跨书/重启进度、返回滚动位置；不当持久断点。')]
with (lib/'batches/ak-controls-deepening-coverage.csv').open('w',newline='') as f:
 wr=csv.DictWriter(f,fields);wr.writeheader();wr.writerows(new)
old=[row(1,'USB传入并安全结束','E-AK-02;E-AKCD-01','Media Device复制后断线；型号手册补通知栏Safely Remove及System Information/Unmount。','扫描触发、空库/坏文件、复制取消、同名覆盖、移动后歌单。'),row(2,'放入队列或歌单','E-AK-02;E-AKCD-01','队列Next/Last；新建名称Add、加歌Complete；删除成员确认DELETE且不删存储音频。','当前项移除后的播放、撤销、顺序持久化、整歌单删除后的恢复。'),row(3,'添加与移除常用文件夹书签','E-AK-02;E-AKCD-02','Folder内右侧菜单Add Bookmark，图示Bookmarked及顶部入口；左侧书签图标移除；最多5个。','第6个拒绝/替换、移动目录、跨重启保留；不是音频时间书签。',True),row(4,'调整下拉快捷入口','E-AK-09;E-AKCD-01;E-AKCD-04','SR35手册53独立支持Notification Panel拖动增删排序；静态图24为历史布局。','V1.22具体All Lock图标、满额、默认恢复及当前布局。',True),row(5,'无线双向传文件','E-AK-05;E-AKCD-01','型号手册补FTP地址/凭据及匿名仅下载；Windows网络位置细节仍为SE180示例。','断线、权限、覆盖、退出会话及部分文件清理；不是云同步。'),row(8,'手机音频经SR35播放','E-AK-02;E-AK-08;E-AKCD-06;E-AKCD-08','型号图在Bluetooth页有Sink开关；开启会断开原设备；共用FAQ的AK Connect前置差异保留；SCMS-T限制有官方FAQ。','手机来电、断开恢复、音量保存与当前服务；共用FAQ不作型号截图。',True),row(10,'更新、备份与锁键差异','E-AK-03;E-AK-04;E-AKCD-01;E-AKCD-04','历史手册轮锁与V1.22扩侧键分开；新版专用电量/空间条件优先于旧手册；更新初始化设置。','All Lock精确图/逐键规则、应用/歌单/进度保留、更新中断回退。',True),row(12,'从AK Connect库下载','E-AK-02;E-AKCD-01;E-AKCD-03','旧混合行收束下载；同网选库、流播/下载分流、默认/逐次目录、通知完成及暂停/删除管理；计时/USB DAC移至独立F13/F14。','断网续传、删除临时/成品范围、重名覆盖、下载后离线回查；扬声器连接取消不当下载取消。',True)]
base={x['flow_id']:x for x in csv.DictReader((lib/'coverage.csv').open())}
updates=[dict(flow_id=x['flow_id'],changes={k:{'old':base[x['flow_id']].get(k),'new':v} for k,v in x.items() if base[x['flow_id']].get(k)!=v},replacement_row=x) for x in old]
(w/'old-coverage-updates.json').write_text(json.dumps(updates,ensure_ascii=False,indent=2)+'\n')
(w/'scope-manifest.json').write_text(json.dumps(dict(modified_cases=['cases/M-065-astell-kern.md'],new_batch_files=['batches/ak-controls-deepening-evidence.jsonl','batches/ak-controls-deepening-coverage.csv'],artifacts=[x['archived_artifact'] for x in E if x['archived_artifact']],new_evidence=len(E),new_flows=len(new),old_flow_updates=len(updates)),ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(evidence=len(E),new_flows=len(new),old_flow_updates=len(updates),static_artifacts=sum(x['visual_observed'] for x in E))))
