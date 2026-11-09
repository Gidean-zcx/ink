# ESP32-S31 移植骨架报告

基线：`2228df7de99de9fea15fddc22868a0de1caae770`。只编译，不连接设备。已完成 S31 交叉编译；模拟器和内存检查进行中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | `build-s31/ink-s31.bin` 已链接，尺寸报告在 `build-s31/size.txt` | `taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 2 | 【实现】完成；开机自检已交叉编译，未上机执行 | FreeRTOS、heap_caps、esp_timer；主机微秒换算测试待跑 | 同上；`ctest --test-dir build -R unit-s31-osal-ticks` |
| 3 | 【估算】S31 数据已有，主机数据待补 | `.su` 栈帧和 `.ci` 直接调用图；非完整上限 | `python3 tests/esp32s31/stack_report.py build-s31` |
| 4 | 【实测】本地同脚本通过；远端未运行 | workflow 新增编译步骤，错误直接使任务失败 | `bash ports/esp32s31/build.sh` |
| 5 | 待验证 | 模拟器全量与 ASan/UBSan | 待补 |

## 没做到的

- 未运行真实硬件、未执行设备自检；未推送，远端 CI 不声称已通过。

## 和设计不一致的

- HAL 按任务书保持不可用；端口入口先自检，发现屏幕尺寸为零就记录“没有屏幕”并返回，不进入会要求帧缓冲的完整系统启动。完整系统和应用仍参与编译、链接。

## 改过的已有测试

- 无。

## 要用户定的

- 无。

## 顺手发现

- 本机没有 Ninja。ESP-IDF v6.1 的 Makefiles 后备会硬编码 `CPU 数 + 2` 并发；脚本只在当前 Python 进程中将该命令限制为 `make -j 2`，不修改已安装 IDF。

## 新增的机制

- 按任务书新增 ESP-IDF 端口与 FreeRTOS OSAL；HAL 仅提供明确失败的桩。
- 开机自检是任务书验收 2 要求的验证；不新增常驻产品任务或服务。

## 要改的文档

- 合并时更新平台文档与项目入口中的端口状态；本任务不修改共享文档。
