# 系统服务收尾复查（第 3 轮）

结论：**不通过，0 严重、2 高。** 上轮四项问题的原复现场景及四项继承中级问题均已修复；成员删除仍漏掉阅读器延迟保存，撤销保护仍漏掉 Wi-Fi 保存失败后的票据。两项均独立复现于常规和 ASan/UBSan 构建。S2、S6 不能据现有测试全绿判定完成。

2026-11-05；对象 `6f68926`，差异范围 **`ed14461..HEAD`**，共 56 个文件。本轮按用户要求做窄范围复查：旧问题、指定差异的新严重/高问题、成员数据枚举；没有扩展成全仓审计。以下两项是相关修复仍有遗漏，未另确认该提交引入其他严重/高问题。

只修改本报告，不提交。构建最多 `-j4`，CTest 最多 `-j3`，构建和全量测试串行；没有带窗口的模拟器。全量中的既有 `gui-smoke` 使用 `SDL_VIDEODRIVER=dummy`。临时探针、合成数据和日志均在忽略的构建目录；没有改产品源码或已有测试，没有调用子代理。

## 已确认问题

### R3-01（高）：删除并复用成员编号后，旧阅读器的延迟保存恢复了成人阅读位置

- 位置：`apps/reader/reader.c:250`、`:495`；本轮相关保护 `system/ui/app_registry.c:90`、`system/services/storage/storage_service.c:108`、`system/policy/members.c:604`。现有测试缺口：`tests/services_finish/removal_alarm.c:71` 已先切走并等 UI 处理，再延迟 UI，未覆盖旧阅读页面尚未关闭的时序。
- 【实测】通过真实 `reader.open_book`、`reader.goto_page` 打开“测试长书”至第 3 页。扣住 UI 执行队列以固定清理延迟，另一成人经成员服务切换、真实人工确认接口删除原成员，再同编号建孩子。此时原位置文件不存在。放行 UI 后，旧页面销毁重新写回 `offset=1488`；孩子打开同一本书恢复到页索引 2。没有替换产品函数或伪造删除成功；普通换人、不删除的对照保留成人进度，符合预期。
- 【实现】`save_cb` 只保存旧 `RD.member`，没有打开时的成员代次；`book_destroy` 仍补保存。新增 app 注册表检查只保护 `save_state`，不覆盖页面销毁。原始存储只在 `removing` 为真时拒绝写；编号释放/复用后，这笔旧写入又被接受。删除静态目录正确，不能证明旧写者已经退出。
- 复现命令：V5，`INK_REVIEW_CASES=reader_late`。常规证据 `build/services-review3-extra-0bzahtex/`；内存检查 `build-asan/services-review3-extra-1h__gelq/`。两套均为 control **13 通过**，reused **14 通过、2 失败**，模拟器退出 0、完成标记存在，无 sanitizer 诊断。UI 延迟是显式时序探针，不代表已做实机时序验证。
- 影响与修法：旧成人的个人阅读状态归给新孩子，属于 R2-01 删除隔离的高等级遗漏。阅读器打开时绑定成员代次，定时、销毁等全部保存入口都校验；或删除前等待所有旧页面/应用写者退出，并让存储写入携带生命周期身份。增加“旧阅读页面待关闭→删除→复用→放行→重新阅读”的回归。该项不是归因于本轮新引入。

### R3-02（高）：Wi-Fi 表满时生成无版本的撤销票据，随后删除真人新保存的网络

- 位置：`system/services/net/wifi.c:156`、`:164`、`:169`、`:189`，`system/services/data/datastore.c:856`；测试缺口：`tests/services_finish/undo_data.c:58` 起只覆盖原保存成功的 Wi-Fi 逆操作。
- 【实测】真实 `net.wifi` 集合达到 **8 条**上限后，AI 连接第 9 个合成网络：连接成功、记录保存失败，却仍得到 `undo_ms>0`、`revision=0`。真人确认忘掉一个旧网络，再成功保存目标网络；`wifi.list` 此时能读到目标。AI 携最初的 object/revision=0 调 `system.undo`，直接得到 `undone`，真人新保存的目标网络消失。没有 OOM、I/O 故障或保存函数替身；容量上限由真实数据服务执行。
- 【实现】保存失败仅计数，是否发撤销仍只看操作前的 `can_undo`。失败时 `written_ver` 没被写入，票据把 0 传给条件删除；`ink_db_del_if_ver` 将 0 当成“不检查版本”。因此本轮增加的版本保护在保存失败路径失效，旧票据仍获 `INK_SRC_CONFIRMED` 删除权限。
- 复现命令：V5，`INK_REVIEW_CASES=wifi_undo`。常规证据 `build/services-review3-extra-t6aa_w3f/`；内存检查 `build-asan/services-review3-extra-rb6xc870/`。两套各 **17 通过、2 失败**，模拟器退出 0、完成标记存在，无 sanitizer 诊断。
- 影响与修法：现实的“表满→真人腾位置并保存→旧撤销”顺序会丢新凭据。只有实际保存成功并拿到非零记录版本，才可发删除该记录的票据；失败分支不能把版本 0 当作撤销授权。增加保存失败后重新保存的正反例。原 R2-03 的记录/文件两条链已修；此项是本轮扩展到 Wi-Fi 的同类保护遗漏，不把它说成本轮才引入的缺陷。

## 上轮问题逐项复核

下列正向测试本次在两套构建均运行。八组新增回归另用 `--baseline ed14461` 撤掉相应修复；全部编译、链接成功并产生功能断言失败，脚本退出 1，不是以编译失败充当反例。

| 原编号 | 本次结论 | 独立证据 |
| --- | --- | --- |
| R2-01 严重 | 原闹钟/提醒泄漏已修，整体仍有 R3-01 | `removal_alarm` 删除、复用、重启共 44+15 检查通过；额外枚举探针见下表。 |
| R2-02 高 | 原缓存越界已修 | `replay` 22 检查通过：跨成员/参数/能力碰撞、在途重试、迟到回执、关闭空间及正常重试。源码检查准备和答复入口的成员代次与策略校验。 |
| R2-03 高 | 原记录/文件覆盖已修，同类入口仍有 R3-02 | `undo_data` 37 检查通过：人工更新/追加、重建记录、原子替换、移动后修改、通知已读、校时和已成功保存的 Wi-Fi。条件删除在数据锁内、文件逆操作在修改锁内。 |
| R2-04 中 | 已修 | `empty_removal` 4 检查通过；空白卡不再被缺少父目录卡住，既有失败隔离测试仍通过。 |
| AUD18-106 中 | 已修指定问题 | `update_history` 3+6+9 检查通过，旧格式迁移、两个失败版本跨重启保留、普通新版本与人工重试规则；128 KiB 历史上限和失败时拒绝已披露。 |
| AUD18-206 中 | 已修指定问题 | `offpanel` 6 检查通过，面板拒绝不能报显示成功，恢复后重试能关机；全量 `integration-power-menu` 通过。 |
| AUD18-207 中 | 已修指定问题 | `text_window` 7 检查通过，长选区、右选择标记、拼音点击映射；源码截取保持完整 UTF-8。 |
| AUD18-402 中 | 已修指定问题 | `keyboard_snapshot` 16 检查通过，页面与普通/数字/密码输入组合，凭据内容掩码。 |
| AUD18-404 低 | 仍保留，未冒充完成 | 能力文档总表本轮未改，修复报告明确列为未做；不影响本报告严重/高计数。 |
| R1-01 高 | 保留修复 | `upgrade_crash` 四链 16 次启动通过。 |
| R1-02 高 | 保留修复 | `playback` 12 检查及全量 `integration-media` 通过。 |
| R1-03 高 | 保留修复 | `activity` 31 检查通过，含 1,600 次动作和单成员积压。 |
| R1-04 严重 | 保留修复 | `permissions` 26 检查及 `policy` 30 检查通过。 |
| R1-05 高 | 故障清理保护保留；总范围仍见 R3-01 | `removal` 卡/Flash/缓存/PIN 四链 12 次启动通过。 |
| R1-06 中 | 设置修复保留 | `undo` 13 检查通过；非设置入口按 R2-03/R3-02 分别判断。 |
| S4-DET 中 | 保留修复 | 两套 `stack_meter.py` 均 0 失败，全量确定性测试通过，未改预算或比较项。 |

上轮原 A/B/C2 已修项的正向再验证：`files` 覆盖 AUD18-101/102/103，`update` 覆盖 104/105/107/108，`storage` 覆盖 109，`audio voice` 覆盖 201/204/205，`text` 覆盖 202/208/209，`shutdown` 覆盖 203，`bindings` 覆盖 401，`lowbattery` 两分支覆盖 403；`handoff isolation` 保留原 S6 场景。均通过。本次没有重做这些更早提交的反向实验。

## 成员删除：独立枚举与核对

【实现】在 `apps/`、`system/` 全部 C 源码搜索 `ink_db_def_t`、`INK_DB_MEMBER/DEVICE`、`ink_layout_member`、原始存储写入、成员路径及 `member/owner` 暂存字段，结合注册表和实际写入点枚举。不能仅以是否放在成员目录判断归属。

| 数据/状态 | 所有权与清理路径 | 本次证据、边界 |
| --- | --- | --- |
| `activity.log` | 成员集合；删除集合缓存和成员 Flash 目录；待记槽绑定代次 | V5 `inventory` 哨兵删除及重启通过；V2 `removal_alarm/activity` 覆盖延迟待记。 |
| `notify.inbox`、`notify.prefs` | 两个成员集合；收件箱持有引用会阻止清理，换人释放；迟到通知绑定代次 | V5 两集合删除及其他成人保留通过；V2 延迟通知通过。`notify.prefs` 未在公开集合注册表中，按实际源码另列入，未漏掉。 |
| `text.user_freq` | 成员集合；输入结束释放引用，删除事务遗忘缓存/定时 flush | V5 实际集合哨兵删除、重启通过；已持有的 DB 阻止清理有 V2 `removal` 对照。 |
| `media.positions` | 成员集合；延迟 Flash/卡保存核对播放时成员代次 | V5 集合删除/重启通过；源码检查 `media_service.c:103/:121`。 |
| `reader.<书哈希>.pos` | 成员原始 Flash 文件；清理读缓存/预取后删目录 | 静态文件哨兵通过，但真实阅读器延迟重写失败，**R3-01**。 |
| `<应用>.state`、应用自定义成员集合/原始小文件 | 成员目录统一清；app 注册表 `save_state` 增加代次检查 | V5 原始状态哨兵、V2 自定义集合通过；不能把 app 回调保护推广为所有页面保存。 |
| `/content/.m/<成员>/`、其 `apps/<应用>/` | `/me`、`/members/<成员>`、`/app` 的实际存放处，含录音等个人大文件、未完成临时文件 | V5 嵌套文件及应用文件删除、复用、重启通过；V2 卡故障保留编号，活跃句柄/作业阻止删除，旧目录句柄代次失效。 |
| `/cache/`，特别是 `sys/media-pos` | 现实现含成员媒体断点，不能因是全局目录就忽略；删除成员时清整个可重建缓存 | V5 全局缓存哨兵删除/重启通过；活跃缓存句柄阻止清理。其他家庭共享 `/content` 保留，有控制文件实测。 |
| `time.alarms` | **设备集合中按 owner 保存的私有闹钟/提醒** | 本轮显式扫描 owner、删除持久记录/内存闹钟/响铃并重排 RTC；V2 删除→复用→重启及原时间测试通过。 |
| `member.table` 的目标记录 | 身份、角色、PIN 盐/校验、尝试次数、等待时间、removing 状态 | 最后才持久删除身份并释放编号；V2 `removal` 故障/重试和全量 people/reset 通过。 |
| 任务、文件作业、撤销票据、去重/在途回执、事件、PIN 作业 | 任务按 owner/creator 清理并取消；其余旧身份的状态按代次失效，部分仍留 RAM 至回收 | V2 `removal_alarm/replay/undo_data` 覆盖旧任务、事件、通知、撤销及回执。这里是逻辑失效，不宣称逐字节擦除 RAM。 |
| 剪贴板、输入/选区/undo、AI/听写/播放录音会话、应用页面 | 换人时直接撤销采集/清剪贴板，UI/媒体队列完成释放 | V2 `text/bindings/audio/voice/handoff/isolation/playback` 及全量对应测试通过；页面自己的持久写入另见 R3-01。 |
| 设备设置、`net.wifi`、`bt.devices`、升级状态/失败历史、共享书籍 | 现有设计/实现是设备共享，没有按成员存放，不随个人删除 | 从成员清理清单中明确排除；Wi-Fi 本轮的撤销保护问题另列 R3-02。通信服务当前短信暂存也没有成员 owner，不把它伪报成已清的个人集合。 |

V5 `inventory` 在真实的五类成员集合写合成哨兵（四类已注册，另含 `notify.prefs`），同时写另一成人控制记录；删除并重建后分别检查旧记录消失、控制记录保留。两套均首次启动 **54 检查通过**，重启 **20 检查通过**。这是存储清理验证，不把合成哨兵说成每个业务都完成了用户操作测试。

## 验收与架构审视

| 编号 | 本次结论 | 依据 |
| --- | --- | --- |
| S1 | 尚未全部收尾 | 原指定场景及四项中级修复成立；R3-01/02 仍需修。 |
| S2 | 不通过 | 确认/权限原测试保持；R3-02 仍可借旧撤销删除真人新数据。 |
| S3 | 通过本次验证 | `actions` 两套各 6 检查，真实能力表完整；预算未放宽。 |
| S4 | 通过本次构建、全量及内存检查 | V1 两套各 86/86；V2 两套各 27 组、低电量保存对照和栈计量通过。 |
| S6 | 不通过 | 删除身份生命周期仍漏阅读器旧写者，R3-01。 |
| S5 | 范围/规则检查完成 | 只改本报告、不提交；指定差异隐私匹配 0、空白检查通过，无预算变更或超 5 MB 文件。 |

【实现】能力仍经总线和服务，页面快照复用输入服务序列化，未发现把服务内部状态改成 UI 私改；新增 generation/条件版本接口方向合理。但成员生命周期没有覆盖所有写者，逆操作把 0 版本作为无条件权限，这两处边界未闭合。AI 能力开放与原 OWNER 例外未被收窄；文档漏项 AUD18-404 仍保留。文件撤销使用全卡修改序号的保守失效策略，修复报告已披露，本轮不另判严重/高。

## 独立命令与证据

使用用户指定的 `$INK_DEPS`；现有缓存核对为 `INK_WERROR=ON`、指定字体/只读 LVGL/Python 依赖，内存构建另为 `INK_SANITIZE=ON`。以下各轮串行，没有同跑构建或全量。

V1：构建均退出 0；常规全量 **86/86，26.06 秒**，ASan/UBSan 全量 **86/86，70.37 秒**。日志为两套目录下 `services-review3-build.log`、`services-review3-ctest.log`。

```bash
cmake --build build -j4
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build-asan --output-on-failure -j3
```

V2：下面命令各跑两套（第二套在脚本后加 `--build build-asan`），全部退出 0。27 组未接入 CTest，已单独执行。主证据目录 `build/services-finish-0glgxe75/`、`build-asan/services-finish-96eh_e5x/`；日志 `services-review3-regressions.log`、`services-review3-saved.log`、`services-review3-stack.log`。

```bash
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py files storage update text bindings lowbattery audio voice handoff isolation shutdown policy actions permissions undo playback activity upgrade_crash removal replay undo_data removal_alarm empty_removal update_history text_window keyboard_snapshot offpanel
INK_FINISH_SAVED=1 UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py lowbattery
$INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/stack_meter.py
```

V3：新增八组反向，退出 **1（预期）**。全部正常链接并实际检出旧问题。已审读兼容包装：只补旧版本缺失符号/旧回调契约，未把断言关闭或改成恒真。证据 `build/services-review3-reverse.log`、`build/services-finish-w5_ogw7n/`。

```bash
UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/python tests/services_finish/run.py --baseline ed14461 replay undo_data removal_alarm empty_removal update_history text_window keyboard_snapshot offpanel
```

V4：【实测】`git diff --check ed14461..HEAD` 退出 0；总规则 §6 的隐私正则对本轮差异匹配 **0** 行；56 文件均小于 5 MB；无 `*.budget.json` 变化。已有 C 测试的断言/等待阈值没有改，`run.py` 增加组别、持久启动和只读旧源码替换。UI/公共头文件的变更均对应本轮问题所需接口或语义修复，未见依赖升级、无关重构。两套 PQBN AI 最低内部余量 **32,880 字节**，门槛 **32,768**，内部失败分配 0。证据 `build/services-review3-independent-closure.json`。

V5：下方为本次独立探针的完整脚本，使用构建已有的编译、链接参数；不替换产品实现。提取后按组运行，设置 `INK_REVIEW_BUILD=build-asan` 可复现内存检查版本。`reader_late`、`wifi_undo` 脚本退出 1 表示发现问题，`inventory` 退出 0；各模拟器进程退出 0。日志为两套目录下 `services-review3-probes.log`、`services-review3-wifi-undo.log`。

```bash
$INK_DEPS/build-deps/pyenv/bin/python - <<'PYTHON'
from pathlib import Path
s = Path('docs/tasks/services-finish/REVIEW.md').read_text()
probe = s.split('<!-- REVIEW3_PROBE_BEGIN -->\n```python\n', 1)[1].split('\n```', 1)[0]
Path('build/services-review3-probes.py').write_text(probe)
PYTHON
INK_REVIEW_CASES=reader_late $INK_DEPS/build-deps/pyenv/bin/python build/services-review3-probes.py
INK_REVIEW_CASES=inventory $INK_DEPS/build-deps/pyenv/bin/python build/services-review3-probes.py
INK_REVIEW_CASES=wifi_undo $INK_DEPS/build-deps/pyenv/bin/python build/services-review3-probes.py
```

边界：本轮仅 macOS 模拟器证据，没有 Linux、真机或真实中枢验证；不做界面外观十屏对照。未核查范围外的已知待办，不把全量全绿扩大为整个产品验收。两项高问题都不需要新的产品选择。

## 独立探针脚本

<!-- REVIEW3_PROBE_BEGIN -->
```python
import json, os, shlex, subprocess, tempfile
from pathlib import Path
root=Path.cwd(); build=(root/os.environ.get('INK_REVIEW_BUILD','build')).resolve()
work=Path(tempfile.mkdtemp(prefix='services-review3-extra-',dir=build))
common=r'''
#include "tests/services_finish/product.h"
#include "ink/sys/crc.h"
#include "ink/hal_power.h"
static int results;
static char result_status[24];
static void result(void *ctx,const char *id,const ink_args_t *args) {
    (void)ctx;(void)id;ink_args_get_str(args,"status",result_status,sizeof result_status);results++;
}
static bool confirmed(const char *cap,const char *args) {
    if(!BY_HAND(cap,args)||g_rc.status!=INK_RC_PENDING_CONFIRMATION)return false;
    unsigned token=0;sscanf(g_rc.object,"confirm:%u",&token);
    int before=results;
    return ink_confirm_decide(token,true,INK_SRC_TOUCH)==0&&WAIT_FOR(results>before,5000)&&!strcmp(result_status,"done");
}
'''
entry=r'''
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="review3-probe",.fn=run,.stack_bytes=16384,
        .priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg,NULL)==OSAL_OK,"test task");
}
'''
sources={}
sources['reader_late']=common+r'''
static bool ui_held, release_ui;
static ink_exec_job_t held_ui_job;
static void hold_ui(void *ctx) { (void)ctx; ui_held=true; while(!release_ui) osal_task_delay_us(10000); }
static void noop(void) {}
static void run(void *arg) {
    (void)arg; delay_ms(1800);
    CHECK(ink_cap_subscribe("confirm.result",result,NULL,INK_EXEC_DIRECT)==0,"listener");
    CHECK(confirmed("member.add","{\"id\":\"oldadult\",\"role\":\"adult\"}"),"add adult");
    CHECK(ink_member_switch("oldadult")==0,"enter adult"); delay_ms(300);
    CHECK(AI("reader.open_book","{\"title\":\"测试长书\"}")&&g_rc.status==INK_RC_DONE,"open actual reader");
    CHECK(AI("reader.goto_page","{\"page\":3}")&&g_rc.status==INK_RC_DONE,"adult advances to page 3");
    char path[96], raw[64]={0};
    snprintf(path,sizeof path,"/user/m/oldadult/reader.%08x.pos",(unsigned)ink_crc32(0,"测试长书",strlen("测试长书")));
    CHECK(ink_exec_post(INK_EXEC_UI,&held_ui_job,hold_ui,NULL)==0&&WAIT_FOR(ui_held,1000),"delay UI cleanup");
    CHECK(ink_member_switch("owner")==0,"other adult takes device");
    if (!getenv("INK_REVIEW_CONTROL")) {
        CHECK(confirmed("member.remove","{\"id\":\"oldadult\"}"),"delete while UI cleanup queued: %s",result_status);
        CHECK(confirmed("member.add","{\"id\":\"oldadult\",\"role\":\"child\"}"),"reuse as child");
        CHECK(ink_storage_read(path,raw,sizeof raw-1)<0,"old reading position absent before delayed callback");
    }
    release_ui=true; on_ui(noop);
    CHECK(WAIT_FOR(!ink_storage_pending(),4000),"late saves settled");
    int n=ink_storage_read(path,raw,sizeof raw-1);
    if(getenv("INK_REVIEW_CONTROL")) CHECK(n>0,"normal handoff preserves adult reading position");
    else CHECK(n<0,"deleted adult reading position returned: %s",raw);
    CHECK(ink_member_switch("oldadult")==0,"enter recreated member"); delay_ms(300);
    CHECK(AI("reader.open_book","{\"title\":\"测试长书\"}")&&g_rc.status==INK_RC_DONE,"open reader again");
    double restored=sim_metric_value("reader.restored_page");
    CHECK(restored==(getenv("INK_REVIEW_CONTROL")?2:0),"recreated member inherited adult page index %.0f",restored);
    ink_probe_count("test.finished");
}
'''+entry
sources['inventory']=common+r'''
static const ink_db_def_t PREFS={.name="notify.prefs",.scope=INK_DB_MEMBER,.schema=1,.json=true};
static const char *const collections[]={"activity.log","notify.inbox","notify.prefs","text.user_freq","media.positions"};
static const char *const rawpaths[]={"/user/m/oldadult/reader.12345678.pos","/user/m/oldadult/app.synthetic.state"};
static const char marker[]="{\"text\":\"SYNTH_PRIVATE\"}";
static const ink_db_def_t *def(int i) { return i==2?&PREFS:ink_db_find_def(collections[i]); }
static void inspect(void) {
    for(int i=0;i<5;i++) {
        ink_db_t *db=ink_db_open(def(i),"oldadult");
        CHECK(db&&ink_db_get(db,88001,NULL,NULL,0)==INK_DB_ERR_NOT_FOUND,"old record absent: %s",collections[i]);
        ink_db_close(db);
        db=ink_db_open(def(i),"owner");
        CHECK(db&&ink_db_get(db,88001,NULL,NULL,0)==INK_DB_OK,"other adult record preserved: %s",collections[i]);
        ink_db_close(db);
    }
    char raw[64];
    for(int i=0;i<2;i++) CHECK(ink_storage_read(rawpaths[i],raw,sizeof raw)<0,"raw member file absent: %s",rawpaths[i]);
    CHECK(ink_member_switch("oldadult")==0,"enter child");
    ink_fs_stat_t st;
    CHECK(ink_fs_stat("/me/nested/private.txt",&st)!=0,"nested personal file absent");
    CHECK(ink_fs_stat("/app/synthetic/private.txt",&st)!=0,"app private file absent");
    CHECK(ink_fs_stat("/cache/sys/synthetic",&st)!=0,"global cache removed");
    CHECK(ink_fs_stat("/family/SYNTH_CONTROL.txt",&st)==0,"shared content preserved");
    CHECK(ink_member_current()->role==INK_ROLE_CHILD,"reused member is child");
}
static void run(void *arg) {
    (void)arg; delay_ms(1800);
    CHECK(ink_cap_subscribe("confirm.result",result,NULL,INK_EXEC_DIRECT)==0,"listener");
    if(getenv("INK_REVIEW_INSPECT")) {inspect();ink_probe_count("test.finished");return;}
    CHECK(confirmed("member.add","{\"id\":\"oldadult\",\"role\":\"adult\"}"),"add adult");
    CHECK(ink_member_switch("oldadult")==0,"enter old adult");delay_ms(300);
    int registered=0;
    for(int i=0;ink_db_def_at(i);i++) if(ink_db_def_at(i)->scope==INK_DB_MEMBER) registered++;
    CHECK(registered==4,"all registered member collections enumerated: %d",registered);
    for(int i=0;i<5;i++) for(int k=0;k<2;k++) {
        ink_db_t *db=ink_db_open(def(i),k?"owner":"oldadult");
        ink_db_write_t w={.id=88001,.data=marker,.len=sizeof marker-1,.src=INK_SRC_TOUCH};
        CHECK(db&&ink_db_put(db,&w,NULL)==INK_DB_OK,"seed actual collection: %s",collections[i]);
        if(db) CHECK(ink_db_wait(db,ink_db_last_ver(db),4000000)==0,"collection durable");
        ink_db_close(db);
    }
    for(int i=0;i<2;i++) CHECK(ink_storage_write_atomic(rawpaths[i],marker,sizeof marker-1)==0,"seed raw file");
    CHECK(ink_fs_write_atomic("/me/nested/private.txt",marker,sizeof marker-1)==0,"seed personal file");
    CHECK(ink_fs_write_atomic("/app/synthetic/private.txt",marker,sizeof marker-1)==0,"seed app file");
    CHECK(ink_fs_write_atomic("/cache/sys/synthetic",marker,sizeof marker-1)==0,"seed cache");
    CHECK(ink_fs_write_atomic("/family/SYNTH_CONTROL.txt",marker,sizeof marker-1)==0,"seed shared control");
    CHECK(WAIT_FOR(!ink_storage_pending(),4000),"raw files durable");
    CHECK(ink_member_switch("owner")==0,"owner manages family");delay_ms(300);
    CHECK(confirmed("member.remove","{\"id\":\"oldadult\"}"),"remove old adult");
    CHECK(confirmed("member.add","{\"id\":\"oldadult\",\"role\":\"child\"}"),"reuse as child");
    inspect(); ink_db_flush_all();
    CHECK(WAIT_FOR(ink_db_idle()&&!ink_storage_pending(),4000),"durable checkpoint");
    ink_probe_count("test.finished");hal_power_reboot();
}
'''+entry

sources['wifi_undo']=common+r'''
#include "ink/hal_net.h"
static void add_ap(void *ctx) { (void)ctx; sim_wifi_ap_add("SYNTH_TARGET",-40,""); }
static uint32_t wid(const char *ssid) { uint32_t n=ink_crc32(0,ssid,strlen(ssid))&0x7fffffff; return n?n:1; }
static void run(void *arg) {
    (void)arg;delay_ms(1800);
    CHECK(ink_cap_subscribe("confirm.result",result,NULL,INK_EXEC_DIRECT)==0,"listener");
    const ink_db_def_t *d=ink_db_find_def("net.wifi");
    ink_db_t *db=ink_db_open(d,NULL);
    for(int i=0;i<8;i++) {
        hal_wifi_known_t k={0};snprintf(k.ssid,sizeof k.ssid,"SYNTH_OLD_%d",i);
        ink_db_write_t w={.id=wid(k.ssid),.data=&k,.len=sizeof k,.src=INK_SRC_TOUCH};
        CHECK(db&&ink_db_put(db,&w,NULL)==INK_DB_OK,"seed 8 saved networks");
    }
    CHECK(db&&ink_db_count(db)==8,"known network capacity reached"); ink_db_close(db);
    vtk_event_at(vtk_now()+SIM_MS(1),add_ap,NULL);delay_ms(20);
    CHECK(AI("wifi.connect","{\"ssid\":\"SYNTH_TARGET\"}")&&g_rc.status==INK_RC_DONE,"connect ninth network");
    CHECK(g_rc.revision==0&&g_rc.undo_ms>0,"unsaved connection advertises unversioned undo");
    char undo[160];snprintf(undo,sizeof undo,"{\"object\":\"%s\",\"revision\":%u}",g_rc.object,g_rc.revision);
    CHECK(confirmed("wifi.forget","{\"ssid\":\"SYNTH_OLD_0\"}"),"person frees one saved slot");
    CHECK(BY_HAND("wifi.connect","{\"ssid\":\"SYNTH_TARGET\"}")&&g_rc.status==INK_RC_DONE&&g_rc.revision>0,"person saves target network later");
    CHECK(AI("wifi.list",NULL)&&strstr(g_result,"SYNTH_TARGET"),"later credentials exist before undo");
    CHECK(AI("system.undo",undo),"old failed-save inverse answered");
    CHECK(g_rc.status==INK_RC_STALE||g_rc.status==INK_RC_FAILED||g_rc.status==INK_RC_PENDING_CONFIRMATION,"unversioned old inverse deleted later save: %s",ink_rc_name(g_rc.status));
    CHECK(AI("wifi.list",NULL)&&strstr(g_result,"SYNTH_TARGET"),"later human credentials survive old inverse");
    ink_probe_count("test.finished");
}
'''+entry

flags={}
for line in (build/'CMakeFiles/sim-media.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        k,v=line.split(' = ',1);flags[k]=shlex.split(v)
base=shlex.split((build/'CMakeFiles/sim-media.dir/link.txt').read_text())
failed=False
for name in os.environ.get('INK_REVIEW_CASES','reader_late inventory').split():
    src=work/(name+'.c');src.write_text(sources[name]);obj=work/(name+'.o');binary=work/name
    subprocess.run([base[0],*flags['C_DEFINES'],*flags['C_INCLUDES'],'-I'+str(root),*flags['C_FLAGS'],'-Wno-unused-function','-c',str(src),'-o',str(obj)],cwd=build,check=True)
    link=base.copy();link[link.index('-o')+1]=str(binary)
    for i,w in enumerate(link):
        if '/tests/integration/' in w and w.endswith('.o'):link[i]=str(obj);break
    subprocess.run(link,cwd=build,check=True)
    variants=[('control',{'INK_REVIEW_CONTROL':'1'}),('reused',{})] if name=='reader_late' else [('prepare',{}),('inspect',{'INK_REVIEW_INSPECT':'1'})] if name=='inventory' else [('run',{})]
    state=work/(name+'-state');(state/'sd').mkdir(parents=True)
    for label,env in variants:
        out=work/(name+'-'+label)
        cmd=[str(binary),'--profile',str(root/'simulator/profiles/metalio-plus.json'),'--assets',str(build/'assets'),'--content',str(build/'test-content'),'--out',str(out),'--end','35s','--quiet','--no-random-outages']
        if name=='inventory':cmd+=['--flash-image',str(state/'user.img'),'--sd-dir',str(state/'sd')]
        r=subprocess.run(cmd,cwd=root,capture_output=True,text=True,timeout=180,env={**os.environ,**env,'UBSAN_OPTIONS':'halt_on_error=1'})
        (work/(name+'-'+label+'.stderr.log')).write_text(r.stderr)
        data=json.loads((out/'metrics.json').read_text());m=data['metrics']
        ok=r.returncode==0 and m.get('test.finished')==1 and not m.get('test.fail',0)
        print(name,label,'PASS' if ok else 'FAIL','exit',r.returncode,'checks',m.get('test.pass',0),'failures',m.get('test.fail',0),flush=True)
        for w in data.get('warnings',[]):
            if 'CHECK FAILED' in str(w):print(w,flush=True)
        failed|=not ok
print('Evidence directory:',work.relative_to(root))
raise SystemExit(1 if failed else 0)
```

SEVERE=0 HIGH=2
