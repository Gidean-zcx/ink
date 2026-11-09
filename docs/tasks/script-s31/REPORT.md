# script-s31 执行报告

基线：`96887c78874e3e58945fb6aaf6f511911f739e88`。范围：框架原型测量，不改产品代码，不连接设备，不选择运行时。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】主机普通配置通过，内存检查待补 | 四候选各 21 项共用检查、两轮确定性回放、五轮计时；截图逐像素相同；MicroQuickJS 七项独立防护检查通过；[完整样本](host-measurements.json)、[反向对照](negative-controls.json) | `taskpolicy -c utility bash tools/appfw-spike/run.sh all`；`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/appfw-spike/negative.py` |
| 2 | 【实测】通过 | 下表、[分节绝对值与增量](s31-measurements.json)，已补 §6.5；链接图无原型防护测试 | `taskpolicy -c utility bash tools/appfw-spike/s31.sh` |
| 3 | 【实测】默认固件通过；全量／内存检查执行中 | 前后默认镜像 SHA-256 和链接图逐字相同；产品代码无修改 | 同上；全量命令待补 |

## 没做到的

- 全量／内存检查执行中。
- 未上机，未测 S31 动态堆、栈、GC 最坏停顿及真实显示时延；本任务只要求编译链接。

## 已测基线

- 【实测】默认 S31 构建退出 0；Flash 1,148,400、DIRAM 82,958、PSRAM 静态 BSS 382,152 字节。镜像 1,212,288 字节，SHA-256 `00fd4b36e4774a3dfa8413440a9bd0d7214b973c71beb66a88064ff2d3f06965`。

ESP-IDF v6.1、RISC-V GCC 15.2.0，沿默认 `sdkconfig.defaults`、`-O2`。以下全部为【实测】字节，相对上面的默认固件；保留同一页面／能力桥及页面载荷，不含模拟器测试任务。四配置源码只在 `tools/appfw-spike/`。

| 候选 | Flash 增量 | 内部 DIRAM 增量 | PSRAM 静态增量 |
| --- | --- | --- | --- |
| C＋LVGL | +3486 | +80 | 0 |
| 受限 JSON | +3844 | +80 | 0 |
| Lua 5.4.8 | +95060 | +104 | 0 |
| MicroQuickJS `4119f681` | +138806 | +112 | 0 |

Flash 是 IDF 分类值，镜像文件另有对齐和内部段装载数据；静态 PSRAM 为 0 不代表运行时不用 PSRAM。主机 MicroQuickJS 预留 131072 字节 arena，不把预留量写成内部活跃峰值。归档 SHA-256 `911ebe922ec84acf530a4db64ea3963fbcc4f9ae84e176ecfa819273fb5fa638`，许可证和来源见[第三方记录](../../../tools/appfw-spike/THIRD_PARTY.md)。

【实测】反向对照：去掉 MicroQuickJS 中断回调，无限循环在 5 秒宿主超时后被测试进程终止；恢复后同一套检查通过。Lua 配额与 CPU 结算反向对照也通过，所有临时源码改动逐字恢复。旧工具缺资源及未带文字会话版本时已实际失败；只补原型依赖、复用既有 `AI()` 帮手，不改产品策略。

## 和设计不一致的

- 无。本次是单页、有界测量，不选择运行时，不实现应用安装／完整 A2UI 或扩展 F10 的验收范围。

## 改过的已有测试

- `harness.c`：文字操作复用既有 `AI()` 帮手读取 `text.state` 的会话／版本，再走能力调用；原来的“调用成功且文本可见”拆成两个断言，均保留。页面释放检查纳入 MicroQuickJS 标签。
- `measure.py`：增加第四候选与七项防护检查，保留原有所有断言。
- `negative.py`：构建并行度降到 2；计时反向对照适应多行计时边界，仍只移除同一 `vtk_sched_point`。
- `CMakeLists.txt`：补当前基线已要求的 `ink_ui_fonts`／`ink_icons` 资源依赖；原工具单建目标会因缺图标直接退出。未改根工程或全量测试期望，未放宽预算。

## 要用户定的

- 暂无。

## 顺手发现

- 【实现＋实测】当前 IDF 尺寸工具的 `json2` 汇总不导出 `used_diff`；测量脚本额外取 raw 链接图差分，用 `used_diff` 交叉核对，避免把绝对值写成增量。

## 新增的机制

- 固定 128 KiB PSRAM 缓冲与中断回调：任务要求的 MicroQuickJS 有界原型，仅在测量目标中。
- 显式注入的 S31 测量配置：任务要求的四候选静态差分；默认端口不引用它。
- 未新增产品任务、线程、状态机、持久格式或配置。

## 要改的文档

- 任务明确允许更新 `docs/system/app-framework-design.md` §6.5；不修改共享需求与决策文档。
