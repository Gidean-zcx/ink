# 怎样把竞品资料用于后续产品设计

更新：2026-10-27。这里是问题入口，事实以逐产品案例和证据为准。当前研究尚未覆盖每个产品的每个功能；文档步骤和静态画面也不是实机验证。任何条目都不能仅凭“支持某功能”直接成为设计决定。

需要横向筛选细节时，打开[交互依据表](reference-points.csv)：可按AI、预览、确认、同步、取消、重启、音频等标签查前置条件、动作、可见结果和未知分支；每行直达F/E和原页/审计。具体覆盖以当前表及F/E为准；它是增量检索入口，不代表全库所有任务，也不能把未收录解释为产品不支持。字段和证据方式见[读取约定](data-dictionary.md)。

表中可直接筛task_depths、evidence_hands_on及source_types；实际操作另由operation_audit_paths直达审计，单图仍留在artifact_path。下方问题表是阅读入口，不单独评级；必须核对所引E的取得方式，不能把关联任务的D4转移给其中一条操作文档。

## 怎样判断资料已经足够深入

先列固定型号/版本的功能与帮助目录，再按实际用户任务补入口、前置条件、逐步动作、界面反馈、数据影响、结束与恢复。一个按钮的存在只能证明入口；成功主流程有依据后，还需单列错误、取消、空内容、权限、离线、中断与重装等适用分支。无资料的分支标未知，不根据常见设计补写。

后续正式设计引用时，需要能回答“哪个版本、哪个条件、动作影响什么、结果凭什么确认、失败能否恢复”，并从任务直达来源页/时间点/操作审计。宣传、帮助步骤、静态图和真实操作分别标记；奖励机制描述不证明学习有效，用户测评不替代本家庭儿童观察。每产品都继续保留覆盖缺口，直到明确范围有依据闭合，才能讨论该范围分析完成。

## 从设计问题进入

| 想解决的问题 | 从哪里开始 | 一组可直接复查的证据 | 取用时必须核对 |
| --- | --- | --- | --- |
| AI任务从语音、按键或组件启动后，在哪里继续操作 | [Shortcuts F20～23](cases/M-043-shortcuts.md) | [入口、锁屏与执行环境](artifacts/shortcuts-entry-and-organization/source-notes.md)；RP-M043-11～13/15 | 配置、出现入口和实际执行分开；文档有解锁、补输入与转主App条件，未实测时延或恢复 |
| 当前页面共享给AI的到底是什么 | [Shortcuts F03/08](cases/M-043-shortcuts.md) | [输入类型与剪贴板](artifacts/shortcuts-entry-and-organization/source-notes.md)；RP-M043-14/24 | 类型影响入口；剪贴板范围/到期不证明其他App副本被清除 |
| 提醒、打开编辑器、导入模板是否已经执行任务 | [Shortcuts F17/19/25](cases/M-043-shortcuts.md) | [Gallery、URL和提醒回访](artifacts/shortcuts-entry-and-organization/source-notes.md)；RP-M043-16/20/21 | 提醒需点按，导航与执行有不同入口；加入集合不证明业务数据已写入 |
| AI移除入口、删文件夹或复制配置，会影响哪些对象 | [Shortcuts F11/24](cases/M-043-shortcuts.md) | [组织、删除与副本](artifacts/shortcuts-entry-and-organization/source-notes.md)；RP-M043-17～19 | Remove保留All，删夹可保留内容；副本权限/自动化继承与跨端恢复未知 |
| 导入资料时，字段与时间怎样对应到目标对象 | [Shortcuts F18/26/27](cases/M-043-shortcuts.md) | [联系人映射与时间格式](artifacts/shortcuts-entry-and-organization/source-notes.md)；RP-M043-22/23 | 原示例变量有歧义；秒/毫秒、时区、显示精度及部分写入必须分别核，未执行导入 |
| 媒体已回放或图点已出现，整套学习资料是否已保存 | [Quizlet F24/25](cases/M-021-quizlet.md) | [媒体与图示](artifacts/quizlet-media-and-sharing/source-notes.md)；RP-M021-18～20 | 按平台区分添加/学习；局部保存、整集提交、替换撤销和重载恢复分别核对 |
| 内容选中、加入文件夹和打标签分别改变什么 | [Quizlet F26](cases/M-021-quizlet.md) | [材料归集](artifacts/quizlet-media-and-sharing/source-notes.md)；RP-M021-21 | 高亮不证明提交；类似子文件夹不证明嵌套，删夹/移除与混合权限未知 |
| 发出链接、设置密码或嵌入页面后，对方能做什么 | [Quizlet F04/27](cases/M-021-quizlet.md) | [分享、查看、编辑与嵌入](artifacts/quizlet-media-and-sharing/source-notes.md)；RP-M021-22/23/30 | 文档入口有别；接收端身份、撤权后旧副本、嵌入进度和真实访问结果未验证 |
| 申请、接受邀请、退出和重新加入会怎样影响进度 | [Quizlet F21/29/30](cases/M-021-quizlet.md) | [班级与Study Groups](artifacts/quizlet-media-and-sharing/source-notes.md)、[Study with friends](artifacts/quizlet-audio-and-study/source-notes.md)；RP-M021-24～26 | 两类群组路径分别引用；申请不证明批准，重加不证明旧进度恢复 |
| 搜索、评分与推荐分别能证明材料的什么价值 | [Quizlet F23/28/31/32](cases/M-021-quizlet.md) | [发现内容与评价](artifacts/quizlet-media-and-sharing/source-notes.md)、[个人集查找](artifacts/quizlet-audio-and-study/source-notes.md)；RP-M021-27～29 | 评分/称号不证明准确性；首页推荐主链D1，学校偏好保存不证明推荐已改变或可控 |
| AI会话被最小化、离开后，内容是否还在 | [Quizlet F19](cases/M-021-quizlet.md) | [对话与生成对象](artifacts/quizlet-audio-and-study/source-notes.md)；RP-M021-11 | 文档限定US14岁以上；会话规则不替代卡组保存、刷新或跨端恢复 |
| 录音能否继续上传，选错语言怎样改 | [Quizlet F02/12/18](cases/M-021-quizlet.md) | [音频输入、重建与生成边界](artifacts/quizlet-audio-and-study/source-notes.md)；RP-M021-08～10/17 | 本机上传草稿、重新生成、来源准确性和播放断点分开；没有实际录音或恢复 |
| 游戏得分和大家的学习结果能说明什么 | [Quizlet F20/21](cases/M-021-quizlet.md) | [加步奖励与群组统计](artifacts/quizlet-audio-and-study/source-notes.md)；RP-M021-12/13 | 依据产品规则而非学习效果；不能推导个人长期掌握或儿童适配 |
| AI读哪些材料、解答来源是否经过核验 | [Quizlet F01/22/23](cases/M-021-quizlet.md) | [上下文范围、解答与查找](artifacts/quizlet-audio-and-study/source-notes.md)；RP-M021-14～16 | 选中文字/符合条件标签页不等于全页面访问；题库与追加AI输出分别核对 |
| AI对话生成卡片后，怎样知道已经保存到正确位置 | [Quizlet F01](cases/M-021-quizlet.md) | [外部AI生成与编辑入口](artifacts/quizlet-material-workflows/source-notes.md)；RP-M021-01 | 只有官方文档；账号归属、可见性、持久回执和重复生成未证 |
| 扫描资料怎样成为能检查的练习 | [Quizlet F03/05](cases/M-021-quizlet.md) | [人工配对、校订及草稿/发布](artifacts/quizlet-material-workflows/source-notes.md)；RP-M021-02/06 | 不能把识别框当准确配对；拒权、取消重扫及强退恢复未知 |
| 多套材料临时学一次，与建立新学习包有什么差别 | [Quizlet F04](cases/M-021-quizlet.md) | [临时组合、新集与复制资格](artifacts/quizlet-material-workflows/source-notes.md)；RP-M021-03～05 | 组合保存、源集保留和学习统计分别核；去重/进度继承没有依据 |
| 改评分规则会影响哪些活动和旧成绩 | [Quizlet F08](cases/M-021-quizlet.md) | [UK/US条件与偏好范围](artifacts/quizlet-material-workflows/source-notes.md)；RP-M021-07 | 设置作用范围有文档，历史成绩和长期调度回算未知 |
| 打开设备文件，是否已经在电脑保存了副本 | [QUADERNO F18](cases/M-060-quaderno.md) | [临时预览、取出、打印转换](artifacts/quaderno-file-lifecycle/source-notes.md)；RP-M060-05/06 | 两代手册分开；临时文件、持久导出和传输中断后重传不是同一结果 |
| 同步、删除和恢复会怎样影响新资料与旧记录 | [QUADERNO F04/06/07/08](cases/M-060-quaderno.md) | [备份目录、时间改写及回收站范围](artifacts/quaderno-file-lifecycle/source-notes.md)；RP-M060-07～09/14 | 文档规则，未实际恢复；解除绑定、导入旧备份和永久删除分别判断 |
| AI怎样确定连接的是哪台设备、使用哪个对象 | [QUADERNO F19/20/21](cases/M-060-quaderno.md) | [设备ID、文件入口和模板第一页](artifacts/quaderno-file-lifecycle/source-notes.md)；RP-M060-10～13 | 网络状态不替代PC服务连接；没有家庭账号、全部数据合并或无损导入证明 |
| 应用包已出现、安装确认与服务可用怎样分开 | [Astell&Kern F06/07](cases/M-065-astell-kern.md) | [旧机四幅照片与兼容表日期](artifacts/ak-openapp-states/source-notes.md)；RP-M065-01～03 | SP1000M示范，不是SR35实测；取消按钮不证明取消结果，登录页不证明认证成功 |
| 说“记下来”时，AI会写到哪个应用 | [Gemini F12/13](cases/M-042-gemini.md) | [最近访问目标、指定应用与待办对象](artifacts/gemini-notes-reminders/source-notes.md)；RP-M042-01/02/04 | 官方步骤重建；各连接器动作不同，实际目标和写入回读尚未操作 |
| 生成笔记后的撤销与写入前确认是什么关系 | [Gemini F10/12/13](cases/M-042-gemini.md) | [通知Undo与Live后台例外](artifacts/gemini-notes-reminders/source-notes.md)；RP-M042-03 | 不能把操作后反馈当写前批准；撤销时限和恢复结果未知 |
| 笔迹已保存，为何仍可能失去剪贴或撤销 | [QUADERNO F12/17](cases/M-060-quaderno.md) | [Gen.2具体页与状态](artifacts/quaderno-gen2-ink/source-notes.md)；RP-M060-01～04 | 明确代际；灰度显示不等于没有颜色数据，校准描线也不等于普通书写；未实操 |
| 点确认之前是否已经修改数据 | [AntennaPod F14/13](cases/M-040-antennapod.md) | [筛选即时写入、标签临时编辑](artifacts/antennapod-state-controls/source-notes.md)；RP-M040-01/02/03 | 固定源码，未运行；相似按钮的提交时点/对象不同，关闭不自动等于撤回 |
| 批量整理是否覆盖每项的独有信息 | [AntennaPod F13](cases/M-040-antennapod.md) | [共同标签算法与取消](artifacts/antennapod-state-controls/source-notes.md)；RP-M040-03 | 每节目分别提交，未证明整批事务、部分失败恢复或同步 |
| 清队列、删音频和处理数量分别说明什么 | [AntennaPod F03/05](cases/M-040-antennapod.md) | [警告、数据库与文件边界](artifacts/antennapod-state-controls/source-notes.md)；RP-M040-04～07 | 本地原件与下载不同；请求数/UI事件不能证明持久保存或删净 |
| “全部内容”和零结果的范围怎样解释 | [AntennaPod F14](cases/M-040-antennapod.md) | [空条件与收藏范围](artifacts/antennapod-state-controls/source-notes.md)；RP-M040-08 | 筛选作用于来源状态及字段，未实际查询，不代表物理文件可用 |
| 同一个学习键何时揭示答案、何时提交评价 | [MM-WM01 F02/03](cases/M-023-maimemo-device.md) | [回忆09](artifacts/maimemo-device-controls/image-09.png)、[答案10](artifacts/maimemo-device-controls/image-10.png)、[详情21](artifacts/maimemo-device-controls/image-21.png)；RP-M023-01/03 | 不同阶段的静态示意，非连续过程；真实写入、重复按键与撤销未测 |
| 长按熟知或收藏后，怎样取消和恢复 | [MM-WM01 F03](cases/M-023-maimemo-device.md) | [熟知确认23](artifacts/maimemo-device-controls/image-23.png)及[逐图摘记](artifacts/maimemo-device-controls/source-notes.md)；RP-M023-02 | 有弹窗示意不证明每次出现、取消无写入或撤销能恢复原排期 |
| “只回顾、不计反馈”是否表示所有动作都只读 | [MM-WM01 F02/04](cases/M-023-maimemo-device.md) | [回顾19](artifacts/maimemo-device-controls/image-19.png)与[正文/状态说明](artifacts/maimemo-device-controls/source-notes.md)；RP-M023-08 | 回顾页仍可长按标记；模式声明与具体动作副作用分开 |
| 调学习量、听词定时和查看统计能否直接照文档实现 | [MM-WM01 F02/04/07](cases/M-023-maimemo-device.md) | [总览11](artifacts/maimemo-device-controls/image-11.png)、[听词14](artifacts/maimemo-device-controls/image-14.png)、[统计16](artifacts/maimemo-device-controls/image-16.png)；RP-M023-04/05/07 | 文图方向冲突、定时单位缺失、图表范围未明；统计任务仍D1 |
| 儿童答错后何时重试，输入错误和知识错误怎样分开 | [Math F20](cases/M-031-duolingo-adventures.md) | [不完整输入](artifacts/duolingo-math-interactions/browser/36-equation-feedback.png)→[课末回顾](artifacts/duolingo-math-interactions/browser/64-next-task.png)→[操作审计](artifacts/duolingo-math-interactions/browser-operation-audit.md)；RP-M031-04/09 | 第二段两错在课末重做；不套Chess立即重试；漏输入原因未定，不当儿童效果证据 |
| AI得到计算结果，是否已经替用户作答 | [Math F20](cases/M-031-duolingo-adventures.md) | [计算器与待答题](artifacts/duolingo-math-interactions/browser/20-calculator-closed.png)；RP-M031-01 | 求助结果、待提交输入、判分分别核；计算器不是已验证Math Coach |
| 学完后刷新保留结果，能否当作备份 | [Math F18/20](cases/M-031-duolingo-adventures.md) | [刷新学习页](artifacts/duolingo-math-interactions/browser/77-learning-page-ready.png)与[状态72–77](artifacts/duolingo-math-interactions/browser-operation-audit.md)；RP-M031-10/11 | 页面仍提示建档保存；仅同临时上下文，未测试进程重启/换设备/刷机恢复 |
| 读了多少、是否读懂与归档，怎样分别判断 | [Inoreader F03/16](cases/M-044-inoreader.md) | [阅读进度图](artifacts/inoreader-reading-state/active-reading-image-09.png)、[批量归档图](artifacts/inoreader-reading-state/active-reading-image-10.png)；RP-M044-01/02 | 预计时长不是实际投入，百分比不是掌握；选中含0%条目不证明归档结果 |
| AI保存网页后，怎样核对真正完成 | [Inoreader F03](cases/M-044-inoreader.md) | [预览/保存分离](artifacts/inoreader-reading-state/active-reading-image-11.png)；RP-M044-03 | 缩略图与标题不是完整正文或已入库；失败、重复与撤销未知 |
| 新闻被过滤或退订后，能否找回 | [Inoreader F01/17](cases/M-044-inoreader.md) | [日志路径、历史范围和退订冲突](artifacts/inoreader-reading-state/source-notes.md)；RP-M044-04/05/10 | 回看不等于恢复；取消邮件源的删除/90天保留口径未统一，不能承诺 |
| 收藏和开启离线，是否已经能在断网时阅读 | [Inoreader F05](cases/M-044-inoreader.md)、[Ground F02](cases/M-046-ground-news.md) | [单篇离线标识](artifacts/inoreader-reading-state/source-notes.md)、[联网限制](artifacts/ground-source-scope/source-notes.md)；RP-M044-08、RP-M046-01 | 分开意图、下载完成和账户权益；没有实际断网、冲突或重装验证 |
| AI发出同一个手势前，要先读取什么状态 | [墨墨F01](cases/M-017-maimemo.md) | [鸿蒙手势规则](artifacts/maimemo-input-and-sync/source-notes.md)；RP-M017-01/02 | 答案前后同一下滑分别是揭晓/认识；文档规则，不是实操；误评撤销仍未知 |
| AI识别图片后，怎样判断真正加入了多少词 | [墨墨F10](cases/M-017-maimemo.md) | [识词双栏与两种计数](artifacts/maimemo-input-and-sync/harmony-04.jpg)；RP-M017-03 | 分类里的已选和本次勾选不同；不能把图中40当本次成功导入量，提交仍未观察 |
| 已打开同步，为什么另一端还没更新 | [墨墨F04](cases/M-017-maimemo.md) | [后台触发、等待及接续条件](artifacts/maimemo-input-and-sync/source-notes.md)；RP-M017-06/07 | 开关、触发、上传完成和目标接收分开；答案阶段/并行冲突未知，不当整机备份 |
| AI从话语或笔记生成日程，在哪一步形成真正事件 | [AiPaper F14](cases/M-007-viwoods.md) | [语音→字段→确认示意p42](artifacts/viwoods-chinese-workflows/cn-42.png)、[圈选p43](artifacts/viwoods-chinese-workflows/cn-43.png)；RP-M007-01/02 | 历史手册静态图；可编辑标题/起止/备注与确认分开。真实保存/同步/取消未知，日历事件不等于手写待办 |
| AI写完一封邮件后，究竟已做了什么 | [AiPaper F22](cases/M-007-viwoods.md) | [生成预览→复制回编辑器p66](artifacts/viwoods-chinese-workflows/cn-66.png)、[详细边界](artifacts/viwoods-chinese-workflows/source-notes.md)；RP-M007-03/04 | 发送仍独立；收件地址在生成前已有，不能说AI选了收件人；草稿/投递结果未操作 |
| 设置自动同步后，是否已经有可恢复备份 | [AiPaper F10/16/17](cases/M-007-viwoods.md) | [第三方云周期p60](artifacts/viwoods-chinese-workflows/cn-60.png)、[A到B手工传递p71](artifacts/viwoods-chinese-workflows/cn-71.png)；RP-M007-05/07 | 周期设置、人工接收、双向合并与历史备份分别核；不能给V3.15原生云或刷机恢复补证 |
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

RP是交互依据表中的稳定定位编号，不是新增功能或来源。例如筛“邮件”，RP-M007-03支持“独立预览”，RP-M007-04支持“复制回编辑器后另有发送”；两条共用F22和同一p66。引用时仍须保留“历史手册示意、没有发送实测”。筛“重启”则能找到实际网页的成功与未取得结果，而非只得到“支持续播”。

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

## 同一篇内容，保存和恢复了哪个位置

[TIME整期样刊案例](cases/M-047-time-for-kids.md)增加RP-M047-01～07，可直接比较四种不同结果：

| 问题 | 可复查依据 | 引用限制 |
| --- | --- | --- |
| 最后浏览页是否覆盖显式书签 | [封面后重启回第4页](artifacts/time-sample-reader/browser/22-browser-restart-base-entry.png)，[完整动作](artifacts/time-sample-reader/browser-operation-audit.md) | 同资料目录浏览器恢复，不是账户/服务器备份 |
| 暂停后关闭再打开是否仍暂停 | [同页重开0:28/1.5x](artifacts/time-sample-reader/browser/15-audio-reopened.png) | 自动继续，未保留暂停意图；没有实际听音 |
| 刷新能否继续同一秒点 | [刷新前0:53](artifacts/time-sample-reader/browser/16-paused-before-reload.png)→[刷新后0:01/1x](artifacts/time-sample-reader/browser/17-listen-after-reload.png) | 本次界面重建回起始；不外推其他播放器 |
| 读后怎样得到可解释的产出 | [同刊部位图/填空](artifacts/time-sample-reader/teacher-p9.png)与[教师流程](artifacts/time-sample-reader/teacher-p8.png) | 文档中的成人组织任务，未观察自动评分或儿童效果 |

同篇文章纸刊第4页在重排目录是第3项，说明引用位置时需注明单位。正式设计仍先确定要恢复的内容、成员、状态与条件，再引用相符证据；本轮不据此确定接口、数据模型或账户制度。

## 系统与新闻：从一个词追问到可验证的对象

第44批增加RP-M004-01～10、RP-M010-01～02，可在[交互依据表](reference-points.csv)按下列问题筛选。资料来自通用帮助与静态图，目标机型/实际固件未验证；表中的“结果”包括文档所述结果，须同时读取evidence_basis。

| 要讨论的问题 | 直接依据 | 仍不能推出什么 |
| --- | --- | --- |
| AI说“已静音”改变了谁 | [四音量渠道](artifacts/boox-system-content-deepening/volume-channels.jpg)，RP-M004-01 | 不能推媒体/闹钟也关闭，或Call滑条证明有蜂窝通话 |
| 关屏后任务为何停止 | RP-M004-02/03，[逐项条件](artifacts/boox-system-content-deepening/source-notes.md) | 冻结、自启动、无线连接、音频和自动关机不是同一开关 |
| 看见屏幕是否就能操作 | [BOOXDrop限制](artifacts/boox-system-content-deepening/booxdrop-cast-conditions.jpg)，RP-M004-06 | 不证明反向控制，更不证明结构化数据和跨app动作 |
| “已导入新闻”是否能离线读 | RP-M004-07、RP-M010-01 | OPML订阅、首次联网加载、正文/附件缓存和阅读进度分别核对 |
| 这条阅读记录能证明学过吗 | RP-M004-08 | 最后阅读日期不证明读完或掌握 |
| 账号切换是否就是家庭档案 | RP-M010-02 | 保留账号信息并重新登录，不证明隔离、同步或恢复 |

设计引用仍采用“需求/问题 → 具体F任务 → E来源 → 画面/段落 → 未证分支 → 本项目待验证事项”。一个商品有某功能的宣传，只能进入候选清单；有操作文档可比较路径；需要触感、错误或恢复结论时必须取得相应行为证据。

## 查询内容是否真正准备好（第45批）

“找到了书”“购买成功”“已有文件”“能离线打开”要用不同依据。新增[PocketBook任务与缺口](cases/M-003-pocketbook.md)及RP-M003-01～10，可从以下问题进入：

| 问题 | 直接定位 | 证据边界 |
| --- | --- | --- |
| 换字号后页数变化是否表示进度增加 | [57页计页说明及控件](artifacts/pocketbook-reading-media/manual-p57.png)，RP03 | 文档区分原书页与屏数；未实际重排 |
| 关播放器会不会继续播放 | [31页Exit/Collapse](artifacts/pocketbook-reading-media/manual-p31.png)，RP01 | 仅控件，后台和断点仍需实测 |
| 已购或拿到ACSM是否已能读 | [22–23、59–61页摘记](artifacts/pocketbook-reading-media/source-notes.md)，RP05/06 | 文档还要求下载或授权；未购买/复制 |
| 同步是否覆盖所有数据、能否恢复 | [Dropbox目录及自动条件](artifacts/pocketbook-reading-media/source-notes.md)，RP07 | 仅指定文件目录，删除冲突/恢复未知 |
| 边缘手势和开机图是否总有同一效果 | [映射82页](artifacts/pocketbook-reading-media/manual-p82.png)与[Logo80页](artifacts/pocketbook-reading-media/manual-p80.png)，RP08–10 | 当前配置/自动模式/启动场景都有限定，普通睡眠不外推 |

正式引用应携带这些条件与缺口。RP是增量检索层，未覆盖的任务仍查产品目录与案例；不能将已建立78条检索依据称为全库逐页面状态已完成。

## 查询控制是否在当前条件下生效（第46批）

新增[Sony任务](cases/M-036-sony.md)和RP-M036-01～10：

| 要查的问题 | 直接依据 | 限制 |
| --- | --- | --- |
| 耳机切换后音量限制还有效吗 | [57页输出例外](artifacts/sony-system-audio/manual-p57.png)，RP01/02 | 这是规则文字，没有实际测音量；旋钮示例值不是分贝 |
| 锁按键是否也保护触屏/数据 | [15/55页摘记](artifacts/sony-system-audio/source-notes.md)，RP03/09 | HOLD、屏灭与认证分开；忘码格式化不是无损恢复 |
| 点主页封面与播放键是否同一动作 | [27页小组件](artifacts/sony-system-audio/manual-p27.png)，RP05 | 文档分别打开和控制，尚未实操 |
| 下载完为什么还不在曲库 | [113页路径图](artifacts/sony-system-audio/manual-p113.png)，RP06 | 文件、扫描范围和索引分开，图中sdcard不凭名称当物理卡 |
| 状态图标和设置开关说明什么 | [53页状态表](artifacts/sony-system-audio/manual-p53.png)及[电源摘记](artifacts/sony-system-audio/source-notes.md)，RP07/08 | Care图标不等于正在充电；Off不排除文档的72小时规则 |

更新前文件留存与完整数据恢复另看RP10。正式引用带上输出/应用/版本及未知分支；88条RP仍是增量依据，不是全库全部状态已结构化。

## AI拿到什么、执行到哪里、怎样返回（第47批）

[Shortcuts案例](cases/M-043-shortcuts.md)新增RP-M043-01～10，直接查[正文摘记与逐篇获取审计](artifacts/shortcuts-data-flow/source-notes.md)：

| 设计问题 | RP及证据定位 | 不能省略的限制 |
| --- | --- | --- |
| 没找到输入时要问什么 | RP01/02，E-SCDF-01/02/15 | 没有入口、空输入、暂停补参数不同；取消和前序产物未测 |
| 显示给用户的名称是不是交给AI的对象 | RP03/04，E-SCDF-05/09～11/26 | 字典可显示键而传值，文本拼接可能改变类型 |
| 一次选项会执行哪些动作 | RP05，E-SCDF-12/13 | 菜单只走一支；条件结果来自所走路径最后有输出的动作 |
| 处理十条是否真的导入了十条 | RP06/09，E-SCDF-14/22～25/27/28 | 次数不等于不同内容数量；部分失败、重复与业务回读未知 |
| 看见网页是否能操作它 | RP07，E-SCDF-16～18 | 活动网页上下文、许可和完成回调各自成立；不覆盖全部原生App |
| 取消、错误、没有输出分别说明什么 | RP08/10，E-SCDF-04/06/07/19～21；第55批补E-SCEN-03 | 回调/预览不是业务保存证明；两篇27帮助对编辑器外预览范围有差异，见[补证](artifacts/shortcuts-entry-and-organization/source-notes.md) |

这些依据属于官方文字重建；当前无快捷指令界面观察/实际执行，不能据此确定动画、时延或恢复。数据表中的evidence_basis与persistence_result必须一起读取。

## 墨水屏 UI 与刷新

跨场景设计先读[墨水屏 UI 专题](/home/workspace/reader/docs/research/eink-ui-interaction.md)：包含下拉/列表/输入/AI/听书/学习的候选交互，Plus 固定源码依据，以及可见、可读、完成的验证区别。其建议未升级为需求，也不增加案例的实机覆盖等级。
