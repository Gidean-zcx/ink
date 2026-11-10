# cell-features-merge 执行报告

基线：`ac18303cf0dcf0459a5a8f7c7d57f6b17ddc193a`。本地分支 `claude/codex-cell-features-merge`；仅编译、宿主机测试和本地提交，不操作设备、不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 功能合并 | 进行中 | 移动网络已合入并编过；保留主线 80 个订阅槽和短信参数契约 | `taskpolicy -c utility cmake --build build -j2` |
| 1 开机延迟 | 【实测】已复现，定位中 | 基线成员就绪 2417.653 ms，合移动网络后 5056.334 ms；初始化均约 2107 ms；前者中枢通过、后者 `tool_calls=0` | `tests/hub/e2e.py`，证据在 `work/cell-features-merge/{baseline,mobile}-e2e.log` |
| 1 普通全量与原分支测试 | 待验证 | 待补 | 待补 |
| 1 ASan/UBSan 全量 | 待验证 | 待补 | 待补 |
| 2 S31、GCC、隐私 | 待验证 | 待补 | 待补 |
| 3 能力清单与差异 | 待验证 | 待补 | 待补 |

## 没做到的

- 执行中，尚未作完成结论。

## 和设计不一致的

- 待核对；不修改设计合同迁就实现。

## 改过的已有测试

- `ai_surface.c` 迁入移动网络项，`sms.send` 必填参数保留主线的 `to`，未迁入归档的 `to,text` 变更。

## 要用户定的

- 暂无新增产品决定。

## 顺手发现

- 两个原分支均曾把 `sms.send` 必填预期改成 `to,text`；本任务按当前主线合同保留 `to`。

## 新增的机制

- 待按最终差异记录；不自行新增机制。

## 要改的文档

- 按任务授权重新生成 `docs/system/capabilities.md`；其余禁改共享文档不动。
