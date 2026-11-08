# phone-gap 检查报告

2026-11-08；基线 `291da6760931601e4783ebaddabc08e5bcab22eb`。结论：**尚未达到“没装应用的手机”标准**；已有较完整的模拟器服务底座，系统应用、统一应用框架、真实家庭服务器和整机端口仍有缺口。本任务只修改本报告，不删减功能，不修改需求、设计或 backlog。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| P-01 硬件取舍 | 已核查【实现】 | [硬件记录][hw]、[背景][bg]、[硬件对照][parity]；见下方硬件边界 | V1 |
| P-02 能力及设置逐项状态 | 已核查【实现】 | 79 项：有 13、部分 56、没有 4、未查到 6；覆盖代码中全部 16 个设置项 | V1、V2、V3 |
| P-03 backlog 归属、工作量 | 已列建议【估算】 | 使用 [backlog][backlog] 的真实任务名；未覆盖项单列，不视为已排期 | V1 |
| P-04 验证边界、依赖顺序 | 已列【实现／估算】 | 区分模拟器、历史局部硬件测量、真实服务器；末节列顺序 | V1、V2 |
| P-05 文件、链接及隐私 | 通过【实测】 | 仅本报告；118 个本地引用目标有效；空白检查通过；隐私扫描 0 匹配 | V3 |

V1：`taskpolicy -c utility rg --files system apps platform drivers boards ports tests hub docs/tasks`；配合 `rg -n`、`sed -n` 定位下表实现、任务书及文档。
V2：静态阅读表内测试文件及 `CMakeLists.txt` 的登记；**没有运行测试**，不把历史报告的“通过”写成本次实测。
V3【实测】：静态检查确认 79 行均有状态／证据／归属／估算、16 个设置逐项出现、backlog 名称有效、118 个引用文件及行号片段有效，文件小于 5 MB。`taskpolicy -c utility git diff 291da676..HEAD --check` 退出 0；同范围 `--stat` 只有本报告。按总规则 §6 原隐私正则扫描基线到 HEAD，0 匹配（grep 退出 1）。复核提交后再执行同样的差异与隐私检查。

本地引用目标可直接复核（文档检查，不运行产品测试）：

```sh
taskpolicy -c utility python3 - <<'PY'
from pathlib import Path
import re
p = Path("docs/tasks/phone-gap/REPORT.md")
links = re.findall(r"^\[[^\]]+\]:\s*(\S+)", p.read_text(), re.M)
assert len(links) == 118
assert all((p.parent / u.split("#")[0]).is_file() for u in links)
print("118 个本地引用目标有效")
PY
```

## 硬件边界与估算口径

- 【实现】按 [背景 §3][bg] 与 [硬件记录][hw]：S31 双核、16 MB PSRAM／Flash、4.7 英寸 1216×684 墨水屏、存储卡、NT26 4G、Wi-Fi、BH1098G 音频、摄像头、RTC、电量／充电芯片；[对照表][parity]还列出冷暖前光、触摸／按键、振动和加速度计。资料查阅日期为本报告日期，未重新查询上游或读取设备。
- 【实现】适配取向：沿用现有 HAL、软总线和 C 系统应用，用分页、局部刷新及明确完成回执；重模型、听写、语音合成、网页渲染属于家庭服务器。蓝牙按耳机／音箱／翻页器分别核查，不把耳机行为模型当芯片协议。未知硬件能力保留为“未查到”，不据此取消功能。
- 状态按本基线：“有”只表示所列子能力已有对应实现；“部分”同时写出缺口；“没有”有明确未实现记录或入口证据；检索不能确认的写“未查到”。下表代码与测试证据均为【实现】；除注明历史局部测量外，涉及硬件的功能均为**真机未验证**。没有在本次操作设备。
- 末列均为【估算】，单位为约一小时的有界任务；假设复用现有服务、每项含适量回归，不含等待硬件／付费授权。共用前置和同一页面任务有重叠，**不能逐行相加**。协议、端口和真实服务器未知项先用 1～2 项盘点，再估实现，不承诺一小时做完整子系统。0 表示所列模拟器子能力未发现新增工作，仍受整机验证边界限制。
- [新任务书][sem-task]记载用户已选框架五项推荐 A；本基线未含对应实现。[输入宿主][input-task]、[卡片][cards-task]明确依赖尚未合入的前序分支；这里只统计当前基线，不能据任务书认定已完成。

## 能力对照

表内 backlog 名均指 [backlog][backlog] 对应节；“未覆盖”是待主会话排期的建议。链接后的 L 为基线行号；同一行的测试仅证明存在相应覆盖代码。

| 能力 | 现在（有、部分、没有） | 证据（代码文件、测试、设计文档） | 归哪个 backlog 任务 | 缺的部分大约几个一小时任务 |
| --- | --- | --- | --- | --- |
| A01 整机 OSAL／HAL 端口与驱动装配 | 部分：有接口、芯片驱动及模拟端口，尚无系统真机端口 | [背景 §5][bg]；`ports/` 只有 `sim/`；[启动][boot]；真机未验证 | 未覆盖：建议真机端口与逐驱动集成 | 先 2 项盘点；实现待估 |
| A02 C 应用注册、权限、前后台、冻结与状态保存 | 有：已有基本 C 应用开发接口 | [注册与生命周期][apps]、[接口][app-api]；[应用测试][apps-test]；[服务设计 §8][svc-design] | app-framework-2（开发说明及示例收尾） | 1（说明与最小示例） |
| A03 主题、字库、图标和基础控件 | 有：卡片、按钮、开关、滑杆、分段及分页样式 | [主题][theme]、[主题接口][theme-api]；[界面测试][ui-test]；[界面规范 §1][ui-spec] | app-framework-2（复用现有组件） | 0（基础组件） |
| A04 通用列表／网格、分页及系统文件选择器 | 部分：壳内有分页，未查到通用列表和 FilePicker | [导航][nav] L333、L1078；[框架设计 §9 F4/F9][fw-design] | app-framework-2；前置 file-index | 3～5 |
| A05 控件语义、自动 AI 快照、对象与版本 | 部分：页面手写快照，缺通用控件语义合同 | [导航][nav] L1159、[远程][remote] L569；[任务 F1][sem-task]；[页面版本场景][page-test] | app-framework-2（已另发 ui-semantics） | 1～2（合入后复核） |
| A06 人与 AI 共用能力、权限和确认策略 | 部分：总线与策略已有，文本框菜单及网页点击仍直调 | [能力总线][cap] L82、[文本框][field] L152、[远程][remote] L424；[能力策略测试][cap-test] | capability-table；app-framework-2、remote-app 接入口 | 2～3 |
| A07 按成员的数据集合、版本、分页和本地落盘回执 | 有：可供系统应用复用 | [数据服务][data]、[数据能力][data-caps]；[数据测试][data-test]；[服务设计 §3][svc-design] | 系统应用复用；跨端回执见 B24 | 0（本地数据底座） |
| A08 换成员时收拢在途调用、事件与上传 | 部分：已有多处身份检查，集中收拢未实现 | [总线][cap]、[成员][members]；[member-quiesce 任务][quiesce-task] | member-quiesce | 1～2 |
| A09 确认事务及重启后可查询结果 | 部分：确认卡／校验已有，待确认表在 RAM，缺持久结果查询 | [确认][confirm] L24、L524；[确认测试][confirm-test]；[持久确认任务][durable-task] | confirm-durable；ai-pages 接界面 | 2～3 |
| A10 服务卡片、活动卡、主页编辑和图标菜单 | 部分：app.cards 已有；今日页多处固定空状态，未统一活动／编辑 | [应用目录][apps] L424、[导航][nav] L430；[卡片任务 F5][cards-task] | app-framework-1（ui-cards 及 F6/F8） | 3～5 |
| A11 后台任务进度、结果、取消与 Tasks 页 | 部分：job.list／cancel 已有，未查到 Tasks 页面 | [任务服务][jobs] L264；[服务设计 §13.1][svc-design]；[框架 F6][fw-design] | app-framework-1 | 1～2 |
| A12 全局搜索与回到结果位置 | 部分：仅 Apps 名称过滤，缺设置／联系人等跨域搜索 | [导航][nav] L264、L366；[界面测试][ui-test] L86；[框架 F7][fw-design] | app-framework-1；联系人结果依赖 phone-sms | 2～3 |
| A13 生成页面、受限脚本、应用包安装／更新／卸载 | 部分：有研究原型，未查到系统内可安装的完整链路 | [框架设计 §6.5/§9][fw-design]、[原型报告][spike]；[已定方向][sem-task] | app-framework-2 只覆盖部分；包生命周期未覆盖 | 先 1 项拆合同；实现待估 |
| B01 主页三屏、Dock、状态栏、控制中心 | 部分：壳与通知可用，多个图标没有应用内容，缺蓝牙／充电等状态展示 | [导航][nav] L182、L283、L693、L1290；[界面测试][ui-test] | app-framework-1；各系统应用；settings-all | 1～2（壳补接；应用另计） |
| B02 锁屏、时钟／通知、播放和图片锁屏 | 部分：这一页／分钟时钟及通知数已有，缺图片与播放锁屏 | [锁屏][lock] L30；[规范 §3.5][ui-spec]；[唤醒测试][wake-test]；真机未验证 | power-update-pages；kid-member | 2～3 |
| B03 家庭成员、密码、切换及找回／清空设备 | 部分：成员、密码、限速及重置服务已有；找回和完整页面未齐 | [成员][members]、[成员门][gate]；[成员测试][people-test]；[规范 §7.2][ui-spec] | kid-member；settings-all；找回依赖真实中枢 | 2～4（设备端） |
| B04 孩子主页、使用时段／限时和家长设置 | 部分：儿童身份／权限／音量限制已有，未查到时限服务与完整儿童页 | [成员][members]、[音频][audio] L790；[规范 §7.3][ui-spec] | kid-member、settings-all；限时后端未单列 | 3～5 |
| B05 AI 对话：按键、免按键、打断、长答及异常页 | 部分：设备会话和分页面板已有，缺完整 AiHome／离线／额度等页 | [AI 服务][ai]、[AI 面板][ai-panel]；[免按键场景][voice-test]、[轮次测试][ai-race]；[规范 §7.4][ui-spec] | ai-pages；speech-cache 另有[任务书][speech-task] | 3～5（设备页面及接入） |
| B06 真模型、听写／语音合成和家庭服务器 | 部分：仅假模型、替身语音、合成身份；不构成真实对话可用 | [中枢说明][hub-readme]；[模型替身][fake-model]、[语音替身][fake-speech]；[端到端测试][hub-test] | 未覆盖：中枢第 2 步起，hub-step1 不能包办 | 先 2 项按模型／语音／身份拆分；实现待估 |
| B07 对话历史、AI 行为记录、回执及记忆查看／删除 | 部分：activity.list 已有；缺对话历史页面和真实记忆 | [总线][cap] L1284、[记忆包][hub-memory]；[规范 §7.11][ui-spec] | ai-pages、settings-all；记忆后端未覆盖 | 2～3（设备页面）；后端待拆 |
| B08 中文／英文／数字／密码键盘、候选和听写 | 部分：输入服务和键盘已有，正式词库／完整输入宿主未齐；听写依赖中枢 | [拼音][pinyin] L53、[键盘][keyboard]、[听写][dictation]；[输入测试][text-test]；[服务设计 §9][svc-design] | app-framework-2（ui-input-host）；正式词库未单列 | 2～3；许可核清另计 |
| B09 长文本翻页、草稿、选区问 AI／加单词 | 部分：光标／选区／撤销已有，长文与持久草稿缺失 | [文本服务][text]、[文本框][field]；[输入测试][text-test]；[服务设计 §15.4][svc-design] | text-long；app-framework-2 | 2～3 |
| B10 跨应用剪贴板、敏感标记与成员清空 | 有：服务及 AI 读取授权存在；按钮共路缺口记 A06 | [剪贴板][clipboard]、[文本能力][text-caps] L248；[输入测试][text-test] | app-framework-2 复用；settings-all 接授权 | 0（服务） |
| B11 电话：拨号、来电、接听／挂断、通话记录 | 部分：AT 能力／来电打断已有，缺电话应用与持久通话记录，双向音频未知 | [通信][comm] L238、[打断卡][interrupt-ui]；[AT 模型测试][sim-test] L619；[硬件对照][parity]；真机未验证 | phone-sms；通话音频／VoLTE 验证未单列 | 4～6（软件）；硬件先 1 项核查 |
| B12 短信：会话、写入、长短信、失败／未知回执 | 部分：收发接口已有，只保留 16 条内存消息，缺持久会话与完整长短信 | [通信][comm] L31、L102、L192、L263；[AT 模型测试][sim-test] L619；[依赖 §4.3][deps]；真机未验证 | phone-sms | 3～5 |
| B13 联系人：增删改查、选择联系人和号码 | 没有：未实现联系人应用／业务集合，图标不能替代 | [依赖 §4.3][deps]、[phone-sms 范围][backlog]；[启动注册][boot] L88、[启动图标][nav] L1290 | phone-sms | 2～3 |
| B14 时钟与闹钟：重复、稍后响、关掉、重启恢复 | 部分：时间／闹钟服务和打断已有，缺 Clock 编辑管理页 | [时间][time]、[时间能力][time-caps]、[打断卡][interrupt-ui]；[时间测试][time-test]；RTC 唤醒真机未验证 | memo-calendar-clock | 2～3 |
| B15 秒表、倒计时、世界时钟 | 未查到独立实现；time.zone 不等于世界时钟应用 | [时间能力][time-caps] 全部注册项、[页面注册][nav] L33、[系统应用分类][sys-apps] | memo-calendar-clock 的 Clock 需细分 | 2～3（建议补齐，不视为已定细节） |
| B16 日历、日程、重复提醒、接下来 | 部分：公农历及提醒服务已有，缺日历／日程业务与页面 | [时间能力][time-caps] L278、L312；[时间测试][time-test]；[规范 §7.14][ui-spec] | memo-calendar-clock | 3～4 |
| B17 备忘、清单、编辑／撤销、离线保存 | 没有：有通用数据和文字服务，备忘应用尚未实现 | [依赖 §4.3][deps]、[规范 §7.6][ui-spec]；[启动][boot] L88、[启动图标][nav] L1290 | memo-calendar-clock；前置 text-long | 3～5 |
| B18 录音及备忘录音附件 | 部分：Ogg Opus 录存／播放已有，缺录音管理及备忘关联 | [媒体服务][media]；[媒体测试][media-test] L88；[规范 §7.6][ui-spec]；真机未验证 | memo-calendar-clock | 1～2 |
| B19 通知中心、未读／清除、渠道与高优先打断 | 有：按成员持久收件箱、渠道、勿扰及界面分页；渠道设置页另计 C15 | [通知][notify]、[导航][nav] L657；[通知测试][notify-test]、[界面测试][ui-test] L128；真机打断未验证 | settings-all 接渠道；phone-sms 等复用 | 0（现有中心／服务） |
| B20 文件浏览、拷贝、移动、删除、导入和安全错误 | 部分：文件能力／作业已有，缺 Files／Import 页面及转换对接 | [文件能力][file-caps] L321；[文件测试][file-test]；[规范 §7.8/§7.9][ui-spec] | files-app；导入转换中枢后端未覆盖 | 3～5（设备端） |
| B21 文件稳定编号、类型、媒体索引和换卡 | 没有：现回执仍以路径为对象，稳定编号／媒体索引待补 | [文件能力][file-caps] L188、[文件接口][file-api]；[编号任务][index-task] | file-index（已拆 file-index-1） | 1～2 |
| B22 图片解码、实屏 16 灰抖动、缩略图和版本失效 | 部分：远程 JPEG 区域解码已有，未查到本地 PNG／JPEG 缩略图链路 | [远程][remote] L205；[图片任务][image-task] | file-index（已拆 file-index-2） | 1～2，加 1 项与索引联接 |
| B23 U 盘模式／无线传输 | 部分：USB 借卡／归还已有；无线传文件未实现，配网热点不等于传输 | [文件能力][file-caps] L353、[文件测试][file-test]；[服务设计 §13][svc-design]、[无线任务][wireless-task]；真机未验证 | wireless-transfer；files-app | 2～3（无线服务）＋1～2（界面） |
| B24 备份、清空后恢复、跨设备／身份重绑定 | 部分：成员备份／恢复已有，缺设备设置／凭据、真实重绑定、增量对账及页面 | [备份服务][backup] L1415、[备份测试][backup-test]；[已知限制][backup-report] | backup-restore、boot-flow、power-update-pages；真实重绑定未覆盖 | 3～5（设备收尾及页面）；中枢待拆 |
| B25 相机取景、拍照存卡、扫码结果 | 部分：camera.capture 只保留最后一张 RAM 图，缺取景／存卡／扫码 | [相机][camera] L3、L40；[GC2145 驱动][camera-driver]；[规范 §12][ui-spec]；真机未验证 | camera-gallery；前置 file-index | 3～5 |
| B26 相册、看图、选择／删除与问照片 | 没有：相册／媒体库应用尚未实现 | [相机][camera] L3、[依赖 §4.3][deps]、[规范 §4][ui-spec]；[启动图标][nav] L1290 | camera-gallery | 2～3 |
| B27 网页／远程画面：地址、翻页、长按、文字提交 | 部分：区域画面和点坐标已有，其余输入及内容语义未接；服务器浏览器未实现 | [远程][remote] L424、L569、[中枢远程包][hub-remote]；[远程场景][remote-test] | remote-app；真实浏览器后端未覆盖 | 3～4（设备）；后端待拆 |
| C01 网络设置：Wi-Fi 扫描／连接／记住／忘记 | 部分：服务齐，页面有扫描／输密码，未查到忘记网络按钮 | [Wi-Fi][wifi] L210、[导航][nav] L997；[成员网络测试][people-test]；真机未验证 | settings-all；boot-flow 复用 | 1～2 |
| C02 网络设置：自动／Wi-Fi／4G（net.prefer） | 有：服务和分段选择均存在 | [选网][net] L122、[导航][nav] L997；[自动选网测试][net-test]；真机未验证 | settings-all 复核 | 0（模拟器） |
| C03 网络设置：在线选 SIM（cell.sim_slot） | 有：设置、界面及 NT26 驱动链路存在 | [通信][comm] L310、[导航][nav] L1060、[NT26][modem-driver]；[AT 测试][sim-test]；真机未验证 | phone-sms／settings-all 复核 | 0（模型）；真机核查见 B11 |
| C04 飞行模式及明确关闭无线 | 未查到系统设置／对应能力 | [网络设置定义][net] L122、[页面注册][nav] L33；[依赖 §4.3][deps]明确列缺飞行 | settings-all；无线关闭后端未覆盖 | 1～2（先核共享电源边界） |
| C05 APN／漫游／流量统计与上网热点 | 未查到；pairing 热点只用于配网 | [网络][net]、[通信][comm] 注册项；[配对][pairing] L975；[网络 HAL][net-hal] | 未覆盖：建议网络设置细分 | 先 1 项核硬件及需求；实现待估 |
| C06 蓝牙：扫描、配对、连接、忘记与音频路由 | 部分：耳机服务／模型已有，SetBluetooth 未接，芯片扫描配对协议未知 | [蓝牙][bt] L205、[蓝牙 HAL][bt-hal]；[媒体测试][media-test]；真机未验证 | settings-all；真实蓝牙协议／端口未覆盖 | 2～3（页面）；先 1 项协议核查 |
| C07 蓝牙音箱模式及翻页器 | 部分：BH1098G 有模式命令，未查到系统音箱接入或 HID | [音频芯片驱动][audio-driver]、[蓝牙 HAL][bt-hal]；[backlog 保留项][backlog] | settings-all 保留入口；HID／音箱后端未覆盖 | 先 1 项分别核查；实现待估 |
| C08 显示：亮度（display.light） | 有：界面／能力／前光接口已接 | [前光][light] L33、[导航][nav] L908；[界面回归][ui-regression]；真机系统控制未验证 | settings-all 复核 | 0（模拟器） |
| C09 显示：暖光（display.warmth） | 有：界面／能力／冷暖两路已接 | [前光][light] L36、[导航][nav] L912；[界面回归][ui-regression]；真机未验证 | settings-all 复核 | 0（模拟器） |
| C10 显示：字号（display.font_size） | 有：设置实际影响字体和布局；有儿童档 | [导航][nav] L920、[主题][theme] L121；[字号场景][ui-scenes] L194、L358；[历史字号观察][project] | settings-all 复核 | 0（当前壳） |
| C11 显示：自动锁屏（power.lock_after_s） | 有：服务、能力、界面可选时长 | [电源][power] L291、[导航][nav] L939；[唤醒测试][wake-test]；真机未验证 | power-update-pages／settings-all | 0（模拟器） |
| C12 显示：锁屏画面（display.lock_style）及壁纸 | 部分：两种画面可由能力设置，但未查到样式选择控件和 Wallpaper 页 | [导航][nav] L1431、[锁屏][lock] L30；[界面回归][ui-regression] L277 | settings-all、power-update-pages；前置 file-index | 1～2 |
| C13 声音：音量（audio.volume） | 有：实体键、滑杆及能力接同一设置 | [音频][audio] L786、[导航][nav] L951；[媒体测试][media-test]；真机未验证 | settings-all 复核 | 0（模拟器） |
| C14 声音：勿扰（notify.dnd） | 有：声音页和控制中心已接；保留高优先打断 | [通知][notify] L694、[导航][nav] L975；[通知测试][notify-test] | settings-all 复核 | 0（模拟器） |
| C15 声音：铃声、按键提示音、振动、渠道开关 | 部分：通知提示音／振动已有，设置行显示“暂无选项”，铃声不能选 | [导航][nav] L978、[通知][notify] L174、[通知能力][notify-caps]；[通知测试][notify-test]；真机未验证 | settings-all；铃声／振动可配后端未单列 | 2～3 |
| C16 语音：叫名字策略（voice.handsfree）及 SetVoice | 部分：三档设置已有，控制中心只切关／使用时，完整页面缺 | [音频][audio] L783、[导航][nav] L887、L729；[免按键场景][voice-test]；真机未验证 | settings-all、ai-pages | 1～2（页面） |
| C17 孩子音量上限（audio.child_max） | 部分：策略／人工确认已有，未查到 SetKid 配置页 | [音频][audio] L790、[能力策略测试][cap-test] | settings-all、kid-member | 1（页面，计入 C20） |
| C18 AI 读剪贴板（ai.read_clipboard） | 部分：敏感授权已有，隐私设置页未接 | [文本][text] L541、[剪贴板][clipboard]；[输入测试][text-test] | settings-all（SetPrivacy） | 1（计入 C29） |
| C19 成人锁屏后再验证时长（member.idle_return_s） | 部分：服务已支持，缺设置入口；D-95 时长仍暂定 | [成员][members] L814、[唤醒测试][wake-test]；[决策记录][decisions] D-95 | settings-all；member-wake 已有服务 | 1（页面） |
| C20 家人管理、密码和孩子权限设置 | 部分：SetMembers／SetKid 未接，限时策略还需后端 | [成员][members]、[成员测试][people-test]；[规范 §4/§7.3][ui-spec] | settings-all、kid-member；与 B03/B04 重叠 | 2～4 |
| C21 存储容量、存储卡、占用／清理和备份入口 | 部分：file.free／状态已有，设置中的存储备份行未绑定 | [文件能力][file-caps] L349、[导航][nav] L901；[文件测试][file-test]；真 FAT／卡真机未验证 | settings-all、files-app；清理统计范围待拆 | 2～3 |
| C22 电池、充电、省电、低电量和空电关机页 | 部分：电源策略与关机图已有，缺完整电量／省电／充电页面及校准 | [电源][power]、[关机图][off-ui]；[低电量测试][lowbat-test]；[硬件记录][hw]；整机功耗真机未验证 | power-update-pages；校准未覆盖 | 2～3（页面）；校准先 1 项盘点 |
| C23 日期时间：自动校时（time.auto）／手动设置 | 部分：能力和中枢校时已有，未查到日期时间设置页 | [时间][time] L746、[时间能力][time-caps] L269；[时间测试][time-test]；真机未验证 | settings-all 未明确日期时间子页，建议补细项 | 1～2 |
| C24 日期时间：时区（time.zone） | 部分：9 个时区及夏令时规则已有，缺用户选择页 | [时间][time] L75、L746；[时间测试][time-test] | settings-all；与 C23 共页 | 1 |
| C25 系统语言切换、区域与日期格式 | 未查到：已有中文界面，不等于可切语言 | [设置定义][settings]、[导航][nav]、[系统应用分类][sys-apps]的历史建议；未查到 locale 能力 | 未覆盖；历史“不进首版”不能替代本次用户标准 | 先 1 项列范围／字库影响；实现待估 |
| C26 无障碍：大字、朗读、触控辅助 | 部分：大字与 AI 回答语音播放已有，未查到通用焦点朗读／辅助输入设置 | [主题][theme] L121、[AI][ai] L318、[文本框][field]；[框架设计 §5][fw-design] | app-framework-2、settings-all；无障碍细项未覆盖 | 先 1 项明确合同，2～3 项基本接入【估算】 |
| C27 更新：自动下载（update.auto_download） | 部分：设置／网络策略已有，缺用户设置入口 | [更新][update] L1120；[更新测试][update-test] | settings-all、power-update-pages | 1（页面） |
| C28 更新、卡刷、自检、失败回滚和恢复模式 | 部分：设备服务／模型已有，完整页面、真机引导及真实固件源未接 | [更新][update]、[更新卡][update-ui]；[更新测试][update-test]；[服务设计 §12.5][svc-design]；真机未验证 | power-update-pages；真机引导／中枢固件源未覆盖 | 2～3（页面）；引导／固件源先 2 项拆分 |
| C29 隐私、活动记录保留与 AI 记忆设置 | 部分：成员策略、活动记录和剪贴板授权已有，缺 SetPrivacy／Records／AiMemory 和真实记忆 | [总线][cap] L1284、[文本][text] L541、[记忆包][hub-memory]；[规范 §7.11][ui-spec] | settings-all、ai-pages；记忆后端未覆盖 | 2～3（页面）；与 B07/C18 重叠 |
| C30 关于、版本／设备信息、诊断结果 | 部分：HAL／更新服务有信息，关于行无动作、SetAbout 未接 | [设备 HAL][device-hal]、[更新][update]、[导航][nav] L902 | settings-all | 1～2 |
| C31 首次开机、配网、家庭绑定、恢复及吊销 | 部分：配对状态与热点／卡文件模型已有，缺 Boot 系列页和真实账号绑定 | [配对][pairing]、[配对测试][pair-test]；[中枢账号包][hub-accounts]、[规范 §7.1][ui-spec]；真机配网未验证 | boot-flow、pairing；真实账号服务未覆盖 | 3～4（设备页）；中枢先 1 项拆分 |
| D01 触摸、盖板／实体键、振动和加速度计的合理使用 | 部分：驱动／模型和输入仲裁已有，系统方向适配／振动选项未齐 | [输入][input]、[板级触摸][board-touch]、[硬件对照][parity]；[接线测试][board-test]；[历史触摸／重力测量][phase3] | settings-all；A01 端口；自动方向策略未覆盖 | 1～2（系统接入）；端口另计；整机真机未验证 |
| D02 墨水屏刷新、前光、灰阶和中文可读性 | 部分：实现及局部真屏测量已有，当前完整系统未验 | [显示服务][display]、[真屏记录][phase3]；[界面回归][ui-regression]；最终去噪改动仅模型验证 | A01；perf-latency；遵守 D-110 已固定算法 | 1～2（集成核对）；不安排本任务上机 |
| D03 RTC 闹钟、锁屏接电话／短信和省电唤醒 | 部分：模型能走，P16／模组唤醒线路仍有证据缺口 | [电源 HAL][power-hal] L62、[硬件对照][parity]；[时间故障场景][rtc-test]；真机未验证 | phone-sms、memo-calendar-clock；硬件验证未覆盖 | 先 2 项核查；实现待估 |
| D04 外放／麦克风、蓝牙录放、通话焦点和音质 | 部分：外放／录音／Opus／五档焦点已有；真音频、耳机协议和蜂窝音频未验证 | [音频][audio]、[媒体][media]、[蓝牙 HAL][bt-hal]；[媒体测试][media-test]；[依赖 §3][deps] | settings-all、phone-sms；真实音频验证未覆盖 | 先 2 项分别核外放／耳机与通话；实现待估 |
| D05 Flash、卡、USB 的数据安全和资源预算 | 部分：原子写／卡借用／预算已有；本项目分区、FAT、USB 及并发未在真机验证 | [存储][storage]、[文件接口][file-api]；[断电测试][powercut-test]、[文件测试][file-test]；[服务设计 §2][svc-design] | A01、file-index、power-update-pages、perf-memory | 先 2 项移植核查；实现待估 |
| D06 性能流畅：开机／点按／翻页／输入／开应用及长期内存 | 部分：有场景预算和确定性测试，缺完整系统前后对比及真机结论 | [预算][budget]、[确定性][det-test]、[界面测试][ui-test]；[真屏记录][phase3]约 0.52 秒仅历史翻书测量 | perf-memory、perf-latency；依赖系统应用完整 | 4～6（模拟器基线与有收益优化）；真机待估 |
| D07 紧急呼叫／锁屏求助、位置、NFC、生物识别、补光灯 | 未查到完整能力；硬件清单未给出后四项的支持证据，不能推定支持或取消 | [背景 §3][bg]、[硬件清单][hw]、[设备 HAL][device-hal]、[系统应用待确认][sys-apps] | 未覆盖：分别核能力与硬件边界 | 先 1～2 项核查；实现不可估 |
| D08 计算器、天气等出厂工具，以及浏览器／邮件边界 | 未查到计算器／天气／邮件实现；网页仅 B27，旧分类曾建议后移 | [系统应用分类][sys-apps]、[需求 R-037][req]、[启动][boot]；“手机标准”与旧分类需由主会话核清 | 未覆盖：保留清单，不自行增加／删去产品需求 | 先 1 项核范围；计算器约 1～2，联网工具待估 |

## 没做到的

- 未运行构建、全量测试、内存检查、窗口模拟器或反向对照：本任务明确只查报告，不修改代码；测试文件只作为静态证据，不作本次通过证明。
- 没有读取其他克隆的工作区、在线决策数据库或真实设备；另分支任务只据基线内任务书记录，未声称合入。真实厂商、运营商、家庭资料和费用均未涉及。
- 真机端口、未知芯片协议和中枢后续工作无法凭现有证据给可靠总工期，表中保留盘点任务与“待估”，没有把它们当作不需要。

## 和设计不一致的

- 【实现】G-002／R-061 尚未全面成立：通用语义缺 A05，文本框与远程点击共路缺 A06；只有能力名字或手写快照不等于所有页面和行为可供 AI 使用。
- 【实现】主页多个系统应用入口只有图标；[启动][boot]只注册 home／reader，远程页通过 `remote.open` 单独进入，`shell.launch` 无匹配时拒绝；不能把画板清单或壳测试当系统应用完成。
- 【实现】确认结果持久化（A09）、完整恢复协议（B24）、相机存卡（B25）、真实中枢（B06）尚未达到合同；未改设计迁就实现。

## 改过的已有测试

无；产品代码、测试、预算和依赖均未改。

## 要用户定的

- 仅记录待主会话核清：紧急呼叫／锁屏求助；语言／区域和出厂工具的完整范围；蓝牙音箱／翻页器的入口及硬件路线。保留既有功能，不因旧分类的建议自行删减，也不把本报告候选升级为需求。
- 正式输入法词库的许可和来源、D-95 时长仍沿既有待定记录；[新任务书][sem-task]已经确认的框架五项 A 不重复提问。未知或有费用的真机验证只列缺口，本任务不执行。

## 顺手发现

- [依赖表][deps]仍有“主题／图标／通知中心没有”等旧描述，与当前壳代码不符；[backlog][backlog]顶端的旧状态也不能代表所有后发任务的集成状态。
- [README][readme]／[背景][bg]的“自研代码未上机”需区分整套系统与[局部测量固件][phase3]；当前不能宣称所有硬件驱动已验。

## 新增的机制

无；产品代码改动 0 行，仅新增本报告。

## 要改的文档

仅列建议，由主会话统一维护：backlog 补真机端口、真实中枢、通信音频、网络细项和工具边界，保留已发分支任务；ui-dependencies 逐行对齐源码；框架设计／决策记录同步任务书里的已定五项 A；系统应用分类及 SRS 对齐本次“手机标准”和可复验门槛；README／background 区分局部上机与完整系统。没有改动这些文档。

## 按依赖排的建议顺序

1. 先核对并集成 member-quiesce、confirm-durable 等底座修复，让后续页面复用可靠的成员与结果边界。
2. 依 F1 → F2／text-long → 列表分页推进统一语义、输入和恢复，再接 F5 卡片、活动／任务、搜索及文件选择器。
3. 并行补 file-index-1／2 → 无线传输及图片／音频对象，为文件、相册、壁纸和录音附件提供稳定引用。
4. 将真实中枢拆为模型语音、账号重绑定、记忆、备份／固件源和浏览器，分别与设备接口做合成契约核对后再接真实服务。
5. 在这些服务之上接首次开机／成员 → 设置／AI → 联系人／电话／短信 → 闹钟日历备忘 → 文件相机相册／远程页，每条任务按约一小时再拆。
6. 真机端口先做不接设备的编译与接口核对，后续硬件验证按已有授权边界另行安排，保留 D-110 已固定算法和不再上机的约束。
7. 系统应用完整后执行 perf-memory／perf-latency，以开机、输入、通话打断、并发和长期使用场景给出可复验结论，不用模拟器预算替代真机体验。

[backlog]: ../backlog.md
[hw]: ../../hardware/README.md
[bg]: ../../background.md
[parity]: ../../research/simulator/hardware-parity.md
[phase3]: ../../hardware/2026-11-06-phase3.md
[project]: ../../project.md
[readme]: ../../../README.md
[req]: ../../requirements.md
[decisions]: ../../decisions/README.md
[ui-spec]: ../../product/system-ui-spec.md
[sys-apps]: ../../product/system-apps.md
[svc-design]: ../../system/system-services-design.md
[deps]: ../../system/ui-dependencies.md
[fw-design]: ../../system/app-framework-design.md
[spike]: ../appfw-spike/REPORT.md
[sem-task]: ../ui-semantics.md
[input-task]: ../ui-input-host.md
[cards-task]: ../ui-cards.md
[quiesce-task]: ../member-quiesce.md
[durable-task]: ../confirm-durable.md
[index-task]: ../file-index-1.md
[image-task]: ../file-index-2.md
[wireless-task]: ../wireless-transfer.md
[speech-task]: ../speech-cache.md
[backup-report]: ../backup-restore/REPORT.md
[boot]: ../../../system/boot/main.c
[apps]: ../../../system/ui/app_registry.c
[app-api]: ../../../system/include/ink/sys/appreg.h
[theme]: ../../../system/ui/theme.c
[theme-api]: ../../../system/ui/ink/ui_theme.h
[nav]: ../../../system/ui/shell/navigation.c
[field]: ../../../system/ui/textfield.c
[lock]: ../../../system/ui/lock_mark.c
[gate]: ../../../system/ui/shell/member_gate.c
[ai-panel]: ../../../system/ui/shell/ai_panel.c
[keyboard]: ../../../system/ui/shell/keyboard.c
[interrupt-ui]: ../../../system/ui/shell/interrupt_card.c
[off-ui]: ../../../system/ui/shell/off_picture.c
[update-ui]: ../../../system/ui/shell/update_card.c
[remote]: ../../../system/apps/remote/remote.c
[cap]: ../../../system/policy/cap.c
[settings]: ../../../system/policy/settings.c
[members]: ../../../system/policy/members.c
[confirm]: ../../../system/policy/confirm.c
[data]: ../../../system/services/data/datastore.c
[data-caps]: ../../../system/services/data/data_caps.c
[jobs]: ../../../system/services/res/task.c
[ai]: ../../../system/services/ai/ai_service.c
[audio]: ../../../system/services/audio/audio_service.c
[media]: ../../../system/services/audio/media_service.c
[comm]: ../../../system/services/comm/comm_service.c
[pinyin]: ../../../system/services/text/pinyin.c
[text]: ../../../system/services/text/text_service.c
[text-caps]: ../../../system/services/text/text_caps.c
[clipboard]: ../../../system/services/text/clipboard.c
[dictation]: ../../../system/services/text/voice_input.c
[time]: ../../../system/services/time/time_service.c
[time-caps]: ../../../system/services/time/time_caps.c
[notify]: ../../../system/services/notify/notify_service.c
[notify-caps]: ../../../system/services/notify/notify_caps.c
[file-caps]: ../../../system/services/file/file_caps.c
[file-api]: ../../../system/include/ink/sys/file.h
[backup]: ../../../system/services/backup/backup_service.c
[camera]: ../../../system/services/camera/camera_service.c
[net]: ../../../system/services/net/net_service.c
[wifi]: ../../../system/services/net/wifi.c
[bt]: ../../../system/services/bt/bt_service.c
[light]: ../../../system/services/display/light.c
[power]: ../../../system/services/power/power_service.c
[update]: ../../../system/services/update/update_service.c
[pairing]: ../../../system/services/hub/pairing.c
[input]: ../../../system/services/input/input_service.c
[display]: ../../../system/services/display/display_service.c
[storage]: ../../../system/services/storage/storage_service.c
[device-hal]: ../../../platform/include/ink/hal_device.h
[net-hal]: ../../../platform/include/ink/hal_net.h
[power-hal]: ../../../platform/include/ink/hal_power.h
[bt-hal]: ../../../platform/include/ink/hal_bt.h
[camera-driver]: ../../../drivers/chips/gc2145/gc2145.c
[modem-driver]: ../../../drivers/chips/nt26/nt26.c
[audio-driver]: ../../../drivers/chips/bh1098g/bh1098g.c
[board-touch]: ../../../boards/metalio_plus/metalio_touch.c
[hub-readme]: ../../../hub/README.md
[hub-memory]: ../../../hub/ink_hub/memory/__init__.py
[hub-accounts]: ../../../hub/ink_hub/accounts/__init__.py
[hub-remote]: ../../../hub/ink_hub/remote/__init__.py
[fake-model]: ../../../hub/ink_hub/models/fake.py
[fake-speech]: ../../../hub/ink_hub/speech/fake.py
[hub-test]: ../../../tests/hub/e2e.py
[apps-test]: ../../../tests/integration/apps.c
[ui-test]: ../../../tests/ui_shell_test.c
[ui-regression]: ../../../tests/ui_shell_regressions.c
[ui-scenes]: ../../../tests/ui_shell_scenes.c
[page-test]: ../../../tests/scenarios/ai_page_version.script
[cap-test]: ../../../tests/capability_table/policy.c
[data-test]: ../../../tests/integration/data.c
[confirm-test]: ../../../tests/integration/confirm.c
[wake-test]: ../../../tests/integration/member_wake.c
[people-test]: ../../../tests/integration/people.c
[voice-test]: ../../../tests/scenarios/voice_handsfree.script
[ai-race]: ../../../tests/integration/ai_race.c
[text-test]: ../../../tests/integration/textinput.c
[sim-test]: ../../../tests/simulator/selftest.c
[time-test]: ../../../tests/integration/timesvc.c
[media-test]: ../../../tests/integration/media.c
[notify-test]: ../../../tests/integration/notify.c
[file-test]: ../../../tests/integration/files.c
[backup-test]: ../../../tests/backup_restore/device.c
[remote-test]: ../../../tests/scenarios/remote_browse.script
[net-test]: ../../../tests/integration/net_auto.c
[lowbat-test]: ../../../tests/integration/notify_lowbat.cmake
[update-test]: ../../../tests/integration/update.cmake
[pair-test]: ../../../tests/pairing/device.c
[board-test]: ../../../tests/simulator/selftest.c#L1105
[rtc-test]: ../../../tests/integration/timesvc_line_low.script
[powercut-test]: ../../../tests/integration/data_powercut.cmake
[budget]: ../../../tests/scenarios/ai_ask.budget.json
[det-test]: ../../../tests/simulator/determinism.cmake
