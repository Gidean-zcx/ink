# Tonie 普通故事续播与实体控制：本地证据复核候选

复核日期：2026-10-27。范围仅限已有本地原件；未联网、未操作实机、未修改 `docs/` 或既有批次。下列官方帮助正文/目录快照都属于文档证据，不是实机结果。

## 当前案例与目录基线

- 案例：`docs/research/interaction-library/cases/M-038-tonie.md`。
- 覆盖表：`docs/research/interaction-library/batches/tonie-controls-deepening-coverage.csv`。
- 150 项目录：`docs/research/interaction-library/batches/tonie-controls-deepening-inventory.csv`，其中 1 代 63 项、2 代 65 项、Tonieplay 14 项、家庭管理 8 项。目录条目不是 150 项功能已完成。
- 已有普通故事相关任务：F-M038-01（1 代同一 Tonie 取下再放续播，期间不能播放另一 Tonie；旧 2018 手册）、F-M038-03（1 代敲击切曲与倾斜段内跳转）、F-M038-07（2 代联网 app 当前章节、暂停/播放、重启/跳过）、F-M038-10（2 代睡眠计时）、F-M038-14（日出闹钟条件）。
- F-M038-13 与 E-TNCD-09/10/11/12 只覆盖 **Tonieplay 游戏**的检查点、轮次、章节选择和跳过限制，不能作为普通 Tonie/Creative Tonie 故事断点证据。

## 深读的本地正文或正文级快照（6 组）

### 1. 2 代 app 播放控制

- 本地原件：`work/tonie-controls-deepening/tn-pages1.json`；正文抽取：`work/tonie-controls-deepening/extracted/06.txt`。
- 官方 URL：https://support.tonies.com/hc/en-150/articles/37307520777874-Playback-Features-FAQ
- 已有证据/任务：E-TONI-08 / F-M038-07。
- 范围：Toniebox 2；联网；帮助称适用于 Tonies，Tonieplay 仅保留音量控制。地区路径为 `en-150`，固件与 app 版本未知。
- 可确认：app 显示当前 Tonie 和当前章节，可暂停/播放、重启/跳过；暂停后可以继续。
- 仍不能确认：暂停跨自动关机、重启、换公仔、离线、换盒或换账号后是否保留；“Restart”回当前章节开头还是整本开头；“all Tonies”是否明确包括每一种 Creative/Special Creative。
- 对当前缺口：只能加强“联网同一播放会话的暂停/恢复”，不能填补普通故事的持久断点。

### 2. 2 代快速换公仔导致播放错对象，以及恢复动作

- 本地正文级目录快照：`work/tonie-controls-deepening/extracted/24.txt` 第 31–40 行附近；原快照 `work/tonie-controls-deepening/tn-pages5.json`/`tn-pages6.json` 中同一 Troubleshooting 条目。
- 官方栏目 URL：https://support.tonies.com/hc/en-us/sections/29258108517522-Troubleshooting
- 条目标题：`Tonie is playing the audio from a different Tonie`；本地目录没有解析出文章 URL。
- 已有证据/任务：E-TNCD-24 是排错目录证据；当前主要挂在 F-M038-11，尚未形成普通故事换公仔状态任务。
- 范围：Toniebox 2；US/CA 英文目录快照；固件未知。
- 可确认：官方摘要把“播放了另一只 Tonie 的音频”归因于孩子换公仔太快；建议移下当前公仔、等几秒、再放目标公仔。摘要写的是恢复 listening，**没有说明恢复到目标公仔的原时间点**。
- 对当前缺口：这是可以改变台账的新增故障分支，能补“实体对象识别存在换入竞态及显式等待恢复”；不能回答 A→B→A 是否回 A 的旧断点。

### 3. 2 代敲击切曲失效的恢复

- 本地正文级目录快照：`work/tonie-controls-deepening/extracted/24.txt` 第 46–48 行；原快照同上。
- 官方栏目 URL：https://support.tonies.com/hc/en-us/sections/29258108517522-Troubleshooting
- 条目标题：`Tapping the Toniebox 2 to skip forward and back isn't working`；本地目录没有解析出文章 URL。
- 150 项目录：I-M038-115，当前 `index_only_unread`，仅 E-TNCD-02，无 flow。
- 范围：Toniebox 2；US/CA 英文目录摘要；固件未知。
- 可确认：帮助摘要建议让盒子关机再唤醒；无活动约五分钟会自动关机，醒来后敲击应恢复。
- 对当前缺口：可新建/补充 2 代敲击排错分支，且它与“重启后播放位置是否恢复”是两回事。摘要没有描述重启后的音频位置，因此不能据此填断点。

### 4. 2 代普通故事首次下载、快速换公仔和自动关机之间的边界

- 本地正文级目录快照：`work/tonie-controls-deepening/extracted/24.txt` 第 42–48 行；`work/tonie-controls-deepening/tn-pages6.json` 的 Settings/Troubleshooting 摘要。
- 官方栏目 URL：https://support.tonies.com/hc/en-us/sections/29036371272594-Toniebox-2
- 相关目录项：I-M038-083（手动关机）、I-M038-086（自动关机）、I-M038-116（内容未更新）。
- 已有证据/任务：目录 E-TNCD-02/E-TNCD-24；内容更新另有 E-TONI-14/F-M038-09。
- 可确认：普通 Tonie 首次开始前先下载开头章节，Tonieplay 则需完整游戏下载；无故事、歌曲或游戏活动且移除顶部对象后约五分钟自动关机。另一摘要给出 2 代倒置、单耳按四秒的本地手动关机动作，并称 app 当时不能手动关机。
- 仍不能确认：首次仅有开头章节时取下再放的断点；关机前是否刷写位置；手动/自动关机后是否恢复；网络中断与缓存淘汰。
- 对当前缺口：能补“关机触发”和“首章先行”的前置条件，不能证明重启恢复。

### 5. 睡眠计时与日出闹钟冲突

- 本地原件：`work/tonie-controls-deepening/extracted/13.txt`；原快照 `work/tonie-controls-deepening/tn-pages1.json`。
- 官方 URL：https://support.tonies.com/hc/en-gb/articles/29255589102866-Sleep-Light-with-Timer-Sunrise-Alarm-guide
- 已有证据/任务：E-TNCD-13 / F-M038-10、F-M038-14。
- 范围：Toniebox 2；GB 英文；日出闹钟要求接充电器且 3+ 模式；盒子需在线进入 app 设置，具体固件/app 版本未知。
- 已充分入库：5–60 分钟后关机；可重启/停止计时或立即关机；双耳五秒复用上次计时配置；再次双耳五秒结束计时；顶部仍有 Tonie 播放时闹钟不会响；电量耗尽也不会启动闹钟。
- 新判断：本地正文没有说明计时结束后保存故事位置、次晨继续、闹钟停止/贪睡、重复日程或睡眠期间换公仔。无需重复造新 E/F，维持现有缺口更准确。

### 6. 1 代暂停续播与实体动作

- 当前正式依据：E-TONI-01，旧官方 `Toniebox Guide EN 1.0`（2018-04）PDF，第 2 页 Playing and pausing；官方 URL：https://assets.ctfassets.net/l6vo76cztcax/6PkEgkjjGmwpatuF0n3lTH/1e2878aad4e7f405e9b07fc1eab54fdb/Manual_Toniebox_EN.pdf
- 相关正式正文：E-TONI-03 / https://support.tonies.com/hc/en-us/articles/29036493650962-How-do-I-fast-forward-and-rewind-using-the-Toniebox-1；E-TONI-04 / https://support.tonies.com/hc/en-us/articles/29036580563602-How-do-I-skip-back-and-forth-between-tracks-using-the-Toniebox-1
- 已有任务：F-M038-01、F-M038-03。
- 可确认：取下 Tonie 暂停，期间没有播放另一 Tonie 时把同一个放回可从原处续播；大小耳调音量；敲侧面切曲；倾斜并保持可在曲内快进/后退。
- 仍不能确认：当前固件是否保持旧手册语义；A→B→A 后 A 的位置；自动关机、耗尽电量、重启、换盒后的恢复；Creative Tonie 是否逐对象独立持久保存；敲击力度/误识别/暂停时手势。
- 对当前缺口：这是目前唯一明确的普通故事“原处续播”证据，但限定为旧 1 代且期间未播放另一 Tonie；不能外推 2 代。

## 建议根代理入库的最小变化

1. **有新依据，可改变两个交互缺口**：把 2 代“快速换公仔导致错对象→移下→等几秒→放目标公仔”作为 F-M038-11 或普通播放异常分支；把“敲击失效→等自动关机→唤醒再试”作为 2 代敲击排错分支。两者来源均是 E-TNCD-24 的正文级目录摘要，证据等级仍是 D1/D2 边界，不能写成实机验证。
2. **不新增普通故事断点结论**：2 代 app 暂停/播放只证明联网会话控制；快速换公仔摘要没有给时间点；睡眠/关机/重启正文也没有给位置持久性。
3. **不重复 F-M038-13**：Tonieplay 的 last position/checkpoint、quiz 轮次和章节选择保持游戏专属，不映射到 Tonie/Creative Tonie。
4. **章节选择边界**：普通 Tonie 已证明 app 可看当前章节并跳过，但没有证明可打开章节目录任意选章；Tonieplay 章节选择仅特定游戏例外。两类都不应写成统一章节浏览器。

## 冲突与未知汇总

- 代际冲突：1 代旧手册给出受限原处续播；2 代当前帮助只给 app 会话暂停/播放，不能互相替代。
- 控制范围：2 代 Playback FAQ 称功能适用于 Tonies、Tonieplay 仅音量；是否覆盖所有 Creative/Special Creative 未明说。
- 换公仔：2 代目录摘要证明快速换入会出现对象识别竞态，但没有进度语义。
- 章节：普通 Tonie 的“当前章节/跳过”与 Tonieplay 的“多数不可选章节”属于不同内容模型。
- 睡眠/闹钟：已知正在播放会压住日出闹钟；睡眠计时结束后的断点、次日继续、闹钟停止/贪睡/重复未知。
- 重启恢复：所有本地正文都未给普通 Tonie/Creative 在重启、断电、自动关机、换盒后的精确恢复规则。
- 证据限制：本次只读本地官方帮助快照；目录摘要可能被截断，未解析到两个排错条目的文章 URL；没有连续视觉、设备日志或音频时间点。
