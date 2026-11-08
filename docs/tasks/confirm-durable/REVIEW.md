# confirm-durable 独立复查（第 1 轮，唯一一轮）

复查日期：2026-11-09。基线：`5e73902ecf637a74697b4f3f2e52dab6eaf2b35d`；受查提交：`4b6bba96c7c8252edb40e18a99d0133150d9fe90`。

**不通过，不能合并。严重 0 项，高 2 项，中 3 项。** 开发报告明确承认未通过完整验收，没有把全量失败伪报为通过。本轮独立复跑后确认两项正常确认流程会被丢弃，并发现结果表用满后持续拒绝新动作。仅修改本报告；未修改产品或既有测试源文件，未提交。临时编译、变异和模拟器证据在忽略的构建目录内。

## 验收逐条核对

| 编号 | 复查结论 | 独立证据 |
| --- | --- | --- |
| 1 | 指定断电切点通过，不能据此接受完整实现 | 普通与 ASan/UBSan 全量均包含 `confirm-durable`：确认和自动动作各四个切点及重启，共 17 阶段、343 检查、0 失败；执行中没有最终回执返回 `unknown` 符合设计。移除执行前记录的变异退出 1。结果容量问题另见 R2。 |
| 2 | 部分通过 | 原专项覆盖 pending、accepted、rejected、expired、done 等状态；独立补查发现成功撤销被报 failed、中枢确认到期后仍报 accepted，见 R3、R4。 |
| 3 | 正向通过，执行路径成员反例未完成 | 原专项的切成员及内容版本变化拒绝均通过；版本变异退出 1。成员变异只验证查询隔离，见 R5。 |
| 4 | 不通过 | `confirm.status` 在代码注册，`confirm/request` 是独立协议方法、不进模型工具表，符合设计。两套构建退出 0，但全量均 197/207 通过、10 失败。产品代码新增 287、删除 45；新增口径未超 300，增删合计 332，报告已披露，不以未约定的计数口径另列缺陷。 |

运行命令（使用用户提供的 `INK_DEPS`；缓存中 Werror、字体、Python、LVGL 路径和 sanitizer 开关已核对）：

```sh
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

【实测】普通全量耗时 97.81 秒，内存版 187.56 秒，CTest 均退出 8。日志：`build/review-build.log`、`build/review-ctest.log`、`build-asan/review-build.log`、`build-asan/review-ctest.log`。正向专项证据分别在 `build/confirm-durable-_uqqr0sz/`、`build-asan/confirm-durable-oq1j6xxm/`。未发现 ASan/UBSan 错误；功能失败场景不能算内存验收通过。

全量失败项：`ui-privacy-regressions-metalio-plus`、`ui-privacy-regressions-desktop-lcd`、`capability-table-doc`、`device-events`、`integration-people`、`integration-member-wake-stream`、`integration-dictation`、`integration-races`、`integration-confirm`、`integration-notify`。

【实测】从基线用 `git show <基线>:system/policy/{cap,confirm}.c` 取源码，经当前构建 flags 编译，从 stdin 输入编译器，在相同测试对象和其余库之前重链。独立结果存于 `build/review-baseline/results.json`：

| 既有测试 | 当前 | 基线替换 cap/confirm |
| --- | --- | --- |
| integration-dictation | 211 通过、7 失败 | 218 通过、0 失败，退出 0 |
| integration-notify | 178 通过、1 失败 | 179 通过、0 失败，退出 0 |
| integration-people | 111 通过、1 失败 | 112 通过、0 失败，退出 0 |
| integration-confirm | 113 通过、1 失败 | 113 通过、1 失败，退出 1 |

因此前三项是本次改动引入的回归。`integration-people` 尚未定位到具体根因，不能把“错误 PIN 后查询失败”直接解释为 PIN 绕过。`integration-confirm` 是已验证的基线失败；另外五项功能失败未独立重建基线，不冒称已排除本次影响。`capability-table-doc` 是新增能力与禁止本任务修改的生成文档不一致，合并时仍需更新。

## 问题

### R1｜高｜执行前写盘超时后直接丢弃已同意的动作

- **位置**：`system/policy/confirm.c:419–434`，尤其 425–426；请求在 482–487 已从待确认表取走。
- **复现命令**：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^integration-(dictation|notify)$' --output-on-failure -j2`。
- **结果**：【实测】原有听写流程中，人同意后未进入 listening，7 个断言失败；清空通知也未执行，1 个断言失败。基线均通过。另将失败分支只加一个 probe 计数、从 stdin 编译重链，分别得到 `review.commit_failed.text.voice_start=1`、`review.commit_failed.notify.clear=1`；测试表现不变，证据 `build/review-commit-diagnostic/results.json`。
- **原因与影响**：【实现】落盘只等 5 秒，失败后关闭卡片并释放 `h`，既不执行，也不写 failed、不发结果。普通版存储等待最大值分别约 10.0 秒、9.2 秒。这是既有正常流程触发，不需要人为卡线程。已取走的请求无法再次确认；running 记录以后即使写成了，查询也只能是 unknown。
- **最小修法**：在确认服务内调整卡片收尾与写盘等待的顺序，避免交互刷新挡住该提交；等待失败必须走现有失败回执，不能只释放请求。保持“提交后才执行”，用上述原断言验证，不逐个服务补丁或扩大测试等待。

### R2｜高｜结果没有回收路径，正常使用会把所有新确认和自动写操作堵死

- **位置**：`system/policy/confirm.c:59`、`638–667`；现有 ACK 入口 `system/policy/cap.c:1671–1680` 没有处理结果表。
- **复现命令**：运行附录的复现器，查看 `capacity` 和 `restart-full` 场景。
- **结果**：【实测】通过真实 tools/call 入口，以不同 request_id 顺序执行普通 `clipboard.write`，每次等持久 done，并向现有 ACK 入口确认已发送事件。946 次成功，第 947 次起 tools 回 failed、结果查询 unknown，随后另一个新请求也失败。复用闪存镜像重启后，947 条记录（含编号元数据）仍在，新 request_id 同样失败。证据：`build/review-final-probes/capacity/out/metrics.json`、`build/review-restart-probe/restart-full/out/metrics.json`。
- **原因与影响**：【实现】集合采用默认 256 KiB 上限，不删除任何记录；自动结果仅写 journal，没有与结果 ACK/回收关联。不是“只有中枢一直不确认接收”才会满，当前根本没有回收路径。达到上限时 `ink_db_put` 拒绝，`ink_confirm_hold` 和 `ink_confirm_track` 共用此表，新确认与自动写操作均受影响，重启无效。次数随回执长度变化，947 不是固定产品上限。
- **最小修法**：接通设计已规定的结果 ACK 回收，自动动作也走同一路径；仅回收已确认接收且不属于最近至少 16 条的记录。不要直接覆盖未确认接收的结果，也不要只调大容量。开发报告已披露没做回收，但这会造成正常使用中的持续故障，不能作为可合并边界。

### R3｜中｜成功撤销被持久化成 failed

- **位置**：`system/policy/confirm.c:235–236`、`666–667`；真实成功撤销回执在 `system/policy/cap.c:1234`。
- **复现命令**：附录复现器的 `undo` 场景。
- **结果**：【实测】先用真实 `notify.post` 产生可撤销通知，再经 tools/call 调 `system.undo`。通知确实消失，持久查询却是 `"state":"failed","receipt":{"status":"undone",…}`。8 个前置/效果检查通过，终态检查失败；证据 `build/review-final-probes/undo/out/metrics.json`。
- **原因与影响**：【实现】两处都把任何非 INK_RC_DONE 状态当作失败，丢掉了既有回执枚举语义。代码也会把 INK_RC_QUEUED 当失败；本轮实测的是成功撤销，不冒称已复测全部排队能力。
- **最小修法**：把回执到持久状态的映射集中到共用写入处；成功撤销归已执行，排队不能直接归失败，保留原回执。无需新状态机。

### R4｜中｜中枢确认的 accepted 状态不检查期限

- **位置**：`system/policy/confirm.c:69–78`、`690–695`。
- **复现命令**：附录复现器的 `expiry` 场景。
- **结果**：【实测】提交 expires_ms=10000 的 confirm/request，在期限内人工同意并查到 accepted；同成员、同连接再等 11 秒，查询仍返回 accepted。证据 `build/review-final-probes/expiry/out/metrics.json`。
- **原因与影响**：【实现】期限只在 held_t 中，accepted 后释放；持久 binding 未保留期限，状态查询只查启动区间、成员代次和连接。旧确认的查询结果失真。中枢按合同还需在执行前检查自己的期限，本轮没有证明过期动作实际执行，故定中。
- **最小修法**：在现有 binding 中保留期限，同一启动内查询 accepted 时比较期限；重启继续按现有 lease 检查失效，不增加定时任务。

### R5｜中｜成员反例实际只验证查询隔离

- **位置**：`tests/confirm_durable/run.py:25–30`、`tests/confirm_durable/regression.c:105–111`。
- **复现命令**：`taskpolicy -c utility python3 tests/confirm_durable/run.py --build build --mutant member`。
- **结果**：【实测】退出 1，唯一核心失败是第 105 行调用 state 时，孩子读到了成人结果。变异只将 read_result 的 binding.member 判断替换成 true，没有撤掉接受/执行路径的成员、epoch 关口。
- **影响**：能证明结果查询隔离有断言，不能证明“去掉执行前成员检查，旧确认会执行且测试失败”。开发报告对此已如实标“部分通过”；本项是验收证据缺口，不定高。
- **最小修法**：在现有专项内补一次真实执行路径的反向对照，验证旧成员动作的副作用计数；复用现有夹具，不另建测试框架。

版本和提交反例也已独立运行，均退出 1：`--mutant version` 命中旧内容实际执行；`--mutant commit` 命中执行前没有 running 记录。日志分别为 `build/review-mutant-member.log`、`build/review-mutant-version.log`、`build/review-mutant-commit.log`。

## 品味、规模与规则核对

- 单一结果集合、跨重启编号、共用执行器都是本任务需要的；未发现额外常驻任务、线程、独立文件格式、配置项或通用框架。报告对超过 100 行的解释基本成立。
- 可删/缩减的是 R3 两处重复而错误的终态二分判断；在共同写入处处理即可。R1 在确认服务修，R2 接现有 ACK，不应逐业务服务增补机制。
- 差异没有涉及并行任务的文件/传输服务、system/ui、文本服务，也未改共享 requirements、ideas、decisions、backlog 或生成能力表。设计文档仅增列任务已要求的 confirm.status，未改合同迁就实现。
- 既有测试只改了 ai_surface 的能力目录期望；未发现放宽旧断言、预算或超时。专项中的等待包含启动、真实期限及异步完成观察；没有据此把既有流程回归改成通过。
- `git diff --check` 通过；相对基线的提交差异按总规则扫描本机路径/MAC，无命中。系统代码未引用模拟器头文件，确认仍要求人来源，新增查询经能力表开放。
- 本轮未接触设备、真实账号或外部服务，未运行 Linux gcc。任务是策略与持久化，未修改界面布局文件，不适用界面任务的 10 屏设计对照要求。

## 附录：额外探针的可复现源码

以下只复用现有测试夹具，把待审产品源码原样编译进边界对象；新增 wrapper 仅读取 token/记录数、投递既有 ACK。不会改源码文件，临时产物写到 build/review-repro-*。C 片段替换现有 regression.c 的 run 函数，其他初始化、网关和断言 helper 原样复用。undo/expiry/capacity 失败数应分别为 1/1/3，复用 capacity 镜像后的 restart-full 为 1；模拟器本身未挂 budget 时进程可退出 0，应以 test.finished 和 test.fail 判定。复现器据这些指标最终返回非零。

【实测】已从本报告提取并执行下列原文命令，退出 1；四个场景均 test.finished=1，失败数依次为 1、1、3、1。日志：`build/review-repro.log`。

执行：

```sh
taskpolicy -c utility python3 - <<'PY'
from pathlib import Path
text = Path("docs/tasks/confirm-durable/REVIEW.md").read_text()
c = text.split("<!-- REPRO_C -->\n```c\n", 1)[1].split("\n```", 1)[0]
py = text.split("<!-- REPRO_PY -->\n```python\n", 1)[1].split("\n```", 1)[0]
exec(compile(py, "<review-repro>", "exec"), {"PROBE_C": c})
PY
```

<!-- REPRO_C -->
```c
static void send_tool(const char *name, const char *args, const char *rid) {
    char json[1024];
    snprintf(json, sizeof json, "{\"name\":\"%s\",\"arguments\":%s,\"_meta\":{\"member\":\"owner\",\"epoch\":%u,\"request_id\":\"%s\",\"asked\":true}}", name, args ? args : "{}", ink_hub_member_epoch(), rid);
    gateway(json, true);
}
static bool query_rid(const char *rid) {
    char args[100]; snprintf(args, sizeof args, "{\"request_id\":\"%s\"}", rid);
    return AI("confirm.status", args) && g_rc.status == INK_RC_DONE;
}
extern void review_ack(void);
extern unsigned review_count(void), review_last_token(void);
static bool query_token(unsigned token) { char args[64]; snprintf(args,sizeof args,"{\"token\":%u}",token); return AI("confirm.status",args) && g_rc.status==INK_RC_DONE; }
static void run(void *arg) {
    (void)arg; delay_ms(1800);
    CHECK(ink_db_register(&EFFECT)==0 && (effects=ink_db_open(&EFFECT,NULL)), "effect collection");
    CHECK(ink_cap_register(CAPS,3)==0, "probe capabilities");
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online() && ink_hub_member_matches("owner",ink_hub_member_epoch(),ink_hub_connection()),30000), "gateway connected");
    const char *mode=getenv("INK_REVIEW_CASE");
    if (!strcmp(mode,"restart-full")) {
        unsigned next=review_last_token()+1;
        ink_probe_value("review.results_after_restart",review_count());
        send_tool("clipboard.write","{\"text\":\"SYNTHETIC-RESTART\"}","SYNTHETIC-RESTART");
        CHECK(WAIT_FOR(query_token(next) && strstr(g_result,"\"state\":\"done\""),10000),"new automatic action works after restart, got %s",g_result);
    } else if (!strcmp(mode,"undo")) {
        CHECK(AI("notify.post","{\"title\":\"SYNTH_UNDO\"}") && g_rc.status==INK_RC_DONE, "real undoable notification");
        char args[120]; snprintf(args,sizeof args,"{\"object\":\"%s\"}",g_rc.object);
        send_tool("system.undo",args,"SYNTHETIC-UNDO");
        CHECK(WAIT_FOR(query_rid("SYNTHETIC-UNDO") && strstr(g_result,"\"receipt\""),10000), "undo receipt durable");
        CHECK(strstr(g_result,"\"state\":\"done\"") && strstr(g_result,"\"status\":\"undone\""), "successful undo reported done, got %s",g_result);
        CHECK(AI("notify.list",NULL) && !strstr(g_result,"SYNTH_UNDO"), "actual notification removed");
    } else if (!strcmp(mode,"expiry")) {
        char json[600];
        snprintf(json,sizeof json,"{\"title\":\"合成中枢确认\",\"body\":\"example.com 合成表单\",\"member\":\"owner\",\"parent\":false,\"expires_ms\":10000,\"digest\":\"%064d\",\"_meta\":{\"member\":\"owner\",\"epoch\":%u}}",0,ink_hub_member_epoch());
        gateway(json,false);
        ink_confirm_view_t card;
        CHECK(ink_confirm_list(&card,1)==1,"short-lived hub card exists");
        CHECK(ink_confirm_decide(card.token,true,INK_SRC_TOUCH)==0,"person accepts");
        durable(); state(card.token,"accepted");
        delay_ms(11000); durable(); state(card.token,"expired");
    } else if (!strcmp(mode,"capacity")) {
        int succeeded=0;
        for (int i=0;i<1200;i++) {
            char rid[40]; snprintf(rid,sizeof rid,"SYNTHETIC-CAPACITY-%04d",i);
            unsigned before=review_last_token();
            send_tool("clipboard.write","{\"text\":\"SYNTHETIC\"}",rid);
            bool done=WAIT_FOR(review_last_token()>before && query_token(before+1) && strstr(g_result,"\"state\":\"done\""),10000);
            if (!done) { ink_probe_value("review.first_failure",i+1); CHECK(false,"ordinary automatic write %d cannot finish, state=%s",i+1,g_result); break; }
            succeeded++;
            review_ack();
        }
        ink_probe_value("review.succeeded",succeeded);
        ink_probe_value("review.results_count",review_count());
        durable();
        CHECK(succeeded==1200,"old results do not permanently exhaust the store (%d)",succeeded);
        unsigned next=review_last_token()+1;
        send_tool("clipboard.write","{\"text\":\"SYNTHETIC-AFTER-FULL\"}","SYNTHETIC-AFTER-FULL");
        CHECK(WAIT_FOR(query_token(next) && strstr(g_result,"\"state\":\"done\""),10000),"a later ordinary action still works");
    }
    ink_probe_count("test.finished");
}
```

<!-- REPRO_PY -->
```python
import json, os, shlex, subprocess, shutil, tempfile
from pathlib import Path
root=Path.cwd(); build=root/'build'; work=Path(tempfile.mkdtemp(prefix='review-repro-',dir=build))
flags={}
for line in (build/'CMakeFiles/sim-media.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        k,v=line.split(' = ',1); flags[k]=shlex.split(v)
link=shlex.split((build/'CMakeFiles/sim-media.dir/link.txt').read_text())
source=(root/'tests/confirm_durable/regression.c').read_text()
a=source.index('static void run(void *arg)')
b=source.index('void ink_main(void)')
source=source[:a]+PROBE_C+source[b:]
cap_extra=r'''
void review_ack(void) {
    ink_json_t *j=osal_malloc(sizeof *j,OSAL_MEM_EXTERNAL);
    char text[100]; snprintf(text,sizeof text,"{\"boot\":%u,\"upto\":%u}",CB.boot_tag,ev_sent);
    if (j && ink_json_parse(j,text,strlen(text))==0) event_ack(NULL,"device/event_ack",j,0);
    osal_free(j);
}
'''
confirm_extra=r'''
unsigned review_count(void) { return (unsigned)ink_db_count(CF.db); }
unsigned review_last_token(void) { return CF.next_token; }
'''
objects=[]
for n,text in enumerate([source,'#include "confirm_boundary.c"\n'+confirm_extra,'#include "cap_boundary.c"\n'+cap_extra]):
    obj=work/f'{n}.o'
    cmd=[link[0],*flags['C_DEFINES'],*flags['C_INCLUDES'],*flags['C_FLAGS'],'-I'+str(root/'tests/confirm_durable'),'-Wno-unused-function','-x','c','-c','-','-o',str(obj)]
    subprocess.run(cmd,input=text,text=True,cwd=build,check=True)
    objects.append(str(obj))
index=next(i for i,w in enumerate(link) if '/tests/integration/' in w and w.endswith('.o')); link[index]=objects[0]
index=next(i for i,w in enumerate(link) if w.endswith('.a')); link[index:index]=objects[1:]
binary=work/'regression'; link[link.index('-o')+1]=str(binary)
subprocess.run(link,cwd=build,check=True,capture_output=True)
results=[]
for mode in ['undo','expiry','capacity','restart-full']:
    path=work/mode; path.mkdir(exist_ok=True); (path/'sd').mkdir(exist_ok=True)
    if mode=='restart-full': shutil.copyfile(work/'capacity/user.img',path/'user.img')
    env={**os.environ,'INK_REVIEW_CASE':mode}
    cmd=[str(binary),'--profile',str(root/'simulator/profiles/metalio-plus.json'),'--assets',str(build/'assets'),'--content',str(build/'test-content'),'--out',str(path/'out'),'--end','2000s' if mode=='capacity' else '50s','--quiet','--no-random-outages','--flash-image',str(path/'user.img'),'--sd-dir',str(path/'sd')]
    r=subprocess.run(cmd,cwd=root,env=env,text=True,capture_output=True,timeout=90)
    (path/'run.log').write_text(r.stdout+r.stderr)
    d=json.loads((path/'out/metrics.json').read_text())
    result={'mode':mode,'exit':r.returncode,'metrics':{k:v for k,v in d['metrics'].items() if k.startswith('review.') or k.startswith('test.')},'warnings':d.get('warnings',[])}
    results.append(result); print(json.dumps(result,ensure_ascii=False),flush=True)
(work/'results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2))

raise SystemExit(1 if any(r['metrics'].get('test.fail',0) or not r['metrics'].get('test.finished') for r in results) else 0)
```

SEVERE=0 HIGH=2
