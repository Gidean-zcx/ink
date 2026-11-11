# port-cell-3 执行报告

基线：`281dbe7130514a602c68e4c6de2b7e53c81a5db1`。状态：原厂接入完成，收尾验证中。仅本机编译和宿主测试，不接设备、不对外提交。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 原厂实现与来源 | 部分完成【实现】 | 已切原厂 modem/iot_eth/UHCI；来源和改动见 vendor 的 SOURCE；C10 与睡眠配置分支见下文 | 来源逐行对照、SHA-256 |
| 2 构建与测试 | 收尾中【实测】 | S31 宿主 36/36、固件编过、gcccheck 6/0；全量主工程、ASan 和反向对照待补 | 见验证记录 |
| 3 上机核对清单 | 待整理 | 宿主结果不代表真机验证 | 待补 |

## 没做到的

- C10：原厂 InitGpio 已使用 SRDY 低电平唤醒；现有电源 HAL 的 EXT1 ANY_HIGH 仍会覆盖它，该文件不在本任务允许修改范围，不能声称浅睡端到端已修。
- 没有真机验证；没有 Linux 原生构建环境，另以交叉 GCC 和 S31 固件编译查警告。

## 和设计不一致的

- 未确认 ECSCLKEX 的持久性，仍拒绝发送。代码保留 `config_readonly` 失败事件，不报初始化成功；但对于已经真实握手的通道仍接 iot_eth，以保留换装 14/15 的已有联网行为。此处没有按“任一配置拒绝就停止后续接网”的严格解释关闭网络，须由主会话复核，不算该条完全通过。
- 已开通道的 SPIC 和每次开机最多一次 CFUN 关开沿用本项目真机偏差；原厂部分失败分支改成保留可诊断的 AT 通道。

## 改过的已有测试

- 尚无。

## 要用户定的

- 尚无。

## 顺手发现

- 尚无。

## 新增的机制

- 待按原厂任务、栈和任务书要求统计。

## 要改的文档

- 由电源任务修改 `hal_power_s31.c` 的 SRDY EXT1 极性，并复核保网浅睡与 P14 停放；本任务未改共享文件。

## 上机核对清单

- 探测、CFUN、查卡、ECNETCFG、已开通道重新握手、DHCP、对时。
- 重启联网恢复、切卡、短信 PDU 提示、ATD/ATA/CHUP。

## 验证记录（收尾中）

- S31 宿主全量：`100% tests passed out of 36`，`Total Test time (real) = 37.43 sec`。
- S31 固件：`INK_S31_LIGHT_SLEEP=OFF taskpolicy -c utility bash ports/esp32s31/build.sh`，exit 0，无 warning/error。
- GCC：`taskpolicy -c utility python3 $INK_DEPS/../ink-codex/tools/gcccheck.py build-s31-host ports/esp32s31/s31_modem.c ports/esp32s31/s31_cell_rx.c ports/esp32s31/hal_cellular_s31.c ports/esp32s31/tests/cell.c vendor/iot_eth/iot_eth.c vendor/iot_eth/iot_eth_netif_glue.c`：`gcccheck: 6 compiled, 0 with findings`。
- 产品新增 382 行：既有文件对基线、新 vendor 文件对上游逐行计新增；删除不抵扣，测试和文档不计。450 行内。
