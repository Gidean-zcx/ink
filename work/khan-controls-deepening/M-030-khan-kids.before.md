# M-030 Khan Academy Kids：自主阅读、引导学习与进度

查阅：2026-10-26。范围：官方家长帮助中的移动应用；未固定 iOS/Android 构建与地区。资料以英语课程为背景，不能把年级映射直接用于本家庭的中文、英语或数学水平。**D2 文档重建，无亲自操作、无连续视觉观察**。

## 流程与状态

| 流程 | 入口、动作与反馈 | 分支/数据 | 证据 |
| --- | --- | --- | --- |
| F-M030-01 同一本书自主读或跟着读 | 主页紫色图书馆 → Books → 选书 → Read to Me 或 Read by Myself。前者有朗读和文字高亮；后者无朗读，使用箭头翻页。 | 从 Reading 分类进入书籍时，可能是含问题、点图等互动的阅读课；入口改变任务，不能只看书名。 | E-KHK-01 |
| F-M030-02 引导路线、进度与奖励 | 主页播放入口进入交替安排的学习活动；路线依表现调整，间歇发放奖励。家长入口经成人操作门槛后，可选儿童档案与学习级别。 | 图书馆进度以不同颜色勾号区分掌握/进行中/发展中；需查看正确档案和级别。中途离开引导路线，再返回可能开始下一课，不能宣称课内精确续接。 | E-KHK-02、03 |
| F-M030-03 离线找活动 | 图书馆 → 行李箱图标 → Kodi’s Suitcase 的离线活动；Create 也可离线使用。 | 首次建立账户需联网；离线范围是指定内容区，不能推广为全课程离线。 | E-KHK-04 |

## 需要继续拆的游戏与学习细节

- Books 与 Reading 分离，为“不想被提问打断”和“带练习理解故事”提供不同入口。具体模式能否中途切换、是否保留同页位置，资料未明确。
- 奖励与完成/准确表现相关，但确切阈值、失败后提示顺序、重复刷课是否重复奖励、奖励动画能否跳过均未知。现阶段不足以完成游戏数值或关卡策划分析。
- 家长看到勾号缺失时，官方让检查档案/级别并重启同步；这说明进度可见性还受视图与同步影响，不能立即解释为学习记录被删。
- “掌握”是产品的进度标签；没有本家庭实际学习效果、迁移能力或长期保持证据。
- 儿童账号切换、多人抢用、退出应用、断电时课内作答保存粒度尚未查清。

## 对本项目的意义（推断）

关联 R-014、R-018、R-019 与已知 5–6 岁自主阅读能力：同一内容可以支持自在读听和互动学习，但不必每次听故事都插入测验。AI 应区分当前阅读模式、课内位置、作答与能力记录；儿童得到奖励和真正掌握知识应分别观察。尚未决定课程路线、奖励形式或家长入口。

## 后续观察脚本

固定版本与测试档案，同一本书分别从 Books/Reading 进入：记录所有按钮、朗读高亮、提示、答错重试和退出位置。再选一项数学活动，连续记录首次失败、提示后成功、重复完成及奖励变化。首次先由成人检查流程，后续家庭观察需单独记录儿童实际表现；这些步骤均未执行。

## 证据

- E-KHK-01：[书籍与阅读模式](https://khankids.zendesk.com/hc/en-us/articles/4409036780955-Learn-more-about-the-books-in-the-Khan-Academy-Kids-app)。
- E-KHK-02：[学习级别与进度](https://khankids.zendesk.com/hc/en-us/articles/360041615571-How-does-the-learning-level-adjust-and-how-do-I-view-my-child-s-progress)。
- E-KHK-03：[学习主题与路线](https://khankids.zendesk.com/hc/en-us/articles/360014856151-Learning-Topics-Using-Khan-Academy-Kids-in-Educational-Settings)。
- E-KHK-04：[离线内容](https://khankids.zendesk.com/hc/en-us/articles/360029139531-Learn-on-the-go-with-offline-content-in-Khan-Academy-Kids)。

尚未覆盖完整课程、每种题型、教师模式、全部角色房间与奖励体系。不能把这三项任务称为完整儿童产品拆解。

## 深化：提示、答错、奖励与重进（2026-10-26）

本节补充原有三项任务，不替换旧证据。新增 **D2 文档重建**，未操作应用、未听到应用音效、未连续观看一题。版本仍未固定；依据为 2025 年产品经理设计说明和 2026-02-20 更新的官方帮助。设计文章 HTML 明确发布日期为 2025-04-29、更新日为 2025-05-01，不把文章当作当前所有题型的逐题规格。

| 流程 | 文档可支持的动作与状态 | 仍断开的部分 | 证据 |
| --- | --- | --- | --- |
| F-M030-04 求助与错后再试 | 进入 Learning Path；可点 Kodi 听她接下来要说的话。答错分支有 Kodi 的思考手势、鼓励再试及温和错误音；需要额外帮助时有分步动画示范正确做法。 | “点 Kodi”不等于已证明重播上一条提示。示范是自动触发还是按钮调用、连续错几次出现、示范后由谁完成操作、是否允许跳过，均未说明。 | E-KHD-01、03 |
| F-M030-05 正确反馈与奖励去向 | 正确有庆祝和正确音；**首次尝试即答对**时，粒子飞向礼物车；成功活动逐步积累后，车带来角色礼物。回到主页后，可点底部角色，在其房间使用逐渐增加的收藏。 | 首次正确奖励是独立分支，不能接成“先答错→重试正确→同样粒子”。错后正确的奖励、最后一题结算、车到达阈值、领奖/关闭按钮、回主页的具体操作未知。 | E-KHD-01、03 |
| F-M030-06 学习路径被打断后重进 | Learning Path 中途被打断→回到应用→按 Play，官方明确是开始**下一课**。 | 这不是原题续做；该条未区分切后台、强退、断网和跨设备。未完成课的尝试记录、是否计入完成和何时再出现未知；不能推广至 Books 或图书馆活动。 | E-KHD-02 |

E-KHD-02 本次已打开帮助正文，限定并澄清 E-KHK-03 的旧索引证据：原 F-M030-02 中“可能开始下一课”应按上述明确规则理解。它仍不证明操作系统强杀等每一种中断都采用同一保存策略。

上述证据把错误反馈、示范与奖励条件分开了，但没有提供一名儿童从提示到结束的一次完整记录。也未证实错误扣分、限次、强制重做、求助处罚或重复刷课收益。角色礼物是参与反馈，不能据此认定数学或阅读已掌握。

对本项目的推断：可以分别保存首次独立作答、提示使用、提示后正确和任务是否完成；不要把它们压成单一“成功”。对已能自主阅读、偏好听书的 5–6 岁儿童，语音提示应可以主动调用，并与完整听故事模式分开。若采用下一课重进策略，应显式说明上一活动状态；这是本项目建议，不是已核实的 Khan 界面。

### 本次取证与未能观察的材料

- E-KHD-01：[产品经理设计说明](https://blog.khanacademy.org/supporting-english-language-acquisition-with-khan-academy-kids/)，An Expressive Guide Character、Auditory Feedback、Step-by-Step Animations、A Visual Reward System。打开正文并核对公开 HTML 的日期字段；内嵌 KodiOops／ThisIsHow／Sparkles GIF 链接可定位，但读取器不支持 GIF，未把它们标为已观察。
- E-KHD-02：[学习主题与路线帮助](https://khankids.zendesk.com/hc/en-us/articles/360014856151-Learning-Topics-Using-Khan-Academy-Kids-in-Educational-Settings)，How do children use the app?，2026-02-20；正文直读，针对中断后按 Play 的规则。
- E-KHD-03：[家长开始使用帮助](https://khankids.zendesk.com/hc/en-us/articles/360006764812-Parent-and-Home-Account-Users-Getting-Started-and-creating-an-account-with-Khan-Academy-Kids)，Tap the big green button、Tap the animals，2026-02-20；正文直读。
- E-KHD-04：[官方教师培训 Three Ways to Play](https://sites.google.com/khanacademy.org/khanacademykids/getting-students-into-the-app/three-ways-to-play)，可读模块及其嵌入的 `Student_5Minute_Intro_to_KhanKids.mp4`。公开页面 HTML 指向 [Drive 演示](https://drive.google.com/file/d/1-DE8WB6u4JU7GCeXqH2yMSWzuwYSwcGz/view)；工具无法读取视频，故无逐秒观察或操作结论。
- 学习主题页所连的[interactive learning 旧帮助](https://khankids.zendesk.com/hc/en-us/articles/360007065732-How-does-Khan-Academy-Kids-support-interactive-learning-)本次返回 404。搜索到的 Khan 主站练习重开/掌握分数说明不属于 Kids，未采用。
