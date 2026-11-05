# 第十七轮 C2 路：模块划分、AI 原生与文档审计

完成日期：2026-11-05。对象：`0eb64d4ee52b540eadfa1637d0e896fbb8e87ab0`。按 [TASK.md](TASK.md) 独立完成，只读产品代码和既有资料，运行既有测试，没有编写实验程序。

## 一页结论

**有条件可行：模块划分总体符合“软总线 + 原子能力”，可以沿当前方向继续开发；当前证据不足以验收“AI 看得到所有输入状态、异步编辑总能作用于原目标、关机提示必然代表保存完成”。**

本轮新问题 **4 条：严重 0、高 2、中 1、低 1**。均由代码/文档核查得出，错误场景的实际后果标为推断，没有把既有测试通过冒充专项复现。

| 编号 | 级别 | 结论 | 应优先闭合的契约 |
| --- | --- | --- | --- |
| AUD18-401 | 高 | 文本工具只操作执行时的焦点；异步请求没有目标会话和编辑版本 | AI 编辑绑定目标与版本，过期请求明确失败 |
| AUD18-403 | 高 | 低电量保存超时和保存成功进入同一呈现接口，都显示“东西都存好了” | 保存结果由电源服务传给关机图；文档不能承诺超时路径也全部保存 |
| AUD18-402 | 中 | 普通/数字键盘没有接入统一 AI 快照；另查 `text.state` 也拿不到全部面板语义 | 为非模态输入面板补充语义上下文 |
| AUD18-404 | 低 | 能力总表漏了任务、编辑等已实现接口，测试核对的是源码中的另一份清单 | 同步文档，并写清测试覆盖范围 |

【代码核查】新增任务表没有变成第二套执行系统；音量键和 AI 共用 `system.set_volume`；文本服务管理编辑、壳负责键盘；关机图负责绘制、电源服务负责关机。升级和词库对固定系统资源使用 HAL，并配合持卡/存储任务，不应为了“走总线”改成经用户 `file.*` 工具反向访问。恢复出厂的机制集中在策略文件，可进一步抽成维护服务，但本轮没有仅凭文件位置把它判成缺陷。

【复现】普通构建、AddressSanitizer + UBSan 构建均 **85/85 通过**，配置、构建和 CTest 都退出 0。`integration-ai-surface` 两种构建各 `test.pass=1254`、`test.finished=1`。这证明现有回归通过，不证明本轮新增的契约问题不存在。

【代码核查】【复现】第十四轮 **AUD15-001～011 的原问题均能对应到修复和通过的既有回归**。架构节没有全部完成：事件成员归属已补上；原始来源链、调用者应用身份、远端 `needs` 和事件必达性仍有已知边界。详见 [verification.json](verification.json)，没有把这些旧事项重复列为新问题。

## 新问题

### AUD18-401【高】文本能力未绑定编辑会话和内容版本，迟到请求会落到当前字段

**证据【代码核查】**：

- `system/services/text/text_service.c:76–104` 切焦点创建新会话，`:95` 已有内部会话号；`system/services/text/text_caps.c:17–59` 的 `text.state` 不返回该会话号，也没有内容/编辑版本。
- `text_caps.c:62–92` 的插入、提交和 `:144–157` 的选择、撤销、重做，操作的都是执行当刻的全局输入会话。`:173–187` 的能力参数没有目标会话与预期版本，也没有绑定 `activity`。
- `system/policy/cap.c:133–134` 的 `page_gen` 校验只适用于有 `activity` 的能力，所以不能给这些全局文本工具补上目标约束。`system/ui/textfield.c:102–109` 切换焦点直接进入新输入会话。
- `text_service.c:171–182` 用当前光标/选区替换文字，`:340–349` 调当前输入框的提交回调；这些方法本身不接收原目标。

**影响【推断】**：同一成员让 AI 编辑一个普通输入框后，在请求回来前换到另一个普通输入框，旧请求仍可修改或提交新框。即使没换框，人继续改字或移动选区也会使原有字符位置失效；分开的 `text.select`、`text.insert` 之间同样存在这个窗口。可能出现非预期覆盖和提交，符合特定现实操作下的数据风险，定高。已有会话内撤销不能保证提交后或结束原会话后可恢复。本轮没有复现数据丢失。

**建议**：`text.state` 暴露稳定的字段/会话标识及编辑版本；AI 编辑携带预期目标和版本，在 UI 执行任务内原子校验，不匹配返回 `stale`。选区替换可作为一个原子编辑动作。即时键盘仍使用当前会话，不要求每个字符绕一遍远端能力调用。

**核查入口与现有测试边界**：沿 `text.state` → `text.select/insert/submit` → `ink_text_*` 阅读上述位置。`tests/integration/textinput.c:415–439` 验证同一会话内顺序选择、插入、撤销；`:398–402` 验证会话切换清历史，没有检查 AI 请求等待期间的同成员焦点/内容变化。未新增测试。

### AUD18-402【中】普通与数字键盘的可见状态未接入统一 AI 快照

**证据【代码核查】**：`system/ui/shell/keyboard.c:125–139` 在 `lv_layer_top()` 创建键盘，`:275–315` 显示组合串、候选、听写提示，`:317–319` 只登记 `presenter(sync)`；没有登记 overlay/snapshot。`:289–290` 表明数字、号码键盘也走这一路。`system/ui/ui_task.c:290–304` 只调用已登记顶层浮层或底下 activity 的快照；`system/services/ai/ai_service.c:471–498` 正是用它生成提问上下文。

`system/services/text/text_caps.c:17–59` 的 `text.state` 能另行读非敏感框的值、组合串、光标、选区、语音状态；**所以不是 AI 完全读不到文本**。但它没有当前候选页、`voice_notice`、动作键的完成/发送/搜索语义。`system/ui/textfield.c:168–172` 的“问 AI”直接把文字当问题，也没有补上这些面板信息。

**文档核查**：系统服务设计 `:60` 将输入框列入“有浮层时页面快照就是浮层上的字”的规则；当前输入面板尚不满足。这里核查的是已实现面板的语义接口，不是要求实现正式键盘外观。

**影响【推断】**：用户针对当前输入提示或候选问“为什么这样”“这个是什么意思”，自动上下文可能只有底下页面；单靠 `text.state` 也无法准确解释面板的当前提示。由于文本主体已有读取能力，按上下文缺口定中，不等同于数据丢失或密码内容应向 AI 开放。

**建议**：提供非模态输入面板的语义快照，并与页面组合：当前非敏感输入目标、值/选区、输入方式、提示及必要的候选语义。统一敏感字段遮蔽。不要只为快照把键盘改成拦住整页输入的模态卡片。

**现有测试边界**：`tests/integration/ai_surface.c:301–310` 的提问检查只确认 `page` 对象和部分系统状态存在，没有聚焦输入面板后的快照断言。本轮未增加实验。

### AUD18-403【高】关机呈现接口不携带保存结果，低电量超时仍显示全部已保存

**证据【代码核查】**：`system/services/power/power_service.c:48` 将低电量等待上限定为 5 秒；`:125–135` 启动 flush 后，条件是“数据和存储空闲 **或** 到截止时间”，两种结果均调用 `PS.off_picture("battery")`。`system/ui/shell/off_picture.c:57–59` 只拿原因字符串，`:49` 无条件写“没电了，东西都存好了。充上电再开机”，`:27–29` 也把同样的结论放进快照。

保存未完成的状态确实存在于当前设计：`system/services/data/datastore.c:288–310` 在追加失败时保留待写块并延迟重试，`:1019–1028` 在仍有待写块时返回非空闲。持续失败或超过截止时间的保存，与成功保存会进入相同的关机呈现路径。

**文档核查**：系统服务设计 `:829` 写“先把要写的写完，再画”，`ui-dependencies.md:168` 把“没电时先存再写”记为已做，没有说明低电量保存超时这一分支。按 TASK 对不实保证的口径，**错误的保存完成保证定高**；并非把低电量有界断电本身定成错误。

**影响【推断】**：当保存持续失败时，屏幕会长期保留“全已保存”的错误结论，用户无法据此知道有内容尚未确认落盘。本轮未模拟这个错误路径，也未声称已复现真实数据丢失。额外等待屏幕的时间可能让部分写入完成，但接口没有据此重新确认，不能作为完成保证。

**建议**：电源服务传递结构化的保存结果（完成、超时、失败或未确认），壳按结果显示；没有完成证据时使用准确或中性的文案。保留低电量有界关机策略，保存判断继续留在服务层。

**现有测试边界**：`tests/integration/power_menu.c:184–200` 覆盖正常存储下低电量关机、关机图和重启留存；没有验证保存未完成时的显示。它的通过不能支持上述无条件保证。

### AUD18-404【低】能力总表遗漏已开放的任务、编辑等接口，测试核对的是另一份手工表

**证据【文档核查】【代码核查】**：系统服务设计 §1.3（`:69–83`）没有任务行，也未列下列当前已开放接口：

| 应归入的位置 | 漏项 | 实现/测试证据 |
| --- | --- | --- |
| 后台任务 | `job.list`、`job.cancel`、`job.running`、`job.finished` | `system/services/res/task.c:227–239`，设计 §13.1 本身已写明 |
| 文本输入 | `text.select`、`text.undo`、`text.redo` | `system/services/text/text_caps.c:173–180`，设计 §9.4.1 已写明 |
| 系统、通知 | `system.set_volume`、`interrupt.list`、`interrupt.ack` | `system/policy/system_actions.c:246`；`tests/integration/ai_surface.c:31、39` |

设计 `:67` 称测试逐项核对“这张表”。实际 `ai_surface.c:29–74、157–184` 校验其源码内 `FOR_AI`、`NOT_FOR_AI` 等表，不读取设计文档。当前运行时和测试表已更新，所以测试可以全过、文档总表仍缺项。

**影响**：维护者按总表判断能力开放程度会遗漏已完成接口。不是运行时缺工具。旧阅读/通信等域不在本系统服务表主题内的差异，没有全部算作漏项。

**建议与核查入口**：补全任务行及上述接口，明确“运行时由测试表校验，文档另行同步”；或从同一登记数据生成审计清单。比较结果见 [surface-comparison.json](experiments/surface-comparison.json)，仅为源码名称比较，没有新增抓包程序。

## 模块边界与四类总线对象

以下为【代码核查】【文档核查】，测试佐证另列。判定依据为系统架构 §4、§5，尤其 §4.6 允许类型化快捷接口；调用 HAL 或普通 C 函数本身不是违反软总线。

| 模块/链路 | 当前职责与边界 | 判断 |
| --- | --- | --- |
| `res/task.c` | `ink_task_begin/progress/end` 保存 16 条展示记录；`job.list/cancel` 是能力，`job.running` 是状态，`job.finished` 是事件；真正的工作仍由文件服务和下载作业负责。表在 PSRAM、共用服务锁，无新专用任务。列表按成员过滤，共享更新由成人取消（`:60、95、130、200`）。 | 划分合理。展示任务与调度作业分开有必要，不建议合并成一个万能作业系统。当前只在内存保留、不跨重启，是 §13.1 明示的范围。 |
| `off_picture.c` → power | 壳消费关机原因，绘制和等待屏幕完成；电源服务发起 flush 并决定何时断电。书签从应用卡片读取，仅空间开放时添加（`:34–62`）。 | 主方向合理；服务到呈现的结果契约不完整，见 403。 |
| 音量键 → capability → setting | `system_actions.c:54–73` 汇总按键步数后调用 `system.set_volume`；`:41–45` 再进入设置服务。设置 `audio.volume` 持久保存，能力是调节入口，`audio.now_playing` 是只读状态。 | 人与 AI 共用 provider。锁定且在放声音才响应实体键，`while_locked` 没有把关闭空间开放给 AI。无需为键盘扫描再加 AI 工具。 |
| 恢复出厂 | `system_actions.c:96–214` 包含记号、启动恢复、晚到卡清理和通知；内部 Flash 操作经 `ink_storage_run`，卡清理交给文件服务。`system/boot/main.c:58–65` 在成员/应用等服务启动前恢复；显示与存储基础服务已先启动。 | 不属于页面实现。策略入口与维护机制放在同文件，建议后续拆 `maintenance/reset` 服务，入口仍是 `system.factory_reset`。仅文件组织混合不单列缺陷。未把文档已披露的晚到卡语义当新发现。 |
| 文本服务、控件、键盘 | `text_service.c` 管会话/编辑历史；`text_caps.c` 是工具适配；`pinyin.c` 管词库；`voice_input.c` 管听写；`textfield.c` 管焦点和呈现；`keyboard.c` 是壳。人和 AI 最终进入同一编辑实现。 | 职责清楚；工具目标绑定与快照接入有 401、402。逐字符操作可用类型化接口，不必把所有 UI 事件序列化为工具调用。 |
| 词库 → HAL | `pinyin.c:86–109` 在后台持卡后读固定系统词库，校验/分配失败退回小词表并释放卡。 | 是系统资源加载机制，不是用户文件浏览能力；不应经 `file.read` 的文本分页接口加载二进制词库。 |
| 升级 → HAL/storage | `update_service.c:458、647、676、813、916` 等检查/下载/安装/补资源路径持卡；Flash 槽写入 `:326、353`、资源写入 `:889` 经 `ink_storage_run`，`storage_service.c:421–429` 承担 Flash 守卫。 | 固定系统包路径和 HAL 访问合理；保留文件服务的卡互斥、存储任务的 Flash 执行边界。真正中枢协议与硬件 OTA 不在本次结论内。 |
| data、time、notify、confirm、app | data→storage 串行持久化；time→RTC/power 是下层机制；通信/时间事件→notify→打断；确认在策略层；app 生命周期在 UI 任务串行，存储仍交给 storage。 | 与第十四轮判断一致。查询/动作走能力，事实走状态，持久偏好走设置，变化走事件；锁、焦点、RTC 寄存器和日志压缩无需成为独立 AI 工具。 |

【复现】`layering`、`sdk-boundary` 两种构建通过。【代码核查】前者（`tests/layering.cmake:4–24`）只扫描 `system/services`，对旧 AI 面板文件和 executor 明确例外；后者（`tests/sdk_boundary.cmake:3–24`）扫描 `apps/`，并不等于扫描所有系统应用。这些是静态依赖约束，不能证明所有间接依赖、事务和跨任务状态正确。

## AI 能力开放与观察范围

### 清单、结构化结果、仅真人能力

【复现】【代码核查】本次使用原 `integration-ai-surface`，其 `fetch_tools` 经模拟中枢分页路径获取工具，核对登记表。没有改测试抓出另一份运行时清单，也没有真实模型会话。源码期望表为 **108 个 AI call + 6 个人类专用 call**；本次所有相应断言通过。108 中 104 个全局 call 检查实际返回的名称、标题和必需参数；4 个 `book` 专属 call（前后翻页、跳页、读页）在这个测试中只检查本地声明，没有打开阅读页重新拉表。事件与 call 分开，不把 108 写成全部工具/事件条目数。

【代码核查】新服务的读取入口，包括 `job.list`、`text.state`、`data.status`、`media.status`、`update.status`，使用结构化 `ink_cap_result`；不只返回一句话。已有大记录 JSON 回归覆盖第十四轮的截断问题。但部分 `outputSchema` 仍只是宽泛 `object/array`，现有清单测试不等于每个返回字段的语义、完整性都验证了。

| 真人专用能力 | 理由与接口边界 | 当前证据 |
| --- | --- | --- |
| `member.unlock`、`member.set_pin` | PIN 在设备输入；AI 可以查询成员和认证状态、请求切换，但不应成为 PIN 的传输中介。 | `system/policy/members.c:595–601` 标 OWNER；`ai_surface.c:59–60` 明列例外。 |
| `confirm.accept`、`confirm.reject` | 确认是人的决定；AI 可以看待确认清单、撤回自己发起的请求、接收结果。 | `system/policy/confirm.c:508–509`、`ai_surface.c:57–58`。 |
| `system.upgrade`、`system.upgrade_from_card` | 安装期间设备不可用并重启；本轮依据现有规则由人启动。AI 仍可查状态、检查和下载，相关副作用依现行策略处理。 | `system/services/update/update_service.c:1017–1019` 标 OWNER、`ask_by_hand`；`ai_surface.c:61–62`。 |

【文档核查】以上六项的理由在当前规则下成立，不是忘记开放。D-105 新确认规则明确未落地，按 TASK 排除，不因代码仍执行旧规则新增问题。

### 页面和浮层

下表是【代码核查】的语义可见性，不把“有回调”扩大成“任何锁定/安装/断电时都能启动 AI 对话”。

| 界面 | 当前 AI 可读的内容 | 结论/位置 |
| --- | --- | --- |
| 活动页面 | activity 自己提供的纯文本快照 | `ui_task.c:298–304`；没有回调就是空，不自动遍历 LVGL 控件。 |
| 确认卡 | 标题、详情、确认/取消语义 | `confirm_card.c:58–68`，已接入；PIN 数字不输出。 |
| 打断卡 | 标题、正文、动作标签 | `interrupt_card.c:23–29`，已接入。 |
| 普通/数字/号码键盘与听写面板 | `text.state` 可另外读部分会话数据，面板自身无快照 | `keyboard.c:275–319`，见 402。 |
| PIN 数字键盘 | 由成员门/确认卡提供非敏感外层语义 | `member_gate.c:30–35`、`confirm_card.c:58`；不导出输入数字是合理例外，与普通数字输入框不同。 |
| 安装卡 | 正在安装及当前显示的百分比 | `update_card.c:15–20、41–46`；文字与按十个百分点节流的画面一致。服务还有 `update.status`。 |
| 家人选择/成员门 | 固定的“设备锁着，请选择家人或输入密码” | `member_gate.c:30–35`；只有概括语义，不包含当前选择/错误提示的完整逐项快照。空间关闭时 AI 调用仍受策略限制。 |
| 关机图 | 已关机、正文及允许保留的书签 | `off_picture.c:27–32、49–52`，已接入第 5 层；保存状态含 403，断电后不能对话。 |
| 电源菜单 | 关机、重启、取消 | `power_menu.c:17–24`，已接入。 |

### 事件、来源和撤销

【代码核查】【复现】事件登记和 AI 暴露规则接入转发；`cap.c:714–801` 保留最近 8 条、按成员转发并给本地订阅者补成员。`integration-ai-surface` 覆盖通知、确认结果和离线恢复，`integration-races` 覆盖成员变化期间的事件归属。没为转发新增专用任务，大缓冲在 PSRAM。资源不足、超出编码上限、离线超过 8 条仍有明示丢弃行为，不能说事件全部可靠送达。

【代码核查】数据/时间/通知的部分写入保留 `src`，对象级 `system.undo` 只对已登记 inverse、当前成员和有效窗口生效（`cap.c:951–1003`）。文字编辑另外有会话内最近 16 步历史，AI 和人的改动都能按该历史撤销。两者不是全局持久操作日志。确认后执行的来源变为 `CONFIRMED`，文字历史没有来源字段，`clipboard.c:45–46` 也不保留传入来源。故“谁发起、谁批准、谁执行”和“是否可撤销”必须逐类说清，不能用总线已有 `src/undo_ms` 推导全覆盖。

【文档核查】【代码核查】`page.actions` 的 768 字节限制是**已有 HUB2-006**。当前源码表中 104 个全局名字编码需 1521 字节（不含结尾零），不能装入该缓冲；但 [中枢设计 §7.4](../../hub/hub-architecture.md) 已规定空数组视为未知，分页拉 `tools/list`，不能解读成“没有工具”。真实中枢的 fallback 和设备协议改造不在本轮验收范围；不重复报告为新缺陷。

## 指定文档真实性

【文档核查】【代码核查】逐节核对结果如下；“符合”限当前模拟器接口/现有测试覆盖，不代表实机验收。

| 章节 | 核对结论 | 证据与限制 |
| --- | --- | --- |
| 服务设计 §9 文本、剪贴板、听写 | 部分符合 | 会话、UTF-8 字符位置、选择、16 步撤销、成员切换清理已有实现，`integration-text-main/nocard/baddict`、听写测试通过。中文词边界暂为两个字、正式词库/长文本翻页未完成已披露。新增异步编辑和快照问题见 401、402；没有把隐私相关所有动作的完备性宣称为本路验收结论。 |
| §10 音频 | 已检查的结构与现状说明相符 | 焦点是内部机制，媒体/录音是能力，`audio.mic` 是状态；既有 media/listening/dictation 测试通过。模拟器结果不能证明通话物理声音链路和芯片能力。 |
| §11 蓝牙、Wi-Fi、成员、恢复出厂 | 已检查的结构基本相符 | 协议未知/真机 HAL 缺失已说明；恢复记号、启动先清、晚到卡重试对应 `system_actions.c:96–214` 和 `integration-people-reset`。文档 `:739` 仍说四层浮层，新关机图已是第 5 层；应随总表一起更新。既有晚到卡风险已写明，不重复编号。 |
| §12 更新 | 已检查的分层与模拟器实现相符 | 持卡、存储任务写槽、安装卡、下载任务、人的安装入口均存在；`integration-update` 通过。真正 `update/*` 协议、硬件分区/回滚明确未验收。 |
| §13 系统动作、任务列表 | 部分符合 | 音量键走同一能力、任务只展示而不接管执行、16 条内存历史、文件/下载接入与代码相符。关机“先全部保存”的无条件说法不成立，见 403。 |
| §14 决定和硬件问题 | 边界表述合理 | 临时决定与待测项区分。未查询线上决策板、未读实机资料，也没有替用户重定 D-95。 |
| §15 验证记录 | 当前正向回归可核对；历史反向记录未重做 | 本次两套构建均 85/85。§15.1 的 72/72 明确是第一阶段历史数，不是当前总数；最紧 AI 场景内存数与本次一致。历史撤回修复的反向对照只是查阅记录，未据此声称本次复现。§15.4 的延期事项按 TASK 排除。 |
| `ui-dependencies` §4.1 | 基本相符 | 已明确“有”仅指模拟器；文件/文本/生命周期标部分，蓝牙标接口和模型，配对/备份标无，未冒充实机完成。新增 401、402 表明文本能力还有契约缺口。 |
| `ui-dependencies` §7 | 多数已做条目有代码对应 | 电源菜单、锁屏音量、麦克风状态、任务、`data.status`、文字编辑均有实现；保存只到本地、D-95/D-105 和同步后移已说明。关机保存措辞需要按 403 限定。 |

**内存证据【复现】**：本次 `ai_ask-metalio-plus-pqbn/metrics.json` 的 `mem.internal_total.min_free_bytes=33192`，离 32 KiB 底线 **424 字节**；与 §15.1 相符。`ai_surface` 为 34496 字节。它们是模拟器统计，不是 S31 栈或堆实测；没有据此承诺增加功能后仍满足预算。摘要见 [validation-summary.json](experiments/validation-summary.json)。

## 第十四轮修复核查

“已修复”表示原报告的具体原因已在当前源码处理，相应既有回归本次通过；不是重跑旧实验或完成所有错误注入。逐条证据在 [verification.json](verification.json)。

### 原编号问题

| 编号 | 状态 | 本次决定性代码证据 | 本次通过的相关既有测试 |
| --- | --- | --- | --- |
| AUD15-001 | 已修复 | `storage/storage_service.c:624–642` 区分加载失败，`data/datastore.c:587、628` 拒绝按空集合打开 | `integration-data` |
| AUD15-002 | 已修复 | `file/file_service.c:429、821` 在移动/递归作业入口保留虚拟根 | `integration-files`、`integration-people` |
| AUD15-003 | 已修复 | `file/file_caps.c:151–153` 分别计算 UTF-8 游标和 EOF | `integration-files` |
| AUD15-004 | 已修复 | `data/data_caps.c:15–16、130–145` 按记录上限分配、整条分页 | `integration-data`、`integration-ai-surface` |
| AUD15-005 | 已修复 | `notify/notify_service.c:275、289–292` 写入和宣告绑定既定成员 | `integration-races` |
| AUD15-006 | 已修复 | `notify/interrupt.c:68–107` 单一应用者反复收敛 | `integration-races` |
| AUD15-007 | 已修复 | `system/policy/confirm.c:360` 传原成员；`cap.c:429` 执行前查成员 | `integration-races` |
| AUD15-008 | 已修复 | `time/time_service.c:437–455、676–677` 保存/重算计划 | `integration-time-reboot` |
| AUD15-009 | 已修复 | `ai_surface.c:134–154、169–173` 解析实际 `inputSchema` 必需参数 | `integration-ai-surface` |
| AUD15-010 | 已修复 | `system/ui/app_registry.c:88–105、121–127、324–334` 保存入队失败时不停止/冻结 | `integration-apps` |
| AUD15-011 | 已修复 | `power/power_service.c:168–174` 锁定忙碌分支有界采样 | `integration-notify` 及低电量测试 |

表内服务相对路径均位于 `system/services/`；测试路径位于 `tests/integration/`。两种构建均覆盖这些测试。

### 架构节后续事项

这些在原报告架构节没有独立编号，`R14-ARCH-*` 仅为本次核查锚点，不是新增缺陷编号。

| 核查锚点 | 状态 | 当前结论与证据 |
| --- | --- | --- |
| R14-ARCH-SOURCE | 未修复 | 待确认表/列表保留发起来源，但执行仍改为 `CONFIRMED`（`confirm.c:360`），计数只分 ai/user（`cap.c:426`），结果事件没有原始来源（`confirm.c:146–165`）。`docs/audits/README.md:70` 所说“在结果事件里”不符合当前代码，不能以此认定来源链已完整。 |
| R14-ARCH-APP-CALLER | 未修复 | 应用权限仍按当前前台应用查询（`cap.c:123–125`），§15.4 明确后移。 |
| R14-ARCH-NEEDS | 未修复 | `app_registry.c:298–302` 仍只查 `ink_cap_find`，没有用可覆盖远端的 `ink_cap_available`。 |
| R14-ARCH-EVENT-MEMBER | 已修复 | 事件缓存/信封/订阅者都带成员，并在转发时判断（`cap.c:714–798`）；`races`/`media` 回归通过。`seq/boot` 仍后移。§15.4 前段把 member 混在待做列表中，后段已说明它完成，宜统一文字。 |
| R14-ARCH-EVENT-LOSS | 未修复 | 每订阅者分配、离线最近 8 条和编码上限仍在（`cap.c:727、735、787`）。这是先前已经披露的可靠性边界，不新增编号。 |
| R14-ARCH-SNAPSHOT | 部分修复 | 统一路由继续有效，安装卡、成员门、关机图已接；键盘仍缺，详见 402。 |
| R14-ARCH-UNDO | 部分修复 | 原有对象短时撤销继续有效，新增文字历史和 AI undo/redo；没有完整来源链和持久全局活动记录。D-105 部分不在本次范围。 |
| HUB2-006（相邻既有事项） | 部分修复 | 设备缓冲仍不足；中枢设计已定义空 actions 的发现回退。真实实现未查，设备改进后移，不列新问题。 |

## 对象、执行命令与未查范围

对象提交：`0eb64d4ee52b540eadfa1637d0e896fbb8e87ab0`，`2026-11-05T11:17:56+08:00`，`docs: part C of the audit again, structure only`。构建依赖只取自用户指定主检出。以下为执行过的命令，配置/构建输出和完整 CTest 摘要见 [experiments/README.md](experiments/README.md)。

```sh
D=<主检出目录>
cmake -S . -B build-audit -DINK_WERROR=ON -DINK_GUI=OFF \
  -DINK_CJK_FONT="$D/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$D/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$D/build/_deps/lvgl-src"
cmake --build build-audit -j 4
"$D/build-deps/pyenv/bin/ctest" --test-dir build-audit -j 4 --timeout 400 --output-on-failure

cmake -S . -B build-audit-asan -DINK_WERROR=ON -DINK_GUI=OFF -DINK_SANITIZE=ON \
  -DINK_CJK_FONT="$D/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$D/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$D/build/_deps/lvgl-src"
cmake --build build-audit-asan -j 4
"$D/build-deps/pyenv/bin/ctest" --test-dir build-audit-asan -j 4 --timeout 400 --output-on-failure
```

| 构建 | 配置 | 编译 | CTest | 总用时 |
| --- | --- | --- | --- | --- |
| 普通，Werror | 退出 0 | 退出 0 | 退出 0，85/85 | 30.10 s |
| ASan + UBSan，Werror | 退出 0 | 退出 0 | 退出 0，85/85 | 81.36 s |

**未查与证据边界**：未编写/运行新的实验程序，未修改源文件做反向对照；未穷举分配失败或竞态；未开 GUI 手工走查；未联网、未调用子代理或外部模型、未接真实中枢、未接触设备或读取 `ink-hw`。正式 UI、正式词库、`ports/esp32s31`、协议 v0.2 改造、D-105、配对、备份、无线传输和 D-95 计时遵循 TASK 排除，不记作本轮问题。也未查询线上 Issue/PR/决策板，避免违反本路离线约束。

**写入边界**：审计产物仅在本目录的三个主文件及 `experiments/`；构建产物在允许的 `build-audit/`、`build-audit-asan/`。开始时工作区已有 28 个旧审计实验文件删除和运行器 `codex-run.log`，均未修改或恢复。本轮未改产品代码、测试、旧文档和 Git 历史，未提交、推送或建分支。独立核查而非产品修复，文中建议均未实施。
