# reader

基于 [CloudZao E-Ink4 Plus](https://github.com/CloudZao/Metalio-E-INK4-Plus)，探索供本人和子女使用的 AI 原生个人设备：阅读、学习、听书、日常工具，以及具备相应硬件条件时的短信和通话。暂称“家庭 AI 手机”。

项目建立于 2026-10-26，位于 /home/workspace/reader。

## 两个核心目标

用户已明确：把原来刷消遣 app 的时间转成高价值信息获取与知识、能力增长；同时提供系统级原生 AI first，使所有系统/app 行为、数据以及任意场景和页面都能被 AI 操作或获取。

两个目标记录为 [G-001、G-002](/home/workspace/reader/docs/requirements.md)，用于评估全部功能。当前增加成人/儿童学习动力、游戏互动、个人新闻及儿童新闻/百科研究；具体玩法与新闻形态保持候选。

已知儿童使用者为 5–6 岁，识字能力可以、可独立阅读，用户希望提供听书。儿童原型据此探索自主读、纯听和读听切换，具体交互仍待讨论。

## 当前阶段

收集零散想法、整理产品原型、研究关键架构与可行性。用户于 2026-10-26 将硬件进一步限定为 **只考虑 E-Ink4 Plus**；软件产品和应用架构重新构思，吸收原理及可复用驱动。

具体板卡版本及模组装配、首版功能、预算与续航指标尚未确定。新增讨论包括服务器备份同步、app 重装后的数据恢复、账户是否必要。竞品研究已按用户要求扩大到整个相关市场：阅读器、墨水屏平板/手机、学习与词典、单词卡、HiFi 和故事机等；社区固件仅是其中一类参考。基本方向须经用户认可，才进入全系统设计；当前没有开始固件开发、PCB 设计或采购。

## 项目资料

后续设计优先从[按问题查竞品依据](docs/research/interaction-library/design-reference-guide.md)进入，再查具体任务、证据与缺口；不必按时间顺序读完简报。

- [第三十九期：AI预览与数据传递](/home/workspace/reader/docs/briefs/039-ai-preview-and-transfer.md)：日程字段确认、邮件预览/回编辑器/发送、第三方云周期与多设备接收；新增可筛选的交互依据表，仍明确部分覆盖。
- [第三十八期：读听切换与恢复](/home/workspace/reader/docs/briefs/038-reading-audio-and-recovery.md)：公开样刊的缩放、听读、关闭、重启与窄屏实际操作；按恢复条件反查证据，保留未完成分支。
- [第三十七期：成人引导与系统结果](/home/workspace/reader/docs/briefs/037-adult-guidance-and-system-controls.md)：区分纸面激励、教师任务和App反馈；补Kompakt闹钟/地图/截图等操作与结果边界。
- [第三十六期：控件与状态依据](/home/workspace/reader/docs/briefs/036-controls-and-state-evidence.md)：Readwise官方渲染器合成样本、X2剩余手册任务及墨案历史控件；区分按钮、状态和完成证据。
- [第三十五期：AI工作流与阅读证据](/home/workspace/reader/docs/briefs/035-ai-workflows-and-reading-evidence.md)：外部AI逐篇筛选、订阅批次、测验与画像；小猿历史订正/听书链，保留版本和运行证据缺口。
- [第三十四期：内容对象与学习材料](/home/workspace/reader/docs/briefs/034-content-objects-and-study-materials.md)：Anki批量编辑的影响范围、公式准备、题库修订分发与自托管同步；补读原目录剩余30页。
- [第三十三期：家庭听书与语音控制](/home/workspace/reader/docs/briefs/033-family-audio-and-voice-controls.md)：儿童档案与位置、前台语音范围、RSS导入与绑卡、取盘后的后台下载。
- [第三十二期：学习游戏与完成结果](/home/workspace/reader/docs/briefs/032-learning-games-and-completion.md)：Chess实际错答/提示/重试/结算及刷新，Quizlet活动规则，Anki结束与重建。
- [第三十一期：同步、可用状态与AI产物](/home/workspace/reader/docs/briefs/031-sync-readiness-and-ai-results.md)：转写连接/纪要控件、历史手机计划到设备词卡，及未观察到的确认、保存和恢复。
- [第三十期：学习阶段与设备状态](/home/workspace/reader/docs/briefs/030-learning-stages-and-device-state.md)：历史辨识/拼写选择、拍题宣传片的范围、设置与实际状态、歌词条件，并完善证据直达与字段说明。
- [第二十九期：操作障碍与执行结果](/home/workspace/reader/docs/briefs/029-gameplay-errors-and-action-results.md)：儿童放置受挫、历史漏答确认、源码请求与完成的区别，并补直达证据链。
- [第二十八期：阅读计划与交互依据](/home/workspace/reader/docs/briefs/028-reading-plans-and-interaction-evidence.md)：任务计划的修改范围、阅读统计、提示与报告，以及宣传视频仍不足以支持的结论。
- [第二十七期：学习状态与证据复用](/home/workspace/reader/docs/briefs/027-learning-states-and-evidence-reuse.md)：四条学习机教程的具体画面、笔记撤销范围、档案与清除对象。
- [第二十六期：声音、档案与研究复用](/home/workspace/reader/docs/briefs/026-audio-controls-and-research-reuse.md)：音频控制、AI预览/保存、家庭档案与恢复，附按设计问题检索入口。
- [第二十五期：答完、答对与保存](/home/workspace/reader/docs/briefs/025-practice-feedback-and-recovery.md)：逐题型纠错/结束、手写编辑/恢复、资料副本与备份的范围。
- [第二十四期：控件范围与交互证据](/home/workspace/reader/docs/briefs/024-controls-scope-and-evidence.md)：推荐控制、儿童时间管理、当前App显示与保存对象，及单词机型号排除。
- [第二十三期：学完、保存与恢复](/home/workspace/reader/docs/briefs/023-learning-completion-and-records.md)：学习完成、记录上传、日历补卡与书库登记的区别，及阅读设置和宣传图的核查。
- [第二十二期：AI能做什么，退出后留下什么](/home/workspace/reader/docs/briefs/022-ai-actions-and-resume-conditions.md)：AI通信/语音/接入的作用范围，阅读设备睡眠与恢复的条件。
- [第二十一期：读完、少推和清除](/home/workspace/reader/docs/briefs/021-reading-feedback-and-data-reset.md)：历史阅读终点、推荐控制与数据重置的不同作用，型号按键与恢复缺口。
- [第二十期：儿童纠错、任务完成与可恢复状态](/home/workspace/reader/docs/briefs/020-child-feedback-and-resumable-state.md)：儿童实际错答与返回、AI引用定位、设备后台与恢复的证据边界。
- [第十九期：阅读队列、内容版本与 AI 动作](/home/workspace/reader/docs/briefs/019-reading-queues-and-ai-actions.md)：阅读节奏、正文更新与批注、分享范围和 AI 工具的证据边界。
- [第十八期：关闭页面之后，内容和状态发生了什么](/home/workspace/reader/docs/briefs/018-reading-controls-and-source-completeness.md)：词卡实际开关、媒体状态、题库材料完整性与设置生效条件。
- [第十七期：动作作用于哪里，数据留在哪里](/home/workspace/reader/docs/briefs/017-action-targets-and-lifecycle.md)：设备与配套App的控制范围、定时停止、应用生命周期和账号可见性。
- [第十六期：可见、可改与可恢复](/home/workspace/reader/docs/briefs/016-visibility-editability-and-recovery.md)：临时副本、笔迹与显示、删除路径、跨端连接及恢复条件。
- [第十五期：自由探索、撤销与自动停止](/home/workspace/reader/docs/briefs/015-exploration-undo-and-stopping.md)：儿童构数与教材提问、误评撤销与媒体恢复、自动推进及计时条件。
- [第十四期：新闻控制、学习记录与离线故事](/home/workspace/reader/docs/briefs/014-news-learning-offline-evidence.md)：新闻选择与评级、学习完成与打卡、离线故事与家庭绑定，附资料深度和复用方法。
- [第十三期：家庭内容、练习录音与音频角色](/home/workspace/reader/docs/briefs/013-household-recording-and-audio-roles.md)：家庭进度、临时/保存录音、音频方向及公开选馆器实际分支。
- [第十二期：副本、索引与恢复状态](/home/workspace/reader/docs/briefs/012-data-objects-and-state-boundaries.md)：reMarkable跨端副本、欧路词库索引、Sony播放范围与介质恢复。
- [第十一期：学习动作与恢复粒度](/home/workspace/reader/docs/briefs/011-learning-actions-and-recovery.md)：Duolingo匿名实际作答、墨墨导入/PDF、Tonie游戏恢复与家庭对象权限。
- [第十期：任务证据与设计复用](/home/workspace/reader/docs/briefs/010-task-evidence-and-reuse.md)：新闻更正与规则试跑、学习恢复、AI试题校订和离线读听，附设计问题到任务和证据的检索方式。
- [第九期：条件与交互证据](/home/workspace/reader/docs/briefs/009-conditions-and-interaction-evidence.md)：单词准备的多个阶段、儿童百科活动、新闻播放器与静态界面的证据边界。
- [第八期：操作状态与研究复用](/home/workspace/reader/docs/briefs/008-state-and-research-reuse.md)：题库更新、新闻偏好、读听与笔记出口，续补儿童学习、家庭数据恢复与版本冲突。
- [第七期：竞品交互证据与复用](/home/workspace/reader/docs/briefs/007-interaction-findings.md)：介绍续播、恢复、手势、学习与 AI 数据口径的具体差别，明确仍未完成的深度。
- [竞品交互资料库](/home/workspace/reader/docs/research/interaction-library/README.md)：65 组已知目录，含逐产品案例、任务覆盖、来源与静态摘页、按设计问题检索；当前数量统一见 [案例索引](/home/workspace/reader/docs/research/interaction-library/case-index.md)。已新增公开样刊网页的有限实际操作记录；尚无手机或电子纸实机操作。
- [第六期：竞品交互研究深度](/home/workspace/reader/docs/briefs/006-interaction-research-depth.md)：历史批次；纠正此前偏功能盘点的问题，建立深度标准与首批案例。
- [第五期：学习、个人新闻与系统 AI](/home/workspace/reader/docs/briefs/005-learning-news-ai-first.md)：两个核心目标、成人/儿童学习体验、个人报纸候选、全场景 AI 覆盖。
- [第四期：跨品类竞品与产品启发](/home/workspace/reader/docs/briefs/004-cross-category-competitors.md)：13 类、43 组产品/服务，新增查词学习、精听、音频和专注体验。
- [第三期：从固件仓库吸收功能](/home/workspace/reader/docs/briefs/003-firmware-feature-inspiration.md)：39 项盘点、AI 记录与内容流转候选、不同安装机制的边界。
- [第二期：Plus 通信与数据恢复](/home/workspace/reader/docs/briefs/002-plus-connectivity-and-data.md)：硬件边界、布丁固件线索、备份与账户建议。
- [第一期产品与技术简报](/home/workspace/reader/docs/briefs/001-family-ai-phone.md)：整体方向、场景原型；通信判断已随本轮证据修正。
- [想法记录](/home/workspace/reader/docs/ideas.md)：保存原始想法及逐条讨论结果。
- [需求与边界](/home/workspace/reader/docs/requirements.md)：区分已经明确的要求和仍待了解的信息。
- [CloudZao 硬件与源码](/home/workspace/reader/docs/research/cloudzao.md)：板卡差异、固定提交的驱动抽查、通信能力边界。
- [Plus 蜂窝通信与社区固件](/home/workspace/reader/docs/research/eink4-plus-cellular.md)：NT26、短信/通话条件、原理图证据缺口与固件目录。
- [数据备份、同步与账户](/home/workspace/reader/docs/research/data-backup-sync.md)：系统统一备份、整机/按 app 恢复、卸载和刷机的数据边界。
- [墨水屏高刷调研](/home/workspace/reader/docs/research/eink-high-refresh.md)：PaperBoyS3、60Hz 的含义、迁移条件与估算。
- [AI 原生与应用平台调研](/home/workspace/reader/docs/research/ai-native-platform.md)：系统能力、动态应用、渐进式披露、语音和内容导入。
- [竞品研究总览](/home/workspace/reader/docs/research/competitor-landscape.md)：跨品类研究入口与早期场景记录。
- [跨品类竞品矩阵](/home/workspace/reader/docs/research/market-competitor-matrix.md)：43 组产品/服务的功能依据、可借鉴点和依赖；待补品牌另列。
- [功能机会与完整场景](/home/workspace/reader/docs/research/cross-category-opportunities.md)：新增 16 项候选及八种场景比较，产品价值与 Plus 实现条件分别讨论。
- [成人与儿童学习动力研究](/home/workspace/reader/docs/research/learning-engagement.md)：研究依据、游戏策划、字节系公开机制、连续学习与能力成就；C-025～C-028。
- [个人新闻与儿童发现研究](/home/workspace/reader/docs/research/personal-news.md)：编辑简报、订阅与来源对照、可解释的价值标准、新闻与百科；C-029～C-032。
- [固件功能分类与候选池](/home/workspace/reader/docs/research/firmware-feature-landscape.md)：官方/社区功能、运行位置、来源及可借鉴点；建议与需求分开。
- [协作约定](/home/workspace/reader/AGENTS.md)：让后续讨论延续当前阶段和协作方式。

## 如何推进

直接用自然语言提出一个想法即可，无需先整理完整需求文档。每次讨论围绕当前想法展开：

1. 先说明可行性判断与成立条件。
2. 解释主要限制、实现代价，以及可能更合适的替代办法。
3. 标明哪些是资料支持的事实，哪些只是推断，哪些需要实际验证。
4. 必要时提出一个最小验证办法，并将有价值的结论记入资料。

只追问当前判断所必需的信息；具体功能在用户表达后再进入需求清单。阶段随用户后续意图调整。

## 初步方向

项目起点是个人单片机阅读器；现已扩展为家庭 AI 原生设备，阅读仍是重要场景。当前总体判断为“有条件可行”，不意味着全部场景已经能在同一款板卡上实现。

显示和通信均围绕 E-Ink4 Plus 研究。其源码已有直接 NT26 接入路径；完整短信、通话音频及待机接收仍需对应具体模组和接线。其他型号只保留历史比较或原理参考。硬件事实、上游描述、代码抽查、估算和助手建议在对应文档中分别标注；目前没有本项目实机测试结果。
