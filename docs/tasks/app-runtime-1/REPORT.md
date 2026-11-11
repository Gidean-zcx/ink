# app-runtime-1 执行报告

基线：`a108101cbb81bd4a0b4785d66f0ab3a4fcb32003`。当前状态：核心实现与宿主验收已跑通，正在收尾全量、反向对照和 S31/ASan 检查。未碰设备，未推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】正向 101 项检查、0 失败；七项反向均检出错误，且各自执行到结束 | `build/app-runtime-eag8e54h/`；反向汇总 `build/app-runtime-reverse.json`，孩子反向更新证据 `build/app-runtime-ll__u9y8/` | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/app_runtime/run.py --build build`；同命令加 `--mutant install/uninstall/bad/permission/snapshot/child/card` 分别执行 |
| 2 | 待验证 | 全量、ASan、能力表、S31、gcccheck、ui-scale、隐私 | 待补 |
| 3 | 待验证 | 格式文档、设计差异、截图 | 待补 |

## 没做到的

全量、ASan、S31 正在验证，最终结果待补。首轮全量为“97% tests passed, 11 tests failed out of 365”，其中部分在源文件更新期间运行，不能作为最终结论。修正空目录刷新、忽略索引事件并同步新能力的测试表后，七项相关回归全部通过（`build/Testing/Temporary/LastTestsFailed.log` 不作为最终证据）。

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

## 要改的文档

`docs/system/capabilities.md` 须由合并方重新生成：增加 `app.page_action`、`app.page_turn`。遵守总规则第 7 节，本分支不修改共享生成文档；已在 `build/app-runtime-capabilities.md` 生成并 `--check` 通过（532 项定义）。
