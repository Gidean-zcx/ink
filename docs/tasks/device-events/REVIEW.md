# device-events 独立复查

日期：2026-11-06。任务基线 `f1f72ae9`，受检提交 `670dd6eb`；上一轮复查提交 `053a4cd5`。覆盖旧报告，只修改 REVIEW.md，不修改产品／测试源码，不提交。

**结论：复查不通过，严重 0、高 2。** 上一轮 DE-R3 的连接／事件发布窗口和 DE-R4 的普通语音换人浮层已修到位；新发现的 DE-R5、DE-R6 表明成员隔离仍未覆盖新请求的页面上下文及已完成回答的朗读缓存。执行报告披露的未完成功能继续不予验收，没有把它们另算成“冒充通过”的严重问题。

## 上一轮问题复核

| 编号 | 结论 | 独立证据 |
| --- | --- | --- |
| DE-R1 握手隔离（前轮高） | 指定问题已修 | 首次握手、重连、A→B→A、认证变化、同步失败及序列化期间换人均通过；hello／send-boundary 反向对照检出；`97645ea2` 的 hub 源码重现原失败 |
| DE-R2 声音失败提示（前轮高） | 已修 | 真实 downlink push／open 拒收后仍显示失败；扬声器、旧通知、首段文字顺序通过；对应反向对照和 `97645ea2` 的 AI 源码均检出 |
| DE-R3 发布窗口（上一轮高） | 指定的事件受众／请求问题已修 | 身份与 epoch 同一临界区更新；真实工作线程发布前的双向换人、认证关闭／重开通过；`publish-epoch`、`wire-identity` 检出；`053a4cd5` hub 源码重现旧受众错误。声音播放仍有 DE-R6 |
| DE-R4 AI 展示隔离（上一轮严重） | 指定的普通语音换人问题已修 | 真实语音上传、STT 和 tools/call 换人后旧浮层／回答缓冲清除；本次查看最终 GC16 图，未见旧私人转写。`ui-*`、`voice-handoff` 检出；`053a4cd5` AI＋动作源码重现旧问题。新提问的上下文另见 DE-R5 |

## 逐条验收

| 条目 | 结论 | 核对结果 |
| --- | --- | --- |
| G1 全量、内存、预算 | 既有检查通过，新增复现不通过 | 【实测】普通／ASan+UBSan 构建均退出 0，全量各 103/103，42.24／77.89 秒，无跳过；预算、确定性、分层均过。新增边界见 DE-R5、DE-R6 |
| G2 原子能力、AI 原生、分层 | 调用通路符合，成员隔离不通过 | 【实现】tools/call 仍经能力策略，绑定成员／交付代次，无新用户操作或新页面漏接能力。成员真值、连接身份、UI 执行器职责清楚；页面上下文和声音输出还没有使用同一个身份边界 |
| G3 依赖状态 | 已更新，表述需补边界 | 【实现】ui-dependencies.md 仅改“成员、PIN”状态格和报告链接，未改设计判断；不能据此认为发布前的所有隔离都完成 |
| G4 页面要求 | 不适用 | 无新页面，不要求十屏设计稿对照。本次仅核实 DE-R4 的最终命令行渲染图；未启动带窗口 GUI |
| G5 范围；总规则 §1／§4 | 范围符合；隔离不符合 | 【实现】差异为服务、策略、接口、必要的模拟／合成中枢、测试和任务资料，无新增常驻任务、越层引用、硬件操作或外发。DE-R5／DE-R6 违反成员隔离规则 |
| DE-1 device/event seq、boot、member、ACK | 本批字段／重发通过 | 【实测】两次持久 Flash 启动各 94 项，boot 递增；丢 ACK、重连保持同一事件标识，错误 boot／超前 ACK 被拒绝；实际接收成员与 payload 独立核对。8 条内存环不能当持久结果库 |
| DE-2 ink/member、_meta、实时输出 | 不通过 | 【实测】入站、排队、A→B→A、旧连接、旧请求、旧实时会话的已有检查通过；新的合法身份仍可能携带旧页面或继续旧声音，见两条问题 |
| DE-3 budget_ms、录音与背压 | 本批实现通过 | 【实测】线上剩余 budget、排队不重置 deadline、30 秒录音上限、下行失败终止与文案均过，反向对照有效；未放宽产品期限或预算 |
| DE-4 caps/list 与 actions | 分页通过；可选字段行为未齐 | 【实测】三页取回、同 rev 原子替换、不透明 cursor、重复 cursor／失败保留旧表、完整 actions 通过。【实现】重复名和 32 项上限拒绝、暂存放 PSRAM。只消费 name／form，offline／timeout_ms 只是容忍而未落实对应策略；actions_more 可选优化未做 |
| DE-5 sim_hub、假设备、契约 | 已实现的合成范围通过 | 【实测】两套 hub-contract 各 125 项，hub-json-crosscheck、hub-simulator-e2e 通过。proto1 仍固定合成儿童，自报 authed 不授予成人权限，不能当正式成员证明 |
| DE-6 D-105 | 部分，未完成 | 【实现＋实测】asked、ask_by_hand、活动记录回归保留；undo_ms=3000 和 asked 短信／拨号明确回 -32021，不是三秒撤销后执行 |
| DE-7 反向对照 | 26 项及四项修前源码复现有效 | 【实测】逐项编译、链接、模拟器退出 0，指定断言失败且 test.fail>0。hello 反向在首个窗口检出时 test.finished=0，不能说整套破坏后的场景完成 |

### 合同中其余项目

按 contracts-step1.md“要设备改的”和概要设计 §13，以下逐项保留状态，不因后续依赖而自动缩减任务：

- **未完成**：中枢成员表全部分页替换、member.reset_pin；正式成人解锁证明；三秒撤销窗口；sync/*、dataset／complete／hub_instance／restore_token、一致清单、原子安装及 history_gap；末端计时上传。
- **未新增／未验证**：内容包；远程画面的翻页、长按、提交文字；真 Opus／JPEG 模式；可选 rt.close reason／heard_frames；计划短连。真中枢桥仅已验证本机合成链路。
- **基线已有且回归通过**：远端 result、完整 actions 列表、语音面板和 asr.stream。
- **明确排除**：配对状态及 hello v2、公钥、地址／令牌、首次开机；确认结果持久化、confirm.status／confirm/request；块传输。与它们相依的未完成功能仍不标通过。

已有测试变更核对：ai_tools 的 done／answers 改为精确 1／3，并要求切换及旧轮取消各 1，符合换人取消旧轮的规则，资源上限没放宽；races 改查实际接收成员；starts／actions 适配绑定身份接口；policy 补当前 member／epoch 和真实握手；handoff 显式关闭认证以适配 D-95。未见删除旧测试或用 sleep 掩盖产品时序。现有“新成员新轮”测试只看新提问／回答是否继续，未检查线上 page.text；现有排队声音测试在 ink_member_switch 返回后检查，状态通知已经完成，测不到 DE-R6。

## 已证实的新问题

### DE-R5（高）：新成员提问仍能上传旧成员的页面快照

- **位置**：`system/services/ai/ai_service.c:530`、`:771`；`system/ui/ui_task.c:292`；`system/ui/app_registry.c:285`。
- **【实测】**：用正常页面 API 建立带合成成人私密内容的页面，真实成员工作线程通过 `ink_member_unlock("kid", "", ...)` 进入无 PIN 的孩子空间，并暂停在状态发布前，再在 UI 任务调用正常文字提问。接收端独立记录为 kid、epoch=2、authed=true，但线上 `ai.ask.page.text` 含成人页面的 `SYNTHETIC_OWNER_PRIVATE_PAGE`。问题组 9 通过、1 失败；等待状态发布及旧页面销毁的对照组 11 通过、0 失败。普通和 ASan+UBSan 两套结果一致，编译、链接、模拟器退出 0，`test.finished=1`。
- **复现命令**：执行文末脚本，检查 `snapshot-case` 与 `snapshot-control`。
- **原因**：新轮只捕获当前成员身份，随后从尚未交接的 UI 页面取快照；成员通知异步销毁旧页，快照没有绑定页面所属身份。连接身份校验因新轮身份确实正确而放行。上一轮旧回答浮层修复没有覆盖新问题携带的旧页面上下文。
- **修法**：页面快照绑定其实际所属成员、代次，并与新轮身份一致；交接完成前拒绝或延后提问。校验必须覆盖页面、输入框和发送前的整个请求，不能只增加等待时间。补充成员交接期间新请求的实际线上 `page.text` 断言。

### DE-R6（高）：已完成回答的朗读缓存仍依赖状态通知撤销

- **位置**：`system/services/audio/audio_service.c:526`、`:732`；身份先变、后通知的路径为 `system/policy/members.c:475`～`:483`。
- **【实测】**：通过正常 AI 按下／松开入口、麦克风上传、模拟中枢 TTS 和最终回执完成一轮成人语音；在朗读缓存尚未播完时，经真实成员工作线程切到孩子并暂停状态发布。先留 200 ms 让在途声音退出，再观察 800 ms，扬声器仍增加 810 ms 播放；问题组 10 通过、1 失败。先释放状态发布的对照组增加 0 ms、11 通过、0 失败。普通和 ASan+UBSan 两套结果一致，编译、链接、模拟器退出 0，`test.finished=1`。
- **复现命令**：执行文末脚本，检查 `speech-case` 与 `speech-control`。
- **原因与边界**：播放循环只检查 `g_output_revoked`，缓存未绑定成员身份；这个标记由 `member.current`／`member.authed` 通知设置。新身份的代次虽然已立即失效，已经结束实时会话的朗读仍可继续播放。复现刻意延迟发布以放大调度窗口，810 ms 是该受控窗口的结果，不声称普通切换固定泄露这么久。
- **修法**：给待播流、解码后 PCM 和最终扬声器提交绑定成员／代次，在播放出口依声音类型复核当前身份与权限，及时 abort 私密朗读；不能只依赖稍后到达的状态回调。增加“回答已结束、缓存未播完、身份已变而状态发布延迟”的回归。

普通 `system.switch_member` 能力后排队物理 AI 键的补充场景没有复现 DE-R5：该能力同步回主页。报告不将受控发布窗口泛化为每次切换都会泄露。

## 本次执行命令与证据范围

以下构建／测试按序执行，构建最多 `-j4`、ctest 最多 `-j3`，未并行运行两次构建或两轮全量。全量的 gui-smoke 使用 SDL dummy，无窗口。两套缓存均 INK_WERROR=ON，内存版另开 INK_SANITIZE=ON；依赖使用题设 INK_DEPS，合成中枢用已配置的锁定 Python 环境。

~~~sh
cmake --build build -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
python3 tests/device_events/reverse.py --build build
python3 tests/device_events/reverse.py --build build --baseline 053a4cd5 publish-audience voice-private
python3 tests/device_events/reverse.py --build build --baseline 97645ea2 hello speech-id
python3 tests/services_finish/run.py --build build handoff actions policy isolation playback
python3 tests/services_finish/run.py --build build-asan handoff actions policy isolation playback
git diff --check f1f72ae9..HEAD
~~~

上述命令均退出 0。两套 device-events 均为两次启动各 94 项、握手／声音 87 项、身份隔离 108 项，零失败且 test.finished=1；补充服务回归各为 15／6／37／15／12 项。反向脚本的退出 0 表示成功检出指定失败，不是破坏后的产品通过。

相对 f1f72ae9 的提交差异按总规则隐私正则扫描为 0 命中；新增 C 行未发现 write／read／fgets 调用。未在 Linux gcc、真机或真实厂商上执行，不承诺这些环境通过。未接触硬件、真实家庭数据、付费接口或远端写入；所有复现文件／产物在临时或忽略目录，仓库跟踪文件只改本报告。

## 两条新问题的可重建复现

在普通构建和内存构建已完成后执行。脚本只生成临时夹具、目标文件、二进制和日志；不替换产品源码。唯一调度钩子复用已有 member_boundary.c，在真实成员工作线程写状态前暂停；接收成员由 hub_observer.c 独立观察。每组 case 暂停发布，control 则先释放发布并完成交接。DE-R6 的等待只用于观察声音是否仍输出，不是产品修复或测试绕过。

~~~sh
python3 - <<'PY'
from pathlib import Path
text = Path("docs/tasks/device-events/REVIEW.md").read_text()
exec(text.split("~~~python\n", 1)[1].split("\n~~~", 1)[0])
PY
~~~

内存检查复现使用同一命令，并在 python3 前加 `INK_REVIEW_BUILD=build-asan`。本次已执行两套：snapshot-case／speech-case 各仅 1 个目标失败，control 均为零；编译、链接、模拟器均退出 0，test.finished=1，无 sanitizer 诊断。

~~~python
from pathlib import Path
import json, os, shlex, subprocess, tempfile
root = Path.cwd()
build = (root / os.environ.get("INK_REVIEW_BUILD", "build")).resolve()
out = Path(tempfile.mkdtemp(prefix="ink-device-events-review-"))
sources = {
'snapshot': r'''
#include "@ROOT@/tests/services_finish/product.h"
void test_member_pause(void), test_member_release(void), test_member_pause_done(void), test_member_change(const char *id);
bool test_member_paused(void), test_member_change_done(void);
static char private_page[128];
static void create_private(lv_obj_t *screen, void *arg) {
    (void)arg;
    CHECK(!strcmp(ink_member_current()->id, "owner"), "private page is created by the owner");
    snprintf(private_page,sizeof private_page,"SYNTHETIC_OWNER_PRIVATE_PAGE");
    lv_label_set_text(lv_label_create(screen),private_page);
}
static void destroy_private(void) { private_page[0]=0; }
static int snapshot_private(char *out,size_t cap) { return snprintf(out,cap,"%s",private_page); }
static const ink_activity_t PAGE={.name="synthetic-private",.app="reader",.create=create_private,.destroy=destroy_private,.snapshot=snapshot_private};
static void open_private(void) { ink_ui_push(&PAGE,NULL); }
static void ask_child(void) {
    CHECK(!strcmp(ink_member_current()->id,"kid"),"new question belongs to the child");
    ink_ai_ask_text("SYNTHETIC_CHILD_QUESTION");
}
static volatile bool pin_done;
static void pin_told(void *ctx,int result,uint32_t wait_s) { (void)ctx; (void)wait_s; CHECK(result==INK_PIN_OK,"real PIN worker accepts child space"); pin_done=true; }
static void run(void *arg) {
    (void)arg;
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online()&&ink_cap_available("voice.chat"),30000),"owner connection ready");
    on_ui(open_private);
    test_member_pause();
    CHECK(ink_member_unlock("kid","",pin_told,NULL)==0,"ordinary member unlock queued on real worker");
    CHECK(WAIT_FOR(test_member_paused(),5000),"real member switch is paused before publication");
#ifdef REVIEW_CONTROL
    test_member_release();
    CHECK(WAIT_FOR(pin_done,5000),"control publishes member change");
    test_member_pause_done();
    CHECK(WAIT_FOR(strcmp(ink_ui_page_name(),"synthetic-private"),5000),"control waits for old page destruction");
#endif
    on_ui(ask_child);
    CHECK(WAIT_FOR(sim_metric_value("test.wire.asks.kid")>0,10000),"new question is delivered to child receiving context");
    const char *wire=sim_hub_last_ask();
    bool leaked=wire&&strstr(wire,"SYNTHETIC_OWNER_PRIVATE_PAGE");
    ink_probe_value("review.private_snapshot_leaked",leaked);
    CHECK(!leaked,"new child request must not upload cached owner page snapshot");
#ifndef REVIEW_CONTROL
    test_member_release();
    CHECK(WAIT_FOR(pin_done,5000),"member publisher completes normally");
    test_member_pause_done();
#endif
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="snapshot-review",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"review task started");
}
''',
'speech': r'''
#include "@ROOT@/tests/services_finish/product.h"
void test_member_pause(void),test_member_release(void),test_member_pause_done(void);
bool test_member_paused(void);
void test_ai_review_voice(void),test_ai_review_release(void);
static volatile bool pin_done;
static void pin_told(void *ctx,int result,uint32_t wait_s){(void)ctx;(void)wait_s;CHECK(result==INK_PIN_OK,"real member unlock finishes");pin_done=true;}
static void warmup(void){ink_ai_ask_text("SYNTHETIC_WARMUP");}
static void run(void *arg){
    (void)arg;ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online()&&ink_cap_available("voice.chat"),30000),"connection ready");
    on_ui(warmup);CHECK(WAIT_FOR(sim_metric_value("ai.answers")>0,10000),"text turn completes");
    double answers=sim_metric_value("ai.answers"),bytes=sim_metric_value("hub.binary_bytes");
    on_ui(test_ai_review_voice);
    CHECK(WAIT_FOR(sim_metric_value("hub.binary_bytes")>bytes,3000),"real voice frames uploaded");
    on_ui(test_ai_review_release);
    CHECK(WAIT_FOR(sim_metric_value("ai.answers")>answers,15000),"voice answer completes with speech queued");
    test_member_pause();CHECK(ink_member_unlock("kid","",pin_told,NULL)==0,"real member worker queued");
    CHECK(WAIT_FOR(test_member_paused(),5000)&&!strcmp(ink_member_current()->id,"kid"),"identity changed before publication");
#ifdef REVIEW_CONTROL
    test_member_release();CHECK(WAIT_FOR(pin_done,5000),"control waits for publication");test_member_pause_done();
#endif
    delay_ms(200);
    double before=sim_audio_played_ms();
    delay_ms(800);
    double extra=sim_audio_played_ms()-before;
    ink_probe_value("review.old_speech_after_handoff_ms",extra);
    CHECK(extra==0,"old adult speech must not play in the child's space: %.0f ms",extra);
#ifndef REVIEW_CONTROL
    test_member_release();CHECK(WAIT_FOR(pin_done,5000),"publisher completes");test_member_pause_done();
#endif
    ink_probe_count("test.finished");
}
void ink_main(void){product_main();osal_task_cfg_t cfg={.name="speech-review",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"review task started");}
''',
}
hub_rules = {'rules': [{'method': 'hello', 'think_ms': 40, 'result': {'session': 'synthetic', 'proto': 1}}, {'method': 'ai.ask', 'think_ms': 50, 'result': {'done': True}}, {'method': 'ai.listen', 'think_ms': 50, 'listen': {'stt_ms': 50, 'transcript': 'SYNTHETIC_OWNER_QUESTION', 'tts_first_ms': 50, 'tts_chars_per_s': 8, 'tts_speed': 2}, 'stream': {'notify': 'ai.delta', 'interval_ms': 50, 'chunks': ['成人的合成私人回答请勿对其他成员继续播放。']}, 'result': {'done': True}}]}
hub = out / "hub.json"
hub.write_text(json.dumps(hub_rules, ensure_ascii=False))
flags = {}
for line in (build / "CMakeFiles/sim-device-events-isolation.dir/flags.make").read_text().splitlines():
    if " = " in line:
        key, value = line.split(" = ", 1)
        flags[key] = shlex.split(value)
base = shlex.split((build / "CMakeFiles/sim-device-events-isolation.dir/link.txt").read_text())
for name, text in sources.items():
    src = out / (name + ".c")
    src.write_text(text.replace("@ROOT@", str(root)))
    for control in (False, True):
        label = name + ("-control" if control else "-case")
        obj, binary = out / (label + ".o"), out / label
        subprocess.run([base[0], *flags["C_DEFINES"], *(["-DREVIEW_CONTROL"] if control else []),
                        *flags["C_INCLUDES"], *flags["C_FLAGS"], "-c", str(src), "-o", str(obj)],
                       cwd=build, check=True)
        link = base.copy()
        link[next(i for i, item in enumerate(link) if item.endswith("/isolation.c.o"))] = str(obj)
        link[link.index("-o") + 1] = str(binary)
        subprocess.run(link, cwd=build, check=True)
        folder = out / (label + "-out")
        run = subprocess.run([str(binary), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
                              "--assets", str(build / "assets"), "--content", str(build / "test-content"),
                              "--out", str(folder), "--hub", str(hub), "--end", "45s", "--quiet",
                              "--no-random-outages"], cwd=root, capture_output=True, text=True, timeout=60,
                             env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})
        (out / (label + ".log")).write_text(run.stdout + run.stderr)
        data = json.loads((folder / "metrics.json").read_text())
        metrics = data["metrics"]
        print(label, "exit", run.returncode,
              {k: v for k, v in metrics.items() if k.startswith(("test.", "review."))},
              data.get("warnings"), flush=True)
        expected = 0 if control else 1
        assert run.returncode == 0 and metrics.get("test.finished") == 1
        assert metrics.get("test.fail", 0) == expected
        measure = "review.private_snapshot_leaked" if name == "snapshot" else "review.old_speech_after_handoff_ms"
        assert (metrics[measure]["max"] > 0) == (not control)
print("artifacts:", out)
~~~

SEVERE=0 HIGH=2
