# member-quiesce：换成员时先收尾在途的调用、事件和上传

总规则先读 docs/tasks/codex-rules.md，第 7 节优先。报告写到 docs/tasks/member-quiesce/REPORT.md。

## 要做的（docs/tasks/backlog.md 原文）

预算 150 行产品代码。services-finish 的 R6-01、R6-02 和 device-events 的 DE-R5 是同一个根因：换成员时还有在途调用、事件和上传。**不要逐个服务传递身份。** 在换成员这一处集中解决：切换时先不接新调用，等在途能力调用和排队的事件、上传结束（有超时，超时的取消并如实回执），再切换。已按成员绑定的长作业（文件、下载）不动。验收：这三条的复现不再成立；换成员的额外时延【实测】写进报告；列出哪些逐服务的代次检查因此可以删（只列，不删）。

## 从哪里看起

- 换成员：system/policy/members.c 的 `ink_member_switch`。凡是改 `M.cur`、调 `invalidate_identity` 的地方都算换人（`close_space`、解锁后换人的路径等），都要走同一处收尾。
- 能力调用：system/policy/cap.c 的 `prepare`、`start`、`finish`、`settle`；在途的调用在 `CB.pool`（`pending_t`）。
- 事件：`ink_cap_emit` / `ink_cap_emit_for`、`keep_event`、`event_job`；发往中枢的事件在 `EV[]`。
- 上传：AI 提问在 system/services/ai/ai_service.c 约 520–540 行拼页面文字和状态，经 `ink_hub_call_for` 发出。
- 三条复现：docs/tasks/services-finish/REVIEW.md 的 R6-01、R6-02（文件末尾的探针脚本，`REVIEW_MODES='data alarm reminder cancel'` 和 `REVIEW_MODES=events`）；docs/tasks/device-events/REVIEW.md 的 DE-R5（文件末尾的脚本）。先把它们写成 tests/ 里的回归测试，确认在基线上失败，再改。

## 边界

- 只在换成员这一处加“先停收、等收尾、再切换”。用已有的锁和执行器，不新建常驻任务。
- 等待有上限，用能力 QoS 里已有的超时，不另起一套。超时的调用取消，回执如实写取消，不假装成功。
- 文件、下载这类已按成员绑定的长作业不动。
- 各服务里已有的代次检查一行不删，只在报告里列出哪些因此可以删。
- 同时在跑的还有 speech-cache（只改 audio 服务）、file-index-1（文件服务）、ui-semantics（system/ui）。不要改它们的文件。

## 验收

1. 三条复现都写成测试：基线上失败，改后通过；撤掉这处改动，测试重新失败。
2. 换成员的额外时延：在模拟器里实测，有在途调用和没有时各测一次，写进报告。
3. 报告里列出可以删的逐服务代次检查（文件和行号）。
4. 全量测试和内存检查通过；产品代码不超过 150 行。
