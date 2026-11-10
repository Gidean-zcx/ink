# cell-features-merge 执行报告

基线：`ac18303cf0dcf0459a5a8f7c7d57f6b17ddc193a`。本地分支 `claude/codex-cell-features-merge`；仅编译、宿主机测试和本地提交，不操作设备、不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 功能合并 | 【实测】专项通过 | 两个归档已合入；移动网络、紧急呼叫、AI 契约 3/3 通过 | `ctest --test-dir build -R '^(mobile-network|emergency-call|integration-ai-surface)$' --output-on-failure -j2` |
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
- 【实测】完整合并后中枢通过，成员就绪 2417.671 ms。撤销归档紧急分支的显示上电互斥两行，成员就绪恢复为 5056.327 ms。继续核查真实阻塞路径，不能仅凭完整合并通过判定根因已修。
- 【实测】无时间收费的临时 trace 把阻塞定位到 `member.add` 内首次打开 `member.retired`：三个文件的大小查询分别等待约 901、893、763 ms，期间各有一次整屏刷新。`storage.wait_for_idle_ms` 含这期间服务读取的耗时，不等于全程在后台空闲条件里轮询。

## 新增的机制

- 待按最终差异记录；不自行新增机制。

## 要改的文档

- 按任务授权重新生成 `docs/system/capabilities.md`；其余禁改共享文档不动。
