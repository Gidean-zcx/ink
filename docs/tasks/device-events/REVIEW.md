# device-events 独立复查

复查日期：2026-11-06。基线 `f1f72ae9`，受检提交 `013efa53`。范围为任务清单开头 G1～G5、device-events 一节及总规则。只修改本报告，不改产品代码、不提交。

结论：**本轮复查不通过，严重 0、高 2**。一项是握手期间换人导致上下文串成员；另一项是声音拒收提示被丢弃，且现有测试无法发现。未完成范围另列，不混作已验收。

## 验收核对

- 原 REVIEW.md 为空，没有本任务上一轮问题清单；先复核本轮 `b4c71d45` 声称补强的取消、隔离和失败边界。
- 已读取任务书、执行报告、两端约定和概要设计 §13。报告明确承认的未完成项会独立列出，不能由测试通过推导为整项完成。
- 普通构建和 ASan/UBSan 构建均退出 0；两轮全量 ctest 分别 103/103 通过（43.02 秒、75.99 秒）。构建 `-j4`，ctest `-j3`，串行执行；仅原有 SDL dummy 无窗口检查。
- 原有 12 个反向对照全部复现；补跑 handoff、actions、policy、isolation，分别 15、6、36、15 个检查通过。另证实以下两项高等级问题，不能仅凭原测试全过验收。

| 条目 | 复查结论 | 独立证据 |
| --- | --- | --- |
| G1 全量、内存、预算 | 原有检查通过，存在漏测 | 两套 103/103，无跳过；两套 INK_WERROR=ON，内存构建另开 INK_SANITIZE=ON；预算上限未放宽。新增复现见 DE-R1、DE-R2 |
| G2 原子能力、AI 原生、分层 | 本次增量符合 | tools/call 仍进能力策略关口；新增接口用于传输身份；未新增需要另行开放的用户操作或页面；能力目录、策略、分层测试通过 |
| G3 依赖状态 | 已更新，表述需随修复校正 | 仅改 ui-dependencies.md 的“成员、PIN”状态格并链接报告；不能据此认为握手时序也已验证 |
| G4 页面要求 | 不适用 | 服务层任务无新页面，不要求 10 屏设计稿对照；未启动带窗口 GUI |
| G5 范围、总规则 §1/§4 | 未见范围外产品改动 | 核对 27 个差异文件：服务、策略、接口、必要的模拟中枢／合成服务端和测试；无硬件操作、付费调用、提交或外发 |
| DE-1 seq/boot/member、事件 ACK | 已实现部分通过 | 两次持久 Flash 启动 boot 递增；ACK 丢失重连保留事件身份；错误 boot／超前 ACK 拒绝；8 条内存环的既定丢旧边界未扩大承诺 |
| DE-2 成员和交付 | **不通过** | 入站缺失／错误元数据、分配和排队期间换人、A→B→A、会话及排队语音取消的回归通过；握手期间换人可串到另一成员上下文，见 DE-R1 |
| DE-3 时间与背压 | **部分通过** | 剩余 budget、排队不重置期限、30 秒录音上限及其反向对照通过；声音拒收后的提示会丢失，见 DE-R2 |
| DE-4 caps/list 全分页 | 本批实现通过 | 实际 sim_hub 三页取回；不完整页不发布、rev 改变／重复 cursor／超时保留旧表；代码核对重复名称和 32 项上限；暂存表在 PSRAM；完整 actions 回归通过 |
| DE-5 假设备、契约和桥接 | 现有合成范围通过 | hub-contract 内部 125 项、hub-json-crosscheck、hub-simulator-e2e 均实际执行通过；proto1 固定合成儿童，未把它当成正式成人身份验证 |
| DE-6 D-105 | 未完成，报告已说明 | asked、ask_by_hand、活动记录沿用基线且策略回归通过；undo_ms=3000 仅校验并拒绝，不是三秒可撤销执行 |
| DE-7 反向对照 | 12 个既有对照有效，覆盖不完整 | 每个指定保护撤回后确有断言失败；仅删掉声音错误通知时原测试仍全过，见 DE-R2 |

任务书引用范围还有明确未完成项：`members/list` 全页替换与 `member.reset_pin`、正式成人解锁证明、三秒撤销窗口、`sync/*` 和数据集元数据／一致清单／原子快照／history_gap、设备末端计时上传，以及 §13 列出的内容包、网页新动作、真 Opus/JPEG、可选的 rt.close 附加字段和计划短连。报告已披露这些差距；本次不把“坦承未完成”重复计作虚报严重问题，也不把它们从任务书中自行划掉。配对、确认持久化、块传输是任务书明确排除项。

测试期望变化已逐项核对：ai_tools 改为换成员取消旧轮，并新增恰好一次执行／取消断言；races 改查接收时连接受众；starts/actions/policy 适配身份接口；handoff 显式关闭认证以适配基线 D-95。未发现删除旧测试、抬高资源上限或用新增 sleep 掩盖实现时序。录音测试的 31 秒等待是在虚拟时间内越过真实上限。

附加边界检查：把合成 `/user` 填至 free_bytes=0、写入明确失败后，原版 ink-sim 仍可重启（退出 0），未将“满盘一定无法更新 boot”这一猜测列为问题。两份构建日志和所有复现产物仅在忽略的 build 目录；没有真机或 Linux gcc 实测。提交差异隐私扫描 0 命中，新增代码无 write/read/fgets 调用；`git diff --check f1f72ae9..HEAD` 通过。

本次独立运行的既有检查（按序执行，全部退出 0）：

```sh
cmake --build build -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
python3 tests/device_events/reverse.py --build build
python3 tests/services_finish/run.py --build build handoff actions policy isolation
```

## 已证实的问题

### DE-R1（高）：hello 等待期间的成员变更被消费，新成员问题发到旧成员上下文

- 位置：`system/services/hub/hub_link.c:472`、`:482`、`:324`。
- 触发：成人发出 hello 后、回执到达前切到孩子。`member_changed` 被 exchange 清零，但只有 HS_ONLINE 才发 ink/member；HS_HELLO 没有保留待同步状态。握手回执随后直接上线、发送排队请求，`ink_hub_member_matches` 只检查本地身份，未检查中枢已知身份。
- 【实测】真实启动、成员切换和 `ink_ai_ask_text` 路径；模拟中枢只增加接收时身份观测。双向均复现，附录保留“孩子握手期间切回成人”：`review.hello_epoch=2`、`review.new_epoch=3`、`review.member_notifications=0`、`review.ask.kid=1`，成人问题进入孩子上下文，隔离断言失败 1 次。对照组在线后再切换，问题正确进入 owner，test.fail=0。模拟器均退出 0，判断依据是 test.fail，不把退出码当通过。全为合成数据，未连接真实家庭；正式账号系统尚未实现，此处证明的是设备协议路由错误。
- 复现命令：运行附录重建脚本，再执行 `build/review-device-events/hello --profile simulator/profiles/metalio-plus.json --assets build/assets --content build/test-content --out build/review-device-events/hello-out --end 40s --quiet --no-random-outages`，检查 metrics.json 的 test.fail=1 和 review.ask.kid=1。
- 影响：握手或重连与换人重叠时，页面和问题可能按另一成员交给 AI；同时中枢回来的工具调用因旧 epoch 被拒。DE-2 的“出站快照、RPC 不重新绑定成员”不能判整体通过。
- 修法：保存本连接已发送的 hello/member 身份；上线发送任何成员数据前确保当前身份已同步。HELLO 期间变更应保留到上线补发，或关闭旧握手后重新连接。增加上述握手窗口回归，并验证普通在线切换仍通过。

### DE-R2（高）：下行拒收的提示混用两种编号，且原测试测不到“显示失败”

- 位置：`system/services/ai/ai_service.c:351`（发送 AI.session_seq），`:237`（kind=4 却匹配 AI.stream）；测试漏项在 `tests/device_events/ai_boundary.c:46`；对应 `REPORT.md:11` 的“中止该段声音并显示失败”。
- 触发：先完整打字提问一轮，再按 AI 键语音提问；两种编号成为 session_seq=2、stream=1。在真实模拟中枢下行的第一帧注入一次 ink_audio_stream_push 拒收，实际走 rt 分发和 UI 队列。
- 【实测】声音确实中止，ai.downlink_overflow=1；但 kind=4 通知因为 2≠1 被 ui_apply 丢掉，ai.speech_failed=0。等这一轮真正完成，界面仍显示“回答完成 · 按 AI 键继续问，按 HOME 关闭”。直接以语音开始的对照组两种编号都是 1，会正确提示声音失败。两组模拟器退出均为 0，问题组 test.fail=1，对照组 test.fail=0。
- 测试有效性：【实测】仅在临时编译副本删除这一个 post(...)，保留声音中止保护，原 device-events 测试仍在两次启动中全部通过。现有断言只看 tts_stream、tts_done_for 和 overflow 计数；原 reverse.py 的 downlink 对照撤掉整个处理分支，只能证明“中止”被测到，证明不了“提示”被测到。按复查规则“测试实际测不到”定为高。
- 复现命令：运行附录重建脚本；`build/review-device-events/downlink-real-typed --profile simulator/profiles/metalio-plus.json --assets build/assets --content build/test-content --out build/review-device-events/downlink-real-typed-out --end 55s --quiet --no-random-outages`。检查 test.fail=1、ai.downlink_overflow=1、缺失 ai.speech_failed，及 warnings 中的实际状态文案。
- 修法：错误通知使用与 kind=4 接收侧一致的会话身份，并在完成后的状态中保留失败提示；补“先文字／听写或其他实时会话、再语音”的真实往返测试，断言界面提示，增加只撤掉错误通知的反向对照。

## 高等级问题可重建复现

在仓库根目录，普通构建完成后运行下列命令。只在忽略的构建目录生成目标文件、二进制和输出；产品源码不作修改。DE-R1 的观测器读取模拟中枢从 hello/ink/member 学到的连接身份，不采信问题内容自报的成员。DE-R2 只对实际到达的音频帧注入一次拒收；对照组不先打字提问。末尾还重建仅删除错误提示的测试副本，证明原断言漏测。

```sh
python3 - <<'PY'
from pathlib import Path
p = Path('docs/tasks/device-events/REVIEW.md').read_text()
exec(p.split('```python\n', 1)[1].split('\n```', 1)[0])
PY
```

```python
from pathlib import Path
import json, shlex, subprocess
root = Path.cwd()
build = root / 'build'
out = build / 'review-device-events'
out.mkdir(exist_ok=True)
flags = {}
for line in (build / 'CMakeFiles/sim-device-events.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
base = shlex.split((build / 'CMakeFiles/sim-device-events.dir/link.txt').read_text())
main = r'''
#include "@ROOT@/tests/services_finish/product.h"
#include "@ROOT@/system/services/hub/hub_link.c"
static void review_ask(void) { ink_ai_ask_text("SYNTHETIC_ADULT_PRIVATE_QUESTION"); }
static void review_run(void *arg) {
    (void)arg;
    CHECK(ink_member_switch("kid") == 0, "start as synthetic child");
    ink_hub_demand();
#ifdef REVIEW_CONTROL
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "control handshake completed");
#else
    for (int i = 0; i < 30000 && get_state() != HS_HELLO; ++i) osal_task_delay_us(1000);
    CHECK(get_state() == HS_HELLO, "entered real hello wait");
#endif
    ink_probe_value("review.hello_epoch", ink_hub_member_epoch());
    CHECK(ink_member_switch("owner") == 0, "switch to synthetic adult");
    ink_probe_value("review.new_epoch", ink_hub_member_epoch());
    CHECK(WAIT_FOR(ink_hub_online() && ink_cap_available("voice.chat"), 30000), "online after handoff");
    on_ui(review_ask);
    CHECK(WAIT_FOR(sim_metric_value("review.ask.owner") + sim_metric_value("review.ask.kid") > 0, 3000), "private ask delivered");
    CHECK(sim_metric_value("review.ask.kid") == 0 && sim_metric_value("review.ask.owner") == 1,
          "REVIEW_HELLO: adult question must reach adult context");
    ink_probe_value("review.member_notifications", sim_metric_value("hub.requests.ink/member"));
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="review", .fn=review_run, .stack_bytes=16384,
        .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "review task started");
}
'''
observer = r'''
#define sim_hub_receive review_original_receive
#include "@ROOT@/simulator/environment/sim_hub.c"
#undef sim_hub_receive
void sim_hub_receive(struct hal_ws *ws, uint64_t t, const uint8_t *data, size_t len, bool binary) {
    if (!binary) {
        cJSON *msg = cJSON_ParseWithLength((const char *)data, len);
        const char *method = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(msg, "method"));
        if (method && !strcmp(method, "ai.ask")) {
            char key[80];
            snprintf(key, sizeof key, "review.ask.%s", MC[member_slot(ws)].member);
            sim_metric_add(key, 1);
        }
        cJSON_Delete(msg);
    }
    review_original_receive(ws, t, data, len, binary);
}
'''
def compile_obj(name, source, extra=()):
    obj = out / (name + '.o')
    cmd = [base[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
           *extra, '-x', 'c', '-c', '-', '-o', str(obj)]
    subprocess.run(cmd, input=source.replace('@ROOT@', str(root)), text=True, cwd=build, check=True)
    return str(obj)
obs = compile_obj('observer', observer)
for case, extra in [('hello', ()), ('online-control', ('-DREVIEW_CONTROL',))]:
    obj = compile_obj(case, main, extra)
    link = [part for part in base if '/tests/device_events/' not in part]
    binary = out / case
    link[link.index('-o') + 1] = str(binary)
    pos = next(i for i, part in enumerate(link) if part.endswith('.a'))
    link[pos:pos] = [obj, obs]
    subprocess.run(link, cwd=build, check=True)
    folder = out / (case + '-out')
    run = subprocess.run([str(binary), '--profile', str(root / 'simulator/profiles/metalio-plus.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(folder),
        '--end', '40s', '--quiet', '--no-random-outages'], cwd=root, capture_output=True, text=True, timeout=60)
    (out / (case + '.log')).write_text(run.stdout + run.stderr)
    result = json.loads((folder / 'metrics.json').read_text())
    metrics = result['metrics']
    print(case, 'process_exit=', run.returncode,
          {k: v for k, v in metrics.items() if k.startswith(('test.', 'review.'))})
    print(result.get('warnings', []))
    assert run.returncode == 0 and metrics.get('test.finished') == 1
    assert metrics.get('test.fail', 0) == (1 if case == 'hello' else 0)

audio = r'''
#include "ink/sys/audio.h"
static bool review_fail_next;
static int review_push(int id, const uint8_t *data, size_t len) {
    if (review_fail_next) { review_fail_next = false; return -1; }
    return ink_audio_stream_push(id, data, len);
}
#define ink_audio_stream_push review_push
#include "@ROOT@/system/services/ai/ai_service.c"
#undef ink_audio_stream_push
#include "@ROOT@/tests/services_finish/product.h"
static void typed(void) { ink_ai_ask_text("SYNTHETIC_FIRST_TYPED_TURN"); }
static void begin_voice(void) { ink_ai_key(true, osal_now_us()); }
static void end_voice(void) {
    ink_probe_value("review.ai_seq", AI.session_seq);
    ink_probe_value("review.rt_sid", AI.stream);
    review_fail_next = true;
    ink_ai_key(false, osal_now_us());
}
static void inspect(void) {
    CHECK(AI.speech_failed, "REVIEW_DOWNLINK: real rejected downlink must be visible on current AI sheet");
    sim_warn("REVIEW_DOWNLINK status: %s", lv_label_get_text(AI.status));
    ink_ai_dismiss();
}
static void review_run(void *arg) {
    (void)arg;
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online() && ink_cap_available("voice.chat"), 30000), "hub ready");
#ifdef REVIEW_TYPED_FIRST
    on_ui(typed);
    CHECK(WAIT_FOR(sim_metric_value("ai.answers") == 1, 10000), "typed turn completes first");
#endif
    double answers = sim_metric_value("ai.answers");
    on_ui(begin_voice);
    delay_ms(1000);
    on_ui(end_voice);
    CHECK(WAIT_FOR(sim_metric_value("ai.downlink_overflow") == 1, 10000), "real downlink refusal observed");
    CHECK(WAIT_FOR(sim_metric_value("ai.answers") > answers, 25000), "real voice roundtrip finished");
    on_ui(inspect);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="review-audio", .fn=review_run, .stack_bytes=16384,
        .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "review task started");
}
'''
def run_probe(case, link, end, flash=None):
    binary = out / case
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
    folder = out / (case + '-out')
    cmd = [str(binary), '--profile', str(root / 'simulator/profiles/metalio-plus.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(folder),
        '--end', end, '--quiet', '--no-random-outages']
    if flash:
        cmd.extend(['--flash-image', str(flash)])
    run = subprocess.run(cmd, cwd=root, capture_output=True, text=True, timeout=60)
    (out / (case + '.log')).write_text(run.stdout + run.stderr)
    result = json.loads((folder / 'metrics.json').read_text())
    metrics = result['metrics']
    print(case, 'process_exit=', run.returncode, {k: v for k, v in metrics.items()
        if k.startswith(('test.', 'review.', 'ai.answers', 'ai.speech', 'ai.downlink'))})
    print(result.get('warnings', []))
    assert run.returncode == 0 and metrics.get('test.finished') == 1
    return metrics
for case, extra in [('downlink-real-typed', ('-DREVIEW_TYPED_FIRST',)), ('downlink-real-control', ())]:
    obj = compile_obj(case, audio, extra)
    link = [part for part in base if '/tests/device_events/' not in part]
    pos = next(i for i, part in enumerate(link) if part.endswith('.a'))
    link.insert(pos, obj)
    metrics = run_probe(case, link, '55s')
    assert metrics.get('test.fail', 0) == (1 if extra else 0)

ai = (root / 'system/services/ai/ai_service.c').read_text()
assert ai.count('post(AI.session_seq, 4, 0, NULL);') == 1
ai = ai.replace('post(AI.session_seq, 4, 0, NULL);', '/* Review mutant: omit only the error notification. */')
mutant = (root / 'tests/device_events/ai_boundary.c').read_text().replace('#include INK_TEST_AI_SOURCE', ai)
obj = compile_obj('no-speech-notify', mutant, ('-iquote', str(root / 'tests/device_events')))
link = list(base)
pos = next(i for i, part in enumerate(link) if part.endswith('/ai_boundary.c.o'))
link[pos] = obj
flash = out / 'notification-mutant.img'
flash.unlink(missing_ok=True)
boots = []
for number in (1, 2):
    metrics = run_probe('notification-mutant-boot-' + str(number), list(link), '90s', flash)
    assert metrics.get('test.fail', 0) == 0
    boots.append(metrics['test.event_boot']['max'])
assert boots[1] == boots[0] + 1
```

SEVERE=0 HIGH=2
