# capability-table 独立复查

复查日期：2026-11-06。基线：`1c43c28a`；被查版本：`743df5027964a7533954916ad1703c0ba1f3b8e9`。旧 `REVIEW.md` 为空，无上一轮问题可追踪。

结论：**暂不通过，严重 0、高 1**。普通和 ASan/UBSan 全量测试均通过，但能力总表的成员范围、确认条件存在实际反例，新增检查没有发现。未发现本次改动引入产品运行时的数据丢失、成员隔离失效或代替人工确认。

本轮仅覆盖本报告；没有改产品代码、原有测试或预算，没有提交。核查依据为 `review-prompt.md`、`backlog.md` 开头及 capability-table 一节、`codex-rules.md`。

## 逐条验收

| 项目 | 结论 | 本轮独立证据 |
| --- | --- | --- |
| C1 从代码生成所需列 | 部分通过 | 【实测】198 个定义：115 个调用、27 个事件、14 个设置、42 个状态。所需列和源码链接齐全；成员/确认文字的正确性见 R1。关联面明确标为服务上下文，未冒充逐调用的数据流。 |
| C2 CTest 校验一致性、描述与风险 | 结构检查通过，策略语义不通过 | 【实测】`tools/capabilities.py --check`、10 项 Python 回归、4 项新增 CTest 通过；删除描述、effect、settings.get/set 的内存负对照被拒绝。运行时 142 项元数据及 3976 个 `confirm_for` 结果相符；这些结果不足以证明生成文字符合完整调用行为，见 R1。 |
| C3 每个设置都有能力 | 能力覆盖通过 | 【实现／实测】14 项均经通用 `settings.get/set` 按 id 查找；运行时目录一致。儿童权限文字及其校验仍有 R1 的缺口。 |
| C4 用户操作缺口、小改或列出遗留 | 通过 | 【实现】复核报告的返回、锁屏/唤醒、AI 会话、键盘、文本选区、电源菜单、远程点击和占位主页入口；也核对了已有能力但按钮绕行的情况。报告明确未补产品入口及原因，未把这些遗留项写成已完成。 |
| G1 全量及内存检查、预算 | 通过 | 【实测】普通 100/100，44.07 秒；ASan/UBSan 100/100，76.50 秒；均退出 0、无跳过。预算、断言、时限和原测试均未修改。 |
| G2 原子能力、按钮/AI 同路、新页快照 | 本次无新增入口/页面；基线缺口已说明 | 【实现】没有产品代码变更；`integration-ai-surface` 通过。不能据此认定既有按钮已全部改走总线，报告也未作此宣称。 |
| G3 依赖状态 | 通过 | 【实现】仅改 `ui-dependencies.md` 对应状态单元格，仍保留“部分”和原判断。 |
| G4 页面与视觉验收 | 不适用 | 无界面改动，因此未执行十屏对照；全量中的 `gui-smoke` 强制 SDL dummy，无窗口。 |
| G5 范围及总规则 §1、§4 | 通过，策略文档问题另列 | 【实现／实测】8 个变更文件均服务于本任务；无硬件、依赖、分层或产品路径改动。新增 C 导出在 `vtk_host_call` 内输出并检查文件操作返回值；未新增 `write/read/fgets`。未碰设备、真实身份、付费接口或远端写入。 |
| 差异、隐私和链接 | 通过 | 【实测】`git diff --check 1c43c28a..HEAD` 退出 0；规定隐私正则 0 匹配；208 个本地文件链接全部存在，无超过 5 MB 的变更文件。 |

## R1｜高｜成员范围和确认条件的校验止于元数据，实际错误的总表仍能通过全部检查

位置：`tools/capabilities.py:223`（成员解释）、`:299`（设置成员文字）、`:193`（确认解释）、`:334`（设置元数据比较）；`tests/capability_table/export.c:53`、`:66`（导出范围）；对应产物 `docs/system/capabilities.md:105`、`:114`、`:370`。`REPORT.md:10` 的“表与代码一致”不能按现有证据整体判为通过。

复现命令：先执行下文 V1，再执行 P1、P2。P1 编译一个临时测试入口，链接现有产品库，使用真实软总线及既有人工确认测试辅助函数；不改任何产品源码。P2 只改 Python 内存副本。

结果：

1. 【实测】默认孩子白名单没有 `settings.set`，AI 仍可将 `audio.volume` 改为 35，回执 `done`；`notify.list` 也回 `done`。总表却将这类操作写成“儿童须白名单”，设置行写成“成人、儿童白名单”。【实现】`system/policy/members.c:283` 是 `child_ok` 直接放行，白名单用于**额外**放行；并非两者同时满足。104 个可调用能力走了生成器的这一错误文字分支。
2. 【实测】成人先在设备确认开启、再确认关闭 `ai.read_clipboard`；随后 AI 调 `system.undo`，回执为 `pending_confirmation`，设置仍为关闭。总表写的却是“无需”。【实现】`system/policy/cap.c:1183` 保留设置撤销者的来源，实际还要过 `system/policy/settings.c:209`。导出的 `confirm_for(system.undo)` 对此返回 none，但它只是外层闸门的结果。
3. 【实测】仅把内存中 `audio.child_max.child_may_change` 从 false 改为 true，生成设置行即从“仅成人”变为“成人、儿童白名单”；`check_runtime` 仍打印 `Runtime matches: 142 capabilities, 14 settings`。导出没有该字段，比较也没有该字段；已有名为 `member` 的负输入只翻转能力的 `child_ok`，测不到设置项的儿童权限。

影响与级别：这不是新增产品越权，而是本任务核心交付的权限/确认语义不可信。全部新增测试在上述错误存在时仍通过，属于复查规则“测试实际测不到它声称的东西”；将相关症状合并为一项高问题，不重复计数。

修法：按实际策略修正 `child_ok`、白名单和系统级能力的文字；将 `system.undo` 的确认条件表达为按被撤销操作/设置判断。导出并比较 `child_may_change`，补真实调用结果与生成语义的对照，至少覆盖无匹配白名单的儿童、儿童可写/不可写设置、普通撤销与需确认的权限撤销。负对照应证明错误文字和错误设置权限都会失败。不要通过改产品权限或降低测试要求来迁就总表。

## 实测命令与复现

V1：使用已有、已核对参数的 `build` / `build-asan`，均启用 `INK_WERROR=ON`，后者另启用 `INK_SANITIZE=ON`；依赖使用用户指定的 `INK_DEPS`。以下构建/全量测试逐轮串行执行：

```sh
cmake --build build -j4
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
python3 tools/capabilities.py --check
python3 tests/capability_table/test_catalogue.py
```

两轮 CTest 记录分别在 `build/Testing/Temporary/LastTest.log`、`build-asan/Testing/Temporary/LastTest.log`。未执行 Linux gcc 或实机验证；本机 clang 构建通过不等于 Linux 验证通过。

P1：在仓库根目录、V1 完成后运行。只生成临时对象、可执行文件及模拟输出，退出后自动清理；单进程编译，不启动 GUI。

<!-- P1_BEGIN -->
```sh
python3 - <<'PY'
from pathlib import Path
import shlex, subprocess, tempfile
root = Path.cwd()
flags = (root/'build/CMakeFiles/sim-capability-inventory.dir/flags.make').read_text()
def flags_for(name):
    return shlex.split(next(s.split(' = ', 1)[1] for s in flags.splitlines() if s.startswith(name+' = ')))
source = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ink_main product_main
#include "system/boot/main.c"
#undef ink_main
#include "tests/integration/whole_product.h"
static struct { int kid, listed, allows, setting, volume, notify, adult, on, off, undo, after; } r;
static void print_result(void *unused) {
    (void)unused;
    if (printf("REVIEW kid=%d listed=%d allows=%d setting=%d volume=%d notify=%d adult=%d on=%d off=%d undo=%d after=%d\n",
        r.kid,r.listed,r.allows,r.setting,r.volume,r.notify,r.adult,r.on,r.off,r.undo,r.after)<0) abort();
}
static void test_task(void *unused) {
    (void)unused;
    delay_ms(1500);
    if (!BY_HAND("system.switch_member", "{\"id\":\"kid\"}")) osal_panic("kid callback");
    r.kid=g_rc.status;
    const ink_member_t *m=ink_member_current();
    for (int i=0;i<8 && m->allow[i];i++)
        if (!strncmp("settings.set",m->allow[i],strlen(m->allow[i]))) r.listed=1;
    r.allows=ink_member_allows("settings.set",ink_cap_find("settings.set")->child_ok);
    if (!AI("settings.set", "{\"id\":\"audio.volume\",\"value\":35}")) osal_panic("setting callback");
    r.setting=g_rc.status; r.volume=(int)ink_setting_get("audio.volume");
    if (!AI("notify.list", NULL)) osal_panic("notify callback");
    r.notify=g_rc.status;
    if (!BY_HAND("system.switch_member", "{\"id\":\"owner\"}")) osal_panic("adult callback");
    r.adult=g_rc.status;
    r.on=setting_by_hand("ai.read_clipboard",1);
    r.off=setting_by_hand("ai.read_clipboard",0);
    if (!AI("system.undo", "{\"object\":\"setting:ai.read_clipboard\"}")) osal_panic("undo callback");
    r.undo=g_rc.status; r.after=(int)ink_setting_get("ai.read_clipboard");
    vtk_host_call(print_result,NULL);
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg={.name="review",.fn=test_task,.stack_bytes=16384,
        .priority=OSAL_PRIO_NORMAL,.core=-1,.flags=OSAL_TASK_STACK_EXTERNAL};
    if(osal_task_create(&cfg,NULL)!=OSAL_OK) osal_panic("review task");
}
'''
with tempfile.TemporaryDirectory(prefix='capability-review-') as directory:
    tmp=Path(directory); obj=tmp/'probe.o'; exe=tmp/'probe'
    compile_cmd=['cc',*flags_for('C_DEFINES'),*flags_for('C_INCLUDES'),*flags_for('C_FLAGS'),
                 '-I',str(root),'-x','c','-c','-','-o',str(obj)]
    subprocess.run(compile_cmd,input=source,text=True,check=True)
    link=shlex.split((root/'build/CMakeFiles/sim-capability-inventory.dir/link.txt').read_text())
    link=[str(obj) if s.endswith('/tests/capability_table/export.c.o') else
          str(exe) if s=='sim-capability-inventory' else s for s in link]
    subprocess.run(link,cwd=root/'build',check=True)
    run=subprocess.run([str(exe),'--profile','simulator/profiles/metalio-plus.json',
        '--assets','build/assets','--content','build/test-content','--out',str(tmp/'out'),
        '--end','15s','--quiet','--no-random-outages'],capture_output=True,text=True,check=True)
    result=next(s for s in run.stdout.splitlines() if s.startswith('REVIEW '))
    print(result)
    assert result == 'REVIEW kid=0 listed=0 allows=1 setting=0 volume=35 notify=0 adult=0 on=1 off=1 undo=3 after=0'
PY
```
<!-- P1_END -->

结果：断言通过、命令退出 0。`0=done`，`3=pending_confirmation`；`listed=0` 表示白名单不匹配，`allows=1` 表示实际成员策略仍放行；`on/off=1` 表示两次人工确认成功，`after=0` 表示 AI 撤销仍在等人确认。

P2：错误的儿童设置权限仍通过运行时一致性检查。

<!-- P2_BEGIN -->
```sh
python3 - <<'PY'
from pathlib import Path
import sys
sys.path.insert(0, 'tools')
import capabilities as c
entries, sources = c.load(Path('.'))
entry = next(e for e in entries if e.kind == 'setting' and e.id == 'audio.child_max')
assert not entry.flag('child_may_change')
entry.fields['child_may_change'] = ['true']
c.validate(entries)
c.check_runtime(entries, Path('build/test-out/capability-table/capability-inventory.json'))
print(next(s for s in c.render(entries, sources).splitlines() if s.startswith('| `audio.child_max`')))
PY
```
<!-- P2_END -->

实际结果：命令退出 0，输出 `Runtime matches: 142 capabilities, 14 settings`，设置行变为“成人、儿童白名单”。修正检查后应拒绝这一内存负对照。

SEVERE=0 HIGH=1
