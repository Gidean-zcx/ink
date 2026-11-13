# notify-ui-1：通知页照 AOSP 重做（分组、展开、直接回复），加上状态栏和控制中心的勿扰显示

（手机模式、离线只收短信拆到 phone-mode-1，排在 phone-3 合入之后；见文末“拆出去的”。）

总规则先读 `docs/tasks/codex-rules.md`（第 9、10、11 节优先）和 `docs/tasks/vendor-common.md`（第 11 条）。

**D-140（总规则第 11 节）**：本任务是“照着重写”。第一步写对照表 `docs/tasks/notify-ui-1/对照表.md`（参照产品每一页、每个操作和状态，外壳看 `docs/tasks/shell-research-1.md` 和交付计划这一行的来源出发）并单独提交；计划和 `replan-sysapps/SOURCES.md` 点名的 C/C++ 代码能直接用的照搬。参照安卓的地方按第 11 节为这台设备改，做完逐行验收。

## 为什么

用户 2026-11-12（D-129、D-131）：状态栏、控制中心和通知属于第一优先的外壳部件，照成熟设计完善。交付计划 v4 第 3a 节“通知页面”：AOSP 的分组、展开、直接回复，进试用版 22。两条直接作验收（来自 `docs/tasks/frameworks-research-1/SHELL.md`）：分组展开用稳定的对象标识，新通知来时手指下面那条不能被换成另一条；“请求已受理”不能显示成已经成功。

服务层已合入（`notify-svc-1`，接口清单见 `docs/tasks/notify-svc-1/REPORT.md` 末尾）：`ink_notify_view_t.actions[]/nactions/reply_cap/reply_args`（`system/include/ink/sys/notify.h`）；能力 `notify.act`、`notify.reply`、`notify.dnd_get`、`notify.dnd_set`、`notify.replies_set`、事件 `notify.dnd_changed`（`notify_model.c:212-217`）；`ink_notify_dnd_state()`。短信通知 `.group = from`，回复目标 `sms.send`（`notify_service.c:685-686`）；未接来电归 `missed-calls` 组（`:630`）。`control-1` 把控制中心的通知行放进了 `notification_list.c`，这个文件从现在起归本任务（接口见 `docs/tasks/control-1/REPORT.md` “新机制和接口”）。

## 现状和缺口（起步分支 `claude/system-layer` c4ad7dea）

1. 完整通知页写在 `settings_body.c`（页面定义 :26，`notify_bind` :629，绘制 :660-686，快照 :785-800 附近）：平铺、每页 4 条、只有“删除”、图标套圆圈。
2. 点短信或未接来电通知什么也不打开：`shell.notify_open`（`navigation.c:313`）只执行旧字段 `action`，而这两类通知只带 `actions[]`。
3. 控制中心通知行（`notification_list.c`）用带框卡片和圆圈图标，标题直接显示号码；查名字照 `sms.c:213` 的 `ink_contacts_match(peer, true, …)`。
4. 勿扰在界面上只是开关（`control_center.c:76`），状态栏没有勿扰标记。
5. 页面在手指按着时重画会吞掉这次点按；只有备份页用 `ink_ui_pressing()`（`ui.h:114`）修过。

## 成熟来源

AOSP SystemUI（Lineage `22de88b7b8eb979c816669c7963dee66ae4953cd`，只读克隆到 `work/`）：`ShadeListBuilder.java:94-146`、`NotificationContentView.java:1480-1535`、`ModesTile.kt:103-142`。MuditaOS（`2cd0e4721405c0bf263f311f27d38aadae6ac89f`）：`StatusBar.cpp:30-105`、`NotificationsModel.cpp:43-94`。PebbleOS（`aa0c27e8330d010e9202f9a706a56002db5861ee`，只看行为）：`timeline_actions.c:1143-1168`、`notification_window.c:241-280`。行号以固定提交核对为准，写进 `docs/tasks/notify-ui-1/SOURCE.md`。

## 开工第一步

- 读 `notify_service.c:625-631` 和 `:676-689`，把短信、未接来电各个 action 的 cap 和 args 写进 SOURCE.md，验收按这个断言。
- 核对宿主机测试里 wall clock 能否注入，确认勿扰时段判断读的是它。
- 核对 `notify.dnd_active/dnd_reason/dnd_until` 状态是否存在。

## 要做的

1. 通知页搬家并按 AOSP 重写：页面定义、列表行、绘制、快照和 `shell.notify_open/next/prev` 的处理从 `settings_body.c`、`navigation.c` 挪进 `notification_list.c`（太多就加 `notification_page.c`）。挪来的原文不算行数；原处只删不加，`navigation.c` 最多加一行初始化调用。页面名 `Notifications`、对象 `notify:inbox`、能力名 `shell.notifications`、`shell.notify_open/next/prev`、`notify.dismiss/clear/read` 都不改；渠道开关页（`SetNotifications`）留在 `settings_body.c`。
2. 分组和排序：关掉的渠道不显示；按 `group` 分组（短信按号码，未接来电一组，其余一条一组）；组按最新一条时间排，高优先级在前。一组一行：名字（联系人名，查不到才显示号码）、条数、最新正文、时间、未读标记；图标不套圆圈，细分隔线，不画卡片框。控制中心的 `ink_notification_list_draw` 同样换成组行，签名不变。
3. 展开和收起：点展开区就地展开，每条完整正文、时间和自己的动作（最多 3 个）、有回复目标的加“回复”；放不下就分页，不滚动；再点收起。点组行本身执行这一组第一个动作（修缺口 2）；只带旧 `action` 的通知照旧能开。一屏最多一个主按钮。
4. 直接回复：点“回复”，该组下就地出现回复区：5 条快捷回复（读 `notify.dnd_get` 的 `replies`）、共用输入框（现有键盘）、“发送”主按钮。发送一律走 `notify.reply`，来源原样传下去（人点的直接发，AI 发起且无 `_meta.asked` 的由服务弹确认卡）。结果照回执如实写：“已发出：好的”“没发出”“结果不确定，请到短信里核对”，不把“已受理”写成“已发出”。离线（`net.airplane` 开）时写清发不出的原因，旁边给“切回联网”，按钮走现有 `net.set_airplane` 能力，不新开路径；孩子看到“家长管理”。“改快捷回复”入口（`notify.replies_set`）最后做。
5. 手指下面那条不变：页面开着来新通知，列表不重排，顶上一行“有 1 条新通知”，点它才重排；手指按着时异步重画等抬手后再做（照备份页 `ink_ui_pressing()`）；动作和删除绑定通知 id，删除带 `if_revision`。
6. 清除：删一条、清空全部只删通知箱记录，不动短信原文和通话记录；`notify.clear` 的 `ask_by_hand` 照旧。
7. 状态栏和控制中心勿扰显示（合计不超过 20 行，列进报告）：勿扰生效时状态栏右边出现月亮，只刷图标那一块；控制中心勿扰格第二行写“开 · 到 7:00”“时段中 · 到 7:00”或“关”，数据来自 `ink_notify_dnd_state()`，状态变了只刷这一格。离线照旧用飞机图标。勿扰格长按的去向本任务不改（等 phone-mode-1 加手机模式页后再改 `control_center.c:17`）。
8. 能力和快照：新用户操作都登记成能力，人和 AI 同一入口：`shell.notify_expand(group)`、`shell.notify_collapse`、`shell.notify_reply_open(id)`、查看新通知。页面快照写清每组的名字、条数、最新正文、未读；展开组里每条的 id、正文、可做的动作（能力加参数）；回复区的快捷回复和发送入口。孩子只看到自己的通知，可给自己开勿扰。

## 共同要求

- 来源一个不落，`SOURCE.md` 写每部分对应文件和行号，没照搬的写原因。
- 只用令牌和共用控件；点击区域不小于 108 px；一屏最多一个主按钮；不画重框，图标不套圆圈；空页用共用空状态；不写死像素颜色（`python3 tools/ui_scale_check.py`，ctest `ui-scale`）。
- 家人隔离、孩子模式；确认卡只给 D-105 的高风险动作；只刷变了的那块（D-127）；按下反馈照 D-31；密码不进日志。
- D-138：通知要调的能力不存在时在那一行写清原因，不能点了没反应或出白页。
- 文字大小跟系统一起缩放，默认和最大一档各出一套截图；放不下就分页或换行，不缩字。
- 原地重写，旧通知页代码删掉。
- 共享文件：`docs/system/capabilities.md` 用 `python3 tools/capabilities.py` 重新生成；新能力、状态、设置、事件进 `tests/integration/ai_surface.c` 该测试要求的各张表（FOR_AI 和不给 AI 的表，以及 STATES、SETTINGS、EVENTS 名单）；新文案英文加进 `assets/i18n/en.tsv`（改完重新运行 cmake 配置）。都只在末尾追加。
- 截图放 `docs/tasks/notify-ui-1/`，单张不超过 300 KB，只提交关键页，先看干净的整页图；用造数据，不出现真实号码和姓名。
- 绝不放宽、跳过测试或改它的时序；有意改变的行为才改已有测试，逐个列进报告。

## 范围和预算

- 只改：`notification_list.c/.h`，新文件 `notification_page.c`；`settings_body.c`、`navigation.c` 里通知页那几段（只删）；`shell_internal.h` 通知字段；`control_center.c` 勿扰格显示、`status_bar.c` 勿扰图标；`system/services/notify/` 里确有必要的小接口；相关测试、CMake 新文件行、`docs/tasks/notify-ui-1/`。
- 不碰：`theme.c`、`ui_theme.h`、`ui_list.c`、`textfield.c`、`airplane_card.c` 等弹窗（dialogs-1）；`settings_registry.c` 和设置各页（settings-2）；`lock_mark.c`；`system/apps/sms|phone|contacts`；`comm_service.c`（phone-3）；设备。开工前主会话确认 shell-sweep-1、ai-panel-1 不同时改上述外壳文件。
- 预算：产品代码约 800 行（挪来的原文不算），固件增量约 20 KB。超出时不要自己砍：做到哪写到哪，剩下的列进报告“没做到的”。顺序：第 1–6 条，再 7、8，改快捷回复最后。

## 验收

1. AOSP 走一遍（模拟器脚本，每步截图并断言）：“妈妈”3 条短信、陌生号码 1 条、1 个未接来电、1 条系统通知，共 4 组；展开见 3 条完整正文；选第 1 条快捷回复，`sms.send` 号码和文字对、无确认卡、行里“已发出：好的”；键盘输入自由回复；点组行进会话；未接来电“回电”调电话能力且号码对（能力名取自开工第一步）；删“妈妈”组和“全部清除”后短信原文都在；同一路径用 AI 来源再走一遍，AI 回复无 `_meta.asked` 时弹确认卡。
2. 手指下面那条不变：按住第 2 组不放注入新通知，按着期间不重画，抬手后点按落在原 id（看回执对象），顶上出现“有 1 条新通知”，顺序不变，点它才重排；反向对照：改成按行号绑定或按着时重画，必须失败。
3. 刷新面积（显示统计断言）：来新通知只刷“新通知”那一行；展开收起只刷这一组往下；勿扰格只刷这一格，月亮只刷图标块；反向对照：改整页重画必须失败。
4. 勿扰显示：用注入时间走一遍，手动勿扰开关和一段时段，状态栏月亮和勿扰格第二行文字正确。
5. D-138：能力不存在时那一行写明原因；离线时回复写明原因并给“切回联网”；无通知时共用空状态；都不出白页。
6. 改前改后截图（默认和最大字号各一套）：改前通知页、控制中心通知区；改后分组页、展开的组、回复区、孩子的通知页、带勿扰状态的控制中心和状态栏。
7. 测试和构建：清空 `build/test-out` 后全量 ctest（含 `ui-scale`）过；ASan 跑通知、外壳相关和 `ui-scene-*`；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告。
8. 报告写清：固件增量、产品代码行数、`gcccheck` 结果；改过的已有测试及理由；在 `navigation.c`、`settings_body.c`、`control_center.c`、`status_bar.c` 和通知服务里动过的行；新能力清单；没做到的和要用户定的；上机先核对的清单（本任务不上机，发短信花钱要用户同意）。

## 拆出去的：phone-mode-1（另写简报，排在 phone-3 之后）

原稿第 7、8 条：三种手机模式（联网、勿扰、离线）、`phone.mode`/`phone.mode_set`、手机模式页、勿扰页（例外、重复来电、日历忙时、最多 5 段时段）、离线页（只收短信与频率 15/30/45/60）、定时连网（`comm_service.c` 射频窗口）、通知页顶上入口行、`control_center.c:17` 长按去向、`shell.phone_modes`。简报里要补清：`phone.mode` 按当前家人计算且离线优先于勿扰；`phone.messages_only`、`phone.messages_every` 的设置注册方式（不碰 `settings_registry.c`）；时钟注入点；紧急呼叫只验证现有行为不被破坏；预算约 650 行。
