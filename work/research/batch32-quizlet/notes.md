# Batch 32：Quizlet 相邻未读模式有限核查

查阅日期：2026-10-27。范围固定为 M-021 既有 Studying 目录附近的 4 个任务：Match、Blocks、Blast、Classic Quizlet Live 的加入。没有重查 Write、Spell、Test、AI Practice Test。没有登录、安装、建局、加入、作答或账户操作。

## 基线与选择

- M-021 正文把 Charms、Blocks、Match、Live 留在“仅目录、规则未展开”；68 行 inventory 是 4 个父级加 64 个文章入口，不是 68 个已完成任务。
- frozen inventory 明列 Blocks（order 48）、Classic Live join（59）、Match（60）。Blast 不在该 2026-10-27 frozen 68 行中，但现行官方“Studying on Quizlet”把 Blast 与 Blocks、Match、Classic Live 并列为 Activities，且委托明确优先 Blast，因此作为第 4 个相邻任务登记；这不代表可直接改旧 68 行目录。
- 旧本地原件只含上述目录标题，没有这些文章正文。旧 `fetch-probe.json` 已记录同一官方 Help 域普通 GET 为 HTTP 403；遵守停止条件，本轮没有再次直连、换头或切换域名。正文来自公开搜索对官方 Help 页的文本抽取，HTTP 状态未知，不是源 HTML。

## 1. Match

来源：`https://help.quizlet.com/hc/en-us/articles/360031183611-Playing-Match`；官方 Help；页面标示 iOS/Web/Android；精确 App 构建、帮助修订日和订阅边界未给。

- 入口：登录账号，打开学习集，选 Match，再选 Start game。
- 操作：依次选一个术语/释义及其配对；每局呈现 6 对，大集需多局才覆盖全部材料。
- 反馈：错误配对给计时增加 1 秒；正文以计时竞速与最终 score 为反馈。
- 结束/取消：全部配对完成后结束并显示 score。移动端 Options 有 Restart Match。
- 保留状态：未说明分数/最好成绩是否长期保存、跨端同步；未说明 Restart 是否确认、X/返回/切后台如何处理当前局。
- 建议映射：相同 URL 无旧 E；相同任务无完整旧 F。F-M021-10 只证明 Match 可离线，不能承担 Match 规则。建议 root 若合并则新增 F；旧 E-QZCD-03 仅作目录父证据，新正文应新增 E。

## 2. Blocks

来源：`https://help.quizlet.com/hc/en-us/articles/37203179032717-Playing-a-game-of-Blocks`；官方 Help；页面标示 iOS/Web/Android；精确构建与订阅边界未知。

- 入口：至少 6 个术语/释义；网站需登录后打开集，选 Blocks→Play；App 段从打开集开始。
- 操作：拖放 3 个拼块，填满行/列得分；3 块放完后答对一题，取得下一组 3 块；可刷新当前问题；Options 可只学星标词及选问题格式。
- 反馈：完整行/列加分，答对后发新块；正文未说明答错或刷新题目的惩罚/解释。
- 结束/取消：无可走步时自然结束；页面提示挑战 top score。未给主动退出/重开路径。
- 保留状态：top score 是否保存、同步、清零均未说明；未说明中途退出是否保存棋盘。
- 平台/资格：特定地区的年轻用户需家长邮件确认同意后才能玩；没有给付费套餐条件。
- 建议映射：相同 URL/任务均无旧 E/F；E-QZCD-03 只证明目录存在。建议新增 F/E；不要把 F13 星标复习的结束/统计规则套入 Blocks。

## 3. Blast

来源：`https://help.quizlet.com/hc/en-us/articles/27393733229965-Playing-a-game-of-Blast`；官方 Help；页面标示 iOS/Web/Android；精确构建和付费套餐边界未知。

- 入口：主持账号注册时识别为 teacher；英语学习集至少 6 对；加拿大、英国不可用，diagram set 不可用。只能从网站主持，玩家可用移动设备加入/游玩。
- 操作：网站登录→集→Blast→Get Started→选提示/答案组合→投影主持屏→玩家用 join code/name 或 App QR 加入→Create game→Start game；Options 调 asteroid speed、choice 数、game duration。
- 反馈：移动小行星显示词/释义；先击中正确目标者得分；玩家会被分组。
- 结束/取消：时间结束，最高分获胜。未给主持人提前结束、取消或重开步骤。
- 保留状态：断线重连、迟到加入、平局、成绩/历史保存均未说明。
- 建议映射：frozen 68 行无此入口；旧 F/E 也没有同 URL/同任务。若 root 接纳为现行相邻模式，应新增 F/E，并另决定是否更新目录快照；不要映射 Classic Live 的资格或退出规则。

## 4. Classic Quizlet Live：玩家加入

来源：`https://help.quizlet.com/hc/en-au/articles/360031249731-Helping-players-join-a-game-of-Classic-Quizlet-Live`；官方 Help en-AU；页面标示 iOS/Web/Android；精确构建和订阅边界未知。

- 入口：主持人分享 join code。浏览器/电脑进入 Quizlet Live 页面；App 路径为登录→Search→Play Quizlet Live→Join a game。
- 操作：输入 code（App 也可扫 QR）→输入名字→Next；主持人在全部人员加入后开始。
- 反馈：正文只说明进入等待/开始关系，没有还原实际作答或排名反馈。
- 结束/取消：本页没有玩家离开、主持取消或游戏自然结束流程。
- 保留状态：无效/过期 code、重连、重新加入、姓名冲突、局后成绩保存均未观察。
- 平台/资格：正文说注册时识别为 teacher 的账号可启动 Classic Live；本页的玩家浏览器路径没有明确要求登录，不能据此断言游客一定可加入。Teams 建议同队坐一起，Individuals 可远程。
- 建议映射：相同 URL无旧 E；inventory 的 order 59 与旧 E-QZCD-03 是目录证据，没有完整旧 F。建议新增“加入 Live”F/E；主持建局、实际答题与结束应保持为未观察分支。

## 横向边界

- Match 的“配完结束”、Blocks 的“无步结束”、Blast 的“计时结束”、Live 的“主持开始”属于四个不同状态机，不能互相补齐退出、恢复或成绩保留。
- Match/Blocks 页面标示三平台，但帮助正文中的控件分段不等于已在具体 App 构建验证。Blast 明确网站主持、移动可玩；Classic Live 本页只核加入路径。
- 没有页面给出完整订阅价格/Plus 门槛。Blast 给 teacher 身份、英语集、地区与 diagram 限制；Blocks 给年轻用户家长确认；这些资格不能外推到 Match 或 Live 玩家。
- 没有观察：任何 UI 图像、错误分支、主动取消确认、后台/断网恢复、跨端同步、历史成绩、排行榜重置、无障碍操作、音效控制。

## 原件与完整性

- `raw/match.txt`、`raw/blocks.txt`、`raw/blast.txt`、`raw/classic-live-join.txt`：按公开搜索返回的官方帮助正文摘录整理的工作快照，不是 HTML。
- `http-sha.json`：上述工作快照 SHA-256，以及既有普通 GET 403 记录的 SHA。没有新增 HTTP 请求状态可报告。
