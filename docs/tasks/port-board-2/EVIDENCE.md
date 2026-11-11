# 验证证据

所有构建均 `taskpolicy -c utility`、最多 `-j2`，ctest 最多 `-j2`；命令中的 `ctest` 使用 `$INK_DEPS/build-deps/pyenv/bin/ctest`。原始运行日志保存在本地忽略的 `work/port-board-2/`，不提交本机路径或构建产物。

## 宿主机与反向对照

```sh
cmake -S ports/esp32s31/tests -B build-s31-host -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-s31-host -j2
$INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build-s31-host --output-on-failure -j2
python3 ports/esp32s31/tests/vendor_negative.py --out build-s31-host/reverse
python3 ports/esp32s31/tests/vendor_sources.py --baseline 281dbe71
```

【实测】`100% tests passed out of 47`，退出 0。17 条反向对照：每条编译退出 0，运行均因断言失败退出 -6；没有把编译失败算作撤修成功。覆盖恢复脉冲、10 ms 拉伸、TCA STOP／共同持锁、回退读、Flash drain 原语、GPIO/UHCI 清理、P14 放电脉宽、P12 复位、10% 前光、横屏坐标、空闲补采、触摸 ID 重试、30 ms 消抖、备用地址、初始化失败清 INT、BOOT 双边沿、马达 PWM。

## 主工程普通全量及基线对照

按任务给定字体、Python、LVGL 路径配置，`INK_WERROR=ON`；普通构建退出 0。全量原话：

```text
99% tests passed, 1 tests failed out of 391
Total Test time (real) = 727.26 sec
The following tests did not run:
  186 - hub-contract (Skipped)
  187 - hub-json-crosscheck (Skipped)
  188 - hub-simulator-e2e (Skipped)
The following tests FAILED:
  244 - integration-races (Failed)
```

【实测】失败为 `tests/integration/races.c:120`：`it is in the inbox of who it is for (0)`。两次独立新输出目录对照：

```text
current exit=1 warnings=1
6.757s CHECK FAILED (tests/integration/races.c:120) it is in the inbox of who it is for (0)
baseline exit=1 warnings=1
6.757s CHECK FAILED (tests/integration/races.c:120) it is in the inbox of who it is for (0)
```

【实测】基线对照没有切换本分支：`git show 281dbe71:drivers/chips/<name>/<name>.c` 取 tca9555、ft6336u、sc7a20h 三个在主模拟器实际编入且本次修改的源文件，按 `build/compile_commands.json` 的原命令编译，替换临时 `libink_drivers.a` 中的同名对象，再按 `build/CMakeFiles/sim-races.dir/link.txt` 的原参数链接。其余对象、脚本、资源、预算不动。当前和基线均使用全新 `--out`，两者相同失败；没有修改失败断言、等待时长或通知产品代码。完整基线检出可直接用任务构建命令与 `ctest -R '^integration-races$'` 复核。

三项 skip 原因是所选 Python 缺少 Hub 锁定依赖。已发现 `$INK_DEPS/hub/.venv/bin/python` 可用，补跑结果待补。

## GCC、S31、内存检查

【实测】`ink-codex/tools/gcccheck.py build` 检查三个共享芯片源：`gcccheck: 3 compiled, 0 with findings`。同工具对 `build-s31-host` 中板级、I2C、IRQ 测试翻译单元和 `hal_device_s31.c`：`gcccheck: 4 compiled, 0 with findings`；这些翻译单元实际包含端口板级、输入、总线和启动清理代码。首次发现的加速度计 unused-value 已修正并重跑通过。

S31 编译、S31 宿主机 ASan/UBSan 结果待补。主工程 ASan 按 `vendor-common.md` 第 7 条不运行。
