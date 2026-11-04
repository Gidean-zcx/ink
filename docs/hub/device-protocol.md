# 设备—中枢协议：现状和扩展

状态：**待用户审核**，配合[家庭中枢概要设计](hub-architecture.md) §7。这是中枢和设备两边开发时共同遵守的约定；假设备的契约测试按它写。v0.2、v0.3 按 Codex 的两轮设计审查（[第一轮](../audits/2026-11-04-codex-hub-design/report.md)、[第二轮](../audits/2026-11-04-codex-hub-design-r2/report.md)）改过。

对照的代码是提交 `9b3c24a`：`system/services/hub/hub_link.c`、`rt.c`，`system/policy/cap.c`，`system/services/ai/ai_service.c`，`system/apps/remote/remote.c`，`simulator/environment/sim_hub.c`、`sim_remote.c`。“系统服务”指分支 `claude/system-services`（提交 `a81c1b8`）上的[系统服务详细设计](https://github.com/Gidean-zcx/ink/blob/claude/system-services/docs/system/system-services-design.md)和代码，还没合入 main。

原则（用户要求）：**不推翻已有协议。** §1 的消息全部保留，语义不变；§3 只加，每一条都写明为什么。

## 1. 现状：实现里实际有的消息

连接：设备主动连 `wss://…/v1/device`（`system/boot/main.c:44`）。文本帧是 JSON-RPC 2.0，二进制帧见 §1.3。

### 1.1 控制消息

| 方法 | 方向 | 类型 | 参数 | 回答 |
| --- | --- | --- | --- | --- |
| `hello` | 设备 → 中枢 | 请求 | `{"proto":1,"device":"<档案名>","member":"default"}` | 任意 `result` 就上线；`error` 就退避重连，没有分类。系统服务分支读 `result.now_ms` 校时 |
| `caps/list` | 设备 → 中枢 | 请求 | `{}` | `{"tools":[{"name","title","form"}]}`；设备只读 `name`、`form`，只读第一页 |
| `caps/changed` | 中枢 → 设备 | 通知 | 不读 | — |
| `tools/list` | 中枢 → 设备 | 请求（整数 id） | 系统服务分支多了 `cursor`、`domain` | `{"tools":[{"name","title","effect","confirm","version"?,"form"?,"inputSchema","outputSchema"?}],"nextCursor"?}`。**只列当前页面上、当前成员能用、开放给 AI 的**（`cap.c:666`），不是全部定义；main 上限约 6 KiB |
| `tools/call` | 中枢 → 设备 | 请求（整数 id） | `{"name","arguments","page_gen"?,"_meta":{"request_id"?,"page_gen"?}}` | **扁平回执** `{"status","object"?,"revision"?,"undo_ms"?,"say"?,"detail"?,"page_gen"?,"result"?}`（`result` 在系统服务分支）。全部按 AI 来源处理（`cap.c:802`） |
| `tools/call` | 设备 → 中枢 | 请求 | `{"name","arguments","_meta":{"request_id","page_gen"}}` | **MCP `CallToolResult`**：`structuredContent` 里是回执字段，`isError` 为真算失败。main 上 `result` 被丢弃，系统服务分支保留（那边 `cap.c:590`） |
| `rt.open` | 设备 → 中枢 | 请求 | `{"sid","cap","params"}` | `result` 内容不校验；`error` 则设备判定会话被拒，错误码不交给拥有者（`rt.c:129`） |
| `rt.close` | 双向 | 通知 | `{"sid"}` | — |
| `rt.*`（其他） | 中枢 → 设备 | 通知 | `{"sid",…}` | 交给会话的 `on_control` |
| `ai.listen` | 设备 → 中枢 | 请求，**45 秒超时，从创建请求算，没联网也在计时** | `{"stream","format":"opus","rate":16000,"frame_ms":60,"page_gen","page":{"app","text","actions":[…]}}`；`actions` 放不下 768 字节时是 `[]` | 一轮结束时回答，如 `{"done":true,"source":"page"}`；内容不读 |
| `ai.ask` | 设备 → 中枢 | 请求，45 秒超时 | `{"question","page_gen","page":{…}}` | **回答里的文字被忽略**，文字只能走 `ai.delta` |
| `ai.listen.end` | 设备 → 中枢 | 通知 | `{"stream","frames"}` | — |
| `ai.stt` | 中枢 → 设备 | 通知 | `{"id","text"}`，`id` 是 `ai.listen` 的请求号；**替换显示**，多条只留最后一条 | — |
| `ai.delta` | 中枢 → 设备 | 通知 | `{"id","text","seq"?}`；追加，一轮总共 2048 字节，超了被忽略 | — |
| `ai.tts.end` | 中枢 → 设备 | 通知 | `{"id","stream","frames"}` | — |
| `device/event` | 设备 → 中枢 | 通知（系统服务分支） | `{"event","at_s","args"}`；断网时留最近 8 条 | — |

**顺序约定**：`ai.listen` 的回答必须在所有朗读帧和 `ai.tts.end` 之后；设备收到回答就关这一轮的会话，已收到的声音继续放完（详细设计 §6.5）。

### 1.2 一轮语音的真实流程

```text
设备  rt.open {sid:7, cap:"voice.chat", params:{}}
设备  ai.listen {stream:7, format:"opus", rate:16000, frame_ms:60, page_gen:12, page:{...}}   不等 rt.open 的回答
中枢  rt.open 的 result {sid:7}
设备  [kind=1 sid=7 seq=0..n] 上行语音帧                                                 确认之后才发，之前先缓存
设备  ai.listen.end {stream:7, frames:n}
中枢  ai.stt {id:<ai.listen 的 id>, text:"翻到下一页"}
中枢  tools/call {name:"reader.next_page", arguments:{}, page_gen:12, _meta:{request_id:"r1"}}   可多次
设备  result {status:"done", say:"已翻页", page_gen:13}
中枢  ai.delta {id, text:"好的。"}
中枢  [kind=2 sid=7 seq=0..m] 下行朗读帧
中枢  ai.tts.end {id, stream:7, frames:m}
中枢  ai.listen 的 result {done:true}
设备  rt.close {sid:7}
```

中枢要接受 `ai.listen` 比 `rt.open` 的回答先到（`ai_service.c:528`）。打断：设备本机停播，`rt.close` 旧会话，再 `rt.open` 新会话。**语音对话的代码里没有 `rt.turn`**（详细设计 §6.2 写了，实现没用）。

### 1.3 二进制帧

帧头 10 字节，小端（`rt.h`）：`[kind u8][sid u8][seq u16][t_ms u32][flags u8][0]`，每个 WebSocket 消息一帧。`flags`：位 0 单元结束，位 1 完整画面，位 2 可丢弃。`sid` 必须是开着的会话，否则丢弃（`rt.c:57`）。

| kind | 方向 | 负载 |
| --- | --- | --- |
| 1 语音上行 | 设备 → 中枢 | 一个 Opus 包，16 kHz 单声道，60 ms（模拟器是替身格式） |
| 2 语音下行 | 中枢 → 设备 | 同上。下行缓冲总共 512 帧（30.72 秒），满了新帧被丢弃；预缓冲 3 帧 |
| 3 画面区域 | 中枢 → 设备 | `[x u16][y u16][w u16][h u16][编码 u8][刷新提示 u8]` + 数据。编码：0 原始 4 位灰度，1 4 位灰度 + LZ4，2 灰度 JPEG（模拟器是替身）。提示：0 要快，1 要清楚，2 整屏刷新。4 位灰度高半字节在前，灰度 `(v + 8) / 17` |
| 4 输入 | 设备 → 中枢 | `[type u8]…`。**现在只实现了** `1` 点按 `[x u16][y u16]`；设计里还有 2 长按、3 翻页 `[dir i8]`、4 按键 `[key u8]`、5 提交文字，第 6 步要补 |
| 5 画面配额 | 设备 → 中枢 | `[credits u8][max_bytes u32]`，现在 `max_bytes` 是 1 MiB |

`browser.session` 的 `params`：`{"w","h","gray":16,"url"?}`，整段不超过 239 字节，URL 太长时不带（中枢用起始页）。**现在不带 `dpi`**。

## 2. 中枢必须遵守的限制

逐条来源见[概要设计 §1.3](hub-architecture.md)。假设备测试全部检查：

1. 发给设备的每条 JSON ≤ 128 词法单元（对象、数组、键、值各算一个）。Codex 用生产解析器实测过边界。**按序列化后的实际数量判断，协议内容按 110 留余量。**
2. `ai.delta`、`ai.stt` 单条 `text` ≤ 511 字节，按 UTF-8 字符边界切；**`ai.delta` 一轮总共 ≤ 1.8 KB**（设备 2048，留余量）；`ai.stt` 只发一条。
3. `ai.listen`、`ai.ask` 要在设备的 45 秒之内回答：按 `budget_ms` 减 3 秒；没有这个字段按 30 秒算（§3.15，[概要设计 §5.3](hub-architecture.md)）。不等人确认。朗读帧按实时发，领先 ≤ 3 秒，不突发。
4. 中枢发的请求，`id` 是整数。
5. 能力名：目录项 ≤ 39 字节；会话能力名 ≤ 23 字节；工具名 ≤ 47 字节。`request_id` ≤ 39 字节（按转义后的原文算）。
6. 方法名 ≤ 31 字节（设备发送上限；接收上限是 39，协议统一按 31）。设备按前缀订阅（≤ 15 字节）。
7. **不发应用层保活**。合法 JSON 和二进制都会刷新设备的空闲计时；保活用 WebSocket ping。
8. 每条新连接当作设备什么都没说过；同一设备的新连接到了，旧连接立刻关。
9. 画面帧只在有配额时发；一个配额一个单元；单元最后一帧带位 0；**传到一半的单元不能被新内容打断**。重连后 `shown` 算白屏。
10. 未认证的连接只接受 `hello`、`pair/*`，单条 ≤ 4 KB，10 秒内要有其中之一。
11. 地址 `urls` 里的每个 ≤ 127 字节（`H.url[128]`）。

## 3. 扩展

总表：

| 编号 | 内容 | 方向 | 不做会怎样 | 设备改动 |
| --- | --- | --- | --- | --- |
| 3.1 | `hello` 第 2 版 | 设备 → 中枢 | 没有身份，公网上线不了 | 必须 |
| 3.2 | 配对 `pair/*` | 双向 | 设备进不了家庭 | 必须，`hub_link` 要加状态 |
| 3.3 | `ink/member`、`members/list`、成员代次 | 双向 | 记忆分不了成员，旧输出串到新成员 | 必须 |
| 3.4 | `caps/list` 分页和更多字段 | 设备 → 中枢 | 放不下；离线策略到不了设备 | 超过一页时必须 |
| 3.5 | `rt.close` 带原因 | 设备 → 中枢 | 打断时历史截断不准 | 可选 |
| 3.6 | `device/event` 加序号 | 设备 → 中枢 | 事件丢了发现不了 | 必须 |
| 3.7 | `confirm/request` | 中枢 → 设备 | 中枢的高风险能力没法确认 | 做 `budget.set`、支付页上的点击时必须 |
| 3.8 | 块传输 `blob/*`、帧 kind 6、7 | 双向 | 书、音频、固件、备份文件传不了 | 必须 |
| 3.9 | 同步 `sync/*` | 设备 → 中枢 | 没有备份 | 必须，数据存储要加原子安装 |
| 3.10 | 升级 `update/*` | 设备 → 中枢 | 没有 OTA | 系统服务已定义，这里统一到块传输 |
| 3.11 | 听写 `asr.stream` | 双向 | 输入法没有语音输入（R-056） | 设备底层会话做；`rt.c` 不用改 |
| 3.12 | 回执里的 `result` | 中枢 → 设备 | 查询能力没有结果 | 系统服务分支已做 |
| 3.13 | 外发队列 | 中枢 → 设备 | 设备离线时的消息没地方放 | `_meta.member`、`_meta.epoch` 检查 |
| 3.14 | 定义字典和可用集合 | 中枢 | 工具不全或全丢 | 可选改进 `actions` |
| 3.15 | `budget_ms` | 设备 → 中枢 | 中枢算不准还剩多少时间 | 必须（没有时按 30 秒） |

### 3.1 `hello` 第 2 版

```json
{"jsonrpc":"2.0","id":1,"method":"hello","params":{
  "proto":2,
  "device":"dv_8f3a","token":"<令牌>",
  "hw":"metalio-e-ink4-plus","fw":"1.2.0","fw_code":1200,
  "manifest_hash":"9c1e…","frame_v":2,
  "member":"m_ba","authed":true,"member_epoch":17,
  "net":"wifi","caps_rev":17,"members_rev":4}}
```

回答：

```json
{"session":"s_01","proto":2,"now_ms":1793777018000,"hub_instance":"7c1e…",
 "caps_rev":18,"members_rev":4,"outbox":2,
 "pins":["sha256/…"],"urls":["wss://…:8443/v1/device"],"next_token":"…"}
```

- `now_ms`：校时，系统服务分支已经这样读。
- `pins`、`urls`、`next_token` 都可以没有。首版不发 `next_token`（不轮换令牌，审查 HUB4-004）。`pins` 有两个时表示换密钥的过渡期。
- `hub_instance`：中枢每次从备份恢复或导入时换一个新的随机值（§3.9）。`caps_rev`、`members_rev` 只在同一个 `hub_instance` 内比较，变了就整份重新取：恢复后修订号可能涨回同一个数，内容却不一样。
- `manifest_hash`：固件的能力定义变了才变，用来让中枢的能力字典失效（§3.14）。
- 错误码见 §4。`revoked` 时设备停止重连。现在任何错误都退避重试，要改。
- 中枢保存 `fw`、数据格式版本（底座设计 §10.3）。

**兼容**：`proto:1` 只在开发模式（配置 `dev_allow_proto1`）接受，映射到一个开发家庭，给现有模拟器场景用。公网部署默认拒绝。

### 3.2 配对

只在没有令牌时用；设备用同一个地址，不发 `hello`。

**设备连接的状态，持久保存**：`UNPAIRED`（有地址，没令牌）→ `PAIRING`（已 `pair/begin`，等认领）→ `COMMITTING`（用户确认了，等令牌或已存令牌但还没成功 `hello` 过，记着 `txid`）→ `BOUND`（第一次 `hello` 成功）→ `REVOKED`（收到吊销，停止重连）。现在 `hub_link` 一连上就发 `hello`，只有 ONLINE 才能发通知（`hub_link.c:318`、`:577`），所以这里要改。

| 消息 | 方向 | 内容 |
| --- | --- | --- |
| `pair/begin` 请求 | 设备 → 中枢 | `{"proto":2,"hw":"…","fw":"…","nonce":"f1a9…","txid"?:"…","preauth"?:"…"}` → `{"code":"K7QF2M9X","expires_s":600}` |
| `pair/offer` 请求（整数 id） | 中枢 → 设备 | 手机确认后发：`{"txid":"t1","family":"张家","by":"爸爸","name":"爸爸的设备"}`。设备立即回 `{"shown":true}`，显示确认卡 |
| `pair/accept` 请求 | 设备 → 中枢 | 用户在设备上点确认：`{"txid":"t1"}` → `{}`。点取消发 `pair/reject`，回到认领码 |
| `pair/complete` 请求（整数 id） | 中枢 → 设备 | `{"txid":"t1","device":"dv_8f3a","token":"…","urls":[…],"pins":[…],"members_rev":1,"now_ms":…}`。设备存进 NVS 后回 `{"ok":true}` |
| `pair/expired` 通知 | 中枢 → 设备 | 认领码过期，设备重新 `pair/begin` |

**认领网址由设备自己拼**：`https://<设备已知的控制台地址>/claim/<code>#s=<观察到的公钥 SHA-256 前 16 字节，base64url>`。中枢的回答里**没有网址**，防止第一次连接被替换时指向别人的网站（审查 HUB-008）。控制台地址来自配对链接或配对文件；只给了设备地址时，就是同一个主机的 443。

**电脑输入也要核对公钥**（审查 HUB2-001）：设备同时显示 `code` 和**核对码**（公钥 SHA-256 的前 60 位，base32，12 个字符，分组显示）。电脑上两个都要填。**控制台一律要求核对**（二维码的 `#s=` 或手输的核对码），不看设备说自己有没有固定公钥，那句话也可能被改。否则中间人可以转发真实认领码，让用户看到正确的家庭名，自己拿走令牌。

**信任规则**：有公钥（固件预置、配对文件、配对链接）就核对，不对就断开。没有公钥时只能走“手机打开用户给的地址，靠可信证书比对公钥”这一条，控制台没有可信证书就不能配对（[概要设计 §6.1](hub-architecture.md)）。

**等待**：`pair/begin` 之后不受 10 秒限制，最多等 10 分钟，WebSocket ping 保活。断线后用同一个 `nonce` 重连，拿回同一个 `code`。

**令牌丢不了**（审查 HUB-012）：

- 中枢先把令牌记成“待生效”，第一次用它 `hello` 成功才生效。`pair/complete` 的确认丢了也没事，设备照样 `hello`。
- 设备没收到 `pair/complete`：重连时 `pair/begin` 带 `txid`，中枢重发（生成新的待生效令牌，旧的作废）。
- 认领码 10 分钟有效；**待生效的令牌留 7 天**（设备可能存好就断电了）。过期后中枢对它回 `-32013 pending_token_expired`，并记住这个令牌哈希 30 天，好和“令牌不对”区分开。
- 设备在 `COMMITTING` 收到 `pending_token_expired`（或 `auth_invalid`）：清掉待生效的令牌，回到 `UNPAIRED`，重新认领。在 `BOUND` 收到 `auth_invalid`：不清身份，退避重连并提示。

**配对文件**：`pair/begin` 带 `"preauth":"<一次性码>"`，中枢核对后直接走到 `pair/complete`，不用手机，也不用设备确认（把卡插进去已经是人的动作）。一次性码只能用一次，24 小时过期。

限流：同一 IP 每分钟 10 次；全局未完成的配对不超过 32 个；认领码错误次数限流。

### 3.3 成员

**`ink/member`（设备 → 中枢，通知）**：`{"member":"m_kid","authed":false,"epoch":18}`。设备切成员、解锁、闲置退回时发，`epoch` 递增，也放在 `hello` 里。

切换时设备关掉所有实时会话、`abort` 已排队的声音（`finish` 会接着放）、作废确认卡；中枢取消这些会话和这一轮，丢弃没发出的帧和文字，结束 AI 上下文（审查 HUB-011）。

**`_meta.member`、`_meta.epoch`**：中枢发给设备的 `tools/call`、`confirm/request` 带目标成员和**交付时**的代次；和当前不一致时设备回错误 `-32023 stale_member`，中枢留着等下次。持久结果不绑定发起时的代次：成员回来后中枢重新检查权限，用当前代次交付（审查 HUB2-008）。

**`_meta.asked`**（D-105）：`tools/call` 带 `true` 表示这一步是人亲口要求的，由中枢策略代码比对这次的动作范围后填，模型碰不到（[概要设计 §7.5](hub-architecture.md)）。设备对对外动作和系统设置：有它就直接做，没有就弹确认卡；高风险的能力（`ask_by_hand`）一律弹卡。

**`_meta.undo_ms`**（D-105）：发短信、打电话、分享这类收不回的，带 `3000`。设备把内容显示出来才开始计时，到点才执行，回执在这之后。这期间人取消、切成员、断线、这一轮被取消，都不执行。结果和确认卡记在同一张表（§3.7），查不到就是 `unknown`。中枢要把这 3 秒算进 `budget_ms`，不够就不发。

**`members/list`（设备 → 中枢，请求）**：`{"cursor"?}` → `{"members":[{"id","name","role","mark"}],"nextCursor"?,"rev"}`。一页不超过 8 人（实测 85 词法单元）。读完所有页才替换成员表。

设备上加、删成员走已有的 `member.add`、`member.remove`（系统服务 §11.2），中枢经事件知道。

忘记 PIN：家长登录控制台发起，经外发队列调设备的 `member.reset_pin`（新能力，只给中枢，系统级）。PIN 本身永远不经过中枢。

### 3.4 `caps/list`

```json
{"tools":[{"name":"voice.chat","form":"session"},
          {"name":"tts.speak","offline":"local","timeout_ms":8000}],
 "nextCursor":"…","rev":18}
```

- 中枢**按序列化后的实际词法单元切页**，每页不超过 110。第 1 版设备只读第一页，所以最要紧的排前面，默认只发 `name` 和非默认的 `form`。实测：每项 `name`、`title`、`form` 时 17 项恰好 128，再加一个 `rev` 就超；10 项带 5 个字段是 121。
- 新字段都可省；省了就是默认（调用形态、离线失败、按服务质量默认超时）。设备不认识的字段不用管。
- 参数 `{"cursor":"…"}`。第 2 版设备读完全部页才替换目录，没读完不替换，否则缺的能力会被当成中枢没有。
- 中枢按成员过滤，成员变了发 `caps/changed`。

### 3.5 打断时带上听到哪

```json
{"sid":7,"reason":"barge_in","heard_frames":42}
```

`reason`：`barge_in`（用户开口打断）、`dismiss`（关了面板）、`done`（正常结束）、`cancel`（听写取消）。没有这些字段时，中枢用已发帧数估计，只是估计。

### 3.6 事件

沿用系统服务分支已实现的名字：

**`device/event`（设备 → 中枢，通知）**：`{"event":"message.received","at_s":1793777018,"args":{…},"seq":120,"boot":3,"member":"m_ba"}`。

- 新加 `seq`、`boot`、`member`。设备只留最近 8 条，老的被挤掉；`boot`（启动序号）加 `seq` 让中枢发现缺口，发现了就查状态（`state.list`、`confirm.status`），不能把唯一结果寄托在事件上。
- 单条 ≤ 1 KiB（设备 `EVENT_JSON_CAP`）。

**`device/event_ack`（中枢 → 设备，通知）**：`{"boot":3,"upto":120}`，设备才清。

`confirm.result` 也走这里，字段按系统服务 §7：`{"token","cap","decision","status","detail","say","object"}`，`decision` 取 `accepted`、`rejected`、`expired`、`withdrawn`、`member_changed`。

### 3.7 中枢请求确认

**单独的方法，不是 `tools/call`**（审查 HUB-005）：现在所有入站 `tools/call` 都按 AI 来源处理，不能靠自报来源区分；只有中枢策略关口的代码能发这个方法，模型工具表里没有。

**`confirm/request`（中枢 → 设备，请求，整数 id）**

```json
{"title":"AI 想提交这张表单","body":"example.com · 姓名、电话",
 "member":"m_ba","parent":false,"expires_ms":60000,"digest":"3fa9…"}
```

- 回答 `{"token":17}`（卡片排进去了）或错误。设备回到“待确认”的表，满了回错误。`parent:true` 时要成人 PIN（系统服务 §7 第 6 条）。
- 带 `_meta.member`、`_meta.epoch`（§3.3）。`token` 要跨重启不复用，绑定成员和 `digest`（现在的计数器只在内存里）。
- 用户操作后发 `device/event` 的 `confirm.result`，带 `token`、`decision`。`digest` 原样带回。
- 设备的 `ink_hub_on_request` 要加 `confirm/` 前缀。

**中枢的确认记录**：一次性；绑定家庭、目标成员、成员代次、能力、参数的规范化摘要（`digest`）、目标版本（如网页地址和版本）、过期时间。收到结果后逐项重新校验才执行；重复事件不再执行；断线、换人、过期都作废。

**结果丢了怎么办**（审查 HUB2-007）：`confirm.list` 只列还在等的，查不到不等于没执行。设备保留最近至少 16 条确认结果（要落盘，收到 `device/event_ack` 才能清），新增只读能力 `confirm.status {token}` → `{"state":"pending|accepted|done|failed|rejected|expired|withdrawn|member_changed|unknown","receipt"?:{…}}`。

- 设备执行的能力：执行前先落盘“执行中”，完成后改成 `done` 或 `failed`。重启后还是“执行中”的回 `unknown`；查不到也算 `unknown`。中枢告诉用户请确认，**不自动重试**。
- 中枢执行的能力（`confirm/request`）：设备这边到 `accepted` 为止。事件丢了，中枢查到 `accepted`，确认记录也还有效才执行；记录已作废就如实说没执行。

### 3.8 块传输

一条连接解决书、音频、字形、词库、固件、备份文件，不开第二条 TLS（D-89）。**设备拉取**：一次要一段，写完这段才要下一段，天然控流，可续传。

**帧路由**：kind 6、7 不属于会话，`sid = 0`。设备要改：`on_binary` 把 `sid = 0` 的交给块传输客户端（现在被丢弃，而且二进制接收者只有一个，要能分发）；`class_of` 把 kind 7 算后台优先级（现在默认算画面）。

**下载**

```text
设备  blob/fetch {"id":"sha256:…","offset":0,"len":65536}          请求
中枢  result {"size":1048576,"sha256":"…","xfer":3}
中枢  [kind=6 sid=0 seq=0..3] 负载 = [xfer u16][offset u32] + 数据；最后一帧 flags 位 0
```

- 一段 ≤ 64 KiB，分成 ≤ 16 KiB 的帧。设备写完这段再 `blob/fetch` 下一段，那就是确认。
- 吞吐【估算】：1.6 Mbit/s 下 1 MiB 净传输 5.2 秒，加 16 个往返（RTT 50～200 ms）共 6～8.4 秒。实际要测。
- 后台优先级；有实时会话时设备先不要下一段（`ink_jobs_hold`）。
- `id` 来自内容对象、`update/check` 或 `sync/snapshot`。**内容哈希不是授权**：按“这位成员能不能看这个对象”校验。

**上传**

```text
设备  blob/put.begin {"name":"…","size":…,"sha256":"…","member":"m_ba","kind":"photo"}  → {"upload":7,"offset":0}
设备  [kind=7 sid=0] 负载 = [upload u16][offset u32] + 数据
中枢  blob/put.ack {"upload":7,"offset":65536}       通知；中枢已落盘的位置，每 64 KiB 一次
设备  blob/put.end {"upload":7}                      → {"id":"sha256:…"}，中枢校验整个文件的哈希
```

- `upload` 是数字，只在这条连接上有效。设备最多领先 ack 两段。
- 断线后 `blob/put.begin` 同样的 `sha256` 回答已落盘的 `offset`，接着传。重复或越界的块丢弃。
- 超过家庭配额回错误。没传完的 24 小时后清掉。
- `blob/put.end` 的回答只说明中枢收到了。设备要删本地原件之前，先 `blob/status {"id":"sha256:…"}` → `{"durable":true}`：内容和包含它目录记录的完整快照都进了对象存储才回真，之前回假（审查 HUB4-002，[概要设计 §4.5](hub-architecture.md)）。

### 3.9 同步

**两个随机编号，不做加一**（审查 HUB-010、HUB2-004。从备份恢复后加一会重复）：

- `hub_instance`：中枢 128 位随机，每次恢复、导入都换，不进快照的回退范围。`hello` 回答里带。
- `dataset`：设备 64 位随机，“设备 × 集合 × 成员”建立、恢复、安装快照时换。数据集元数据里还记 `complete`（数据完整）、上次同步的 `hub_instance`（从没同步过为空）、`restore_token`（装快照时拿到的一次性令牌）。

**`sync/push`（设备 → 中枢，请求）**

```json
{"collection":"memo","member":"m_ba","dataset":"a1b2c3d4e5f60708","schema":1,"from_ver":118,"to_ver":126,
 "records":[{"id":5,"sort":1793000000,"ver":120,"updated_s":1793000000,"src":"human","schema":1,"body":"…"},
            {"id":7,"ver":126,"del":true}]}
```

回答 `{"acked_ver":126}`（中枢已落盘），设备 `ink_db_ack(126)`。同一批重发不重复写入（去重键 `dataset + id + ver`）。旧 `dataset` 的推送被拒绝。单条消息 ≤ 32 KiB，放不下的分批。`body` 是 JSON 集合就直接嵌，二进制集合 base64。`from_ver` 接不上时中枢回错误 `sync_gap`。

**`sync/status`（设备 → 中枢）**：`{"collection","member","dataset","hub_instance","last_ver","history_gap","complete","restore_token"?}` → `{"acked_ver","action":"continue"|"manifest"|"snapshot"|"wait","session"?}`。设备开机对账，或换机后知道从哪继续。`history_gap` 是墓碑被挤掉（每集合最多 256 条）。

| 设备报上来的（从上往下，审查 HUB3-002、HUB4-003） | `action` |
| --- | --- |
| 用户刚看过预览，对这个数据集做了决定 | 按决定回 `manifest` 或 `snapshot`，决定只管这一次；`complete` 为假时只能是 `snapshot` |
| `complete` 为假 | `wait` |
| `dataset` 认识，`hub_instance` 一样，没有 `history_gap` | `continue` |
| `dataset` 认识，`hub_instance` 不一样或有 `history_gap` | `manifest` |
| 不认识，带着没用过的 `restore_token`，令牌属于当前的 `hub_instance`，而且中枢副本从发令牌之后没变过 | 它成为当前的，旧的被拒绝，令牌作废，`acked_ver` 等于快照的版本。没有 `history_gap` 回 `continue`，有就回 `manifest`（审查 HUB5-002）；同一个 `dataset` 重试返回同样结果 |
| 不认识，中枢这边没有数据 | `continue`，从 0 开始 |
| 不认识，其他情况 | `wait`。**不动中枢的数据** |

`wait` 时设备照常本地用，这个数据集先不同步，过一会再问。用户看预览后决定，下一次 `sync/status` 回 `manifest`（用设备的）或 `snapshot`（用中枢的，然后走 `sync/snapshot`）。旧的 `hub_instance` 只说明发生过恢复或搬家，不能当作可以按缺项删除的证明。

**全量对账会话**（审查 HUB2-005）：`action:"manifest"` 时：

1. 设备取已落盘版本 `V` 对应的一致清单（内存里接受了、还没落盘的不算），作为文件上传（`blob/put`，每行 `id`、`ver`、`hash`）。
2. `sync/manifest {"session","dataset","v":V,"count","blob","complete":true}`。
3. `sync/need {"session","cursor"?}` → `{"need":[id,…],"nextCursor"?}`：分页取要补的记录编号，一页不超过 100 个（128 词法单元的限制）。
4. 设备用 `sync/push {"session",…}` 补这些记录。这些记录暂存，不影响现有副本。这条记录如果已经又改了，就发当前版本（比 `V` 新），已经删了就发墓碑。对账期间这个数据集不发增量。
5. `sync/commit {"session","v":V}` → `{"acked_ver":V}`。中枢在一个事务里应用：清单里没有的移到历史（可找回），不一致的以设备为准。重发同一个提交返回同样的结果。
6. 设备 `ink_db_ack(V)`。`V` 之后的改动接着增量同步，不会被吞。中途断了，整个会话作废重来。

**`sync/snapshot`（设备 → 中枢，恢复时用）**：`{"collection","member"}` → `{"snapshot":"sn_…","restore_token":"…","blob":"sha256:…","size":…,"schema":1,"next_id":…,"last_ver":…,"sha256":"…"}`，用块传输下载。设备校验后**原子安装**，失败退回原状、继续用旧的 `dataset`。成功后生成新的 `dataset`，`complete` 为真，记下 `restore_token`，再走一次 `sync/status`。令牌是一次性的，绑定这个“设备 × 集合 × 成员”、这份快照、当时的 `hub_instance` 和中枢副本的版本。现有 `ink_db_put` 不保留编号和版本，所以数据存储要加接口。

字段对应系统服务 §3.2 的记录模型。

### 3.10 升级

系统服务 §12.3 定义了 `update/check` 和 `update/fetch`。建议：`update/check` 回答里加不可变的 `blob`（`sha256:…`）；下载用 §3.8 的 `blob/fetch`。这样固件和书共用一套可续传的下载代码。**整个下载过程固定这个包**，不能每块都解析“最新的”。如果系统服务那边已经按 `update/fetch` 实现了，中枢两个都支持，但 `update/fetch` 也要带包的标识。

### 3.11 听写 `asr.stream`

语音输入（R-056、`claude/system-layer` 上的 D-65）。一路实时会话，不经过 AI。**不加帧类型，设备的 `rt.c` 不用改**：上行用现有语音帧，结果用 `rt.` 开头的通知，走现有的 `on_control`。行为规则见[概要设计 §5.3.1](hub-architecture.md)。

```text
设备  rt.open {sid:9, cap:"asr.stream", params:{"format":"opus","rate":16000,"frame_ms":60,"kind":"text","interim_ms":800}}
中枢  result {sid:9}
设备  [kind=1 sid=9 seq=0..n] 语音帧，边说边传
中枢  rt.asr {sid:9, seg:0, final:false, text:"今晚七点"}
中枢  rt.asr {sid:9, seg:0, final:true,  text:"今晚七点到家。"}
中枢  rt.asr {sid:9, seg:1, final:false, text:"给我留"}
设备  rt.turn {sid:9, op:"end"}                停顿，或用户点“说完了”
中枢  rt.asr {sid:9, seg:1, final:true,  text:"给我留饭。"}
中枢  rt.asr.done {sid:9, segs:2, ms:5400}
设备  rt.close {sid:9}
```

| 字段 | 含义 |
| --- | --- |
| `kind` | `text` 或 `number`（转成数字，打电话号码用）。`password` 一律拒绝 |
| `interim_ms` | 中间结果的最小间隔。默认 800；0 表示只要定稿 |
| `seg` | 句子序号，从 0 起。同一个 `seg` 后来的结果替换前面的 |
| `final` | 这一句定稿了，带标点和数字转写，之后不再变 |
| `text` | 不超过 480 字节；一句太长时中枢拆成两个 `seg` |
| `segs`、`ms` | 一共几句、计费的音频时长 |

**怎么结束**（审查 HUB-007 改正：“5 秒没有新文字”不能当作说完，只要定稿时长句末尾才出结果）：

| 情况 | 谁判断 | 结果 |
| --- | --- | --- |
| 用户说完 | 设备的语音检测，或点“说完了”，发 `rt.turn {op:"end"}` | 定稿后 `rt.asr.done` |
| 设备不再发音频 | 中枢：3 秒没收到语音帧 | 同上 |
| 厂商没反应 | 中枢：连续收到语音 10 秒，厂商既没结果也没状态 | `rt.asr.error`，`code: timeout` |
| 一路太长 | 中枢：总时长 120 秒 | 定稿后 `code: too_long` |

**出错**：先发 `rt.asr.error {sid, code, message}`，再发 `rt.close {sid}`。`code` 取 `budget`、`vendor`、`not_allowed`、`timeout`、`too_long`。额度不够时也先确认 `rt.open` 再发这个（`open_reply` 不把错误码交给会话）。设备收到后提示，保留已定稿的文字，退回键盘。

**取消**：`rt.asr.done` 之后的 `rt.close` 是正常结束；之前的 `rt.close` 就是取消（用户点“用键盘”），中枢丢掉没发完的结果。可以带 `reason`（§3.5），不是必需。

**空会话**：设备没连上时发出的 `rt.open`，重连后请求仍会发出，随后马上 `rt.close`（`open_reply` 的既有行为，所有实时会话都可能这样）。中枢会看到一个没有语音帧的会话：按取消处理，不算错误，不计费，不留听写记录，已经连上的厂商连接直接断开。

**断线**：不续传。中枢断开厂商连接；设备发现断线就关这个会话并退回键盘，旧的 `seg` 作废。要注意 `on_online` 会把还开着的会话重新登记一遍，而且不先通知拥有者，所以设备要自己看连接状态，先关。

**设备这边**已做完（`claude/system-services` 提交 `23d8a74`）：能力 `text.voice_start`（现在 AI 调要过确认卡，D-105 之后人亲口要求就直接开）、`text.voice_finish`、`text.voice_cancel`，状态 `text.voice`，事件 `text.voice_ended {how, why}`。麦克风同一时间只有一个拥有者；和 AI 键、唤醒词、来电、录音、锁屏的抢占关系，等采集结束回调后再交接，都是这个能力的前提（设备底层会话）。

**目录**：`asr.stream` 进 `caps/list`（`form: session`），不进模型工具表。离线或这位成员不允许时设备查不到它，话筒键可以据此变灰或隐藏。

### 3.12 回执里的 `result`

查询能力（`library.search`、`memory.search`、`usage.today`）的结果放在 `structuredContent.result`。

```json
{"content":[{"type":"text","text":"找到 2 本"}],
 "structuredContent":{"status":"done","say":"找到两本。",
   "result":{"items":[{"id":"b1","t":"海底两万里"},{"id":"b2","t":"星星邮差"}],"nextCursor":"2"}}}
```

- **整条消息受 128 词法单元限制**，所以查询能力都要分页：参数 `cursor`、`limit`，结果 `nextCursor`。字段名和结构要短。
- 结果放不下的（整篇文章、长列表）给对象编号，用块传输取。
- `result` 要符合能力描述的 `outputSchema`。设备上只在回调期间有效（系统服务 §1.2）。
- main 上的 `remote_reply` 丢弃 `result`，系统服务分支已改；合入之前查询能力只能用 `say`。

### 3.13 外发队列

设备不在线时要发的东西（概要设计 §6.8）：**就是普通的 `tools/call`，等设备上线再发**。不新增消息，只多 `_meta` 字段：

```json
{"name":"notify.post","arguments":{…},
 "_meta":{"request_id":"o123","member":"m_ba","epoch":19,"expires_at_s":1793800000}}
```

- 中枢持久保存，设备 `hello` 后发。当前成员不是目标成员、或者目标是成人但没解锁，就留着不发。发送时重新检查权限，带当前的 `_meta.epoch`（§3.3）。
- `request_id` 去重，设备重启后可能再收到一次，所以外发的能力要能承受重放（设备的去重环只有 32 条）。
- 只有不产生副作用的（通知、显示）才进队列；有副作用的动作不重放。过期的丢弃并记录。

### 3.14 中枢怎么用 `tools/list`

**不缓存整张清单**（审查 HUB-002）：它是当前页面、当前成员的视图。中枢按“`fw` + 能力名 + 版本”累积一本**定义字典**：

1. 第一次连上、或 `manifest_hash` 变了，拉一遍 `tools/list`（分页）。
2. 每轮 `ai.listen` 的 `page.actions` 给出此刻可用的名字。字典里没有的，当场再拉一次 `tools/list`，那时它就在当前页面上。
3. **`actions` 是空数组时不算数**（审查 HUB2-006）：设备的 768 字节缓冲放不下时就是 `[]`，Codex 实测 53 个全局能力名就要 780 字节。中枢把它当作“不知道”，拉 `tools/list` 当前视图来定这一轮的可用集合。这时用户还在说话，不影响第一句回答。
4. 给模型的工具 = 定义字典 ∩ 这一轮的可用集合 ∩ 成员规则。
5. 这一轮里工具调用让页面变了（回执的 `page_gen` 变了），下一步之前重新确定可用集合。

不需要改设备就能工作，只是多几个来回。设备可以改进：`actions` 只列绑定当前页面的能力，放不下时带 `"actions_more":true`；`tools/list` 带 `{"all":true}` 一次给全部定义。

### 3.15 `budget_ms`

`ai.listen`、`ai.ask` 加 `"budget_ms":38000`：**实际发送时**这个请求还剩多少毫秒。设备创建请求时就开始计 45 秒，连接要花多久中枢不知道（审查 HUB2-009）。没有这个字段时中枢按 30 秒算。中枢再留 3 秒余量，时间不够就在句子边界停。以后设备可以改成“没有活动才超时”，这个字段就不那么紧了。

## 4. 错误码

JSON-RPC `error.code`：

| 代码 | 名字 | 设备该怎么做 |
| --- | --- | --- |
| -32601 | 方法不存在 | 现有 |
| -32602 | 参数不对 | 现有 |
| -32603 | 中枢内部错误 | 现有 |
| -32010 | `auth_invalid` 令牌不对 | 退避重连，多次失败提示 |
| -32011 | `revoked`，`data.wipe` 可选 | 停止重连，提示“已从家庭移除”；`wipe:true` 时走恢复出厂 |
| -32012 | `proto_unsupported`，`data` 带 `min`、`max` | 停止重连，提示升级（底座设计 §10.3） |
| -32013 | `pending_token_expired` 待生效的令牌过期了 | `COMMITTING` 时清掉令牌，回 `UNPAIRED` |
| -32020 | `not_allowed` 成员或儿童规则不允许 | 会话被拒 |
| -32021 | `unavailable` 这台中枢没装（比如没开浏览器） | 会话被拒 |
| -32022 | `busy` 路数满了 | 会话被拒，稍后重试 |
| -32023 | `stale_member` 设备发现成员或代次变了（设备回给中枢） | 中枢留着，等目标成员回来再发 |
| -32030 | `pair_code_invalid` | 重新 `pair/begin` |
| -32031 | `pair_expired` | 重新 `pair/begin` |
| -32040 | `sync_gap` 版本接不上 | `sync/status` 对账 |
| -32041 | `quota_exceeded` 家庭存储配额满 | 提示，不重试 |

业务结果不用错误码，用回执状态（底座设计 §6.4）。

## 5. 版本

- 协议版本 `proto`，中枢支持当前和上一版（N/N-1，底座设计 §10.3）。第 1 版只在开发模式下算。
- 帧版本 `frame_v`：现在是 2（10 字节帧头）。
- 新字段一律可省，旧设备不认识就忽略。但**不能超过 128 词法单元**，加字段前要算。
- 对外 MCP 走官方规范，和这里无关（D-12）。设备说的是 MCP 形状的私有协议，不是完整 MCP。

## 6. 还没定、开发时再定的

- `ink/context`、`ink/intent`、`resources/read`、`ink/ui`：底座设计 §6.5 列了，现在都不是必需（页面快照放在 `ai.listen` 里）。做选区“解释”和声明式页面时加。
- `rt.turn`：详细设计写了，语音对话的代码没用，打断用的是 `rt.close` 加新会话，建议改设计里的说法；听写用它的“说完”（§3.11）。
- 语音帧的 UDP 通道（D-54）：4G 实测后再定。
- 设备探针上传的格式（第 2 步要定一部分：文字上屏、第一个样本播出）。

## 修订记录

- v0.1（2026-11-04）：初稿，对照提交 `9b3c24a` 的实现。同日补听写 `asr.stream`（§3.11）。
- v0.2（2026-11-04）：按 Codex 设计审查修改，并对照系统服务分支 `a81c1b8`：现状补 45 秒、2048 字节、`ai.ask` 文字被忽略、`rt.open` 不等回答；配对改成两阶段，认领网址由设备拼；成员代次；`caps/list` 按词法单元切页；事件统一成 `device/event` 并加序号；确认改成单独方法并绑定记录；块传输按段拉取，上传编号改成数字；同步加数据代次和全量对账；听写的结束条件分开；新增 §3.12～§3.14。
- v0.3（2026-11-04）：按 Codex 第二轮复核修改：电脑认领要核对码，待生效令牌 7 天和 `pending_token_expired`；`hub_instance`、`dataset` 随机编号，`sync/status` 的判断表，全量对账会话；`actions` 空数组当作不知道；`confirm.status`；`_meta.epoch` 是交付时的代次；`budget_ms`（§3.15）。同日按用户的要求（服务器只是运行时，D-101）加 `blob/status`；按 Codex 第三轮改 `sync/status` 的判断表（`wait`、`snapshot`）；按第四轮加 `restore_token`，收紧 `durable`，首版不发 `next_token`；按第五轮让 `restore_token` 碰到 `history_gap` 时走 `manifest`，并绑定 `hub_instance`。按用户的要求（D-105）加 `_meta.asked`；按第六轮加 `_meta.undo_ms`。
