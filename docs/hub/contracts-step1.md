# 第 1 步的两端约定

基线：`fdff779f2f2746ab10194e32a00b7dc343365e2c`（2026-11-05）。此文件冻结本任务的字段和保守默认值；不把待定建议升格成用户需求。来源为[概要设计 §10](hub-architecture.md#10-开发步骤和验收)“第 1 步验收前定”，以及[设备协议](device-protocol.md)。

| 项目 | 字段和规则 | 来源／实现边界 |
| --- | --- | --- |
| 服务基线：分页 | `tools/list {cursor?: string, domain?: string}` → `{tools: [...], nextCursor?: string}`。`cursor` 是生产函数给的字符串，不假设页大小。定义是当前页面、当前成员、对 AI 开放的视图。中枢 `caps/list` 的实际 JSON-RPC 信封按 ≤110 词法单元分页，绝对上限 128；`rev` 为目录代次。本步 `rev=1` | 概要 §7.3、§7.4；协议 §1.1、§3.4。设备仍只读 caps 第一页，优先放 `voice.chat`、`asr.stream` |
| 服务基线：回执 | 中枢 → 设备 `tools/call`，设备返回扁平 `{status, say?, page_gen?, result?, ...}`；反向调用返回 `{content:[{type:"text",text}], structuredContent:{status,say?,result?}, isError}`。查询结果只在 `structuredContent.result`；大结果分页或对象引用，不能超过 128 | 概要 §7.4；协议 §3.12。当前基线已保留 result，不改设备 |
| 能力可用集合 | 定义字典键 `(fw, name, version)`，`manifest_hash` 变化清字典。每轮 `page.actions` 非空时取其中已知定义；未知名或空数组重新拉完整当前视图；**空数组是“不知道”**。回执 `page_gen` 变化后下一步重新发现。模型可用工具再与成员规则求交集；系统设置及家长确认工具不给开发儿童身份 | 概要 §7.4；协议 §3.14。131 条清单由当前固件运行时导出；数目会随固件变，不写死 |
| Scope | `{family, actor, member, permission_version}` 只由服务端 Authority 产生；每次数据访问和执行都复核。家庭独立 SQLite 文件，成员作为数据分区；成人不能看另一成人；明确家长关系才能看孩子。`permission_version` 绑定发起者与目标两者版本 | 概要 §5.4；本步只用两家三人的合成测试；不提供账号接口 |
| 成员代次 | 连接有服务端 `connection` 随机编号；`ink/member {member, authed, epoch}` 的 `epoch` 必须递增。实时输出绑定 `(connection, member, epoch, sid, turn)`，出队时再次核对；切换取消旧输出。交付 `tools/call` 带 `_meta.member`、`_meta.epoch`。开发 proto1 固定合成儿童，不能靠自报 `authed` 进入成人 | 概要 §5.4；协议 §3.3。设备尚不校验 `_meta`，正式接入需改设备 |
| 确认的持久身份 | `token` 跨重启不复用，绑定 `{family, member, epoch, cap, digest, target_version, expires_at}`；`digest=sha256(UTF-8 JSON)`，JSON 键排序、无额外空格、不转义中文、拒绝 NaN。卡片正文必须来自同一份参数；断线、换人、目标版本变化或到期即失效，一次性消费 | 概要 §7.5；协议 §3.7。**待定**：token 的最终线类型与编码；保守默认把它当不透明值，中枢不生成设备 token。本步不发确认卡 |
| 实时输出与持久结果 | 实时输出换代次后丢弃。持久结果保留发起 Scope 和 `audience`，不绑定发起时 epoch；交付时复核目标当前权限、当前成员及成人已解锁，才填当前 epoch。外发队列只保存通知/显示，不重放副作用 | 概要 §5.4、§6.8；协议 §3.13。本步有 Scope/队列框架，不发送持久结果或实现外发服务 |
| 时间预算 | `ai.listen/ai.ask.budget_ms` 为实际发送时剩余毫秒；缺省 30000，减 3000 余量，最多按设备 45000 计算。从中枢收到请求时开始本地 deadline，含识别、发现和工具往返。最终 result 在全部朗读帧和 `ai.tts.end` 之后；单条 stt/delta ≤511 UTF-8 字节，回答文字总计 ≤32 KB（超出在整字处截断并附上和设备同一句提示，2026-11-10 audit12-fix），朗读按 1800 字节算，stt 只发一条。预填 5×60ms，其后按实时发 | 概要 §5.3；协议 §2、§3.15。设备还没发 budget_ms，本步依赖缺省值；完整播放暂停和发送预算在第 2a 步 |
| 动作范围 | `{scope, connection, epoch, action, target, materials[], expires_at, arguments_digest}`。策略代码逐项精确比较；资料只能取授权集合子集；参数规范化摘要必须相同，过期或权限变化拒绝。高风险不因有范围而自动通过；`_meta.asked` 只由策略代码填，模型不能自报 | 概要 §7.5；协议 §3.3。**待定**：定范围模型的输出格式、语义识别和收件人解析；保守默认没有范围就拒绝对外动作与系统设置。本步没有定范围模型、短信或真实外发 |
| 受限执行收到什么 | 仅人的原话、`materials` 点名资料及授权动作工具。没有隐式记忆、历史或听写；模型输出不得扩大 `target`、参数或资料集。网页和收到的消息只算资料，不能产生授权 | 概要 §7.5。`restricted_context` 是确定性框架；本步没有接入私人资料 |
| 自动待执行动作与结果 | 持久 `job.id` 随机 UUID；幂等键由调用者给，按 `(family, actor, member, key)` 唯一，复用时必须同参数摘要。状态 `queued/running/external/unknown/done/cancelling/cancelled`；记录 attempts、lease、Scope、kind、payload、effect、result。运行安全任务租约过期可重排；发出外部请求前落 `external`，重启成 `unknown`，不重试。`job.status {id}` 按 Scope 查状态 | 概要 §5.10、§7.5。任务 API 和合成执行器可用；自动设备动作尚未发出，不把任务状态冒充设备确认状态 |
| 自动设备动作的查询 | `tools/call._meta.request_id` ≤39 字节，交付时带 member/epoch；短信、电话、分享 `undo_ms=3000`，从设备显示开始计时；这一轮不够 3 秒就拒绝，不缩短。按 request_id 查 `confirm.status`，状态不明就是 unknown，无自动重发 | 概要 §7.5；协议 §3.3、§3.7。**待定**：`confirm.status` 支持 request_id 的最终参数键；保守默认结果不明不重试。第 1 步不发这类动作 |

## 要设备改的

- 第 2a／3 步：真正的成员代次、交付成员检查、成人解锁证明、D-105 的 asked／undo 窗口，和 `budget_ms`。
- 确认 token 的持久生成、执行前落盘、最近至少 16 个结果、`confirm.status` 按 token／request_id 查询、事件 ACK 后清理。
- `caps/list` 全部分页读完后原子替换；actions_more 为可选优化，本步已兼容空数组。
- 本步不改 `system/`、`apps/`、`drivers/` 或协议；配对、同步、块传输和内容包仍按后续阶段冻结。

## 本步默认值与证据边界

仅本机、合成家庭、假厂商、proto1 显式开关；不会信任客户端自报家庭。网页设置仅用本机或 SSH 通道，真实 HTTPS／公钥核对放第 3 步。`worker` 只做合成哈希探针，不宣称是内容转换器。`/metrics` 本步返回开发测量 JSON（Prometheus 格式尚未实现）。这些是保守开发默认值，不修改原设计合同，也不说明真实语音、部署镜像或真实家庭可用；验证范围见[报告](../tasks/hub-step1/REPORT.md)。

## 对话记录、重播和照片附件（2026-11-11）

- 记录由设备的成员集合 `ai.history` 保存，最多 50 轮、正文合计 128 KiB，超限删最旧；单条沿用数据集合的 8 KiB 上限，超长回答按完整字符截断并注明。设备本地的 `ai.history.list/open/delete/clear` 只操作当前成员；清空必须由人确认。中枢不维护第二份历史。
- `ai.replay {stream,text}` 使用已有 `voice.chat` 会话，文字最多 32767 UTF-8 字节。合成中枢播放合成音频，不回送文字；真实中枢目前明确返回 `-32021 / unavailable`，不宣称真实语音已接入。设备离线拒绝重播，保留回答文字。
- `ai.ask`、`ai.listen` 可带 `attachments` 数组，至多一张：`{"id":1,"width":128,"height":128,"gray4":"0123..."}`。复用 `file.thumb` 的按行十六进制灰度编码，每字符一个像素（0 黑、f 白）；宽高均为正整数且不超过 512，`gray4` 必须恰有 `width*height` 个小写十六进制字符、最多 65536 字节。当前设备输出最长边 128，无放大，附件约 16 KiB 以下，不上传原图或真实文件路径。
- 相册通过 `photo.ask` 请求当前有权查看的缩略图，回调核对成员代次、页面代次、照片编号与卡内容修订；变化则丢弃，再由既有成员绑定的请求通道发送。按钮和 AI 使用同一能力。当前按钮发送固定问题“请解释这张图”。模拟中枢核对形状、大小、编码并计数 `hub.photo.valid/invalid`；真实中枢只校验并接收，假模型仍不具备视觉理解能力。
