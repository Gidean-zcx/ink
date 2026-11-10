# 应用写法：以已实现的 C 接口为准

核查日期：2026-11-10；任务基线：`62b0e83da1d34f50629b5e576807db1381b26078`。本文是 F13 的接入说明，不把[设计建议](app-framework-design.md)中的接口草图当成 SDK。

[“记一笔”示例](../../examples/jot/jot.c)只链接到 `sim-jot` 测试程序，不加入出厂应用。它有三个可见槽位的列表、一页短输入、六个原子能力和一张服务卡片，最多保存八笔，每笔不超过 95 字节。代码块逐字摘自该文件，由 `app-framework-snippets` 检查；整个示例随测试编译。

## 1. 注册应用与启动顺序

出处：[appreg.h:51，应用声明](../../system/include/ink/sys/appreg.h#L51)、[appreg.h:65，注册](../../system/include/ink/sys/appreg.h#L65)、[app.h:27，应用内存](../../system/include/ink/sys/app.h#L27)。

应用声明、能力表、数据库定义都用静态存储期。平台启动服务后注册能力、集合和应用；框架要求在界面启动前完成注册。`entry` 是入口页面，`system_app` 表示是否出厂自带，`needs` 是缺了就不可见的能力，`permissions` 是应用来源调用所需的权限声明。注册失败必须处理。

```c
static const ink_app_def_t INK_JOT_APP = {
    .id = "jot", .title = "记一笔", .icon = "memo", .entry = &INK_JOT_LIST,
    .child_ok = true, .ops = &INK_JOT_OPS, .card = ink_jot_card
};
int ink_jot_init(void) {
    if (ink_db_register(&INK_JOT_DATA) != 0 ||
        ink_cap_register(INK_JOT_CAPS, sizeof INK_JOT_CAPS / sizeof *INK_JOT_CAPS) != 0) return -1;
    return ink_app_register(&INK_JOT_APP);
}
```

示例的测试宿主通过仅用于该测试的启动包装，在产品启动流程调用 `ink_ui_start()` 前注册 `ink_jot_init()`；真实集成应放进自己的启动装配位置。首次加载集合由应用在创建页面时调用 `ink_app_db_open_async()` 发起，后台执行器读盘，界面执行器收到结果后重绘；读取期间显示状态并提供快照。[测试宿主](../../tests/jot/app.c)只调用 `jot.open`，不再替应用预先读盘。切换家人时 `on_stop` 释放旧集合；尚未完成的打开由框架按原成员身份收尾。

大块数据使用 `ink_app_alloc`，把 `psram` 参数设为 true，并用 `ink_app_free` 释放。应用不自行建任务；耗时工作交给已有执行器再回界面。**还没有**可安装应用包加载器、完整脚本 SDK 或从设计草图自动生成的页面运行时。

## 2. 页面与生命周期

出处：[ui.h:36，ink_activity_t](../../system/include/ink/sys/ui.h#L36)、[ui.h:72，页面栈](../../system/include/ink/sys/ui.h#L72)、[appreg.h:28，生命周期](../../system/include/ink/sys/appreg.h#L28)。

`name` 是页面稳定名称；`app` 必须指向注册的应用 id，框架据此管理前后台。`create` 接收框架拥有的新屏幕；`destroy` 清理页面引用；`snapshot` 导出语义。打开用 `ink_ui_push()`，返回用 `ink_ui_pop()`；从真实按钮进入可用 `ink_ui_open()` 获得按下反馈。页面和 LVGL 操作都在界面任务执行；不要在回调里等待磁盘、网络或落盘。

```c
static const ink_activity_t INK_JOT_LIST = {
    .name = "jot.list", .app = "jot", .create = ink_jot_list_create, .destroy = ink_jot_list_destroy,
    .snapshot = ink_jot_list_snapshot, .on_key = ink_jot_key
};
static const ink_activity_t INK_JOT_EDIT = {
    .name = "jot.edit", .app = "jot", .create = ink_jot_edit_create, .destroy = ink_jot_edit_destroy,
    .snapshot = ink_jot_edit_snapshot
};
```

前台、后台、冻结、停止通过 `ink_app_ops_t` 通知；冻结前停止应用自己的活跃工作，停止后释放数据引用。`settle` 可把同一轮输入的重绘合并到一次。本例没有后台工作，也不声明 `INK_BG_*`。外观复用现有 [ui_shell.h:1](../../system/ui/ink/ui_shell.h#L1) 的主题控件；它仍位于系统界面目录，并非独立发布的 SDK。

## 3. 能力、效果、孩子权限与确认

出处：[cap.h:29，效果等级](../../system/include/ink/sys/cap.h#L29)、[cap.h:87，能力声明](../../system/include/ink/sys/cap.h#L87)、[cap.h:130，同步调用](../../system/include/ink/sys/cap.h#L130)、[cap.h:141，软总线调用](../../system/include/ink/sys/cap.h#L141)。

每个用户动作注册一个有意义的 `id`，不要把 LVGL 事件当成 AI 接口。`input`／`output` 是现有 JSON Schema 声明；`run` 填结构化回执。`effect` 分读、导航、本地写、本地破坏、对外和系统操作。`activity` 将操作限定到对应页面；`expose` 限定来源；`exec` 选择已有执行器，默认在界面任务执行。

```c
static const ink_cap_t INK_JOT_CAPS[] = {
    {.id = "jot.open", .title = "打开记一笔", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = ink_jot_open},
    {.id = "jot.new", .title = "写一笔", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "jot.list", .run = ink_jot_new},
    {.id = "jot.focus", .title = "输入内容", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "jot.edit", .run = ink_jot_focus},
    {.id = "jot.save", .title = "保存这一笔", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .activity = "jot.edit", .run = ink_jot_save},
    {.id = "jot.prev", .title = "上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "jot.list", .run = ink_jot_turn},
    {.id = "jot.next", .title = "下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "jot.list", .run = ink_jot_turn}
};
```

按钮通过 F1 动作绑定调用能力；AI 按快照的能力 id 和参数调用同一软总线，携带 `page_version`，不能直接调用 `run`。`ink_cap_invoke_json()` 适合界面中的同步能力；异步或跨执行器用 `ink_cap_call()` 的 `done` 收回执，`INK_RC_QUEUED` 不表示动作完成。`result` 只在完成回调期间有效，调用方需拷贝需要保留的数据。

孩子权限有两层：应用 `child_ok` 决定目录和打开可见性，能力 `child_ok` 参与执行策略。**当前实现中**，能力 `child_ok=true` 直接允许孩子；为 false 时，孩子的 `allow` 前缀仍可放行；时限、锁定和系统动作限制另外检查。不能把 false 当成绝对禁用，也不能把应用可见当成每个能力都获准。依据：[members.h:24](../../system/include/ink/sys/members.h#L24)、[members.h:64](../../system/include/ink/sys/members.h#L64)、[members.c:366](../../system/policy/members.c#L366)、[cap.c:97](../../system/policy/cap.c#L97)。头文件对“白名单还能收窄”的注释与实现不同，本文按实现说明，未修改框架。

确认由策略层决定，页面不自造“确认通过”的来源。实际规则见 [cap.c:80](../../system/policy/cap.c#L80)：`parent_pin` 且已设置密码时要求家长验证；`ask_by_hand` 要本人确认；普通触摸／按键／系统来源直接执行；AI 的本地破坏操作、没有本人意图标记的对外操作要求确认。`INK_FX_SYSTEM` 这个等级本身不会对所有大人操作自动弹卡，头文件枚举注释不能代替这些判断。示例的本地写无需确认；也没有声明撤销，不把保存称为可撤销。

能力可选填 [cap.h 的 `prepare_confirm`](../../system/include/ink/sys/cap.h)：签名为 `ink_rc_t (*prepare_confirm)(ink_args_t *args, ink_receipt_t *rc)`。策略决定需要确认后，在进入等待之前、当前策略关口所在执行器上调用一次；无需确认时不调用。钩子修改待确认参数的副本，绑定当时的业务上下文，也可增加供 `ask` 占位符显示的说明字段；私有字段须覆盖或丢弃调用方传入的同名值。成功返回 `INK_RC_DONE`；失败返回相应终态并填写 `rc->detail`／`rc->say`，请求不会进入等待。说明和确认摘要使用处理后的参数，确认后交给 `run` 的也是这些参数；执行时仍由能力核对绑定是否有效，钩子不改变确认、成员或家长密码规则。

## 4. F1：快照和控件语义

出处：[ui_semantics.h:16，字段与动作](../../system/include/ink/sys/ui_semantics.h#L16)、[ui_semantics.h:42，生命周期约定](../../system/include/ink/sys/ui_semantics.h#L42)、[ui_semantics.h:50，快照与动作桥](../../system/include/ink/sys/ui_semantics.h#L50)、[ui.h:121，运行时快照](../../system/include/ink/sys/ui.h#L121)。

页面模型 `id` 等于页面的 `name`。字段提供稳定 id、角色、标签、可见值与对象版本；动作提供能力、参数和可用性。屏幕控件与快照读同一份值。字段、模型及其字符串由页面持有，不能借用返回后的栈变量。值或可用动作变化后通知 `ink_ui_content_changed()`；`page_version` 和业务对象的 `revision` 是两种不同版本。

```c
static ink_ui_field_spec_t ink_jot_spec = {
    .id = "body", .role = "textbox", .label = "内容", .object = {"jot:input", 1}
};
static const ink_ui_action_spec_t INK_JOT_EDIT_ACTIONS[] = {
    {.id = "focus", .label = "输入", .capability = "jot.focus", .enabled = true},
    {.id = "save", .label = "保存", .capability = "jot.save", .enabled = true}
};
static const ink_ui_page_spec_t INK_JOT_EDIT_MODEL = {
    .id = "jot.edit", .title = "写一笔", .object = {"jot:input", 1},
    .fields = &ink_jot_spec, .field_count = 1,
    .actions = INK_JOT_EDIT_ACTIONS, .action_count = 2
};
```

普通页面的 `snapshot` 可调用 `ink_ui_semantics_snapshot()`；本例输入页用 F2 的短框快照，列表用 F4 的列表快照，它们都带 F1 语义。触摸处理用 `ink_ui_semantics_action()`／`ink_ui_list_action()`，目标含当前页面版本与对象引用；AI 从刚读的快照提取同样内容。提供者仍须核对业务参数中的对象版本，F1 不会替应用校验任意参数。运行时的 `ink_ui_snapshot()` 还会检查页面所属家人；测试同时比较生产 AI 使用的 `ink_ui_context_snapshot()` 与实际页面快照。

密码、验证码、证件字段设置 `secret=true`，快照只导出有值／无值。秘密不能放在对象 id、标签或动作参数里。F1 本身不打开输入会话。**还没有**从任意 LVGL 控件自动推导完整语义树的接口，应用必须显式声明。

## 5. F2 与长文本：输入框和草稿

出处：[textfield.h:18，创建短框](../../system/include/ink/sys/textfield.h#L18)、[textfield.h:21，F2 绑定](../../system/include/ink/sys/textfield.h#L21)、[textfield.h:34，长文宿主](../../system/include/ink/sys/textfield.h#L34)、[textinput.h:22，配置与字节上限](../../system/include/ink/sys/textinput.h#L22)、[textinput.h:47，会话与草稿](../../system/include/ink/sys/textinput.h#L47)。

短框先用 `ink_textfield_create` 创建，把 `focus` 参数设为 false，用固定位置父容器，绑定页面字段和聚焦动作，再聚焦。聚焦能力的处理函数调用 `ink_textfield_short_focus()`；`ink_textfield_text()` 取当前值，`VALUE_CHANGED` 与完成回调对应同一输入服务的编辑。短框只移动自身到固定键盘上方，结束后回原位。示例点键盘“完成”只结束输入，再按“保存”调用 `jot.save`。

```c
static void ink_jot_edit_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    ink_shell_topbar(screen, INK_JOT_EDIT_MODEL.title);
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .initial = "", .hint = "写下这件事", .max = 95};
    ink_jot_field = ink_textfield_create(screen, &cfg, NULL, NULL, false);
    if (!ink_jot_field) ink_app_fail("jot field allocation failed");
    lv_obj_set_pos(ink_jot_field, ink_ui_px(INK_MARGIN), ink_ui_px(INK_STATUS_BAR + INK_PRIMARY + INK_GAP));
    lv_obj_set_width(ink_jot_field, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN));
    if (ink_textfield_bind_short(ink_jot_field, &INK_JOT_EDIT_MODEL, &ink_jot_spec, &INK_JOT_EDIT_ACTIONS[0]) != 0)
        ink_app_fail("jot input binding failed");
    ink_ui_pill(screen, "保存", INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY,
        INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, true, ink_jot_click, "save");
}
```

AI 代填普通字段走 `text.insert` 等现有能力，带当前会话与编辑版本；不要把字符串直接塞进 LVGL 标签。秘密输入还须配 `ai_hidden=true`，只允许本人编辑和提交。

长文本已有 `ink_textfield_long_create()`、`ink_textfield_long_refresh()`、`ink_textfield_long_snapshot()`；配置 `draft_page` 为稳定页面／对象命名空间、`draft_id` 为已有引用或 0 新建。应用自己画标题与返回入口，在 `settle` 中刷新即可；控件也会跟进保存状态。`ink_text_draft_saved()` 的 -1／0／1 分别是失败、待落盘、已落盘。导航恢复只保存草稿 id 和页面名，正文由草稿服务保存；不能把整篇正文塞进应用恢复状态。`max` 按字节计，上限 4 KiB；光标、选择与分页位置按字符处理，用 `ink_text_byte_offset()`／`ink_text_char_offset()` 转换，分页避免截断字素簇。现有运行例见 [长文测试](../../tests/text_long/editor.c)。**还没有**任意长度文档编辑器，也没有所有页面自动重排避让键盘的接口。

## 6. F4：列表、网格和分页

出处：[ui_list.h:7，位置状态](../../system/include/ink/sys/ui_list.h#L7)、[ui_list.h:9，绑定与容量](../../system/include/ink/sys/ui_list.h#L9)、[ui_list.h:20，创建和翻页](../../system/include/ink/sys/ui_list.h#L20)。

列表放在专用容器里；`capacity` 最多 16，`columns=1` 是列表，大于 1 是网格。绑定回调只收到当前可见项，第一次创建子控件，以后更新同一控件。稳定对象 id 用记录编号，不能用屏幕行号；示例用数据库返回的 `rec.id`，并把 `rec.ver` 放进字段对象引用。`position` 保存页码与选中对象，换家人必须清理。

```c
static void ink_jot_list_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    ink_shell_topbar(screen, INK_JOT_LIST_MODEL.title);
    ink_jot_watch = ink_state_watch_handle("app.cards_rev", ink_jot_changed, NULL, INK_EXEC_UI);
    if (ink_jot_watch < 0) ink_app_fail("jot subscription failed");
    if (!ink_jot_db) {
        ink_jot_status = "正在读取记录";
        ink_jot_notice = ink_ui_notice(screen, ink_jot_status, INK_STATUS_BAR + INK_PRIMARY + INK_GAP);
        if (ink_app_db_open_async("jot", &INK_JOT_DATA, screen, ink_jot_loaded, NULL) != 0) ink_jot_loaded(NULL, NULL);
        return;
    }
    lv_obj_t *body = ink_ui_box(screen, 0, INK_STATUS_BAR + INK_PRIMARY + INK_GAP,
        INK_CANVAS_W, INK_CANVAS_H - INK_STATUS_BAR - 2 * INK_PRIMARY - 2 * INK_GAP - INK_MARGIN, 0, INK_SURFACE_WHITE, false);
    ink_jot_list = ink_ui_list_create(body, &INK_JOT_LIST_MODEL, &ink_jot_position,
        3, 1, INK_CONTROL, "还没有记录", ink_jot_bind, NULL);
    if (!ink_jot_list) ink_app_fail("jot list allocation failed");
    ink_ui_list_refresh(ink_jot_list, (size_t)ink_db_count(ink_jot_db));
    ink_ui_pill(screen, "记一笔", INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY,
        INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, true, ink_jot_click, "new");
}
```

页面动作的前两个固定为上一页／下一页能力，可再带一个页面动作。能力处理函数调 `ink_ui_list_turn()`，物理键可委托 `ink_ui_list_key()`；快照用 `ink_ui_list_snapshot()`，包含总项数、页数、当前偏移、可见项和截断说明。空页和末页由组件处理；点击可用 `ink_ui_list_action()` 共用 F1 检查。容器删除会释放视图，应用的 `destroy` 退订状态并清空借用指针。本例每页三笔，八笔上限明确，绑定时最多扫描八条；这不是无限列表的数据源实现。

## 7. F9：文件选择器

出处：[file_picker.h:5，回调与打开](../../system/include/ink/sys/file_picker.h#L5)、[file.h:61，类型、稳定编号与查找](../../system/include/ink/sys/file.h#L61)。

在界面任务调用 `ink_file_picker_open`，指定文件类型和完成回调；类型可选 `INK_FS_IMAGE`／`INK_FS_AUDIO`／`INK_FS_TEXT` 或 `INK_FS_ANY`。它列出当前家人有权读取的文件并分页，保留调用页；回调只给稳定文件编号，0 表示取消。返回 -1 表示没打开，也不会回调。调用页被删、换人或认证变化时会关闭并丢弃回调。

选择后将文件 id 交给已有后台执行器，在后台调用 `ink_fs_lookup()` 重新核对存储卡、权限并取元数据，不能把选择时的裸路径当成永久授权。需要读文件时仍走文件服务。选择器属于系统组件，示例没有附件功能，因此没有假造一个文件选择调用。**还没有**选择多个文件、选目录或“另存为”对话框的应用接口。

## 8. F5：服务卡片

出处：[appreg.h:40，卡片结构](../../system/include/ink/sys/appreg.h#L40)、[appreg.h:61，提供者](../../system/include/ink/sys/appreg.h#L61)、[appreg.h:90，变更和成员过滤](../../system/include/ink/sys/appreg.h#L90)；现有能力注册在 [app_registry.c:539](../../system/ui/app_registry.c#L539)。

`card(index, out)` 每次返回 1 表示有这一张，0 表示结束；回调必须轻，不依赖应用已经打开，不阻塞、不建页。用稳定 `id`、标题、真实内容和打开能力，家庭成员专属内容填 `member`。为空表示提供者已按当前家人过滤，不表示允许共享私密内容。`app.cards` 与“今日”共读同一来源，`ink_app_card_visible()` 做成员过滤。

```c
static int ink_jot_card(int index, ink_card_t *out) {
    if (index) return 0;
    *out = (ink_card_t){0};
    snprintf(out->id, sizeof out->id, "new");
    snprintf(out->title, sizeof out->title, "记一笔");
    snprintf(out->text, sizeof out->text, "记下新的事情");
    snprintf(out->action, sizeof out->action, "jot.open");
    snprintf(out->member, sizeof out->member, "%s", ink_member_current()->id);
    out->rev = 1;
    return 1;
}
```

本例卡片只提供真实的入口说明，不在应用未加载时伪造记录数量；`jot.open` 也就是测试使用的打开能力。内容变化调 `ink_app_card_changed("jot")`，框架更新 `app.cards_rev`，由“今日”现有机制合并刷新；不要加卡片自己的定时任务。书签类可设置 `bookmark`，本例未设置。卡片动作只有能力 id，没有独立的任意参数字段；需要参数的入口应由自己的能力在当前成员上下文中解析。

## 9. 状态、设置的读写与订阅

出处：[state.h:52，状态退订](../../system/include/ink/sys/state.h#L52)、[settings.h:60，设置退订](../../system/include/ink/sys/settings.h#L60)、[state.h:27，状态定义](../../system/include/ink/sys/state.h#L27)、[state.h:43，读取与订阅](../../system/include/ink/sys/state.h#L43)、[settings.h:32，设置定义](../../system/include/ink/sys/settings.h#L32)、[settings.h:54，读写与订阅](../../system/include/ink/sys/settings.h#L54)、[exec.h:1，已有执行器](../../system/include/ink/sys/exec.h#L1)。

| 数据 | 读取 | 修改者与方式 | 订阅 |
| --- | --- | --- | --- |
| 实际状态，如电量、联网情况 | `ink_state_find` 得句柄，`ink_state_get_int`／`ink_state_get_str` 取值；可能尚无值 | 定义它的服务持有 `ink_state_define` 返回的句柄，用对应 `ink_state_set_*` 更新；应用不改别的服务的状态 | `ink_state_watch` 按名称前缀订阅，在指定执行器回调中再读最新值；服务可用 `min_interval_ms` 合并变化 |
| 用户设置 | `ink_setting_get`；需元数据时用 `ink_setting_at`／`ink_settings_json` | 页面和 AI 通过 `settings.set`；C 调用可用 `ink_setting_set`，传实际来源并检查回执，仍经过设置策略。定义者用 `ink_settings_define` 提供范围与 `apply` | `ink_settings_watch` 按名称前缀订阅并指定执行器；回调读最新值，硬件是否已生效看对应状态 |

界面订阅选择 `INK_EXEC_UI`，后台服务选择合适的已有执行器；回调短小，不能在输入或中枢回调里阻塞。不要用一份应用私有变量代替系统状态。设置写入接受后会异步保存，`apply` 可能尚未运行；设置的 `system`、`child_may_change`、`ask_by_hand` 控制实际权限和确认，见 [settings.c:195](../../system/policy/settings.c#L195)。

页面用 `ink_state_watch_handle`／`ink_settings_watch_handle` 取得正整数句柄，失败为 -1；分别配对 `ink_state_unwatch`／`ink_settings_unwatch`。能力事件对应 `ink_cap_subscribe_handle`／`ink_cap_unsubscribe`，见 [cap.h:185，事件退订](../../system/include/ink/sys/cap.h#L185)。原来的三个订阅接口仍返回 0／-1，已有调用方不用改。

可退订的订阅选择 UI、MEDIA 或 BG；退订必须在订阅的同一个串行执行器上调用，返回 0 才能释放上下文，错误执行器、失效句柄返回 -1。该执行器先结束正在运行的回调，再执行退订；排队中的旧回调会被丢弃。回调内部退订会阻止后续调用，当前调用继续到返回，所以不要在它仍使用上下文时释放上下文。名称前缀也须保留到退订。DIRECT 没有串行执行器，可退订接口拒绝它；原有 DIRECT 常驻订阅继续可用。

本例列表页创建时订阅 `app.cards_rev`，收到变更后读取最新记录数，销毁时退订：

```c
static void ink_jot_changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (ink_jot_list) ink_ui_list_refresh(ink_jot_list, (size_t)ink_db_count(ink_jot_db));
}
static void ink_jot_list_destroy(void) {
    ink_state_unwatch(ink_jot_watch);
    ink_jot_list = NULL; ink_jot_notice = NULL;
}
```

## 10. 恢复状态与业务数据分开

出处：[appreg.h:25，1024 字节上限](../../system/include/ink/sys/appreg.h#L25)、[appreg.h:35，保存／恢复回调](../../system/include/ink/sys/appreg.h#L35)、[appreg.h:79，主动保存](../../system/include/ink/sys/appreg.h#L79)。

`save_state` 把导航位置等小状态写入给定缓冲，返回实际字节数，0 表示不留状态；`restore_state` 在 `on_start` 后恢复它。`ink_app_save_state()` 使用这同一份回调，不关闭应用。本例总共三页以内，只保存一个字节的页码；翻页能力里主动保存，停止时清掉内存位置，重开和重启都靠已存状态恢复。实际列表刷新还会把页码夹到有效范围。

```c
static int ink_jot_save_state(void *buf, size_t cap) {
    if (!cap) return 0;
    *(uint8_t *)buf = (uint8_t)ink_jot_position.page;
    return 1;
}
static void ink_jot_restore_state(const void *buf, size_t len) {
    if (len == 1) ink_jot_position.page = *(const uint8_t *)buf;
}
static void ink_jot_stop(void) {
    ink_db_close(ink_jot_db); ink_jot_db = NULL;
    ink_jot_position = (ink_ui_list_position_t){0};
}
static const ink_app_ops_t INK_JOT_OPS = {
    .on_stop = ink_jot_stop, .save_state = ink_jot_save_state, .restore_state = ink_jot_restore_state
};
```

这一个字节没有正文、指针或本机结构体布局。框架按家人保存，但提交到存储队列成功不等于已经耐久；模拟重启测试在后台等存储排空。这里没有扩写完整断电协议，也不声称未提交的短输入会自动恢复。需要长文草稿使用上一节的草稿引用。

## 11. 按成员存业务数据

出处：[data.h:38，集合定义](../../system/include/ink/sys/data.h#L38)、[data.h:89，打开与成员身份](../../system/include/ink/sys/data.h#L89)、[data.h:102，写入／分页](../../system/include/ink/sys/data.h#L102)、[data.h:120，落盘状态](../../system/include/ink/sys/data.h#L120)、[appreg.h:83，应用集合命名](../../system/include/ink/sys/appreg.h#L83)。

集合名以应用 id 加点开头；`INK_DB_MEMBER` 隔离家人，`INK_DB_DEVICE` 是设备级，不能拿来存私人备忘。注册定义须使用静态存储期。`ink_app_db_open()` 是可能读盘的同步调用。页面使用 [appreg.h:84，后台打开](../../system/include/ink/sys/appreg.h#L84) 的 `ink_app_db_open_async(app, def, page, done, ctx)`：在界面任务发起，`page` 为页面持有的 LVGL 根对象，后台执行器按发起时捕获的家人身份打开，完成后回界面任务调用 `done(ctx, db)`。返回 0 表示接受，-1 表示未发起且不回调；完成时 `db=NULL` 表示打开失败。页面根对象删除、换家人或认证变化后不回调，框架关闭取得的引用；正常回调接管该引用并配对关闭。

```c
static const ink_db_def_t INK_JOT_DATA = {
    .name = "jot.entries", .title = "记一笔", .scope = INK_DB_MEMBER,
    .schema = 1, .json = true, .ai_read = true, .ai_write = false,
    .max_records = 8, .max_bytes = 8192
};
```

示例的打开调用见第 6 节 `create`，完成回调如下；失败状态同时显示并进入页面快照：

```c
static void ink_jot_loaded(void *ctx, ink_db_t *db) {
    (void)ctx;
    if (db) { ink_jot_db = db; ink_ui_redraw(); }
    else {
        ink_jot_status = "暂时读不了记录，请返回后重试";
        lv_label_set_text(ink_jot_notice, ink_jot_status);
        ink_ui_content_changed();
    }
}
```

本例是 JSON 集合，`ai_read=true` 开放 `data.list`／`data.get`，`ai_write=false` 让写入经有明确语义的 `jot.save`。列表的每一项和输入框也都有快照；AI 可分页读取其余项，不把可见三条当成全部数据。`ink_db_put` 的 `src` 保留实际来源，分配出的 id 才是稳定对象编号。

```c
static void ink_jot_save(ink_call_t *c) {
    const char *text = ink_textfield_text(ink_jot_field);
    if (!ink_jot_db || !text || !*text) { c->rc.status = INK_RC_INVALID; return; }
    char payload[600];
    ink_jw_t json;
    ink_jw_init(&json, payload, sizeof payload);
    ink_jw_raw(&json, "{\"text\":"); ink_jw_str(&json, text); ink_jw_raw(&json, "}");
    ink_db_write_t write = {.data = payload, .len = json.len, .src = c->src};
    uint32_t id = 0;
    if (json.overflow || ink_db_put(ink_jot_db, &write, &id) != INK_DB_OK) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.say, sizeof c->rc.say, "这笔没能记下");
        return;
    }
    snprintf(c->rc.object, sizeof c->rc.object, "jot:%u", (unsigned)id);
    c->rc.revision = ink_db_last_ver(ink_jot_db);
    snprintf(c->rc.say, sizeof c->rc.say, "已接收，正在保存");
    ink_app_card_changed("jot");
    ink_ui_pop();
}
```

`ink_db_put()` 成功只说明内存接受，后台尚在落盘，所以示例回执写“已接收，正在保存”。耐久要等 `ink_db_durable_ver()` 到达写入版本；`ink_db_wait()` 只能在后台等，不能放在保存按钮中。业务写入失败须保留输入并回失败；本例不重试、不丢弃旧笔记、不放宽容量。分页读取用 `ink_db_cursor_t` 和 `ink_db_next()`，用完集合引用配对 `ink_db_close()`。

## 12. 复现与证据边界

构建依赖和环境按[任务总规则 §5、§7](../tasks/codex-rules.md)，先完成全量构建，生成字库和测试内容。改示例后可用 `taskpolicy -c utility cmake --build build --target sim-jot -j2` 编译，`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^app-framework-' --output-on-failure -j2` 跑示例与片段检查；Linux 没有 `taskpolicy` 时直接运行后面的命令。内存检查用同样配置加 `-DINK_SANITIZE=ON` 的 `build-asan`。

测试宿主会在同一模拟闪存上运行两个独立进程；比较触摸与 AI 的记录正文和来源、逐项比较快照和实际 LVGL 标签、核验孩子允许／拒绝、跨家人隔离和恢复页码。可选反向对照见 [run.py](../../tests/jot/run.py)，只编译临时的示例变体，不改框架或原文件。要保留临时证据设置 `INK_KEEP_EVIDENCE=1`。

这里只验证 C 接口与模拟器行为，没有操作设备，没有测实机时延、内存和功耗。长文本、文件选择和设置在本文按现有头文件说明；示例实际使用后台打开和状态退订，没有附件、长文或设置页。后台取消、三类退订及 100 次页面开关由 `app-framework-gaps` 检查，反向对照见 [run.py](../../tests/fw_gaps/run.py)。验收结果、缺口与建议见 [FW-F13 报告](../tasks/fw-f13/REPORT.md)。
