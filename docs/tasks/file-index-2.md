# file-index-2：图片解码、抖动到 16 级灰、缩略图

总规则先读 docs/tasks/codex-rules.md，第 7 节优先。报告写到 docs/tasks/file-index-2/REPORT.md。

## 要做的

backlog 的 file-index 一节拆成两半，这是后一半，预算 250 行产品代码：图片解码、按屏幕实际的 16 级灰度抖动、缩略图（有上限）。

file-index-1 正在另一个克隆里改 system/services/file/。**这个任务不改那个目录**：代码放新目录（例如 system/services/media/），缓存键用调用方给的“对象编号＋版本”，等 file-index-1 合入后再接上。

## 从哪里看起

- 先找仓库里已有的解码：LVGL 自带的 PNG、JPEG 解码，相机或相册已有的代码。能复用就复用。要引入新的第三方库，报告里写理由、许可和大小。
- 灰度：看界面怎样把颜色变成屏幕的 16 级（system/ui/theme.c、system/services/display/、ports/sim/hal/hal_display_sim.c）。每一级的实测亮度在 drivers/display/pqbn/panel_e0470a14.h 的 `PANEL_LEVEL`。抖动的目标就是这 16 级，不另起一套灰度表。
- 内存：大缓冲放 PSRAM（`OSAL_MEM_EXTERNAL`），不新建常驻任务。

## 验收

1. 解码 PNG 和基线 JPEG；坏文件、超大图如实返回错误，不崩（测试）。
2. 抖动：一张渐变图抖动后的平均亮度，和原图相差不到一级（按 `PANEL_LEVEL` 算）（测试）。
3. 缩略图：边长、单张大小、缓存总量都有上限，超了淘汰最旧的（测试）。
4. 同一编号换了内容（版本变了），旧缩略图不再用（测试）。
5. 全量测试和内存检查通过；产品代码不超过 250 行（第三方库不算，但报告里要写）。
