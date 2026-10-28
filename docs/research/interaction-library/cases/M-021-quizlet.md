# M-021 Quizlet：资料生成、校订、作答与复习

## 结构化竞分（2026-10-28）

- 对象/产物边界：
  - 原始资料、草稿学习集（Draft Set）、发布学习集（Set）、Study Guide、Practice Test、文件夹/标签、共享会话与群组为不同对象；可见性（Visible to）与可编辑者（Editable by）独立管理（F‑M021‑01/02/04/12/21/26）。
  - 学习模式对象分离：Flashcards、Learn、Spaced Repetition、Test、Practice Test 与游戏（Match/Blocks/Blast）是不同模式（F‑M021‑06/07/09/11/12/14～17）。
- 完成/恢复语义：
  - Flashcards 会话位置在手机与网站间不跨端；Test “未做完离开不保存进度”；Learn 的 Reset Progress 会删除该模式进度（F‑M021‑06/11/07）。
  - 删除/隐藏/复制为不同操作：自建集 Delete 不可恢复；Hide 他人集不等于删除；Make a copy 生成新集且与来源分离（F‑M021‑04）。
- 对 E‑Ink4 Plus 与 5–6 岁+成人家庭的启发：
  - 把“生成草稿/人工校订/发布/学习模式进度/分享与群组/离线缓存”严格区分；“显示分数/连胜”不等于知识掌握；在设备上需显式呈现会话保存粒度与重置后果。

查阅 2026-10-27；范围为查阅日官方 en-US 帮助及原批次 en-AU/en-CA 材料涉及的 Web、iOS、Android。当前登记32条任务：31条D2文档重建，移动首页推荐1条D1。未安装、登录、生成、提交答案或修改账户，没有连续视觉观察或实际操作。精确 App 构建、功能上线日期及帮助修订日期未知，不把网页抓取日当版本号。免费、Plus/Unlimited、教师账户资格随功能区分；US与UK评分帮助分别限定。此前直接 HTML 请求返回 403 后停止该路线，保存的是公开网页工具返回的正文文本，**不是源 HTML 或界面截图**。

## 功能目录与范围

最初按官方 Studying 类目及三个展开目录登记 [逐项 inventory](../batches/quizlet-controls-deepening-inventory.csv)：**4 个父级、64 个文章入口，共 68 行**。前三组分别23/12/27篇，Folders的两项来自类目页。第54批后，原64篇均关联正文记录，另登记两篇从文章链接进入的补充材料，共70行；不是70项功能，也没有改动原目录的文章数量。两篇背景说明不单独新增任务，早期短记也不等于所有分支已核。未涵盖 Account、Billing、Teaching、Schools 等其他类目，不宣称产品全功能已穷尽。目录证据为E-QZCD-01～04，正文另列对应E。

| 功能族 | 已核范围 | 未完成部分 |
| --- | --- | --- |
| Creating, editing, and saving sets（23） | 原23篇均有正文记录，包括两篇背景说明；已有草稿/生成/输入/校订与合并，续补媒体、图示、Study Groups及评分 | 创建成功后的恢复、媒体替换/重录撤销、图点映射、成员变动、评分聚合及事实核查仍缺 |
| Accessing and sharing（12） | 原12篇均有正文记录；补公开搜索、教师班级、共享/嵌入、编辑与查看密码、访问失败，以及移动首页的概述 | 申请/撤权何时生效、接收端结果、旧副本与同步、空结果和推荐操作链仍缺；首页任务仅D1 |
| Studying（27） | 原登记27项均已有正文记录；续补音频指南/节目、Ask Quizlet、Superhuman Go、Charms、朋友会话、教材解答及关键词查找；三项旧游戏记录已回填目录 | 正文记录包括早期短记，不等于当前构建全部验证；播放器控件、会话/学习记录恢复、错误反馈和权限变化仍缺，Live仍仅加入路径 |
| Folders（类目页列 2） | 两篇正文补文件夹建立/加入内容、文件夹内标签及可见性条件，见F26 | 移除/删夹对内容影响、空夹、混合对象权限、标签重名/迁移与恢复未知 |

## 操作流程

| F 编号 | 入口 / 前置 | 操作与反馈 | 状态 / 结束 | 缺口及证据 |
| --- | --- | --- | --- | --- |
| F-M021-01 AI生成并校订 | 登录→Create→Create a new flashcard set | 输入/粘贴资料、上传文件或给主题；Smart Assist产出预览草稿，可编辑、删卡、添卡；同一集可切手动创建 | 校订后发布成为学习集；生成预览与发布分开 | 生成失败、重生成覆盖、自动事实核查未证实。E-QZLT-01 |
| F-M021-02 Study Guide | 网站帮助路径 Generate→Study guide→上传/粘贴→Start transforming | 生成大纲、卡组等；More 可改题名、可见性、查看原上传或删除 | Your library→Study guides 找回；生成卡组另设可见性，具体路径见下文 F04 | 免费次数有限；限定英/法/德及所列地区年龄，最低门槛14岁；中国大陆未在所列地区。指南正文/生成题目的逐项校订控件与删除联动未知。E-QZLT-02、E-QZCD-08 |
| F-M021-03 资料输入与结构化导入 | 网站导入；另有手动建卡及移动扫描分支 | 文本按分隔符解析后选语言；扫描需人工选词面/释义并校订，详见下方续补 | 输入、预览、发布分开；扫描至少两张才可发布 | 异常行、重复、相机拒绝、无字、取消重扫与提交时点未知。E-QZLT-03；E-QZMW-02、07 |
| F-M021-04 校订、复制与导出 | 自有集 More→Edit；他人集 More→Make a copy | 编辑文字/语言、行顺序与两列；网站行垃圾桶删卡，手机左滑卡再点垃圾桶；保存 Done/勾号。复制后 Create；自建集 More→Export→排列→Copy text | 补卡组可见性、整集删除及隐藏动态；第54批补Editable by与Visible to不同入口，详见下文。导出仍仅文本 | 高级内容/复制品导出限制保留；单卡误删、历史版本及并发编辑恢复未知。E-QZLT-04、11、12；E-QZCD-05～08；E-QZMS-05、07、08 |
| F-M021-05 草稿中断与恢复 | 创建/编辑时自动保存；Library→Flashcard sets→In Progress | 重开草稿继续、Create发布或丢弃；未保存词旁有警告，断网时保留网页等待重连 | 草稿仅自己可见，不在网站与App间同步；移动端发布前退出账号会丢弃草稿。跨端找不到先检查是否发布 | 丢弃不可恢复；重启进程、发布后同步时间及失败界面未知。E-QZLT-05；E-QZMS-05 |
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

## 2026-10-27续补：游戏活动的完整一轮与结束条件

本次从旧目录中的Match、Blocks、Live加入继续读正文，另发现Blast的单人/多人分支。旧68行目录快照保留；四个相邻任务登记为F14～17，均D2、未实际操作。[来源摘记与冲突](../artifacts/quizlet-game-modes/source-notes.md)可直接回查官方原页。

| F编号 | 入口与一轮操作 | 结束/恢复与限制 |
| --- | --- | --- |
| F-M021-14 Match | 集→Match→Start game→逐对选择；一局6对，错配计时加1秒；手机Options可重开/改图示配对类型 | 配完显示分数；重开确认、离开再进及历史保存未知。E-QZGM-01 |
| F-M021-15 Blocks | 至少6对；集→Blocks→Play；放3块，成行列得分，答对取得下3块；可换题、星标子集和题型 | 无步可走结束；答错说明、刷新代价、棋盘/最高分恢复未知。E-QZGM-02 |
| F-M021-16 Blast | Web多人主持选提示/答案、共享加入方式、Create/Start；可调速度/选项/时长。另有单人集→Blast→Play | 计时比较得分；错击/同分/提前结束/重连未知。英语集/地区/图示集限制需按来源核。E-QZGM-03 |
| F-M021-17 Classic Live加入 | 浏览器填代码、Join、名字、Next；App从Search进入，可扫码；之后等主持开始 | 只还原加入任务，未还原整局作答/结算；游客、无效码、取消和重连未知。E-QZGM-04 |

**Blast资格存在同日来源差异**：先前搜索摘要写教师专属且未列单人；root直接打开相同原页写学生和教师可主持，并有单人步骤。没有可靠修订时间，不能把两段拼成同一版本；当前采用root正文，保留差异记录。Classic Live的教师主持说明不借给Blast。

对G-001的启发是区分知识作答和游戏表现：配对速度、空间放置、抢答竞速的成绩不能互相替代，更不能自动等于记忆掌握。5–6岁儿童的文字难度、误操作与奖励解释仍需实际原型验证；没有决定本项目要采用这四种游戏。

## 2026-10-27续补：输入、临时组合和保存

本次补读原目录八项正文，更新原F01/03/04/05/08，不新建任务编号。逐篇原地址、正文定位与未核分支见[来源摘记](../artifacts/quizlet-material-workflows/source-notes.md)，原68行目录就地更新；总览和资格说明不冒充完整操作。

| 原任务 | 具体补充 | 不能从中推出 |
| --- | --- | --- |
| F01外部AI | Claude或ChatGPT接入Quizlet后，可从需求生成并在对话翻卡，再打开Quizlet编辑；E-QZMW-01/05 | Smart Assist发布前校订流程同样适用；默认账号/可见性已明确；AI可访问全部页面或任意数据 |
| F03扫描与手动输入 | 扫描词框需用户分别选择词面/释义，可键盘改错、加卡与预览删除；手动建卡可在所选卡下插入；E-QZMW-02/07 | OCR已正确配对、取消不写入、扫描草稿有相同恢复保证 |
| F04复制与合并 | 高级内容影响复制资格；网站组合可直接用于一次活动，或另存为新集且保留原集；E-QZMW-03/04 | 退出临时组合就丢失答题统计；永久新集自动继承星标/进度或持续跟随来源更新 |
| F05草稿与发布 | 手动创建说明再次区分自动草稿与显式发布；E-QZMW-07 | 自动保存覆盖强退、系统重启和编辑冲突 |
| F08地区评分说明 | UK帮助给等级默认条件、网站/手机提交入口，Test单答案偏好跨同集活动；E-QZMW-06，与原US帮助对照 | 语言例外在当前构建一致；设置变更会回算旧分数或重排复习 |

E-QZMW-08是学习方式总览，只帮助区分普通Test、AI Practice Tests和Learn下的模式关系；不能由“八种”推算全产品功能数。第52批时原17条任务均为D2文档重建，没有实际操作。外部AI连接拒绝、重复请求、生成对象持久化、扫描取消/重扫、合并冲突及旧记录映射，仍是下一步关键缺口。

## 2026-10-27续补：音频、问答与共享学习

第53批补八篇正文，原Studying目录27项均已关联正文依据；其中Blocks、Match、Live加入是回填既有第32批证据，不能算本批新阅读。当时其他目录仍有21个仅目录的文章条目，已在第54批补正文；不是21项完整功能。具体来源和字段限制见[音频与学习摘记](../artifacts/quizlet-audio-and-study/source-notes.md)。

原任务的补充：F01加入Superhuman Go的授权、上下文选择及跳转，E-QZAS-06；F02加入音频输入、上传草稿继续与错语言重建，E-QZAS-02。F12补同篇帮助明确排除的音频直接输入；不把Study Guide录音草稿的恢复规则套给F05卡组草稿，也不推导中间格式转换能绕过限制。

| F编号 | 任务与结束状态 | 主要缺口与证据 |
| --- | --- | --- |
| F-M021-18 生成音频节目并找回 | 选择资料/提示生成，进入Library收听；公开节目可分享 | 只还原生成与找回，未还原播放控件、续播或离线；E-QZAS-01 |
| F-M021-19 Ask Quizlet解释与继续学习 | 从首页/集页提问或上传，查看回答后选建议动作；最小化和离开的会话结果不同 | US14岁以上；生成内容与会话持久化分开，写入回执及引文未知；E-QZAS-03 |
| F-M021-20 Charms一局游戏 | 匹配得分，经问题元素答题可加步，步数耗尽结束 | 错题讲解/重做、结算、保存和黑白可区分性未核；E-QZAS-05 |
| F-M021-21 共享会话与群组结果 | 公开卡组选模式并分享，加入完成后看群组结果；匿名限一次 | 成员可见最新结果，模式统计不同；退组/撤回、断线及历史未知；E-QZAS-04 |
| F-M021-22 查教材解答及追加帮助 | 按教材/问题与章页题定位解答，部分可调用AI帮助；App另有扫描/近期入口 | 专家题库与AI输出的核验范围分开；空结果、纠错与生成练习保存未知；E-QZAS-07 |
| F-M021-23 在限定卡组范围查内容 | 网站选择最近看过或自建集，查词返回包含内容的集合；手机用浏览器 | 不是全站或单卡定位证明；分词、零结果与索引更新未知；E-QZAS-08 |

第53批时23条任务均D2、无实际操作。对本项目可讨论：AI是否应把会话、内容和进度的保存分别告知；成人听学如何回到原材料校订；儿童游戏奖励是否延长娱乐而未增加知识练习。它们是研究问题，未确认方案；Charms的产品规则也不是5–6岁适用性或学习收益的证据。

## 2026-10-27续补：媒体、组织、分享与权限

第54批补原目录剩余21篇正文，并沿链接读取录音故障与学校信息两篇。原始文章定位、平台差异与未知分支见[媒体与共享摘记](../artifacts/quizlet-media-and-sharing/source-notes.md)。全部通过公开文档重建，未录音、上传、分享、申请加入或提交评分。

| F编号 | 入口、操作与结束 | 状态、限制及未核分支 |
| --- | --- | --- |
| F-M021-24 给词卡添加媒体 | 编辑卡组→图库选图或上传自有图；网站选文本域→麦克风→按住空格录音，松开保存并回放，可重录/删除，整集另有Create/Done | 手机自有图片限释义侧，手机自定义音频只学已有；不能把局部回放当整集已保存。替换/重录撤销、拒权后草稿和离线未知；故障文章含旧系统步骤。E-QZMS-01～03、22 |
| F-M021-25 创建图像标注练习 | 网站新集→Add diagram→一张图→点位置→词/可选释义→加点，至少两点才能创建；可拖选模糊文字或移除点 | 手机可学习已有图示；图替换/点位重排、模糊撤销与同步未知。来源格式字样“MBP”原样保留，未验证。E-QZMS-04 |
| F-M021-26 用文件夹和标签归集材料 | 网站创建文件夹，从内容Save加入；手机文件夹加号筛选、选中高亮后Done。文件夹All旁建标签，内容Saved→标签→Save | 高亮不等于提交；标签被比作子文件夹，但不证明嵌套。空夹、混合权限、移除/删夹和恢复未知。E-QZMS-09、10 |
| F-M021-27 分享或嵌入学习活动 | 对象Share→选渠道/复制链接；公开集More→Embed→选活动→复制HTML到外部网页，嵌入内可换模式/打开原集 | 同页既称仅公开可分享，也写密码集可分享且另传密码，保留范围差异。接收端访问、旧链接撤回、嵌入身份/进度未知；不是控制API。E-QZMS-06、20 |
| F-M021-28 查找公开学习集 | 主题搜索→Flashcard sets→按词数/作者类型/内容筛选；网站另有学校/课程；可按用户名找作者，点标题学习 | 与F23的本人Recent/Created内容搜索分开；筛选不证明题目准确。排序、空结果、来源版本和校验未知。E-QZMS-17 |
| F-M021-29 加入及退出教师班级 | 登录后找教师Users→Classes→申请；教师接受后才可看班级私有集。Library班级更多菜单退出；部分移动分支经Members减号与Yes | Join不等于批准；所有者无退出项。误退可被重新加入，但未承诺恢复旧进度；拒绝/撤回与权限时点未知。E-QZMS-18、21 |
| F-M021-30 建立Study Group成员关系 | 公开集Groups→建组命名→邀请；接收者登录并接受。可加集、切组或从自己姓名菜单离组，最多8人；Track Progress分Know/Still Learning | 与F21匿名可试一次的Study with friends保留为不同文档路径，不证明版本替换。满员、拒绝、撤回、同步与退组数据未知。E-QZMS-11 |
| F-M021-31 提交、修改及查看评分 | 网站集页留评分→提交；可点已有评分更改并再次提交；作者打开集看总评分 | 不能删除已留评分、不能关闭评分，作者不收评分通知。刻度、身份、聚合与历史未知；不是正确性认证。E-QZMS-13～15 |
| F-M021-32 移动首页推荐与资料偏好（D1） | 帮助概述入门问答后的Study Bites、进度卡与课程/活动推荐；学习后出现Study Bites。另有修改/移除学校信息并保存的步骤 | 未还原问答、开始/结束活动、推荐控制及返回位置。首页为全球13岁以上、偏美国大学场景；学校设置限13国，未列中国大陆。移动设置的Manage subscription路径保留为文档原措辞，未验证。E-QZMS-19、23 |

原F04另补：编辑设置的**Editable by**可选班级、密码或自己；查看设置的**Visible to**另有密码访问和更改密码路径。Just me说明出现private措辞，但步骤没有证明两字段同时改变；旧密码失效、撤权后会话/副本及并发编辑仍未知。原F05补跨端找不到集时检查是否发布，不能据此承诺发布后的同步时延。E-QZMS-05、07、08。

E-QZMS-12的Top creator标准涉及创作量、浏览和相对活跃，不是知识正确性认证；E-QZMS-16只解释卡组内容模型。这两篇作为背景登记，不制造额外“功能”任务。

新增13条[按问题查阅的依据](../reference-points.csv)，编号RP-M021-18～30。后续原型可据此讨论：AI导入材料后怎样说明对象、权限和保存结果；家长分享儿童资料后怎样确认可用；成人推荐怎样兼顾相关性与知识边界。以上均为研究建议。原目录正文覆盖完成不代表整个Quizlet、所有平台或异常分支已研究完成，更不证明这些机制适合5–6岁儿童。
