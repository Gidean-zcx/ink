# 应用框架实测工具

只在显式注入 `inject.cmake` 时增加三个 `EXCLUDE_FROM_ALL` 目标，不修改根构建或产品源码。运行的是现有无窗口模拟器、系统能力表和输入服务，既不启动 SDL 窗口，也不接设备。

```bash
export INK_DEPS=依赖检出目录
bash tools/appfw-spike/run.sh all
```

构建固定 `-j4`，三个目标顺序构建。输出在忽略的 `build-spike/` 和 `work/appfw-spike/evidence/`。脚本先核对 Lua 归档 SHA-256；下载和解压都只写 `work/`。要完整重建第三方源码，使用新的检出或确认解压目录没有自行修改。

每个候选先跑两轮确定性模式，再跑五轮 `--cpu measured:1`。每轮都检查真实触摸、AI 调用同一能力、旧版本拒绝、非法能力／参数、输入服务和页面释放；脚本比较两轮完整指标及三种入口的初始截图。失败会退出非零。样本数可由 `INK_SPIKE_SAMPLES` 调整；正式记录用五次，保留所有样本而非只留最好值。

```bash
# 与普通原型相同配置，只改变目录并打开 ASan/UBSan。
INK_SPIKE_BUILD=build-spike-asan INK_SPIKE_SANITIZE=ON \
  INK_SPIKE_OUT=work/appfw-spike/evidence-asan bash tools/appfw-spike/run.sh all
```

计时区间：初始化包含公共模型的分配和候选初始化入口；建页包含解析／执行、共同控件创建与布局，不含文件读取、首屏绘制和屏幕波形；开关是 `LV_EVENT_VALUE_CHANGED` 回调到同一能力完成，不含触控传输、队列等待和屏幕显示。各区间两端显式调用 `vtk_sched_point`，让模拟器先结算尚未记入虚拟时钟的宿主 CPU 工作，再读 `osal_now_us`。测量模式把宿主线程 CPU 时间乘 1，加调度和固定时间成本；此模式停用 `sim_charge_cycles` 的周期标注，**未经 S31 标定，不能把微秒数当真机速度**。确定性模式不为 Lua 解释计算另加猜测成本，仅用于行为回放。

内存记录两类：整个模拟场景的内部 RAM／PSRAM 堆高水位（包含既有系统、字库缓存、编辑器和测试任务）；`appfw.model`、`appfw.lua`、`lvgl` 标签分别记录分配峰值。标签峰值发生时间可能不同，不相加当作同一时刻峰值。`appfw.model` 包含固定 8194 字节源码暂存区和 JSON 解析临时区；C 没有源码暂存区。Lua 另列分配器请求字节峰值和包含分配器开销的 PSRAM 标签峰值。模拟器池不计本机静态数据，宿主文件／PNG／输出内存走 `vtk_host_call`，不冒充设备内存。

配额的反向对照：`"$INK_DEPS/build-deps/pyenv/bin/python" tools/appfw-spike/negative.py`。它临时移除本目录 Lua 分配器的一行配额检查，用原验证器确认失败；在 `finally` 中逐字恢复源码并重建、确认恢复后通过。只能独占运行，不能与其他构建同时执行；产物仍在忽略目录。

字符数是输入文件和紧凑快照的 Unicode 码点数，保留输入文件空白；另列 UTF-8 字节和物理行数。C 输入统计整个 `candidate_c.c`，JSON／Lua 统计各自页面文件，适配器和测量代码另列清单。它表示本轮页面载荷，不包含提示词、接口说明、能力 manifest、模型推理或 token 数，也没有调用真实 AI。

页面快照取当前成员和页面版本。示例只有合成数据，不保存真实偏好。JSON 是固定八行的窄协议，**没有实现 A2UI 兼容**；Lua 未打开标准库，只有建页与受限 `set` 桥，分配上限 128 KiB、每次脚本调用 20000 指令中断。它们是测量原型，不是可安装应用 SDK 或完成安全审计的沙箱；长列表、长文、GC 最坏停顿等不在本任务的单页范围。

S31 编译前只读检查：

```bash
bash -c 'source "$HOME/esp/esp-idf/export.sh" && command -v idf.py && idf.py --version'
```

本轮因 IDF 需要的 Python 虚拟环境不存在而失败，没有安装／修改全局环境，没有目标链接图，Flash 和静态 RAM 增量均写“没测”。即使本机程序能运行，也不能代替 esp32s31 编译或推算目标二进制大小。

来源、锁定版本、哈希与许可证见[第三方记录](THIRD_PARTY.md)，本轮结果见[实测记录](../../docs/research/app-framework-spike.md)。
