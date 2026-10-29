# M-046 Ground News：围绕同一事件比较报道

## 范围与证据强度

基线查阅2026-10-26，本轮复核2026-10-27；研究 Web、iOS、Android 官方帮助，构建版本未知。主要帮助页标 2026-03-09 更新，AI 比较摘要页标 2026-08-29。**D2：多段操作文档可拼出任务路线，未登录、付费、点击产品状态、发送反馈或连续观察。**不是儿童专用新闻，不把外部出版商页面视作 Ground 自有阅读器。

原批覆盖事件比较、保存回访及部分个性化；本次深化补入主要功能域目录、全局控制、搜索、外部媒体、账户条件和 My News Bias 公开示例。仍未遍历全部套餐/版本按钮，没有操作推荐系统或个人账户，不能称全功能实测。

## 功能目录与内容机制

| 功能 | 确认范围 | 边界 |
| --- | --- | --- |
| 事件聚合与来源比较 | 事件内有完整报道列表/轮播区，能排序与筛选（E-GRND-01） | 聚合错配怎样纠正、每篇原文如何内外跳转未实测 |
| 偏向/事实性/所有权 | 偏向取 AllSides、Ad Fontes、MBFC 可用评级的平均；事实性基于后两者；都评**出版物整体**。所有权另行分类（E-GRND-03） | 不代表某篇正确、事实中立或某一观点更真；没有评级不能画成中立 |
| Blindspot | 根据报道分布规则识别两侧覆盖不对称（E-GRND-03） | “一方较少报道”不能直接推定其有意隐瞒或该事更重要 |
| AI 比较摘要 | 事件标题下的比较页签；需不同政治倾向有足够独立报道，覆盖不足时可能只有普通摘要（E-GRND-04） | 官方称有人参与监督，不等于逐篇人工核验；准确率无本次证据 |
| 个性化/状态 | 关注主题、地点或媒体；收藏；移动端 Premium+ 可隐藏已看过报道（E-GRND-02、05、06） | “看过”的判定阈值、是否读懂和推荐权重未知 |
| 阅读辅助 | 本次深化确认移动端翻译、外部媒体入口和明确不支持离线；原生文章朗读、词汇解释、儿童分级仍无证据 | 见下文；外部播客不是 Ground 文章朗读 |

## 一次事件任务及回访

**F-M046-01 的文档重建路线：**从事件进入→查看比较摘要与来源分布→筛选一组报道→读相关来源→清除条件对照另一组→保存待继续。它是研究者安排的任务顺序，官方没有规定必须如此使用或设定结业判定。

| F 编号 | 前置状态/入口 | 用户动作 | 反馈与数据变化 | 结束/返回及缺口 | 证据 |
| --- | --- | --- | --- | --- | --- |
| F-M046-01 | 已进入一事件，具有所需权限 | 在标题下选比较摘要；完整报道区右上打开筛选，选偏向及事实性条件后显示结果 | 来源集合只保留匹配项；可按最新、名称等排序。没有足够覆盖时比较摘要可能不存在 | 清除条件或进入新事件会复位**该事件筛选**；外部原文返回后的滚动位置未知 | E-GRND-01、03、04、07；D2 |
| F-M046-02 | 打开想保留的事件 | 移动端右上三点→保存；以后个人页→已保存报道。Web 用书签，之后从 For You 的保存页或账号内保存列表进入 | 保存为后续回访入口；未证明是全文副本 | 后续官方教程明确 Web/App 均不支持离线浏览；取消收藏、同步冲突、断网恢复、原文删除与滚动位置未知 | E-GRND-02、E-GND-14；D2 |
| F-M046-03 | 需要调整信息流 | Discover 搜主题/地点/媒体→关注；移动端个人页→设置→开启隐藏已读 | 相关事件进入 My Feed；Premium+ 隐藏已读会移除已看过条目 | 未证明关注等于只看该主题；已读误判如何撤回、切账号是否继承未知 | E-GRND-05、06；D2 |
| F-M046-04 | 来源缺失/摘要错误/页面异常 | Web 事件右上旗帜→类别与说明→提交；Android 三点→问题入口；iOS 三点→报告问题后交给邮件应用 | 官方说明团队审阅并可能联系反馈者；不同平台提交流程不同 | 不承诺立即修复；本次未提交，成功页/邮件送达未知 | E-GRND-08；D2 |

F-M046-01 的自然结束是用户已经能说出“哪些来源报道、差异何在、还缺什么”。这是评价任务的建议，不是现有软件的完成状态。F-M046-02 补出退出后的回访入口，仍未证实跨重启精确续读。

## 分支与异常

| 条件 | 已知表现/恢复 | 未确定 |
| --- | --- | --- |
| 来源不足 | 比较摘要可能缺席，更多报道出现后可能可用（E-GRND-04） | 加载占位、何时重新生成、旧摘要版本 |
| 来源付费墙 | 有付费/$ 标签；Ground 订阅不能解锁外部媒体；有权限的用户可过滤付费来源（E-GRND-07） | 已拥有该媒体订阅怎样验证、外链登录失败 |
| 筛选太窄 | 可清除筛选或换事件复位（E-GRND-01） | 零结果文案和是否给放宽建议 |
| 断网/收藏回访 | 移动端收藏需要网络（E-GRND-02） | 重试按钮、离线占位和缓存失效机制 |
| 错误、缺来源、坏链接 | 有 F-M046-04 报告路径（E-GRND-08） | 不把帮助中心自身的“提交失败”模板当成应用内报错 |
| 中断/误触/账号切换 | 没有明确恢复规则 | 原文返回、播放恢复（若有）、收藏删除、同步及状态撤销均待查 |

## 编辑、难度、家长与本项目分析

选择机制同时包含聚合、来源元数据、显式关注和 AI 摘要；不能描述成全部人工精选或完全自动事实核查。现有证据未证明有答题、难度适配、积分、每日终点或家长管理。理解政治坐标、媒体来源、报道差异本身需要背景知识；对 5–6 岁孩子独立使用为**信息不足且适配较弱**，可以由成人选一个简单事件讨论，但这属于使用建议。

对 R-020/G-001 有依据可借鉴的是“先把同一事件收拢，再看差异”，有机会减少重复新闻；“能减少实际阅读耗时或扩大观点理解”仍需验证。来源标签不能替代阅读证据，隐藏已读也不能当成已经掌握。

对 G-002/R-007/R-021 的**推断**：AI 上下文至少需含事件与原文的对应关系、评级适用层级、评级缺失、当前筛选、摘要生成时间/来源集合、外部权限、收藏和报告状态。若摘要缺席，AI 应保留“覆盖不足”状态；若换事件导致筛选复位，不能继续沿用旧条件。未证实 Ground 存在可供本设备调用的完整动作/API。

## 证据与获取记录

以下是2026-10-26基线：打开官方正文，当时未看截图或操作产品。2026-10-27新增三张官方静态图单列于后文，仍无产品实际操作。

- **E-GRND-01**：[Sort and Filter News Sources](https://help.ground.news/en/articles/6049857)，入口、选项、Resetting Filters、Example。
- **E-GRND-02**：[Save Articles for Later](https://help.ground.news/en/articles/2044545)，移动/Web保存与回访、联网注释。
- **E-GRND-03**：[Methodology](https://ground.news/rating-system)，Bias Ratings、Factuality Score、Ownership、Blindspot；页面未标更新日。
- **E-GRND-04**：[Bias Comparison Summary](https://help.ground.news/en/articles/3189505)，入口、AI 与准确性、摘要不出现条件；2026-08-29。
- **E-GRND-05**：[Follow Your Favorite Topics & Interests](https://help.ground.news/en/articles/950017)，Web/Mobile。
- **E-GRND-06**：[Hide Read Mobile Stories](https://help.ground.news/en/articles/3523649)，Premium+、设置路径。
- **E-GRND-07**：[Hide Paywalled Articles](https://help.ground.news/en/articles/245569)，外部权益与标签。
- **E-GRND-08**：[How to Report an Issue](https://help.ground.news/en/articles/9683777)，Web/Android/iOS 和提交后流程。

## 后续最小核查

同一具体版本上录一个覆盖充分与一个不足的事件：筛选→零结果→清除→外部原文→返回→保存→重启回访。另查取消关注、取消收藏、隐藏已读撤销和套餐锁定；提交问题只需观察表单，未经授权不发送。完成前保持 D2 文档研究，不升级实测。

## 深化：控制选题、来源与阅读量（2026-10-26）

以下均为公开官方帮助重建；**无登录、无改变偏好、无播放、无连续观察**。帮助页多标 2026-03-09，个别另列；实际 App 构建和地域未固定。公开 My News Bias 页面明确是 Demo Data，仅用于字段核查，不是本研究者的阅读记录。

### 主要功能域目录

| 功能域 | 任务与范围 | 依据/未闭合处 |
| --- | --- | --- |
| 新闻入口 | 非个性化首页；地区 Top Stories、本地 Local、重大事件 In Focus；个性化关注流与自定义专题另行设置 | E-GND-01～03、07；本地不足时会补较大地区新闻，不能把“本地”理解为严格地域边界 |
| 主动检索 | 标题/关键词/来源；Web 可粘贴文章 URL 找聚合事件，事件内还能搜媒体 | E-GND-01；URL 不保证匹配成功；历史深度和高级日期语法未知 |
| 关注与主题排除 | 取消关注、Show Me Less、Manage Sources & Topics、屏蔽来源 | E-GND-04～06；取消关注、减少主题、屏蔽媒体不是同一动作 |
| 专题组织 | Web Custom Feeds 的任一包含、全部包含、排除；Premium/Vantage，帮助称 Web 专属 | E-GND-07；实际移动版本是否后来上线未实测 |
| 事件/来源比较 | 原 F-M046-01 的摘要、偏向、事实性、所有权、Blindspot 与事件内筛选；新增 Web 全局筛选和移动付费墙过滤 | 原 E-GRND-01/03/04/07、E-GND-08/09；标签仍评媒体整体，偏向不等于真伪 |
| 文章阅读辅助 | 内部浏览器便于切来源；可切官方称 External Browser/Reader View 的入口；移动端单篇/自动翻译；网页主题、移动 Night Mode | E-GND-11/12/18；不保证去除外站所有广告，网页翻译属浏览器能力 |
| 听看其他媒体 | 事件下 Podcasts and Opinions 有节目提及、摘要及可用的标签/时间戳，跳原平台观看或收听 | E-GND-13；不是所有文章 TTS，也不证明 Ground 统一保存播放位置 |
| 收藏、已读、恢复 | 原 F-M046-02/03；收藏回访与隐藏已读；Web/App 都不支持离线浏览 | 原 E-GRND-02/06、E-GND-14；没有精确续读、取消收藏/已读撤销的操作证据 |
| 通知与邮件 | Android/iPhone 不同通知入口，可开关每日新闻提醒；订阅含部分 newsletters | E-GND-10/15；定时静默的应用内控件、邮件逐类退订和频次未核定 |
| 阅读习惯分析 | My News Bias：来源、主题、地区、偏向、事实性和所有权等分布 | E-GND-15/16；帮助称 Web 专属 Vantage；示例不能证明真实统计阈值和更新时延 |
| 外部发现 | 浏览器扩展连接外站文章与 Full Coverage；支持 Chrome/Firefox/Edge/Opera/Brave | E-GND-19、01；未安装，不保证每个外站匹配或所有字段免费 |
| 账户/权限/反馈 | 同一账号跨端、购买渠道对应的恢复权益、原问题报告流程 | E-GND-14/15/17；升级可保留账号数据，不等于任何登出/删号都可恢复 |

这张目录覆盖官方帮助中的主要用户功能域；尚有套餐细目、邮件配置、账户数据删除和全部异常状态未核定。“官方说控制观点范围”也不是已经测得用户理解更全面。

### 新增任务链与分支

| 流程 | 入口→动作→可确认反馈/结束 | 恢复、权限与缺口 | 证据/深度 |
| --- | --- | --- | --- |
| F-M046-05 选地区与本地入口 | Web 登录→右上 Edition 选地区；移动首页→Edit News→启用 Top Stories 地区→出现可重排的主题按钮。Local 可搜索地区或使用设备定位 | Local 覆盖不足可能补省/州或全国报道；可重新编辑位置。另有手动搜索入口，拒绝定位权限后的页面未知 | E-GND-02/03；D2 |
| F-M046-06 主动找到一个事件 | Web 顶部搜标题/关键词或粘贴 URL；App→Discover→搜索→点结果→事件覆盖→点来源读其报道。App 的 Edit News→In Focus 可启用持续事件 | 搜索不保证穷尽；事件内 Web Full Coverage 上方另有来源搜索。之后可接原收藏流程；外站返回位置未知 | E-GND-01、原 E-GRND-02；D2 |
| F-M046-07 取消主题关注 | 手机 Ground News→Edit News→Favourite Topics→Manage→主题三点→Unfollow；iPad 为 For You→Following；Web 登录→For You→Discover→去掉主题勾选 | 对象从关注列表移除，不代表全站禁止出现；Web trending bar 当前不能由此自定义 | E-GND-05；D2 |
| F-M046-08 减少主题或屏蔽媒体并回改 | App 事件三点→Show Me Less→选择主题；Web 标题旁阻挡图标进入。媒体则进事件 Full Coverage→来源三点→Remove Source→Update | 未选择时关闭/取消不保存。App Profile→My Activity→Manage Sources & Topics 可管理回改；移动屏蔽会反映到登录后的 Web | E-GND-04/06；D2；恢复的逐个按钮/确认反馈未写明 |
| F-M046-09 建立有边界的专题流 | Web Premium/Vantage 登录→For You→Custom Feeds→Create New Feed→命名→设任一包含/全部包含/排除→Confirm→刷新查看 | 排除栏会剔除含任一被排除主题的事件；不是逻辑“全部同时出现才排除”。编辑/删除/零结果引导未知 | E-GND-07；D2 |
| F-M046-10 把筛选设为默认 | Web My Account→Preferences→设来源排序、偏向、事实性、地区、所有权/付费墙，自动用于故事页和流；移动 Profile→Settings→Hide Sources with Paywalls | 移动过滤 Premium/Vantage 的 Full Coverage，关闭同一开关恢复。全局默认与原事件内临时筛选不同；二者冲突优先级未知 | E-GND-08/09；D2 |
| F-M046-11 阅读外语报道并改变阅读视图 | App 打开原文→右上翻译图标→选语言；Profile→Settings→Preferences→Automatic Translation 可自动翻译。原文右上书本图标切 External Browser/Reader View | 翻译面向订阅者；Web 说明使用浏览器翻译，不能当同一原生功能。来源语言排除过滤尚不支持；返回和翻译位置保存未知 | E-GND-11/12；D2 |
| F-M046-12 听看事件相关节目 | 事件页下滑→Podcasts and Opinions→选可用节目提及→原平台；有适用链接时可跳对应时间戳 | 标签取决于可用数据和套餐；结束/返回、广告、外部权限与续播由原平台及跳转状态共同决定，未测 | E-GND-13；D2 |
| F-M046-13 控制每日打扰 | Android 导航 Alerts→右上设置；iPhone Profile→设置→Preferences→Manage Notifications→开/关 Daily News Update | 帮助提到静默建议和 Android 声音，但未给应用内定时表；不能宣称已支持精确静默时段或绕过系统静音 | E-GND-10；D2 |
| F-M046-15 检视自己的信息范围 | Vantage 提供 My News Bias；公开 demo 展示时间窗口、Stories/Articles、来源/主题与偏向等分布 | D1：本轮只读示例页文字，没有真实账号入口→统计更新→点击下钻过程；不把示例数字记为真实行为或学习效果 | E-GND-15/16；D1 |
| F-M046-16 权益未识别时辨别账号/渠道 | Web 购买者核对原登录方式；App Profile→设置底部可登出换回原账号。商店购买者帮助列 Profile→Manage Account→Restore In-App Purchase | 官方警告 Restore 会重新激活已取消订阅，不能当无副作用刷新；本轮未执行。网站订阅用错入口可能一直加载；实际恢复结果未知 | E-GND-17；D2 |

收藏与离线范围的新证据已补入原 F-M046-02，没有把同一回访任务重复登记为新流程。已读阈值、撤销与精确续读仍保留未知。

### 摄入量与观点范围的区别

**事实边界：**每日提醒可以关闭，已读可以隐藏，主题和来源可调整；这些控制没有被证明构成“读满若干分钟自动结束”的机制。非个性化首页与个性化流并存，也不等于首页完全不受地区和用户配置影响。全局筛选的持续默认与单事件条件的复位应分开记账。

**研究推断：**专题布尔条件能降低无关内容，屏蔽来源/偏向筛选却也可能缩窄证据范围。可先围绕一个问题保存事件，再对照原文差异并停止；这是本项目建议的知识任务边界。设计时应展示已启用的排除条件、被排除来源数量及可撤回入口，而不是只显示“已个性化”。时间减少和观点扩大需要分别评价，既不能以更多阅读量为成功，也不能用左右比例均匀证明事实判断正确。

My News Bias 的公开 demo 明确区分 Stories 与 Articles；它描述接触的新闻集合，不是理解测验。本次未获得行为计数、清除历史、统计更新延迟或隐藏已读撤销的具体规则。源评级分布也不能直接推断读者自身政治立场。

账号范围补充：Vantage 帮助称升级后保存流、关注和阅读历史仍属于原账号，且 My News Bias 当时为 Web 专属；不将“移动支持即将推出”算成已上线。Full Coverage 的外部付费墙仍由原媒体控制。对 5–6 岁孩子，来源政治标签、争议事件和自我阅读统计都不是现成的儿童理解支架；成人选题与解释仍必要。

### 新证据索引

均于 2026-10-26 打开正文；未查看帮助内截图或点击账户控件。页内“Thanks for feedback/提交失败”是帮助中心模板，不是产品状态。

- E-GND-01：[搜索/找覆盖](https://help.ground.news/en/articles/5609857)，Web/App、URL、In Focus、事件内来源搜索，2026-03-09。
- E-GND-02：[地区新闻](https://help.ground.news/en/articles/1001537)，Web Edition、移动 Top Stories，2026-03-09。
- E-GND-03：[本地流](https://help.ground.news/en/articles/5428225)，两端设置、覆盖不足补充，2026-03-11。
- E-GND-04：[减少主题与回改](https://help.ground.news/en/articles/1499905)，Show Me Less、取消和管理，2026-03-09。
- E-GND-05：[取消关注](https://help.ground.news/en/articles/911873)，手机/iPad/Web、trending 限制，2026-04-15。
- E-GND-06：[移除媒体](https://help.ground.news/en/articles/916353)，Remove Source→Update、Web 同步，2026-03-09。
- E-GND-07：[自定义 Web 专题](https://help.ground.news/en/articles/866177)，三个条件、Confirm/刷新及套餐，2026-03-09。
- E-GND-08：[Web 全局来源过滤](https://help.ground.news/en/articles/7212801)，Preferences、默认作用范围，2026-03-09。
- E-GND-09：[移动全局付费墙过滤](https://help.ground.news/en/articles/3523713)，开关和 Premium/Vantage，2026-03-09。
- E-GND-10：[通知配置](https://help.ground.news/en/articles/898305)，Android/iPhone、每日更新开关，2026-03-09。
- E-GND-11：[外语翻译](https://help.ground.news/en/articles/3405953)，单篇/自动、Web 浏览器、不能过滤语言，2026-03-09。
- E-GND-12：[减少广告干扰](https://help.ground.news/en/articles/1096641)，External Browser/Reader View、原站广告边界，2026-03-09。
- E-GND-13：[Podcasts and Opinions](https://help.ground.news/en/articles/4663041)，事件提及→原平台，2026-03-09。
- E-GND-14：[开始使用](https://help.ground.news/en/articles/225473)，账号与 Note About Internet Access，2026-03-16；限定旧 E-GRND-02 的离线范围。
- E-GND-15：[Vantage 功能与 FAQ](https://help.ground.news/en/articles/7541441)，My News Bias、Web 范围、升级数据和 newsletters，2026-03-09；不据宣传结果承诺理解改善。
- E-GND-16：[My News Bias 公开 demo](https://ground.news/my-news-bias-vantage)，只读 Demo Data 字段，示例日期/生成机制未知。
- E-GND-17：[订阅未识别](https://help.ground.news/en/articles/244865)，购买渠道、恢复的副作用、原登录方式，2026-08-29。
- E-GND-18：[深色模式](https://help.ground.news/en/articles/1858561)，Web 菜单→Settings→Website Theme；移动 Profile→Settings→Preferences→Night Mode，2026-03-09。
- E-GND-19：[浏览器扩展](https://help.ground.news/en/articles/1750529)，平台范围及比较入口，2026-03-09；未安装。

地区帮助链接的“New App Update”文章 `1835265` 本次为 404，未据不可读视频补全手势。定向搜索未取得可用的逐条清除阅读历史、已读撤销、邮件分类管理及精确续读帮助；仍列为缺口，不写成产品不支持。

## 2026-10-27：完整帮助目录与状态边界

[目录CSV](../batches/ground-controls-deepening-inventory.csv)登记8顶层、3子类和65篇独立帮助文章，共76节点。仅以article-card计正文入口，重复快捷卡及同数字ID的别名slug不另计。正文已读、仅目录和原有证据分开；包含账单/创作者合作，不能把76当软件功能总数。E-GRCD-01。

本轮正常匿名Chromium访问search入口HTTP403后即停止，未登录或绕过，没有产品动作或D4。帮助正文及图取得HTTP200；三张[移动筛选](../artifacts/ground-controls-deepening/factuality-image-3.png)、[Web筛选](../artifacts/ground-controls-deepening/factuality-image-4.png)、[来源评级](../artifacts/ground-controls-deepening/factuality-image-5.png)均实际查看，但不是连续操作。完整边界见[来源摘记](../artifacts/ground-controls-deepening/source-notes.md)。

### 已有任务修订

- F-M046-01：Full Coverage→Sort & Filter；排序Bias/Factuality/Latest/Alphabetical，偏向/事实性/所有权/Locality/Paywall组合；Show Results应用，Clear或换事件复位。移动静态图含Unknown，比较摘要覆盖不足时只给普通摘要。 零结果、取消回滚、外链返回位置/付费墙与全局默认优先级未实测；教程三机构与方法论两机构口径不一。E-GRND-01、E-GRND-03、E-GRND-04、E-GRND-07、E-GRCD-02。
- F-M046-03：Discover关注后进入个性化流；For You帮助明确使用地点、关注主题/来源和App使用模式，不是只按显式订阅。移动Premium+可Profile Settings打开Hide Read Stories。 已读阈值、撤销、隐式行为权重与关闭个性化入口未核；观看/打开不等于理解。E-GRND-05、E-GRND-06、E-GRCD-07。
- F-M046-04：Web旗帜、Android事件三点、iOS邮件入口报告问题；建议缺失来源时读者邮件提供名称/URL，机构另走表单且不保证收录或时限。 未提交；成功反馈、修复时延、聚合错配处理及新增来源审核结果未知。E-GRND-08、E-GRCD-11。
- F-M046-05：地区主题按钮；本地手动搜索；覆盖不足补更大地区。地区可能同时改变偏向条配色，但可独立设颜色，不能混为内容过滤。 。E-GND-02、E-GND-03、E-GRCD-09。
- F-M046-06：标题/关键词/URL找覆盖，来源名找媒体页；In Focus持续更新事件。官方无静态可下载全来源清单，搜索未命中不代表来源永不支持。 检索穷尽/匹配失败提示、事件更新版本/终点、外链返回位置未核。E-GND-01、E-GRCD-05、E-GRCD-06。
- F-M046-08：取消不保存，Manage Sources & Topics回改，App屏蔽反映到Web；Positive帮助把Show Me Less写作减少而非完全禁止，实际算法效果未测。 。E-GND-04、E-GND-06、E-GRCD-07。
- F-M046-09：Web For You Custom Feeds→命名→任一包含/全部包含/任一排除→Confirm刷新。专页称Premium/Vantage，Positive页称Pro+，不统一套餐含义。 编辑/删除、空结果、当前移动是否上线及套餐措辞冲突未核。E-GND-07、E-GRCD-07。
- F-M046-16：网站购买须回正确账户/原登录方法；绑定其他登录方式后仍属原账户，不是转移订阅。商店Restore In-App Purchase帮助警告会重启已取消订阅；网站购买用此入口可能持续加载。 恢复是否成功/完整保留未知；不能由邮箱域名推注册方式，不能把Restore当无副作用刷新。E-GND-17、E-GRCD-13、E-GRCD-15、E-GRCD-16。

### 新增任务

#### F-M046-17 修改个人政治偏向评级并回查

Web/App搜索媒体→来源页铅笔进入修改；Manage Sources & Topics查看已改项；只限政治偏向。已知问题页为部分Android列源页已有评级/事件页可改的替代入口。

D2；E-GRCD-04、E-GRCD-10。缺口：编辑选项、确认/取消/恢复默认、跨端同步、对统计/筛选作用及当前构建是否仍有故障未核。

#### F-M046-18 独立切换偏向条配色

Web账户Preferences→Bias Bar选US/International可不改地区版；App Profile→Settings→Bias Bar Color Scheme；美国与国际红蓝含义相反。

D2；E-GRCD-03、E-GRCD-09。缺口：未操作颜色生效/跨端同步；不能据颜色推断内容或用户立场发生变化。

#### F-M046-19 为同一账户关联与移除登录方式

Web Account Information→Sign-in methods查看/增加/移除；每provider一账户；文档称原邮箱、保存文章和订阅不变，移除登录方式不删账户；找回按原注册方式分流。

D2；E-GRCD-13、E-GRCD-15、E-GRCD-16。缺口：授权弹窗、重复/冲突账户、最后登录方式移除、失败回滚及实际恢复未核；不将Gmail邮箱等同已用Google注册。

#### F-M046-20 请求删除账户数据并区分订阅

帮助要求全部设备先登出→填写数据删除表→声明删除数据不影响订阅；Web Account和App Settings底部退出。

D2；E-GRCD-14。缺口：未打开/提交表单；字段、删除完成反馈/时限、影响对象、撤回或备份恢复未核；不是取消订阅。

#### F-M046-21 处理新闻流加载失败及卡片标题错配

官方建议排查网络/VPN/广告拦截后重载，移动端重装重登；Compact Cards错配可Profile→Settings关该项；Known Issues标调查或修复中。

D2；E-GRCD-08、E-GRCD-10。缺口：仅官方建议；未执行/测恢复成功；未给构建、缓存/偏好保留、重试耗时或恢复后的滚动位置。

#### F-M046-22 追溯媒体评级及未评级状态

搜索媒体→来源专页→分别看Bias/Factuality/Ownership→评级机构链接查看依据；图含Unknown筛选与各机构分项；方法论说无评级不进入Bias Bar、无事实性评分则无score。

D2；E-GRCD-02、E-GRND-03。缺口：未打开机构详情/实际点击；平均计算与更新版本、个人改值如何影响总览、未评级在筛选计数中的规则未知；评价对象是媒体不是单篇。

### 相互矛盾的来源与本项目推断

事实性教程称平均三机构，方法论明确两机构；政治偏向才列三机构。教程所附来源页图也在事实性栏列两机构。保留冲突，不宣称“居中一定更准确”；未评级不等于中间或低事实性，评级层级是媒体整体。静态筛选图包含Unknown、Clear和关闭X，不能据图证明取消是否回滚、数目何时变化或缺少评级如何计算。E-GRCD-02、E-GRND-03。

积极新闻页把Custom Feeds写成Pro+，专页写Premium/Vantage；同一日期不同名称未自行等同。For You明确结合使用模式，这比仅“关注主题”多一层隐式输入；减少某主题不保证从全部入口禁止出现。Known Issues最后编辑为6月，不代表已经验证当前10月App仍发生同样问题。E-GRCD-07/10、E-GND-07。

本项目启发（建议）：选题价值、媒体属性、某篇证据、个人改值和阅读行为分别保存来源；新闻卡片与事件对象也可能不同步。AI需要能报告显式过滤和隐式推荐的已知边界，不能把“政治偏向改成中间”当成报道事实变化。对儿童应另外研究年龄适配和理解支架，而不是把成人评级条缩小搬用。账户身份、登录凭据、内容记录、订阅及删除请求需分开，仍不等于已选定本项目账户设计。

本轮仍缺实际筛选零结果、外链返回、收藏撤销、已读阈值、个人统计更新和备份恢复；当前App未安装/登录，无法验证触感、时延、推荐变化或学习/认知收益。

### 本轮证据

- E-GRCD-01：[帮助中心完整可见目录](https://help.ground.news/)；8个顶层类、3个子类；65篇独立article-card；更新日期未标。
- E-GRCD-02：[事实性筛选与原始评级依据](https://help.ground.news/en/articles/5224577)；Web/App筛选、来源页；3张已查看官方图；更新日期2026-04-15。
- E-GRCD-03：[偏向条与地区配色](https://help.ground.news/en/articles/245249)；US与UK/Canada/International语义；更新日期2026-03-12。
- E-GRCD-04：[修改个人政治偏向评级](https://help.ground.news/en/articles/245313)；两端新闻源页铅笔、Manage Sources & Topics；更新日期2026-03-09。
- E-GRCD-05：[In Focus事件流](https://help.ground.news/en/articles/722625)；世界事件持续更新；未给归档/结束规则；更新日期2026-03-09。
- E-GRCD-06：[新闻源搜索与目录范围](https://help.ground.news/en/articles/6049601)；没有完整静态可下载来源清单；两端搜索/关注；更新日期2026-03-18。
- E-GRCD-07：[正向主题与推荐依据](https://help.ground.news/en/articles/950337)；专题、For You输入、Custom Feeds套餐措辞；更新日期2026-03-09。
- E-GRCD-08：[新闻流不加载的恢复建议](https://help.ground.news/en/articles/263489)；网络/干扰排查、重新加载、重装及求助；更新日期2026-03-12。
- E-GRCD-09：[配色与新闻地区独立设置](https://help.ground.news/en/articles/263425)；Web Preferences Bias Bar；App Profile Settings；更新日期2026-04-11。
- E-GRCD-10：[已知问题与临时处理](https://help.ground.news/en/articles/5237505)；2026-06-17页面：编辑评级、登录、加载、Compact Cards；更新日期2026-06-17。
- E-GRCD-11：[建议增加新闻来源](https://help.ground.news/en/articles/1750273)；读者邮件建议/机构提交表单；不保证收录；更新日期2026-10-13。
- E-GRCD-12：[新闻配置导览](https://help.ground.news/en/articles/6220033)；链接式任务目录；不是独立界面操作；更新日期2026-03-09。
- E-GRCD-13：[为同一账户关联登录方式](https://help.ground.news/en/articles/6692033)；Account Information Sign-in methods；每provider一个；更新日期2026-06-10。
- E-GRCD-14：[请求删除账户数据](https://help.ground.news/en/articles/334913)；各设备先退出、删除表单和声明；订阅状态另管；更新日期2026-07-28。
- E-GRCD-15：[找回登录入口](https://help.ground.news/en/articles/245697)；邮件密码与社交登录分流；更新日期2026-03-09。
- E-GRCD-16：[密码重置失败](https://help.ground.news/en/articles/335041)；社交登录、垃圾邮件、链接过期求助；更新日期2026-03-09。
- E-GRCD-17：[家庭订阅边界](https://help.ground.news/en/articles/648577)；本文声明无家庭分享套餐；不外推数据共享全部能力；更新日期2026-03-09。

原E-GRND-01/03/04/06、E-GND-07/17复查沿用编号，追加原HTML与SHA；同一来源重抓不增加证据数量。没有发送反馈、申请删除、关联账号、重装或改变任何产品账户状态。

## 目录正文补读：联网、语言与版本范围（2026-10-27）

原目录四篇补正文，保持21个任务，不把简短功能说明另算交互。原址、日期和静态套餐图见[来源范围核查](../artifacts/ground-source-scope/source-notes.md)。

| 补回任务 | 已有依据与限制 | 来源 |
| --- | --- | --- |
| F-M046-02、F-M046-16 | Web/App需网络，无离线浏览；相同凭据可访问权益，不证明全文缓存、阅读位置或全数据同步 | E-GNRS-01 |
| F-M046-16 | 套餐页及一张概览图列权益，仍残留Vantage移动版即将推出的措辞；不能把更新时间当所有描述已核准，也不作当前套餐保证 | E-GNRS-02 |
| F-M046-01、F-M046-22 | 多源聚合/Blindspot为机制声明，三评级机构措辞未解决已有口径差异；不等于单篇事实核验 | E-GNRS-03 |
| F-M046-06、F-M046-11 | 非英语来源按原语言展示；与移动翻译能力分开，不能由此推断中文UI、自动翻译或全部中文媒体覆盖 | E-GNRS-04 |

没有登录、离线尝试、订阅更改或实际界面操作。四篇各一次GET均200；全局安全脚本/CSP曾触发抓取器误判，离线核查保留正常文章正文及纠正记录，没有把误判报告当产品行为。

## 结构化竞分（2026-10-28）

- 交互（关键控件）
  - 事件页签：Bias Comparison、Full Coverage 的 Sort/Filter（F-M046-01）
  - 个性化：关注/取消关注/Show Me Less、自定义专题流（F-M046-03/07/08/09/10）
  - 回访与提醒：保存、隐藏已读、通知（F-M046-02/03/13）
  - 外部阅读与媒体：Reader View/External Browser、Podcasts and Opinions（F-M046-11/12）
  - 反馈：Report an Issue/建议新增来源（F-M046-04）

- 信息架构（对象/层级/状态）
  - 对象：事件（event）/报道（story）/来源（outlet）/专题流（custom feed）/评级（bias/factuality/ownership）
  - 本批第三方证据补充：`E‑GRCD‑18/19` 用于佐证“偏向/事实性/所有权方法论与阅读入口/离线说明”的官方口径。

本批补充记录（2026‑10‑28）：匿名网页未取得“筛选→清除/撤销→刷新/返回”的稳定可操作闭环；见 `../artifacts/ground-news-controls-deepening/2026-10-28-browser-operation-audit.md`（E‑GRCD‑18）。保持 D2，并注明“为何无 D4”（需登录/有状态环境）。
  - 状态：偏向条/事实性评分基于“媒体整体”，非逐篇；过滤条件有“全局默认”与“事件内临时”两层（F-M046-01/10/22）
  - 地域：Edition 与 Bias Bar 颜色独立设置（F-M046-05/18）

- 关键任务流（D2）
  - “找事件→比较→筛选→读原文→保存回访”：F-M046-01/02/06/11
  - “个性化→减少干扰→专题聚合→默认过滤”：F-M046-03/07/08/09/10
  - “异常/缺口→报告→跟踪 Known Issues”：F-M046-04/21

- 失败与恢复（已知/未知）
  - 已知：不支持离线；URL 找覆盖不保证命中；过滤过窄可清除（F-M046-02/06/01）
  - 未知：精确续读/外站返回位置、已读/收藏撤销、个性化与默认过滤冲突时的优先级、统计更新延迟（F-M046-02/03/10/15）

- 对 E-Ink4 Plus / 5–6 岁儿童 & 成人家庭（推断）
  - 启发：将同一事件聚合与差异比较作为“个人报纸”的基础操作；把“来源/立场/事实性/所有权”显式展示给成人用户（F-M046-01/22）
  - 风险：评级是媒体层级，非逐篇；Blindspot 与过滤容易造成“只看一侧”；无离线/续读在墨水屏上阅读体验与回访成本较高（F-M046-01/02）
