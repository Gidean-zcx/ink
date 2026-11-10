# audit10-fix 修复报告

基线：`63df2b557d4cc506e15a1357ecb67dbc57b5b7e1`。B10-02 已修复；B10-01 的后台订阅候选与原 `sms` 测试冲突，按任务书停止调整并撤回候选。全量检查执行中，不能当作全部验收通过。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| B10-01 | 未完成，按任务书停止 | 【实测】基线新测试失败；改回后台订阅后新测试通过，但原 `sms` 在 17.955 秒报 `capture is settled`；候选已撤回 | 下方专项命令 |
| B10-02 | 专项通过 | 【实测】`audit10-b10-02` 修前退出 1、修后退出 0；第二页快照与真实按钮一致，返回第一页动作恢复且实际点击进入 `wifi.forget` 的人工确认 | 下方专项命令 |
| 全量测试 | 未通过；仅 B10-01 行为失败 | 【实测】300 项已执行：首次 296 通过、1 失败、3 环境跳过；补齐中枢 Python 后 3 项及其依赖全部通过。原 `sms`、B10-02 均通过 | 下方全量及补跑命令 |
| 内存检查 | 未运行 | 待补充 | 待补充 |
| S31 编译 | 未运行 | 待补充 | 待补充 |
| 差异与隐私 | 阶段检查通过，收尾再查 | 【实测】指定基线至当前提交隐私扫描 0 命中，`git diff --check` 通过；短信产品源码无差异 | 任务规则指定扫描、`git diff --check` |

基线命令：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R '^(audit10-b10-0[12]|sms)$'`。CTest 退出 8；两个新测试均正常完成后报对应行为失败，原 `sms` 退出 0。日志：`work/audit10-fix/baseline-tests.log`。初次专项缺少生成的图标资源，补齐完整构建后才做上述基线比较。

## 修改与专项证据

- B10-02：【实现】`system/ui/shell/navigation.c` 在原 `block(INK_CONTROL)` 分页计算后记下按钮页码，快照沿用该结果；不另算字号或页面高度。产品代码增加 3 行、删除 2 行，净增 1 行；一个页码字段占 PSRAM 4 B，无动态分配。实际按钮、AI 都仍走 `wifi.forget` 及既有确认规则。
- B10-01：【实现】只尝试将 `system/apps/sms/sms.c` 的 `message.received` 订阅从 `INK_EXEC_DIRECT` 恢复为 `INK_EXEC_BG`。通信服务先注册后台入库回调，执行器按投递顺序执行，因此页面回调会在入库回调完成后投递 UI 刷新。
- 【实测】候选构建后运行同一专项命令，两个新增测试通过；原 `sms` 的 `write` 阶段 118 个检查通过、1 个失败，17.955 秒的截图检查失败。日志 `work/audit10-fix/fixed-tests.log`；原始指标和图片 `work/audit10-fix/sms-bg-write/`、新短信通过证据 `work/audit10-fix/sms-bg-arrival/`。
- 【实现】原 `sms` 在收到 17 条消息后再次 `open_list()`，然后等 `ink_ui_busy()` / `ink_display_busy()` 为空闲，再于 1 ms 后截图；后台刷新可能随后才投递，空闲检查不覆盖尚未投递到 UI 的后台回调。【实测】上述候选会在这一检查处失败；未修改原测试、阈值或增加等待。这里只证明该最小候选有冲突，没有证明所有入库完成通知方案均不可行。按任务书要求在此停止，最终短信产品源码与基线相同；新增回归保持注册和原断言，仍应报错。
- B10-02 反向对照：【实测】保留修前编译的同一测试二进制，修后再次运行 `INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/audit10_fix/run.py --build build --case b10-02 --binary work/audit10-fix/baseline-b10-02`，退出 1，23 个检查通过、1 个失败，仍唯一报第二页的隐藏动作。日志 `work/audit10-fix/wifi-reverse.log`。默认 CTest 不依赖 Git 历史。

## 全量检查命令与证据

- 【实测】普通配置按任务规则，`taskpolicy -c utility cmake --build build -j2` 退出 0；日志 `work/audit10-fix/build-final.log`。
- 【实测】`INK_KEEP_EVIDENCE=1 PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2` 退出 8，215.20 秒，唯一失败 `audit10-b10-01`；日志 `work/audit10-fix/ctest.log`。
- 【实测】首次默认 Python 缺少中枢依赖，已有 3 项测试返回 77。使用现成只读依赖环境，`taskpolicy -c utility cmake -S . -B build -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"` 后，以上测试命令增加 `-R '^(hub-contract|hub-json-crosscheck|hub-simulator-e2e)$'`，4/4（含导出前置项）通过、无跳过，退出 0、18.78 秒；日志 `work/audit10-fix/ctest-hub.log`。未安装或升级依赖；后续 ASan 使用同一配置。

## 没做到的

- B10-01 未交付可验收修复，原因和数据见上；不通过改旧测试、加等待或再加机制绕过冲突。
- 全量测试因 B10-01 未通过；内存检查和 S31 编译待完成。未接触设备或实网业务。

## 和设计不一致的

- B10-01 的页面及 AI 快照仍不能及时反映入库消息；不修改设计迁就这一行为。

## 改过的已有测试

- 无。

## 新增的机制

- 无。

## 要用户定的

- 无。

## 要改的文档

- 主会话在 backlog / UI 依赖中记录 B10-02 已修、B10-01 未完成；`index-compact/REVIEW.md` 中短信订阅修复的旧结论仍需注明 B10-01。共享文档未改。

## 顺手发现

- 无。
