from pathlib import Path
p=Path('docs/research/interaction-library/cases/M-063-shanling.md');s=p.read_text()
s=s.replace('查阅日期2026-10-26。固定','初查2026-10-26，本批补证2026-10-27。固定',1)
s=s.replace('账户、地区服务可用性及 Eddict app 构建未知。','账户、地区服务可用性及实际安装的 Eddict app 构建未知。本批另取得官方 Eddict 使用说明书V1.2（16页），是伴随App共用手册，不是M1 Plus高级手册；下载页当前Android2.3.7/2026-09-11与手册修订号分开。实际查看其中p12/15静态页，示例设备不是M1 Plus。[E-SLCD-01～03]')
s=s.replace('官方快速指南完整目录只覆盖安全/规格、部件、控制与接口、升级、常见问题、附件/保修及其译文；','官方快速指南章节覆盖安全/环保、部件、控制与接口、升级、常见问题、附件（含保修卡）及其译文；')
s=s.replace('英文部件11、控制12、连接13、升级14、故障15。','英文部件11、控制12、连接13–14、升级14–15、故障15；日文另在23页列技术证明入口。')
needle='表中 E 简写均保留各自前缀：SHAN 为旧证据，SHCD 为本批。目录出现不提高任务覆盖等级。'
repl='''表中E简写保留各自前缀：SHAN为初批，SHCD为上轮，SLCD为本次。目录出现不提高任务覆盖等级。

本批[逐条台账](/home/workspace/reader/docs/research/interaction-library/batches/shanling-controls-deepening-inventory.csv)有**114行**：M1快速指南22个章节/说明域、11个部件标签、5个FAQ；作者自报V2.1的评测目录39项；共用Eddict手册21个章节域、12个App本地设置、下载页4个范围项。译文合并定位，父级/标签单列便于检索；114不是功能完成数。官方M1指南、商业评测、伴随App目录分别标注，不合成一份“当前M1全菜单”。本批没有新增F，只补F01/08/10。

| 本轮优先问题 | 补到的依据 | 仍不能下的结论 |
| --- | --- | --- |
| 断点与计时 | M1评测列Memory playback的Off/Track/Position，以及Sleep Timer/Auto-standby/Auto-Shutdown；Eddict p9/15有同类App设置。 | **Eddict p15明确播放设置只作用于App本地播放，对连接的山灵设备无影响。**不能在手机设定后声称已设置M1计时/断点；设备保存时点、断电粒度、计时重置和结束动作仍未知。 |
| 歌单/文件管理 | 原评测补到指定文件夹扫描、歌单导出能力和创建顺序的描述。 | 仍无官方M1队列编辑/删除确认链；App可加队列不等于M1具备同样队列。导出再导入不是本项目验证过的无损排序/恢复办法。 |
| SyncLink | 官方共用App手册补控制端、设备搜索、连接成功反馈、iOS只做控制端；英文提必要时Music Scan。 | 这些是App侧补证，M1设备入口仍靠既有型号评测；重新扫描作用对象、权限拒绝、配对失败/重试、退出/后台后恢复未知。 |
| 网络服务与传歌 | 共用手册把Wi-Fi传歌、SyncLink、DLNA及投屏分开；英文提供App接收页的电脑浏览器步骤。 | 不能把手机接收地址/目录套成M1 Wi-Fi传歌流程；不能将streamer投屏示例、手机NAS/云下载迁移为M1原生能力。 |

上述M1菜单名称来自E-SHCD-03商业评测；App范围来自E-SLCD-02 p2、5–6、9、12–15。只是菜单或声明的条目保持D1/目录，不为数量新建流程。'''
assert needle in s;s=s.replace(needle,repl)
s=s.replace('文件与索引分离；空卡、重名、取消和完成提示待查。 | E-SHAN-01；E-SHCD-01 |','文件与索引分离；评测另描述可选指定文件夹扫描，不能因此补出过滤/去重。空卡、重名、取消和完成提示待查。 | E-SHAN-01；E-SHCD-01；E-SHCD-03 |')
s=s.replace('控制设备不等于复制/备份内容。AirPlay 只定位 Network Services/Wi-Fi、AirPlay 开关；完整发起/退出、DLNA、Tidal账户和离线未查明。 | E-SHAN-02；E-SHAN-04；E-SHCD-04 |','控制设备不等于复制/备份内容。共用App手册补控制端搜索与连接反馈，且播放设置不改变M1设置；具体条件/冲突见下节。AirPlay、DLNA、Tidal账户和离线仍未闭合。 | E-SHAN-02；E-SHAN-04；E-SHCD-04；E-SLCD-01～03 |')
s=s.replace('建立对现有文件的引用，不等于导入音频。M3U8/相对路径、重复导入、失效项、换卡恢复及版本变化未知；未执行示例脚本。','建立对现有文件的引用，不等于导入音频。作者另说可导出，歌单依创建次序、后添在末尾；不能当当前固件排序保证。导出步骤/文件名、M3U8/相对路径、重导冲突、失效项和换卡仍未知；未执行示例脚本。')
insert='''## SyncLink补证：App与设备分层

官方Eddict手册V1.2中文p6：手机入口为设备控制→SyncLink，允许蓝牙提示，打开控制端后自动搜索，选设备连接，文档描述显示连接成功，然后控制设备本地音乐；iOS只能做控制端。英文p12另说设备先启用SyncLink，并可能需Music Scan才能正确显示曲库。**这是共用操作说明，本批没有在M1 Plus上确认成功反馈、菜单版本或扫描对象。**原F08的M1入口仍来自Kajetblog型号评测，不能升级成已实测或型号专用官方全链。[E-SLCD-02；E-SHCD-04]

三项范围差异保留：

- 手册p2明确非单一型号；官网App下载页虽更新到Android2.3.7，其SyncLink名单仍未列M1 Plus。旧M1产品介绍又明确支持Eddict/SyncLink，故缺项是资料覆盖差异，不能据此说不支持。没有把2.3.7当作手册截图构建，也未推定iOS版本。[E-SLCD-01/02；E-SHAN-04]
- 英文p12要求双方蓝牙和Wi-Fi打开，中文p6先讲蓝牙，再称Wi-Fi模式同理；旧M1评测报告两无线不能同时打开。本文不把共用英文前提直接作为M1必做步骤，也不据评测报告解释硬件原因。[E-SLCD-02；E-SHCD-03]
- 中文p5“电脑传歌”段与手机段内容重复；英文p12给出同网后电脑浏览器打开App所示地址的步骤。它没有给M1设备自己的网页入口/保存目录。DLNA选择媒体渲染设备与SyncLink控制设备本地音乐不同；p13无线投屏明确以streamer示例说明，不能补成M1镜像操作。[E-SLCD-02]

本次实际查看[App手册p12](/home/workspace/reader/docs/research/interaction-library/artifacts/shanling-controls-deepening/eddict-v12-p12.png)和[p15](/home/workspace/reader/docs/research/interaction-library/artifacts/shanling-controls-deepening/eddict-v12-p15.png)：前者展示Controller开关及已连接/已配对/可用分组，示例连接SCD3.3；后者同时显示App计时/Resume等设置和“对连接设备无影响”的正文，下方USB授权示例是UA7。**都不是M1 Plus界面观察**，不能用来证明该机配对、权限或DAC连接成功。完整范围摘记见[本批短摘记](/home/workspace/reader/docs/research/interaction-library/artifacts/shanling-controls-deepening/source-notes.md)。

'''
s=s.replace('## 修正、分支与剩余未知\n',insert+'## 修正、分支与剩余未知\n')
s=s.replace('全部查于2026-10-26。旧证据保持可追溯；新记录不修改旧批次。重复托管的同一指南不是独立来源。','旧证据查于2026-10-26；本批2026-10-27本地复读M1指南、HTTP200复抓中国V1.8页及HiFi Oasis正文，建议追加到旧E。英文/download/118和两篇旧Eddict/MTouch帮助普通GET返回403后停止，未用别的域名/接口绕过。新取得Eddict官网页和手册均HTTP200；没有下载App/固件。重复托管及派生静态页不是独立来源。')
s+='''
| E-SLCD-01 | [Eddict官方下载页](https://shanling.com/download/76)，Android2.3.7，页面发布日期2026-09-11 | 官方App版本/功能及机型名单；名单未列M1不推出不支持；只有正文读取。 |
| E-SLCD-02 | [Eddict官方使用说明书](https://shanling.com/uploads/image/EddictPlayerAPP.pdf)，16页、末页文档V1.2 | 官网说明书链接；日期/对应App构建未标，全文文本已读；p2/15限定范围。 |
| E-SLCD-03 | 同一Eddict PDF p12/15静态摘页，derived_from E-SLCD-02 | 两张实际view_image；其他机型示例，不重复计算独立来源，无连续操作。 |
'''
# Keep the new evidence rows contiguous with the existing table.
s=s.replace('\n\n| E-SLCD-01','\n| E-SLCD-01')
p.write_text(s)
