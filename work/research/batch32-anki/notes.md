# M-020 Anki：学习结束、统计与过滤牌组退出/恢复补查

查阅日期：2026-10-27。范围仅限 Anki Desktop 官方现行手册的本地原件，属于文档重建（D2），没有安装或启动 Anki、使用账户、执行同步，也没有连续视觉观察。手册页面未给统一修订日期；因此“现行手册”只表示本次取得的公开页面，具体安装构建未知。原件副本在 `raw/`，先前普通公开 GET 的 HTTP 记录摘录在 `http-records.json`，本批文件哈希见 `SHA256SUMS`。

## 目录与旧记录定位

- 案例：`docs/research/interaction-library/cases/M-020-anki.md`。
- 目录：`docs/research/interaction-library/batches/anki-controls-deepening-inventory.csv`。其中 Studying=M020-P05/E-ANK-01/F-M020-01、02；Filtered Decks=M020-P12/E-AND-11/F-M020-12；Stats=M020-P17/E-AND-14/F-M020-17。
- 主 coverage：`docs/research/interaction-library/coverage.csv` 第282–298行附近；本批不修改。
- 旧证据：`docs/research/interaction-library/batches/anki-deepening-evidence.jsonl`，同 URL 已有 E-ANK-01、E-AND-11、E-AND-14。本批建议沿用旧 E，不重复建号。

## 1. 学习中退出到概览或牌组列表，再继续

- 来源 URL：https://docs.ankiweb.net/manual/studying.md
- 章节/行：Decks，原件行20–24；Study Overview，行31–47。
- 入口与操作：在牌组列表点击某牌组后进入其 study/deck overview；点 **Study Now** 开始。学习中按 `S` 返回 overview；窗口顶部点 **Decks** 可随时返回牌组列表。菜单的 Study Deck 或 `S` 还可学习当前所选牌组。
- 反馈/状态：overview 显示当天到期数量，并按 New、Learning、To Review 分组；所点牌组成为 current deck。手册说明 Study Now 会继续出示当天应显示的卡，直至这批卡耗尽。
- 取消/恢复：这是明确的导航退出入口，但手册没有把“返回 overview/Decks”描述成取消学习，也未说明离开时当前未评分卡的题面、计时、滚动位置或自动推进状态如何恢复。再次 Study Now 是否精确回到同一张未评分卡，未知。
- 适用版本：Anki Desktop 现行手册；未给该路径最低版本，安装构建未知。
- 映射建议：沿用 E-ANK-01、F-M020-01（回忆与自评）；若根代理认为“跨页面暂停/恢复”是独立任务，再决定是否新 F。本批不建议并入 F-M020-02（暂埋/停用/删除）。

## 2. 当日可显示队列耗尽与“完成”的边界

- 来源 URL：https://docs.ankiweb.net/manual/studying.md
- 章节/行：Study Overview，原件行31–44；Decks，行12–16。
- 入口与操作：选择当前牌组，在 overview 点 **Study Now**，持续回答，直到“cards to be shown for the day”耗尽。
- 反馈/状态：牌组列表显示当天 New、Learn、Due；overview 显示当天到期数。这里的完成对象是“当天被显示的卡”，且学习范围为当前牌组及其子牌组，不等同整个集合无待复习内容。
- 取消/恢复：手册此段没有给完成页文案、按钮、庆祝反馈，也没有说明达到每日上限后如何区分“真正无到期卡”和“仍有卡但受上限限制”。可从 Custom Study 增加当日上限（见过滤牌组页行37–45），但那是额外操作，不应把普通结束反馈推断成完整清空。
- 适用版本：Anki Desktop 现行手册；构建未知。
- 映射建议：同任务沿用 E-ANK-01、F-M020-01。若补充“增加今日 new/review limit”，同 URL `filtered-decks.md` 已是 E-AND-11，可作为 F-M020-12 的相邻证据，不新建 E。

## 3. 查看统计时切换牌组/全集/历史范围，并正确解释 Today

- 来源 URL：https://docs.ankiweb.net/manual/stats.md
- 章节/行：Selecting Decks / Collection，原件行47–64；Today，行80–109；More，行66–78。
- 入口与操作：进入 statistics window；默认看当前牌组及子牌组。可在顶部文本框键入任意牌组；Anki 2.1.61 起也可用底部 deck selector。勾选 Collection 切换全集；2.1.28+ 可在顶部搜索框加过滤器；顶部可把默认最近12个月改为 all history 或 deck life。
- 反馈/状态：Today 顶部文本统计把“一次回答”计为一次 review，因此同一卡可计多次，learning card 的回答也计入。Again Count 是按下 Again 的回答次数；Learn/Review/Relearn/Filtered 是回答所属类别，其中 Filtered 指在过滤牌组中提前学习的回答。Today 不受底部/历史时间范围选择影响。
- 取消/恢复：这组操作是查看范围切换，手册未描述显式 Apply/Cancel、关闭后是否记住选择或恢复默认。删除笔记后的复习历史仍进全集统计，但因牌组归属不可知，不进具体牌组统计（行71–75）。
- 适用版本：基础路径为现行 Desktop 手册；deck selector 明确为 2.1.61+，搜索过滤器与新图为 2.1.28+；安装构建未知。
- 映射建议：沿用 E-AND-14、F-M020-17（查看一题或整体历史）。这是旧任务的交互与解释深化，不建议新 F。

## 4. Empty 退出过滤牌组，随后 Rebuild 恢复同一筛选任务

- 来源 URL：https://docs.ankiweb.net/manual/filtered-decks.md
- 章节/行：Home Decks，原件行70–85；Creating Manually，行91–105。
- 入口与操作：在过滤牌组的 study overview 点 **Empty**，把全部卡移回各自 home deck，同时保留空的过滤牌组；以后在该牌组 overview 底部点 **Rebuild**，按相同 filter options 重新取卡。手动创建时由 Tools > **Create Filtered Deck**，设搜索条件后点 **Build**。
- 反馈/状态：Build 临时把匹配卡从原牌组移入过滤牌组；Empty 后过滤牌组仍在牌组列表，可再填充。Rebuild 是重新执行筛选，不是恢复 Empty 前卡片集合的快照；实际取回内容可能随卡片状态、时间及排除条件变化。
- 取消/恢复：Empty 提供可恢复工作容器的退出路径，但页面未说明确认弹窗、Undo、空牌组视觉反馈或 Rebuild 失败提示。暂停、暂埋或已在另一过滤牌组的卡不会被拉入（行113–115），因此浏览器搜索数与重建结果可能不同。
- 适用版本：Anki Desktop 现行手册；未给该路径最低版本，构建未知。
- 映射建议：沿用 E-AND-11、F-M020-12（额外练一组）。这是现有 gap“取消/重建反馈”的直接深化，不新建 E/F。

## 5. 删除过滤牌组时归还卡片并移除容器

- 来源 URL：https://docs.ankiweb.net/manual/filtered-decks.md
- 章节/行：Home Decks，原件行70–89。
- 入口与操作：删除 filtered deck。
- 反馈/状态：手册明确其卡片处理与 Empty 相同，即卡片回到 home deck；同时空的过滤牌组从牌组列表移除。删除过滤牌组不会删除卡片。
- 取消/恢复：页面未给删除入口的具体菜单、确认文案、Undo 支持或恢复已删除过滤牌组定义的方法。若需要继续相同筛选任务，Empty+Rebuild 比删除更保留容器；这只是由两条官方行为推得的操作选择，不是手册承诺的撤销机制。
- 适用版本：Anki Desktop 现行手册；构建未知。
- 映射建议：沿用 E-AND-11、F-M020-12。同任务补足“退出后卡片状态”，不新建 F；不要映射到普通牌组删除或 F-M020-02。

## 证据边界与未知

- 只读取 Markdown 文本，未查看页面内截图，因而按钮位置仅采用正文所说的 top/bottom/overview，不补视觉细节。
- 没有实机或固定版本验证；未观察完成页、确认框、异常提示、Undo、跨重启状态、同步冲突或插件影响。
- “学习结束”在手册证据中只稳妥到“当天将显示的卡耗尽”。每日上限、过滤牌组、暂埋/停用会使“结束”不等于全部待办归零。
- 本批没有重复已补的撤销、导入或媒体恢复。
