# 短信任务复查（第 1 轮）

日期：2026-11-09。基线：`f0b0c539f3f8a30bca03f663484287d65c76de33`。复查版本：`896009fd5342d8b2564e2f86ed94b7d7f4862d98`。使用用户提供的 `INK_DEPS`、`INK_DESIGN`。本轮只写本报告；测试产生临时构建／证据文件，不修改产品或测试源码、不提交、不接设备、不发真实短信。

## 结论

**不通过。确认严重 1 项、高 2 项、中 3 项。** 旧公开短信能力仍破坏成员隔离，长来信不能接收，长文草稿没有恢复入口；另有长短信裁切、首次 AI 导航丢参、回执被遮挡的问题。前两项旧接口／接收缺口在基线已存在，任务应当补齐，开发报告也如实披露；不算本提交新引入，更不据此指控虚报完成。

以下均为本轮独立读代码、构建或执行所得。普通和 ASan／UBSan 全量都为 **224/225，退出 8**，唯一失败是按总规则第 7 节留给合并阶段更新的 `capability-table-doc`。它不能写作全量通过，也不单列为高等级产品缺陷。

## 验收核对

| 验收 | 结论 | 独立证据 |
| --- | --- | --- |
| 1 收信入会话、重启保留、发送结果真实显示 | 部分 | 原 `sms` 四阶段通过；17 条来信和 4 条发送记录在新进程恢复。旧发送能力不记账（01），发送页三种结果均被遮挡（05）。 |
| 2 超过 70 个汉字发送和显示 | 不通过 | 118 个连续汉字经 AT 模型发送、显示通过；同长度来信读取失败（02）。80 个汉字加换行可发送、保存，但显示不完整（03）。F3 已保存草稿重启后无法继续（06）。 |
| 3 成员隔离及反例 | 不通过 | 新页面和 `data.list` 隔离通过；撤掉缓存集合归属后原测试准确失败 2 条。孩子仍可用公开 `message.list` 读成人短信（01）。 |
| 4 每页 AI 快照、AI 发信确认 | 部分 | 三页快照可放入实际 3072 字节 AI 上下文；另测四个长消息会话的列表快照也通过。AI 发信先停在确认卡，人确认后只发送一次。首次直接调用 `sms.thread` 却打开空会话（04）。 |
| 5 能力表、全量／内存检查、350 行预算 | 部分 | 新增 8 项能力注册；两套运行时均与临时生成表一致（221 项能力／事件、16 项设置）。产品改动保守计 234 行。两套全量均仅能力文档失败；ASan／UBSan 无检测报错。 |

没有在 Linux／gcc 或设备上复测，不把 macOS 模拟器结果外推到这两者。

## 问题

### SMS-R1-01（严重）：旧公开能力仍绕过成员会话库

- 文件：`system/apps/sms/sms.c:82,212,222`；现存入口 `system/services/comm/comm_service.c:218–226,264–276`。
- 结果：【实测】成人收到合成短信后切到孩子，AI 调用 `message.list` 仍返回成人短信；直接调用 `message.send` 成功发出一条，但 `sms.messages` 记录数为 0。开发报告已披露，不能据新页面隔离通过判整个验收通过。
- 复现命令：执行下文补充探针，设置 `INK_SMS_REVIEW_PHASES='legacy legacy-send'`。两阶段均退出 1、`test.finished=1`；前者先确认孩子调用成功，再捕获回执 `1 条短信，最近一条来自 00000000001：合成短信：回家后联系`；后者确认 AT 模型已发出而会话未记账。
- 最小修法：把现存 `message.*` 入口统一接到成员会话库，发送按钮使用同一条能力路径，删掉绕行的第二套发送入口。无需新增路由器或逐调用方修补；涉及共享服务既有代码，须由合并主会话协调范围。

### SMS-R1-02（高）：超过 70 个汉字的来信仍无法进入会话

- 文件：`system/services/comm/comm_service.c:36–39,92–100`；新订阅点 `system/apps/sms/sms.c:50–54,222`。
- 结果：【实测】向 AT 模型投递 118 个汉字成功，但 `comm.sms_read_failed=1`，等待后会话仍是 0 条。200 字节收信缓冲容不下正文，尚未产生新页面订阅的事件；这不是显示截短。
- 复现命令：补充探针 `INK_SMS_REVIEW_PHASES=long-rx`，退出 1、`test.finished=1`；原 `tests/sms/sms.c:44,139` 的来信夹具只有短文本，不能证明本条验收。
- 最小修法：只扩充通信服务的收信正文缓冲到现有短信参数支持的长度，并复用原接收流程验证长来信及重启恢复；不用新增接收机制。

### SMS-R1-03（中）：带换行的合法长短信被固定高度卡片裁掉

- 文件：`system/apps/sms/sms.c:136–139,185–187`。
- 结果：【实测】80 个汉字分成 16 行（加 15 个换行，总共 95 字符）可发出、可落盘，但真实标签超出消息卡片；当前每条消息固定占一页，后半部分也没有翻页入口。原测试的 118 个连续“中”字刚好能放下，不能覆盖这种普通排版。
- 复现命令：补充探针 `INK_SMS_REVIEW_PHASES=multiline`，退出 1，恰好一个真实标签边界断言失败；AT 正文逐字相等和落盘正文逐字相等的断言通过。截图只有约 10 行，后面被裁切，页码仍为 `1 / 1`。
- 最小修法：让会话正文复用已有长文本分页，只按可见正文窗口显示，保留整条数据；不缩字号、不放宽断言。

### SMS-R1-04（中）：首次直接调用会话能力会清空目标号码

- 文件：`system/apps/sms/sms.c:109–110,156`。
- 结果：【实测】启动后先收到短信，未打开过 `Sms`，直接 AI 调用 `sms.thread` 返回成功，但首次创建页面时因 `S.member` 为空而清掉刚写入的 `S.peer`，会话无标题、无记录。已有测试总是先打开列表，所以未命中。
- 复现命令：补充探针 `INK_SMS_REVIEW_PHASES=cold-thread`，退出 1、`test.finished=1`，实际 `peer="" count=0`。
- 最小修法：在导航能力入口先执行成员状态清理，再设置目标号码；创建页面时不再清除本次导航参数。

### SMS-R1-05（中）：成功、失败、未知回执都被按钮盖住

- 文件：`system/apps/sms/sms.c:84–88,159,177`；开发测试 `tests/sms/sms.c:145–151`。
- 结果：【实测】实际点发送，等待 UI 回调写入最终结果后，`S.notice` 的文字分别是“已发出”“发送失败，未发出”“结果未知，请先核实”，但标签纵坐标是 `961–1003`，后创建的“写长短信”按钮是 `923–1023`，标签完全被白色按钮覆盖。页面仍显示原文和可点击的“发送”，用户看不到是否需要核实。现有测试检查数据库状态，没有检查发送页的回执可见性。
- 复现命令：补充探针 `INK_SMS_REVIEW_PHASES='receipt-ok receipt-fail receipt-unknown'`。三个阶段均完成并命中重叠断言；实看成功、失败和未知状态的 `snap-review.png`。
- 最小修法：把现有提示条放到按钮上方的空白区域，保留现有主题间距；无需加浮层、通知或新状态机。

### SMS-R1-06（高）：长文显示已经保存，重启后却重新创建空草稿

- 文件：`system/apps/sms/sms.c:112,165,190–192,210`；接口合同 `system/include/ink/sys/textinput.h:28–29`。
- 结果：【实测】F3 输入 118 个汉字，等 `ink_text_draft_saved()==1` 和存储空闲，另起进程使用同一镜像。原 `text.drafts` 记录仍在，但重新选同一个收件人、打开长文页后正文为空。调用方每次都传 `draft_id=0`，既不保存引用，也没有应用恢复回调；`S.draft` 只在 RAM 中。丢的是正常用户继续编辑的入口，未声称底层删除了已落盘字节。
- 复现命令：补充探针 `INK_SMS_REVIEW_PHASES='draft-write draft-read'`。写入阶段退出 0，重启阶段确认旧草稿存在，然后“保留全部长文”断言失败、退出 1。
- 最小修法：用现有应用保存／恢复接口保留本成员、收件人和 `draft_id`，重建 F3 时传回该引用；正文继续归原数据服务。不要另存一份正文或新建草稿格式。

## 视觉复核（已逐屏查看 10 屏）

工作区没有 `out/ui-compare/`。本轮使用全量复测留下的真实截图，通过同一 `tools/ui_compare.py --reuse` 在临时目录生成并查看 10 张“设计／实现／差分”对照图。没有把默认 `sim-ui-scenes` 的占位短信页当作本任务成品；三张短信图来自 `sim-sms`。

| 页面 | 1/4 灰度差 /255 | 明显差异及判断 |
| --- | ---: | --- |
| Sms | 32.312 | 没有搜索、联系人姓名／头像、时间和未读数；顶部加号换成底部全宽“写短信”。四会话补查中，预览的下一行被卡片边缘切到半字。 |
| SmsThread | 100.430 | 多个按内容收缩的气泡变成一条占满宽高的黑卡；联系人变号码，日期、拨号、日程建议和就地回复框缺失。多行正文的实际裁切另见 03。 |
| SmsCompose | 36.807 | 大编辑卡变成 101 高短框，收件人只剩号码，没有字数和“帮我写”；键盘“发送”变为“完成”，先结束编辑再显示发送按钮。回执遮挡另见 05。 |
| Main | 54.194 | 时钟、图标和 Dock 放大，内容纵向位置改变；真实空阅读状态替代示例书名，短信图标没有示例未读数。属于现行主题／既有页面和夹具差异。 |
| Apps | 42.961 | 分组字号、间距放大，工具组分页；短信只出现在“记和联系”，没有重复入口。 |
| ImePinyin | 39.031 | 文本框更矮，候选变成黑白选择块；“大写”“删除”“空格”替代设计稿图形／空白键。既有输入法差异。 |
| ImeEdit | 36.919 | 选区显示方括号，设计稿是黑底反白；工具栏是圆按钮，“全选”与“剪切”的配置不同。既有组件差异。 |
| ImePassword | 32.303 | 标题为“网络密码”，缺网络名卡、显示按钮和隐私说明；密码仍掩码。既有页面差异。 |
| Confirm | 37.809 | 背景淡化符合当前规范；文案“只有你确认后才会继续”“确认”比原稿泛化，卡片高度和正文行数不同。不是短信专用确认卡的伪造截图。 |
| SetNetwork | 36.303 | 示例网络列表换成未连接／未扫描状态和两个操作按钮，SIM 区改成文字状态；既有服务状态差异。 |

全部超过目标 12，但不能为了分数把现行放大字号或第 8 级灰改回旧画板。前三屏的结构缺项与开发报告相符，不能称为视觉对齐；最小修正是在现有 F2／F3／F4 上补接联系人和会话布局，先解决 03、05，不再造组件。其余七屏用于复核共用环境，未归责为本次短信改动；未发现需要单列的新错字。设计稿示例数据不要求硬编码进产品。

## 构建、测试与范围检查

- 【实测】普通及 ASan 构建均退出 0，`INK_WERROR=ON`；ASan 另启 `INK_SANITIZE=ON`。全量各 224/225、无跳过；耗时分别 142.69 秒、246.74 秒。检查日志没有 AddressSanitizer／LeakSanitizer 错误或 UBSan `runtime error`。
- 【实测】两套短信四阶段各通过 117／33／21／27 条断言、零警告；内部 RAM 总最低余量 47,840 字节，原门槛 32,768 未变，内存分配失败为 0。全量包含的确定性测试通过。
- 【实测】原 `--mutant` 编译、链接成功，运行退出 1，`test.finished=1`，只失败 `sms.c:111,114` 两条隔离断言，未被超时或未完成假冒。
- 【实测】生成能力表到临时文件并对两套运行时检查，均退出 0：221 项能力／事件、16 项设置。提交的共享能力文档仍停在 213 项，故全量不绿；主会话合并时更新即可，不应放宽测试。
- 【实现】基线差异只有所报 10 个文件；新增产品文件 223 行、启动 2 行、导航 4 行、CMake 5 行，合计 234。已有测试只增加 8 项严格能力清单、应用数量从 7 改 8；未发现删测试、放宽预算、`#if 0` 或用等待掩盖断言失败。虚拟事件投递后的短等待不作为产品正确性证据。
- 【实测】基线到 HEAD 的 `git diff --check` 退出 0；指定隐私正则零命中。未动其他任务、设备路径、依赖版本或共享设计文档。本轮结束时仅本 `REVIEW.md` 为新增工作区文件。

复测命令（在仓库根目录，沿用已核对的两套 CMake 配置；构建和全量测试依次执行，均最多 `-j2`）：

```sh
taskpolicy -c utility cmake --build build -j2
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/sms/run.py "$PWD/build/sim-sms" "$PWD" "$PWD/build/assets" "$PWD/build/test-content" "$(mktemp -d -t sms-review-negative)" --mutant
git diff --check f0b0c539f3f8a30bca03f663484287d65c76de33 HEAD
git diff f0b0c539f3f8a30bca03f663484287d65c76de33 HEAD | rg -n '/Users/|/private/tmp|[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}'
```

最后一条预期无输出、退出 1。能力文档检查和成员反例的非零退出如上，不算测试全过。

## 品味和规模、架构审视

- 234 行未超预算。会话集合是任务明确要求；沿用数据日志、后台执行器、F2／F3／F4 和确认卡，没有新增常驻任务、重试器、队列、配置项或持久化格式。
- **应删的重复路径：** `sms.send` 包住仍独立公开的 `message.send`，业务存储跟页面在一起，原通信能力可绕开。这是 01 的根因。最小方向是收口到已有通信能力，再删掉重复发送包装及旧全局列表缓存；不要加拦截层，也不要逐个 AI／按钮调用方修补。
- 新页面的按钮与 AI 确实走同一能力，选号码确实经 `contacts.select`；但是原 `message.*` 仍开放，不能把“新页面共用能力”当成系统边界已经一致。04 表明能力还依赖先手动打开页面的隐含状态，06 表明未正确接好已有草稿合同。
- 除上述重复发送边界，没有证据支持再删一套机制或扩展通用框架。本轮只给最小修法，未改实现。

## 补充探针复现脚本

以下只把已有测试及少量补充场景作为编译输入，重用构建库；产品源码不变，产物进入新临时目录。两个 `draft-*` 阶段共用同一 Flash 镜像，其余阶段隔离。输出每阶段的退出码、完成标记和断言。待复核的断言失败是预期结果。

在仓库根目录执行：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" - <<'PY'
from pathlib import Path
t = Path('docs/tasks/sms/REVIEW.md').read_text()
code = t.split('```python\n# SMS_R1_PROBE\n', 1)[1].split('\n```', 1)[0]
exec(compile(code, '<sms-review-probe>', 'exec'))
PY
```

```python
# SMS_R1_PROBE
from pathlib import Path
import json, os, shlex, subprocess, tempfile
root = Path.cwd()
build = root / 'build'
out = Path(tempfile.mkdtemp(prefix='sms-review-r1-'))
source = (root / 'tests/sms/sms.c').read_text().replace(
    'static void run(void *arg)', 'static void original_run(void *arg)')
extra = r'''
static void rx_long(void *arg) {
    (void)arg; CHECK(sim_net_deliver_sms(PEER, body), "long inbound accepted at modem");
}
static bool failure_injected;
static void inject_failure(void *arg) { fail_modem(arg); failure_injected = true; }
static void check_card(void) {
    char peer[24], text[355], status[96];
    CHECK(S.count == 1 && read_message(S.db, S.rows[0], peer, text, status) && !strcmp(text, body),
        "multiline SMS is stored without loss");
    CHECK(rendered_body(lv_screen_active()), "all multiline text must fit its visible card");
}
static void check_receipt(void) {
    lv_area_t notice, button; lv_obj_update_layout(S.notice); lv_obj_get_coords(S.notice, &notice);
    CHECK(!strcmp(lv_label_get_text(S.notice), S.result), "receipt label has final result");
    wanted = "写长短信"; located = NULL; find_label(lv_screen_active());
    CHECK(located, "long editor button exists");
    if (located) {
        lv_obj_get_coords(lv_obj_get_parent(located), &button);
        CHECK(notice.y2 < button.y1 || notice.y1 > button.y2,
            "receipt %s must be visible: notice=%d..%d button=%d..%d", S.result,
            notice.y1, notice.y2, button.y1, button.y2);
    }
}
static void review_capture(void *arg) { (void)arg; sim_display_snapshot("review"); }
static void run(void *arg) {
    (void)arg; vtk_host_call(read_phase, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member ready");
    CHECK(WAIT_FOR(sim_net_modem_registered() && registered(), 10000), "modem ready"); quiet();
    for (int i = 0; i < 118; i++) memcpy(body + i * 3, "中", 3); body[354] = 0;
    if (!strcmp(phase, "legacy")) {
        open_list(); deliver(); CHECK(ink_member_switch("kid") == 0, "switch to child"); quiet();
        CHECK(AI("message.list", NULL) && g_rc.status == INK_RC_DONE, "legacy list callable by child");
        CHECK(!strstr(g_rc.say, "合成短信"), "adult inbox exposed to child: %s", g_rc.say);
    } else if (!strcmp(phase, "long-rx")) {
        open_list(); vtk_event_at(vtk_now() + SIM_MS(1), rx_long, NULL);
        CHECK(WAIT_FOR(stored("owner") == 1, 10000), "long inbound must reach SMS store");
    } else if (!strcmp(phase, "legacy-send")) {
        open_list(); CHECK(BY_HAND("message.send", "{\"to\":\"00000000001\",\"text\":\"合成短信\"}")
            && g_rc.status == INK_RC_DONE, "legacy send succeeds");
        CHECK(sim_net_sms_sent() == 1 && stored("owner") == 1,
            "every successful SMS send must have a durable conversation record");
    } else if (!strcmp(phase, "cold-thread")) {
        deliver(); CHECK(AI("sms.thread", "{\"peer\":\"00000000001\"}") && g_rc.status == INK_RC_DONE,
            "direct AI thread open accepted");
        CHECK(WAIT_FOR(S.list != NULL, 10000), "thread loaded"); quiet();
        CHECK(!strcmp(S.peer, PEER) && S.count == 1,
            "cold thread must keep peer: peer=%s count=%zu", S.peer, S.count);
    } else if (!strcmp(phase, "multiline")) {
        body[0] = 0; for (int i = 0; i < 16; i++) strcat(body, i ? "\n中中中中中" : "中中中中中");
        ink_jw_t w; ink_jw_init(&w, arguments, sizeof arguments);
        ink_jw_raw(&w, "{\"to\":\"00000000001\",\"text\":"); ink_jw_str(&w, body); ink_jw_raw(&w, "}");
        open_list(); CHECK(BY_HAND("sms.send", arguments) && g_rc.status == INK_RC_DONE,
            "send valid 95-character multiline SMS");
        CHECK(sim_net_sent_sms(0, sent_number, sizeof sent_number, sent_text, sizeof sent_text) == 1
            && !strcmp(sent_text, body), "modem receives exact multiline text");
        CHECK(AI("sms.thread", "{\"peer\":\"00000000001\"}") && g_rc.status == INK_RC_DONE,
            "open multiline thread"); quiet(); on_ui(check_card);
    } else if (!strncmp(phase, "receipt-", 8)) {
        open_list(); compose(); CHECK(AI("sms.text", NULL) && g_rc.status == INK_RC_DONE,
            "focus short editor"); on_ui(type_short);
        if (!strcmp(phase, "receipt-fail")) on_ui(no_registration);
        if (!strcmp(phase, "receipt-unknown")) {
            vtk_event_at(vtk_now() + SIM_MS(1), inject_failure, NULL);
            CHECK(WAIT_FOR(failure_injected, 10000), "modem failure injected");
        }
        tap_text("发送"); CHECK(WAIT_FOR(S.result[0] && settled(), 10000), "final UI receipt arrives");
        on_ui(check_receipt);
    } else if (!strcmp(phase, "draft-write")) {
        open_list(); compose(); CHECK(AI("sms.long", NULL) && g_rc.status == INK_RC_DONE, "open F3");
        on_ui(type_long); CHECK(WAIT_FOR(ink_text_draft_saved() == 1, 10000), "F3 reports draft saved");
    } else if (!strcmp(phase, "draft-read")) {
        ink_db_t *db = ink_db_open_member(ink_db_find_def("text.drafts"), "owner");
        CHECK(db && ink_db_count(db) == 1, "saved draft exists after restart"); ink_db_close(db);
        open_list(); compose(); CHECK(AI("sms.long", NULL) && g_rc.status == INK_RC_DONE, "reopen F3");
        on_ui(inspect_long);
    }
    CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "storage idle"); quiet();
    vtk_event_at(vtk_now() + SIM_MS(1), review_capture, NULL);
    ink_probe_count("test.finished");
}
'''
source = source.replace('void ink_main(void) {', extra + '\nvoid ink_main(void) {')
flags = {}
for line in (build / 'CMakeFiles/sim-sms.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
link = shlex.split((build / 'CMakeFiles/sim-sms.dir/link.txt').read_text())
obj, binary = out / 'probe.o', out / 'sim-sms-review'
subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
    '-iquote', str(root / 'tests/sms'), '-x', 'c', '-c', '-', '-o', str(obj)],
    input=source, text=True, check=True)
link = [str(obj) if p.endswith('tests/sms/sms.c.o') else p for p in link]
link[link.index('-o') + 1] = str(binary)
subprocess.run(link, cwd=build, check=True)
print('Evidence:', out, flush=True)
phases = os.environ.get('INK_SMS_REVIEW_PHASES',
    'legacy legacy-send long-rx cold-thread multiline receipt-ok receipt-fail receipt-unknown draft-write draft-read').split()
for phase in phases:
    target = out / phase
    flash = out / ('draft.img' if phase.startswith('draft-') else phase + '.img')
    cmd = [str(binary), '--profile', str(root / 'simulator/profiles/metalio-plus.json'),
        '--script', str(root / 'tests/sms/sms.script'), '--budget', str(root / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
        '--flash-image', str(flash), '--sd-dir', str(out / 'sd'), '--out', str(target),
        '--quiet', '--no-random-outages']
    result = subprocess.run(cmd, env=dict(os.environ, INK_SMS_PHASE=phase), capture_output=True, text=True)
    (out / (phase + '.log')).write_text(result.stdout + result.stderr)
    metrics = json.loads((target / 'metrics.json').read_text())
    m = metrics['metrics']
    print(phase, 'exit', result.returncode, 'finished', m.get('test.finished'),
          'fail', m.get('test.fail', 0), 'sms_read_failed', m.get('comm.sms_read_failed', 0), flush=True)
    for warning in metrics.get('warnings', []): print(warning, flush=True)
```

SEVERE=1 HIGH=2
