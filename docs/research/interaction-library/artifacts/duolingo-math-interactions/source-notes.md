# Duolingo数学与音乐：来源范围及版本差异

查阅：2026-10-27。三篇官方正文已读，原HTML/正文/日期元数据留在工作区，获取清单另存[采集元数据](source-captures.json)。没有查看博文动图、连续视频或运行原生App。页面修改日期不代表当前软件构建；以下历史说明不能覆盖本次网页的全部行为。

| E | 原始来源与定位 | 可支持的范围 | 限制 |
| --- | --- | --- | --- |
| E-DUMA-01 | [Math介绍](https://blog.duolingo.com/duolingo-launches-math-app/)，课程、虚拟时钟与FAQ；发布2022-10-26，元数据修改2026-10-01 | 历史初级/中级范围；文字描述拖动钟针时另一针联动；顶部课程选择器换学科；课完成/XP与连续天数、任务的关联 | 未实际操作时钟；正文仍写iOS、英/西语及暂缺Legendary，不作为现行全端能力表 |
| E-DUMA-02 | [数学内容与表现形式的工程说明](https://blog.duolingo.com/developing-math/)，What makes math different、Content at scale、Area and perimeter、Rive；发布2024-10-18，元数据修改2026-09-18 | 同一知识可用数字、积木、文字问题表示；数字范围和情境影响难度；文档介绍动态生成图形及当时iOS/Android实现 | 不是接口契约、当前构建或Plus性能测试；未播放示例动图，不按图片标签猜交互 |
| E-DUMA-03 | [Music介绍](https://blog.duolingo.com/music-course/)，正文与FAQ；发布2023-10-11，元数据修改2025-01-16 | 历史描述用屏幕琴键、音符/谱表、听辨与完整歌曲组织练习；不必自备乐器；音乐课计入共享激励 | 音乐仍只到范围发现，未取得按错键、节奏容差、重听、退出和结算链；文章的效果说辞未审原研究，不采为学习效果结论 |
| E-DUMA-04 | [Math公开入口](https://www.duolingo.com/math)，FAQ与后续匿名UI；[操作审计](browser-operation-audit.md) | 当前FAQ文字说iOS/Android/Web、英语Grade 2–12、Math Coach及跳到主题；本次实际进入Grade 2练习、订正、结算并刷新 | FAQ声称的辅导/跳级没有执行；选Student和Grade 2只是研究条件，不是本家庭年龄/能力资料。不能由本次算术课证明课程全部覆盖或适合5–6岁 |

本次新注册三篇正文E与一条操作E，只有数学练习成为一个新任务F-M031-20；引导及退出补回F-M031-16、F-M031-18。Music和Math Coach仅补范围与缺口，不以一段宣传再造“已深入”的任务。

三篇匿名GET各一次，HTTP200且取得正文；工具初次缺少可选解析库，随后使用标准库解析，没有安装依赖。日期字段取JSON-LD，文章正文和日期分别核对。原件散列保留在采集元数据；正式来源短摘记不依赖工作区HTML才能理解结论。
