# port-sleep-vendor-2：浅睡整条路径照原厂机制实现（不再自己凑）

总规则先读 docs/tasks/codex-rules.md，第 7、8、9 节优先。报告写到 docs/tasks/port-sleep-vendor-2/REPORT.md。**只编译、跑宿主机测试，不碰设备。**

## 背景

用户 2026-11-10 晚上：“睡眠你就复用原厂的机制不行吗”。

原厂的浅睡在这台机器上一直好用：出厂固件闲置 180 秒进待机，每 60 秒定时醒一次，按电源键能唤醒。我们 11-10 的上机测试（`docs/hardware/2026-11-10-selftest.md` 第二、三、七轮）没有完整照原厂做，每次都没醒或醒后 USB 不回来。port-sleep-vendor 已经移植了原厂的 S31 浅睡补丁和睡前步骤，但还有几处和原厂不同，最明显的是：

1. **哪个任务在睡**：原厂在专门的任务 `standby_lp` 里调浅睡，栈 4096 字节放内部内存，固定在 0 号核（`main/boards/common/power_policy/power_policy_standby_lp.cc:556`，`xTaskCreatePinnedToCoreWithCaps(..., 0, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)`）。我们是电源任务直接调，`system/services/power/power_service.c` 的 `.core = -1` 不固定核。
2. **怎么切片和唤醒**：原厂按 `kLightSleepSliceUs`（60 秒）切片，到点重新判断，唤醒源是电源键（GPIO2 EXT1）加定时器（`power_policy_standby_lp.cc:260-340`，`power_hw.cc` 的 `power_hw_light_sleep_once`）。
3. **睡前睡后每一步的顺序和等待**：照原厂 README 和 `power_hw.cc`。port-sleep-vendor 的对照表已经逐条对过，见它的报告，再复核一遍有没有漏的。

## 要做的

预算：产品代码净增不超过 150 行。

1. 让我们端口的浅睡在**和原厂同样的执行环境**里发生：内部内存的栈、固定 0 号核、同样的优先级关系。用最小的改法，例如电源任务固定 0 号核，或者睡眠那一段交给一个内部栈、固定 0 号核的任务。改动只在 S31 端口和电源服务的任务配置里；模拟器的行为不能变。
2. 切片、唤醒源、每段时长、到点后的判断，和原厂逐条对齐。我们的需求比原厂多的两样（时钟芯片 P16 唤醒、加速度计运动唤醒）保留，极性用已经实测过的 `MB_RTC_PENDING`。
3. 报告里给一张“原厂一步 → 我们一步”的对照表，补上 port-sleep-vendor 表里没有的执行环境（任务、核、栈）和切片部分，出处到行号。
4. 写一份“上机核对步骤”，给以后用户在场时用：按什么顺序测，每一步看什么，卡住了怎么救（按电源键能唤醒、长按断电以后自动写回）。这次不上机。

## 验收

1. S31 能编过，0 警告；端口单测（`ports/esp32s31/tests`）的普通版和 ASan 版都通过；为新的执行环境要求（固定核、内部栈）补断言，并做反向对照。
2. 模拟器全量测试照旧通过；gcccheck 通过；做隐私扫描。
3. 报告里有对照表和上机核对步骤。
