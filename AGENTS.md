# reader 协作约定

## 项目阶段与职责

- 先阅读 README.md、docs/requirements.md，以及与当前话题有关的想法记录和参考资料。
- 当前处于想法收集与可行性探索阶段。接受用户零散表达，不要求一次提交完整需求。
- 以中文交流。先回答当前想法是否可行、成立条件及主要限制，再按需要解释。
- 随用户后续指示调整阶段；在需求尚不清楚时，避免自行推进到详细架构、PCB 设计、完整固件开发或采购。
- 用户已经明确的要求、用户仍在考虑的想法、助手建议必须分开记录，不把建议自动升级为需求。
- 对可行性的判断使用“有依据可行 / 有条件可行 / 信息不足 / 当前约束下不建议”等清晰表达，说明理由，不给未经验证的承诺。
- 只追问当前判断所必需的少量问题。现有授权足够时直接完成资料整理和必要核查。
- 根据实际问题讨论算力、内存、存储、显示刷新、功耗、操作体验、软件兼容性和实现成本；不要机械地对每个想法列全套检查表。
- 引用硬件规格、上游功能及限制时记录来源与查阅日期。区分上游文档描述、代码核查、估算和实机测试，估算应写明假设。
- 上游提供某功能，不代表其在目标硬件版本上已被本项目验证。电脑端预处理、设备端执行和云端服务必须分清。
- 将新想法与结论维护在 docs/ideas.md；将明确需求维护在 docs/requirements.md；将外部调研维护在 docs/research/。保留发生变化的关键判断及原因。
- 需要临时分析或下载资料时使用 work/，正式资料按主题保存。仅文档修改做文件和链接检查，不创建无意义的测试或先行搭建完整工具链。

## 跨会话接续

- 新会话先查看仓库最新版本的 README.md、docs/project.md 和 docs/requirements.md，再按当前话题读取 docs/ideas.md 与对应调研；检查相关未结 Issue / PR，避免重复记录或覆盖其他会话的更新。
- docs/project.md 是持续协作入口。讨论完成后将有价值的原始想法、条件、结论与状态写入对应文件；只有远端写入成功并核对后，才报告“已保存”，并给出文件或提交链接。未来用户提供新想法时继续执行，不把持续协作理解为无人触发的定时研究。

## Codex and OpenCode hybrid routing

以下保留用户提供的协作路由要求：

- Keep the root agent on the built-in OpenAI provider and `gpt-5.6-sol`. The root owns requirements, architecture, implementation, debugging decisions, final synthesis, and acceptance.
- Use the overridden `worker` role for bounded command execution, environment inspection, initial log triage, and running existing tests, builds, or linters. A generic request to use a subagent for one of these tasks means `worker`, not `default`.
- The `worker` route is `hybrid-worker`: OpenCode Zen free DeepSeek V4 Flash first, official Codex OAuth next while available, and OpenCode Go paid DeepSeek V4 Flash last.
- Use `opencode_glm`, `opencode_k3`, or `opencode_luna` only when the user explicitly names that role or asks for an independent bounded advisory opinion. These roles must not implement product code or make final decisions.
- Do not use `opencode_opus48` until the OpenCode Zen balance is funded and a fresh end-to-end probe succeeds.
- Child roles inherit the active task's sandbox and approval settings. Do not override permissions, sandbox mode, or approval policy in a role.
- Require helpers to report the exact commands or files inspected, exit status, concise evidence, uncertainties, and whether state changed. The root verifies material claims.
- If a helper fails, times out, or returns ambiguous evidence, stop that route and continue once on the official root. Do not cascade through several paid models for the same task.
