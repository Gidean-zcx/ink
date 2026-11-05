# 家庭中枢第 1 步复查（第 1 轮）

复查日期：2026-11-05。基线：`fdff779f2f2746ab10194e32a00b7dc343365e2c`。被审查版本：`60ea989`。

**结论：本轮不通过，2 个高问题需要修复。** 现有测试全部通过，但补充的交错时序复现说明 H6、H7 仍有缺口。没有发现应定为严重的问题，也没有把后续阶段的真实厂商、配对、记忆、内容等算成本步遗漏。

仅修改本报告，不修改产品代码、不提交。测试运行更新了忽略的构建与测试产物；补充复现用临时目录和合成数据，反向对照只在独立 Python 进程内替换函数。未触碰设备、真实家庭资料、付费接口或远端仓库。`INK_DEPS`、`INK_DESIGN` 按本轮提供的环境使用；本任务未使用设计稿。

## 问题

### R1｜高｜异步交付的权限／代次保护没有覆盖全部调用路径

- 位置：`hub/ink_hub/gateway/server.py:92`、`:187`，`hub/ink_hub/bus/registry.py:46`，`hub/ink_hub/voice/flow.py:50`。
- 【实测】在真实本机 WebSocket 上发 `tools/call llm.ask`，用假模型的 Event 暂停；删除发起成员后放行，仍收到成功回执及旧 Scope 的文字。Registry 只在进入时检查权限，回执发送没有 guard。
- 【实测】阻塞发送器，令 `ai.ask` 的设备 `tools/call` 入队，然后走真实 `ink/member` 处理分支；解除阻塞后，旧 epoch 的翻页命令仍发出。`request()` 不给队列项加 guard，取消等待任务不会撤销已排队消息。输出为 `current_epoch=1 sent_call_epochs=[0]`，还产生一个未取走的 `ConnectionError` future 警告。
- 影响：H6 的异步权限与代次隔离不成立；现有设备尚未校验 `_meta.epoch`，不能靠它补救。现有隔离测试只覆盖 `ai.delta` 路径，不能证明所有能力输出都受保护。
- 复现命令：下方 P1，退出 0 表示两个缺陷均被成功复现；没有修改生产源码。第一部分使用真实本机 WebSocket，第二部分只以可控发送端模拟背压。
- 修法：所有异步调用绑定发起 Scope、连接和代次，在执行与真正出队时复核；将 guard 贯穿 `request()`；取消后清除或失效化待发送项。直接调用应异步分派，以便接收循环及时处理切成员和关闭。

### R2｜高｜取消正在执行的任务会终止唯一调度循环

- 位置：`hub/ink_hub/jobs/queue.py:89`、`:124`，`hub/ink_hub/jobs/runner.py:36`。
- 【实测】用 Event 挂起第一个任务，在 `running` 时取消，再提交第二个任务并放行第一个。`finish()` 对已取消状态抛出 `ValueError('invalid job transition')`，异常穿透 `Runner.run()`；调度任务结束，第二个任务停在 `queued`。
- 影响：H7 的任务框架在正常取消交错下停止处理后续任务，需要重启服务恢复。现有测试仅在领取前取消，没有覆盖运行中取消。
- 复现命令：下方 P2，退出 0；实测 `runner_done=True exception=ValueError('invalid job transition') next_job_state=queued`。只替换合成 handler 的执行时长，实际 Queue 和 Runner 都是原代码。
- 修法：以事务状态检查处理取消与完成竞争，取消赢了就丢弃结果；单个任务的状态竞争不能结束调度循环。补运行中取消后下一任务仍完成的回归测试。

## 逐项验收

所有“实测”均为本轮自行运行；“核查”是代码或文件检查。命令编号见后文。

| 编号 | 本轮结论 | 证据与命令 |
| --- | --- | --- |
| H1 | 通过 | T3 全新临时环境按 frozen 锁文件安装；Python 3.11.15、uv 0.9.5；CLI 就绪、healthz、setup 均通过，84 项 pytest 通过。目录和中文 README 已核查。复用了已安装的固定 Python/uv 可执行程序，没有复用 Python 依赖环境或下载缓存。 |
| H2 | 部分通过 | T1/T3 验证现有握手、目录、双向工具回执和语音控制流；异步工具交付仍受 R1 影响。开发握手默认拒绝、仅本机、假厂商边界明确。 |
| H3 | 现有用例通过，覆盖不完整 | T3 为 **84/84、0 跳过**；覆盖词法预算、乱序、错误、重连与超时。R1/P1 的直接工具回执和发送背压未被覆盖，不能用总通过数证明交付隔离。 |
| H4 | 通过 | T3 中生产 C 解析器接受 **142 条**测试生成消息，最高 **110** 词法单元；T1 的 crosscheck 还验证 128 接受、129 拒绝。T2 每轮另验 19 条、最高 33。 |
| H5 | 通过 | T1 的生产初始化导出结果为 **131 条工具、104 个 actions、overflow=true**；T3 的空 actions 回查、定义完整性和 caps 分页测试通过。不是手抄工具名。 |
| H6 | **不通过** | 静态 Scope、动作范围反例通过；P1 复现撤权后返回结果及换代后发送旧命令，见 R1。未证明真实成人／儿童接入；当前仍是开发合成身份。 |
| H7 | **不通过** | 假厂商、持久队列重开、幂等键、unknown 不重放、worker 文件探针已有实现与通过用例；运行中取消可杀死调度器，见 R2/P2。容器本身未运行。 |
| H8 | 本机部分通过；Docker 未验 | T3 中预检、安装预演、mock Docker、设置码与向导测试通过；`sh -n hub/deploy/install.sh` 退出 0。Compose YAML 可解析。没有真实 Compose config、镜像构建或容器运行证据，报告对此如实披露。 |
| H9 | 通过本轮验证 | T1 普通和 ASan/UBSan 各 **91/91**，原 86 项和两项确定性均通过；桥默认关闭、专用网络线程及 `vtk_inject_external` 路径已核查。T4 三场景的 JSON/PNG 逐字节相同；该额外对照复用了开发阶段的基线二进制，本轮未重建基线。 |
| H10 | 通过 | T2 连续 **5/5**，总计 87.60 秒；每次一轮语音、一次设备翻页、播放至少 300 ms，无坏消息或桥上行丢包。代码以就绪事件启动，无新增 sleep；缺依赖或资产的路径显式退出 77。 |
| H11 | 通过 | T2 重新量到主进程峰值 RSS **44.70～52.83 MiB**，循环 p95 **2.170～2.187 ms**，最大 **10.939 ms**；§4.3 的原实测标注、方法与证据边界已核查，仅该节增加测量内容。未修改设计文档中的旧测量数字。 |
| H12 | 通过文件核查 | `contracts-step1.md` 覆盖 §10 所列分页/result、可用集合、持久确认身份、实时与持久交付、预算、动作范围、受限上下文、自动动作标识及查询；有章节来源、待定项与默认值、设备改动单列。R1 是实现未满足已写约定。 |
| H13 | 配置通过；Linux 未验 | PyYAML 可解析；用基线 YAML 与当前 YAML 比较，`build-test`、`macos` 两个既有 job 内容完全相同；仅加 `hub`，含 Compose config、五次 E2E，无密钥。未触发或取得 Linux CI 运行结果。 |
| H14 | 本机结束检查通过，不能据此整体验收 | T1 已重跑全量和内存检查；基线 diff 为 97 文件、+6779/-4；未改已有测试、预算、system/apps/drivers。指定隐私正则 0 命中，`git diff --check` 退出 0，新增文件无超过 5 MiB，报告/README/契约本地链接无缺失。开发提交已存在；本轮按复查规则不提交。 |

## 架构与范围核查

- 网关、语音编排、能力描述、策略、Scope、队列已有明确目录边界；未实现的包没有登记假能力。Scope 与动作范围的框架确有反例测试，不是仅列空类。
- 对已安装中枢能力，描述表生成目录与 Schema；但交付安全并未形成统一关口：语音通知有 guard，直接调用回执和设备 RPC 却绕过。R1 是“软总线 + 原子能力”审视中需要先收口的具体问题。不能沿用开发报告“输出经连接/会话/代次/权限复核”的无条件结论。
- 没有看到新增绕过 OSAL/HAL 的设备实现，桥的 socket 操作在宿主网络线程，模拟任务的宿主分配/排队走既有包装。没有修改原有测试断言、预算或加 sleep 掩盖时序。
- `docs/hub/contracts-step1.md`、任务报告虽不在路径白名单逐项枚举中，但分别由 H12 和报告要求明确授权，不算越界。设计文档的改动仅为允许的 §4.3 实测补充。
- 本任务不是设备界面还原任务，没有 `out/ui-compare/`，不把“至少 10 屏对照”伪记为完成。向导只核查了本步示例功能和 HTTP 测试，未做设计稿视觉验收。
- Docker、容器峰值、Linux CI、真实语音和实机仍无本轮证据；开发报告已说明这些限制，本轮没有把“尚未验证”直接定为缺陷，也没有宣称远端保存或发布。

## 独立验证命令

从仓库根目录执行，`INK_DEPS` 使用本轮指定环境。T1 使用已有 build 缓存并核对：两套均 `INK_WERROR=ON`；ASan 另有 `INK_SANITIZE=ON`；Python、字体和 LVGL 路径符合任务要求。

### T1：构建、全量及内存检查

```sh
cmake --build build -j8
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j8
cmake --build build-asan -j8
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j8
```

四条退出 0。普通 CTest 91/91，30.07 秒；ASan/UBSan 91/91，128.05 秒，无跳过。

### T2：五次端到端与测量

```sh
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build \
  -R '^hub-simulator-e2e$' --repeat until-fail:5 --output-on-failure -V
```

退出 0；每次 `turns=1`、`tool_calls=1`、虚拟时间 17.05 秒。以下为本轮控制台输出，不是转抄开发报告；RSS 保留原始 bytes 便于复算。

| 次数 | RSS（bytes） | 采样数 | p95（ms） | 最大（ms） | 墙钟（s） |
| --- | --- | --- | --- | --- | --- |
| 1 | 54329344 | 1459 | 2.169833 | 3.562042 | 17.053720 |
| 2 | 51691520 | 1470 | 2.176208 | 7.775458 | 17.046750 |
| 3 | 54738944 | 1468 | 2.185250 | 10.939250 | 17.038588 |
| 4 | 55394304 | 1468 | 2.186625 | 7.407625 | 17.079143 |
| 5 | 46874624 | 1468 | 2.176335 | 5.737084 | 17.057005 |

### T3：全新依赖环境、CLI、pytest 和生产解析器

```sh
PYTHONDONTWRITEBYTECODE=1 hub/.venv/bin/python - <<'PY'
import os, pathlib, subprocess, tempfile
root = pathlib.Path.cwd()
with tempfile.TemporaryDirectory(prefix='hub-review-fresh-') as tmp:
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

退出 0；18 个包重新安装，84 passed in 2.29s；142 条生成消息，最高 110 tokens。

### T4：桥关闭时的基线对照

```sh
PYTHONDONTWRITEBYTECODE=1 hub/.venv/bin/python tests/hub/compare_default.py \
  --before work/baseline-build/ink-sim --after build/ink-sim \
  --assets build/assets --content build/test-content --out work/default-comparison
```

退出 0；`home_basic`、`reader_basic`、`ai_ask` 的 `metrics.json` 和 `final.png` 均逐字节相同。基线二进制的复用限制见 H9。

### T5：反向对照抽查

实际运行了两个独立 Python 进程：通过 `inspect.getsource` 取原方法，以 `exec` 在内存中撤去保护，再 `pytest.main(['-q','-p','no:cacheprovider',用例名])`；不写源码、不复用修改后的进程。

| 内存中撤掉的保护 | 定向用例 | 结果 |
| --- | --- | --- |
| `Connection.valid_output` 中 `self.hub.authority.check(scope)` 替换为 `pass` | `tests/isolation/test_async_output.py::test_permission_change_invalidates_pending_output` | pytest 退出 1，捕获撤权后的 `ai.delta`，1 failed |
| `Connection.request` 中 `if asyncio.current_task().cancelling()` 替换为 `if False` | `tests/contract/test_protocol.py::test_concurrent_reply_does_not_swallow_cancellation` | pytest 退出 1，取消未抛出预期异常，1 failed |

这些测试能测到其对应修复，但覆盖范围不能外推到 R1 的另外两条交付路径。

## 缺陷复现命令

### P1：撤权后的工具结果，以及换代后的已排队命令

```sh
PYTHONDONTWRITEBYTECODE=1 hub/.venv/bin/python - <<'PY'
import asyncio, json, tempfile
from aiohttp import ClientSession, web
from ink_hub.gateway.server import Hub, Connection
from ink_hub.models.fake import FakeModel

async def revoked_result():
    with tempfile.TemporaryDirectory(prefix='hub-review-') as tmp:
        gate = asyncio.Event()
        model = FakeModel(text='synthetic-old-scope-result', gate=gate)
        hub = Hub(tmp, dev_allow_proto1=True, model=model)
        runner = web.AppRunner(hub.application(), access_log=None)
        await runner.setup()
        site = web.TCPSite(runner, '127.0.0.1', 0)
        await site.start()
        port = site._server.sockets[0].getsockname()[1]
        try:
            async with ClientSession() as client:
                async with client.ws_connect(f'http://127.0.0.1:{port}/v1/device') as ws:
                    await ws.send_json({'jsonrpc':'2.0', 'id':1, 'method':'hello',
                                        'params':{'proto':1, 'device':'synthetic-review'}})
                    await ws.receive_json()
                    await ws.send_json({'jsonrpc':'2.0', 'id':2, 'method':'tools/call',
                                        'params':{'name':'llm.ask', 'arguments':{'question':'synthetic'}}})
                    await asyncio.wait_for(model.started.wait(), 2)
                    hub.authority.remove('demo', 'default')
                    gate.set()
                    reply = await asyncio.wait_for(ws.receive_json(), 2)
                    print('revoked result:', json.dumps(reply))
                    assert reply['result']['structuredContent']['say'] == 'synthetic-old-scope-result'
        finally:
            await runner.cleanup()

class Sink:
    def __init__(self):
        self.started = asyncio.Event()
        self.release = asyncio.Event()
        self.sent = []
    async def send_str(self, data):
        if not self.started.is_set():
            self.started.set()
            await self.release.wait()
        self.sent.append(json.loads(data))
    async def close(self):
        pass

async def stale_command():
    with tempfile.TemporaryDirectory(prefix='hub-review-') as tmp:
        hub = Hub(tmp, model=FakeModel(tool='reader.next_page'))
        ws = Sink()
        c = Connection(hub, ws)
        c.scope = hub.authority.issue('demo', 'default')
        c.tools.definitions['development', 'reader.next_page', 1] = {
            'name':'reader.next_page', 'effect':'navigate', 'inputSchema':{'type':'object'}}
        queued, switched = asyncio.Event(), asyncio.Event()
        original = c.queue.put
        async def put(item):
            await original(item)
            method = json.loads(item[2]).get('method')
            if method == 'tools/call':
                queued.set()
            if method == 'caps/changed' and c.epoch == 1:
                switched.set()
        c.queue.put = put
        blocker = asyncio.create_task(c.notify('caps/changed', {}))
        await ws.started.wait()
        await c.receive(json.dumps({'jsonrpc':'2.0', 'id':10, 'method':'ai.ask',
                                    'params':{'question':'synthetic',
                                              'page':{'actions':['reader.next_page']}}}))
        await asyncio.wait_for(queued.wait(), 2)
        member = asyncio.create_task(c.receive(json.dumps({
            'jsonrpc':'2.0', 'method':'ink/member',
            'params':{'member':'default', 'epoch':1, 'authed':False}})))
        await asyncio.wait_for(switched.wait(), 2)
        ws.release.set()
        await asyncio.gather(blocker, member)
        await c.queue.join()
        epochs = [m['params']['_meta']['epoch'] for m in ws.sent if m.get('method') == 'tools/call']
        print('current_epoch=', c.epoch, 'sent_call_epochs=', epochs)
        assert c.epoch == 1 and epochs == [0]
        await c.close()

async def main():
    await revoked_result()
    await stale_command()
asyncio.run(main())
PY
```

### P2：运行中取消导致后续任务不再执行

```sh
PYTHONDONTWRITEBYTECODE=1 hub/.venv/bin/python - <<'PY'
import asyncio, tempfile
from unittest.mock import patch
from ink_hub.data.scope import Authority
from ink_hub.jobs.queue import JobQueue
from ink_hub.jobs.runner import Runner

async def main():
    with tempfile.TemporaryDirectory(prefix='hub-review-') as tmp:
        auth = Authority()
        auth.add('synthetic', 'adult')
        scope = auth.issue('synthetic', 'adult')
        q = JobQueue(tmp+'/jobs.db', auth)
        runner = Runner(q)
        started, release = asyncio.Event(), asyncio.Event()
        async def handler(scope, payload):
            started.set()
            await release.wait()
            return {'ok':True}
        with patch('ink_hub.jobs.runner.example_hash', handler):
            first = await runner.submit(scope, 'first', 'example.hash', {'text':'synthetic example'})
            task = asyncio.create_task(runner.run())
            await asyncio.wait_for(started.wait(), 2)
            q.cancel(scope, first)
            second = await runner.submit(scope, 'second', 'example.hash', {'text':'synthetic example'})
            release.set()
            result = await asyncio.gather(task, return_exceptions=True)
            state = q.get(scope, second)['state']
            print('runner_done=', task.done(), 'exception=', repr(result[0]), 'next_job_state=', state)
            assert isinstance(result[0], ValueError) and state == 'queued'
asyncio.run(main())
PY
```

SEVERE=0 HIGH=2
