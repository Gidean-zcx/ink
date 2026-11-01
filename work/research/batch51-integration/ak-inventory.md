# Batch 51：M-065 Astell&Kern 本地材料盘点

日期：2026-10-27。范围是只读核查现有案例、覆盖、目录台账、证据与本地原件；未联网、未操作设备、未安装 APK、未改正式文件。唯一写入是本报告。

## 对象核对

- `docs/research/interaction-library/catalog.csv` 第 66 行登记：`M-065,Astell&Kern 播放器系列`。
- 现有案例标题为 `M-065 Astell&Kern SR35：文件与歌单、音频角色及声音设置`。因此目录对象是品牌播放器系列，当前有正文证据的代表机型是 SR35；不能把 SR35 结果直接扩成全部 Astell&Kern 型号。
- `case-index.md` 当前记 18 个任务（D1 2、D2 16）。案例明确无设备操作、账号登录、APK/固件执行或耗时测量。

## 本地原件与提取物

| 本地文件 | 性质 | 已有使用状态 |
| --- | --- | --- |
| `work/ak-controls-deepening/SR35_instruction_book_ENG.pdf` | 官方英文 SR35 手册原件，17,114,670 字节，73 个 PDF 文件页，SHA256 `2fc0dd11a7b6babfcc11028e2f1223c16320287f47adb18bb72ae10d97155182` | 全文已由 PyMuPDF 提取；仅若干页有“实际看图”记录。PDF 文件页=印刷页+1。 |
| `work/ak-directory-deepening/manual-pages.json` | 上述 PDF 的逐页文本提取，共 73 条 | 是机器提取材料，不等于每页正文已经人工核读或交互闭环。 |
| `work/ak-controls-deepening/material-locators.txt` | 手册文本定位摘录 | 目录/关键词辅助材料，不是独立证据。 |
| `work/ak-directory-deepening/raw/m3u.html`、`raw/m3u.txt` | 官方区域 FAQ 的保存正文与文本提取 | 已复读正文；不是未读项。只说明支持声明和 Android 路径匹配限制，没有导入、修复流程。 |
| `work/ak-directory-deepening/raw/sr35-support.html`、`raw/sr35-support.txt` | SR35 日本支持页及提取 | 支持项目录；链接标题出现不等于链接正文已读取。 |
| `docs/research/interaction-library/batches/ak-directory-deepening-inventory.csv` | 202 条分层目录台账 | 其中大量 `toc_verified_body_selective`、`body_read_partial` 和 `index_link_only` 必须分别理解。 |

## 与本批主题直接相关、正文虽在本地 PDF 中但仍未闭合的段落

下表不是把单批清单外页面宣布成“全球未读”；它仅列已经保存到本地的 SR35 PDF 正文，以及所有已知旧引用核对后仍缺的交互分支。

| 主题 | 精确本地位置与 PDF 页 | 已取得正文 | 尚未闭合 |
| --- | --- | --- | --- |
| 离线曲库传入、删除与卡管理 | `manual-pages.json` / 原 PDF：印刷 8–12，文件页 9–13 | Media Device 连接；拖放复制；电脑端删除确认；传输完断开；SD 安全卸载；OTA 下载中离网/换页边界 | 同名覆盖、复制/移动中断后的半文件、扫描触发与完成反馈、内外存迁移后曲库和歌单引用、卡重新插入后的重建/去重、损坏恢复均未给。目录项 I-AKDD-005/006 只是目录起页加选择性正文。 |
| 本机文件/目录操作 | 印刷 35–37，文件页 36–38 | Folder 中 play/bookmark/copy/move/remove/edit tags 的菜单名称；按分类播放 | 目标目录选择、确认/取消、覆盖冲突、进度、撤销、标签保存、移动后歌单/收藏/当前队列修复均未闭合。I-AKDD-144～169 标为 `body_read_partial`，其证据主要是菜单列举。 |
| 当前队列（Now Playing） | 印刷 21–23、35–37，文件页 22–24、36–38；设置印刷 53、文件页 54 | 当前列表入口；单曲 Next/Last；批量加到顶部/底部/当前曲后；Playback Settings 入口 | 当前曲被移除后的下一首规则、队列重排后的保存、关机/重启/切 app 后是否恢复、源文件移动/拔卡后的失效处理、撤销均未知。I-AKDD-120/150/169 为 `body_read_partial`。 |
| 持久歌单 | 印刷 38–40，文件页 39–41 | 新建命名、选曲 Complete、成员删除且不删音频、整表播放、部分曲加入 Now Playing、歌单重命名及整表删除确认 | 空名/重名、排序是否持久化、整表删除的恢复、源文件移动/删除后的引用、跨更新/重扫/恢复出厂的保留、导出/导入均未知。静态实际看图仅印刷 40；印刷 38–39是文本提取/正文重建，没有对应已归档实际看图页。 |
| 外部 M3U | `raw/m3u.html`、`raw/m3u.txt`；手册内部歌单印刷 38–40（文件 39–41）及重扫印刷 57（文件 58） | FAQ 正文已复读，说明支持及 Android 路径不匹配条件；手册给内部歌单与数据库重扫 | SR35 的 M3U 放置位置、发现/导入入口、编码、绝对/相对路径、内外存路径写法、重扫去重、错误反馈和批量修复全部未知。F-M065-09 正确保留 D1。这里没有“已保存但未读”的 M3U 正文，只有“已读但流程未闭合”。 |
| 本地/局域网下载形成离线曲库 | AK Connect 印刷 28–30，文件页 29–31；Downloads 印刷 56，文件页 57 | 同网选库、流播与下载分流、通知完成、暂停/删除入口、默认目录或每次选目录 | 断网续传、删除下载项是否清理临时/成品文件、重名覆盖、校验、下载完成后的离线回查、移动后索引、服务器消失后的可用性未知。印刷 30 有实际看图；其余为正文提取/重建。 |
| 播放断点/续播 | 播放页印刷 21–23、35，文件页 22–24、36；故障印刷 66–67，文件页 67–68 | 进度条可拖到曲内时间；暂停/恢复；故障页仅处理无法播放/冻结等 | 没有文档证明精确秒点跨重启续播，也没有音频书签、章节、跨设备、跨 app 或刷机后恢复。队列恢复与曲内断点必须分开；现有 F-M065-16 已明确“视图/检索不构成持久断点”。 |
| APK 安装与离线数据 | Open APP 手册印刷 27、文件页 28；设置 Update 印刷 56、文件页 57 | 手册只说可安装特定 APK 音乐流媒体 app、可从 Service 菜单下载，并提示兼容和数据风险；默认 app 更新与 Open APP 自动更新分开 | 安装确认链、权限、失败/回滚、包架构、卸载、重装后 app 数据和离线下载是否保留、账号退出清理、后台音频焦点都未知。I-AKDD-170/171 是 `prior_evidence_body_not_reread`：旧证据曾读网页正文，本批只登记索引；对应 FAQ 正文并未保存在当前 raw 目录，不能称为“本地已保存未读”。 |
| 更新、数据库重扫、设置重置、恢复出厂 | OTA 印刷 12、文件页 13；System Information/Update 印刷 56、文件页 57；System Reset 印刷 57、文件页 58 | 更新最低电量/网络中断提示；microSD 格式化前备份警告；Initialize Media Scan、Settings Reset、Factory Data Reset 三对象区分 | 确认/取消、重扫对歌单/收藏/队列/断点/app 数据影响、更新后保留清单、失败回退、恢复出厂后任何备份恢复路径均未说明。I-AKDD-083～089 都是 `body_read_partial`；现有材料没有整机或 app 数据恢复证明。 |

## 仅目录提取或索引登记，不能当正文依据

- PDF 目录行 I-AKDD-001～036 的状态统一为 `toc_verified_body_selective`。它们证明章节名和起页，不证明整章人工读完；与本批直接相关的是 Firmware Updates（印刷12/文件13）、Open APP（27/28）、Listening to Music（35/36）、Playlist Management（38/39）及 Settings/System Reset（50起/51起、57/58）。
- 支持页 I-AKDD-170、171 是 `prior_evidence_body_not_reread`：旧 E-AK-06/07 有正文短记，但本批没有复读，且当前没有对应 raw HTML/TXT。
- I-AKDD-194 是 SR35 手册下载页 `index_link_only`，但英文官方 PDF 原件已由另一来源保存并核哈希；下载页未读不等于手册正文缺失。
- I-AKDD-196～200 是旧固件版本发布页 `index_link_only`。它们可能含与 Open APP、曲库、播放恢复有关的历史变更，但正文未保存在本地，本次不能据标题推断功能。
- I-AKDD-202（Mac 数据传输软件）为 `index_link_only`，只证明支持页列出入口。

## 已知旧引用核对结果

- E-AK-02/03/04/06/07/10/11/12 与 E-AKCD-01～11、E-AKDD-01～03 均在案例、覆盖或批次证据中找到对应引用。
- M3U 的旧引用 E-AK-10 已有本地 raw 正文并于 2026-10-27 复读；不能再列为“只看目录”。
- Open APP 的 E-AK-06/07、强制重启 E-AK-11、USB DAC 驱动 E-AKCD-07 等属于旧时在线正文短记、本批未复读；本地没有这些网页正文原件。它们与“PDF 已保存但未闭环”是两类缺口。
- 现有案例已经避免把数据库重扫写成无损恢复，也没有声称有歌单/进度/app 数据备份。这一边界与本地手册正文一致。

## 后续本地阅读优先级（供根代理安排，不作产品结论）

1. 先逐页人工复核印刷 35–40（文件 36–41），把队列、内部歌单和文件操作分别建立状态/删除后果表；其中 38–39 尚无已归档实际看图页。
2. 再复核印刷 53、56–57（文件 54、57–58），专门记录 Playback Settings、Downloads、Application Update、Media Scan/Settings Reset/Factory Data Reset 对数据对象的已知与未知。
3. 将 Open APP 印刷 27（文件 28）与旧 E-AK-06/07 分开：手册只给能力和风险，真正安装/卸载/重装数据链仍需重新取得并保存 FAQ 正文或实机证据。
4. M3U 不必重复读现有 FAQ；下一增量应是 SR35 实际路径/扫描/错误反馈证据。

## 执行记录

全部 shell 调用均通过 `python3` 执行，退出码均为 0：

1. Python 读取 `README.md`、`docs/requirements.md`。
2. Python 递归检索 `M-065`、Astell&Kern、M3U、播放列表、APK、断点、队列、恢复等文本；首次输出过大但进程最终 exit 0，无写入。
3. Python 枚举 Astell&Kern/SR35 本地文件与大小。
4. Python 定位案例、source notes、HANDOFF、material locators 及所有 E-AK 证据条目。
5. Python 读取 202 行目录台账，筛选相关关键词和 I-AKDD-170～202 状态。
6. Python 读取 `manual-pages.json` 的 73 页提取，核对上述印刷页/文件页正文。
7. Python 写入本报告。

状态变化：只新建 `work/research/batch51-integration/ak-inventory.md`；正式文档、案例、覆盖、证据、原 PDF/TXT 均未修改。
