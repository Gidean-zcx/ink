# fixes-1112 汇总

基线：`6171c2bf8d59238c95f2d548663f8b2ab3c6f43a`。执行中；未烧录、未连接串口、未推送。

| 任务 | 做了什么 / 文件 | 测试结果原话 |
| --- | --- | --- |
| ime-switch-key | 保留键盘浮层，更新映射；`keyboard.c`、`ime_keyboard1.*`、`memo.script`、`device_autotest/sim.c` | `ime-keyboard1` 与四项 `autotest-*` Passed；旧版反向对照 5 条断言失败；旧版丢字未稳定复现，详见分报告 |
| timer-seconds | `clock.c` 分秒选择、数字局刷、秒快照；新增验收，更新旧分钟断言，3 条英文、6 张截图 | `100% tests passed out of 5`；旧版反向失败；`gcccheck: 3 compiled, 0 with findings` |
| memo-save-busy | `storage_service.c` 及时申请、连续写入共用已有守卫；新增真实备忘压力验收 | `100% tests passed out of 3`；保存 3.010 秒超时→1.935 秒成功；性能未通过：mean 1.091→1.567 秒，p95 1.035→2.312 秒；部分完成 |
| cell-double-1 | 待复现和修复 | 未运行 |

最终检查：主工程普通全量、ASan、S31 宿主机、S31 固件、gcccheck、隐私检查均待执行。
