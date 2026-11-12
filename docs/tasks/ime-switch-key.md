# ime-switch-key：切换中英文后马上能打字（第一个键不丢）

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md` 第 11 条。

现象（真机和模拟器都有，见 `docs/tasks/backlog.md` 的【待修】）：键盘点“中/英”切换后约 2 秒内按下的第一个键没有产生按键动作，4 秒后正常。切换后键盘连刷三次，最后一次刷新期间的那下点击丢了。设备自动测试的备忘脚本为此在切换后先等 4 秒（`tools/device/autotest/memo.script`）。

## 要做的

1. 先在模拟器里写出能稳定复现的测试：切换后 300 ms、1 秒、2 秒各按一个键，按键都要进入文本（原代码失败）。
2. 找到根因修掉，例如切换时整块键盘重建、按下落在已删除的旧对象上，或刷新期间输入被丢弃。照手机键盘：切换即时生效，切换后马上能打字。
3. 切换只刷新必要的一次（D-127 最少刷新），不再连刷三次。
4. 修好后把 `memo.script` 里切换后的等待改回 1 秒，`tests/device_autotest/sim.c` 里对应的检查照实调整。

## 范围和预算

- 只改：`system/ui/shell/keyboard.c`、必要时 `system/ui/textfield.c` 里和切换有关的几行、`tests/ime_keyboard1.*`、`tools/device/autotest/memo.script`、`tests/device_autotest/sim.c`、`docs/tasks/ime-switch-key/`。不碰别的任务在改的文件。
- 产品代码不超过 80 行；不新增机制。

## 验收

1. 测试：切换后 300 ms、1 s、2 s 的按键都进文本（中→英、英→中，全键和九键）；切换只刷新一次；反向对照（换回旧代码测试失败）。
2. 主工程普通全量 ctest 过（含 `ime-*`、`ui-scale`、`autotest-*`、`ui-snapshot-regressions-*`）。
