# 应用框架实测报告

基线：`39a349d`。范围：独立原型、测量、设计第 3／6 节回填；不改产品代码，不碰设备，不推送。开始：2026-11-05 14:50 UTC。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| B1 | 通过 | 【实测】C／JSON／Lua 同页，三张截图逐像素一致；每轮 20 条检查；两轮确定性结果一致 | `bash tools/appfw-spike/run.sh all` |
| B2 | 进行中 | 测量程序已跑；计时边界已补显式结算，最终数据与文档回填待完成 | `bash tools/appfw-spike/run.sh measure` |
| B3 | 进行中 | 【实测】默认普通 91/91（55.60 秒）、ASan／UBSan 91/91（100.73 秒）；独立原型的 sanitizer 检查在跑 | `cmake --build build -j4`；`ctest --test-dir build --output-on-failure -j3`；内存版换 `build-asan` |
| B4 | 进行中 | 按 1～2 小时时限执行 | 见后续记录 |

## 没做到的

S31 Flash／静态 RAM 没测：现有 ESP-IDF v6.1 的环境脚本提示缺少其 Python 虚拟环境，`idf.py` 不在 PATH。本轮不安装全局工具，不使用宿主二进制大小顶替。

## 和设计不一致的

按任务书的 2～3 个候选上限，选 C 基线＋受限 JSON＋Lua；第 8 节的 MicroQuickJS 未做。完整长列表、长文和安全沙箱准入不在本轮单页范围。

## 改过的已有测试

无。

## 要用户定的

无；本任务不选择产品运行时。

## 顺手发现

- 【实测】首轮原型的编辑结束回调遗漏，造成 3 条检查失败；已补齐回调，最终每轮 20 条均通过，未改既有测试。初次开关样式有缺口，截图核查后修正。
- 【实现／实测】直接读虚拟时钟会漏掉尚未结算的宿主 CPU 时间；已在测量区间两端调用 `vtk_sched_point`，旧读数弃用。
- 【实测】Lua 配额反向对照：只移除一行分配上限，原验证器以 `quota_rejected` 失败；逐字恢复源码、重建后通过。`python tools/appfw-spike/negative.py`；结果在忽略的 `work/appfw-spike/negative/`。
- 全量测试中的 `gui-smoke` 使用既有 `SDL_VIDEODRIVER=dummy`，没有打开窗口。所有构建最多 `-j4`，全量测试最多 `-j3`，顺序执行。
