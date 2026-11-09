# set-bluetooth 执行报告

基线：`a6f4140df5790c22d3f8ff5cfb00557289f3455c`。日期：2026-11-10。执行结束，限制与验收结论见下表；仅本地提交，不推送。【实测】产品代码增删合计 **229 行**，含 CMake 测试注册，低于 250 行预算。

以下命令使用任务提供的 `INK_DEPS`、`INK_DESIGN`；构建与 CTest 最多 `-j2`，均使用 `taskpolicy -c utility`。构建参数照总规则；内存检查只换 `build-asan` 并加 `-DINK_SANITIZE=ON`。

全量另设 `PYTHONDONTWRITEBYTECODE=1`、`PYTHONPATH="$PWD/hub:$INK_DEPS/hub/.venv/lib/python3.11/site-packages"`，只读复用现成中枢依赖，不因缺依赖跳过测试。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 开关、搜索、配对、连接、忘记、入口 | 模拟器专项通过；底层在途扫描不能硬取消 | 【实测】普通和 ASan/UBSan 专项通过；覆盖空搜索、配对失败与成功、点按断开/重连、长按忘记、扫描中关闭、超时、三个入口、孩子权限、重启保留；证据在 `build/test-out/bluetooth/` | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R '^bluetooth-settings$'`（内存版换目录） |
| 2 截图与画板对照 | 截图已检查；原始差分超过工具的 12/255 参考线 | 【实测】SetBluetooth **31.816/255**，未修改设计稿、阈值或截图；[三栏对照](../../../build/bluetooth-compare/SetBluetooth.png)；空搜索、配对失败、超时、孩子截图同在专项证据目录 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/bluetooth_compare.py --build build --design "$INK_DESIGN"` |
| 3 全量测试与内存检查 | **未全通过**；唯一遗留为共享能力文档检查 | 【实测】普通、ASan/UBSan 均为 **286/287 通过、1 失败、0 跳过**，退出 8；唯一失败 `capability-table-doc`，按规则留给主会话统一生成。内存日志无 Sanitizer 错误 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`（加上述环境；内存版换目录） |

【实测】反向对照：分别撤去开关门禁、配对后连接的实现，编译替换对象链接同一验收程序；两次退出均为 1，分别检出 18、2 个失败，均跑到结束。产品源码哈希前后相同；正常实现的专项退出 0。复现：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/bluetooth_reverse.py --build build`；命令和结果在 `build/bluetooth-reverse/{commands,summary}.json`。

【实测】普通、内存构建均成功。最终日志：`work/test-final.log`（308.28 秒）、`work/test-asan.log`（407.12 秒）；内存完整输出见 `build-asan/Testing/Temporary/LastTest.log`。基线能力文档检查通过（434 项定义，`work/baseline-catalogue.log`）；当前目录已在忽略路径生成并校验（438 项定义，`work/capabilities-set-bluetooth.md`），没有修改共享文档。首次全量的电话测试因导航流程变化失败；更新流程后电话专项及两轮最终全量中的电话测试均通过。

【实测】`git diff --check`、报告 3 个本地链接检查通过；基线差异的隐私正则命中 2 处，均为规则指定的假耳机地址，本机路径及真实标识命中 0 处。范围为 14 个任务相关文件，没有共享文档、设计稿、构建产物或硬件模型改动；记录在 `work/privacy-summary.json`。

【实现】架构审视：页面只依赖系统能力、设置/状态与 UI，不引用 HAL 或模拟器。设置和配对由蓝牙服务拥有；按钮与 AI 共用 `settings.set`、`bt.*`，页面提供字段、动作、状态和分页快照，忘记动作也可由 `bt.forget` 调用。

【实现／实测】孩子沿用 `bt.*` 的 `child_ok=true`：可从通话入口或 `shell.bluetooth` 打开耳机页，也可改 `bt.enabled`；通用设置和声音设置入口仍遵守既有成人权限。设置是设备级的，全家共用；普通蓝牙操作不弹确认卡。

## 没做到的

- 【实现】[HAL](../../../platform/include/ink/hal_bt.h) 只有阻塞的 `hal_bt_scan`，没有取消接口。关闭开关后退出页面搜索状态、拒绝新扫描/配对/连接并丢弃在途结果；[当前模型](../../../ports/sim/hal/hal_bt_sim.c)的扫描调用仍最多等待 1.5 秒返回，随后执行断开。没有修改模型的时长、行为或 HAL 契约，不能称为已实现底层扫描硬取消。
- 未操作设备，未验证真实无线协议、音质或功耗；未执行 Linux/gcc 构建。
- 未覆盖连续快速开关和超长设备名等验收外场景。

## 和设计不一致的

- 整体使用项目现有的 1.2 倍主题（按任务要求不算问题）；纸色、静态灰、状态栏字形及测试时间/电量与旧画板不同。为本任务的反馈和忘记操作增加状态说明与“长按已配对的耳机可忘记”。未改公共主题来压低分数；一页能放下时隐藏分页栏。截图无控件重叠。

## 改过的已有测试

- `tests/integration/ai_surface.c` 只增加三个导航能力的预期项，继续逐项验证真实 AI 工具清单；未删除、放宽原有断言，未改资源预算。
- `tests/phone_calls/phone_calls.c`：旧流程默认点“耳机未连”后停留通话页，改为检查进入蓝牙页、返回后仍在同一通话，再继续原流程。原有路由、静音、DTMF、录音限制及隔离断言全部保留，增加三个导航断言；对应任务第 2 条改变的行为。

## 要用户定的

- 暂无新的产品决定。蓝牙音箱与翻页器按任务范围不实现，已有接口保留。

## 顺手发现

- 【实测】起始 HEAD 与指定基线一致，工作区干净；相关未结 Issue/PR 搜索无结果。
- 决策板链接无法由读取工具访问；按任务书和仓库现有记录继续，不修改共享决策文件。
- 开发中专项发现快照缺少字段角色及测试宿主调用位置问题，已修复，普通与内存专项复验通过。`ui-scale` 静态检查通过。

## 新增的机制

- 设置项 `bt.enabled`：任务第 1 条明确要求服务没有开关时补充，复用现有设置落盘与 BG apply。
- `bt.pair` 可选参数 `connect`：任务第 1、3 条要求点按配对并连接、按钮与 AI 走同一能力；原来不传参数的配对语义保留。
- 三个页面导航能力、页面局部操作状态和 F4 列表：用于任务要求的入口、状态与 AI 快照；无新增任务、线程、通用状态机、持久格式、重试、后台缓存或队列。
- 超过 100 行的原因：新页面需真实读配对集合、合并搜索结果、处理异步回执和页面归属、分页、失败状态及 AI 快照，并接三个入口。看过更小方案：仅给设置行绑扫描不能形成页面；直接从页面调 HAL 会绕过能力和权限；改模型取消扫描违反任务边界。本次复用 F1/F4、设置表与能力执行器，没有另造框架。

## 要改的文档

- 主会话统一生成 `docs/system/capabilities.md`：补 `bt.enabled`、三个 `shell.bluetooth*` 导航能力和 `bt.pair(connect)` 参数；本分支按总规则第 7 节不改生成的共享文档。
- `docs/system/ui-dependencies.md`：蓝牙页面和通话、声音入口已接；真实协议与取消扫描接口仍无证据。
- `docs/system/system-services-design.md` §10.4：蓝牙开关、可选配对后连接、孩子继承规则和关闭开关时的在途扫描边界；不改设计合同迁就实现。
