# driver-audit-1b：屏幕与输入驱动差异审计

审计日期：2026-11-11。基线：`87bf48655ee4820837b0f54a43c8f5aed37278c2`。

状态：进行中，逐模块补充证据。只读源码，不改产品代码、测试及构建文件，不操作设备。

证据标记：【实现】为源码核查；【实测】仅表示本次在电脑上执行命令所得结果，不代表真机验证；【估算】为尚待验证的风险和改动量。按总任务说明，不运行构建、全量测试、ASan、S31 编译和 gcccheck。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1：全部模块与流程、双方出处 | 进行中 | 下方模块记录 | `nl -ba`、`git show`、`git diff` |
| 2：前十风险、直接拷贝清单 | 待完成 | 下方清单 | 源码交叉核查 |
| 3：仅新增本任务目录文件 | 开工时工作区干净 | 基线与当前 HEAD 一致 | `git status --short --branch`、`git rev-parse HEAD` |
| 补充：原厂主分支是否更新 | 待核查 | 原厂公开仓库 | 只读远端查询 |

## 来源与复现约定

- 本项目行号均取指定基线。`U:` 表示原厂 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7` 的 `main/boards/metalio-e-ink4-plus/`；`U0:` 表示同路径的 `690a023c66862af103a3187464373ee7c5efcd14`。用 `git -C "$INK_UPSTREAM" show 874538e:<路径>` 配合 `nl -ba` 复核，不移动原厂 HEAD。
- 【实测】GitHub 公开 `main` API 返回 `874538eb54f7f7eb593cd40cd1ba6dc8497226e7`，提交日期 `2026-11-07T11:32:56Z`，标题 v2.0.77；compare 返回 `identical`、ahead/behind 均为 0。网页烧录列表的 2.0.78 不等于已公开源码。来源：[main 提交 API](https://api.github.com/repos/CloudZao/Metalio-E-INK4-Plus/commits/main)、[指定提交](https://github.com/CloudZao/Metalio-E-INK4-Plus/commit/874538eb54f7f7eb593cd40cd1ba6dc8497226e7)，查阅 2026-11-11。
- ESP-IDF 核查版本：`fff9895c82d744c7237be8847347bdd1b07c6643`。下文 `IDF:` 为该检出下的相对路径；配置默认值不等于本次检查过最终 ELF/map。

## 模块记录

以下事实均为【实现】，硬件后果为条件性风险，未在本次上机复现。

| 模块 | 原厂做法（文件:行） | 我们的做法（文件:行） | 差异 | 可能的坑（场景及后果） | 严重度 | 建议 |
| --- | --- | --- | --- | --- | --- | --- |
| 屏幕输出／完成与中止 B01 | `U:display/output_lcd/lcd_driver.c:247–283,564–641,1263–1299`：LCD/GDMA 同核；完成先屏蔽 EOF；中止在 IRQ 核等待旧 ISR 退出、reset 并清 pending；中止不回调完成 | `ports/esp32s31/display_upstream.cmake:2,19–23` 固定旧版；`hal_display_s31.c:67–71,190–204` 先清回调再 abort，成功后直接重置队列 | 我们缺：新输出层的同步与 EOF 清理 | 超时／guard 中止或连续相切换时，旧 EOF 可能继续读旧队列、补旧 bounce，或污染新帧；旧 abort 还会伪报完成。当前初始化通常同核，不能把跨核风险当作每次必现 | 高 | 直接取新版 `lcd_driver.c/.h` 与原版队列；HAL 改为先在任务中 abort 返回，再清回调／复用队列，见迁移说明 |
| LCD 中断与 IRAM B02 | `U:display/output_lcd/lcd_driver.c:915–926` 无条件 `ESP_INTR_FLAG_IRAM`；`247–265` 完成路径调 `gdma_stop`；`471–505` 多批路径调未标 IRAM 的 `start_ckv_cycles:340`；新版未消除此问题 | 旧版同类调用仍在；`ports/esp32s31/sdkconfig.defaults:1–42` 未开启 `GDMA_CTRL_FUNC_IN_IRAM`；`hal_display_s31.c:147–164` 回调标 IRAM | 一样：继承原厂风险；不能宣称所有被调函数均在 IRAM | 若缓存关闭时恰好结束一帧，默认在 Flash 的 `gdma_stop`／HAL 调用可能触发 cache error；当前 684 行不会进入大于 1000 行的 CKV 续批分支，后者不列作当前必现故障 | 高 | 复用原厂并核对最终 map、控制函数及其依赖的 IRAM 放置；不得只凭函数入口标记验收；D-36 互斥仍须保留 |
| 送行队列／DMA 内存 B03 | 两版本 `display/output_common/line_queue.c` 完全相同：`26` 用普通 `calloc` 分配指针表，`32` 行缓冲显式 INTERNAL；`lcd_driver.c:818–847` bounce/描述符为 INTERNAL\|DMA | `hal_display_s31.c:342` 两条 128 项队列；`sdkconfig.defaults:10` 普通分配仅 16 字节以内优先内部内存；指针表约 512 字节／条 | 一样：行数据内部，不保证指针表内部；新版也未修 | IDF 默认分配策略优先把该指针表放 PSRAM（`IDF:components/heap/heap_caps.c:107–124`）；若将 GDMA ISR 改成 cache-safe，却只检查行缓冲，会在 `lq_read:75` 访问外存指针表 | 中 | 原厂队列可直接复用，但其 ISR 可达元数据须显式放内部内存；该必要安全适配不能靠单改 IRAM 配置替代 |
| 扫描几何／CKV B04 | `U:display/output_lcd/lcd_driver.c:212–220,787–805,1136–1151,1328–1335`：S31 原生高度、0 dummy byte、精确 RMT tick，SPV 后省一行等待 | 固定旧版会把 684 向上补成 688，8 位总线每行加 1 dummy byte，周期取整微秒；`hal_display_s31.c:335–339` 参数可启用新版原生扫描分支 | 我们缺 | 首尾像素错位、额外扫描和 gate/source 相位偏差；换版后物理驱动时长会变，不能直接声称 PQ-BN 的灰阶标定仍等效 | 高 | 拷新版输出传输；保持本项目 PQ-BN/HAL 接口，核对相间节拍与原厂 SPV 时间戳，逐项上机验证 |

### 屏幕输出核查补充（进行中）

- `epdiy.c` 与 `line_queue.c/.h` 在两个提交之间无差异。`highlevel.c:125–129` 的全屏更新不再受 dirty 掩码限制；`206–250` 新增黑色均衡模式全屏处理，防止“图没变就不刷”跳过黑色补偿。本项目用 PQ-BN，不编这些高层文件，不能把它们记成本项目已获得的修复。
- `gdma_stop` 的证据是 `IDF:components/esp_driver_dma/Kconfig:3–9`（默认 n）、`linker.lf:9–13,46–72`（控制函数与 HAL 条件放 IRAM）、`src/gdma.c:687–701`（调用 HAL）。未编译，最终放置待 map 核实。
- GDMA 的缓存安全与 LCD 是两回事：原厂 `lcd_driver.c:593–597` 仅在 `CONFIG_LCD_RGB_ISR_IRAM_SAFE` 下设 `isr_cache_safe`；IDF `src/gdma.c:1132–1134` 才据此加中断 IRAM 标志。入口写 `IRAM_ATTR` 不自动让中断在 Flash 操作期间运行，也不证明数据可访问。

## 最可能出事的前十个坑

待完成。

## 直接拷贝清单

待完成；改动量均为【估算】，本任务不实施。

## 上机先核对清单

待完成；本任务不操作设备。

## 没做到的

- 按任务边界不做真机验证、修复、构建及测试；不据源码审计宣称硬件通过。

## 和设计不一致的

待核查。

## 改过的已有测试

无。

## 要用户定的

待核查。

## 顺手发现

待核查。

## 新增的机制

无。产品代码改动 0 行。

## 要改的文档

待核查；本任务不修改共享文档。

## 收尾核查

待完成文件范围、链接及隐私检查。
