# device-events 独立复查

日期：2026-11-06。任务基线 f1f72ae9，受检提交 7834882c；上一轮复查及修前状态为 97645ea2。本次覆盖旧报告，只修改 REVIEW.md，不改产品或测试源码，不提交。

**结论：复查不通过，严重 1、高 1。** 上一轮 DE-R1 指定的握手窗口及 DE-R2 的声音提示问题已修到位；本次另证实两处成员隔离缺口。已披露的未完成功能仍未验收，不能把测试全过理解为任务全部完成。

## 上一轮问题

| 编号 | 复核结果 | 独立证据 |
| --- | --- | --- |
| DE-R1 握手期间换人 | 原复现已修；成员隔离整体仍有 DE-R3、DE-R4 | 真实首次握手、重连、双向切换、A→B→A、同成员认证变化、同步发送失败及发送分配边界的新增场景通过。撤回 hello 同步或最终发送检查均触发目标断言；将 97645ea2 的 hub_link.c 放入当前夹具，原握手隔离断言失败 |
| DE-R2 声音失败提示 | 已修 | 文字提问后再语音，真实下行 push／stream_open 拒收都终止声音，完成后保留失败文案；扬声器失败、旧通知及首段文字顺序回归通过。仅删错误通知也触发失败；修前 ai_service.c 在新夹具中复现提示失败 |

## 逐条验收

| 条目 | 结论 | 核对结果 |
| --- | --- | --- |
| G1 全量、内存、预算 | 既有检查通过，但有漏测 | 【实测】普通及 ASan+UBSan 构建均退出 0；全量各 103/103，42.43 秒／77.35 秒，无跳过。两套 INK_WERROR=ON，内存构建另开 INK_SANITIZE=ON。资源预算未放宽；新增独立复现见下 |
| G2 原子能力、AI 原生、分层 | 调用路径保留；身份边界不合格 | 【实现】tools/call 仍经能力策略，结果结构化，未新增缺少 AI 入口的用户操作。连接、实时会话、AI 展示的职责基本合理，但身份真值与协议代次分开发布（DE-R3），AI 切换保留旧成员展示（DE-R4），不能判隔离通过 |
| G3 依赖状态 | 已更新，不能据此扩大验收 | 【实现】ui-dependencies.md 仅改“成员、PIN”的状态格及报告链接；已覆盖的握手修复成立，新发现的隔离限制应在修复时同步 |
| G4 页面要求 | 本任务不适用 | 无新页面，不要求十屏设计稿对照。为 DE-R4 另查看了真实模拟器渲染图，确认文字仍在屏上；未启动带窗口 GUI |
| G5 范围与总规则 §1/§4 | 范围符合；成员隔离不符合 | 【实现】差异限于服务、策略、接口、必要的模拟替身／合成中枢、测试、构建登记及任务资料；无新增常驻任务或越层依赖。两处隔离问题违反总规则 §4 |
| DE-1 seq、boot、member、事件 ACK | 标识与重发通过；受众隔离不通过 | 【实测】两次持久 Flash 启动各 94 项检查，boot 递增；ACK 丢失后同身份重发，错误 boot／超前 ACK 拒绝。DE-R3 证明正确的 payload.member 不等于正确的接收上下文。8 条内存环不是持久结果库 |
| DE-2 ink/member、代次、交付 | 不通过 | 【实测】缺失／错误元数据、分配中换人、排队后 A→B→A、旧请求及实时声音撤销的现有测试通过；发布窗口和旧浮层仍串成员，见 DE-R3、DE-R4 |
| DE-3 budget_ms、录音上限、背压 | 本批实现通过 | 【实测】发送剩余时间、排队不重置期限、30 秒录音上限及其反向对照通过；声音失败提示已补齐实际下行验证 |
| DE-4 caps/list、actions | 本批实现通过 | 【实测】真实模拟中枢三页取回；中间页不发布、rev 改变／重复 cursor／超时保留旧表；完整 actions 回归通过。【实现】重复名称和 32 项上限有拒绝分支，暂存表在 PSRAM；actions_more 是未做的可选优化 |
| DE-5 sim_hub、假设备、契约 | 现有合成范围通过 | 【实测】两套 hub-contract 内部各 125 项通过，hub-json-crosscheck 和 hub-simulator-e2e 通过。真实中枢仍是固定合成儿童的 proto1；不等于成人解锁证明或正式家庭已验证 |
| DE-6 D-105 | 部分，未完成 | asked／ask_by_hand／活动记录沿用基线且回归通过；undo_ms=3000 只有检查与明确拒绝，不是“上屏后三秒可撤销执行” |
| DE-7 反向对照 | 19 项及两个修前复现有效 | 【实测】逐项编译、链接和模拟器均退出 0，指定断言确实失败。原测试没有覆盖本报告新发现的两种成员边界 |

§13 中未完成的项目逐项保留：三秒撤销窗口；中枢成员表全分页替换及 member.reset_pin；正式成人解锁证明；sync/*、数据集元数据、一致清单、原子安装与 history_gap；末端计时探针上传；内容包、远程画面的新动作、真 Opus／JPEG 模式；可选的 rt.close 原因／播放进度及计划短连。执行报告已明确披露，未发现把这些项目冒充完成。配对、固定公钥、地址／令牌和首次开机流程、确认持久化、块传输属于本节明确排除范围。远端 result、语音面板／asr.stream 和本机中枢桥的基线回归仍通过。

已有测试改动核对：ai_tools 的 done／answers 改为精确 1／3，同时要求 switch=1、旧轮取消=1，符合换成员取消旧轮的协议，资源上限没提高；但这些计数测不到旧文字是否消失。races 改查接收连接的实际成员；starts／actions 适配绑定身份的接口；policy 补交付元数据和已同步连接；handoff 显式关闭成人认证以适配基线 D-95。未见删除旧测试、放宽资源预算或用 sleep 掩盖产品时序。

## 已证实的问题

### DE-R4（严重）：AI 正常切到孩子空间，成人转写仍留在屏上

- **位置**：system/services/ai/ai_service.c:850、:859–864；调用点 system/policy/system_actions.c:79。ai_service.c:234–279 的 UI 通知也只比提问编号，不核对成员边界。
- **触发与复现命令**：运行下方附录，查看 voice-out。使用正常 AI 按下／松开、实际模拟语音上传、ai.stt 和带当前 member/epoch 的 system.switch_member；全程不暂停任务、不修改状态、不直接设置 AI 内部字段。
- **【实测】结果**：action.ai.system.switch_member=1，hub.member_turns_cancelled=1，当前成员已是 kid 且新目录已同步，但 AI.question 仍为“你问：SYNTHETIC_OWNER_PRIVATE：我的私人备忘处理完后，请切到孩子空间。”；状态变成“网络断了，回答没收全，请再试一次”，浮层没有关闭。问题组 test.pass=4、test.fail=1，移除保留例外的临时对照组 test.pass=5、test.fail=0；两组编译、链接、模拟器均退出 0，test.finished=1。已打开 voice-out/frames 中最后一张 GC16 图核对，成人转写确实可见，不能用“后台会话已经取消”解释为展示已隔离。
- **原因与影响**：AI 发起切换时设置 g_switch_is_mine，成员变更回调特意跳过 ink_ai_dismiss。正常支持的换成员操作因此把上一位成人的话留给孩子，不需要竞态或故障注入，按普通使用直接串成员定为严重。这处例外在 f1f72ae9 已存在，属于本次成员隔离验收仍未消除的基线缺口，不冒称本轮新引入。
- **修法**：换成员一律清掉旧轮的转写、回答及排队展示通知；若要显示“切换成功”，另产生不含旧成员内容、绑定新身份的提示。增加真实语音工具切换后对浮层内容／显示结果的断言。附录只在临时编译输入中移除该保留例外作对照，不修改产品文件。

### DE-R3（高）：本地身份已变、状态通知未到时，成人事件可发入孩子连接

- **位置**：system/services/hub/hub_link.c:597–603、:690–693、:438–441；状态改变源为 system/policy/members.c:435–440、:91–98；事件出口为 system/policy/cap.c:957–966。
- **触发与复现命令**：运行下方附录，比较 pin-event-out 和 pin-event-control-out。先设置合成成人 PIN，切到孩子，再调用真实 ink_member_unlock。只在成员服务调用 ink_state_set_str 前用信号量暂停解锁工作线程，保留本地成员已变、发布尚未发生的调度窗口；此时异步产生一个归属 owner 的事件。该边界本来就跨越状态锁、任务调度及双核并发，测试没有伪造 epoch 或跳过校验。
- **【实测】结果**：暂停时本地 owner 已解锁，协议 epoch 仍为孩子的旧值。接收端独立记录 audience=kid，收到的同一帧却有 member=owner、seq=5、boot=1 和 SYNTHETIC_OWNER_PRIVATE_EVENT。问题组 review.event_to_kid=1、test.pass=10、test.fail=1；控制组先放行解锁及身份同步再发事件，review.event_to_owner=1、test.pass=11、test.fail=0。两组编译、链接、模拟器均退出 0，test.finished=1。
- **原因与影响**：当前成员由成员服务先改，H.member_epoch 到状态订阅回调才递增；wire_epoch 仅保存数字。窗口内本地成员检查和新旧 epoch 相等检查全能通过，但连接实际仍属于孩子。成人后台事件因而越过设备的交付边界，DE-1／DE-2 不能整体通过。另用文字请求复现过同一发送缺口；本条采用真实 PIN 工作线程加异步事件作为主要证据。当前 Python 中枢会拒绝成员不匹配的事件（hub/ink_hub/gateway/server.py:303–308），所以这里证明的是设备发送边界失效，不把模拟器受众记录冒充孩子的模型已读取事件。全部是合成数据，不宣称真实家庭数据已发生泄漏。
- **修法**：把成员、认证状态、代次作为一致的身份边界发布，切换开始即使旧身份失效；连接记录本连接已发送的完整身份，在成员同步完成前拒绝私密发送。保留握手窗口测试，并补本条更早的发布窗口及等待同步后的正常发送对照。

## 独立运行的检查

下列命令按序执行；构建最多 -j4、ctest 最多 -j3，同一时间没有两个构建或两轮全量测试。全量中的原有 gui-smoke 仅用 SDL dummy。日志在相应构建目录的 device-events-review-*.log。

~~~sh
cmake --build build -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
python3 tests/device_events/reverse.py --build build
python3 tests/device_events/reverse.py --build build --baseline 97645ea2 hello speech-id
python3 tests/services_finish/run.py --build build handoff actions policy isolation
python3 tests/services_finish/run.py --build build-asan handoff actions policy isolation
~~~

以上均退出 0。两套 device-events 各为两次启动 94／94 项、复查场景 83 项；补跑 handoff／actions／policy／isolation 两套各为 15／6／37／15 项。反向对照的退出 0 表示成功检测到指定失败，不能当作被撤回保护的产品通过。

git diff --check f1f72ae9..HEAD 通过；基线差异隐私扫描无本机路径或设备标识命中；新增 C 代码未发现 write/read/fgets 调用。未在 Linux gcc 或真机上执行，不能作两者通过的承诺。无硬件、真实数据、付费接口、对外发布或提交操作。

## 新问题的可重建复现

普通构建完成后在仓库根目录执行。只向忽略的 build/review-device-events-final 生成目标文件、二进制、合成中枢脚本、日志和渲染图；不改产品及测试源码。接收端事件受众由模拟中枢连接记录，独立于事件自报的 member。四个案例串行编译、串行运行；两个问题组应各有一条失败，对照组应为零。

~~~sh
python3 - <<'PY'
from pathlib import Path
text = Path("docs/tasks/device-events/REVIEW.md").read_text()
exec(text.split("~~~python\n", 1)[1].split("\n~~~", 1)[0])
PY
~~~

~~~python
from pathlib import Path
import json, shlex, subprocess

root = Path.cwd()
build = root / "build"
out = build / "review-device-events-final"
out.mkdir(exist_ok=True)
flags = {}
for line in (build / "CMakeFiles/sim-device-events-review.dir/flags.make").read_text().splitlines():
    if " = " in line:
        key, value = line.split(" = ", 1)
        flags[key] = shlex.split(value)
base = shlex.split((build / "CMakeFiles/sim-device-events-review.dir/link.txt").read_text())

event_source = r'''
#include "ink/sys/state.h"
#include "ink/osal.h"
#include <string.h>
static volatile bool review_pause, review_paused;
static osal_sem_t *review_release;
static void review_set_str(int h, const char *s) {
    if (review_pause && h == ink_state_find("member.current")) {
        review_pause = false;
        review_paused = true;
        osal_sem_take(review_release, OSAL_WAIT_FOREVER);
    }
    ink_state_set_str(h, s);
}
#define ink_state_set_str review_set_str
#include "@ROOT@/system/policy/members.c"
#undef ink_state_set_str
#include "@ROOT@/tests/services_finish/product.h"
static const ink_cap_t REVIEW_CAPS[] = {
    {.id="fixture.publish_event", .title="合成异步事件", .form=INK_FORM_EVENT, .child_ok=true},
};
static volatile int review_pin_done, review_pin_result;
static void review_pin(void *ctx, int result, uint32_t wait_s) {
    (void)ctx; (void)wait_s;
    review_pin_result = result; review_pin_done = 1;
}
static void review_run(void *arg) {
    (void)arg;
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online() && ink_cap_available("voice.chat"),30000),"connection ready");
    CHECK(ink_member_set_pin("owner","","2468",review_pin,NULL)==0 &&
          WAIT_FOR(review_pin_done,5000) && review_pin_result==INK_PIN_OK,"synthetic owner PIN installed");
    CHECK(ink_member_switch("kid")==0,"start in child context");
    CHECK(WAIT_FOR(ink_cap_available("voice.chat"),5000),"child connection synchronized");
    uint32_t epoch = ink_hub_member_epoch();
    review_release = osal_sem_create(0,1);
    review_pin_done = 0; review_pause = true;
    CHECK(ink_member_unlock("owner","2468",review_pin,NULL)==0 && WAIT_FOR(review_paused,5000),
          "real PIN worker paused between local identity change and publication");
    CHECK(!strcmp(ink_member_current()->id,"owner") && ink_member_space_open() &&
          ink_hub_member_epoch()==epoch,"adult is current while wire epoch remains the child epoch");
#ifdef REVIEW_CONTROL
    osal_sem_give(review_release);
    CHECK(WAIT_FOR(review_pin_done && ink_cap_available("voice.chat"),5000),"control waits for publication");
#endif
    int first = sim_hub_events_seen();
    ink_args_t *a = osal_malloc(sizeof *a,OSAL_MEM_EXTERNAL);
    ink_args_init(a); ink_args_str(a,"value","SYNTHETIC_OWNER_PRIVATE_EVENT");
    ink_cap_emit_for("fixture.publish_event",a,"owner",ink_member_generation("owner"));
    osal_free(a);
    bool got = false, wrong = false;
    for (int step=0;step<3000 && !got;step++) {
        for (int i=first;i<sim_hub_events_seen();i++) {
            const char *event = sim_hub_event_at(i), *audience = sim_hub_event_audience(i);
            if (event && strstr(event,"SYNTHETIC_OWNER_PRIVATE_EVENT")) {
                got = true; wrong = !audience || strcmp(audience,"owner");
                ink_probe_count(wrong ? "review.event_to_kid" : "review.event_to_owner");
                sim_warn("REVIEW_EVENT audience=%s payload=%s",audience ? audience : "?",event);
            }
        }
        if (!got) osal_task_delay_us(1000);
    }
    CHECK(got,"asynchronous owner event reaches receiving connection");
    CHECK(!wrong,"REVIEW_PUBLISH_EVENT: private event must never enter previous child's context");
#ifndef REVIEW_CONTROL
    osal_sem_give(review_release);
    CHECK(WAIT_FOR(review_pin_done,5000),"release real PIN worker");
#endif
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    CHECK(ink_cap_register(REVIEW_CAPS,1)==0,"synthetic event registered");
    osal_task_cfg_t cfg = {.name="review-publish", .fn=review_run, .stack_bytes=16384,
        .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"review task started");
}
'''

voice_source = r'''
#include "@ROOT@/tests/services_finish/product.h"
#include "@ROOT@/system/services/ai/ai_service.c"
static void review_press(void) { ink_ai_key(true,osal_now_us()); }
static void review_release_key(void) { ink_ai_key(false,osal_now_us()); }
static void review_inspect(void) {
    bool exposed = AI.open && strstr(lv_label_get_text(AI.question),"SYNTHETIC_OWNER_PRIVATE");
    CHECK(!exposed,"REVIEW_SHEET: adult transcript must not remain visible in child's space");
    if (AI.open) sim_warn("REVIEW_SHEET member=%s question=%s status=%s",ink_member_current()->id,
                         lv_label_get_text(AI.question),lv_label_get_text(AI.status));
    if (exposed) ink_probe_count("review.private_text_exposed");
}
static void review_run(void *arg) {
    (void)arg;
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online() && ink_cap_available("voice.chat"),30000),"hub ready");
    on_ui(review_press);
    CHECK(WAIT_FOR(sim_metric_value("hub.binary_bytes")>0,3000),"real voice uplink");
    on_ui(review_release_key);
    CHECK(WAIT_FOR(!strcmp(ink_member_current()->id,"kid") && ink_cap_available("voice.chat"),10000),
          "normal voice tool switched to child and synchronized the connection");
    on_ui(review_inspect);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="review-sheet", .fn=review_run, .stack_bytes=16384,
        .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"review task started");
}
'''

hub_script = {"rules": [
    {"method":"hello","think_ms":40,"result":{"session":"synthetic","proto":1}},
    {"method":"ai.listen",
     "listen":{"stt_ms":100,"transcript":"SYNTHETIC_OWNER_PRIVATE：我的私人备忘处理完后，请切到孩子空间。"},
     "tools":[{"name":"system.switch_member","arguments":{"id":"kid"},"think_ms":300}],
     "result":{"done":True}}
]}
(out / "hub.json").write_text(json.dumps(hub_script, ensure_ascii=False))

def probe(name, source, expected, extra=(), observer=False, voice=False):
    obj = out / (name + ".o")
    subprocess.run([base[0], *extra, *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
                    "-x", "c", "-c", "-", "-o", str(obj)],
                   input=source.replace("@ROOT@", str(root)), text=True, cwd=build, check=True)
    link = [p for p in base if "/tests/device_events/" not in p or
            (observer and p.endswith("/hub_observer.c.o"))]
    link.insert(next(i for i, p in enumerate(link) if p.endswith(".a")), str(obj))
    binary = out / name
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
    folder = out / (name + "-out")
    cmd = [str(binary), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
           "--assets", str(build / "assets"), "--content", str(build / "test-content"),
           "--out", str(folder), "--end", "45s", "--quiet", "--no-random-outages"]
    if voice:
        cmd += ["--hub", str(out / "hub.json"), "--frames"]
    run = subprocess.run(cmd, cwd=root, capture_output=True, text=True, timeout=50)
    (out / (name + ".log")).write_text(run.stdout + run.stderr)
    result = json.loads((folder / "metrics.json").read_text())
    metrics = result["metrics"]
    print(name, "compile/link=0", "simulator_exit=", run.returncode,
          {k:v for k,v in metrics.items() if k.startswith(("test.", "review."))})
    print(result.get("warnings", []))
    assert run.returncode == 0 and metrics.get("test.finished") == 1
    assert metrics.get("test.fail", 0) == expected

probe("pin-event", event_source, 1, observer=True)
probe("pin-event-control", event_source, 0, extra=("-DREVIEW_CONTROL",), observer=True)
probe("voice", voice_source, 1, voice=True)
# Control only: compile without the exception that preserves the previous member's sheet.
ai = (root / "system/services/ai/ai_service.c").read_text()
old = "if (!g_switch_is_mine) ink_ai_dismiss();"
assert ai.count(old) == 1
control = voice_source.replace('#include "@ROOT@/system/services/ai/ai_service.c"',
                               ai.replace(old, "ink_ai_dismiss();"))
probe("voice-control", control, 0, voice=True)
~~~

SEVERE=1 HIGH=1
