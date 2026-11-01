# 怎样把竞品资料用于后续产品设计

更新：2026-10-27。这里是问题入口，事实以逐产品案例和证据为准。当前研究尚未覆盖每个产品的每个功能；文档步骤和静态画面也不是实机验证。任何条目都不能仅凭“支持某功能”直接成为设计决定。

## 从设计问题进入

| 想解决的问题 | 从哪里开始 | 一组可直接复查的证据 | 取用时必须核对 |
| --- | --- | --- | --- |
| 新闻从纸刊版面切到听读，退出后到底恢复哪个位置 | [The Week Junior F05/07/11/12](cases/M-048-week-junior.md) | [E-WJCD-02/04：同页关闭重开、浏览器重启和窄屏实际操作](artifacts/week-reader-recovery/browser-operation-audit.md) | 印刷页、重排栏目/段落、音频秒点、倍速和暂停意图分别核；同页iframe保留不等于持久化，重启音频结果未知；不是原生App/实体设备 |
| 儿童奖励到底记录努力、活动次数还是掌握 | [Khan F14/15](cases/M-030-khan-kids.md) | [目标表/贴纸/证书及原PDF](artifacts/khan-adult-guidance/source-notes.md) | 官方配套纸面活动、成人组织；不是App自动成就，没有学习效果验证 |
| AI安排的学习任务何时对孩子可见，修改影响谁 | [Khan F16](cases/M-030-khan-kids.md) | [分配弹窗](artifacts/khan-adult-guidance/dialog.png)、[日期分组](artifacts/khan-adult-guidance/assignments.png)与[文档边界](artifacts/khan-adult-guidance/source-notes.md) | 学生范围、日期、Save、归档与删除分别核；静态X按钮不证明取消语义，家庭版不同 |
| 没有屏幕反馈时，AI怎样确认截图、下载和定位结果 | [Kompakt F22/23/25](cases/M-012-mudita.md) | [帮助原文与日期](artifacts/mudita-remaining-controls/source-notes.md) | 截图文件、地图包、授权、GPS位置和天气显示分别核；未实测错误、恢复或时延 |
| AI展示“知识关系”时，原文、推断和视图状态怎样区分 | [Reader F33](cases/M-015-readwise.md) | [固定渲染器与合成样本](artifacts/readwise-graph-renderer/source-notes.md) | 同源边不证明语义，单条来源计数、筛选/选中/退出不同；这是示例本地运行，非Reader原生实测 |
| 语音找到笔记之后，是否真的打开了正确内容 | [X2 F25/26](cases/M-009-iflytek-office.md) | [历史手册原页与任务](artifacts/x2-remaining-tasks/source-notes.md) | 入口、候选、打开、转换、保存分别核；AIUI不等于大模型，搜索结果和恢复仍缺 |
| 导入或导出后，怎样确认AI完成了任务 | [墨案 F06/08](cases/M-054-moaan.md)，对照[X2 F27](cases/M-009-iflytek-office.md) | [接收/书架/导出路径画面](artifacts/moaan-legacy-media/source-notes.md) | 文件收到、登记、打开、完整性分开；历史型号未知，路径提示不等于文件验证 |
| AI怎样给我一批新闻，并允许结束 | [Reader F27/28](cases/M-015-readwise.md) | [E-RWSK-04/05固定工作流与冲突](artifacts/readwise-ai-workflows/source-notes.md)；[官方订阅工作流](https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/skills/feed-catchup/SKILL.md) | 取样范围、推荐理由、当前批次与写入对象；next不能默认等于全读完，未运行 |
| AI能否从阅读出题，再形成可保存的学习成果 | [Reader F29/31/32](cases/M-015-readwise.md) | [E-RWSK-06/08/09](artifacts/readwise-ai-workflows/source-notes.md)；[官方测验工作流](https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/skills/quiz/SKILL.md) | 逐题等待/评分与真实掌握分开，生成/写回/导入题库分别验证；没有登录执行 |
| 个性化会不会把我限制在旧兴趣中 | [Reader F30/34/35](cases/M-015-readwise.md) | [E-RWSK-07/11/12](artifacts/readwise-ai-workflows/source-notes.md)；[画像原文](https://github.com/readwiseio/readwise-skills/blob/2d1ce9627c611d24f510dfc2e05a123fa509d2f6/skills/build-persona/SKILL.md) | 历史缺席不是排斥，最后打开不是完整阅读事件；推断、用户纠正和持久保存分别核对 |
| 儿童订正通过和首次独立答对怎样区别 | [小猿历史F07](cases/M-056-xiaoyuan-hardware.md) | [E-XYHL-01作者文字与抽帧](artifacts/xiaoyuan-historical-learning/source-notes.md)；[原报告](https://post.smzdm.com/p/a3x3nk5d/) | 2024未知型号不套S6/E1，等级阈值/误判修正/奖励到账与同题完整操作仍缺 |
| AI批量修改学习资料会影响哪些题目 | [Anki F06/07/10](cases/M-020-anki.md) | [E-AND-05/09/10补读与E-ANCM-51](artifacts/anki-content-and-maintenance/source-notes.md)；[浏览操作原文](https://docs.ankiweb.net/manual/browsing.md) | 笔记、派生卡、模板、检索结果、选区及当前项分别核对；批量设置与逐项翻转不同，撤销结果未测 |
| 公式题库导入、修订和自有服务器同步怎样区分 | [Anki F23–25](cases/M-020-anki.md) | [E-ANCM-19/23/25](artifacts/anki-content-and-maintenance/source-notes.md)；[公式原文](https://docs.ankiweb.net/manual/math.md) / [分享原文](https://docs.ankiweb.net/manual/contrib.md) | 源文本、显示资源、判题规则、发布者版本、接收者副本和个人记录分别判断；同步不是备份，桌面生成不等于Plus本地支持 |
| 多个孩子共享书库时，怎样区分各自听到哪里 | [Audible F15–17/26](cases/M-039-audible.md)，对照[Yoto](cases/M-037-yoto.md)与[Tonie](cases/M-038-tonie.md) | [E-AUFC-02/04：儿童档案与删除](artifacts/audible-family-controls/source-notes.md)；[原来源](https://help.audible.com/s/article/listen-with-a-kids-profile?language=en_US) | 共享授权、档案、设备设置、个人位置与备份分开；多机同时听不证明进度跟随个人，案例不自动决定本项目账户制度 |
| 语音在锁屏、别的App和不同播放端能做什么 | [Audible F20/29–31](cases/M-039-audible.md) | [E-AUFC-23：Alexa前台限制](artifacts/audible-family-controls/source-notes.md)；[原来源](https://help.audible.com/s/article/listen-with-alexa?language=en_US) | Maven推荐、App内语音、Echo与全系统AI分开；唤醒成功、内容消歧、执行权限及最终结果逐项核，不外推全页面访问 |
| 学习游戏怎样允许探索、判错并引导重试 | [Duolingo Chess F19](cases/M-031-duolingo-adventures.md)、[Quizlet F14–17](cases/M-021-quizlet.md)、[Anki F01/12/17](cases/M-020-anki.md) | [E-DUCX-02：状态24–43操作审计](artifacts/duolingo-chess-interactions/browser-operation-audit.md)；[原来源](https://www.duolingo.com/chess)；[Quizlet规则及来源冲突](artifacts/quizlet-game-modes/source-notes.md) | 合法动作与题目约束、提示使用、重试与首次正确、游戏终点和学习收益分开；实际棋课仅一次匿名网页样本，其他活动仍为文档依据 |
| 让AI调整下周的学习任务 | [讯飞F19/23](cases/M-027-iflytek-learning.md) | [E-IFPR-03：计划教程时间点](artifacts/iflytek-planning-reading-deepening/source-notes.md)；[原来源](https://k12-swift.xunfeixxj.com/xxjserver/wx/h5/help/video/%E5%B0%8F%E5%AD%A6%E8%87%AA%E5%AE%9A%E4%B9%89%E5%AD%A6%E4%B9%A0%E8%AE%A1%E5%88%92.mp4) | 一本书的计划与每周计划；任务库候选、排序/增删、确定、持久保存；已完成数据与家长端同步未知 |
| 怎样把阅读或练习记录当作学习依据 | [讯飞F19–24](cases/M-027-iflytek-learning.md)、[BOOX F17](cases/M-004-boox-neoreader.md) | [E-BXRD-07：本机统计帮助摘记](artifacts/boox-reading-controls-deepening/source-notes.md)；[原来源](https://booxsupport.zendesk.com/hc/en-us/articles/10701454503572-Reading-Dashboard) | 阅读本数/百分比、提示、发音分、碎片奖励分别表示什么；本机统计是否随进度同步；避免把计时当理解 |
| 孩子听到一半问AI，之后继续故事 | [读听横向比较](comparison.md)，再看[Kindle F11–14](cases/M-001-kindle.md)、[Khan F01/06/12](cases/M-030-khan-kids.md)及Yoto/Tonie案例 | [E-KDAF-09：真人音频控件](artifacts/kindle-audio-family-deepening/source-notes.md)；[原来源](https://d1ergij2b6wmg5.cloudfront.net/kug/kindle_scribe_2024/v1/en-US/image/audibleplayer.png) | 真人音频/TTS/读屏，书籍和使用者，位置单位，暂停原因，恢复到原句还是下一任务，离线准备 |
| 孩子答错后怎样引导，而不是机械刷奖励 | [学习与反馈比较](comparison.md)、[Khan F02/04/05/13](cases/M-030-khan-kids.md)，查[全部案例](case-index.md)中的小猿、DragonBox、Britannica | [E-KHCD-10：离散图像及限制](artifacts/khan-controls-deepening/source-notes.md)；[原来源](https://blog.khanacademy.org/supporting-english-language-acquisition-with-khan-academy-kids/) | 首次答案、提示层级、重试、独立完成与示范后完成；奖励触发、退出权与恢复；5–6岁语言能力 |
| 成人考试怎样从作答走到交卷和错题复盘 | [驾考宝典F01/02/03](cases/M-033-jiakaobaodian.md)、[粉笔F04/09](cases/M-032-fenbi.md) | [E-JKCD-04：状态15–25操作审计](artifacts/jiakao-controls-deepening/browser-operation-audit.md)；[原来源](https://www.jiakaobaodian.com/mnks/exam/car-kemu1-beijing.html) | 首次回答/重选、漏答确认、可见成绩、错题移出与记录保存；匿名网页、历史作者报告与当前原生App分开 |
| 成人如何持续学习且能主动结束 | [学习动力专题](../learning-engagement.md)、[学习比较](comparison.md)中的Anki/Quizlet/背词案例 | [不背E-BBLC-02：辨识后放弃/开始拼写](artifacts/bbdc-learning-controls-deepening/source-notes.md)；[原来源](https://agirls.aotter.net/post/51654) | 新学与复习、任务终点、撤销自评、错题重现、学习收益；历史分支不能套当前版本，不把停留时长当掌握 |
| 如何做有边界、能扩展视野的个人新闻 | [个人新闻研究](../personal-news.md)、[新闻比较](comparison.md)中的Inoreader/Reader/Ground/TIME | [E-INCD-08：新闻修订与发布版本](artifacts/inoreader-controls-deepening/source-notes.md)；[原来源](https://www.inoreader.com/blog/2026/10/introducing-article-edit-history.html) | 来源选择、去重、更正、已读定义、推荐理由、视角补足和当期终点；内容价值仍需个人标准 |
| AI说“保存好了”，到底保存了什么 | [Kompakt F13/14](cases/M-012-mudita.md)、[Kindle F03/05/06/10](cases/M-001-kindle.md)、[AI与数据比较](comparison.md) | [E-MACD-07/08：录音停止、命名保存与关闭](artifacts/mudita-app-controls-deepening/source-notes.md)；[原来源1](https://support.mudita.com/en/support/solutions/articles/77000596747-how-to-record-a-voice-recording-) / [原来源2](https://support.mudita.com/en/support/solutions/articles/77000596769-how-to-listen-to-a-voice-recording) | 草稿/确认/持久写入/云副本/导出分别判定；关闭页面不一定保存，生成预览不一定已写入 |
| 刷机或重装后怎样恢复家庭数据 | [备份专题](../data-backup-sync.md)、[恢复比较](comparison.md)、[Khan F09/10/13](cases/M-030-khan-kids.md) | [E-KLCD-07/09：账号进度声明与重装步骤](artifacts/khan-library-controls-deepening/source-notes.md)；[原来源1](https://khankids.zendesk.com/hc/en-us/articles/4457349308059-Chinese-Parent-Guide-%E5%AE%B6%E9%95%BF%E6%8C%87%E5%8D%97-%E5%BC%80%E5%A7%8B%E4%BD%BF%E7%94%A8%E5%8F%AF%E6%B1%97%E5%AD%A6%E9%99%A2%E5%84%BF%E7%AB%A5%E7%89%88) / [原来源2](https://khankids.zendesk.com/hc/en-us/articles/360006761532-How-do-I-delete-an-app-and-re-download) | 档案、账号、设备、作品与学习进度；卸载与删档案；报告/截图与可恢复备份；这组证据本身不证明刷机恢复 |
| 下拉、手势和电子纸显示应怎样配合 | [系统交互比较](comparison.md)、[Kindle F17–19](cases/M-001-kindle.md)、[M7 F07](cases/M-057-meebook.md)、[高刷研究](../eink-high-refresh.md) | [E-MRCD-01：作者报告中的模式选择与实际状态](artifacts/meebook-reading-controls-deepening/source-notes.md)；[原来源](https://www.helpix.ru/opinion/202409/60517-meebook_m7.html) | 操作区域/当前应用/系统全局的作用范围；触摸延迟、帧率、残影需同型号实测；别套竞品硬件能力到Plus |
| AI收集的学习资料怎样才算导入可用 | [百词斩设备F03](cases/M-022-baicizhan-device.md)及[内容关联比较](comparison.md) | [E-BCLV-01：历史计划、同步与词卡画面](artifacts/bcz-learning-video-preview/source-notes.md)；[原教程](https://www.baicizhan.com/little_class/article/892/share_view) | 计划配置、同步、当前词本与可作答资源分别核对；型号未知，确认/完成回执未抽到，末段词卡不证明全部下载成功 |

其他产品从[案例索引](case-index.md)进入；未覆盖项查[待核清单](backlog.md)。表中入口是示例，不是缩减已知竞品范围或确定首版功能。

## 每次引用的最小单元

**设计问题 → 产品/型号/版本 → F任务 → E证据 → 支持的行为与缺口 → 本项目取舍 → 验证办法。**

F是稳定任务编号，E是稳定证据编号。同一任务补证继续修订原F；同一URL的新快照追加原E。上表可直接打开一组关键证据，其余材料按编号查案例、覆盖表和来源索引。字段含义与当前检查范围见[读取约定](data-dictionary.md)。不同版本结论冲突时保留双方；不要从旧结论摘掉其限定条件。

例如设计“AI帮我停止并保存录音”，可以引用Kompakt F14及E-MACD-07/08：文档区分停止、命名保存和关闭回放画面。它能支持“这几个状态值得分别设计”的判断；不能支持“我们照搬Done按钮就能可靠保存”，也没有证明锁屏/来电后的行为。后续原型需验证中断、取消和真实持久结果。

每个设计条目建议填写以下内容，暂不提前替本产品作决定：

- 用户目标及成人/儿童条件，关联G-001或G-002及具体需求。
- 参考F/E和适用型号、平台、语言、日期；画面是静态、连续演示还是实际操作。
- 从入口到结束的状态变化，含取消、误操作、失败和中断后恢复；未知明确写未知。
- 为什么采用、调整或放弃该做法，以及E-Ink4 Plus上的实现条件。
- 能否观察到的验收结果；例如重新打开仍到正确的故事位置，而非仅显示“已保存”。

## 怎样判断材料够不够

“看过功能目录”“读过操作帮助”“看过单帧”“看过连续过程”“实际做过”分开记录。D1是功能发现，D2是任务重建，D3要求连续视觉链，D4要求可复现操作日志。D4也仅覆盖那次操作的具体分支，不自动代表整个产品通过验证。

正式库保留短摘记、必要画面、原地址与散列；临时整页/整本资料在work。资料归档保证可以追溯，不保证来源未来不变或行为永远成立。采用前重新核对关键版本和矛盾点。

对当前研究，主要缺口是原生App/设备操作、完整错误与恢复链，以及儿童实际使用和学习效果。继续研究时优先补这些能改变设计判断的证据；不靠追加宣传链接或把同一流程拆成更多编号来表示深入。

功能目录也应持续闭合：本轮Audible原来22项仅目录文章已读正文，原编号回写覆盖状态；其中安装/退役说明仍不当完整播放器任务。某类帮助正文读完，不代表其全部控件、后台行为和平台差异都已验证。

## 一个可复查的例子

设计“AI帮孩子找一组练习”，可先查[讯飞F16](cases/M-027-iflytek-learning.md)及E-IFVC-02的[35秒摘帧](artifacts/iflytek-video-controls-deepening/video-02-035s.png)：演示有需求、候选题、勾选、取消挑题和去作答。它支持区分“查到候选”与“已进入练习”；不证明题目由模型生成，也不证明已导入另一App、离线可用或学习记录保存。

引用时把“画面可见”“教程文字声称”“本项目建议”“待验证结果”分别写出。即使同一视频前后都有题面和结果，也要核对是否换题或剪辑；抽帧不能自动升级为连续过程。具体证据限制见[该批来源摘记](artifacts/iflytek-video-controls-deepening/source-notes.md)。

另一个例子是“把下周计划改轻一点”：可查讯飞F23/E-IFPR-03的[76秒编辑页](artifacts/iflytek-planning-reading-deepening/video-03-076.00s.png)。画面能支持任务排序、增删和确定几个动作；不能证明删任务会保留答题历史、确定后已同步、冲突可撤销。后续设计若采用这些动作，应明确对象和可观察结果，并将这些缺口变成原型验证问题。

对于资料不足的产品，保留已查但不足以支持结论的记录。例如[洪恩官网视频](artifacts/ihuman-video-preview/source-notes.md)只是剪辑宣传，不能以“有视频”升级完整教学链。这可避免后来重复检索，也防止原件遗失后只剩过度概括的结论。


## 可以直接打开的证据链

| 设计问题 | 任务与直接证据 | 引用边界 |
| --- | --- | --- |
| 同题错答后怎样回到可尝试的状态 | [Chess F19](cases/M-031-duolingo-adventures.md) → E-DUCX-02的[错答画面](artifacts/duolingo-chess-interactions/browser/37-constrained-wrong-order.png) → [重试初盘](artifacts/duolingo-chess-interactions/browser/38-after-wrong.png) → [重试成功](artifacts/duolingo-chess-interactions/browser/43-retry-correct.png)，全过程见[动作审计](artifacts/duolingo-chess-interactions/browser-operation-audit.md) | 本次限步题第一步即判错并扣一心，继续后重置同一题面；不推导全部课程的心数、提示或恢复规则，也不证明适合5–6岁儿童。 |
| 孩子答错后如何再试 | [Khan F04](cases/M-030-khan-kids.md) → E-KHCD-10的[图像摘记](artifacts/khan-controls-deepening/source-notes.md) | GIF离散画面不是同题完整错误→提示→奖励链，构建未核；不要把奖励画面当掌握证据。 |
| 有声书怎样定位与接续 | [Kindle F11](cases/M-001-kindle.md) → E-KDAF-09的[静态控件摘记](artifacts/kindle-audio-family-deepening/source-notes.md) | 此入口对应真人有声书；TTS和儿童实体播放器应另查相应案例。AI插话、来电或睡眠后的精确接续未测。 |
| 考试交卷之后发生了什么 | [驾考宝典F03](cases/M-033-jiakaobaodian.md) → E-JKCD-04的[操作审计，状态15–25](artifacts/jiakao-controls-deepening/browser-operation-audit.md) | 有取消与确认交卷的实际匿名网页记录，但本次结果中心空白；不能写成成绩已显示、已保存或App已验证。 |

这些直达入口补充上方问题表，减少从E编号反复搜索。正式设计引用仍需带上适用范围和未闭合分支；失败记录也应保留，因为它能阻止把一次跳转或一条提示误认为任务成功。

## 正式设计时，一条依据怎样才够用

比较机制可用有版本范围的D2文档；决定布局需相应画面；决定手势、时延、错误恢复与持久化，则要补对应实际操作。证据深度按具体结论判断，不因同产品某处有D4就给全部功能背书。

每个拟采用的交互，留下“待解决问题 → 型号/版本 → F任务 → E原始依据 → 适用条件/缺口 → 本项目选择及理由”。最后一项等正式设计获认可后填写；现在不把助手建议写成既定需求。一个功能可以有多条来源，同一篇帮助也可以支持多个任务；新增文章或截图不重复计功能。

尚未补齐的原型关键问题要有明确观察任务。例如研究“继续上次故事”，应指定同一内容、档案与播放位置，分别核退出、断网、重启后的结果；没有这种证据时，只能引用已说明的恢复范围。资料不足时记录已查路线、失败原因和下一步，保留未知，避免以后重复从宣传页重新猜测。

## 用恢复条件查询，而不是只查“支持续播”

下面是[The Week Junior同一期网页](cases/M-048-week-junior.md)的可复用样例；先限定环境，再选能支持当前问题的证据。其他产品也应按此粒度补证，不能把本表结果外推到它们。

| 要恢复的对象与条件 | 当前证据 | 能支持的结论/尚缺什么 |
| --- | --- | --- |
| 印刷页：手动保存→回封面→同资料目录浏览器重启 | F07/E-WJCD-04，[状态14–16](artifacts/week-reader-recovery/browser-operation-audit.md) | 本次回14页；不证明音频秒点、账户或刷机恢复 |
| 听读：2:52暂停、1.5x→关窗口→同页重新打开 | F12/E-WJCD-02/04，[暂停](artifacts/week-reader-recovery/22-audio-pause-speed.png)→[重开](artifacts/week-reader-recovery/24-reopen-reflow-modal.png) | 从原位置附近自动续放且保留倍速；暂停意图没有保留；iframe未销毁，不能当持久化 |
| 听读：关闭播放器→关闭窗口→浏览器重启 | F12，同审计状态25–32 | 页码恢复，耳机入口未见，操作超时；音频位置恢复未知，不能填成功或确诊丢失 |
| 换成空上下文/窄屏 | F05/07，同审计状态33–36及旧空上下文对照 | 从封面开始；窄屏菜单可跳14，仍为整页版面；不是同用户跨设备同步测试 |
| AI插话、来电、睡眠、断网、换孩子后继续 | 尚无本案例实际记录 | 必须指定内容版本、家庭成员、暂停原因和位置单位；保留为原型验证任务 |

上述表是研究证据结构示例，不是已批准的本系统架构。正式设计时才能决定怎样保存、同步和向AI披露这些状态。
