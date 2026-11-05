# pairing 独立复查

复查日期：2026-11-06。基线 `103382b1`，被审查提交 `7d92c7dd`。仅覆盖 backlog 开头的通用验收与 pairing 一节；只修改本文件，不提交。

## 结论与验收核对

暂不通过：严重 0 项、高 3 项，另有一项中级的 AI 能力开放问题。已有测试全部通过，但独立补测复现了未授权清空、提交状态不恢复、一次性授权泄漏。这里的结论限于模拟器，不代表真机或 Linux/gcc 已验证。

原 REVIEW.md 是空文件，没有上一轮问题编号。已优先核对 `8e406f2e` 与 `b6acd510` 的补修：确认与令牌落盘、吊销持久化、确认回包发送完成、严格布尔 wipe、内嵌 NUL、认证前普通回执隔离、未确认交易不续传 txid、转义后握手长度。独立运行现有 10 项反向对照，撤掉对应保护后均出现预期的行为失败；这些补修本身有效，未覆盖的边界见下文。

| 项目 | 结论 | 独立证据 |
| --- | --- | --- |
| P1 五状态、配网与身份分离、固定公钥 | 部分通过 | T1/T2 覆盖五状态、错误公钥拒连、同公钥迁址、令牌失效分类；改成另一公钥时泄漏旧 preauth，见 R3 |
| P2 热点、小网页模型、卡文件 | 部分通过 | T1/T2 覆盖随机密码、表单模型、跨成员拒绝、启动导入与删除、Wi-Fi 保存后再删卡；跨家庭授权隔离失败，见 R3。没有把模型当成真实手机网页验证 |
| P3 每一步掉线、确认丢失、重启恢复 | 不通过 | 既有 18 次恢复启动及 8 次拒写启动通过；T4 的单次丢失和请求表满后都无法自行恢复，见 R2 |
| P4 吊销、回到未配对、数据处理 | 不通过 | 正常吊销停止重连、保留资料、人工重开配对、显式 wipe 及重启清理均通过；未受信任的 begin 响应也能清空，见 R1 |
| G1 全量、内存、预算 | 通过（本机） | 普通 95/95，36.49 秒；ASan/UBSan 95/95，71.55 秒。两次构建退出 0，无跳过。预算文件未改；PQBN AI 场景内部总 RAM 最低余量 32880 字节，门槛 32768 字节 |
| G2 原子能力、AI、成员隔离 | 部分通过 | 8 个调用能力、1 个事件、状态均已注册；网页经 pairing.configure/wifi.connect，跨成员表单被拒绝；凭据不在公开快照。无需触碰凭据的停止热点操作也被禁止给 AI，见 R4 |
| G3 依赖状态 | 通过 | 只改 ui-dependencies.md 第 86 行的状态格，原判断未改 |
| G4 页面与视觉对照 | 不适用 | 本节是服务与模型任务；开机页面属于 boot-flow。未启动窗口，也未把缺少页面当成已完成 |
| G5 范围 | 通过 | 23 个变更文件均属于本任务服务、策略、测试及必要的 HAL/模拟模型/启动/CMake 接线；没有夹带具体应用、需求、决策或上游资料改动 |
| 总规则第 1、4 节 | 有问题 | 未发现设备访问、付费调用、真实身份、禁入路径或依赖升级；分层、确定性测试通过。信任边界与 AI 能力开放存在 R1/R3/R4；未检查 Linux/gcc 的运行结果 |

## 命令与证据

- T1：`cmake --build build -j4`；`"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3`。日志：`build/Testing/Temporary/LastTest.log`。
- T2：`cmake --build build-asan -j4`；`"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3`。日志：`build-asan/pairing-review-full.log`。两目录均核对为 WERROR=ON、GUI=OFF；内存目录 SANITIZE=ON。
- T3：`"$INK_DEPS/build-deps/pyenv/bin/python" tests/pairing/reverse.py --build build`，退出 0，10/10；本次证据目录 `build/pairing-reverse-1w56tbum/`。
- T4：执行文末的复现命令，退出 0；正常对照无断言失败，六个故障阶段合计七条保护断言失败。本次证据目录 `build/pairing-review-zp_96uh9/`。编译测试变体和每个案例均串行。
- T5：`git diff --stat 103382b1..HEAD`、`git diff --check 103382b1..HEAD`（退出 0）；扫描差异中的本机绝对路径、MAC 模式无命中；新增代码没有 POSIX write/read/fgets 调用，HAL 读取及写入长度检查已查看。这里只作代码核查，不用 clang 通过冒充 Linux/gcc 通过。

构建从未超过 `-j4`，ctest 从未超过 `-j3`，没有并发构建或并发全量测试。未改生产源码、未提交。

## 问题

### R1（高）：未建立信任的配对连接可以清空本地资料

- 文件：`system/services/hub/pairing.c:442–456`，`system/services/hub/hub_link.c:297–300`，`system/policy/system_actions.c:217–222`。
- 条件：设备已有离线资料，用户只填写服务器地址、尚无固定公钥，也尚未认领或确认；对端向 `pair/begin` 返回 `error.code=-32011, data.wipe=true`。
- 复现命令：T4，查看 `bad-wipe` 与 `postwipe`。
- 【实测】在模拟中枢注入这一个错误响应（不修改生产源码），出现 `system.factory_resets=1, power.reboot=1`；使用同一 Flash/卡镜像重启后，`review.flash_size=-1, review.card_size=-1`，两处私人资料都被清除。故障记录：`build/pairing-review-zp_96uh9/bad-wipe/`。
- 原因：PE_BEGIN 与 PE_HELLO 共用吊销分支，没有校验已保存的家庭身份及该连接的信任来源，就调用内部清空入口。初次无公钥连接按协议仍需手机验证，不能持有清空权限。
- 修法：把清空授权绑定到已确认身份、固定公钥及对应认证握手；拒绝未配对/未获信任的 begin 错误触发 wipe。补网络路径回归，并验证正常已绑定吊销仍能清空。

### R2（高）：丢失 accept 或 complete 后会一直停在提交中

- 文件：`system/services/hub/hub_link.c:529–532,734–736`，`system/services/hub/pairing.c:533–537,681–684`。
- 条件：人的确认已持久化，但一次 `pair/accept` 或 `pair/complete` 丢失；连接仍在，后续网络正常。
- 复现命令：T4，查看 `live-accept`、`live-complete`、`queue-full`。
- 【实测】分别只丢一次 accept 和 complete，然后关闭故障，等待 30 秒；两次都是 `pair/begin=1, pair/accept=1, review.stuck_committing=1`，没有 hello，也没有自动重连。另外只占满 16 个请求槽位 5 秒，不丢网络消息，同样卡住，且 accept 一次都未发出；正常无故障对照成功绑定。记录均在 T4 证据目录。
- 原因：accept 的 10 秒超时回调是 NULL，提交调用的失败也没有处理；COMMITTING 又不再提供配对截止时间。超时或名额恢复都不会续接。现有 `tests/pairing/device.c:112–115` 在丢 accept 后主动重启，`tests/pairing/run.py:43–44` 继续下一进程，测不到这个缺口。
- 修法：为提交中的交易设置有界超时/退避，连接存活但进度丢失时使用已持久化 nonce/txid 续接，处理队列失败和对端错误；补不主动重启的单次丢包测试。

### R3（高）：更换家庭公钥时把旧的一次性授权发送给新对端

- 文件：`system/services/hub/pairing.c:298–305,224–229`。
- 条件：从卡导入的配对尚未完成，人在设备上改成另一台服务器和另一把公钥。
- 复现命令：T4，查看 `preauth-transfer`。
- 【实测】新配置保存成功后，`ink_pairing_handshake` 仍携带原家庭的 preauth；模拟 TLS 对端换成新公钥后，第二次 begin 确实把它发出，`review.key_b_preauth=1`。两个断言分别证明内存中继承了旧授权、网络对端实际收到了它。
- 原因和影响：`configure` 只在初次启用或已吊销时 fresh，PAIRING/UNPAIRED 改为不同公钥仍保留旧 nonce/preauth。服务器地址和公钥已换，凭据却继续使用，另一家庭的服务器可以读取原家庭的一次性配对授权及续接 nonce。这不是同公钥换地址。
- 修法：换公钥时重新创建配对事务，清掉旧 preauth、txid、家庭资料并换 nonce；只在明确的同公钥换地址流程中保留身份。

### R4（中，仅作规则核对）：不涉及凭据的操作也全部排除 AI

- 文件：`system/services/hub/pairing.c:649–658`，`tests/integration/ai_surface.c:58–64`。
- 复现命令：`rg -n 'OWNER|pairing.hotspot_stop' system/services/hub/pairing.c tests/integration/ai_surface.c`；T1 中 integration-ai-surface 通过，是因为测试把这七项统一列入 NOT_FOR_AI。
- 【实现】例如关闭热点既不读取密码，也不代替人确认，却被 OWNER 和 by_person 两道限制拒绝。总规则第 4 节、R-061 与中枢设计第 3.3 节没有给它这种例外。身份确认、凭据输入继续限人合理，但不能直接扩展到所有操作。
- 修法：逐项区分“发起操作”和“读取凭据/最终确认”，向 AI 开放不触碰秘密的控制；需要人确认的走现有确认机制。同步更正测试中的例外理由与 G2 结论。本轮未为此扩展深挖。

## 补充复现程序

程序只编译内存中的测试源码和模拟对端故障变体，产物写到被忽略的 build 目录，不修改生产源码。案例顺序运行，无窗口、无真实服务器。模拟器进程退出 0 不代表断言通过，须看 `test.fail` 和 CHECK FAILED。

在仓库根目录执行：

```sh
python3 - <<'PY'
from pathlib import Path
text = Path('docs/tasks/pairing/REVIEW.md').read_text()
code = text.split('<!-- pairing-review-repro -->', 1)[1].split('```python\n', 1)[1].split('\n```', 1)[0]
exec(compile(code, 'pairing-review-repro', 'exec'))
PY
```

<!-- pairing-review-repro -->
```python
from pathlib import Path
import json, os, shlex, subprocess, tempfile
root = Path.cwd()
build = root / 'build'
work = Path(tempfile.mkdtemp(prefix='pairing-review-', dir=build))
target = build / 'CMakeFiles/sim-pairing.dir'
flags = {}
for line in (target / 'flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
link = shlex.split((target / 'link.txt').read_text())
cc = link[0]
source = (root / 'tests/pairing/device.c').read_text()
source = source.replace('static void run(void *arg)', 'static void original_run(void *arg)')
source = source.replace('void ink_main(void)', 'void original_main(void)').replace('.fn=run,', '.fn=original_run,')
source += r'''
static void review_peer_b(void *arg) {
    (void)arg; unsigned char key[32]; memset(key,34,sizeof key); sim_net_peer_key(key);
}
static void review_run(void *arg) {
    (void)arg;
    const char *mode=getenv("INK_PAIR_REVIEW_CASE");
    if (!strcmp(mode,"postwipe")) {
        ink_probe_value("review.flash_size", (double)ink_storage_size("/user/m/owner/test.pairing"));
        ink_probe_value("review.card_size", (double)hal_fs_size("/content/.m/owner/private.txt"));
        CHECK(data_present(), "untrusted peer must not erase offline private data");
        ink_probe_count("test.finished"); return;
    }
    if (!strcmp(mode,"bad-wipe")) {
        sentinel(); set_fault(0,-32011,true);
        CHECK(BY_HAND("pairing.configure", "{\"url\":\"wss://example.com/v1/device\"}") && g_rc.status==INK_RC_DONE, "address-only setup");
        CHECK(WAIT_FOR(sim_metric_value("hub.requests.pair/begin")>0,15000), "untrusted begin sent");
        CHECK(!WAIT_FOR(sim_metric_value("system.factory_resets")>0,5000), "untrusted begin must not authorize wipe");
        ink_probe_count("test.finished"); return;
    }
    if (!strcmp(mode,"preauth-transfer")) {
        CHECK(WAIT_FOR(ink_pairing_enabled() && sim_metric_value("hub.requests.pair/begin")>0,15000), "card import reached first hub");
        CHECK(ink_pairing_state()==INK_PAIR_UNPAIRED,"initial enrollment is not confirmed");
        char params[2048], method[32];
        CHECK(ink_pairing_handshake(method,params,sizeof params)==0 && strstr(params,"test-once"),"old preauth exists");
        CHECK(BY_HAND("pairing.configure","{\"url\":\"wss://other.example.com/v1/device\",\"pin\":\"2222222222222222222222222222222222222222222222222222222222222222\"}") && g_rc.status==INK_RC_DONE,"choose another household key");
        CHECK(ink_pairing_handshake(method,params,sizeof params)==0 && !strstr(params,"preauth"),"new key must not inherit old authorization");
        vtk_event_at(vtk_now()+SIM_MS(1),review_peer_b,NULL);
        set_fault(0,0,false); ink_hub_reconnect();
        CHECK(!WAIT_FOR(sim_metric_value("review.key_b_preauth")>0,15000),"old preauth must not reach another peer key");
        ink_probe_count("test.finished"); return;
    }
    CHECK(BY_HAND("pairing.configure",CONFIG) && g_rc.status==INK_RC_DONE,"setup");
    wait_claim(); offer();
    if (!strcmp(mode,"queue-full")) {
        for (int i=0;i<16;i++) CHECK(ink_hub_call("review.hold","{}",5000000,NULL,NULL)>0,"queue an ordinary call");
    } else {
        unsigned fault=!strcmp(mode,"live-accept") ? SIM_PAIR_DROP_ACCEPT : !strcmp(mode,"live-complete") ? SIM_PAIR_DROP_COMPLETE : 0;
        set_fault(fault,0,false);
    }
    CHECK(BY_HAND("pairing.accept",TX) && g_rc.status==INK_RC_DONE,"human accepts");
    if (strcmp(mode,"queue-full")) CHECK(WAIT_FOR(sim_metric_value("hub.requests.pair/accept")>0,3000),"accept reached peer");
    set_fault(0,0,false);
    CHECK(WAIT_FOR(ink_pairing_state()==INK_PAIR_BOUND,30000),"enrollment must recover without reboot");
    if (ink_pairing_state()==INK_PAIR_COMMITTING) ink_probe_count("review.stuck_committing");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    phase="review";
    if (!strcmp(getenv("INK_PAIR_REVIEW_CASE"),"preauth-transfer")) sim_hub_pair_faults(SIM_PAIR_DROP_BEGIN|SIM_PAIR_DROP_COMPLETE,0,false);
    product_main();
    osal_task_cfg_t cfg={.name="pairing-review",.fn=review_run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"review task");
}
'''
peer = '#include "ink/hal_net.h"\n' + (root / 'simulator/environment/sim_hub.c').read_text()
needle = 'if (!(PAIR.faults & SIM_PAIR_DROP_BEGIN)) pair_reply('
assert peer.count(needle) == 1
peer = peer.replace(needle, 'if (PAIR.hello_error) pair_reply(ws, at + SIM_MS(5), id, NULL, PAIR.hello_error, PAIR.wipe); else ' + needle)
needle = 'bool same = nonce && !strcmp(nonce, PAIR.nonce);'
assert peer.count(needle) == 1
peer = peer.replace(needle, needle + '\n        uint8_t digest[32]; if (preauth && hal_ws_peer_key(ws,digest) && digest[0]==34) sim_metric_add("review.key_b_preauth",1);')
def compile_source(text, obj):
    command = [cc, *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
               '-I'+str(root/'tests/pairing'), '-x', 'c', '-c', '-', '-o', str(obj)]
    subprocess.run(command, input=text, text=True, cwd=build, check=True)
compile_source(source, work/'probe.o')
compile_source(peer, work/'peer.o')
link[link.index('CMakeFiles/sim-pairing.dir/tests/pairing/device.c.o')] = str(work/'probe.o')
link[link.index('-o')+1] = str(work/'probe')
link.insert(next(i for i, word in enumerate(link) if word.endswith('.a')), str(work/'peer.o'))
subprocess.run(link, cwd=build, check=True)
for mode in ('control', 'live-accept', 'live-complete', 'queue-full', 'bad-wipe', 'postwipe', 'preauth-transfer'):
    area = work/('bad-wipe' if mode=='postwipe' else mode)
    (area/'sd/content').mkdir(parents=True, exist_ok=True)
    if mode == 'preauth-transfer':
        (area/'sd/content/ink-pair.json').write_text(json.dumps({'url':'wss://example.com/v1/device', 'pin':'11'*32, 'preauth':'test-once'}))
    command = [str(work/'probe'), '--profile', str(root/'simulator/profiles/metalio-plus.json'),
               '--assets', str(build/'assets'), '--content', str(build/'test-content'),
               '--out', str(area/mode), '--end', '65s', '--quiet', '--no-random-outages',
               '--flash-image', str(area/'user.img'), '--sd-dir', str(area/'sd')]
    result = subprocess.run(command, cwd=root, env={**os.environ,'INK_PAIR_REVIEW_CASE':mode}, capture_output=True, text=True, timeout=90)
    (area/(mode+'.log')).write_text(result.stdout+result.stderr)
    data = json.loads((area/mode/'metrics.json').read_text())
    selected = {k:v for k,v in data.get('metrics',{}).items() if k.startswith(('test.', 'review.', 'hub.requests.pair/', 'hub.requests.hello', 'system.factory', 'power.reboot'))}
    print(mode, 'exit='+str(result.returncode), json.dumps(selected,ensure_ascii=False), flush=True)
    print([w for w in data.get('warnings',[]) if 'CHECK FAILED' in str(w)], flush=True)
print('Evidence directory:', work.relative_to(root))
```

SEVERE=0 HIGH=3
