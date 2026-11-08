# speech-cache：device-events 审计的 DE-R6（语音缓存）

总规则先读 docs/tasks/codex-rules.md，第 7 节优先。报告写到 docs/tasks/speech-cache/REPORT.md。

## 要做的

docs/tasks/device-events/REVIEW.md 里的 DE-R6（语音缓存，system/services/audio/audio_service.c 约 526、732 行）。预算 60 行产品代码。

先确认它在现在的代码上还能复现：

- 不能复现：报告里写清楚怎么确认的，交 `TASK_COMPLETE`，不改代码。
- 能复现：先写成失败的测试，再在根因那一处修。

## 边界

- member-quiesce 在同时改 system/policy/ 的换成员流程。这里只改 audio 服务和它的测试，不改 system/policy/。如果 DE-R6 的根因其实是“换成员时还有在途的东西”，只写报告，不修。

## 验收

1. 复现写成测试：基线失败，改后通过，撤掉修复又失败。
2. 全量测试和内存检查通过；产品代码不超过 60 行。
