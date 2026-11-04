# 设计复核任务说明（2026-11-04，家庭中枢设计第二轮，交给 Codex）

你是独立的审查方，做的是本项目（用户自己一家用的 AI 墨水屏设备）服务器端的**软件设计复核**。上一轮（`docs/audits/2026-11-04-codex-hub-design/`，对象 6d10d95）查出 25 条：严重 3、高 12、中 9、低 1。设计已经改到 v0.2，处理写在 `docs/audits/README.md` 的“家庭中枢设计审查：处理”。

目标：

1. **核实处理**：HUB-001～025 逐条判断（已解决 / 部分解决 / 未解决 / 改出了新问题），说明依据。
2. **改动有没有带来新的严重或高问题**：v0.2 新加的东西最值得查：配对的三条信任路径和两阶段提交、同代快照、数据代次和全量对账、确认记录、成员代次和 `audience`、一轮的边界和发送节奏倒推、定义字典、`worker` 容器。这些放在一起有没有矛盾，有没有走不通的路径。
3. **设计能不能作为开发依据**：现在开始按 §10 第 1 步开发，还有什么必须先定。

上一轮已经确认没问题、这轮没改的部分不必重审。重点是**严重**和**高**；中、低级可以记，但不要为了数量去找。定级同上一轮：**严重**是照此开发主要功能做不出来，或者会丢失、泄露家庭数据；**高**是要大改设计或大量返工，或者明显达不到用户的要求。

## 1. 先读

1. `AGENTS.md`（末尾的 hybrid routing 是路由说明，本次不要调用任何子代理或外部模型，你自己完成）。
2. `docs/audits/2026-11-04-codex-hub-design/report.md`（上一轮）、`docs/audits/README.md` 的“家庭中枢设计审查：处理”。
3. **审查对象**：`docs/hub/hub-architecture.md`、`docs/hub/device-protocol.md`、`docs/research/hub-server-survey.md`、`docs/decisions/README.md` 的“第九轮”（D-73～D-94）。
4. **依据**：`docs/requirements.md`、`docs/system/system-detailed-design.md`、`docs/system/foundation.md`。设备代码同上一轮（`system/services/hub/`、`system/policy/cap.c`、`system/services/ai/ai_service.c`、`system/services/audio/audio_service.c`、`system/apps/remote/remote.c`、`apps/reader/reader.c`、`ports/sim/hal/`）。
5. 别的会话正在做的，用 `git show <分支>:<路径>` 读：`claude/system-services`（设计引用的是提交 `a81c1b8`：`docs/system/system-services-design.md`，`system/policy/cap.c`、`system/policy/confirm.c`、`system/services/data/`）；`claude/system-layer`（D-65、R-056）。

## 2. 规则

- **只读审查**：不改产品代码、测试、档案、现有文档；不提交、不推送、不建分支。
- **产出只写在** `docs/audits/2026-11-04-codex-hub-design-r2/`：`report.md`、`findings.json`、`verification.json`，需要时加 `experiments/`。单个文件不超过 1 MB。
- **边做边写**：现在就建好 `report.md` 的骨架；每查完一块就写进去，运行可能被中途打断。最后再写“一页结论”。
- 不接触设备，不读 `~/Documents/Workspace/ink-hw/`，不在产出里写 MAC、IMEI、ICCID、Wi-Fi 信息、令牌、密钥。
- 不写中枢的实现代码（小实验脚本除外）。来源可以联网核对，但不要注册、购买、调用付费接口。服务器价格按用户要求不查。

## 3. 写法与格式

- 报告用中文，说人话；代码标识、命令、路径保持原样。
- 每条结论标明证据类型：**【复现】**、**【代码核查】**（`文件:行号`）、**【文档核查】**、**【推断】**。推断不得写成事实。
- `report.md`：
  1. 一页结论：还有没有严重或高；能不能作为第 1 步的开发依据；还必须先定什么。
  2. **处理核实**：`编号 | 结论 | 依据 | 说明`。
  3. **新问题**：编号 HUB2-001 起，每条含定级、标题、证据、位置、影响、建议。
  4. 结尾写明审查对象提交（`git log -1`）、看了什么、跑了什么、哪些没查。
- `findings.json`：只放新问题，数组，每项 `{"id","severity","area","title","evidence_type","locations":[...],"impact","recommendation"}`；`verification.json`：`[{"id","verdict","evidence","note"}]`。

## 4. 完成后

最后的回复里给出：处理核实的统计、新问题数量（按严重度）、最重要的几条、报告路径。
