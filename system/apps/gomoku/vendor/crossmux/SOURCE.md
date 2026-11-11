# CrossMux 五子棋来源

来源：https://github.com/0x1abin/crossmux 。
固定提交：`a6fd569f7552db4ea1a6550ced7bc06568ae0e61`。查阅日期：2026-11-12。

四个文件完整复制，修改行：无。界面参考同提交的 `GomokuMenuActivity.cpp`、`GomokuGameActivity.cpp`；存档内容参考 `GomokuStore.cpp`，未沿用其路径和文件格式。

| 原文件 | SHA-256 |
| --- | --- |
| `src/activities/apps/gomoku/GomokuBoard.h` | `7c3be2c37f528b6a8408d69662614e5df5f0774598a3c9d73c2dd37933570aa3` |
| `src/activities/apps/gomoku/GomokuBoard.cpp` | `da7b21b5c79e002941bed33fa48aec8832702814af905f1f89f82e4f1b22aadf` |
| `src/activities/apps/gomoku/GomokuAI.h` | `6a99cd92ac528362bfaeaa27320b32f5caa677d343b0501668f0f7d22955f30e` |
| `src/activities/apps/gomoku/GomokuAI.cpp` | `0a1336e7a94978f64018216cfbadda4f20bae799333be91b532ccf9801867eef` |


兼容头位于 `../../compat/`：`HalStorage.h` 只将原文中未用于本应用存档的 `HalFile` 操作接到存储 HAL；`Arduino.h` 将时钟、让步和随机数接到 OSAL/HAL；`Logging.h` 接系统日志。
模拟器每次 `millis()` 查询按 320000 周期计入搜索成本，是保守的【估算】，并非 S31 性能实测；真机该成本标注为空操作，预算以真实 OSAL 时钟为准。
