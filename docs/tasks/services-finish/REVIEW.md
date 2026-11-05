# 系统服务收尾窄范围复查（第 4 轮）

结论：**不通过，0 严重、1 高。** R3-02 的 Wi-Fi 撤销问题已修；R3-01 能挡住校验前已经删除的旧阅读页面，但校验与存储提交之间仍可穿过删除，见 R4-01。上轮其余已修问题的相关回归保持通过。除这一项修复遗漏外，未确认 `ed14461..HEAD` 引入其他严重/高问题。

2026-11-05；对象 `cabcb3c`（`cabcb3cb54002bb5795530ad6c26da06e326dcfc`），差异范围 `ed14461..cabcb3c`，64 个文件。本轮只核对上一轮问题、指定差异及成员删除数据清单，没有扩展成全仓审计。只修改本报告，不提交；由本复查方亲自执行命令，没有调用子代理。

## 已确认问题

### R4-01（高）：阅读器的代次校验与存储提交没有形成同一个生命周期保护，删除后仍能写回旧成人进度

- **位置**：`apps/reader/reader.c:253`、`:257`；`system/services/storage/storage_service.c:108`、`:179`、`:499`；`system/policy/members.c:604`。测试缺口在 `tests/services_finish/reader_late.c:24`、`:46`。
- **【实测】复现**：真实能力打开“测试长书”并到第 3 页。让真实 `save_cb` 先通过成员代次检查，在调用存储接口之前暂停；另一成人经成员服务和真实人工确认接口删除原成员，随后按同编号添加孩子。放行前旧位置不存在；放行后真实存储重新写出 `offset=1488`，孩子重新打开同书，真实 `reader.read_page` 回执为 **3**，预期 **1**。删除后不复用编号的分支也重新出现旧文件。普通换人对照仍正确保存第 3 页。
- **【实测】结果**：普通和 ASan/UBSan 两套均为 control **13 通过、0 失败**；removed **12 通过、1 失败**；reused **16 通过、2 失败**。所有模拟器退出 0、`test.finished=1`；探针脚本退出 **1**，没有 sanitizer 诊断。证据分别为 `build/services-review4-gap-dxh6b9jw/`、`build-asan/services-review4-gap-7q0cxcyi/`，日志为两套目录下的 `services-review4-reader-gap.log`。完整复现命令及脚本见 V4。
- **【实现】原因**：`save_cb` 的检查只在回调入口，未登记在途写者。删除只拒绝已经进入存储队列的工作；此时这笔保存还未入队。存储入口只问成员是否处于 `removing`，不接收旧调用的成员代次；删除完成、`used=false` 后，甚至未重建成员时，该判断也不再阻止旧写入。因此仅在入口加代次比较，不能把检查和提交变成一个不可穿过的操作。
- **测试为何漏掉**：现有 `reader_late` 在进入保存检查之前扣住 UI；删除后放行，新增检查当然会拒绝。本次只把暂停点移至检查之后，保留真实阅读器、真实删除和真实存储。未伪造保存结果，也未修改产品源码或已有测试文件。
- **影响与修法**：旧成人状态归入新孩子空间，R3-01 尚未彻底解决。保存应携带打开书时的成员代次，并让代次核对、在途/排队登记与删除互斥；或者删除先等待所有已进入保存路径的写者退出，再清理和释放编号。增加“校验已通过→删除/复用→实际提交”的回归；再加一次无锁检查仍有同类间隙。
- **证据边界**：这是在可并发服务调用边界显式暂停/恢复任务的确定性时序探针；人工确认通过真实接口注入，没有模拟屏幕点击。它证明检查与提交缺乏同步，不代表已测得真机自然触发频率。按时序问题计 **高**，不把 R2-01、R3-01、R4-01 重复计数，也不声称是 `4c0662f` 新引入。

## 上轮问题逐项复核

以下正向回归本次在普通及 ASan/UBSan 构建都独立执行。V3 的反向对照均成功编译、链接并实际触发功能断言失败，不以编译错误充当反例。

| 原编号 | 本轮结论 | 本次证据 |
| --- | --- | --- |
| R3-01 高 | 原复现场景已修，整体仍有 R4-01 | `reader_late` 六分支共 88 检查通过；V4 校验后暂停的分支失败。 |
| R3-02 高 | 已修 | `system/services/net/wifi.c:164` 只有保存成功且版本非零才发票据；`wifi_undo` 23 检查通过。旧源码 4 项断言失败。 |
| R2-01 严重 | 原闹钟/提醒、集合与文件清理已修；删除生命周期仍不通过 | `removal_alarm` 44+15、`member_inventory` 58+22 检查通过；跨校验写入见 R4-01。 |
| R2-02 高 | 指定问题已修 | `replay` 23 检查通过，含成员、来源、能力、参数碰撞、在途重试、迟到回执及关闭空间；准备、执行、答复入口均核查。 |
| R2-03 高 | 指定记录/文件问题已修 | `undo_data` 37 检查通过；条件删除在 DB 锁内，文件逆操作与修改入口共用锁。 |
| R2-04 中 | 已修 | `empty_removal` 4 检查通过；缺父目录的空白卡可删，故障清理仍保留编号。 |
| AUD18-106 中 | 已修指定问题 | `update_history` 3+6+9 检查通过，覆盖旧格式迁移、两个失败版本跨重启及人工重试规则。 |
| AUD18-206 中 | 已修指定问题 | `offpanel` 6 检查及全量 `integration-power-menu` 通过；拒绝面板提交不能报告显示成功。 |
| AUD18-207 中 | 已修指定问题 | `text_window` 7 检查通过，覆盖长选区、右选择标记和拼音点击映射。 |
| AUD18-402 中 | 已修指定问题 | `keyboard_snapshot` 16 检查通过，普通/数字输入带目标与版本，凭据内容掩码。 |
| AUD18-404 低 | 继续保留 | 能力文档漏项未修，修复报告明确披露；不计严重/高。 |
| R1-01 高 | 保留修复 | `upgrade_crash` 四链、16 次启动通过。 |
| R1-02 高 | 保留修复 | `playback` 及全量 `integration-media` 通过。 |
| R1-03 高 | 保留修复 | `activity` 31 检查通过，含 1,600 次动作与单成员积压。 |
| R1-04 严重 | 保留修复 | `permissions` 26、`policy` 30 检查通过。 |
| R1-05 高 | 故障清理保护保留 | `removal` 卡/Flash/缓存/PIN 四链、12 次启动通过；总生命周期边界见 R4-01。 |
| R1-06 中 | 设置修复保留 | `undo` 13 检查通过；其他逆操作见 R2-03/R3-02。 |
| S4-DET 中 | 保留修复 | 两套 `stack_meter.py` 均 0 失败，全量确定性测试通过。 |

本次额外随差异核对的同类撤销修复：`undo_schedules` **21** 检查通过，闹钟响铃、稍后再响、重复提醒推进和表内移动的运行期版本有效；`undo_owner` **12** 检查通过，延迟提供者不能借新成员身份生成票据。旧源码分别有 6、3 项断言失败。`system/policy/cap.c:1112` 将票据绑定原 pending 调用，`:1174` 执行逆操作继续携带原代次；未见这些变更引入新的严重/高问题。

上一轮保留的 A/B/C2 已修项也做正向再验证：`files`（AUD18-101/102/103）、`update`（104/105/107/108）、`storage`（109）、`audio voice`（201/204/205）、`text`（202/208/209）、`shutdown`（203）、`bindings`（401）、低电量两个分支（403），以及 `handoff isolation`，均通过。没有重做这些更早修复的全部反向实验。

## 成员删除：独立枚举

【实现】从 `apps/`、`system/` 的全部 `ink_db_def_t`/scope、原始存储写入、`ink_layout_member`、卡路径、`member/owner/creator` 字段及持有者反查，不沿用报告清单作为枚举依据。当前实际成员集合共 **5** 类，其中注册表可见 **4** 类，另有未注册的 `notify.prefs`；设备集合 `time.alarms` 和 `member.table` 必须另计。

| 按成员的数据或状态 | 实际保存与清理路径 | 核对结果/边界 |
| --- | --- | --- |
| `activity.log` | `system/policy/cap.c:165` 成员集合，Flash 成员目录及 DB 缓存统一清理；延迟活动槽带代次 | 集合哨兵、另一个成人控制、重启通过；`activity/removal_alarm` 覆盖积压和待记槽。 |
| `notify.inbox` | `system/services/notify/notify_service.c:39` 成员集合，换人释放持有引用；迟到通知核对代次 | 哨兵与重启、真实通知和延迟投递回归通过；活跃 DB 引用阻止清理。 |
| `notify.prefs` | `system/services/notify/notify_service.c:41` 成员集合，虽未注册，实际文件同样在成员目录 | 独立列入哨兵删除及另一成人保留检查，跨重启通过。 |
| `text.user_freq` | `system/services/text/text_service.c:28` 成员集合，输入结束释放；删除取消 flush/通知/压缩定时器并遗忘缓存 | 哨兵、重启及 `storage/text` 通过。 |
| `media.positions` | `system/services/audio/media_service.c:37` 成员集合；`:103` 延迟 Flash 保存核对播放成员代次 | 哨兵、重启及播放换人回归通过；不把静态哨兵说成所有并发时序已证明安全。 |
| `reader.<书哈希>.pos` | 原始 `/user/m/<成员>/` 文件，读缓存/预取一并清除 | 静态删除通过；真实阅读器在检查后迟到保存仍回写，**R4-01**。 |
| `<应用>.state`、应用自定义集合/原始小文件 | `system/ui/app_registry.c:81` 与通用成员目录；注册表保存入口核对应用成员代次 | 自定义集合、原始状态及另一成人保留通过；通用接口仍不携带写者代次，不能从静态清理推导全部应用写者安全。 |
| `/content/.m/<成员>/`，包括 `apps/<应用>/`、录音、嵌套文件和未完成文件 | `/me`、`/members/<成员>`、`/app` 的物理目录；递归删除。活跃个人文件句柄或文件作业阻止释放编号，旧目录句柄验代次 | 嵌套/应用文件删除、复用、重启及故障重试通过；录音由个人文件句柄保护并在换人时收尾。 |
| `/cache/`，特别是 `/cache/sys/media-pos` | 全局目录中含成员媒体断点，成员删除清整个可重建缓存，活跃缓存句柄阻止清理 | 普通缓存与实际媒体断点路径的哨兵跨重启消失；共享 `/family` 控制文件保留。 |
| `time.alarms` 内按 `owner` 保存的闹钟/提醒 | 设备集合；`ink_time_forget_member` 扫 owner，删持久记录、内存定时项和响铃，等待落盘并重排 RTC | `removal_alarm` 的真实能力创建、删除、复用、重启通过；不能因是设备集合而略过。 |
| `member.table` 的身份、角色、权限、PIN 盐/校验、尝试次数及等待状态 | 先持久化 `removing` 隔离，清完目标数据后才删身份记录/释放编号；PIN 作业核对代次 | 四种故障及重启重试、全量 people/reset 通过；仅逻辑删除，不承诺 RAM/Flash 物理取证擦除。 |
| 任务与文件作业 | 任务按 member/creator 清，包含完成历史和由此人发起的共享任务；活动任务取消；文件作业状态验代次 | `removal_alarm` 核对历史消失、两任务取消及旧句柄失效；`files` 保留原作业检查。 |
| 确认卡、撤销票据、去重/在途回执、事件、PIN 作业 | 确认卡在换人时撤回；票据/回执/事件及 PIN 作业绑定原身份或代次；传输队列换人/关空间失效 | `permissions/replay/undo_owner/removal_alarm/handoff` 通过。部分 RAM 留至槽位回收，未宣称逐字节清零。 |
| 剪贴板、输入/选区/undo/redo、AI/听写/播放录音会话、应用页面 | 换人直接清剪贴板/撤销采集，UI、媒体队列完成释放 | `text/bindings/audio/voice/handoff/isolation/playback` 及全量通过；页面自己的持久写入另见 R4-01。 |
| 排除：设备设置、`net.wifi`、`bt.devices`、升级状态/`update.failures`、共享书籍 | 当前设计/实现为设备共享，没有成员归属 | 不随个人删除。`system/services/comm/comm_service.c:36` 的短信暂存也没有 member 字段，单独识别为当前共享 RAM，没有冒报成已删除的成员数据。 |

【实测】`member_inventory` 在五类实际集合写合成哨兵，并保留另一成人控制；准备/删除/复用 **58** 检查、重启 **22** 检查，两套均通过。该测试验证存储清理，不等同于每个业务的完整操作或所有并发窗口验证。

## 验收与架构审视

| 验收项 | 本轮结论 |
| --- | --- |
| S1 | 未全部收尾：R4-01 仍需修。 |
| S2 | 原确认/权限及本轮撤销指定场景通过；未发现新增严重/高问题。 |
| S3 | 真实能力表 `actions` 两套各 6 检查通过，预算不变。 |
| S4 | 普通和 ASan/UBSan 构建、全量及现有服务回归通过；这不能覆盖 V4 的额外时序缺口。 |
| S6 | 不通过：删除与旧阅读器写者的生命周期同步未闭合。 |
| S5 | 差异/隐私/文件大小检查通过；本复查只改报告、不提交。 |

【实现】模块仍经软总线和服务分层，输入快照复用文本服务序列化，未发现 UI 直接改服务内部状态。按钮与 AI 继续走共同能力、结构化回执和策略入口，凭据及原 OWNER 例外未放松。新增版本条件由服务持有是合理边界；成员身份检查仍停留在调用前，与存储提交分离，这是 R4-01 的根因。AI 能力文档漏项 AUD18-404 继续保留；没有用本轮回归代替全产品能力覆盖审计。

## 独立命令与证据

使用用户指定的 `$INK_DEPS`。核对现有两套 CMake 缓存：`INK_WERROR=ON`，指定字体、Python 和只读 LVGL 依赖；`build-asan` 另为 `INK_SANITIZE=ON`。所有构建/测试串行执行，构建最多 `-j4`、CTest 最多 `-j3`；没有启动带窗口的模拟器，既有 `gui-smoke` 使用 SDL dummy。

### V1：构建和全量

四条均退出 **0**。常规 **86/86，25.78 秒**；ASan/UBSan **86/86，69.71 秒**。日志为相应构建目录下 `services-review4-build.log`、`services-review4-ctest.log`。

```bash
cmake --build build -j4
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build-asan --output-on-failure -j3
```

### V2：相关回归

常规分下列两批执行，ASan/UBSan 在 `run.py` 后加 `--build build-asan` 执行相同的全部组别。每套共 **32 组、78 次启动、809 项检查**通过，脚本退出 **0**。另跑低电量已保存分支，两套各 5 检查通过；栈计量均 0 失败。

```bash
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py reader_late wifi_undo undo_schedules undo_owner member_inventory removal_alarm removal replay undo_data empty_removal update_history text_window keyboard_snapshot offpanel
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py files storage update text bindings lowbattery audio voice handoff isolation shutdown policy actions permissions undo playback activity upgrade_crash
INK_FINISH_SAVED=1 UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py lowbattery
INK_FINISH_SAVED=1 UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --build build-asan lowbattery
$INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/stack_meter.py
$INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/stack_meter.py --build build-asan
```

证据目录：`build/services-finish-_gjqztuh/`、`build/services-finish-_a4ws926/`、`build-asan/services-finish-c8607a0t/`。主日志：`build/services-review4-focused.log`、`build/services-review4-inherited.log`、`build-asan/services-review4-regressions.log`；附加结果及退出码在 `build/services-review4-extra-results.json`。

### V3：反向对照

两条均退出 **1（预期）**，四组本轮修复及八组继承修复全部检出功能问题。旧 API 包装只补符号/原契约，不关闭断言。未对 V4 假设修复编造“修后通过”。

```bash
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --baseline 1daec81 reader_late wifi_undo undo_schedules undo_owner
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --baseline ed14461 replay undo_data removal_alarm empty_removal update_history text_window keyboard_snapshot offpanel
```

日志：`build/services-review4-reverse.log`、`build/services-review4-inherited-reverse.log`；证据目录 `build/services-finish-onbq04j9/`、`build/services-finish-nsz0goot/`。

### V4：独立的校验后保存探针

下方脚本只在忽略的构建目录生成探针，复用既有编译/链接参数。它引用原样的产品阅读器，仅用包装函数固定一次存储调用前的调度窗口；放行后调用真实存储。两条探针命令均退出 **1**，表示 R4-01 仍在。

```bash
$INK_DEPS/build-deps/pyenv/bin/python - <<'PYTHON'
from pathlib import Path
s = Path('docs/tasks/services-finish/REVIEW.md').read_text()
probe = s.split('<!-- REVIEW4_PROBE_BEGIN -->\n```python\n', 1)[1].split('\n```', 1)[0]
Path('build/services-review4-reader-gap.py').write_text(probe)
PYTHON
$INK_DEPS/build-deps/pyenv/bin/python build/services-review4-reader-gap.py
INK_REVIEW_BUILD=build-asan $INK_DEPS/build-deps/pyenv/bin/python build/services-review4-reader-gap.py
```

### V5：范围和资源检查

【实测】`git diff --check ed14461..HEAD` 退出 0；总规则 §6 隐私正则匹配 **0**；64 个差异文件均小于 5 MB，无预算文件变化。已有测试未删除或放宽；`replay.c` 新增来源碰撞断言。两套 PQBN AI 场景内部总 RAM 最低空闲 **32,880 字节**，原门槛 **32,768**，余量 **112**；内部失败分配均 **0**。证据 `build/services-review4-scope.json` 及本次全量生成的场景指标。

边界：只有 macOS 模拟器证据，没有 Linux、真机、真实中枢或界面外观十屏对照。本任务不涉及界面外观，不需要新产品决定。现有全绿测试不能替代成员生命周期时序验证。

## 独立探针脚本

<!-- REVIEW4_PROBE_BEGIN -->
```python
import json, os, shlex, subprocess, tempfile
from pathlib import Path
root=Path.cwd()
build=(root/os.environ.get('INK_REVIEW_BUILD','build')).resolve()
work=Path(tempfile.mkdtemp(prefix='services-review4-gap-',dir=build))
source=(root/'tests/services_finish/reader_late.c').read_text()
source=source.replace('#include "product.h"','#include "tests/services_finish/product.h"')
source=source.replace('static bool ui_held, release_ui;', 'bool ui_held, release_ui;\nextern bool review_arm;')
start=source.index('static void hold_ui(void *ctx) {')
end=source.index('\nstatic void noop',start)
source=source[:start]+'''static void hold_ui(void *ctx) {
    (void)ctx;
    review_arm = true;
    ink_test_reader_save();
}
'''+source[end:]
(work/'probe.c').write_text(source)
(work/'reader_gap.c').write_text(r'''
#include "ink/sys/storage.h"
#include "ink/osal.h"
#include <string.h>
extern bool ui_held, release_ui;
bool review_arm;
static int review_write(const char *, const void *, size_t);
#define ink_storage_write_atomic review_write
#include "apps/reader/reader.c"
#undef ink_storage_write_atomic
static int review_write(const char *path, const void *data, size_t len) {
    if (review_arm && !strncmp(path, "/user/m/oldadult/reader.", 23)) {
        review_arm = false;
        ui_held = true;
        while (!release_ui) osal_task_delay_us(10000);
    }
    return ink_storage_write_atomic(path, data, len);
}
void ink_test_reader_save(void) { save_cb(NULL); }
''')
flags={}
for line in (build/'CMakeFiles/sim-media.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        k,v=line.split(' = ',1);flags[k]=shlex.split(v)
link=shlex.split((build/'CMakeFiles/sim-media.dir/link.txt').read_text())
objects=[]
for name in ('probe','reader_gap'):
    obj=work/(name+'.o')
    subprocess.run([link[0],*flags['C_DEFINES'],*flags['C_INCLUDES'],'-I'+str(root),*flags['C_FLAGS'],'-Wno-unused-function','-c',str(work/(name+'.c')),'-o',str(obj)],cwd=build,check=True)
    objects.append(str(obj))
for i,word in enumerate(link):
    if '/tests/integration/' in word and word.endswith('.o'):
        link[i]=objects[0];break
binary=work/'reader-gap'
link[link.index('-o')+1]=str(binary)
link.insert(next(i for i,x in enumerate(link) if x.endswith('.a')),objects[1])
subprocess.run(link,cwd=build,check=True)
failed=False
for variant in ('control','removed','reused'):
    path=work/variant;(path/'sd').mkdir(parents=True)
    env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'}
    if variant=='control':env['INK_REVIEW_CONTROL']='1'
    if variant=='removed':env['INK_REVIEW_NO_REUSE']='1'
    cmd=[str(binary),'--profile',str(root/'simulator/profiles/metalio-plus.json'),'--assets',str(build/'assets'),'--content',str(build/'test-content'),'--out',str(path/'out'),'--end','25s','--quiet','--no-random-outages','--flash-image',str(path/'user.img'),'--sd-dir',str(path/'sd')]
    run=subprocess.run(cmd,cwd=root,env=env,capture_output=True,text=True,timeout=180)
    (path/'stdout.log').write_text(run.stdout);(path/'stderr.log').write_text(run.stderr)
    metrics=json.loads((path/'out/metrics.json').read_text());m=metrics['metrics']
    ok=run.returncode==0 and m.get('test.finished')==1 and m.get('test.fail',0)==0
    print(variant,'PASS' if ok else 'FAIL','exit='+str(run.returncode),'checks='+str(m.get('test.pass',0)),'failures='+str(m.get('test.fail',0)),flush=True)
    for w in metrics.get('warnings',[]):print(w)
    failed |= not ok
print('Evidence directory:',work.relative_to(root),flush=True)
raise SystemExit(1 if failed else 0)
```

SEVERE=0 HIGH=1
