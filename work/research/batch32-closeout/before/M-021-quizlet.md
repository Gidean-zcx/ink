# M-021 Quizlet：资料生成、校订、作答与复习

查阅 2026-10-27；范围为查阅日官方 en-US 帮助及原批次 en-AU/en-CA 材料涉及的 Web、iOS、Android。未安装、登录、生成、提交答案或修改账户；所有流程仍是 D2 文档重建，没有连续视觉观察。精确 App 构建、功能上线日期及帮助修订日期未知，不把网页抓取日当版本号。免费、Plus/Unlimited、教师账户资格随功能区分；美国评分帮助不推广为所有地区均可用。本轮直接 HTML 请求返回 403 后停止该路线，保存的是公开网页工具返回的正文文本，**不是源 HTML 或界面截图**。

## 功能目录与范围

本轮按官方 Studying 类目及三个展开目录登记 [逐项 inventory](../batches/quizlet-controls-deepening-inventory.csv)：**4 个父级、64 个文章入口，共 68 行**，不是 68 项已分析功能。前三组分别 23/12/27 篇；Folders 的两项来自类目页。未涵盖 Account、Billing、Teaching、Schools 等其他类目，也不宣称产品全功能已穷尽。目录仅证明入口存在；未读正文的条目均保留为目录级。E-QZCD-01～04。

| 功能族 | 已核范围 | 未完成部分 |
| --- | --- | --- |
| Creating, editing, and saving sets（23） | F01～05 的草稿/生成/导入/校订；补整集删除、隐藏动态的区别 | Claude 创建、Study Groups、扫描、图表集、录音、图片、合并、评分等入口尚无本例完整任务；AI事实核查与生成失败仍缺 |
| Accessing and sharing（12） | 导出、离线及 F02/04 的卡组可见性路径 | 搜索、课堂、共享与编辑权限、密码、嵌入、访问失败未逐项拆解 |
| Studying（27） | F06～09；补 Write/Spell 的纠错与结束、Learn 重学、F11 Test、F12 AI Practice Test、F13 Progress/星标 | Podcast、Audio Notes、Ask Quizlet、外部集成、Charms/Blocks/Match/Live、朋友互动及 Expert Solutions 多数仅目录；不套用已研究模式的规则 |
| Folders（类目页列 2） | 已知有 tags、folders 文章；F13 只引用跨集星标复习的文档分支 | 文件夹/标签建立、移除、共享、恢复本身尚未重建；文章标题不作为操作依据 |

## 操作流程

| F 编号 | 入口 / 前置 | 操作与反馈 | 状态 / 结束 | 缺口及证据 |
| --- | --- | --- | --- | --- |
| F-M021-01 AI生成并校订 | 登录→Create→Create a new flashcard set | 输入/粘贴资料、上传文件或给主题；Smart Assist产出预览草稿，可编辑、删卡、添卡；同一集可切手动创建 | 校订后发布成为学习集；生成预览与发布分开 | 生成失败、重生成覆盖、自动事实核查未证实。E-QZLT-01 |
| F-M021-02 Study Guide | 网站帮助路径 Generate→Study guide→上传/粘贴→Start transforming | 生成大纲、卡组等；More 可改题名、可见性、查看原上传或删除 | Your library→Study guides 找回；生成卡组另设可见性，具体路径见下文 F04 | 免费次数有限；限定英/法/德及所列地区年龄，最低门槛14岁；中国大陆未在所列地区。指南正文/生成题目的逐项校订控件与删除联动未知。E-QZLT-02、E-QZCD-08 |
| F-M021-03 结构化导入 | 网站 Create→Flashcard set→Import | 粘贴术语/释义，按分隔符解析；Import后选两侧语言→Create | 文本成为卡对；语言选择影响后续语音/评分 | 词义可用逗号、Tab或横线分隔，行可分号或换行；异常行与重复合并未知。E-QZLT-03 |
| F-M021-04 校订、复制与导出 | 自有集 More→Edit；他人集 More→Make a copy | 编辑文字/语言、行顺序与两列；网站行垃圾桶删卡，手机左滑卡再点垃圾桶；保存 Done/勾号。复制后 Create；自建集 More→Export→排列→Copy text | 补卡组可见性、整集删除及隐藏动态分支，见下文；导出仍仅文本 | 高级内容/复制品导出限制保留；单卡误删、历史版本及并发编辑恢复未知，不能用整集删除说明替代。E-QZLT-04、11、12；E-QZCD-05～08 |
| F-M021-05 草稿中断与恢复 | 创建/编辑时自动保存；Library→Flashcard sets→In Progress | 重开草稿继续、Create发布或丢弃；未保存词旁有警告，断网时保留网页等待重连 | 草稿仅自己可见，不在网站与App间同步；移动端发布前退出账号会丢弃草稿 | 丢弃不可恢复；自动保存计数可见，重启进程后的边界未知。E-QZLT-05 |
| F-M021-06 翻卡自查 | 集→Flashcards | 点击翻面，网站箭头/键盘前后；手机滑动；Options可排序至Still learning/Know、设卡面及播放 | 看完全部卡完成一轮；手机X退出保存本设备会话位置 | 文档明确该进度不跨设备；语音缺失可能是语言不支持。E-QZLT-13 |
| F-M021-07 Learn与错题重答 | 登录→集→Learn→选目标；Study path 须执行 Start studying 才保存，停在设置不算已保存 | Options 选题型/方向/星标/声音；Write/Spell 的错题、结束与重学详见下文 | 重置文档明确移除 Learn 进度；不能据此清空所有模式的历史 | 免费轮次有限；Android Study path 范围存在帮助页分歧；普通 Learn 中断点、跨日调度和重置撤销仍未知。E-QZLT-06；E-QZCD-09～12 |
| F-M021-08 评分与误判纠正 | Learn→Options→Grading options | 选 Relaxed/Moderate/Strict 保存；网站 Test 另有 Require 1 answer only，定义需按逗号/斜杠/分号分隔多答案 | 连胜只计首答，五题起连胜；I was correct 可纠正相关误判，切题型重置连胜。单答案偏好开启后影响该集其他活动，可取消勾选 | Learn 语言/至少3卡条件仍适用；Test 改评分选项仅网站。不能把 Learn/Write 误判按钮套到 Test，也未核它是否改写长期调度。E-QZLT-07、08；E-QZCD-09、17 |
| F-M021-09 间隔复习 | 网站集→Flashcards→Spaced repetition | 翻面后选Repeat/Hard/Okay/Easy，安排不同再现时间；直至没有到期卡 | Options可切学习方法、日新增量；再次打开保留所选方法 | 评级例示间隔不是效果保证；移动端该方法未支持于所查帮助，误评撤销未知。E-QZLT-09 |
| F-M021-10 离线准备与恢复 | iOS Profile→Your settings；Android Account→离线保存 | 自动缓存最近8集，也可单集下载；勾选表示保存。Library的Downloaded/Downloads进入；勾选→移除下载 | 离线Flashcards/Match及创建可用；重连刷新同步离线记录，编辑内容自动发布 | 灰标题未离线可用；下载可能需数分钟。会话位置同步参见F-06，不能据总页承诺全状态恢复。E-QZLT-10、13 |
| F-M021-11 卡组 Test（新增） | 登录→已有集→Test→题量/题型→Start test | 网站作答后 Submit test，显示分数并可回看；Print test 保留已评分副本。移动帮助写答完全部题后看分数/回顾 | **未做完离开不保存进度**；可选仅星标词，复习筛选见 F13 | 免费每集一次；课堂邀请资格另有条件。漏答提交、答题中改题、超时、再次考试与历史成绩保留未说明；不臆造错题解释或误判按钮。E-QZCD-13、17 |
| F-M021-12 AI Practice Test（新增） | 网站 Generate→Practice test→选卡组或粘贴/上传→Start transforming | 调整题数/题型/计时→Take this test→作答→Submit test→分数/回顾；More 可改标题、可见性、查看原上传 | 手机帮助给“+→Practice test→粘贴/文件→题数/题型→Take this test”，已存试卷在 Library 的 Practice tests | 英语及地区年龄/套餐限制；手机文本至少300字符、书面回答至多200字符。计时到零、暂停恢复、题干答案校订/重生成和错题解释未知；不套 Test 的丢进度规则。E-QZCD-16 |
| F-M021-13 Progress/星标定向复习（新增） | 登录→集；Progress 分组需 Plus，手动词条星标是另一路径 | 网站对 Not Studied/Still Learning/Mastered 分组星标→Starred→Review with an activity；移动帮助可直接选择分组学习。手动星标也可选子集进入活动 | 星标只对本人可见，移动端再点星号取消；Progress 汇总跨活动答对/错次数并跨端同步，游戏不计 | Mastered 是产品分类，不是独立掌握测量；阈值、误判重算、共享集修订后的映射、星标与 SRS 调度互通未知。E-QZCD-14、15 |

完整主链以自有资料为例：F-01生成草稿→F-04人工核对词义/语言→发布→F-07作答→F-08处理评分/误判→F-09到期复习。各阶段是不同对象/模式；没有证据表明 AI 生成后无需校订，也未证实 Learn 错题自动无缝迁移到新的 Spaced Repetition 调度。

F04 的删除/可见性分支：网站 Library→Flashcard sets→Created→自有集→More→Edit→顶部可见性→选择→Save→Done；移动帮助采用 Edit set→Settings→Visible to→Done。常规新集默认公开，但低于地区同意年龄者只能建立私有集，不能把默认值无条件套所有账号。More→Delete/Delete set 只能删除自建集，官方明确不可恢复；未给二次确认步骤，不自行补一个确认框。隐藏他人集则是网站 Library→Flashcard sets→Studied→标题旁 More→Remove→Hide this set；手机 App 不提供此隐藏操作，帮助建议移动浏览器。**隐藏动态、删除卡对、删除整集、移除离线下载是不同对象**；隐藏后的找回入口、删集与生成指南的级联关系仍未知。E-QZCD-05～08。

F07 的练习与结束分支：Write 在 Learn→Options 进入，输入后以 Answer/手机键盘箭头提交；第一轮遍历后第二轮重点处理错题，文档要求每题答对两次才结束并显示结果。Don't know 揭示答案；手机帮助要求错答后重输才能前进；误拼可用 I was correct 修正。Spell 仅网站（手机须用浏览器）：听音输入，错字被标出并逐字播读，再尝试；每词正确两次显示本轮结果，可重播/调慢声音。该次数只是模式完成条件，不是长期掌握证明。E-QZCD-09、10。

Study path 重学沿用 F07：网站 Learn→Options→Reset Progress 下 Start Over→Yes, restart Learn；另一移动段列 Restart Learn，均说明删除 Learn 进度。取消/撤销、与 Progress 分组或间隔复习重置的关联未交代。设置页称进度可在网站和移动 App 同步，重置页末却仅列网站与 iOS；**Android 的当前支持范围未解**，不能用前一页自动改写后一页。Learn 总页的移动选项未列 Write，专门 Write 文档却有手机操作；精确构建未知，保留专页路径及概述不完整的差异。E-QZCD-11、12；E-QZLT-06。

AI 资料的可校订边界：Smart Assist 明确给出发布前可改/删/增的卡片预览；Study Guide 文档的具体设置主要是题名、权限与原上传；Practice Test 的 More 也没有列出题干和答案编辑。不能把“生成后可学习”写成“生成题目已经校验”。Practice Test 与 Study Guide 的资格也不同，例如英国生成 Study Guide 门槛为14岁，而 Practice Test 为16岁；中国大陆不在两文所列地区，未验证实际资格。原上传改动后是否更新卡组/试卷、重新生成是否覆盖校订均未知。E-QZLT-01、02；E-QZCD-16。

F13 的移动 Flashcards 分支：星标帮助要求先在 Options 关闭 Sorting，再选 Restart Flashcards 才进行星标；这不是“保持当前翻卡位置”的承诺。跨集星标可借文件夹聚合，再在所选活动的 Options 选 Study starred；文件夹管理本身仍只有目录登记。E-QZCD-15。

## 分支与限制

- “草稿自动保存”“发布集跨设备”“离线学习同步”“Flashcards 会话位置”“Progress 答题统计”“Learn path”和“Test 未完成会话”分别记录。离线帮助概述同步全部离线进度，Flashcards 专页仍说手机会话位置不跨端；Progress 的跨端承诺不能覆盖 Test 明确不保存的会话。
- 发布集与 Study Guide 可见性分别管理；不能由指南私有推断其生成卡组私有。F04 的默认公开有年龄例外；共享编辑授权、权限变更对已有分享链接的生效时点仍未核。
- Test 评分设置的一项答案偏好可跨该集活动生效，但 Test、Learn、Write 的误判修正控件不相同；没有 Test 的 I was correct 依据，也没有提交后按错题自动生成重做队列的依据。定向重学只能采用 F13 已给出的分组/星标路径。
- 未覆盖：空资料、上传超限、生成超时、题目无正确项、AI试题逐题编辑、Practice Test 计时到零和会话恢复、取消订阅后缓存、撤销回答后的审计、同步冲突、账户切换后的下载清理。未知不等于没有功能；Study Guide 删除不可恢复与否也不能照抄卡组 Delete 规则。

## 数据、激励与本项目关系

事实可区分原始上传、生成指南/试卷、卡组/卡对、草稿/发布状态、权限、星标、自评/答案、Learn 进度、Progress 分组、连胜及下载状态。连胜、每题两次正确、Mastered 标签都是产品操作或统计口径，不等于独立验证的知识掌握。生成能力的年龄/地区条件也使其不能直接作为本项目5–6岁儿童自主账户路径。

关联 G-001/G-002、R-016/019/021、H-004/005：本项目可借鉴“资料→可审阅草稿→发布学习包”，把来源定位、校订理由和版本归属于每张卡；AI 应分清修正题目、纠正评分、重置 Learn 和修改复习计划。生成来源、题目校订、提交结果与可恢复状态应分别可查询；进入某学习模式不能被 AI 回报成“已学会”。个人新闻转学习素材可沿该链，公开分享有独立状态。以上均是推断，未选定 Quizlet 接口或算法。

最小后续观察：固定一个网站和一个手机构建，用三张自建卡分别验证断网草稿、误判更正、会话退出、离线同步、跨设备与间隔复习之间的数据边界；本轮未执行。

本轮有限补查已得到可用的普通 Test、Write/Spell 和定向复习文档，但没有模拟登录使用、生成或作答。直接抓取 Test HTML 的一次请求为 403，停止该路线，未换请求头或接口绕过；保存的网页工具响应含多个平台段，正文未明确对应端的独有细节不据导航标签顺序推断。新的操作细节已合回旧 F02/04/07/08，另增 F11～13；沿用原任务编号，未提升实际操作等级。

## 证据

- E-QZLT-01：[Creating study sets with Smart Assist](https://help.quizlet.com/hc/en-us/articles/39606772122509-Creating-study-sets-with-Smart-Assist)；To use; Tips。正文；预览草稿可编辑/删/增，未验证生成结果。
- E-QZLT-02：[Studying with Study Guides](https://help.quizlet.com/hc/en-us/articles/18312306436365-Studying-with-Study-Guides)；Eligibility; platform creation; visibility。正文；地区年龄及语言限制，原始上传与生成卡组分开。
- E-QZLT-03：[Creating sets by importing content](https://help.quizlet.com/hc/en-au/articles/360029977151-Creating-sets-by-importing-content)；Import steps; delimiter rules。官方检索正文；网站文本导入流程。
- E-QZLT-04：[Editing sets](https://help.quizlet.com/hc/en-au/articles/360030212131-Editing-sets)；Web/mobile edit; delete; settings。官方检索正文；校订词义、语言及顺序。
- E-QZLT-05：[Editing draft sets](https://help.quizlet.com/hc/en-us/articles/360029768912-Editing-draft-sets)；Drafts; offline warnings; logout。正文；草稿不跨网站/App同步，丢弃不可恢复。
- E-QZLT-06：[Studying with Learn](https://help.quizlet.com/hc/en-us/articles/360030986971-Studying-with-Learn)；Start Learn; Options; Write/Spell。正文；目标、题型及重输错题。
- E-QZLT-07：[Using grading options (US)](https://help.quizlet.com/hc/en-us/articles/360048313652-Using-grading-options-US)；Relaxed/Moderate/Strict; Learn/Test settings。正文；美国帮助范围，评分宽松程度及语言条件。
- E-QZLT-08：[Studying with Answer Streaks](https://help.quizlet.com/hc/en-us/articles/40011154960653-Studying-with-Answer-Streaks)；How streaks work; I was correct。正文；首答连对及误判修正。
- E-QZLT-09：[Studying with Spaced Repetition](https://help.quizlet.com/hc/en-us/articles/48324742264077-Studying-with-Spaced-Repetition)；Web start; four ratings; options。正文；当前仅网站，四档自评与日新增量。
- E-QZLT-10：[Studying offline with Quizlet mobile apps](https://help.quizlet.com/hc/en-ca/articles/360030565412-Studying-offline-with-Quizlet-mobile-apps)；iOS/Android download; sync; grey titles。正文；最近八组自动缓存及恢复联网刷新。
- E-QZLT-11：[Exporting your sets](https://help.quizlet.com/hc/en-us/articles/360034345672-Exporting-your-sets)；Export procedure; ownership restrictions。官方检索正文；仅网站导出自建集，不导出图片。
- E-QZLT-12：[Copying and editing a set](https://help.quizlet.com/hc/en-us/articles/360029781232-Copying-and-editing-a-set)；Make a copy; premium restriction。官方检索正文；复制后成为另一个学习集。
- E-QZLT-13：[Studying with Flashcards](https://help.quizlet.com/hc/en-ca/articles/360030988091-Studying-with-Flashcards)；Flip; sorting; mobile X and progress。正文；手机会话进度不跨设备，与离线总页口径需区分。

本轮补证（2026-10-27）：下列 L 是已保存网页工具响应的行号，配合文章标题/章节定位，不是原站固有锚点或源码行号。

- E-QZCD-01：[Studying](https://help.quizlet.com/hc/en-us/categories/360001601132-Studying)；类目分组及 Folders 两项。目录，非操作证明。
- E-QZCD-02：[Creating, editing, and saving sets](https://help.quizlet.com/hc/en-us/sections/360004698771-Creating-editing-and-saving-sets)；Creating/editing/saving：23篇目录。目录，非操作证明。
- E-QZCD-03：[Studying](https://help.quizlet.com/hc/en-us/sections/360005938691-Studying)；Studying：27篇目录。目录，非操作证明。
- E-QZCD-04：[Accessing and sharing](https://help.quizlet.com/hc/en-us/sections/360005877232-Accessing-and-sharing)；Accessing/sharing：12篇目录。目录，非操作证明。
- E-QZCD-05：[Editing sets](https://help.quizlet.com/hc/en-us/articles/360030212131-Editing-sets)；To edit / change language / delete terms；正文 L19–104。官方正文，经网页工具读取。
- E-QZCD-06：[Deleting a set](https://help.quizlet.com/hc/en-us/articles/360030171011-Deleting-a-set)；To delete a set；正文 L17–43。官方正文，经网页工具读取。
- E-QZCD-07：[Removing sets from your Recent activity feed](https://help.quizlet.com/hc/en-us/articles/360029004272-Removing-sets-from-your-Recent-activity-feed)；To remove；正文 L19–29。官方正文，经网页工具读取。
- E-QZCD-08：[Changing a set's visibility](https://help.quizlet.com/hc/en-us/articles/360030255191-Changing-a-set-s-visibility)；To change visibility / Who can see / age exception；L17–81。官方正文，经网页工具读取。
- E-QZCD-09：[Studying with Write mode](https://help.quizlet.com/hc/en-us/articles/360030990531-Studying-with-Write-mode)；To study / Settings / Tips；L19–80。官方正文，经网页工具读取。
- E-QZCD-10：[Studying with Spell mode](https://help.quizlet.com/hc/en-us/articles/360030645752-Studying-with-Spell-mode)；To study / Settings / mobile limitation；L21–42。官方正文，经网页工具读取。
- E-QZCD-11：[Setting up a study path](https://help.quizlet.com/hc/en-us/articles/360048314692-Setting-up-a-study-path)；To set up / saving condition；L21–57。官方正文，经网页工具读取。
- E-QZCD-12：[Resetting study path progress](https://help.quizlet.com/hc/en-us/articles/360048806511-Resetting-study-path-progress)；To turn off / Reset Progress / last platform statement；L19–41。官方正文，经网页工具读取。
- E-QZCD-13：[Studying with Test](https://help.quizlet.com/hc/en-us/articles/360030642972-Studying-with-Test)；To study / Tips；L21–59。官方正文，经网页工具读取。
- E-QZCD-14：[Using Progress for targeted studying](https://help.quizlet.com/hc/en-us/articles/360048803491-Using-Progress-for-targeted-studying)；Progress definition / targeted studying；L17–34。官方正文，经网页工具读取。
- E-QZCD-15：[Studying most-missed terms with stars](https://help.quizlet.com/hc/en-us/articles/360031172312-Studying-most-missed-terms-with-stars)；To star / Settings；L19–49。官方正文，经网页工具读取。
- E-QZCD-16：[Studying with Practice Tests](https://help.quizlet.com/hc/en-us/articles/25946589648013-Studying-with-Practice-Tests)；Generate/take / Settings / mobile Tips；L17–82。官方正文，经网页工具读取。
- E-QZCD-17：[Using grading options (US)](https://help.quizlet.com/hc/en-us/articles/360048313652-Using-grading-options-US)；Grading options in Test / Settings；L55–72、110–153。官方正文，经网页工具读取。
