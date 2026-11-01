# Yoto 断点、换卡、跨端、计时、关机与播客：本地正文复核

复核日期：2026-10-27。范围仅限已经保存在仓库中的官方帮助正文；本轮没有联网、没有把旧 HTTP 200 当成新请求、没有设备或 App 实测，也没有创建正式 E/F。目标型号仍按 `M-037` 的 Yoto Player 第三代 / Yoto Mini 2024；历史 OS 更新日志只能说明相应历史版本公布过什么，不能证明当前目标设备实际运行该版本或当前行为仍完全一致。

## 结论

本轮深读 8 篇本地正文，发现两组此前没有进入正式任务描述的明确信息：

1. 历史 OS v2.7.24 更新日志明确写有“播客流从上次播放位置恢复”，但附带“播客发布方支持时”的条件。现有 `F-M037-05` 和 `E-YOD-20` 只使用了控制与版本范围，没有把该播客断点条件写入任务；这是**新增信息**，但证据仍是历史发布日志、非当前实测。
2. 两篇此前在目录 CSV 中仍标为 `index_only` 的播客正文，实际已随分类页保存在本地。它们给出了内置播客/电台绑定实体卡，以及自定义 RSS 播客的完整创建、保存、绑定、取卡再插播放步骤。这是**新增正文信息**。其中“取卡再插”只发生在完成 MYO 绑定后，不能证明普通播放的 A→B→A 会回到原秒点。

其余关于设备选择、睡眠计时、暂停/继续、关机、离线条件的正文都与现有 `M-037` 任务基本重复；复核没有找到普通卡“取出再插保存断点”、A→B→A、手机与设备之间迁移秒点、关机后恢复秒点的明确规则。

## 8 篇正文及逐项判断

| # | 本地正文与定位 | 官方 URL | 已有 E/F | 明确规则 | 判断 |
|---|---|---|---|---|---|
| 1 | `work/yoto-controls-deepening/extracted/playing-your-first-yoto-card.txt` 2、8–24、35–47 行 | https://support.yotoplay.com/en_gb/playing-your-first-yoto-card-HygA_FiXze | `E-YTCD-03`；无 F；inventory `I-YTCD-065` 仍为 `index_only` | 首播插卡后在线流播并后台下载；准备多卡时逐张插入几秒，最后取卡，保持开机、充电、在线、空闲；读卡失败可取出重试并换另一张卡作对照。 | **新增正文确认，但未得到断点规则。** “try another card”属于故障隔离，不是 A→B→A 恢复测试；正文未说取卡时保存位置、重插从何处开始。 |
| 2 | `work/yoto-controls-deepening/extracted/how-to-control-your-yoto-player.txt` 54–103、108–125 行 | https://support.yotoplay.com/en_gb/how-to-control-your-yoto-player-SyGKKj7zg | `E-YTCD-02; E-YOD-01`；`F-M037-05; F-M037-14` | Earlier players：短按电源暂停/继续，包括 App 发起的音频；长按停止；约 3 秒开关机；10 秒硬重启，正文称 Wi-Fi details 和 audio 安全；旋钮长按快退/快进并分级加速。 | **旧结论重复。** “audio are safe”语义不足以证明秒点保存，现有 F05 保留“关机/重启精确断点未知”是正确的。长按停止与约 3 秒关机的手势分界仍未定义。 |
| 3 | `work/yoto-controls-deepening/extracted/app-overview.txt` 25–46、49–92 行 | https://support.yotoplay.com/en_gb/using-the-yoto-app-on-ios-and-android-ryJtFi7Me | `E-YTCD-02; E-YOD-02`；`F-M037-06; F-M037-17` | Device Picker 选择控制与播放目标；点内容后在选定的手机或播放器上播放；Now Playing 可暂停/继续，Maxi Player 可跳章、调音量、15 秒前后跳、滑杆定位；Advanced 可设置 Auto Power-Off 的空闲时长。 | **大多为旧结论重复。** 正文只定义“选择目标后开始/控制”，没有说从手机切到播放器会迁移当前秒点，也没有并发冲突、后台/锁屏/来电恢复规则。Auto Power-Off 只说明触发条件，不说明关机前保存播放状态或再次开机的恢复点。 |
| 4 | `work/yoto-controls-deepening/extracted/how-to-use-the-sleep-timer-on-your-yoto.txt` 2–17 行 | https://support.yotoplay.com/en_gb/how-to-use-the-sleep-timer-on-your-yoto-HJ4gYKj7Mg | `E-YTCD-02; E-YOT-02`；`F-M037-02` | Maxi Player → 月亮/星星 → 设时长；到时停止。运行中换卡或改播别的内容不重置计时；再次进图标选 `off` 取消。 | **旧结论重复。** 它明确回答了“计时器跨换卡继续”，但没有说明新卡从何位置播放，也没有关机、重启、跨设备、闹钟叠加结果。 |
| 5 | `work/yoto-controls-deepening/extracted/does-my-yoto-need-to-be-online-to-play-content.txt` 4–22、25–45 行 | https://support.yotoplay.com/en_gb/does-my-yoto-need-to-be-online-to-play-content-rkmbFFj7fg | `E-YTCD-02; E-YOT-01`；`F-M037-01; F-M037-03` | 普通卡下载完可离线；电台与播客持续流播，必须在线；App 向 Player 发起播放时手机和 Player 都要在线；手机联网时可直接播库内内容；多台播放器各自下载，淘汰按各机使用情况计算。 | **旧结论重复，并补强跨机边界。** “每台独立下载/淘汰”不能推出播放秒点在多机间同步；正文没有手机与设备交接播放的规则。 |
| 6 | `work/yoto-controls-deepening/extracted/yoto-player-software-(os)-updates.txt` 149–172 行，重点 156–160 行 | https://support.yotoplay.com/en_gb/yoto-player-software-(os)-updates-ByglYKimze | `E-YTCD-02; E-YOD-20`；`F-M037-05` | 历史 v2.7.24 列出：播客流可从上次播放位置恢复，条件是播客发布方支持；同一版本把“不取卡的暂停/继续”列为 coming soon。v2.8.6 又列暂停/继续相关修复；v2.6.3966 列开机后卡片有时不自动播放的修复。 | **播客断点是新增信息。** 它只覆盖播客流且有发布方条件；没有说明位置保存频率、换卡/关机/跨机/跨端一致性。历史“coming soon”与当前控制文档已支持暂停/继续是时间演进，不是当前文档冲突。 |
| 7 | `work/yoto-controls-deepening/extracted/category-cards-content-make-your-own.json`，标题约 7388 行、hash `BJCo_FsmMx` 约 7418 行；正文段落 P1–P43（本轮用标准库 HTMLParser 从既有 JSON 只读展开） | https://support.yotoplay.com/en_gb/how-to-add-radio-and-podcasts-BJCo_FsmMx | `E-YTCD-01`；无 F；inventory `I-YTCD-018` 为 `index_only` | 内置内容：Library → Radio/Podcasts → 详情 → 三点/控制面板/底部按钮 → Link to a card → 选手机直接绑定或经 Player。自定义流：网页建 Playlist，`Add Stream` 保存 URL；自定义播客：Podcasts → Add Podcast → 填 RSS → Grab podcast details → Save；回 App 的 MYO 详情绑定，Player 出现红录制图标后绿勾，取卡再插开始播放。 | **新增正文和完整操作链。** “取卡再插”是绑定完成后的验证步骤，不证明普通卡重插断点。单集直链每次新集需更新；RSS 路径会拉名称/集列表。无失败、重复、更新传播、播放完自动续集、断点同步说明。 |
| 8 | 同一 JSON，标题约 7430 行、hash `ByJ3uYjQGe` 约 7460 行；正文 P2–P18 | https://support.yotoplay.com/en_gb/how-to-add-podcasts-that-aren't-in-the-yoto-app-ByJ3uYjQGe | `E-YTCD-01`；无 F；inventory `I-YTCD-019` 为 `index_only` | 网页 Podcasts → Add Podcast → 图标 → RSS → Grab podcast details → Save；App MYO 详情 → Link to a Card → 插 MYO → Link → 红图标/绿勾 → 取卡再插。 | **新增正文，但与第 7 篇后半段实质重复。** 两条都来自同一个分类抓取，不能当作两份独立实测或独立实现证据。 |

## 对指定问题的回答

| 问题 | 本地正文能证明什么 | 仍未知 |
|---|---|---|
| 普通内容断点 | 当前控制文档证明短按电源可暂停/继续；历史日志证明“播客流上次位置恢复（发布方支持时）”。 | 普通故事/音乐取卡、换卡、停止、关机、硬重启后的精确秒点；保存触发时机和精度。 |
| 取卡再插 | MYO 绑定完成后，官方要求取卡再插以播放新绑定列表；首卡故障页要求取出再试。 | 播放途中取卡再插是否续播、从章首还是从头；取卡是否立即提交进度。 |
| A→B→A | 只找到故障排查中“换另一张卡测试”和睡眠计时在换卡后继续。 | A 的播放位置是否保留、B 是否覆盖当前会话、A 返回时恢复到何处。 |
| 手机 ↔ 设备 | Device Picker 可选择播放/控制目标；App 发起到 Player 要求两端在线。 | 正在播放时切目标是重新开始、远程接管还是转移秒点；双端同时播放和冲突优先级。 |
| 睡眠计时 | 设定后跨换卡/改内容继续倒计时，到时停止，可手动关掉。 | 关机/重启后是否保留，跨设备归属，闹钟/来电/蓝牙模式叠加。 |
| 关机/自动关机 | 约 3 秒开关机；Auto Power-Off 可配置离电且无按钮/卡操作的空闲时长；10 秒硬重启声明保留 Wi-Fi details 和 audio。 | 关机前进度提交、再开机是否自动续播、`audio are safe` 是否只指已下载音频、自动关机计时档位及播放中是否算空闲。 |
| 播客 | 在线持续流；可从内置目录绑卡，也可用 RSS 建播客并绑 MYO；历史日志声明发布方支持时从上次位置恢复。 | 当前目标版本是否仍执行该规则；不支持断点的 publisher 如何表现；RSS 更新频率、集序、自动续播、跨机/跨端断点。 |

## 冲突、重复和证据边界

- **没有发现直接语义冲突。** “播客必须在线持续流”与“播客流可恢复上次位置”可以同时成立。
- OS v2.7.24 的“不取卡暂停/继续 coming soon”与当前控制页“短按电源暂停/继续”属于历史版本到当前帮助的演进。不能拿旧日志否定当前说明，也不能由当前说明倒推所有旧固件。
- 两篇自定义播客正文高度重复，且同源于同一个分类 HTML 内嵌数据；应记录为一个操作模型的两个页面变体，不能累计成两次独立验证。
- 所有结论均为官方帮助文档复读。`hands_on=false`、`visual_observed=false` 应保持不变。
- 目录记录中的旧 HTTP 200 和原始 SHA 只证明此前取得文件；本轮没有重新请求。

## 建议父任务后续如何吸收（不在本子任务内修改正式资料）

- 若建立正式任务，可把“播客恢复上次位置（publisher 支持时）”单列为历史版本受限规则，关联 `E-YOD-20`，并明确不是普通卡通用断点。
- 可为播客导入/绑卡建立一条 D2：内置目录绑定与自定义 RSS 两分支，证据仍需从目录 E 升级为正文 E，不能继续只挂 `E-YTCD-01` 目录证据。
- 普通卡 A→B→A、手机/设备切换、关机恢复仍需实机或更直接正文；当前资料不足以新增肯定结论。

## 文件完整性

本轮深读文件 SHA-256：

- `playing-your-first-yoto-card.txt`: `239ed6235d0a5a11fda526fa10cba24e159d5d5932f0f6902417754a67056fa2`
- `how-to-control-your-yoto-player.txt`: `c8f96a7f6122356217fee7c319fe16db70bcbdd6c8f1afe71bea53f8a44a88e1`
- `app-overview.txt`: `2c5bb4387d988933af00c6f348eb72f2b33d13bcd3600268a1668d4832538336`
- `how-to-use-the-sleep-timer-on-your-yoto.txt`: `ad7d83d1adf97efff1c74fb7b0434018a5a79965001b442e059021b60295f8e2`
- `does-my-yoto-need-to-be-online-to-play-content.txt`: `f59deb941f025486984a2c80a1e193e90e5f2044b2c3287192c8ba8d03d0005c`
- `yoto-player-software-(os)-updates.txt`: `10e62b1dae63a2cd3cf6c5d47dd4b60b73a0e062947b98bd1963750329a4d8fc`
- `category-cards-content-make-your-own.json`: `38430c6686974a86dc6f984441bdc1d3dd954309a23fb028d53e8b6540c258e5`

参考正式文件读取时 SHA：inventory `8c5287cbd0f4a3096e08b04b9631a9bb6bece15123484904f33005a35336845e`；case `fd1c9e9dc29ef4135987df82c1a446310d5e539c467dd8086cd3350a4e79cff1`。
