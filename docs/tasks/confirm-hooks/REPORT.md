# confirm-hooks 执行报告

基线：`32cff5a9ee114315fd7cbd13a1453c7a4cbd8ff8`。工作分支：`claude/codex-confirm-hooks`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过【实测／实现】 | 策略不再出现指定能力名前缀或通信头文件；两处能力自行提供钩子 | `grep -nE '"call\.|"system\.|comm\.h' system/policy/confirm.c` 无输出，退出 1 |
| 2 | 通过【实测】 | 新增合成能力绑定上下文、补充说明、确认后核对绑定；失败回执保留原因且不入队。定向 6/6 通过；禁用钩子的独立二进制出现预期 5 处失败 | 见下方验证命令和证据 |
| 3 | 部分完成【实测】 | macOS 全量 298/298 通过；内存检查和 S31 编译待完成 | 见下方验证命令和证据 |

## 没做到的

ASan/UBSan 内存检查和 S31 编译待完成。

## 和设计不一致的

无。说明文字、D-105、家长密码与 AI 不能代确认的策略保持原样；只增加任务要求的可选钩子。

## 改过的已有测试

未修改任何已有断言、期望或预算；仅在 `tests/integration/confirm.c` 追加合成能力与检查。新增手动反向对照 `tests/confirm_hooks/reverse.py`，不依赖 Git 历史，也不修改产品源码或正式测试二进制。

## 要用户定的

暂无。

## 顺手发现

首次按默认 Python 跑全量时，3 项中枢测试因缺依赖自动跳过；改用现有 `INK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"` 后重跑，298/298 全部实际通过。未安装、升级或修改依赖。

## 新增的机制

`ink_cap_t.prepare_confirm`：本任务明确要求；参数绑定和说明复用现有参数表与 `ask` 占位符。不新增任务、状态机、持久文件格式或配置项。

## 要改的文档

已按任务更新 `docs/system/app-framework.md`，说明真实签名、执行位置、返回约定、私有字段处理与执行时复核。无需修改共享文档。

## 验证命令和证据

命令均用 `taskpolicy -c utility`；依赖通过用户提供的 `INK_DEPS` 读取，构建／测试最多 `-j2`，证据用 `INK_KEEP_EVIDENCE=1` 保留在忽略的构建目录。

- 【实测】按总规则配置 `build`，`cmake --build build -j2`：退出 0，日志 `build/build.log`。
- 【实测】`ctest --test-dir build --output-on-failure -j2 -R '^(integration-confirm|audit5-b5-01|audit7-b7-01|confirm-durable|confirm-pages|phone-calls)$'`：6/6 通过，退出 0，日志 `build/confirm-hooks-focused.log`。
- 【实测】配置额外选择现有 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`；`ctest --test-dir build --output-on-failure -j2`：298/298 通过、无跳过，退出 0，235.32 秒，日志 `build/confirm-hooks-full-with-hub.log`。
- 【实测】`"$INK_DEPS/build-deps/pyenv/bin/python" tests/confirm_hooks/reverse.py --build build`：脚本退出 0；独立二进制编译／链接退出 0、测试退出 1，5 处检查失败，包含说明、绑定、失败回执及不入队断言；证据 `build/confirm-hooks-reverse/summary.json` 与 `test.log`。
- 【实现】产品代码新增 36 行、删除 23 行，净增 13 行（含注释），低于 80 行预算；改动集中在能力定义、确认策略与两处提供者。
- 【实测】修改文档的 61 个本地链接全部存在；`git diff --check` 通过。当前基线差异隐私扫描无命中（匹配命令退出 1）。

## 过程记录

- 已读取总规则、任务书和项目接续材料；检出位于指定基线，工作树干净，无上次执行报告。
- 已完成钩子、迁移、使用说明和新增测试；主机构建、定向回归与反向对照通过。
- 已完成 macOS 全量 298 项回归（无跳过）；正在执行内存检查和 S31 编译。
