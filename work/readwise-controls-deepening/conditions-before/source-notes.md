# Reader：内容处理、阅读控制与静态证据

查阅2026-10-27。19篇官方正文由正常匿名GET取得，均HTTP200；网页构建、App版本和套餐未固定。原始HTML、article正文、标题/链接/图片清单及SHA保存在work/readwise-controls-deepening。仅解析公开帮助，没有运行Reader、登录、上传、邮件、退订、分享发布或API动作。下面是短摘记，不是全文镜像。

## 正文定位与边界

| 来源简称 | 关键定位与保留的限制 |
| --- | --- |
| basics（旧E-RDD-01） | reading progress/undo/trash：滑读位置与最远阅读进度分开；Shift+R重设可短暂显示0；隐藏返回按钮仍跟踪。Z、iOS摇动与toast撤销不是无限回滚；Trash仍限原删除设备。单篇移动恢复写Search、批量写View，差异保留。 |
| saving | 扩展取得已渲染内容；移动分享后可加标签/备注、移动位置。iOS Safari已登录源站与Android/其他App仅传URL条件分开；没有宣称绕过来源权限。 |
| organizing（旧E-RDD-06） | 标签应用到文档或高亮；Enter确认并关菜单、Ctrl/Cmd+Enter确认留菜单；移动Feed文件夹选源/Next、后续修改Done。 |
| global（旧E-RDD-02） | 技能含名称、描述、指令、作用域与模型；说明中的读写类别不是完整API清单。Remember+Approve/Deny改变后续工具权限；没有取消/部分成功/回滚证据。 |
| daily-digest | 从Feed与Saved for Later挑选；手机Home入口，网页Configure添加同组文档但不是同一滚动界面。关Digest同时关其邮件；iOS徽标最多1，查看后消失，不是读完所有文章的证明。 |
| action-tags | 高亮note中的.h1/.c1/.qa语法影响同步到Readwise后的呈现；Enter保存、Shift+Enter换行。题卡在Readwise，不当Reader原生复习循环；删除/修改后卡片联动未知。 |
| navigation | 归档/移动默认返回列表，auto-advance可改下一文档；Bump仅Date moved排序置顶。侧栏固定四栏目，其他可排序/钉住；手势配置与真正识别冲突未观察。 |
| eink | Account的总开关启用推荐项；单独调整需关总开关。四项为动画、翻页动画、对比、音量键。BOOX冻结建议未标固件；原文将Daylight一并讨论，不据此确认其面板技术或支持Plus。 |
| appearance | 字体/主题/方向、分页默认与长文布局；同设备分页设置保留。短/长工具栏分别选Modern/Classic，Modern需AI开关。正文解释纵向分页，但另有横屏平板Horizontal pagination双栏条件，不概括为所有平台不支持横向。 |
| parsing | 保存快照不自动重解析；刷新要删后重存且会删Reader/Readwise批注。同URL去重与不同参数URL重复分开；付费来源需具备原站访问条件。 |
| pdfs | 区域截图存Notebook；原版/文字视图的高亮互不叠显但Notebook可查。可导出带批注PDF；他处既有高亮不识别为Reader高亮。未测往返，且不采用原文把PDF简化为图片的技术概括。 |
| email-newsletters | Library/Feed地址不同；旧随机地址仍工作，二次个性化后旧个性化地址失效。Reader退订按钮屏蔽发件人而非退出外部名单，恢复要联系支持；本轮没有联系。原样/文本偏好按发件人跨端保留。 |
| videos | 字幕定位、自由浏览后回播放段；增强文本可能不叠显旧高亮。换字幕语言创建带语言名的新文档，来源必须有字幕选项；没有播放、转录或执行第三方书签脚本。 |
| sharing | 原址与含批注公开副本分开；撤销公开链接需确认。Bundle从过滤视图创建，不自动更新；手动刷新把新增内容加入已有接收者账号，删改传播未知；未创建或分享。 |
| filtered-views | 条件视图按标签/源/作者/查询创建；可按Location或Seen分栏、配置Home与计数。计数定义、跨端排序、查询错误和删除视图后数据未核。 |

## 五张实际查看的静态图

这些都是官方帮助插图，不是本项目操作截图或连续观察。没有查看所嵌GIF，不能把其文件名或替代文字当演示结果。

| E | 图片 | 观察与不能推断的部分 |
| --- | --- | --- |
| E-RWCD-01 | [保存分享面板](annotated-share-sheet_mobile.jpeg) | Saved to Reader、Read Now、五个编号动作和点击任意位置关闭可见；这是示例画面，不证明一次实际保存成功。 |
| E-RWCD-05 | [E-ink Mode](eink-mode_boox_annotated.png) | 总开关及四个灰显子项，页面文字要求先关总开关才能个别调整；未点击，不测残影/功耗。 |
| E-RWCD-06 | [Toolbar layout](reader-toolbar-layout_settings.png) | 短/长文分别有Modern/Classic；示例短文选Modern、长文选Classic，不当默认值。底层Settings可见部分条目，不推完整设置清单。 |
| E-RWCD-13，源E-RDD-02 | [技能表单](global-ghostreader_add-skill-form.png) | 名称/描述/指令、斜杠触发、作用域/模型、Cancel及灰显Save；未填表，未验证校验/保存。 |
| E-RWCD-13，源E-RDD-02 | [工具确认卡](global-ghostreader_tool-approval-card.png) | Tagging、Show details、Remember、Deny、Approve可见；详情没有展开，也没有动作结果。 |

目录从两组帮助侧栏及已读正文h2/h3提取，另列五图的28个明确控件/选项。入口、父子标题、示例和功能有交叉，不能当全部功能分母。尚未读的指南正文保持未知。

## 补充：布局迁移和外部AI能力范围

从E-ink帮助的长文工作流链接进入，另外读取四篇公开正文，均普通GET200，没有执行配置/连接操作，也没有查看其新图片。

- E-RWCD-14，longform：Account/General/Default view可选Currently reading以打开最近文档；不保证精确音频断点。音量键翻页限Android，连续滚动时跳一屏；iOS不提供此选项。长文TTS入口移至More，不是功能消失。
- E-RWCD-15，library-config：Triage、Shortlist、Classic分别组织内容位置；网页Library菜单/Edit Library locations选后Update，移动Account选配置。会移动文档时有确认。Shortlist配置中的位置与Triage中的同名标签视图不同，切换的完整映射/回滚未给。
- E-RWCD-16，recommendations：官方提示示例把时间预算、主题、旧收藏或不同领域筛选后整理清单；全网扩展依赖外部AI的搜索，周期任务依赖外部调度。搜索可能漏项，先按主题列文档是文档建议。其“动作都可逆”不能扩成删除/清空的保证；没有运行示例。
- E-RWCD-17，mcp：只采用Readwise自己公布的数据范围、Reader16项及Readwise6项工具用途和索引更新说明；不是取得工具schema、认证范围或执行回包。客户端安装章节只登记标题，不作为OpenAI等第三方当前用法的验证。新旧服务器的数据范围不同；本文推荐示例提删除，但Reader工具用途表没有明确列文档删除动作，保留覆盖差别。

Action Tags所链readwise.io/mastery正常打开后转到登录页，本轮未登录；这条探索只保存在work/readwise-controls-deepening/mastery-discovery.json，不能据登录壳补练习、评分或题卡生成结果。没有连接MCP、运行CLI、读取用户书库或设置定时任务。
