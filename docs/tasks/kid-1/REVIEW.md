# kid-1 第 1 轮复查

复查日期：2026-11-09。基线：`3fce2beea07d9e25a814cd24c64e8d5305ea9222`；被查提交：`da704af24f134086027056c6431955134032d645`。

本轮独立核查任务书五项验收、总规则与设计约束，只修改本报告，不提交。构建、测试与图像证据留在忽略的构建目录。

## 复查进度

- 已读任务书、总规则、开发报告及产品改动；正在独立复跑测试、核对至少十屏和检查真实交互边界。
- 开发报告承认忘记 PIN 后重置未接通、四屏视觉超标及能力表文档校验失败；这些暂不当作独立实测结果。
- 普通构建通过；普通全量独立实测 232/233，唯一失败 `capability-table-doc`，退出 8，耗时 139.57 秒。日志：`build/kid-1-review-full.log`。

## 已确认问题

### K1-R1｜高｜旧 PIN 表单跨锁屏与换成员保留，孩子能改成人密码并进入成人空间

- 文件：`system/ui/shell/member_pages.c:40–47,161–170,209–219`；`system/ui/shell/member_gate.c:94–103`。
- 复现：大人设置并修改 PIN → 再打开修改页 → 输入正确旧 PIN，停在新 PIN 步骤 → 电源键锁屏 → 121 秒后唤醒 → 在成员门点击孩子 → 在重新露出的成人新 PIN 页输入孩子自己选的 PIN → 换到成人并输入这个新 PIN。
- 本轮命令：`INK_KID_PHASE=review_lock taskpolicy -c utility build/kid-1-review-extras/sim-review --profile simulator/profiles/metalio-plus.json --script build/kid-1-review-extras/review.script --budget tests/ui_scene.budget.json --assets build/assets --content build/test-content --out build/kid-1-review-extras/lock --quiet --no-random-outages`。
- 【实测】退出 1，测试走到结束；`members.pin_set=3`（预期仍是大人的两次设置）、`members.unlocked=1`，最终当前成员是已解锁成人。正常电源键、虚拟时间和触摸操作，无阻塞线程。证据：`build/kid-1-review-extras/lock/metrics.json` 与 `driver.c`。
- 根因：新页面没有随身份失效清理，`P.old` 在进入新密码步骤后一直保留；`shell.pin_submit` 对孩子和锁定状态开放，提交时不核对表单原成员和当前身份。旧成员门被关闭后，新页面独立的根节点留在屏上。
- 最小修法：在本页面集中处理成员/认证变化，关闭并清除过期表单和凭据；提交前拒绝失效的修改会话。复用已有身份与事件接口，不改 PIN 策略，不逐调用方补丁。补上上述真实触摸回归。
- 定级依据：需要大人在改密码第二步离开并跨过锁屏宽限，是现实操作时序导致的成员隔离失效，按复查提示定高。现有 `kid-1` 没覆盖这个序列。复用断言另外报出的 8 次“快照”失败是换成员后快照被正确拒绝为空，**不是 PIN 泄露**。

### K1-R2｜中｜等待结束后仍显示“还要等 30 秒”，与实际可输入状态相反

- 文件：`system/ui/shell/member_pages.c:73–79,123–127,143–155,163`；测试遗漏在 `tests/kid_1.c:112–125`。
- 复现：连续三次错误进入 `PinWait`，不再点击“确定”，等待 31 秒，再点一个数字键。
- 本轮命令：将 K1-R1 命令中的 `INK_KID_PHASE` 改成 `review_wait`，输出目录改成 `build/kid-1-review-extras/wait`。
- 【实测】`remaining()==0`，可以输入数字，但可见消息仍是“错太多次了，还要等 30 秒”；两个显示断言失败，进程退出 1，`test.finished=1`、`test.pass=402`。证据：`build/kid-1-review-extras/wait/metrics.json`。
- 根因：剩余时间只在 `draw()` 读取，数字键只刷新圆点，期限结束没有更新；原测试检查内部时钟与 AI 快照，没有检查屏幕消息。
- 最小修法：复用现有调度，在到期时刷新本页一次并恢复普通输入提示；不做每秒倒数，不新增常驻任务。补上到期后的可见文本断言。

## 反向对照

本轮自己执行 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/kid_1.py --build build --mutant <名称>`，五个变体均编译、链接成功，随后由功能断言失败退出 1：unlock（verify，3 条）、set（write，11 条）、parent（recover，1 条）、privacy（write，3 条）、locked（verify，2 条）。日志：`build/kid-1-review-mutants.log`。原有断言不是空跑，但未覆盖 K1-R1 的会话交接和 K1-R2 的到期画面。

## 过度设计

待完成代码与预算核查后填写。

SEVERE=0 HIGH=1
