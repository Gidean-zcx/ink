# M-007 Viwoods AiPaper：选区、录音与 AI 结果的不同去向

## 范围与版本

查阅2026-10-26。固定 **10.65英寸 AiPaper、Android 13、英文国际版系统 V3.15 文档基线**，不含 Mini、Reader 或 Reader C；实际设备构建与AI服务配置未知。当前官网V3.15、V3.14发布记录为主要依据，早期手册/2025年文章仅补明确路径并保留版本差异。[E-VIW-01～06]

D1+D2资料研究；没有登录、安装、亲自操作或连续视频。本次沿官网AiPaper英文链接取得70页原PDF并查看部分静态页，归档7页；web因文件过大未解析，普通HTTP200取得。PDF元数据2025-04-30不当版本号；SHA256与原镜像URL标识相同，因此补的是原件取得与图像证据，不是新手册。编码异常处凭图核读，不猜按钮。[E-VICD-01～08] **V3.8.5是网址/页面标题，正文自称Version3.8**；因此只作历史增量证据，不能推定精确构建。[E-VIW-07、11、12]

## 功能目录与已查/待查

| 域 | 已查内容 | 尚未闭环 |
| --- | --- | --- |
| 系统 | Home/任务管理、控制中心、刷新三模式、应用/网络故障排查 | 首次向导、全部手势默认值、指纹失败、锁屏录音 |
| Paper/Meeting | 选区、标记/大纲、录音转写、AI重复处理、输出格式 | 新建到落盘全过程、转写错误定位原声、原稿替换/撤销 |
| Learning | 选词AI、批注/高亮、页/章导航、离线英语词典 | 扫描件OCR、TTS和音频位置、跨窗口返回锚点 |
| Knowledge Base | Picking旧入口、来源app命名、OCR后共享/AI | 引文页码/原链接长期保留、删除来源后的关系 |
| Daily/Memo | 待办、日历、笔记联动、V3.15多页Memo | 日历双向冲突、提醒后台权限、删除恢复条件 |
| 文件/云 | ViTransfer、第三方云、V3.15原生云开关、账号分离 | 同名覆盖、离线删除传播、回收站时限当前适用性 |

历史手册目录索引：Quick Start p2；Input p16；Paper p17；Learning p27；Meeting p34；Daily p37；Picking p41；Encryption p46；Apps p47；AI p48；Files p51；Third-party Cloud p54；Mailbox p55；ViTransfer p62；Share p66；Memo p67。本次按实际查看的目录建立[17章台账](../inventories/M007-historical-manual-inventory.csv)；只表示历史章节范围，不是当前全部菜单或独立功能总数。[E-VIW-12；E-VICD-02]

## 任务流程

表中“反馈”均依文档；静态图已查看的条目标E-VICD编号，但没有执行其动作。

| F 编号 | 任务 | 前置状态 | 操作与官方反馈 | 数据变化与结束 | E 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
| F-M007-01 | 从正文进入系统控制 | app打开 | 右上角下拉Control Centre；选择Best Display、Fast或Ultra-fast。长按Home进入Task Manager，可关闭不用的app。 | 刷新策略与关闭进程是两个动作。 | E-VIW-08；E-VIW-09 | 模式是否逐app保存；关闭前自动保存、听读中断和耗时。 |
| F-M007-02 | 从书内选段问AI | Learning中可选文字；联网 | 长按/选择文本→AI Assistant，选区自动带入对话；也可选翻译/词典。 | 选中文本是上下文，不证明AI自动取得整本书。 | E-VIW-06；E-VIW-12 | 关闭对话后原选区/页锚；扫描PDF；长文本截断。 |
| F-M007-03 | 设置重复使用的AI指令 | AI界面；已知任务 | 填命令名称及指令并保存；选段后点该命令。旧文章写最多5条，长按名称确认删除。 | 保存提示词与保存生成结果不同。 | E-VIW-06 | V3.15数量上限、直接编辑入口、已有结果是否受改指令影响。 |
| F-M007-04 | 语音提问并带走结果 | 联网；历史入口需复核 | 旧说明：长按右下AI入口输入最长15秒语音；短按键盘输入。AI输出可导出TXT/HTML。 | 短语音问题与Meeting长录音分属不同任务。 | E-VIW-06 | 误触提交/取消、失败重试；历史会话保留期与最新云端关系。 |
| F-M007-05 | 录音后转写、改结果再输出 | Paper/Meeting；V3.14+；联网AI | 单段上限60分钟；完成后转写、润色/摘要。对结果重新选段、再运行命令；TXT/PDF导出。 | 已证实可连续处理结果；回写原笔记的方式仍未知。 | E-VIW-03 | 开始/停止精确按钮、录音自动分段、原音保留、纠错回听、断网续传。 |
| F-M007-06 | 区分录音数据与显示状态 | Paper/Meeting已有录音数据 | 录音显示开关可隐藏浮窗；侧栏显示当前录音段名，文本可Copy All；用专用滚动条阅读转写。 | 隐藏窗口不应被解释为停止或删除；导出文本不是音频备份。 | E-VIW-04 | 隐藏时是否持续录音、锁屏/切app、后台杀进程后的恢复，均待测。 |
| F-M007-07 | 从长笔记找回标记位置 | Paper已有内容；V3.14+ | 圈选内容→标记图标；侧栏点标记回源页，再圈选或双击编辑弹窗移除。More Features或Multi-Page Editing建大纲，侧栏可改名/删除。 | 标记与大纲是导航对象；删除入口不等于删原文。 | E-VIW-03；E-VIW-04 | 重排/移页后的锚点、同名条目、撤销和恢复。 |
| F-M007-08 | 提取其他app内容并继续AI | 有待摘录材料 | 历史Picking可由下拉开浮动按钮，默认续开最近会话，“…”换/新建；新版本裁剪后可OCR→共享/AI，点选区外取消裁剪。 | 老Picking与新Knowledge Base名称需辨明；当前剪辑按来源app递增命名。 | E-VIW-07；E-VIW-04；E-VIW-05 | 是否保存URL/书页、来源修改、AI使用的是图像还是OCR文本、重复裁剪去重。 |
| F-M007-09 | 从阅读导航回到证据 | Learning已有书 | 点屏幕下部调进度条、拖动或箭头跳章；高亮汇入Reading Notes。V3.13新增离线英语词典，不能据“AI需网络”否定本地词典。 | 书签/标注与临时跳页不同。 | E-VIW-12；E-VIW-05 | 跳转后的一键返回历史、其他语言离线词库、删书后标注保留。 |
| F-M007-10 | 通过ViTransfer传入资料 | 两端同Viwoods账号 | 电脑my.viwoods.com选Import或手机“+”；设备ViTransfer→Receive。 | 是显式收件；不是自动双向云同步。 | E-VIW-12 | 账户切换、重复名、断线/取消/满盘；旧手册路径当前适配。 |
| F-M007-11 | 选择需要同步的内容（D1） | AiPaper V3.15；账号与网络 | 新原生云默认关闭，启用得10GB；Paper/Meeting/Knowledge Base/Memo/Daily分别控制同步，Learning选书上传。 | 原生云、第三方云和ViTransfer要分开；账号内容独立。 | E-VIW-02 | 开关完整菜单、合并规则、离线删除、云满、退出账号本地可见性。 |
| F-M007-12 | 把手写/日程推进为待办（D1） | 相关内容存在 | V3.13圈选手写建待办；V3.14允许补充、批建、按完成状态筛选；V3.15 Memo支持多页和手写待办。 | 待办是另一对象，不能推定会改写原手写。 | E-VIW-05；E-VIW-03；E-VIW-02 | 完整创建/完成/误删恢复、跨日迁移和日历同步冲突。 |
| F-M007-13 | 网络/Google服务故障恢复 | 功能无法联网或授权 | WLAN遗忘网络重连；Google服务异常可Settings→Play Store重启Google Service，按官网授权步骤处理。 | 网络恢复不证明未完成AI请求会自动续跑。 | E-VIW-09 | AI请求队列/收费重试、离线识别语言包、缓存/数据清理影响。 |

| F-M007-14 | 新建及修改Daily日历事件 | 历史手册；Daily日历页 | 点＋→填写名称、全天或起止时间、备注→Confirm；点已有事件进入详情，可编辑/删除。 | 日历事件与手写任务/笔记不同；Cancel入口可见，未执行。 | E-VICD-01；E-VICD-03；E-VICD-04 | 时段冲突、时区、提醒、重复事件、删除恢复和当前构建变化。 |
| F-M007-15 | 绑定日历并处理解除绑定 | 历史手册；Outlook或Google账号 | Daily设置→选择账号授权；页面有手动同步入口。手册称事件增删改自动同步，解绑后设备只保留本地创建事件。 | 企业Outlook需管理员授权；失败建议重试。此解绑规则不代表原生Viwoods Cloud数据规则。 | E-VICD-01；E-VICD-05 | 授权回调、冲突优先级、离线删除传播、解绑前提示及远端是否保留。 |
| F-M007-16 | 从第三方云导入或导出可编辑笔记 | 历史Viwoods Files APK；授权云盘 | Cloud Drive登录；长按云文件→Import→设备目录确认。My Files长按→Export，选择PDF或note及云目录。 | 首个账号默认同步目标；note仅AiPaper能打开编辑，且各应用note格式不能互导，需回到对应应用。 | E-VICD-01；E-VICD-06；E-VICD-07 | PDF与note信息损失、同名覆盖、媒体是否随包、导出失败、重新安装后的完整恢复。 |
| F-M007-17 | 选择第三方云的同步对象与格式 | 历史手册；已授权第三方云账号 | 主页云同步图标短按执行；长按进入云界面→Settings，选择账号、目标文件夹和PDF/note格式。 | Google Drive/OneDrive/Dropbox与V3.15新增原生云分别研究；同步按钮不证明双向合并或独立历史备份。 | E-VICD-01；E-VICD-08 | 调度/自动触发、进度与失败、取消、删除传播、目录多选含义与账号切换。 |

## 版本差异、错误与数据边界

| 分支 | 已有依据 | 未知/最小核查 |
| --- | --- | --- |
| 旧资料称无需账号/本地会话 | 2025AI文章描述基础AI入口；后续Web Search试用要登录，V3.15云同步也依赖账号。[E-VIW-06、07、02] | 不把“无需账号”扩大到全部服务；当前AI配额/付费方案未查明。 |
| 断网 | 当前FAQ明确本地读写可用、AI请求需网络；离线英语词典是独立例外。[E-VIW-10、05] | 录音后转写断线是否排队、重试是否生成重复结果。 |
| USB无法访问 | V3.14在已设锁屏密码且休眠锁定时阻断USB文件访问。[E-VIW-04] | 解锁后重连是否恢复；该保护不能推定所有分享出口也受控。 |
| 音频无声/蓝牙断开 | 当前FAQ说明本机没有内置扬声器，音频使用蓝牙外设。[E-VIW-10] | 断连后录音/播放状态及重连恢复点；不能借Reader的蜂窝能力补AiPaper。 |
| 修改、取消和删除 | 历史Daily事件可编辑/删除；解除外部日历绑定仅保留本地创建事件。[E-VICD-04/05] | 事件、手写任务、日历笔记与云笔记是不同对象；不把解绑规则套到全部内容，回收期未统一核实。 |
| AI生成内容有误 | V3.14允许结果再选段重复命令。 | “改指令→新结果”是否保留上次结果、是否可回原音逐字核对、是否原位覆写。 |
| 同步后内容缺少 | V3.15修复Daily导出/同步笔记格式丢待办内容的已知问题。[E-VIW-02] | 修复说明不是本轮复现通过；需要旧/新同一测试笔记核验。 |

## 实际观察与项目关系

本轮补原PDF静态观察，图片中的按钮未实际触发；没有测延迟、残影、60分钟录音或AI准确率。历史手册不含V3.15原生云说明，F11仍D1；Daily图可见任务框，但不能证明手写待办创建/完成闭环，F12也保持D1。三种刷新模式的宣传速度不构成R-011的60fps/无残影证据。

以下为推断：对G-002，最有价值的是“用户选定输入范围→执行具名指令→检查结果→再次处理→显式导出”；仍缺原音、识别稿、摘要、笔记与任务之间的可追溯关系。Plus可参考固定AI入口和选区操作，但大屏套索/多层面板不宜直接缩到掌上。对成人研究，来源锚点比模型数量更关键；对5–6岁读听，AI短语音入口与长录音必须明显区分，R-018仍需单独验证书/音频断点。

后续最小观察：固定V3.15实际构建，完成一段短会议的录音→断网→重连→转写纠错→摘要→改指令→回原页→导出，再删除一个结果并观察其他对象。另测两个账号和重复文件，记录对象是否新增或替换。

## 证据

全部查于2026-10-26。版本文章来源日期未明者不推算；旧12为文字镜像证据，新VICD为原件与实际看过的静态页。

| E 编号 | 来源 | 定位 |
| --- | --- | --- |
| E-VIW-01 | [AiPaper产品页](https://viwoods.com/products/viwoods-aipaper/) | 10.65英寸、Android13；功能存在层，不作操作证明。 |
| E-VIW-02 | [V3.15发布](https://viwoods.com/pages/system-update-3-15) | Viwoods Cloud、Paper、Memo、Daily修复、账号/密码；官方HTML正文。 |
| E-VIW-03 | [V3.14发布](https://viwoods.com/pages/system-update-3-14) | 60分钟、连续AI、TXT/PDF、标记及大纲。 |
| E-VIW-04 | [V3.14详细变更](https://viwoods.com/blogs/paper-tablet/viwoods-ai-paper-v3-14-update) | 2026-06-02；录音显示/段名、Copy All、OCR后动作、USB锁定。 |
| E-VIW-05 | [V3.13变更](https://viwoods.com/pages/update-v3-13) | 离线英语词典、AI命令覆盖、剪辑命名、待办、批量文件操作。 |
| E-VIW-06 | [选段与自定义AI操作](https://viwoods.com/blogs/paper-tablet/viwoods-ai-enhanced-reading) | 2025-05-26；选段、5条命令、15秒语音、TXT/HTML；旧版本条件。 |
| E-VIW-07 | [历史3.8/3.8.5增量](https://viwoods.com/pages/update-version-3-8-5) | URL与正文版本标签不同；Picking、录音转写、Web Search试用。 |
| E-VIW-08 | [三种刷新模式与控制入口](https://viwoods.com/blogs/paper-tablet/viwoods-e-ink-refresh-modes-explained) | 2025-05-22；右上下拉、Best Display/Fast/Ultra-fast。 |
| E-VIW-09 | [Troubleshooting](https://viwoods.com/pages/troubleshooting) | WLAN、Google Services、Home任务管理；官方HTML正文。 |
| E-VIW-10 | [当前FAQ](https://viwoods.com/pages/frequently-asked-questions-about-viwoods) | AI联网、本地读写、无内置扬声器/蓝牙、V3.15云；按产品系列辨别。 |
| E-VIW-11 | [官方手册入口](https://viwoods.com/pages/user-manuals) | 旧记录为仅发现链接；本次由英文链接普通HTTP取得原件，见E-VICD-01。 |
| E-VIW-12 | [AiPaper厂商手册镜像](https://device.report/m/90bd3c06b4bb4425ad62aef69dd2b553ab622ead68b01a668cea4ab3b5786676) | 70页，镜像标2025-04-30；p1目录、p29/33阅读、p63传输；乱码段不采。 |

### 本次原手册与静态页

所有来源查阅2026-10-26；同一PDF的不同页不是独立交叉验证。

| E 编号 | 来源与定位 | 限制/保存 |
| --- | --- | --- |
| E-VICD-01 | [AiPaper官网所链原版手册](https://cdn.shopify.com/s/files/1/0729/7368/0674/files/viwoods-AiPaper-User-Manual.pdf?v=1787939059)；官方User Manuals→AiPaper English；70文件页；正文1–69 | web因文件过大未解析，普通HTTP取得原件。SHA256等于先前device.report镜像URL标识；这是原件可取得的补证，不称新修订或独立交叉验证。文本编码异常处只凭实际查看图片判断。 |
| E-VICD-02 | [AiPaper手册17章目录](https://cdn.shopify.com/s/files/1/0729/7368/0674/files/viwoods-AiPaper-User-Manual.pdf?v=1787939059)；印刷1；PDF文件2 | 实际看目录，仅标手册章节范围，不当17项独立功能或当前完整目录。；[摘页](../artifacts/office-aipaper-deepening/Viwoods-AiPaper-manual-file-02.png) |
| E-VICD-03 | [Daily月历和任务面板](https://cdn.shopify.com/s/files/1/0729/7368/0674/files/viwoods-AiPaper-User-Manual.pdf?v=1787939059)；印刷37；PDF文件38 | 印刷37；任务勾选框、Notes、事件与笔记反馈标记可见，不能推定完成、跨日迁移的结果。；[摘页](../artifacts/office-aipaper-deepening/Viwoods-AiPaper-manual-file-38.png) |
| E-VICD-04 | [Daily事件创建编辑](https://cdn.shopify.com/s/files/1/0729/7368/0674/files/viwoods-AiPaper-User-Manual.pdf?v=1787939059)；印刷38；PDF文件39 | 印刷38；新建事件的名称、全天、起止、备注、Cancel/Confirm；已有事件编辑/删除入口。未触发取消。；[摘页](../artifacts/office-aipaper-deepening/Viwoods-AiPaper-manual-file-39.png) |
| E-VICD-05 | [Daily外部日历同步](https://cdn.shopify.com/s/files/1/0729/7368/0674/files/viwoods-AiPaper-User-Manual.pdf?v=1787939059)；印刷39；PDF文件40 | 印刷39；Outlook/Google绑定与手动同步入口；解除绑定仅保留本地创建事件是手册规则，未实测。；[摘页](../artifacts/office-aipaper-deepening/Viwoods-AiPaper-manual-file-40.png) |
| E-VICD-06 | [第三方云登录与导入](https://cdn.shopify.com/s/files/1/0729/7368/0674/files/viwoods-AiPaper-User-Manual.pdf?v=1787939059)；印刷52；PDF文件53 | 印刷52；首个账号默认同步账号，可更改；长按文件Import并选设备目录。；[摘页](../artifacts/office-aipaper-deepening/Viwoods-AiPaper-manual-file-53.png) |
| E-VICD-07 | [云导出格式与回导限制](https://cdn.shopify.com/s/files/1/0729/7368/0674/files/viwoods-AiPaper-User-Manual.pdf?v=1787939059)；印刷53；PDF文件54 | 印刷53；PDF/note格式和目标目录；note仅AiPaper可打开编辑且各应用格式不互通。不是所有文件均可恢复原生编辑状态。；[摘页](../artifacts/office-aipaper-deepening/Viwoods-AiPaper-manual-file-54.png) |
| E-VICD-08 | [第三方云同步设置](https://cdn.shopify.com/s/files/1/0729/7368/0674/files/viwoods-AiPaper-User-Manual.pdf?v=1787939059)；印刷54；PDF文件55 | 印刷54；主页短按同步、长按云界面，Settings选择账号/目标目录/格式；没有双向冲突或删除传播规则。；[摘页](../artifacts/office-aipaper-deepening/Viwoods-AiPaper-manual-file-55.png) |
