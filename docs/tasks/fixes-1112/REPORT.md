# fixes-1112 汇总

基线：`6171c2bf8d59238c95f2d548663f8b2ab3c6f43a`。执行中；未烧录、未连接串口、未推送。

| 任务 | 做了什么 / 文件 | 测试结果原话 |
| --- | --- | --- |
| ime-switch-key | 保留键盘浮层，更新映射；`keyboard.c`、`ime_keyboard1.*`、`memo.script`、`device_autotest/sim.c` | `ime-keyboard1` 与四项 `autotest-*` Passed；旧版反向对照 5 条断言失败；旧版丢字未稳定复现，详见分报告 |
| timer-seconds | 待复现和修复 | 未运行 |
| memo-save-busy | 待复现和修复 | 未运行 |
| cell-double-1 | 待复现和修复 | 未运行 |

最终检查：主工程普通全量、ASan、S31 宿主机、S31 固件、gcccheck、隐私检查均待执行。
