# M-007 Viwoods AiPaper：AI预览、确认与数据去向

## 范围与版本

初查2026-10-26；2026-10-27复核手册入口/版本页并逐页看历史控件。固定 **10.65英寸 AiPaper、Android 13、英文国际版系统 V3.15 文档基线**，不含 Mini、Reader 或 Reader C；实际设备构建与AI服务配置未知。官网Update导航本轮仍指V3.15；V3.15、V3.14发布记录为版本基线，早期手册/2025年文章仅补明确路径并保留版本差异。[E-VIW-01～06]

D1+D2资料研究；没有登录、安装、亲自操作或连续视频。2026-10-26沿官网AiPaper英文链接取得70页原PDF并归档7页；2026-10-27重新GET得到相同散列，逐页实看文件2–69并新增13个关键摘页；web因文件过大未解析，普通HTTP200取得。PDF元数据2025-04-30不当版本号；SHA256与原镜像URL标识相同，因此补的是原件取得与图像证据，不是新手册。编码异常处凭图核读，不猜按钮。[E-VICD-01～08] **V3.8.5是网址/页面标题，正文自称Version3.8**；因此只作历史增量证据，不能推定精确构建。[E-VIW-07、11、12]

另取得官网所链75页中文手册，封面修订2025-06-11；PDF元数据2026-09-17不作新固件日期。75页文本已读、29页静态图已看，19个必要摘页归档；邮箱图中的1.9.74仅是应用号。本补充不替代上面的英文国际版/V3.15基线，也不扩展至Reader。见[中文源页与状态摘记](../artifacts/viwoods-chinese-workflows/source-notes.md)、[页台账](../batches/viwoods-chinese-workflows-inventory.csv)。[E-VWCN-01]

## 功能目录与已查/待查

| 域 | 已查内容 | 尚未闭环 |
| --- | --- | --- |
| 系统 | Home/任务管理、控制中心、历史三种刷新模式与V3.12按App作用域、应用/网络故障排查 | 首次向导、全部手势默认值、指纹失败、锁屏录音 |
| Paper/Meeting | 选区、标记/大纲、录音转写、AI重复处理、输出格式 | 新建到落盘全过程、转写错误定位原声、AI回写原稿及其撤销；Paper基本撤销见F18 |
| Learning | 选词AI、批注/高亮、页/章导航、离线英语词典 | 扫描件OCR、TTS和音频位置、跨窗口返回锚点 |
| Knowledge Base | Picking旧入口、来源app命名、OCR后共享/AI | 引文页码/原链接长期保留、删除来源后的关系 |
| Daily/Memo | 待办、日历、笔记联动、V3.15多页Memo | 日历双向冲突、提醒后台权限、删除恢复条件 |
| 文件/云 | ViTransfer、第三方云、V3.15原生云开关、账号分离 | 同名覆盖、离线删除传播、回收站时限当前适用性 |
| Mailbox | 中文历史账户/筛选、手写/文本、AI预览→回编辑器、签名入口 | 草稿持久化、取消/发送失败、投递回执与当前构建 |

历史手册目录索引：Quick Start p2；Input p16；Paper p17；Learning p27；Meeting p34；Daily p37；Picking p41；Encryption p46；Apps p47；AI p48；Files p51；Third-party Cloud p54；Mailbox p55；ViTransfer p62；Share p66；Memo p67。此前按实际查看的目录建立[17章台账](../inventories/M007-historical-manual-inventory.csv)；只表示历史章节范围，不是当前全部菜单或独立功能总数。[E-VIW-12；E-VICD-02]

本轮新增[逐章/页/控件台账](../batches/viwoods-controls-deepening-inventory.csv)，含历史功能页与现行官方增量；父子目录不作任务数。原PDF文件2为印刷1，正文文件3–69为印刷2–68；封面及联系页未做图像观察。全部静态观察索引见[观察审计](../artifacts/viwoods-controls-deepening/observed-pages.json)。字体偏移转换仅定位，正式控件逐页看图核对。

## 任务流程

表中“反馈”均依文档；静态图已查看的条目列相应原件或衍生E编号，但没有执行其动作。

| F 编号 | 任务 | 前置状态 | 操作与官方反馈 | 数据变化与结束 | E 证据 | 尚未核实 |
| --- | --- | --- | --- | --- | --- | --- |
| F-M007-01 | 从正文进入系统控制 | 历史手册/发布说明，app打开 | 右上下拉控制中心；左上向下为全刷，Home长按为任务管理。V3.12 App Display可选独立刷新或跟随系统。 | 显示配置与关闭进程分开；不是性能或持久化实测。 | E-VIW-08/09；E-VICD-01；E-VWCD-01 | V3.15具体页面、保存/重启保持、关闭前落盘与音频恢复。 |
| F-M007-02 | 从书内选段问AI | 历史Learning，PDF/EPUB可选文字；AI需网络 | 长按或Selection→AI带入选区；同栏可翻译/词典/复制/TXT分享/高亮。 | 高亮归入Reading Notes；Back独立入口不证明对话返回锚点。 | E-VIW-06/12；E-VWCD-08/09 | 扫描件、长文本截断、关闭AI后的选区/页/滚动保持。 |
| F-M007-03 | 设置重复使用的AI指令 | 历史AI配置 | 命令名称/指令→Confirm；长按可删除。 | 中文历史p55也保留正文5条、截图4条冲突，不设统一上限。 | E-VIW-06；E-VWCD-13；E-VWCN-01 | V3.15限制/编辑入口、删除确认及改指令对旧结果影响。 |
| F-M007-04 | 语音提问并带走结果 | 历史入口；联网 | 长按AI最长15秒，松开自动提交；短按键盘；TXT/HTML导出。 | 历史24小时内可续聊不等于之后自动删档；短语音不同于长会议录音。 | E-VIW-06；E-VICD-01；E-VWCD-13 | 误松取消/重试、现版历史保留；“AI commands不自动保存”对象含义未解。 |
| F-M007-05 | 录音后转写、改结果再输出 | Paper/Meeting；V3.14+；联网AI | 单段上限60分钟；完成后转写、润色/摘要。对结果重新选段、再运行命令；TXT/PDF导出。 | 已证实可连续处理结果；回写原笔记的方式仍未知。 | E-VIW-03 | 开始/停止精确按钮、录音自动分段、原音保留、纠错回听、断网续传。 |
| F-M007-06 | 区分录音数据与显示状态 | Paper/Meeting已有录音数据 | 录音显示开关可隐藏浮窗；侧栏显示当前录音段名，文本可Copy All；用专用滚动条阅读转写。 | 隐藏窗口不应被解释为停止或删除；导出文本不是音频备份。 | E-VIW-04 | 隐藏时是否持续录音、锁屏/切app、后台杀进程后的恢复，均待测。 |
| F-M007-07 | 从长笔记找回标记位置 | Paper内容存在；版本分别列明 | V3.14圈选标记/大纲；旧标签侧栏检索、长按改名/删除；Daily按关联笔记名回建立关联时的页。 | V3.15新增大纲子层、整本复制、跨笔记移页为功能增量，缺操作确认链。 | E-VIW-03/04/02；E-VICD-01 | 重排/移页后锚点、同名、撤销恢复；不套Picking移页菜单。 |
| F-M007-08 | 提取其他app内容并继续AI | 旧Picking与新Knowledge Base分开 | 旧Picking下拉/浮钮等入口沿用；历史标注模式启用锁页、停用冻结可再启，Save另列。V3.12重组PickNotes/VisualNotes、先注释后裁剪保存。 | V3.14裁剪外取消/OCR动作与V3.13来源命名按各版记录。 | E-VIW-07/04/05；E-VICD-01；E-VWCD-01 | 原URL/页码、取消持久化、AI图像/OCR输入、重复去重。 |
| F-M007-09 | 从阅读导航回到证据 | Learning已有书，历史手册 | 底部五分一区进度/跳章；Reading Notes选一/多/全部批注页汇PDF；书库长按Rename/Delete。 | 当前书/全部书显示范围可选；图中已选值非默认。V3.13离线英语词典另保留。 | E-VIW-12/05；E-VICD-01；E-VWCD-09/10 | 跳转历史、删书标注/回收期限、PDF导出后原生可编辑性；不套Memo30天。 |
| F-M007-10 | ViTransfer收发、跨设备传递与局域网入口 | ViTransfer同账号；WLAN Transfer另需同局域网 | 中文p69–71：网页Import/手机+→设备Receive；A导出→同账号网页选文件及目标B→Sync→B再Receive。独立局域网入口用页面地址传输。 | 中文用my.viwoods.com；旧英文图mytest差异保留，未访问测试域名。手工传递不当双向合并/历史备份。 | E-VIW-12；E-VICD-01；E-VWCD-02；E-VWCN-01 | 重名、断线/取消/满盘、实际收发完成和文件完整性。 |
| F-M007-11 | 选择需要同步的内容（D1） | AiPaper V3.15；账号与网络 | 原生云默认关闭，启用10GB；按App控制，Learning选书上传。 | 账号内容分离；V3.15 Paper/Meeting共用笔记密码，区别旧独立密码；Advanced Settings权限仅功能声明。 | E-VIW-02/10；E-VWCD-11 | 同步/权限完整菜单、密码迁移/找回、离线删除、云满与切号本地可见性。 |
| F-M007-12 | 把手写/日程推进为待办（D1） | 相关内容存在 | V3.13圈选手写建待办；V3.14允许补充、批建、按完成状态筛选；V3.15 Memo支持多页和手写待办。 | 待办是另一对象，不能推定会改写原手写。 | E-VIW-05；E-VIW-03；E-VIW-02 | 完整创建/完成/误删恢复、跨日迁移和日历同步冲突。 |
| F-M007-13 | 网络/Google服务故障恢复 | 功能联网/授权异常 | 沿官网WLAN遗忘重连、Play Store服务恢复。旧手册App长按清理/卸载/添加主页。 | 中文p52也出现正文清缓存、截图Clear Data冲突；没有执行清理。 | E-VIW-09；E-VICD-01；E-VWCD-12；E-VWCN-01 | 登录/笔记保留、卸载重装、AI续跑及计费重试均未知，不保证清理无损。 |
| F-M007-14 | 手动、语音或圈选创建并确认Daily事件 | 历史手册；Daily或已有笔记选区 | 手动＋填字段；中文p42长按＋语音、p43套索→AI创建事件，均先到可编辑标题/全天/起止/备注，再Confirm生成事件示意。已有事件可编辑/删除。 | Cancel可见未执行；事件与F12手写待办不同。图示确认不证明真实写盘。 | E-VICD-01；E-VICD-03；E-VICD-04；E-VWCN-01 | 多日期/相对时间消歧、取消、重复提交、时区/冲突、保存/同步/提醒与删除恢复。 |
| F-M007-15 | 绑定日历并处理解除绑定 | 历史手册；Outlook或Google账号 | Daily设置→选择账号授权；页面有手动同步入口。手册称事件增删改自动同步，解绑后设备只保留本地创建事件。 | 企业Outlook需管理员授权；失败建议重试。此解绑规则不代表原生Viwoods Cloud数据规则。 | E-VICD-01；E-VICD-05 | 授权回调、冲突优先级、离线删除传播、解绑前提示及远端是否保留。 |
| F-M007-16 | 从第三方云或本地导入/导出笔记 | 历史Viwoods Files APK；云盘需授权 | 中文p57 My Notes长选Export→格式与目标；p58 My Files长选Import→My Notes/CloudDrive；p59云文件Import→Learning目录。 | 本地操作不同于旧英文开发中说明；p58目标标题写exported而按钮Import，保留冲突。note只限AiPaper且各应用不互通。 | E-VICD-01；E-VICD-06；E-VICD-07；E-VWCN-01 | PDF/note信息损失、同名覆盖、媒体随包、实际导入/回导及重装恢复。 |
| F-M007-17 | 设置第三方云账号、周期、对象与格式 | 中文历史p60；已授权第三方云账号 | 首页短按同步；长按云界面→Settings，选账号、Auto Sync、7天/3天/24小时、六应用目录及格式。 | 正文note默认不可取消、PDF可选；图中7天不证明默认。第三方云与V3.15原生云分别研究。 | E-VICD-01；E-VICD-08；E-VWCN-01 | 周期起点、休眠/断网补跑、进度/失败/取消、删除传播、云满/切号和历史恢复。 |


## Paper编辑、快捷目录与Memo删除

以下三个新任务仅由历史手册重建，D2不代表实操或V3.15当前菜单验证。

| F | 操作与条件 | 数据/退出边界 | 证据/缺口 |
| --- | --- | --- | --- |
| F-M007-18 Paper编辑选区并撤销 | 套索编辑/撤销原路径保持；中文p26笔迹复制限同应用笔记间，p27文字可跨模块粘贴，空剪贴板入口灰。 | 笔迹与文字是不同对象；返回Paper是独立按钮，两指动作处于撤销节。 | E-VWCD-06/07、E-VICD-01、E-VWCN-01；撤销层数、保存、层/模板与重开保持未知。 |
| F-M007-19 快捷模板与关联目录 | Quick Note编辑→新增→名称/模板→Done；中文p23补使用既有普通目录名建立关联。 | 删快捷入口使目录普通化；删关联目录移除快捷入口。不能推定两种操作都删笔记。 | E-VWCD-04/05、E-VICD-01、E-VWCN-01；多个同名目录冲突、内容去向、撤销/当前上限未知。 |
| F-M007-20 Memo删除与回收 | 长按Memo→Delete；Recycle Bin入口可见，正文称最多保留30天、可恢复或永久删。 | 期限只限历史Memo，不套Paper、Meeting、Learning或系统文件。 | E-VWCD-14；恢复具体按钮/确认、位置/提醒重建、到期与云删除规则未知。 |

Paper文件删除到回收站有独立说明，但该页没有期限；Learning长按删书只有入口。V3.15跨笔记移页/整本复制缺明确目标选择与完成回执，仍只作功能增量。多用户权限、锁屏密码、笔记密码、云账号及邮箱授权分别研究，不能推成统一数据隔离保证。

## 邮箱与Memo提醒的历史任务

以下为E-VWCN-01中文手册补充的D2任务；来源页与控件可从[交互依据表](../reference-points.csv)筛选M-007。没有真实登录、发送或提醒触发。

| F | 前置与步骤 | 可见结果与未知 |
| --- | --- | --- |
| F-M007-21 设置邮箱并切换/筛选 | p61–64：Google/Outlook或手动IMAP/POP3→SMTP；账号下拉、文件夹/日期/分页设置 | 正文最多5账号；20–1000是单文件夹显示条数，不是存储配额。授权、清缓存/删账号结果及恢复未知 |
| F-M007-22 手写/文本邮件与AI预览 | p65–67：填写To/CC/BCC/主题，手写/文本及附件；手写→AI生成→预览→复制回编辑器→另有Send；签名可从写信或设置编辑 | 生成前已有收件地址，不能说AI自动选收件人。预览取消、替换范围、草稿/签名保存、失败重试和投递回执未知 |
| F-M007-23 设置Memo提醒 | p73–74：打开便笺→时钟→提醒开关/日期/时刻→取消/确认；长选工具栏另有提醒入口 | 到点、锁屏/后台、离线、删除与恢复后提醒均未知；不能用F20回收规则推定提醒也恢复 |

## 版本差异、错误与数据边界

| 分支 | 已有依据 | 未知/最小核查 |
| --- | --- | --- |
| 旧资料称无需账号/本地会话 | 2025AI文章描述基础AI入口；后续Web Search试用要登录，V3.15云同步也依赖账号。[E-VIW-06、07、02] | 不把“无需账号”扩大到全部服务；当前AI配额/付费方案未查明。 |
| 断网 | 当前FAQ明确本地读写可用、AI请求需网络；离线英语词典是独立例外。[E-VIW-10、05] | 录音后转写断线是否排队、重试是否生成重复结果。 |
| USB无法访问 | V3.14在已设锁屏密码且休眠锁定时阻断USB文件访问。[E-VIW-04] | 解锁后重连是否恢复；该保护不能推定所有分享出口也受控。 |
| 音频无声/蓝牙断开 | 当前FAQ说明本机没有内置扬声器，音频使用蓝牙外设。[E-VIW-10] | 断连后录音/播放状态及重连恢复点；不能借Reader的蜂窝能力补AiPaper。 |
| 修改、取消和删除 | 历史Daily事件可编辑/删除；解除外部日历绑定仅保留本地创建事件。[E-VICD-04/05] | 事件、手写任务、日历笔记与云笔记是不同对象；不把解绑规则套到全部内容，历史Memo最多30天见F20；其他对象期限仍未核。 |
| AI生成内容有误 | V3.14允许结果再选段重复命令。 | “改指令→新结果”是否保留上次结果、是否可回原音逐字核对、是否原位覆写。 |
| 同步后内容缺少 | V3.15修复Daily导出/同步笔记格式丢待办内容的已知问题。[E-VIW-02] | 修复说明不是本轮复现通过；需要旧/新同一测试笔记核验。 |

历史手册File Encryption（印刷46）称Paper/Meeting密码各自管理且本地保存；V3.15发布页改为共用笔记密码、Settings→Security Settings。保留两期，迁移/找回未核。Apps清缓存/Clear Data、AI命令5/4条的文图差异亦保留，详见[本批独立摘记](../artifacts/viwoods-controls-deepening/source-notes.md)。

## 实际观察与项目关系

本轮补原PDF静态观察，图片中的按钮未实际触发；没有测延迟、残影、60分钟录音或AI准确率。历史手册不含V3.15原生云说明，F11仍D1；Daily图可见任务框，但不能证明手写待办创建/完成闭环，F12也保持D1。三种刷新模式的宣传速度不构成R-011的60fps/无残影证据。

以下为推断：对G-002，最有价值的是“用户选定输入范围→执行具名指令→检查结果→再次处理→显式导出”；仍缺原音、识别稿、摘要、笔记与任务之间的可追溯关系。Plus可参考固定AI入口和选区操作，但大屏套索/多层面板不宜直接缩到掌上。对成人研究，来源锚点比模型数量更关键；对5–6岁读听，AI短语音入口与长录音必须明显区分，R-018仍需单独验证书/音频断点。

后续最小观察：固定V3.15实际构建，完成一段短会议的录音→断网→重连→转写纠错→摘要→改指令→回原页→导出，再删除一个结果并观察其他对象。另测两个账号和重复文件，记录对象是否新增或替换。

## 证据

初批查于2026-10-26；本轮手册/版本/FAQ复读与静态观察为2026-10-27。版本文章来源日期未明者不推算；旧12为文字镜像证据，新VICD为原件与实际看过的静态页。

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

### 2026-10-26原手册与静态页

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

### 2026-10-27控件补证

E-VWCD-01为[官方V3.12增量](https://viwoods.com/pages/system-update-3-12)，E-VWCD-02—14为同一历史PDF的13张衍生静态摘页，不是13份独立来源。原件和观察边界见[独立摘记](../artifacts/viwoods-controls-deepening/source-notes.md)，逐页/编号见[观察审计](../artifacts/viwoods-controls-deepening/observed-pages.json)。官网英文手册本次仍为旧字节；未证更高固件，不把网页当前日期当旧手册修订。

### 中文历史手册新增来源

E-VWCN-01：[官方中文PDF](https://cdn.shopify.com/s/files/1/0729/7368/0674/files/User_Manual_CN-0305.pdf?v=1789609108)，封面修订2025-06-11。具体字段、状态、文图冲突及排除的Reader/未观看视频路线见[源页摘记](../artifacts/viwoods-chinese-workflows/source-notes.md)。同一原件只新增一个E，多页观察不当独立交叉验证；E-VIW-11追加官方入口复核，不覆盖旧获取记录。
