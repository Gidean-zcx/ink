# app-runtime-1 执行报告

基线：`a108101cbb81bd4a0b4785d66f0ab3a4fcb32003`。实现提交：`617679ec`（含 `d033eb64`）。当前状态：实现、正反对照和 S31 检查完成，ASan 全量运行中。未碰设备，未推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】正向 101 项检查、0 失败；七项反向均检出错误，且各自执行到结束 | `build/app-runtime-eag8e54h/`；反向汇总 `build/app-runtime-reverse.json`，孩子反向更新证据 `build/app-runtime-ll__u9y8/` | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/app_runtime/run.py --build build`；同命令加 `--mutant install/uninstall/bad/permission/snapshot/child/card` 分别执行 |
| 2.1 | 【实测】普通全量：99% tests passed, 2 tests failed out of 365；无跳过 | `work/app-runtime/ctest.log`，1116.17 秒；失败为能力共享文档和中枢端到端 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2` |
| 2.2 | ASan 全量运行中；S31 两次编译通过 | `work/app-runtime/validate.log`、`work/app-runtime/s31.log`、`build-s31/size.txt`、`build-s31/stack.txt` | `taskpolicy -c utility bash ports/esp32s31/build.sh`；ASan 同全量命令，目录换为 `build-asan` |
| 3 | 待验证 | 格式文档、设计差异、截图 | 待补 |

## 没做到的

普通全量的两项失败尚待收口：共享能力文档按规则未修改；中枢端到端的合成成员在 6.06 秒才就绪，脚本固定在 3 秒点按，工具调用数为 0。准备了撤去本任务产品改动的手动对照，待 ASan 全量结束后执行。ASan 最终结果待补。

## 和设计不一致的

只实现有界本地声明页格式，不宣称兼容 A2UI v0.9；不含脚本、长文草稿恢复或包签名。格式和资源上限见 `docs/system/app-package-format.md`。

## 改过的已有测试

`tests/integration/ai_surface.c` 的 `FOR_AI` 增加 `app.page_action`（必需参数 `id`）和 `app.page_turn`（必需参数 `direction`）；对应新增能力，不删除检查、不放宽断言。服务设计能力表同步增加这两项。

## 要用户定的

无。

## 顺手发现

无。

## 新增的机制

应用包清单和声明页 JSON：任务书明确要求。无脚本、额外任务或线程。

产品代码净增 491 行（含 CMake 和示例 JSON，排除测试和文档），未超过 900 行。超过 100 行的原因：必须同时覆盖包校验、可变目录、七种控件、能力参数和回执、权限及 AI 快照；只挂载目录的更小方案不能满足这些验收。实现复用了 JSON 解析、现有主题和输入、文件与确认服务。

## 要改的文档

`docs/system/capabilities.md` 须由合并方重新生成：增加 `app.page_action`、`app.page_turn`。遵守总规则第 7 节，本分支不修改共享生成文档；已在 `build/app-runtime-capabilities.md` 生成并 `--check` 通过（532 项定义）。
