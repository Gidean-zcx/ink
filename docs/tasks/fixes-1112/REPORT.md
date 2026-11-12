# fixes-1112 汇总

基线：`6171c2bf8d59238c95f2d548663f8b2ab3c6f43a`。执行中；未烧录、未连接串口、未推送。

| 任务 | 做了什么 / 文件 | 测试结果原话 |
| --- | --- | --- |
| ime-switch-key | 保留键盘浮层，更新映射；`keyboard.c`、`ime_keyboard1.*`、`memo.script`、`device_autotest/sim.c` | `ime-keyboard1` 与四项 `autotest-*` Passed；旧版反向对照 5 条断言失败；旧版丢字未稳定复现，详见分报告 |
| timer-seconds | `clock.c` 分秒选择、数字局刷、秒快照；新增验收，更新旧分钟断言，3 条英文、6 张截图 | `100% tests passed out of 5`；旧版反向失败；`gcccheck: 3 compiled, 0 with findings` |
| memo-save-busy | 没做完：三方案后撤回产品代码；保留复现测试和存储隔离对照 | 方案曾保存 1.935 秒，但延迟回退，且引入 3 项回归；原存储对照三项 exit=0，最终保留失败复现 |
| cell-double-1 | `cell_host.cc` 测试逻辑时钟；两处 CMake 正向登记允许 `-j2`；新增有界压力脚本 | 修改后单轮 `100% tests passed out of 9`；旧版满载 0/122 失败、未复现；满载 270/270、0 失败，各 30 次，`100% tests passed out of 9` |

最终检查：主工程构建通过；普通全量运行待执行。ASan 构建中；S31 宿主机全量、固件、最终 gcccheck 和隐私检查待执行。
