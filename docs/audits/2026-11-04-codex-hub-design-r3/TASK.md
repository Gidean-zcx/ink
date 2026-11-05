# 设计复核任务说明（2026-11-04，家庭中枢设计第三轮，小范围，交给 Codex）

你是独立的审查方，做的是本项目（用户自己一家用的 AI 墨水屏设备）服务器端的**软件设计复核**。上一轮（`docs/audits/2026-11-04-codex-hub-design-r2/`，对象 a740294，设计 v0.2）查出 10 条新问题：严重 3、高 7。设计已经改到 v0.3，处理写在 `docs/audits/README.md` 的“家庭中枢设计复核（第二轮）：处理”。之后用户在摘要页选了 5 项决策（服务器放日本且已有一台、模型和语音混搭、费用上限、远程画面进首版、备份放另一家云），设计又改到 v0.4。

这一轮**范围要小**，只看这次改动：

1. **核实处理**：HUB2-001～010 逐条判断（已解决 / 部分解决 / 未解决 / 改出了新问题），说明依据。上一轮标为“部分解决”的 HUB-002、005、006、008、009、010、011、012、014、018，它们的剩余部分就是这 10 条，一并判断。
2. **这次改动有没有带来新的严重或高问题**：只看 `git diff a740294 HEAD -- docs/hub docs/decisions/README.md` 涉及的内容，和它们直接依赖的设备代码。比如：核对码、待生效令牌、安全日志、`hub_instance` 和 `dataset`、`sync/status` 的判断表、对账会话、`actions` 空数组、`confirm.status`、实时输出和持久结果、`budget_ms` 和发送节奏、匿名和已登录上下文。
3. **用户的选择写得对不对**：对照 `docs/decisions/README.md` 第九轮的 D-74、D-78、D-79、D-85、D-87、D-94 和 `docs/hub/hub-architecture.md` 的 §0、§1.2、§4.1、§4.3、§4.7、§5.2、§5.3、§5.7、§10、§12。看：跨境时延的算术和假设站不站得住；放日本之后设计里还有没有按大陆写、现在不成立的地方；第 0 步量线路够不够回答“这条线行不行”；第 6 步和第 4、5 步并行有没有依赖问题；DeepSeek 和百炼的文档说法准不准（可以查官方文档，不注册、不调接口）。
4. **能不能作为第 1 步的开发依据**：还有什么必须先定。

不要重审别的部分。**只报严重和高**；中、低级如果顺手看到可以记一句，不要去找。定级同前：**严重**是照此开发主要功能做不出来，或者会丢失、泄露家庭数据；**高**是要大改设计或大量返工，或者明显达不到用户的要求。

不要把“还没实现、还没实测”本身报成问题。这是设计阶段。不要为了凑数报设计细节，开发时定了也不会返工的，不算高。

## 1. 先读

1. `AGENTS.md`（末尾的 hybrid routing 是路由说明，本次不要调用任何子代理或外部模型，你自己完成）。
2. `docs/audits/2026-11-04-codex-hub-design-r2/report.md`、`docs/audits/README.md` 的“家庭中枢设计复核（第二轮）：处理”。
3. `git diff a740294 HEAD -- docs/hub docs/decisions/README.md`，再按需看 `docs/hub/hub-architecture.md`、`docs/hub/device-protocol.md` 全文。
4. 设备代码按需看（`system/services/hub/`、`system/policy/cap.c`、`system/services/ai/ai_service.c`、`system/services/audio/audio_service.c`）。别的会话的分支用 `git show <分支>:<路径>` 读：`claude/system-services`（`system/policy/confirm.c`、`system/services/data/`、`system/include/ink/sys/data.h`）。

## 2. 规则

- **只读审查**：不改产品代码、测试、档案、现有文档；不提交、不推送、不建分支。
- **产出只写在** `docs/audits/2026-11-04-codex-hub-design-r3/`：`report.md`、`findings.json`、`verification.json`，需要时加 `experiments/`。单个文件不超过 1 MB。
- **边做边写**：现在就建好 `report.md` 的骨架；每查完一块就写进去。最后再写“一页结论”。
- 不接触设备，不读 `~/Documents/Workspace/ink-hw/`，不在产出里写 MAC、IMEI、ICCID、Wi-Fi 信息、令牌、密钥。不写中枢的实现代码（小实验脚本除外）。不查服务器价格，不注册、购买、调用付费接口。

## 3. 写法与格式

- 报告用中文，说人话；代码标识、命令、路径保持原样。
- 每条结论标明证据类型：**【复现】**、**【代码核查】**（`文件:行号`）、**【文档核查】**、**【推断】**。推断不得写成事实。
- `report.md`：
  1. 一页结论：还有没有严重或高；能不能作为第 1 步的开发依据；还必须先定什么。
  2. **处理核实**：`编号 | 结论 | 依据 | 说明`。
  3. **新问题**：编号 HUB3-001 起，每条含定级、标题、证据、位置、影响、建议。
  4. 结尾写明审查对象提交（`git log -1`）、看了什么、跑了什么、哪些没查。
- `findings.json`：只放新问题，数组，每项 `{"id","severity","area","title","evidence_type","locations":[...],"impact","recommendation"}`；`verification.json`：`[{"id","verdict","evidence","note"}]`。

## 4. 完成后

最后的回复里给出：处理核实的统计、新问题数量（按严重度）、报告路径。
