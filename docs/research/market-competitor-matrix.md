# 跨品类竞品矩阵：阅读、学习、记录、音频与 AI 系统

查阅日期：2026-10-26。对应 I-016、R-004。本表扩大产品参考范围；实施硬件仍限定 E-Ink4 Plus。研究阶段不要求竞品开源，也不要求与 Plus 使用相同芯片、屏幕或操作系统。

深度纠正：本表属于早期功能发现，未逐一分析全部功能交互。用户进一步要求深入后，新增 [交互资料库](/home/workspace/reader/docs/research/interaction-library/README.md)，将后续专题合并为 52 组目录，并从选定任务开始补流程、状态、异常和来源；这里保留原 43 组编号和历史判断。

## 研究口径

用户要求把市场上的阅读器、墨水屏平板/手机，以及百词斩、单词卡、电子词典、HiFi 播放器等都纳入竞品。这里按 **13 类、43 组产品/服务**建立第一轮矩阵，同品牌不同产品形态可以分别列入。它是功能与体验覆盖，不是“已穷尽所有在售型号”的声明；待补品牌和资料缺口单列在文末，后续继续维护。

以下“功能事实”依据厂商、开发者或其官方帮助文档；应用商店页使用开发者提供的描述，上市公司披露使用发行人原文。少数仅取得官方搜索索引或目录的条目明确标注。全部查阅日期同上，均未实机评测；描述有某功能不代表效果已被独立验证，也不等于本项目可直接调用该产品的内容/API。

不把厂商的护眼、学习提分、识别准确率、无延迟或无残影宣传转写成已证实效果。旧型号/旧手册仍可贡献设计参考，但不据此认定正在销售或支持最新功能。下表最后两列是助手分析，不是新增需求。

## 1. 专用阅读器

| 编号与对象 | 官方资料支持的功能 | 值得吸收的体验 | 依赖与边界 |
| --- | --- | --- | --- |
| M-001 Kindle / Scribe 阅读系统 | 查词、生词本、Word Wise、批注、内容集合、文档传入；Scribe 另有笔记。[用户指南](https://d1ergij2b6wmg5.cloudfront.net/kug/kindle_scribe/v1/en-US/html/kug.html) | 阅读中的陌生词自动进入可回顾的学习材料；查完立即回原文。 | 词语提示受书籍/语言支持限制；Scribe 手写不是所有 Kindle 或 Plus 都有的能力。 |
| M-002 Kobo Libra Colour | 阅读批注、笔记、云盘传书、指定地区图书馆借阅。[官方介绍](https://www.kobo.com/news/now-in-colour-bring-your-books-to-life-with-the-new-kobo-libra-colour-and-kobo-clara-colour-ereaders)、[云盘说明](https://help.kobo.com/hc/en-us/articles/15335985512983-Add-books-to-your-eReader-using-Google-Drive) | 书店之外还有自己的文件、借阅内容；来源统一呈现、各自保留访问条件。 | Google Drive 支持限定型号；图书馆、DRM 与账户规则不同。云盘导入不等于全设备备份。 |
| M-003 PocketBook / InkPad Color 3 | 官方目录列多种文件格式、听书/TTS，以及 Cloud、Dropbox、Send-to-PocketBook。[产品目录](https://products.pocketbook.ch/) | 买书、传书、读书、听书形成连续流程；内容不局限于一种传输方式。 | 各型号不同；格式支持与 TTS 语言必须按型号核对，不将全系列能力合并为单机能力。 |

## 2. 墨水屏笔记本与平板

| 编号与对象 | 官方资料支持的功能 | 值得吸收的体验 | 依赖与边界 |
| --- | --- | --- | --- |
| M-004 BOOX NeoReader / Note 系列 | 文档内分屏、关联笔记、翻译和部分型号 AI 阅读入口。[帮助文档](https://booxsupport.zendesk.com/hc/en-us/articles/8569296893844-Split-View) | 原文、解释、笔记之间保留关联；资料理解与记录不必反复退出。 | 官方明确非所有型号支持分屏/手写/AI；Plus 小屏更适合研究单页切换和上下文卡片，布局不能直接照搬。 |
| M-005 reMarkable Paper Pro | 文件夹/标签/搜索、笔记模板、手写转文字、多端同步及云盘导入。[功能页](https://remarkable.com/products/remarkable-paper/pro/details/features)、[转换条件](https://support.remarkable.com/articles/Knowledge/Convert-handwritten-notes-into-text) | 专注创作、快速整理、保留原稿与转换结果的关系。 | 手写转换有联网和账户要求；部分多端功能受订阅影响。专用笔与大屏体验不是 Plus 已具备的能力。 |
| M-006 Supernote Manta / Nomad 体系 | 用标题、关键词、星标组织并导航笔记。[官方说明](https://support.supernote.com/1759244-using-titles-keywords-and-stars) | 长笔记不只有文件夹，还能按内容结构快速找回；AI 也可复用这些索引。 | 本轮只核查组织方式；不能据此承诺 Plus 手写体验或兼容其笔记格式。 |
| M-007 Viwoods AiPaper | 模板、标签、笔记；更新说明列录音转写、AI 指令、知识库、联网搜索试用和日历订阅同步。[产品页](https://viwoods.com/products/viwoods-aipaper/)、[3.8.5 更新](https://viwoods.com/pages/update-version-3-8-5) | 捕获资料后，整理、查询、再利用；AI 与内容工作区结合。 | 分清发布说明中的正式/试用功能和账号条件；有 AI 模块不证明能操作所有第三方 app。 |
| M-008 汉王 N10 Pro 二代 | 厂商描述语音转写、分角色/离线转写、会议纪要、文档批注和多渠道文件传输。[厂商介绍](https://www.hanwang.com.cn/index.php?a=show&c=index&catid=66&id=399&m=content) | 录音原件、转写、摘要和笔记应能关联保存；后续追问可找到依据。 | 离线转写建立在其软硬件上，不能移作 Plus 能力证明；本轮未验证其速度/准确率宣传。 |
| M-009 讯飞智能办公本 X2（历史型号） | 帮助页区分录音/转写/手写的导出途径，音频与 Word/PDF 输出存在不同条件。[导出说明](https://zskzs.iflytek.com/myfiles/d0a8ad2d-836d-4a8f-a587-18c0077db6c1/gzhqd.html) | 导出原始音频和可编辑文本应在设计阶段讲清楚，便于长期留存和迁移。 | 不把 X2 说明当成当前全系功能；文档本身也列有某些录音不能本地导出的限制。 |

## 3. 掌上墨水屏与手机

| 编号与对象 | 官方资料支持的功能 | 值得吸收的体验 | 依赖与边界 |
| --- | --- | --- | --- |
| M-010 BOOX Palma 2 | 自定义智能键、手势、小组件与屏保；按键可唤起 AI 或应用。[产品页](https://shop.boox.com/products/palma2) | 单手操作、全局快捷入口、阅读与日常工具之间少切换。 | 手机形状不能当成蜂窝通话证明；按键、系统和具体型号分别核查。 |
| M-011 Bigme HiBreak Pro | 蜂窝手机、Android 应用、在线 TTS、语音转文字、相机文字识别。[产品页](https://store.bigme.vip/pages/hibreak-pro-landing-page) | 阅读、听读和通信能在同一终端发生；呼叫和消息应融入全局状态。 | 页面推荐区含其他型号，不能混读参数或高刷宣传；其 Android 与相机能力不能直接迁移到 Plus。 |
| M-012 Mudita Kompakt | 官方列出离线工具、音乐、笔记、地图、侧载及实体断网模式。[当前产品说明](https://store.mudita.com/store/mudita-kompakt-global) | 可以安装工具，同时控制干扰；用户选择哪些提醒与入口需要出现。 | 2.0 页面仍使用预告措辞，未将其所有功能算作已交付；实体断网依赖硬件。[预告页](https://mudita.com/kompakt20update/) |

## 4. 阅读软件与内容工作区

| 编号与对象 | 官方资料支持的功能 | 值得吸收的体验 | 依赖与边界 |
| --- | --- | --- | --- |
| M-013 微信读书 | 腾讯的应用说明列书单、读书笔记、阅读历史、全文朗读、有声内容、AI 导读和问书。[开发者应用说明](https://apps.apple.com/cn/app/%E5%BE%AE%E4%BF%A1%E8%AF%BB%E4%B9%A6/id952059546) | 找书、理解、讨论、摘录、读听切换可以围绕同一本书组织。 | 不把用户在原服务中可读，等同于允许下载通用文件；社交与排行榜是可选参考。 |
| M-014 掌阅 App | 公司披露列书籍摘要、精读、思维导图及双人对话式 AI 讲书。[2025 年报摘要](https://static.cninfo.com.cn/finalpage/2026-04-25/1225184807.PDF) | 同一资料可有原文、导读、结构图和音频讲解等入口，按目的选择。 | 本条是 App 的发行人描述，不能归到任意 Ocean/Smart 硬件型号。没有把生成讲解当原书替代品。 |
| M-015 Readwise Reader | 内容聚合、批注/标签、检索、RSS、TTS、导出与 AI 阅读辅助。[文档](https://docs.readwise.io/reader/docs) | “稍后读”资料随后可以成为笔记、复习材料和待听清单。 | 采集、解析、服务端 AI 与设备呈现需拆开研究；不能因支持 PDF 就认为 MCU 能完成同样的解析。 |

## 5. 学词与语言学习软件

| 编号与对象 | 官方资料支持的功能 | 值得吸收的体验 | 依赖与边界 |
| --- | --- | --- | --- |
| M-016 百词斩 App | 图文情境、词根/例句、自定义词库、进度记录与针对性复习。[功能介绍](https://passport.baicizhan.com/privacy/introductions.html) | 同一个词可由图、音、语境进入学习；既可选主题，也可导入自己的词。 | 图片线索是否造成“认图不认词”要用脱离原图的回忆任务观察；不将效果宣传当结论。 |
| M-017 墨墨背单词 | 云词本可组织章节、提取词形；官方 API 描述词汇导入、词本管理和部分学习数据。[云词本](https://memodocs.maimemo.com/docs/QKUlwb6qTiLtzvk49BGc1ZVFnqg)、[API](https://memodocs.maimemo.com/docs/PNvOw2E1AivPtlkRlQgce1W1n2c/) | 阅读中遇到的词可直接变成一组学习内容；AI 收集资料后有明确入库动作。 | 自用凭据与对外发布应用的授权流程不同；存在 API 不代表选定接入墨墨，也不代表所有内容可自由复用。 |
| M-018 不背单词 | 影视/新闻原声例句、词根词缀、拼写测试、按表现调整学习安排。[官网](https://www.bbdc.cn/) | 从“认得释义”扩展到听懂、拼写和真实语境中的使用。 | 原声素材及词典内容有来源依赖；本项目需自己的可用内容方案。 |
| M-019 扇贝单词/听力/口语/阅读 | 逐句训练、听力填空、跟读复述、阅读即时查词及段落辅助。[官方介绍](https://www.shanbay.com/m/intro/) | 单词、句子、文章、听说练习共享学习上下文，减少重复整理。 | 原产品分多个 app；本项目是否分 app 另定。口语自动评分可靠性没有在 Plus 上验证。 |

## 6. 记忆卡片与资料生成练习

| 编号与对象 | 官方资料支持的功能 | 值得吸收的体验 | 依赖与边界 |
| --- | --- | --- | --- |
| M-020 Anki | 牌组、待复习/新学区分、显示答案后反馈；支持字段化内容导入及调度。[学习说明](https://docs.ankiweb.net/manual/studying)、[导入](https://docs.ankiweb.net/manual/importing/text-files) | 先尝试回忆再看答案；数据结构与展示模板分开；学习资料可带入自己的体系。 | 参考卡片与复习原则，不承诺直接运行 AnkiDroid 或完整兼容所有插件/卡组。 |
| M-021 Quizlet | 将笔记/文档制成卡片、提纲与练习；Learn 提供多种题型和针对薄弱内容的练习。[资料生成](https://quizlet.com/features/study-guides/)、[Learn](https://quizlet.com/gb/features/learn) | “给我资料”之后还应有练习入口；生成材料需要可编辑、可追溯。 | 生成式工具并不保证答案正确；部分能力有订阅/服务依赖，不能把产品界面能力当公共 API。 |

实体双面单词卡也作为交互基线：翻面、分组、标记、随手抽查无需多层菜单。这是通用设计比较，不是已评测的某个品牌，也不证明某种复习算法更有效。

## 7. 独立单词机与电子单词卡

| 编号与对象 | 官方资料支持的功能 | 值得吸收的体验 | 依赖与边界 |
| --- | --- | --- | --- |
| M-022 百词斩单词机 Mini / Max | 专门背词、离线学习、图背与发音；Max 页面另列宠物小狗。[硬件官网](https://danciji.baicizhan.com/) | 拿起即学、短会话、实体操作；陪伴式反馈不必依赖复杂动画。 | App 与硬件功能分开记录；不采用网页上的护眼/记忆效果断言作为验证结果。 |
| M-023 墨墨单词机 | 通过 App 绑定和管理词书；设备有英中/中英模式、学习量调整、熟知/暂停/收藏标记。[帮助文档](https://memodocs.maimemo.com/docs/support_wordmachine) | 复习方向、每日负荷、误操作撤回应容易找到；设备可专心承担学习执行。 | 内容下载、学习记录同步与备份是不同问题；首次设置和管理有伴随 App 依赖。 |

## 8. 电子词典、词典笔与词典软件

| 编号与对象 | 官方资料支持的功能 | 值得吸收的体验 | 依赖与边界 |
| --- | --- | --- | --- |
| M-024 CASIO EX-word E-W 系列 | 多部词典和词条/释义中的真人发音，支持范围由词典和条目决定。[官方支持](https://support.casio.com/cn/support/answer.php?cid=003001011004&num=1&qid=110093) | 有来源的释义、发音与例句；查词是快速确定的工具，不必每次等待模型生成。 | 历史系列参考，不代表当前在售全系；词库内容与授权需单独获得。 |
| M-025 有道词典笔 2.0（历史产品） | 官方介绍扫描查词、语音查询、发音及蓝牙耳机连接。[产品说明](https://c.youdao.com/dict/hardware/dictPen.html) | 从纸面遇到的词直接进入解释、发音和后续学习，输入摩擦很低。 | 扫描笔依赖专门光学结构，Plus 不能靠 app 变成扫描笔；可研究手机导入图片作为不同入口。 |
| M-026 欧路词典 | 离线词库、扩展词典、划词、发音、生词本同步与复习。[产品页](https://www.eudic.net/mobile/eudic.aspx)、[查词入口](https://docs.eudic.net/1/shi-yong-zhi-yin/dan-ci-cha-xun) | 全系统可复用的查词服务；保存词、原句和来源，再进入学习 app。 | 不默认兼容全部词典文件；可扩充词库不等于内容可无限制分发。 |

## 9. 儿童学习与综合学习机

| 编号与对象 | 官方资料支持的功能 | 值得吸收的体验 | 依赖与边界 |
| --- | --- | --- | --- |
| M-027 讯飞 AI 学习机 | 诊断、学习、练习衔接；错因分析、知识前置关系、启发式答疑和分级阅读。[官方方案](https://edu.iflytek.com/solution/family/learning-pad) | 错题关联薄弱知识，解释后再做相近任务；先提示再给答案。 | 官方也限定部分能力的学段/科目；不能泛化为所有考试或可靠自动批改。 |
| M-028 小猿 AI（软件） | 作业检查、答疑、同步学练，包含口算、字词听写、诗词、单词和口语场景。[官方产品页](https://leo.fbcontent.cn/) | 不同输入和任务模式共用个人学习记录；日常练习有轻量入口。 | 与小猿学练机是不同证据对象；不能把 App 所有功能写成某款墨水屏硬件已支持。 |
| M-029 洪恩识字 | 将字形/字义、互动练习与逐步可读的故事结合。[官方介绍](https://www.ihuman.com/shizi) | 学到的字进入下一篇可读故事，学习结果有可感知用途。 | 情境和反馈可以参考；动画素材及实际学习收益未被本项目验证。 |
| M-030 Khan Academy Kids | 年龄适配的阅读/数学活动、角色引导和自主探索。[官方介绍](https://www.khanacademy.org/kids) | 同时提供引导路线与自由选择；小任务有明确起止。 | 课程语言、年龄和家庭需求需对应；不照搬课程大纲。 |
| M-031 Duolingo Adventures | 用生活场景中的互动任务组织语言实践。[官方介绍](https://blog.duolingo.com/adventures/) | “完成任务”可成为儿童或成人的练习结构；趣味不等于只给儿童。 | 场景交互可以转成语音/静态图文，但不预设 Plus 可以运行原应用或全部动画。 |

## 10. 考试与题库工具

| 编号与对象 | 官方资料支持的功能 | 值得吸收的体验 | 依赖与边界 |
| --- | --- | --- | --- |
| M-032 粉笔 | 官方列题库、课程和 PC 模拟机考。[官方说明](https://www.fenbi.com/page/download/desktop) | 自由练习、薄弱项训练、限时考试应有不同节奏与结果展示。 | 后两种模式的组织是本项目建议；本页没有证明所有科目题库可导出或都有公开 API。 |
| M-033 驾考宝典 | 官方检索摘要列顺序、随机、专项、难题与模拟入口。[官网](https://www.jiakaobaodian.com/) | 同一题库按目标组合；标清适用考试、地区与内容版本。 | 仍是官方搜索摘要证据；未验证题库接口、完整正文或内容可获取性。 |

## 11. HiFi 播放器与音乐软件

| 编号与对象 | 官方资料支持的功能 | 值得吸收的体验 | 依赖与边界 |
| --- | --- | --- | --- |
| M-034 FiiO JM21 / FiiO Music | JM21 列网络传输与不同工作模式；Music 官方索引列按歌手/专辑/文件夹浏览、播放列表、续播、定时停止、无缝播放及曲间响度调整（ReplayGain）。[设备页](https://fiio.com/jm21)、[应用介绍](https://www.fiio.com/app) | 个人曲库、下一首队列、收藏、可靠续播与少看屏控制。 | 应用页实时解析不完整，功能取自官方搜索正文；不把手机 App 全部功能自动归到每款播放器。 |
| M-035 HiBy R3II（2025） | 实体媒体键、可定制下拉快捷操作、无缝播放、定时关闭、本地/网络音频及多种输出。[官方手册](https://guide.hiby.com/en/docs/products/audio_player/hiby_r3ii_2025/guide/) | 即使屏幕不活跃也能控制播放；常用操作少走菜单。 | DAC、耳放、输出接口与编解码是独立硬件/软件能力；没有据此承诺 Plus 的 HiFi 指标。 |
| M-036 Sony Walkman NW-A306 | 分类曲库、可编辑播放队列/列表、音效及曲间音量差处理。[播放说明](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733545.html)、[音效说明](https://helpguide.sony.net/dmp/1301/v1/en/contents/TP1000733581.html) | 队列与书库分开；插播结束后能回到原播放计划。 | 输出通道会影响可用音效；播放格式和无线方式也影响功耗，不套用其续航数字。 |

## 12. 儿童故事机、有声书与播客

| 编号与对象 | 官方资料支持的功能 | 值得吸收的体验 | 依赖与边界 |
| --- | --- | --- | --- |
| M-037 Yoto Player / Mini | 卡片入口、App 管理、下载后离线播放、睡眠定时。[离线说明](https://support.yotoplay.com/en_gb/does-my-yoto-need-to-be-online-to-play-content-rkmbFFj7fg)、[定时说明](https://support.yotoplay.com/en_gb/how-to-use-the-sleep-timer-on-your-yoto-HJ4gYKj7Mg) | 孩子可独立选内容；出门前知道哪些已下载；睡前有明确结束方式。 | 流式电台/播客例外；定时设置部分在手机 App。物理卡片不是 Plus 已有外设能力。 |
| M-038 Toniebox 1 / 2 | 已下载内容可离线；离线时不能新增下载，部分 App 配置无法同步。[1 代说明](https://support.tonies.com/hc/en-ca/articles/29036416576402-How-do-I-put-my-Toniebox-in-Offline-Mode)、[2 代说明](https://support.tonies.com/hc/en-au/articles/29352008338578-How-do-I-put-my-Toniebox-2-in-Offline-Mode) | “准备内容”与“孩子播放”可以分工；离线状态应让家长看得懂。 | 不将离线播放误作云端设置即时生效；卡片/玩偶操作只参考选择内容的直观性。 |
| M-039 Audible | 章节、速度、计时、片段、书签和笔记。[播放器帮助](https://help.audible.com/s/player?language=en_US)、[片段与笔记](https://help.audible.com/s/article/manage-clips-bookmarks?language=en_US) | “记住刚才这段”可以保存音频位置；读听学习都能回到具体内容。 | 参考播放器行为，不意味着可直接接入其加密书库；不同平台功能也有差异。 |
| M-040 AntennaPod | 按时长/日期等整理播客队列，智能混排可减少同一订阅连续堆积。[队列文档](https://antennapod.org/documentation/queue/sorting) | “给我十分钟新闻”应形成有顺序和时长的待听内容，而不只是搜索结果。 | 这里的时间预算组合是本项目建议，不能说原产品已有相同自然语言任务。 |

## 13. AI 研学与智能手机的系统动作

| 编号与对象 | 官方资料支持的功能 | 值得吸收的体验 | 依赖与边界 |
| --- | --- | --- | --- |
| M-041 NotebookLM | 根据选择的资料生成卡片/测验，调整主题、数量和难度；另有音频概览。[学习功能](https://blog.google/innovation-and-ai/models-and-research/google-labs/notebooklm-app-quizzes-flashcards/)、[音频概览](https://blog.google/innovation-and-ai/products/notebooklm-audio-overviews/) | 用户指定来源后，AI 把资料转成可学、可练、可听的不同形式。 | 云服务参考；本轮没有证明相应公共 API、儿童账号可用性或生成内容准确性。 |
| M-042 Android / Gemini Connected Apps | 官方描述通过连接应用处理媒体、消息和日程；Device assistance 可管理闹钟、设置、通知和媒体。[连接应用](https://support.google.com/gemini/answer/13695044?co=GENIE.Platform%3DAndroid&hl=en)、[设备动作](https://support.google.com/android/answer/15235441?hl=en) | AI 是系统入口，具体动作由有能力的服务执行；界面操作和语音操作能指向同一状态。 | 能力受地区、语言、设备、权限和应用支持影响；不能从其宣传推到“任何 app 均可任意操作”。 |
| M-043 iOS / iPadOS 快捷指令 | 动作按顺序执行，必要时获取输入或授权，结果可呈现。[Apple 指南](https://support.apple.com/guide/shortcuts/run-a-shortcut-from-the-app-apd5ba077760/ios) | 一次意图可以组合多个可理解步骤；用户可以查看实际发生了什么。 | 借鉴动作组合与结果展示，不将快捷指令本身等同于自主 AI 规划，也不选用 iOS 作为实施平台。 |

## 待补齐的品牌、型号与材料

纳入范围与完成核查分开记录，不能因为材料暂缺而把整个品牌从竞品范围删除。

| 对象 | 已取得线索 | 下一项需要补的事实 |
| --- | --- | --- |
| 掌阅 Ocean / Smart / Tango 等硬件 | [品牌店目录](https://ireader.jd.com/)及[认证店商品页](https://detail.youzan.com/show/goods?alias=2op92b4aeas77uz&from_source=gbox_seo)可见型号，功能详情多为图片。 | 每款与 App 的实际功能差异、文件导入/导出、AI 与更新支持。不能把 M-014 的 App 功能直接填入硬件。 |
| 墨案 inkPalm / Mini / 大屏产品 | 找到品牌站入口，访问 [moaansmart.com](https://www.moaansmart.com/)返回错误；第三方目录仅用于发现品牌。 | 当前官方手册、单手操作、词典/听书、开放应用和更新方式。未据第三方参数做本项目技术结论。 |
| 海信 A9 / TOUCH / Hi Reader | [品牌店目录](https://hisense.jd.com/)仍可见这些系列。 | 历史产品体验与当前服务/固件状态分开；尤其是音乐阅读器和蜂窝手机的能力区别。 |
| 小猿学练机 / 彩墨系列 | [官方店目录](https://mall.jd.com/view_search-2998983-51133548-99-1-20-1.html)可见学练机；M-028 是另行核查的软件。 | 具体墨水屏型号的输入、错题流程、离线范围和家长管理，不能用学习 App 能力替代证据。 |
| Meebook / 博阅、Tolino、索尼 DPT / 富士通 QUADERNO、华为/联想电子纸产品 | 纳入扩展名录，本轮未完成对应官方功能材料核查。 | 开放阅读、文档批注、大屏工作流、跨设备交换方面是否有新增功能模式。 |
| 山灵、iBasso、Astell&Kern，以及听力专用机 | 纳入音频扩展名录，本轮已核查 FiiO、HiBy、Sony 三组参考。 | 是否有尚未覆盖的曲库、盲操、语言训练、播放恢复与输出管理体验；不只追加 DAC 参数。 |

## 本轮能得出的结论

1. 产品参考范围已从社区固件扩到阅读、学习、记录、音频与系统操作。现有固件专题保留为硬件相关实现线索。
2. 用户真正要完成的任务常跨品类：一篇文章可以成为生词卡、听力素材、知识笔记和练习题。内容与进度的连续性比单纯增加 app 名称更值得研究。
3. 专用设备的价值常在于任务入口、按键与反馈、离线准备和恢复，而非功能数量。AI 原生设备仍需要能独立使用的明确界面。
4. 单词记忆、知识理解、考试训练、儿童故事和音乐欣赏需要不同节奏；可以共用底层，不宜强制同一种交互。
5. 专门手写层、扫描光学结构、DAC/耳放和蜂窝音频链路是硬件条件；研究其产品价值不等于承诺现有 Plus 可以靠软件全部达到。

跨品类提炼与候选项见 [功能机会与场景](/home/workspace/reader/docs/research/cross-category-opportunities.md)。本轮没有设备采购、实机对比、算法有效性试验或技术栈定案。
