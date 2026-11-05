# services-mid 复查

**结论：部分通过；严重 0、高 0。AUD18-402 仍有一项中级语义缺口，不能按执行报告认定本节全部完成。** 中枢桥资源回收、其余指定修复及本机既有测试通过。

2026-11-06（新加坡时间）；基线 `2c844243`，复查对象 `65983446e00318dc1d4faed646011b03666a2f9a`。已读复查提示、任务清单开头和 services-mid、总规则、执行报告、services-finish 第 6 轮及 hub-step1 第 2 轮复查，并回看五条 AUD18 原问题。开工时本文件为空；本轮仅覆盖本报告，不改产品或测试源码，不提交。

## 逐项验收

“实测”均为本轮独立执行；“核查”为本轮代码或文件检查。命令及证据见后文。

| 验收项 | 结论 | 本轮证据 |
| --- | --- | --- |
| AUD18-106 | 通过 | 【实测】旧失败记录迁移、第二次回滚、重启后两次历史保留、真人重试和产品校验通过；恢复修复前实现后 3 项功能断言失败。失败历史经数据服务持久化，自动检查与安装共用规则 |
| AUD18-206 | 通过 | 【实测】提交失败不报已显示、重试成功；旧实现 2 项功能断言失败。另用模拟 PMIC 持续 I2C 故障走真实 HAL，两套各 6 项通过：失败计数增长、回执 failed、不计 on_panel、不执行普通关机 |
| AUD18-207 | 通过 | 【实测】长选区、两侧标记及拼音插入位置映射通过；旧实现 3 项功能断言失败。【核查】实际点击调用同一字符映射，未用放宽断言代替修复 |
| AUD18-402 | **部分修复** | 【实测】页面与输入组合、会话/版本、密码遮蔽通过，旧实现 9 项断言失败；但原问题明确包含的候选、动作键和语音提示仍缺失，见 SM-01 |
| AUD18-404 | 通过 | 【实测】文档对应列检查覆盖 95 个可调能力、6 个限制调用、37 个状态、10 个设置、20 个事件；旧文档准确检出 8 个能力、1 个状态、1 个事件缺项。【核查】已说明测试源码清单与文档检查的区别，没有冒称自动生成能力总表 |
| services-finish 其余未修中级 | 未找到另一个遗留项 | 【核查】第 6 轮 REVIEW 将其余中级列为已修。【实测】R1-06 的 undo、R2-04 的 empty_removal、S4-DET 的两项确定性和两套栈计量均通过。402 单独保留为部分修复 |
| hub-step1 R4 | 通过 | 【实测】两套各同进程 80 次开关、线程创建失败、描述符耗尽及退出清理通过；144 个迟到回调被识别，关闭后描述符回到 3。另补网络线程先结束的 8 次失败连接，两套均回收。旧桥在第一轮关闭后仍有 2 个被跟踪分配，反向测试退出 1 |
| G1 | 本机通过 | 【实测】普通 94/94（41.52 秒）、ASan/UBSan 94/94（76.48 秒），无跳过；各含 114 项中枢 Python 测试。两套栈计量均 0 失败；预算未变 |
| G2 | 本次改动边界通过，原 402 未全闭合 | 【核查】无新增用户操作或页面，原能力入口与密码遮蔽保留；运行时能力清单检查通过。不能用“无新增页面”替代 402 的既有语义验收 |
| G3 | 通过 | 【实测/核查】ui-dependencies 只改文本、升级、电源、页面快照四行的第 3 列状态；其他判断原样保留 |
| G4 | 不适用 | 本任务未开发页面、未改界面布局；未做 10 屏设计稿或 ui-compare 验收。既有 gui-smoke 使用 SDL dummy，未启动带窗口的模拟器 |
| G5 | 通过范围核查 | 修改集中在明确指派的 R4 桥实现、HAL 失效句柄、测试和文档；没有扩展具体应用。服务设计改动用于 404 清单补漏及状态说明，没有改合同迁就行为 |

## 问题

### SM-01｜中｜AUD18-402 只接入已有文本状态，原键盘语义缺口仍在

- **位置**：`system/services/text/text_caps.c:39–79`；组合入口 `system/ui/ui_task.c:309–317`；真实显示分别在 `system/ui/shell/keyboard.c:121–123`、`:295–312`。现有专项 `tests/services_finish/keyboard_snapshot.c:10–18` 没有候选、动作键、失败提示断言；`docs/tasks/services-mid/REPORT.md:12` 将整条 402 记为复验通过。
- **复现命令**：运行下方 P1。使用真实文本服务、音频焦点、键盘 LVGL 标签和 `ink_ui_snapshot`，不替换生产函数。普通及 ASan/UBSan 各正常完成：12 项控制通过，3 项语义断言失败，无 sanitizer 诊断。
- **结果**：输入 `xian` 后实际键盘显示候选“先”和“搜索”按钮，快照只含 composing、值、光标等字段；随后真实高优先级音频焦点使语音输入拒绝，键盘显示“现在不能用语音输入”，快照仍只有 `voice:"idle"`，没有提示。三处可见标签均由探针读取确认，未把“服务里存在一个字符串”冒充界面显示。
- **影响**：用户针对当前候选、动作键或失败提示向 AI 提问时，自动上下文仍缺原问题指出的信息。原报告已将此定为中级；组合快照及密码遮蔽已有真实实现，保留原等级。
- **修法**：补当前候选页、动作键语义及语音提示的结构化快照；候选分页等由键盘维护的状态要进入组合快照，密码和 ai_hidden 继续统一遮蔽。补与真实可见状态对应的断言；原 402 应先记“部分修复”，修完后再关闭。
- **证据**：从本报告提取 P1 重跑，普通/内存证据为 `build/services-mid-review-keyboard-6r09l0h4/`、`build-asan/services-mid-review-keyboard-4doqxh_m/`，脚本按预期退出 1。非零退出专指语义断言失败，不能当作模拟器崩溃。

## 上一轮高问题与架构核查

- 【实测】附加复验 services-finish 的 R6-01/R6-02：`member_caps` 共 22 次启动、277 项通过，全部 `test.finished=1`。覆盖数据/时间调用中切成员、确认删除、删除后编号复用，以及通知/闹钟/提醒的迟到事件；没有把旧 REVIEW 的高问题照抄为仍然存在。本轮未重跑 services-finish 的全部 39 组，也未重新做这两项历史反向。
- 【核查】桥仍在模拟器宿主适配层。关闭句柄不 join；worker 清 socket 和待发队列，双方结束才释放对象。迟到事件保存独立 owner/代次副本；HAL 校验代次、close_req 和 state，关闭后清空 bridge 指针。退出时在宿主侧等待 worker 收尾。
- 【核查】未新增设备常驻任务、内部 RAM 大缓冲或绕过能力表的用户操作。宿主分配与锁操作在宿主线程或 vtk_host_call 中；原确定性测试通过。两套 PQBN AI 场景内部总 RAM 最低余量均为 32,880 字节，原门槛 32,768，差额仍 112 字节。
- 【核查】基线差异 10 文件；未删旧测试、放宽断言/预算、增加产品 sleep 或跳过测试。新增桥测试用条件变量控制交接。新增 send/recv 检查返回值；涉及的 read/write 也有返回值判断，未新增忽略 read/write/fgets 返回值的调用。
- 【实测】`git diff --check 2c844243..HEAD` 退出 0；按总规则隐私正则扫描完整差异，0 命中；变更文件均小于 5 MB，变更 Markdown 本地链接有效；受保护需求、想法、决策、研究、简报、既有审计原文均未改。

## 验证命令与证据边界

复用已存在的构建缓存，本轮先核对两套均 `INK_WERROR=ON`，内存套另有 `INK_SANITIZE=ON`；字体、Python、LVGL 指向给定的只读依赖目录，`INK_HUB_PYTHON` 指向本仓库已有环境。本轮没有重装依赖或重新配置构建。

依次执行，构建和全量测试没有重叠：

```sh
cmake --build build -j4
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/python" tests/services_mid/reverse.py
UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/python" tests/services_finish/run.py member_caps
UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/python" tests/services_finish/stack_meter.py
UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/python" tests/services_finish/stack_meter.py --build build-asan
```

以上命令均退出 0。反向脚本内部旧行为进程按预期退出 1；四项服务共 6 次模拟器正常完成、17 项功能断言失败（106/206/207/402 分别 3/2/3/9），不是编译错误或崩溃。

- 全量日志：两套 `Testing/Temporary/LastTest.log`；服务专项分别为 `build/services-finish-kiow17jj/`、`build-asan/services-finish-788r08_p/`，各 8 次启动、64 项通过。
- 反向：`build/services-mid-reverse-ara05nel/`，四项服务明细 `build/services-finish-n9tnma7e/`。R4/404 对照 `2c844243`；四项基线已有修复对照 `6f689264^`，已核对该修复提交在基线之前。
- 成员专项：`build/services-finish-rq71y4in/`；栈计量：`build/services-stack-2bf_rw__/`、`build-asan/services-stack-d1nr51cy/`。
- 真实 PMIC 故障补验：在复用编译/链接参数的临时整机探针中执行 system.shutdown，运行参数加 `--i2c-faults 1.0:pmic`，两套各 6 项通过；证据 `build/services-mid-review-offpanel-ol_hmvob/`、`build-asan/services-mid-review-offpanel-ep6mj9u6/`。
- worker 先退出补验：临时编译原 bridge_lifecycle 测试，在未监听的回环端口上等待 `b->finished` 后才关闭句柄，关闭前验证发送被拒绝，随后检查分配/描述符回落和迟到事件。两套各 8 次通过，证据 `build/services-mid-review-worker-first-952n5qav/`、`build-asan/services-mid-review-worker-first-1b1ey1nd/`。最初安排 80 次失败连接的探针超过其 30 秒总时限；定位为每次失败连接约 2 秒的已有等待，3 次诊断和最终 8 次版本均完成，不作为死锁发现。

只有 macOS/Apple clang、模拟器及合成回环中枢证据。没有 Linux gcc、实机、真实中枢账号或真实家庭数据结果；未联网调研、未触碰设备、未调用辅助代理、未提交或推送。本轮生成的探针及构建产物仅在忽略的构建目录；仓库可见改动仅本报告。

## P1：键盘语义缺口复现

从仓库根目录执行。编译参数含原 Werror；内存套沿用原 sanitizer 参数。每套模拟器退出 0 且 test.finished=1，脚本因为三项语义断言失败最终退出 1。可提取下面代码运行：

```sh
PYTHONDONTWRITEBYTECODE=1 "$INK_DEPS/build-deps/pyenv/bin/python" - <<'PY'
from pathlib import Path
text = Path('docs/tasks/services-mid/REVIEW.md').read_text()
code = text.split('```python services-mid-keyboard-probe\n', 1)[1].split('\n```', 1)[0]
exec(compile(code, 'services-mid-keyboard-probe', 'exec'))
PY
```

```python services-mid-keyboard-probe
import json, os, shlex, subprocess, tempfile
from pathlib import Path
root = Path.cwd()
source = r'''
#include "product.h"
static char snapshot[4096];
static int page_snapshot(char *out,size_t cap) { return snprintf(out,cap,"SYNTHETIC_PAGE"); }
static void create(lv_obj_t *screen,void *ctx) { (void)screen; (void)ctx; }
static const ink_activity_t PAGE={.name="synthetic-form",.create=create,.snapshot=page_snapshot};
static bool visible_label(lv_obj_t *o,const char *s) {
    if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN)) return false;
    if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),s)) return true;
    for(uint32_t i=0;i<lv_obj_get_child_count(o);i++)
        if(visible_label(lv_obj_get_child(o,(int32_t)i),s)) return true;
    return false;
}
static void focus(void) {
    ink_ui_push(&PAGE,NULL);
    ink_text_cfg_t cfg={.type=INK_TEXT_PLAIN,.enter=INK_ENTER_SEARCH,.hint="合成输入框"};
    CHECK(ink_text_focus(&cfg,NULL,NULL)>0,"synthetic field focused");
}
static void inspect(void) {
    for(const char *p="xian";*p;p++) ink_text_key(*p);
    ink_text_cand_t candidate;
    CHECK(ink_text_candidates(0,&candidate,1)==1,"candidate available");
    CHECK(visible_label(lv_layer_top(),candidate.word),"candidate is visible on real keyboard");
    CHECK(visible_label(lv_layer_top(),"搜索"),"search action is visible on real keyboard");
    CHECK(ink_ui_snapshot(snapshot,sizeof snapshot,NULL)>0,"combined snapshot fits");
    CHECK(strstr(snapshot,candidate.word)!=NULL,"visible candidate missing from snapshot");
    CHECK(strstr(snapshot,"搜索")||strstr(snapshot,"search"),"search action missing from snapshot");
    sim_warn("REVIEW candidate=%s snapshot=%s",candidate.word,snapshot);
    ink_text_blur(0);
    ink_text_cfg_t cfg={.type=INK_TEXT_NUMBER,.enter=INK_ENTER_SEARCH,.hint="合成输入框"};
    CHECK(ink_text_focus(&cfg,NULL,NULL)>0,"numeric field focused");
    int focus_id=ink_audio_focus_acquire(INK_AUDIO_PRIO_ALERT,NULL,NULL,INK_EXEC_UI);
    CHECK(focus_id>0,"real higher-priority focus");
    CHECK(ink_text_voice_start()!=0 && ink_text_voice_notice()[0],"real voice rejection sets notice");
    CHECK(visible_label(lv_layer_top(),ink_text_voice_notice()),"voice notice is visible on real keyboard");
    CHECK(ink_ui_snapshot(snapshot,sizeof snapshot,NULL)>0,"notice snapshot fits");
    CHECK(strstr(snapshot,ink_text_voice_notice())!=NULL,"visible voice notice missing from snapshot");
    sim_warn("REVIEW notice=%s snapshot=%s",ink_text_voice_notice(),snapshot);
    ink_audio_focus_release(focus_id);
    ink_text_blur(0);
    ink_probe_count("test.finished");
}
static void run(void *arg) {
    (void)arg; delay_ms(1800); on_ui(focus);
    CHECK(WAIT_FOR(sim_metric_value("text.dict_loaded")>=1,3000),"fixture dictionary loaded");
    on_ui(inspect);
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="review-keyboard",.fn=run,.stack_bytes=16384,.priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"test task");
}
'''
all_passed = True
for name in ('build','build-asan'):
    build=root/name
    work=Path(tempfile.mkdtemp(prefix='services-mid-review-keyboard-',dir=build))
    flags={}
    for line in (build/'CMakeFiles/sim-media.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            k,v=line.split(' = ',1); flags[k]=shlex.split(v)
    link=shlex.split((build/'CMakeFiles/sim-media.dir/link.txt').read_text())
    obj=work/'probe.o'
    subprocess.run([link[0],*flags['C_DEFINES'],*flags['C_INCLUDES'],*flags['C_FLAGS'],
                    '-I'+str(root/'tests/services_finish'),'-x','c','-','-c','-o',str(obj)],
                   input=source,text=True,cwd=build,check=True)
    link=[str(obj) if '/tests/integration/' in w and w.endswith('.o') else w for w in link]
    link[link.index('-o')+1]=str(work/'probe')
    subprocess.run(link,cwd=build,check=True)
    result=subprocess.run([str(work/'probe'),'--profile',str(root/'simulator/profiles/metalio-plus.json'),
        '--assets',str(build/'assets'),'--content',str(build/'test-content'),'--out',str(work/'out'),
        '--end','15s','--quiet','--no-random-outages'],cwd=root,text=True,capture_output=True,timeout=60,
        env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1','SDL_VIDEODRIVER':'dummy'})
    (work/'stdout.log').write_text(result.stdout); (work/'stderr.log').write_text(result.stderr)
    data=json.loads((work/'out/metrics.json').read_text())
    print(name,'exit=',result.returncode,'finished=',data['metrics'].get('test.finished'),
          'pass=',data['metrics'].get('test.pass'),'fail=',data['metrics'].get('test.fail'))
    for w in data.get('warnings',[]):
        if 'REVIEW ' in str(w) or 'CHECK FAILED' in str(w): print(w)
    print('Evidence directory:',work.relative_to(root))
    assert result.returncode==0 and data['metrics'].get('test.finished')==1
    all_passed &= data['metrics'].get('test.fail',0)==0
raise SystemExit(0 if all_passed else 1)
```

SEVERE=0 HIGH=0
