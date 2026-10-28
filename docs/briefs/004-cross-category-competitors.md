# 第四期简报：把整个市场作为产品参考

日期：2026-10-26。阶段：竞品研究与产品原型探索。

## 研究范围已经调整

此前对二次开发固件的关注偏重，不能代表你希望参考的市场范围。现在将阅读器、墨水屏平板和手机、阅读软件、学词软件、记忆卡片、独立单词机、电子词典、学习机、题库、HiFi 播放器、故事机和手机 AI 都纳入持续研究。

已建立 **13 类、43 组产品/服务**的[竞品矩阵](/home/workspace/reader/docs/research/market-competitor-matrix.md)，按功能、体验、服务依赖和硬件条件比较。它覆盖主要功能模式，尚不能称为穷尽所有在售型号。资料不足的掌阅硬件、墨案、海信及部分学习机等另列待补，不从范围中排除。

## 跨品类比较带来的主要启发

| 参考方向 | 代表对象 | 我建议吸收的体验 |
| --- | --- | --- |
| 阅读器与书库 | Kindle、Kobo、PocketBook、微信读书 | 找到内容、导入、查词、摘录、读听切换与继续阅读连成一条路径。 |
| 墨水屏笔记与知识整理 | BOOX、reMarkable、Supernote、Viwoods、汉王、讯飞 | 原文、录音、笔记、标签和 AI 整理稿能关联找回，重要记录可导出。 |
| 掌上设备与手机 | Palma、HiBreak、Mudita | 单手操作、可配置按键、清晰返回入口、控制干扰，同时保留所需工具。 |
| 单词与卡片 | 百词斩、墨墨、不背单词、扇贝、Anki、Quizlet | 不同回忆方式、语境、复习负荷和自定义内容；从阅读直接进入学习。 |
| 电子词典与词典笔 | CASIO、有道、欧路 | 快速查到有来源的解释和发音，保存原句；离线基础工具随时可用。 |
| 儿童学习和备考 | 讯飞、小猿、洪恩、Khan Kids、Duolingo、粉笔、驾考宝典 | 引导、自由练、错题巩固和模拟考试采用不同节奏；趣味与专注均可选择。 |
| 音乐、有声书、故事机 | FiiO、HiBy、Sony、Audible、Yoto、Toniebox、AntennaPod | 个人曲库、队列、实体控制、音频书签、逐段学习、离线准备与睡眠定时。 |
| AI 研学与系统动作 | NotebookLM、Gemini、快捷指令、墨墨开放 API | 把资料变成可用内容，把自然语言变成真实动作，并能检查结果。 |

表中整合体验是本项目建议；每个产品具体有哪些功能、哪些仍有条件，详见矩阵中的逐项来源。

## 四个发现尤其值得继续讨论

**1. 单词机的专注体验可以和可安装 app 共存。** 百词斩硬件强调离线背词，墨墨单词机提供学习方向、学习量和熟知标记等直接控制。可借鉴“拿起就能开始一个小任务”，同时由系统保留其他应用入口；不必把整台设备永久限制成一种用途。[百词斩硬件](https://danciji.baicizhan.com/)、[墨墨单词机](https://memodocs.maimemo.com/docs/support_wordmachine)

**2. 阅读、词典、词卡应能交换有上下文的数据。** 墨墨官方 API 已描述词本和学习数据能力，欧路提供查词和生词本；这支持把“文章中的词 → 原句 → 复习”作为明确流程。是否接入现成服务、还是自建应用，仍未选定。[墨墨 API](https://memodocs.maimemo.com/docs/PNvOw2E1AivPtlkRlQgce1W1n2c/)、[欧路功能](https://www.eudic.net/mobile/eudic.aspx)

**3. 音乐、听书、精听、儿童故事值得有各自的控制方式。** 音乐重曲库和队列，听书重章节与书签，精听重句段和提示，儿童故事重独立操作与离线内容。可以共用播放底层；HiFi 音质还取决于实际音频硬件，不能由增加播放器 app 保证。[HiBy 手册](https://guide.hiby.com/en/docs/products/audio_player/hiby_r3ii_2025/guide/)、[Audible 书签](https://help.audible.com/s/article/manage-clips-bookmarks?language=en_US)

**4. AI 第一原生能力应覆盖内容准备和实际执行。** NotebookLM、Quizlet 展示资料转练习的方向，手机 AI 和快捷指令展示动作组合的方向。我们希望两段接起来：准备的资料真正进入 app，实际记录进度，随后还能继续。[NotebookLM 学习功能](https://blog.google/innovation-and-ai/models-and-research/google-labs/notebooklm-app-quizzes-flashcards/)、[Gemini 设备动作](https://support.google.com/android/answer/15235441?hl=en)

## 已整理的新增候选

新增 C-009～C-024 共 16 项：全局查词与语境收藏、多种回忆方式、复习负荷、错题关联、资料转学习包、结构化笔记、逐句精听、音频书签、曲库队列、睡前与家人声音、离线内容包、任务专注模式、跨 app 动作组合、纸面导入、手写研究、可选角色与任务反馈。

这些候选均未成为必做功能；原先三个优先场景也只是局部建议，不能替代更广的产品比较。下一步可用成人主题学习、儿童故事识字、备考、自由阅读、通勤精听、睡前故事、纯听歌和 AI 代办这八种场景检查产品是否连贯。[功能与场景提炼](/home/workspace/reader/docs/research/cross-category-opportunities.md)

硬件仍只考虑 E-Ink4 Plus。产品价值和技术实现分别判断；专用手写、扫描笔、高保真输出及其他设备的刷新性能都不直接转写成 Plus 已具备的能力。基本方向经你认可后，才开始全系统设计。
