# 系统服务收尾复查（第 6 轮）

**结论：不通过，严重 0、高 2。** 上一轮 R5-01/R5-02 的文件修复有效；本轮发现身份绑定仍漏了数据/时间能力和通知事件生产者。已有测试全部通过，不能据此认定家人隔离验收完成。

2026-11-05～06（新加坡时间）；基线 `fdff779`，复查对象 `70295896e50715a28a32b1e2fcf695c9921abb9a`。已读复查说明、任务书、总规则、开发报告、Git 中上一轮复查、A/B/C2 问题表及 C 路 TASK，并对照相关设计。只深挖严重、高问题。本轮仅修改本报告，不提交；开始时本报告是已有未提交改动的空文件，按本次要求覆盖。

## 验收表

| 编号 | 结论 | 本次证据 |
| --- | --- | --- |
| S1 | 不通过 | 上轮两项高及原 A/B/C2 修复的现有回归通过；同类隔离仍有 R6-01、R6-02。旧问题核查见下表 |
| S2 | 本次覆盖通过 | `policy`、`permissions`、`undo*`、`activity`：信封布尔 asked、非高风险设置、高风险真人确认、密码/权限拒绝、活动满额及撤销正反例通过；§7 已同步。真实中枢来源判定不在本次交付 |
| S3 | 通过 | `actions` 对真实能力表的两条请求路径共 6 项检查通过；`ai_service.c:463–476` 动态分配 PSRAM；两套 PQBN AI 场景内部总 RAM 最低空闲 32,880 字节，门槛仍为 32,768，余量 112 字节 |
| S4 | 本机既有检查通过 | 独立配置、构建；普通 CTest 86/86（25.12 秒），ASan/UBSan 86/86（67.27 秒）。两套专项各 38 组、168 次启动、1,911 项检查通过，另各 5 项低电量保存控制及栈计量通过；新增复查探针稳定失败，详见问题 |
| S6 | 不通过 | 文件、媒体、迟到写者及删除复用的原测试通过；真实 `data.put`、闹钟/提醒与通知事件仍可跨成员，见 R6-01/R6-02。原子能力入口有了，但原身份没有传到所有副作用边界 |
| S5 | 已核对，有文档遗留 | 完整基线隐私扫描 0 匹配；预算/场景门槛及受保护需求文件未改。范围例外有相应修复理由。完整基线空白检查仍有一条继承的审计文档 EOF 空行，不计高问题 |

## 旧问题核查

下表的“通过”指本次代码核查及两套对应回归；没有把历史报告中的反向结果冒称本次执行。上一轮三组反向对照另见执行记录。

| 旧编号 | 本次结论与证据 |
| --- | --- |
| R5-01 | 原文件事件路径归属已修。`file_events` 覆盖真实句柄、录音收尾、短写/建目录/删除/移动/作业，以及录音与两种播放启动；原成员/代次保留。通知生产者的相邻遗漏另列 R6-02 |
| R5-02 | 原源/目标路径混用已修。`file_paths` 覆盖四类路径、stat/解析/作业交接、目录复制及儿童权限；`file_reuse` 覆盖旧目录、任务与释放槽后的迟到文件事件。非文件能力的相邻遗漏另列 R6-01 |
| R4-01、R3-01 | `reader_late`、`late_writers`、`member_writes`、`file_late`、`member_inventory` 通过；保存及清理入口保留代次/写者机制 |
| R3-02 | `wifi_undo`、`undo_schedules`、`undo_owner` 通过；原 Wi-Fi 保存失败和撤销归属问题未复发 |
| R2-01、R2-04、R1-05 | `removal`、`removal_alarm`、`empty_removal`、`member_inventory` 通过；删除清理失败保留编号，成功后原集合、目录、闹钟等不残留。已清理后的生产者载荷不由这些测试覆盖，见 R6-02 |
| R2-02 | `replay` 通过；重复号绑定成员、代次、来源、能力及参数，迟到回执隔离保留 |
| R2-03、R1-06 | `undo_data`、`undo` 通过；旧版本撤销拒绝、正常撤销控制保留 |
| R1-01 | `upgrade_crash` 提交前后断电、失败及恢复分支通过 |
| R1-02、S6-01、S6-02 | `playback`、`isolation`、`handoff` 通过；缓冲播放、采集和已进入传输队列的事件隔离保留 |
| R1-03、R1-04 | `activity`、`permissions`、`policy` 通过；活动积压、权限确认与儿童上限原问题未复发 |
| AUD18-101、102、103 | `files` 通过；不可读根不报清理成功，短操作/目录持卡，任务登记先于提交 |
| AUD18-104、105、107、108 | `update`、`upgrade_crash` 通过；持久提交、资源恢复、坏包表及固定下载身份有真实实现 |
| AUD18-109 | `storage` 通过；集合遗忘取消共享计时节点，未破坏独立定时器 |
| AUD18-201、204、205 | `audio`、`voice`、`playback` 通过；抢占、耳机断连与焦点保留原断言 |
| AUD18-202、208、209 | `text` 通过；OOM 不保留旧 redo，隐藏字段不可由 AI 提交，UTF-8 边界保留 |
| AUD18-203、403 | `shutdown`、`lowbattery` 通过；录音结束再持久保存，未保存的低电量关机不宣称全已保存。开发报告当前表漏单列 403，但实际回归仍在 |
| AUD18-401 | `bindings` 通过；内容、光标、选区操作绑定会话和版本 |
| AUD18-106、206、207、402 | `update_history`、`offpanel`、`text_window`、`keyboard_snapshot` 通过；原中级修复有实现与专项检查 |
| AUD18-404 | 低级文档清单漏项仍保留；没有提升等级或据此凑问题 |

## 已确认问题

### R6-01（高）：数据、闹钟和提醒能力仍在执行中途换用当前成员，导致串数据和越权删除

- **位置**：`system/services/data/data_caps.c:21–31`、`system/services/data/datastore.c:763–769`；`system/services/time/time_caps.c:124–155`、`:158–189`、`:236–245`；`system/services/time/time_service.c:541–548`、`:568–578`、`:595–608`。总线在 `system/policy/cap.c:546` 校验原成员，在 `:419–421` 把迟到回执改为 stale，但不能收回已经发生的副作用。
- **【实测】复现**：运行下面的“独立并发探针”，`REVIEW_MODES='data alarm reminder cancel'`。普通及 ASan/UBSan 构建各 8 个分支均正常退出且 `test.finished=1`；每套 4 个同成员对照通过，4 个换人分支各有 1 项隔离断言失败，脚本退出 1，无 sanitizer 诊断。证据 `build/services-review6-rawby8fy/`、`build-asan/services-review6-p_q9zbtz/`。
- **结果**：成人发起真实 `data.put` 后，在集合定义查询返回和打开集合之间换到 `kid`，孩子通过真实 `data.list` 读出 `SYNTH_PRIVATE`。真实 `alarm.set` / `reminder.add` 在等待服务自己的 `T.edits` 锁期间换人，也把成人私有标题保存到孩子名下。反方向，孩子发起针对成人闹钟的 `alarm.cancel`，等待同一把锁时切到成人，成人闹钟被删；不换人的孩子对照不能删。四个换人调用的回执都已是 stale，数据仍已串入或被删除。
- **【实现】原因**：这些 provider 没有传递总线接受调用时的成员/代次。`data.put` 用 `ink_db_open(d, NULL)` 重新选成员；时间服务在拿到 `T.edits` 后才填 `a->owner` 或按当前成人权限判取消。文件服务本轮引入的调用身份绑定没有覆盖这些同类入口。
- **修法**：provider 从 `ink_cap_call_member` 取原身份，成员集合用绑定代次的打开入口；闹钟/提醒把原 owner 与原授权上下文带入修改锁，在锁内重新验证其生命周期和权限。添加上述正反向真实能力链路，不以 stale 回执作为无副作用的证据。
- **边界**：时间探针仅持有/释放真实修改锁；数据探针仅固定真实查询返回后的抢占窗口。不修改产品数据路径或授权结果，不声称已测真机自然触发频率。

### R6-02（高）：通知生产者丢弃原代次，删除并复用编号后仍把成人私有标题发给新孩子

- **位置**：`system/services/notify/notify_service.c:164–174`、`:294–299`；`system/policy/cap.c:969–973` 的普通事件入口重新取当前代次。文件服务已改用 `ink_cap_emit_for`，通知仍走普通入口。
- **【实测】复现**：下面探针的 `REVIEW_MODES=events`。真实 `notify.post` 已写入成人 `former` 的收件箱、准备发送 `notify.posted` 时暂停生产者；经真人确认删除该成人，再经真人确认以同一编号添加孩子并进入新空间，随后恢复原事件。
- **结果**：真实清理成功，新孩子 `notify.list` 不含旧通知；但本地订阅者和新成员的实际模拟中枢连接都收到旧成人标题 `SYNTH_PRIVATE`。普通及 ASan/UBSan 结果一致：各自同成员控制 11 项检查通过，删除复用分支 13 项通过、2 项隔离检查失败；模拟器均退出 0、`test.finished=1`，脚本退出 1，无 sanitizer 诊断。证据 `build/services-review6-hjtfxhdx/`、`build-asan/services-review6-bxkz1e1c/`。
- **【实现】原因**：`ink_notify_post` 早先拿到的 `generation` 只用于写集合，`announce` 未接收它。旧事件在 `ink_cap_emit` 才按字符串成员编号取代次，取得新孩子的有效代次后，总线及传输层现有检查都会放行。仅在队列入队后检查代次，不能保护入队前迟到的生产者。
- **修法**：通知从开始生成、写入、宣告到投递全程携带同一个成员代次，使用带原代次的事件入口；同样检查时间服务等仍用普通入口发送成员私有载荷的生产者。增加“集合已清干净，但生产者仍持有旧载荷”的复用回归；不要只测已经进总线队列的旧事件。

## 执行记录与边界

- 【实测】普通及内存构建均按总规则配置，后者仅增加 `-DINK_SANITIZE=ON`；构建 `cmake --build <目录> -j4`，测试 `SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir <目录> --output-on-failure -j3`。同一时间只跑一轮构建或测试；既有 gui-smoke 使用 dummy，未启动带窗口的模拟器。
- 【实测】完整专项命令与开发报告 V2 相同，38 个组名逐项执行；普通证据 `build/services-finish-c8iy386u/`、内存证据 `build-asan/services-finish-b5vo4zvy/`，日志为两目录的 `services-review6-regressions.log`。每套 168 次启动中 12 次为脚本要求的提前重启，其他 156 次均 `test.finished=1`，功能失败 0。
- 【实测】上一轮反向命令：`UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --baseline 9b976aa file_events file_paths file_reuse`；内存套增加 `--build build-asan`。只从 Git 读旧源码到忽略目录，不改工作区。两套各 59 次启动均退出 0、`test.finished=1`，29 个分支触发 71 项功能断言失败（共执行 654 项检查），脚本均预期退出 1；没有把编译/崩溃当复现。证据 `build/services-finish-u2dfu0z1/`、`build-asan/services-finish-kx8vmexv/`；日志为两目录的 `services-review6-reverse.log`。
- 【实测】另跑 `INK_FINISH_SAVED=1 UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py lowbattery` 和 `tests/services_finish/stack_meter.py`；内存套各增加 `--build build-asan`，均退出 0。低电量各 5 项通过，栈计量各 0 失败；日志为两目录的 `services-review6-saved.log`、`services-review6-stack.log`。
- 【实测】本报告最终探针默认 10 个分支在两套构建重跑，均 10 次正常退出、`test.finished=1`；每套 88 项检查，5 个同成员控制通过，5 个换人/复用分支共 6 项断言失败，脚本退出 1。最终证据 `build/services-review6-3kiohe6k/`、`build-asan/services-review6-p1i70mun/`；日志为两目录的 `services-review6-independent-final.log`。
- 【实现/实测】核对完整 `fdff779..HEAD` 差异。没有抬高预算、改测试场景门槛、删掉旧测试或关闭 sanitizer；D-105 旧测试期望的变动与历史报告列项相符。新增等待用于有条件等待、持久化或固定并发窗口，没有发现用于掩盖失败的产品 sleep。公共头、阅读器保存、UI 语义快照和关机回执变动对应既有修复；栈计量两行、GUI 坐标检查的范围例外在历史报告说明了阻挡 S4 的原因。
- 【实测】完整基线隐私正则匹配 0，202 个差异路径中现存文件均小于 5 MiB；新增 C/C++ 行没有 libc `write`、`read`、`fgets` 调用。`docs/requirements.md`、`docs/ideas.md`、`docs/decisions/README.md` 无差异。`git diff --check fdff779..HEAD` 退出 2，仅 `docs/audits/2026-11-05-codex-services-a/report.md:133` 的 EOF 空行；本次报告差异检查退出 0。
- 【文档核查】服务设计 §15.4/§15.5 仍把已修的 106/206/207/402 或键盘快照列为缺项，与开发报告不同步；这是遗留文档问题，未按严重/高计数。未为迎合实现修改合同或需求。
- 【边界】没有 Linux gcc、真机、真实中枢或手工视觉验证；未联网、未读设备资料、未调用辅助代理、未提交或推送。现有自动化只覆盖列出的模拟器链路，不能证明所有时序安全。R6 两项均有本次完整可重建探针，产品代码和原测试未修改。

## 独立并发探针

下列脚本只生成忽略的构建产物。时间服务使用真实 `T.edits` 锁；数据服务只在真实定义查询返回后暂停；通知服务只在真实事件提交前暂停。不改参数、身份、I/O 结果或断言。默认执行全部 10 分支；`REVIEW_MODES='data alarm reminder cancel'` 只测 R6-01，`REVIEW_MODES=events` 只测 R6-02；设置 `REVIEW_BUILD=build-asan` 复跑内存构建。两套预期均退出 1，具体失败必须与上文一致。可从本报告提取执行：

```bash
$INK_DEPS/build-deps/pyenv/bin/python - <<'PY'
from pathlib import Path
text = Path('docs/tasks/services-finish/REVIEW.md').read_text()
code = text.split('```python review6-probe\n', 1)[1].split('\n```', 1)[0]
exec(compile(code, 'review6-probe', 'exec'))
PY
```

```python review6-probe
import json, os, shlex, subprocess, tempfile
from pathlib import Path
root = Path.cwd()
build = root / os.environ.get('REVIEW_BUILD', 'build')
work = Path(tempfile.mkdtemp(prefix='services-review6-', dir=build))
main = r'''
#include "product.h"
#include "ink/sys/notify.h"
volatile bool review_armed, review_waiting, review_release;
volatile bool review_event_armed, review_event_waiting, review_event_release;
void review_hold(void); void review_unlock(void);
static bool worker_done, accepted;
static ink_rc_t receipt;
static char cap[32], args[256];
static const ink_db_def_t NOTES = {.name="review6.notes", .scope=INK_DB_MEMBER, .schema=1, .json=true, .ai_read=true, .ai_write=true};
static int event_results, event_private;
static char event_status[24];
static void observe(void *ctx,const char *id,const ink_args_t *a) {
    (void)ctx;
    if (!strcmp(id,"confirm.result")) { ink_args_get_str(a,"status",event_status,sizeof event_status); event_results++; }
    else { char title[48]=""; ink_args_get_str(a,"title",title,sizeof title); if(strstr(title,"SYNTH_PRIVATE"))event_private++; }
}
static bool confirmed(const char *id,const char *params) {
    if(!BY_HAND(id,params)||g_rc.status!=INK_RC_PENDING_CONFIRMATION)return false;
    unsigned token=0; int before=event_results;
    return sscanf(g_rc.object,"confirm:%u",&token)==1 && ink_confirm_decide(token,true,INK_SRC_TOUCH)==0 && WAIT_FOR(event_results>before,5000) && !strcmp(event_status,"done");
}
static bool seen(int first,const char *needle) {
    for(int i=first;i<sim_hub_events_seen();i++) { const char *s=sim_hub_event_at(i); if(s&&strstr(s,needle))return true; }
    return false;
}
static void event_receipt(void *ctx,const ink_receipt_t *rc) { (void)ctx; receipt=rc->status; }
static void post_event(void *arg) {
    (void)arg;
    ink_call_opts_t o={.src=INK_SRC_AI,.args_json="{\"title\":\"SYNTH_PRIVATE\"}",.reply_exec=INK_EXEC_DIRECT,.done=event_receipt};
    accepted=ink_cap_call("notify.post",NULL,&o)==0; worker_done=true;
}
static void run_events(bool change) {
    CHECK(ink_cap_subscribe("confirm.result",observe,NULL,INK_EXEC_DIRECT)==0 && ink_cap_subscribe("notify.posted",observe,NULL,INK_EXEC_DIRECT)==0,"event observers");
    CHECK(confirmed("member.add","{\"id\":\"former\",\"role\":\"adult\"}"),"add original adult");
    CHECK(ink_member_switch("former")==0,"original adult enters");
    delay_ms(300);
    review_event_armed=true;
    osal_task_cfg_t cfg={.name="review6-notify",.fn=post_event,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK && WAIT_FOR(review_event_waiting,2000),"real notification is written and event is ready");
    if(change) {
        CHECK(ink_member_switch("owner")==0,"parent enters"); delay_ms(300);
        CHECK(confirmed("member.remove","{\"id\":\"former\"}"),"real adult cleanup completes");
        CHECK(confirmed("member.add","{\"id\":\"former\",\"role\":\"child\"}"),"reuse as child");
        CHECK(ink_member_switch("former")==0 && ink_member_current()->role==INK_ROLE_CHILD,"new child enters");
    }
    ink_hub_demand(); CHECK(WAIT_FOR(ink_hub_online(),10000),"current hub online");
    int first=sim_hub_events_seen(); review_event_release=true;
    CHECK(WAIT_FOR(worker_done,3000) && accepted,"original provider returns");
    CHECK(ink_fs_write_atomic("/me/SYNTH_CONTROL.txt","X",1)==0 && WAIT_FOR(seen(first,"SYNTH_CONTROL"),3000),"current event delivered");
    CHECK(change?event_private==0:event_private==1,"local event must not inherit reused member generation: %d",event_private);
    CHECK(change?!seen(first,"SYNTH_PRIVATE"):seen(first,"SYNTH_PRIVATE"),"old title must not reach reused child's real AI connection");
    CHECK(AI("notify.list",NULL) && g_rc.status==INK_RC_DONE && (change?!strstr(g_result,"SYNTH_PRIVATE"):strstr(g_result,"SYNTH_PRIVATE")!=NULL),"stored inbox lifecycle control");
    ink_probe_count("test.finished");
}
static void worker(void *arg) {
    (void)arg; accepted = AI(cap, args); receipt = g_rc.status; worker_done = true;
}
static void run(void *arg) {
    (void)arg; delay_ms(1800);
    const char *mode = getenv("REVIEW_MODE");
    bool data = !strcmp(mode, "data"), cancel = !strcmp(mode, "cancel");
    bool change = getenv("REVIEW_SWITCH") != NULL;
    if (!strcmp(mode,"events")) { run_events(change); return; }
    CHECK(ink_db_register(&NOTES)==0, "register synthetic collection");
    if (cancel) {
        CHECK(AI("alarm.set", "{\"hour\":7,\"minute\":30,\"weekdays\":127,\"title\":\"SYNTH_PRIVATE\"}") && g_rc.status==INK_RC_DONE, "seed adult alarm");
        unsigned id=0; CHECK(sscanf(g_rc.object,"alarm:%u",&id)==1, "seed id");
        snprintf(cap,sizeof cap,"alarm.cancel"); snprintf(args,sizeof args,"{\"id\":%u}",id);
        CHECK(ink_member_switch("kid")==0, "child is original caller");
    } else if (data) {
        snprintf(cap,sizeof cap,"data.put");
        snprintf(args,sizeof args,"{\"collection\":\"review6.notes\",\"id\":17,\"value\":{\"text\":\"SYNTH_PRIVATE\"}}");
    } else if (!strcmp(mode,"alarm")) {
        snprintf(cap,sizeof cap,"alarm.set");
        snprintf(args,sizeof args,"{\"hour\":7,\"minute\":30,\"weekdays\":127,\"title\":\"SYNTH_PRIVATE\"}");
    } else {
        snprintf(cap,sizeof cap,"reminder.add");
        snprintf(args,sizeof args,"{\"at\":\"2099-12-01 12:00\",\"title\":\"SYNTH_PRIVATE\"}");
    }
    if (!data) review_hold();
    review_armed=true;
    osal_task_cfg_t cfg={.name="review6-call",.fn=worker,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK && WAIT_FOR(review_waiting,1000), "real provider reached boundary after policy gate");
    if (change) CHECK(ink_member_switch(cancel?"owner":"kid")==0,"member changes while provider waits");
    review_release=true;
    if (!data) review_unlock();
    CHECK(WAIT_FOR(worker_done,5000) && accepted,"original call finishes");
    if (change) CHECK(receipt==INK_RC_STALE,"old receipt is stale");
    if (cancel) {
        CHECK(ink_member_switch("owner")==0,"inspect original adult alarm");
        CHECK(AI("alarm.list",NULL) && g_rc.status==INK_RC_DONE && strstr(g_result,"SYNTH_PRIVATE"),"child request must not delete adult alarm: %s",g_result);
    } else {
        CHECK(AI(data?"data.list":!strcmp(mode,"alarm")?"alarm.list":"reminder.list",data?"{\"collection\":\"review6.notes\"}":NULL) && g_rc.status==INK_RC_DONE,"read actual member data through bus");
        CHECK(change?!strstr(g_result,"SYNTH_PRIVATE"):strstr(g_result,"SYNTH_PRIVATE")!=NULL,"accepted adult content must stay out of child collection: %s",g_result);
    }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="review6-main",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"review task");
}
'''
time_boundary = r'''
#include <string.h>
#include "ink/osal.h"
extern volatile bool review_armed, review_waiting;
static osal_err_t review_lock(osal_mutex_t *m,int64_t timeout,const char *caller) {
    if(review_armed && (!strcmp(caller,"ink_alarm_add_versioned") || !strcmp(caller,"ink_alarm_cancel_if_revision"))) {
        review_armed=false; review_waiting=true;
    }
    return osal_mutex_lock(m,timeout);
}
#define osal_mutex_lock(m,t) review_lock(m,t,__func__)
#include "system/services/time/time_service.c"
#undef osal_mutex_lock
void review_hold(void) { osal_mutex_lock(T.edits,OSAL_WAIT_FOREVER); }
void review_unlock(void) { osal_mutex_unlock(T.edits); }
'''
data_boundary = r'''
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/data.h"
extern volatile bool review_armed, review_waiting, review_release;
static const ink_db_def_t *review_find(const char *name) {
    const ink_db_def_t *d=ink_db_find_def(name);
    if(review_armed && !strcmp(name,"review6.notes")) {
        review_armed=false; review_waiting=true;
        while(!review_release) osal_task_delay_us(10000);
    }
    return d;
}
#define ink_db_find_def review_find
#include "system/services/data/data_caps.c"
'''
notify_boundary = r'''
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/cap.h"
extern volatile bool review_event_armed, review_event_waiting, review_event_release;
static void review_emit(const char *id,const ink_args_t *a) {
    char title[48]=""; ink_args_get_str(a,"title",title,sizeof title);
    if(review_event_armed && !strcmp(id,"notify.posted") && strstr(title,"SYNTH_PRIVATE")) {
        review_event_armed=false; review_event_waiting=true;
        while(!review_event_release) osal_task_delay_us(10000);
    }
    ink_cap_emit(id,a);
}
#define ink_cap_emit review_emit
#include "system/services/notify/notify_service.c"
'''
flags={}
for line in (build/'CMakeFiles/sim-media.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key,value=line.split(' = ',1); flags[key]=shlex.split(value)
link=shlex.split((build/'CMakeFiles/sim-media.dir/link.txt').read_text())
objects=[]
for i,source in enumerate((main,time_boundary,data_boundary,notify_boundary)):
    obj=work/f'probe-{i}.o'
    command=[link[0],*flags['C_DEFINES'],*flags['C_INCLUDES'],*flags['C_FLAGS'],f'-I{root}',f'-I{root}/tests/services_finish','-x','c','-c','-','-o',str(obj)]
    subprocess.run(command,input=source,text=True,cwd=build,check=True); objects.append(str(obj))
for i,word in enumerate(link):
    if '/tests/integration/' in word and word.endswith('.o'): link[i]=objects[0]; break
binary=work/'probe'; link[link.index('-o')+1]=str(binary)
first_lib=next(i for i,word in enumerate(link) if word.endswith('.a'))
link[first_lib:first_lib]=objects[1:]
subprocess.run(link,cwd=build,check=True)
failed=0
for mode in os.environ.get('REVIEW_MODES','data alarm reminder cancel events').split():
    for change in (False,True):
        label=mode+('-switch' if change else '-control'); out=work/label
        env={**os.environ,'REVIEW_MODE':mode,'UBSAN_OPTIONS':'halt_on_error=1'}
        if change: env['REVIEW_SWITCH']='1'
        cmd=[str(binary),'--profile',str(root/'simulator/profiles/metalio-plus.json'),'--assets',str(build/'assets'),'--content',str(build/'test-content'),'--out',str(out),'--end','20s','--quiet','--no-random-outages']
        r=subprocess.run(cmd,cwd=root,env=env,text=True,capture_output=True,timeout=60)
        (work/(label+'.stderr.log')).write_text(r.stderr)
        metric=json.loads((out/'metrics.json').read_text()); m=metric['metrics']
        ok=r.returncode==0 and m.get('test.finished')==1 and m.get('test.fail',0)==0
        print(label,'PASS' if ok else 'FAIL','exit=',r.returncode,'finished=',m.get('test.finished'),'checks=',m.get('test.pass'),'failures=',m.get('test.fail',0),flush=True)
        for warning in metric.get('warnings',[]):
            if 'CHECK FAILED' in str(warning): print(warning)
        failed+=not ok
print('Evidence directory:',work.relative_to(root))
raise SystemExit(1 if failed else 0)
```

SEVERE=0 HIGH=2
