# update-pages 执行报告

基线：`3fce2beea07d9e25a814cd24c64e8d5305ea9222`。分支：`claude/codex-update-pages`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | `flow`、`done`：真实模拟下载、人工确认、写槽、重启、自检和完成页 | T |
| 2 | 【实测】通过 | 23% 阻止，充电恢复；注入已发布的剩余空间不足；坏签名重试；新版本自检失败后回滚 | T |
| 3 | 【实测】通过 | 七页快照含版本、说明和能力；AI 无安装能力；12 阶段共 218 条断言，内部 RAM 最少 37968 字节 | T |
| 4 | 待验证 | 全量测试、内存检查、300 行预算 | 待补 |
| G4 | 【实测】超过目标 | 先看七张画板；七页对照为 20.421～34.514/255，未达到 12/255 | V |

## 没做到的

最终全量与内存检查执行中。新增 12 阶段测试和反向对照通过：单独链接基线呈现器后，测试退出 1，触发 6 条断言失败，且执行到测试结束。首次普通全量 233 项中 227 通过、3 失败、3 跳过；旧文案和能力清单期望已更新并定向复测 2/2 通过，缺依赖的中枢测试已选用现成锁定环境，等待最终全量。

## 和设计不一致的

服务没有更新说明和“今晚再装”接口：说明如实显示暂不能查看，按钮为“以后再装”，只返回、不调度。沿用现有人工确认，未增加密码流程。

## 改过的已有测试

1. `tests/integration/update.c`：旧安装文案改为 `UpdateInstall` 和“正在安装，不要关机”；保留无按钮断言，新增 AI 动作为空断言。
2. `tests/integration/ai_surface.c`：严格白名单追加 `shell.update`、`shell.update_close`，仍要求每个注册能力恰好出现在一张表中。未删除测试、跳过测试、放宽预算或断言。

## 要用户定的

暂无。

## 顺手发现

暂无。

## 新增的机制

暂无；任务要求七个升级页面，不新增常驻任务、文件格式或配置。

## 要改的文档

任务规则禁止改共享文档；主会话需更新能力表中的 `shell.update`、`shell.update_close`，以及 `ui-dependencies.md` 的升级页面状态。

## 为什么不能更小

七页共用一个呈现函数、现有主题、浮层和控件语义；删除旧安装占位浮层。更小的仅更换安装卡方案无法覆盖阻止、失败重试和重启结果。服务只追加读取版本、包大小和下载任务编号的接口，未改校验、签名或回滚逻辑。

## 复现命令

以下命令均从仓库根目录执行，依赖路径使用用户给定环境变量。

```sh
# T：12 阶段及反向对照，保留本地证据。
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/update_pages.py --build build
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/update_pages_reverse.py --build build
# V：已验证的七张实际面板截图。
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/test-out/update-pages --reuse --scenes Update UpdateReady UpdateBlocked UpdateFail UpdateInstall UpdateDone BootRecovery
```
