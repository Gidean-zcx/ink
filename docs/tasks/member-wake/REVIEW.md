# member-wake 独立复查

复查日期：2026-11-06。基线：`103382b1`；复查提交：`4bc1f936`。范围：`backlog.md` 开头的 G1～G5 与 `member-wake` 一节。开始时旧 `REVIEW.md` 为空，没有上一轮发现可供回归。

**结论：不通过，严重 0、高 2。** 既有测试全部通过；补充场景复现了成员切换后的应用生命周期错误，以及免密截止后继续上传语音。两项均有源码定位、普通/ASan 复现与临时对象对照。

## 发现

### MW-R1【高】从过期成人成员门选择孩子后，没有恢复应用生命周期

- 位置：`system/ui/app_registry.c:217`、`:279–290`；后续影响位于 `:182–200`、`:147–150`。遗漏场景：`tests/integration/member_wake.c:214–225`。
- 原因【实现】：过期唤醒的 `power_job` 将 `AP.locked` 置为真。成员门选择孩子时，`member.current` 和 `member.authed` 的回调都因身份变化只投递 `member_job`；该任务重建主页，却不重新计算 `AP.locked`。页面随后能打开，应用管理器仍不启动前台应用，直到再次锁屏/唤醒等路径重新同步。
- 复现【实测】：成人设合成 PIN、打开阅读第 4 页、锁屏 121 秒、电源键唤醒，在成员门直接选择“小朋友”，再打开阅读器。主页及阅读器均为 `stopped`，探针退出 1，3 个检查失败。
- 影响验证【实测】：复用现有 `tests/integration/apps.c` 的 alpha/beta 应用夹具：孩子打开 alpha 未调用 `on_start`，仍保留成人的 `owner fixture` 内存；关闭应用返回成功，却不调用 `save_state`，读取孩子的状态文件返回 -1；beta 已声明的权限也被拒绝。普通与 ASan 构建均为 **4 个检查失败、`test.finished=1`**。
- 对照【实测】：只在临时编译对象的 `member_job` 中重算 `AP.locked`，同一夹具 **16/16 通过、退出 0**；仓库源码未修改。现有新测试先认证成人再切换孩子，没有走这条路径。
- 证据边界：真实阅读器已证实生命周期状态错误；跨成员内存残留和保存遗漏由仓库已有生命周期夹具证实，并非实机或现有阅读器正文泄漏的结论。
- 修法：切换成员的 UI 任务须在重建主页前同步当前电源与认证状态，使新成员应用正常启动、恢复和保存。增加“过期成人门 → 孩子 / 另一成人”的端到端测试，检查生命周期、状态保存及成员内存清理。
- 复现命令：运行末尾脚本，`REVIEW_CASE=lifecycle`；对照加 `REVIEW_VARIANT=control`。证据：`build/test-out/review-member-lifecycle-current-out/metrics.json`、`review-member-lifecycle-control-out/metrics.json`。

### MW-R2【高】Wi-Fi 扫描跨过免密截止时，成人语音继续上传

- 位置：`system/policy/members.c:310–319`；关联 `system/services/ai/ai_service.c:368–437`、`:830–840`，`system/services/hub/hub_link.c:709`。遗漏场景：`tests/integration/member_wake.c:157–174`。
- 原因【实现】：到期回调只向 `INK_EXEC_BG` 排队。`ink_member_space_open()` 会按时间返回假，但 `member.authed` 尚未发布关闭，因此语音采集与发送端收不到撤销；二进制语音发送又没有核对该截止时间。`wifi.scan` 本就在同一后台执行器中同步等待扫描完成（`system/services/net/wifi.c:79,202`），模拟器扫描耗时 2.5 秒。
- 复现【实测】：保持默认 120 秒，设合成成人 PIN，将已有免按键语音设置选为“始终”；锁屏 116 秒后用合成唤醒词和连续语音启动会话，在第 119 秒通过能力表发起 `wifi.scan`。扫描跨过截止，`ink_member_space_open()==false`，但 `audio.mic=sending`，随后 **0.5 秒仍上传 1176 字节**。扫描结束、后台队列处理到期任务后才停。普通与 ASan 构建均为 **2 个检查失败、`test.finished=1`**。此复现没有人为卡住执行器。
- 补充边界【实测】：沿用新测试的信号量阻塞方法，默认 120 秒到期后的 2.2 秒继续上传 5826 字节；因此不仅是一次已经在途的帧。
- 对照【实测】：临时编译对象让到期撤销直接执行、不再等待后台队列，相同 Wi-Fi 场景 **13/13 通过、退出 0**。这只用于定位原因，不表示可直接把含锁操作的全部状态通知搬进定时回调。
- 修法：授权到期的撤销不能依赖普通后台工作完成；非本地采集、语音发送及排队帧最终发送的关口须检查成员身份和授权有效性，到期立即停止/丢弃语音。补充“已在上传 → 正常后台操作跨过截止 → 不唤醒”的回归，并保留本地录音可继续的规则。
- 复现命令：运行末尾脚本，`REVIEW_CASE=uplinkscan`；对照加 `REVIEW_VARIANT=control`。证据：`build/test-out/review-member-uplinkscan-current-out/metrics.json`、`review-member-uplinkscan-control-out/metrics.json`。

## 逐条验收

| 项目 | 复查结论 | 独立证据 |
| --- | --- | --- |
| 阅读闲置不退出；自动锁屏后起算 | 通过【实测】 | `integration-member-wake`：阅读 121 秒仍开放，自动锁屏后 119 秒直接返回 |
| 1 分 59 秒 / 2 分 01 秒；电源键 / AI 键唤醒 | 通过【实测】 | 两键 × 两时点均执行；119 秒免密，121 秒显示成员门；唤醒键不启动 AI |
| 错误密码不解锁 | 通过【实测】 | 通过现有 PIN 按钮回调输入错误值、检查错误提示，再输入正确值；`integration-people` 保留递增等待与隔离检查 |
| 正确密码回到原页、原位置 | 通过【实测】 | 真实阅读器第 4 页：页面对象、文本、内容版本、能力回执页码均保持 |
| 孩子不受影响；无 PIN 成人不受影响 | **部分通过** | 已处于孩子空间的两键唤醒、孩子不能切入带 PIN 成人、无 PIN 成人均通过；从成人门直接选孩子失败，见 MW-R1 |
| 时长一处可改、仍为暂定值 | 通过【实测／实现】 | `members.c:37,714–721`；默认 120，已有设置改成 30 后按 30 秒生效 |
| 开发报告 MW-7：后台忙时仍执行截止 | **部分通过** | 能力访问及唤醒门的截止检查有效；已开始的语音会话继续外发，见 MW-R2 |
| `system-services-design.md` §11.4 同步 | 已同步【实现】 | 第 720～730 行记录锁屏起算、原页恢复和暂定时长；其“到期后麦克风不再发送”承诺尚不满足 |
| G1 全量、内存检查、预算 | 通过【实测】 | 普通与 ASan/UBSan 均 95/95；已有预算未修改 |
| G2 能力路径、AI 快照与凭据边界 | 基础路径通过，存在撤销缺口 | 未新增用户操作或页面；复用设置能力、人工 PIN 接口和成员门脱敏快照；AI 的持续执行撤销见 MW-R2 |
| G3 依赖状态表 | 通过【实现】 | `ui-dependencies.md:159` 对应行已更新为“按推荐暂定，模拟器”，没有把占位外观写成完成设计稿 |
| G4 页面规格与对照图 | 不适用 | 本节为服务策略任务，没有新增页面；`MembersBack` 明示复用占位成员门，未声称完成页面像素验收 |
| G5 范围 | 未见无关改动 | 13 个文件；除服务、测试和说明外，应用管理器的 6 行差异直接服务于原页恢复，没有开发具体应用 |

## 命令与反向对照

| 命令 | 结果 |
| --- | --- |
| `cmake --build build -j4` | 退出 0；`INK_WERROR=ON` |
| `$INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j3` | 退出 0，95/95，46.75 秒 |
| `cmake --build build-asan -j4` | 退出 0；`INK_WERROR=ON`、`INK_SANITIZE=ON` |
| `UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build-asan --output-on-failure -j3` | 退出 0，95/95，75.90 秒 |
| 两份 `test-out/member-wake/metrics.json` | 均 174 条检查通过、`test.finished=1`、零警告、零预算违规 |
| `git diff --check 103382b1..HEAD` | 退出 0 |
| 差异隐私/I/O 检查、文档链接检查 | 本机路径/MAC 零命中；未新增 `write/read/fgets`；25 个本地链接均有效 |

四组撤修复均由本轮独立执行：从标准输入编译替代对象，插在原静态库前链接现有 `sim-member-wake` 测试；不改原文件、不覆盖正式测试可执行文件。编译和链接均退出 0，直接运行对应模拟器均退出 1，均到达 `test.finished=1`：

| 临时撤销内容 | 失败检查数 | 关键结果 |
| --- | --- | --- |
| `git show 103382b1:system/policy/members.c` | 25 | 阅读 121 秒与 119 秒免密返回失败 |
| `git show 103382b1:system/ui/app_registry.c` | 32 | 成员门底下成为主页，原书页丢失 |
| 只移除 `ink_member_space_open` 的截止检查 | 1 | 后台队列阻塞时仍放行过期授权 |
| 只移除 `on_power` 的唤醒截止检查 | 5 | 唤醒绕过排队中的关闭任务 |

结果分别在 `build/test-out/review-reverse-{members-baseline,apps-baseline,no-access-deadline,no-wake-deadline}-out/metrics.json`。这证明原有测试确实能测到报告列出的四类修复；本轮两项发现属于它们遗漏的组合场景。

## 范围、规则与架构审视

- 总规则第 1 节：只使用仓库、给定构建依赖和合成夹具；未操作设备、调用付费服务或真实凭据、对外写入、改历史或受保护资料。仅修改本报告，未提交。
- 第 4 节：分层与确定性检查均通过；新计时使用 OSAL，新增共享 64 位计时值使用原子读写，未加常驻任务或大缓冲。未新增忽略返回值的 `write/read/fgets`。没有执行 Linux/gcc 或实机测试，本机通过不代替这些验证。
- 既有测试期望变更与新锁屏规则相符；新增延时用于推进明确的模拟时刻，未发现通过 sleep 掩盖竞态、删除断言或放宽既有预算。
- R-061：成员策略负责授权、应用管理器负责生命周期、现有软总线负责能力调用，划分本身成立。当前缺口在两个边界的同步：新成员页面与生命周期未一致恢复（MW-R1），授权截止与持续流未一致撤销（MW-R2）。没有新增必须另开放给 AI 的操作；PIN 仍只允许人工输入。
- 构建、全量测试和补充探针逐轮串行，构建不超过 `-j4`、ctest 不超过 `-j3`。未启动带窗口的模拟器；既有 `gui-smoke` 使用 SDL dummy。
- 补充实验只生成被忽略的 `build*/test-out/` 对象、可执行文件和合成输出；没有改源文件、测试文件或开发报告。

## 补充复现脚本

在已完成上述构建的仓库根目录执行。`lifecycle` 对应 MW-R1，改成 `uplinkscan` 对应 MW-R2；增加 `REVIEW_VARIANT=control` 运行临时对象对照；增加 `REVIEW_BUILD=build-asan UBSAN_OPTIONS=halt_on_error=1` 使用内存检查构建。当前源码两项退出 1，对照退出 0。脚本读取当前测试夹具并在内存中替换测试入口，源文件保持原样。

```sh
REVIEW_CASE=lifecycle python3 - <<'PY'
from pathlib import Path
doc = Path("docs/tasks/member-wake/REVIEW.md").read_text()
code = doc.split("<!-- member-wake-probe -->\n```python\n", 1)[1].split("\n```", 1)[0]
exec(compile(code, "<member-wake-review>", "exec"))
PY
```

<!-- member-wake-probe -->
```python
BODIES = {
'lifecycle': r'''
#include "ink/sys/layout.h"
static volatile int review_pin_done, review_pin_result;
static void review_pin(void *ctx, int result, uint32_t wait_s) {
    (void)ctx; (void)wait_s;
    review_pin_result=result;
    review_pin_done++;
}
static bool review_click(lv_obj_t *root) {
    for (uint32_t i=0; i<lv_obj_get_child_count(root); i++) {
        lv_obj_t *child=lv_obj_get_child(root,(int32_t)i);
        if (lv_obj_check_type(child,&lv_label_class) && !strcmp(lv_label_get_text(child),"小朋友") && lv_obj_check_type(root,&lv_button_class)) {
            lv_obj_send_event(root,LV_EVENT_CLICKED,NULL);
            return true;
        }
        if (review_click(child)) return true;
    }
    return false;
}
static void pick_child(void) { CHECK(review_click(lv_layer_sys()),"pick child in gate"); }
static void test_task(void *arg) {
    (void)arg;
    delay_ms(1500);
    CHECK(ink_cap_register(TEST_CAPS,1)==0,"test capability");
    CHECK(ink_member_set_pin("owner",NULL,"1234",review_pin,NULL)==0 && WAIT_FOR(review_pin_done,5000) && review_pin_result==INK_PIN_OK,"synthetic PIN");
    CHECK(BY_HAND("app.open","{\"id\":\"alpha\"}") && g_rc.status==INK_RC_DONE,"adult alpha");
    snprintf(alpha_kept,sizeof alpha_kept,"owner fixture");
    snprintf(alpha_memory,sizeof alpha_memory,"owner fixture");
    ink_power_hold(INK_NEED_AWAKE);
    ink_power_lock();
    CHECK(WAIT_FOR(!strcmp(power_state(),"locked"),2000),"locked");
    delay_ms(121000);
    ink_power_unlock(osal_now_us());
    CHECK(WAIT_FOR(!strcmp(top(),"member_gate"),2000),"expired adult gate");
    on_ui(pick_child);
    CHECK(WAIT_FOR(!strcmp(ink_member_current()->id,"kid") && !strcmp(top(),"home"),2000),"child home");
    LOG_CLEAR();
    CHECK(BY_HAND("app.open","{\"id\":\"alpha\"}") && g_rc.status==INK_RC_DONE,"child alpha opened");
    CHECK(SEEN("alpha:start") && !alpha_memory[0],"child open must reset prior member memory; log=%s memory=%s",g_log,alpha_memory);
    snprintf(alpha_kept,sizeof alpha_kept,"child fixture");
    LOG_CLEAR();
    CHECK(BY_HAND("app.close","{\"id\":\"alpha\"}") && g_rc.status==INK_RC_DONE,"child alpha closed");
    CHECK(SEEN("alpha:save"),"child app.close must save; log=%s state=%s",g_log,ink_app_state_name(ink_app_state_of("alpha")));
    char path[INK_LAYOUT_PATH_MAX], saved[64]={0};
    CHECK(ink_layout_member(path,sizeof path,"kid","alpha.state")>=0,"child state path");
    int n=ink_storage_read(path,saved,sizeof saved-1);
    CHECK(n>0 && !strcmp(saved,"child fixture"),"child state must persist; n=%d bytes=%s",n,saved);
    CHECK(BY_HAND("app.open","{\"id\":\"beta\"}") && g_rc.status==INK_RC_DONE,"child beta opened");
    CHECK(call_as("test.perm",NULL,INK_SRC_APP) && g_rc.status==INK_RC_DONE,"declared app permission must work; result=%s",g_rc.detail);
    ink_power_release(INK_NEED_AWAKE);
    ink_probe_count("test.finished");
}
''',
'uplinkscan': r'''
static bool review_scanning(void) {
    char value[24] = "";
    ink_state_get_str(ink_state_find("wifi.state"),value,sizeof value);
    return !strcmp(value,"scanning");
}
static void test_task(void *arg) {
    (void)arg;
    delay_ms(1500);
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(),30000),"online");
    int before=g_pin_done;
    CHECK(ink_member_set_pin("owner",NULL,"1234",pin_told,NULL)==0 && WAIT_FOR(g_pin_done>before,5000) && g_pin_result==INK_PIN_OK,"synthetic PIN");
    CHECK(ink_setting_get("member.idle_return_s")==120,"default 120s grace");
    set_mode(ALWAYS);
    lock();
    uint64_t since=osal_now_us();
    delay_ms(116000);
    mic("wake");
    delay_ms(700);
    mic("speech");
    delay_ms(1000);
    CHECK(!strcmp(mic_state(),"sending"),"synthetic speech uploads within grace");
    uint64_t scan_at=since+119000000ull;
    if(osal_now_us()<scan_at) osal_task_delay_us(scan_at-osal_now_us());
    ink_call_opts_t opts={.src=INK_SRC_AI,.reply_exec=INK_EXEC_DIRECT,.done=rc_done};
    g_done=0;
    CHECK(ink_cap_call("wifi.scan",NULL,&opts)==0,"start the existing wifi.scan capability");
    CHECK(WAIT_FOR(review_scanning(),500),"real Wi-Fi scan is running in exec.bg");
    delay_ms(1500);
    CHECK(!ink_member_space_open() && review_scanning(),"space expired while Wi-Fi scan still runs");
    double sent=sim_metric_value("hub.binary_bytes");
    delay_ms(500);
    CHECK(sim_metric_value("hub.binary_bytes")==sent,"expired space uploaded %.0f extra bytes during real wifi.scan; mic=%s",sim_metric_value("hub.binary_bytes")-sent,mic_state());
    CHECK(strcmp(mic_state(),"sending")!=0,"expired microphone must stop sending during real wifi.scan");
    CHECK(WAIT_FOR(g_done,3000),"Wi-Fi scan eventually completes");
    CHECK(WAIT_FOR(strcmp(mic_state(),"sending")!=0,2000),"capture revoked once BG queue drains");
    mic("silence");
    ink_probe_count("test.finished");
}
'''
}

from pathlib import Path
import json,os,re,shlex,subprocess
root=Path.cwd(); build=root/os.environ.get('REVIEW_BUILD','build')
case=os.environ.get('REVIEW_CASE','lifecycle')
target='sim-apps' if case=='lifecycle' else 'sim-listening'
stem='apps' if case=='lifecycle' else 'listening'
body=BODIES[case]
source=(root/f'tests/integration/{stem}.c').read_text()
a=source.index('static void test_task(void *arg)'); b=source.index('\nvoid ink_main(void)',a)
source=source[:a]+body+source[b:]
flags=(build/f'CMakeFiles/{target}.dir/flags.make').read_text()
opts=[]
for k in ['C_DEFINES','C_INCLUDES','C_FLAGS']:
    opts+=shlex.split(re.search(r'^'+k+r' = (.*)$',flags,re.M).group(1))
link=shlex.split((build/f'CMakeFiles/{target}.dir/link.txt').read_text())
variant=os.environ.get('REVIEW_VARIANT','current')
obj=build/f'test-out/review-member-{case}.o'
exe=build/f'test-out/review-member-{case}-{variant}'
def compile_source(src,out,extra=None):
    cmd=[link[0],*opts,'-I'+str(root/'tests/integration'),*(extra or []),'-x','c','-c','-','-o',str(out)]
    r=subprocess.run(cmd,input=src,text=True,capture_output=True)
    print('compile_exit=',r.returncode,r.stderr,flush=True)
    if r.returncode: raise SystemExit(r.returncode)
compile_source(source,obj)
link=[str(obj) if p.endswith(f'tests/integration/{stem}.c.o') else p for p in link]
link[link.index('-o')+1]=str(exe)
if variant=='control':
    path='system/ui/app_registry.c' if case=='lifecycle' else 'system/policy/members.c'
    control=(root/path).read_text()
    if case=='lifecycle':
        control=control.replace('    ink_ui_restart(stop_for_member);','    AP.locked = ink_power_state() == INK_PWR_LOCKED || !ink_member_space_open();\n    ink_ui_restart(stop_for_member);')
    else:
        control=control.replace('    ink_exec_post(INK_EXEC_BG, &M.idle_job, idle_job, NULL);','    idle_job(NULL);')
    override=build/f'test-out/review-member-{case}-control.o'
    compile_source(control,override,['-I'+str(root/Path(path).parent)])
    link.insert(link.index('libink_system.a'),str(override))
r=subprocess.run(link,cwd=build,text=True,capture_output=True)
print('link_exit=',r.returncode,r.stderr,flush=True)
if r.returncode: raise SystemExit(r.returncode)
out=build/f'test-out/review-member-{case}-{variant}-out'
profile='metalio-plus' if case=='lifecycle' else 'metalio-plus-handsfree'
script='member_wake' if case=='lifecycle' else 'listening'
cmd=[str(exe),'--profile',f'simulator/profiles/{profile}.json','--budget',f'tests/integration/{script}.budget.json','--script',f'tests/integration/{script}.script','--assets',str(build/'assets'),'--content',str(build/'test-content'),'--out',str(out),'--quiet','--no-random-outages']
r=subprocess.run(cmd,text=True,capture_output=True)
print('probe_exit=',r.returncode,r.stdout[-1500:],r.stderr[-1500:],flush=True)
p=out/'metrics.json'
if p.exists():
    v=json.loads(p.read_text())
    print(json.dumps({'warnings':v['warnings'],'checks':{k:v['metrics'].get(k) for k in ['test.pass','test.fail','test.finished']},'budget_violations':v['budget_violations']},ensure_ascii=False,indent=2))

raise SystemExit(r.returncode)
```

SEVERE=0 HIGH=2
