# 家庭中枢第 1 步复查（第 2 轮）

复查日期：2026-11-05。基线：`fdff779f2f2746ab10194e32a00b7dc343365e2c`。被审查版本：`d400d6b`。

**结论：第 1 轮的 R1、R2 两项高问题已验证修复；本轮发现 2 项中问题，严重 0、高 0。** 本机既有验收测试全部通过，但新增 CI 的构建并发和模拟器桥的连接资源回收仍需处理，不给无保留的整体验收结论。

本轮仅覆盖本报告，不改产品代码、不提交。构建均不超过 `-j4`，CTest 不超过 `-j3`；构建和全量测试串行，没有启动带窗口的模拟器。既有 `gui-smoke` 脚本强制 `SDL_VIDEODRIVER=dummy`。测试更新了忽略的生成产物；补充实验只使用临时文件、合成数据和独立进程。没有触碰设备、真实家庭资料、付费接口、凭据或远端写入。环境使用用户指定的 `INK_DEPS`；本任务没有使用 `INK_DESIGN` 中的设计稿。

## 问题

编号接续第 1 轮；R1、R2 的关闭证据见下一节。

### R3｜中｜新增 CI 构建命令仍未限制并发

- 位置：`.github/workflows/sim.yml:84`。
- 复现命令：`sed -n '81,85p' .github/workflows/sim.yml`，对照 `docs/tasks/codex-rules.md` 第 5 节。
- 【核查】新加的 `hub` job 使用 `cmake --build build -j`；总规则已明确最大 `-j4`、禁止裸 `-j`。本机 README 已修正，CI 新任务漏改。此检查只读文件，没有执行该命令。
- 影响：中枢 CI 构建没有显式并发上限，不满足更新后的资源约束。这里不把未运行的 Linux CI 描述为已经发生了资源耗尽。
- 修法：仅将新 `hub` job 的构建行改为 `cmake --build build -j4`；保留 H13 要求不改的两个既有 job。

### R4｜中｜关闭桥接连接后，唤醒 socket 和连接对象一直保留到模拟器退出

- 位置：`simulator/environment/hub_bridge.c:280`、`:286`、`:332`、`:334`。
- 复现命令：下方 P1，编译和运行均退出 0，表示缺陷复现成功。
- 【实测】每次打开桥后关闭，并等待网络线程退出；只把实验子进程的描述符软上限设为 64。30 次关闭后仍保留 **60 个**唤醒 socket，下一次 `socket()` 返回 `EMFILE`。调用 `sim_hub_bridge_shutdown()` 后，创建 socket 才重新成功。
- 【核查】每次 open 都把对象加入 `g_bridges`；close 只设置 stop 并唤醒。`wake[0/1]`、剩余队列和连接对象仅在整个模拟器退出时释放，退出线程也在此时才 join。实验提前 join 已退出线程仍能复现，排除了“只是还没关闭完”。
- 影响：启用桥的同一模拟器进程经历多次连接／重连后，宿主资源持续累积，达到进程限制后不能再连接，只能退出重开。当前五次 E2E 每次都新开模拟器进程，覆盖不到这一生命周期。问题限定于开发桥；默认关闭的路径不受影响。64 是缩短实验所设的上限，不是宣称本机默认第 30 次必坏。
- 修法：在宿主侧回收已退出连接，确保线程退出且迟到回调有安全引用／代次保护后，关闭唤醒 socket、释放队列和对象；不能在模拟任务里阻塞 join。补同一进程反复 open/close 后描述符数量回落的测试，包含建连失败路径。

## 第 1 轮问题复验

| 原问题 | 本轮结论 | 独立证据 |
| --- | --- | --- |
| R1：异步交付缺少完整的权限／代次保护 | **已修复** | 【核查】Registry 执行前后复核 Scope，直接调用异步分派；`send`、`request`、语音和听写出队复核权限／连接／代次，取消使已排队消息失效并清理 Future。【实测】T1/T2/T3 的相关用例通过；T5 将相关模块还原为 `fdf14b8` 的代码，只在内存替换，3 项失败并出现 1 个 Future 清理错误，明确捕获旧 Scope 的回执和 epoch=0 的翻页命令。 |
| R2：运行中取消导致唯一调度循环退出 | **已修复** | 【核查】取消用条件更新；完成和外部执行的事务处理已取消状态，不发布结果；撤权清理不终止调度。【实测】运行中取消后第二个任务完成、取消任务不重放等回归均通过；T5 还原旧 Queue 后 3 项失败，出现 `invalid job transition` 和后续任务等待超时。 |
| R1 收尾补丁 | **已修复** | 【实测】T5 单独恢复 `fba2adf` 的旧 `run_turn`，末尾回执／错误消息在撤权后被丢弃的 2 项测试均以 `ConnectionError` 失败；当前版本这 2 项通过。 |

以上反向对照均在独立 Python 进程完成，测试代码没有改动，生产源码没有撤回。不是只采信开发报告中的通过数。

## 逐项验收

“实测”指本轮实际执行，“核查”指本轮代码或文件检查；T1～T6、P1 见后文。

| 编号 | 本轮结论 | 证据与命令 |
| --- | --- | --- |
| H1 | 通过 | 【实测】T3 使用全新临时 venv 和下载缓存，按 frozen 锁文件重新安装 18 个包；Python 3.11.15、uv 0.9.5；CLI 就绪、healthz/setup、114 项 pytest 通过。【核查】目录、中文 README、锁版本／哈希存在。复用了固定 Python/uv 可执行程序，没有复用 Python 依赖环境或下载缓存。 |
| H2 | 本步合成协议通过 | 【实测】T1～T3 覆盖开发握手、目录分页、双向工具回执、语音／听写控制流和错误返回；撤权／切成员后的旧结果不出队。第 2a 步的完整语音行为与第 3 步身份接入不算本步已验证。 |
| H3 | 既有契约用例通过 | 【实测】**114/114、0 跳过**，含新增 30 项；词法预算、乱序、超限、错误、断线重连、请求超时及新增交付竞争均通过。T5 证明抽查的新用例能检测对应修复。 |
| H4 | 通过 | 【实测】T1/T2 及 T3 的生产 C 解析器接受 **161 条**测试生成消息，最高 **110** 词法单元；T1/T2 另验证 128 接受、129 拒绝。每次 E2E 另验 19 条、最高 33。 |
| H5 | 通过 | 【实测】生产初始化导出 **131 条工具、104 个 actions、overflow=true**；空 actions 回查、完整定义和 caps 分页用例通过。【核查】`export_caps.c` 调用当前产品初始化及生产清单函数，没有手抄名称。 |
| H6 | 本步合成隔离框架通过 | 【实测】两家三成员、权限换代、动作范围各字段的反例及异步交付反例通过。原 R1 已关闭；真实成人解锁、账号及设备端 `_meta` 校验仍未接入，不把合成权限框架外推为真实身份安全验证。 |
| H7 | 本步任务框架通过 | 【实测】持久队列重开、幂等键、unknown 不重放、运行中取消／撤权后继续调度、worker 文件探针通过；原 R2 已关闭。【核查】worker 容器定义存在；容器本身未运行。 |
| H8 | 本机脚本部分通过；Docker 未验 | 【实测】预检、安装预演、mock Docker、设置码／向导、worker 隔离配置测试通过；`sh -n` 退出 0。Docker 不可用，没有真实 Compose config、镜像构建或容器运行结果；mock 不作容器证据。 |
| H9 | 默认行为和单轮桥通过；资源回收有缺陷 | 【实测】T1/T2 普通和 ASan/UBSan 各 **91/91、0 跳过**，含原 86 项及两项确定性；T4 三场景指标和图片与基线逐字节相同。【核查】专用网络线程、外部注入、默认关闭成立；P1 另证实 R4，不能据五次独立进程 E2E 宣称长时间重连无泄漏。 |
| H10 | 通过 | 【实测】T6 串行 **5/5**，合计 **87.51 秒**；每次 1× 实时、一轮语音、一次翻页、播放 ≥300 ms，无坏消息或桥上行丢包。测试以就绪事件启动；缺环境时显式退出 77，本轮无跳过。 |
| H11 | 通过测量核查 | 【实测，开发机】T6 主进程峰值 RSS **44.56～52.48 MiB**，循环 p95 **1.190～1.218 ms**，最大 **41.427 ms**，完整五次见下表。【核查】设计 §4.3 已有正确标注的历史实测、方法和边界；本轮按只写 REVIEW 的限制没有改它。 |
| H12 | 通过文件核查 | `contracts-step1.md` 覆盖 §10 要冻结的分页/result、可用集合、确认身份、交付、预算、动作范围、受限上下文、自动动作标识及查询；来源章节、待定项／默认值、设备改动单列。 |
| H13 | 配置部分通过；有 R3；Linux 未验 | 【实测】PyYAML 可解析，与基线比较两个既有 job 完全相同；新增 hub job 包含契约、五次 E2E、Compose config，未使用密钥。【核查】新 job 留有裸 `-j`，见 R3。没有触发或取得 Linux CI 运行结果。 |
| H14 | 本机结束检查通过 | 【实测】全量和内存检查已跑；基线 diff 为 **102 文件、+7622/-7**；未改 system/apps/drivers 和已有测试。指定隐私模式 0 命中；差异空白检查退出 0；92 个新增文件均 ≤5 MiB；README、契约和执行报告的 14 个本地链接无缺失。开发提交已存在，本轮按复查规则不提交。 |

## 架构、范围和证据边界

- 【核查】网关、能力描述与策略、Scope、语音编排、厂商接口、持久队列各有独立边界；能力描述生成目录和参数定义，回执结构化。权限检查已贯穿直接能力调用和发送队列，修复了前轮总线交付路径不一致的问题。后续阶段空包没有登记成可用能力；确认操作没有开放给 AI。
- 【核查】设备实现继续通过既有能力和 HAL；没有改 system/apps/drivers，没有新增设备常驻任务或绕过 LVGL 线程边界。桥的 socket 工作在宿主线程，模拟任务经宿主包装排队、经 `vtk_inject_external` 接收结果；R4 是这个新模块的生命周期缺口。
- 【核查】没有改原 84 项 pytest、原 86 项 CTest 的断言、预算或跳过规则，没有加 sleep 掩盖时序。桥的 poll、语音实时节奏计时器及任务恢复计时器是实现机制，不能与测试等待混为一谈。
- `docs/tasks/codex-rules.md`、`docs/tasks/ui-shell.md` 的变化来自单独的资源规则提交 `fdf14b8`；没有把它误算为中枢修复夹带。`contracts-step1.md` 和报告由任务明确要求，设计文档的改动仅为允许的 §4.3 测量补充。
- 本任务是骨架和契约任务，不是设备界面还原任务；没有 `out/ui-compare/` 的 10 屏对照可验，不把它记成完成。向导核查了本步示例功能和 HTTP 行为，没有做设计稿视觉验收。
- Docker、容器峰值、Linux CI、真实厂商／音频、真实账号和实机均未获得本轮运行证据。执行报告如实披露的后续阶段事项不计为遗漏；本轮没有新增产品决定。

## 独立验证命令

均从仓库根目录执行。以下构建复用已有缓存：已核对两套均 `INK_WERROR=ON`，ASan 套另有 `INK_SANITIZE=ON`；字体、Python、LVGL 指向用户给定的只读依赖目录，`INK_HUB_PYTHON` 指向本仓库锁定环境。本轮未重新配置这两套构建。

### T1、T2：普通全量和内存检查

```sh
cmake --build build -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
```

四条均退出 0，逐条串行。普通 **91/91，35.81 秒**；ASan/UBSan **91/91，67.13 秒**。两套均无跳过；日志内分别是 `114 passed in 3.20s`、`114 passed in 3.75s`，生产解析器边界正反例通过。

### T3：全新依赖环境、CLI、pytest 和生产解析器

```sh
PYTHONDONTWRITEBYTECODE=1 hub/.venv/bin/python - <<'PY'
import os, pathlib, subprocess, tempfile
root = pathlib.Path.cwd()
subprocess.run(['work/hub-tools/bin/uv', '--version'], check=True)
subprocess.run(['hub/.venv/bin/python', '--version'], check=True)
with tempfile.TemporaryDirectory(prefix='hub-review-r2-fresh-') as tmp:
    env = {**os.environ, 'UV_PROJECT_ENVIRONMENT': tmp+'/venv',
           'UV_CACHE_DIR': tmp+'/cache', 'PYTHONDONTWRITEBYTECODE': '1'}
    subprocess.run(['work/hub-tools/bin/uv', 'sync', '--project', 'hub',
                    '--frozen', '--python', str(root/'hub/.venv/bin/python')],
                   env=env, check=True)
    py = tmp+'/venv/bin/python'
    subprocess.run([py, 'tests/hub/check_fresh.py', '--out', tmp+'/cli'], env=env, check=True)
    subprocess.run([py, 'tests/hub/run_contract.py', '--caps',
                    'build/test-out/hub/caps.json', '--samples', tmp+'/messages.jsonl'],
                   env=env, check=True)
    subprocess.run(['build/hub-json-check', tmp+'/messages.jsonl'], check=True)
PY
```

退出 0。18 个包重新安装；`114 passed in 5.16s`；161 条消息，最高 110 tokens；CLI 就绪、healthz、setup 通过。

### T4：桥关闭时和基线的对照

```sh
cmake --build work/baseline-build --target ink-sim -j4
PYTHONDONTWRITEBYTECODE=1 hub/.venv/bin/python tests/hub/compare_default.py \
  --before work/baseline-build/ink-sim --after build/ink-sim \
  --assets build/assets --content build/test-content --out work/review-r2-default-comparison
```

均退出 0。`home_basic`、`reader_basic`、`ai_ask` 的 `metrics.json` 和 `final.png` 均逐字节相同。复用了此前的基线源码和构建目录；本轮先核对其已有 487 个文件的 Git blob 哈希与 `fdff779` 一致，再执行上述构建。该目录是构建用子集，不宣称是本轮重新检出的完整仓库。

### T5：反向对照

下列命令只在各自子进程中加载旧代码，不写源码。当前版本的相应用例已由 T1～T3 验证通过。

```sh
PYTHONDONTWRITEBYTECODE=1 hub/.venv/bin/python - <<'PY'
import subprocess, sys
code = '''
import importlib, subprocess, sys, pytest
mode=sys.argv[1]
if mode=='R1':
    modules=['ink_hub.agent.tools','ink_hub.bus.registry','ink_hub.voice.flow','ink_hub.gateway.server']
    tests=['hub/tests/isolation/test_delivery.py::'+n for n in [
        'test_registry_rechecks_permissions_after_handler',
        'test_direct_call_drops_result_after_revocation',
        'test_member_change_discards_queued_turn_command']]
    revision='fdf14b8'
elif mode=='R2':
    modules=['ink_hub.jobs.queue']
    tests=['hub/tests/recovery/test_cancel_races.py::'+n for n in [
        'test_cancel_running_job_keeps_scheduler_alive[False]',
        'test_cancel_and_finish_respect_transaction_order[cancel]',
        'test_cancel_before_external_dispatch_prevents_handler']]
    revision='fdf14b8'
else:
    modules=['ink_hub.voice.flow']
    tests=['hub/tests/isolation/test_delivery.py','-k','revoked_turn_tail']
    revision='fba2adf'
for name in modules:
    module=importlib.import_module(name)
    path='hub/'+name.replace('.','/')+'.py'
    source=subprocess.check_output(['git','show',revision+':'+path],text=True)
    exec(compile(source, '<reverse:'+revision+':'+path+'>', 'exec'), module.__dict__)
if mode=='tail':
    import ink_hub.gateway.server as server
    server.run_turn=importlib.import_module('ink_hub.voice.flow').run_turn
sys.exit(pytest.main(['-q','--tb=short','-p','no:cacheprovider',*tests]))
'''
for mode in ['R1','R2','tail']:
    result=subprocess.run([sys.executable,'-c',code,mode],text=True,capture_output=True,timeout=30)
    print(mode, 'exit=',result.returncode)
    for line in (result.stdout+result.stderr).splitlines():
        if line.startswith(('E ', 'FAILED ', 'ERROR ')) or ' failed' in line or 'Future exception' in line:
            print(line)
    assert result.returncode==1
PY
```

外层退出 0；三个 pytest 子进程均按预期退出 1。R1：3 failed、1 error；R2：3 failed；tail：2 failed、19 deselected。失败内容见“第 1 轮问题复验”，不是依赖缺失或调用签名不匹配。

### T6：连续五次端到端及测量

```sh
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^hub-simulator-e2e$' \
  --repeat until-fail:5 --output-on-failure -V -j1
```

退出 0，5/5，87.51 秒。每次 `turns=1`、`tool_calls=1`、虚拟时间 17.05 秒；生产解析器每次接受 19 条消息，最高 33 tokens。下面为本轮实际控制台输出，RSS 来自主进程，不含模拟器子进程或容器。

| 次数 | 峰值 RSS（bytes） | 采样数 | p95（ms） | 最大（ms） | 墙钟（s） |
| --- | --- | --- | --- | --- | --- |
| 1 | 54951936 | 1556 | 1.194792 | 41.426959 | 17.136968 |
| 2 | 54034432 | 1556 | 1.206292 | 7.346583 | 17.085162 |
| 3 | 47529984 | 1557 | 1.189708 | 6.497667 | 17.071608 |
| 4 | 46727168 | 1559 | 1.196917 | 6.184791 | 17.119920 |
| 5 | 55033856 | 1551 | 1.217666 | 16.788791 | 17.065038 |

这些数字仅说明开发机合成负载；没有据此改性能预算或宣称真实语音达标。

## P1：桥连接关闭后的描述符累积

直接包含当前 `hub_bridge.c`，只替换宿主分配和内核调度适配器；连接目标是本实验绑定但没有监听的本机临时端口。主动 join 是为确定线程已经结束，并非把 join 加到生产模拟任务中。文件描述符上限只影响实验子进程；无需 sleep、不启动模拟器窗口。临时二进制退出后删除。

```sh
PYTHONDONTWRITEBYTECODE=1 hub/.venv/bin/python - <<'PY'
import pathlib, subprocess, tempfile
source=r'''
#include <assert.h>
#include <sys/resource.h>
#include "simulator/environment/hub_bridge.c"
void *sim_host_malloc(size_t n) { return malloc(n); }
void *sim_host_calloc(size_t n, size_t s) { return calloc(n, s); }
void sim_host_free(void *p) { free(p); }
void vtk_host_call(void (*fn)(void *), void *arg) { fn(arg); }
void vtk_inject_external(vtk_event_fn fn, void *arg) { fn(arg); }
void sim_net_bridge_event(void *o, uint64_t g, int e, const void *d, size_t n) {}
int main(void) {
    struct rlimit lim;
    assert(getrlimit(RLIMIT_NOFILE, &lim)==0);
    lim.rlim_cur=64;
    assert(setrlimit(RLIMIT_NOFILE, &lim)==0);
    int reserved=socket(AF_INET, SOCK_STREAM, 0);
    assert(reserved>=0);
    struct sockaddr_in addr={.sin_family=AF_INET,.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};
    assert(bind(reserved,(struct sockaddr *)&addr,sizeof addr)==0);
    socklen_t size=sizeof addr;
    assert(getsockname(reserved,(struct sockaddr *)&addr,&size)==0);
    char url[96];
    snprintf(url,sizeof url,"ws://127.0.0.1:%u/v1/device",ntohs(addr.sin_port));
    assert(sim_hub_bridge_configure(url)==0);
    unsigned opened=0;
    for (; opened<64; opened++) {
        sim_hub_bridge_t *b=sim_hub_bridge_open(NULL,opened);
        if (!b) break;
        sim_hub_bridge_close(b);
        assert(pthread_join(b->thread,NULL)==0);
        b->started=false;
    }
    unsigned retained=0;
    for (sim_hub_bridge_t *b=g_bridges;b;b=b->next) {
        retained+=b->wake[0]>=0 && fcntl(b->wake[0],F_GETFD)>=0;
        retained+=b->wake[1]>=0 && fcntl(b->wake[1],F_GETFD)>=0;
    }
    int probe=socket(AF_INET,SOCK_STREAM,0), err=errno;
    printf("closed_connections=%u retained_wake_fds=%u next_socket=%d errno=%d\n",opened,retained,probe,err);
    assert(opened==30 && retained==60 && probe<0 && err==EMFILE);
    sim_hub_bridge_shutdown();
    probe=socket(AF_INET,SOCK_STREAM,0);
    assert(probe>=0);
    close(probe); close(reserved);
}
'''
with tempfile.TemporaryDirectory(prefix='hub-review-r2-fd-') as tmp:
    target=str(pathlib.Path(tmp)/'bridge-fd')
    includes=['.','simulator','platform/include','simulator/kernel/vtk','simulator/memory','simulator/runtime']
    command=['cc','-std=gnu11','-pthread','-Wno-unused-parameter',*[f'-I{p}' for p in includes],'-x','c','-','-o',target]
    result=subprocess.run(command,input=source,text=True,capture_output=True)
    print('compile_exit=',result.returncode)
    if result.returncode: print(result.stderr)
    assert result.returncode==0
    result=subprocess.run([target],text=True,capture_output=True,timeout=10)
    print('probe_exit=',result.returncode,result.stdout,result.stderr)
    assert result.returncode==0
PY
```

【实测】`compile_exit=0`、`probe_exit=0`，输出 `closed_connections=30 retained_wake_fds=60 next_socket=-1 errno=24`。此实验验证连接资源回收，不替代真实内核的外部注入和代次测试；后者由 T1/T2 验证。

SEVERE=0 HIGH=0
