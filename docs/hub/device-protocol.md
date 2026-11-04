# 设备—中枢协议：现状和扩展

状态：**待用户审核**，配合[家庭中枢概要设计](hub-architecture.md) §7。这是中枢和设备两边开发时共同遵守的约定；假设备的契约测试按它写。

对照的代码是提交 `9b3c24a`：`system/services/hub/hub_link.c`、`rt.c`，`system/policy/cap.c`，`system/services/ai/ai_service.c`，`system/apps/remote/remote.c`，`simulator/environment/sim_hub.c`、`sim_remote.c`。“系统服务”指分支 `claude/system-services` 上的[系统服务详细设计](https://github.com/Gidean-zcx/ink/blob/claude/system-services/docs/system/system-services-design.md)，还没合入 main。

原则（用户要求）：**不推翻已有协议。** §1 的消息全部保留，语义不变；§3 只加，每一条都写明为什么。

## 1. 现状：实现里实际有的消息

连接：设备主动连 `wss://…/v1/device`（`system/boot/main.c:44`）。文本帧是 JSON-RPC 2.0，二进制帧见 §1.3。

### 1.1 控制消息

| 方法 | 方向 | 类型 | 参数 | 回答 |
| --- | --- | --- | --- | --- |
| `hello` | 设备 → 中枢 | 请求 | `{"proto":1,"device":"<档案名>","member":"default"}` | 任意 `result` 就上线；`error` 就退避重连 |
| `caps/list` | 设备 → 中枢 | 请求 | `{}` | `{"tools":[{"name","title","form"}]}`；设备只读 `name`、`form` |
| `caps/changed` | 中枢 → 设备 | 通知 | 不读 | — |
| `tools/list` | 中枢 → 设备 | 请求（整数 id） | | `{"tools":[{"name","title","effect","confirm","version"?,"form"?,"inputSchema","outputSchema"?}]}`，上限约 6 KiB |
| `tools/call` | 中枢 → 设备 | 请求（整数 id） | `{"name","arguments","page_gen"?,"_meta":{"request_id"?,"page_gen"?}}` | **扁平回执** `{"status","object"?,"revision"?,"undo_ms"?,"say"?,"detail"?,"page_gen"?}` |
| `tools/call` | 设备 → 中枢 | 请求 | `{"name","arguments","_meta":{"request_id","page_gen"}}` | **MCP `CallToolResult`**：`structuredContent` 里是回执字段，`isError` 为真算失败 |
| `rt.open` | 设备 → 中枢 | 请求 | `{"sid","cap","params"}` | `{"sid"}`；`error` 则设备判定会话被拒 |
| `rt.close` | 双向 | 通知 | `{"sid"}` | — |
| `rt.*`（其他） | 中枢 → 设备 | 通知 | `{"sid",…}` | 交给会话的 `on_control` |
| `ai.listen` | 设备 → 中枢 | 请求 | `{"stream","format":"opus","rate":16000,"frame_ms":60,"page_gen","page":{"app","text","actions":[…]}}` | 一轮结束时回答，如 `{"done":true,"source":"page"}` |
| `ai.ask` | 设备 → 中枢 | 请求 | `{"question","page_gen","page":{…}}` | 文字在 `ai.delta` 和回答里 |
| `ai.listen.end` | 设备 → 中枢 | 通知 | `{"stream","frames"}` | — |
| `ai.stt` | 中枢 → 设备 | 通知 | `{"id","text"}`，`id` 是 `ai.listen` 的请求号 | — |
| `ai.delta` | 中枢 → 设备 | 通知 | `{"id","text","seq"?}` | — |
| `ai.tts.end` | 中枢 → 设备 | 通知 | `{"id","stream","frames"}` | — |

**顺序约定**：`ai.listen` 的回答必须在所有朗读帧和 `ai.tts.end` 之后；设备收到回答就关这一轮的会话（详细设计 §6.5）。

### 1.2 一轮语音的真实流程

```text
设备  rt.open {sid:7, cap:"voice.chat", params:{}}
中枢  result {sid:7}
设备  ai.listen {stream:7, format:"opus", rate:16000, frame_ms:60, page_gen:12, page:{...}}
设备  [kind=1 sid=7 seq=0..n] 上行语音帧
设备  ai.listen.end {stream:7, frames:n}
中枢  ai.stt {id:<ai.listen 的 id>, text:"翻到下一页"}
中枢  tools/call {name:"reader.next_page", arguments:{}, page_gen:12, _meta:{request_id:"r1"}}   （可多次）
设备  result {status:"done", say:"已翻页", page_gen:13}
中枢  ai.delta {id, text:"好的。"}
中枢  [kind=2 sid=7 seq=0..m] 下行朗读帧
中枢  ai.tts.end {id, stream:7, frames:m}
中枢  ai.listen 的 result {done:true}
设备  rt.close {sid:7}
```

打断：设备本机停播，`rt.close` 旧会话，再 `rt.open` 新会话。**代码里没有 `rt.turn`**（详细设计 §6.2 写了，实现没用）。

### 1.3 二进制帧

帧头 10 字节，小端（`rt.h`）：`[kind u8][sid u8][seq u16][t_ms u32][flags u8][0]`，每个 WebSocket 消息一帧。`flags`：位 0 单元结束，位 1 完整画面，位 2 可丢弃。

| kind | 方向 | 负载 |
| --- | --- | --- |
| 1 语音上行 | 设备 → 中枢 | 一个 Opus 包，16 kHz 单声道，60 ms |
| 2 语音下行 | 中枢 → 设备 | 同上 |
| 3 画面区域 | 中枢 → 设备 | `[x u16][y u16][w u16][h u16][编码 u8][刷新提示 u8]` + 数据。编码：0 原始 4 位灰度，1 4 位灰度 + LZ4，2 灰度 JPEG。提示：0 要快，1 要清楚，2 整屏刷新。4 位灰度高半字节在前，灰度 `(v + 8) / 17` |
| 4 输入 | 设备 → 中枢 | `[type u8]…`。现在实现了 `1` 点按 `[x u16][y u16]`；设计里还有 2 长按、3 翻页 `[dir i8]`、4 按键 `[key u8]`、5 提交文字 |
| 5 画面配额 | 设备 → 中枢 | `[credits u8][max_bytes u32]`，现在 `max_bytes` 是 1 MiB |

`browser.session` 的 `params`：`{"w","h","gray":16,"url"?}`，整段不超过 239 字节，URL 太长时不带（中枢用起始页）。**现在不带 `dpi`**，详细设计 §6.2 的例子里有，实现没有。

## 2. 中枢必须遵守的限制

逐条来源见[概要设计 §1.3](hub-architecture.md)。假设备测试全部检查：

1. 发给设备的每条 JSON ≤ 128 词法单元（对象、数组、键、值各算一个）。
2. `ai.delta`、`ai.stt` 的 `text` ≤ 511 字节，按 UTF-8 字符边界切。
3. 中枢发的请求，`id` 是整数。
4. 能力名：目录项 ≤ 39 字节；会话能力名 ≤ 23 字节；工具名 ≤ 47 字节。
5. 方法名 ≤ 31 字节。设备按前缀订阅（≤ 15 字节）。
6. **不发应用层保活**。任何消息都会刷新设备的空闲计时；保活用 WebSocket ping。
7. 每条新连接当作设备什么都没说过；同一设备的新连接到了，旧连接立刻关。
8. 画面帧只在有配额时发；一个配额一个单元；单元最后一帧带位 0。重连后 `shown` 算白屏。
9. 未握手的连接只接受 `hello`、`pair/*`，单条 ≤ 4 KB，10 秒不握手就断。

## 3. 扩展

总表：

| 编号 | 内容 | 方向 | 不做会怎样 | 设备改动 |
| --- | --- | --- | --- | --- |
| 3.1 | `hello` 第 2 版 | 设备 → 中枢 | 没有身份，公网上线不了 | 必须 |
| 3.2 | 配对 `pair/*` | 双向 | 设备进不了家庭 | 必须 |
| 3.3 | `ink/member`、`members/list` | 双向 | 记忆和对话分不了成员 | 必须 |
| 3.4 | `caps/list` 更多字段和分页 | 设备 → 中枢 | 超过 17 项放不下；离线策略到不了设备 | 可选 |
| 3.5 | `rt.close` 带原因 | 设备 → 中枢 | 打断时历史截断不准 | 可选 |
| 3.6 | `ink/event` | 设备 → 中枢 | AI 不知道新短信、闹钟等 | 系统服务分支在做 |
| 3.7 | `confirm.request` | 中枢 → 设备 | 中枢能力的外部副作用没法确认 | 必须（做 `browser.act` 时） |
| 3.8 | 块传输 `blob/*`、帧 kind 6、7 | 双向 | 书、音频、固件、备份文件传不了 | 必须 |
| 3.9 | 同步 `sync/*` | 设备 → 中枢 | 没有备份 | 必须 |
| 3.10 | 升级 `update/*` | 设备 → 中枢 | 没有 OTA | 系统服务已定义，这里统一到块传输 |
| 3.11 | 听写 `asr.stream` | 双向 | 输入法没有语音输入（R-056） | 设备底层会话做；`rt.c` 不用改 |

### 3.1 `hello` 第 2 版

```json
{"jsonrpc":"2.0","id":1,"method":"hello","params":{
  "proto":2,
  "device":"dv_8f3a","token":"<令牌>",
  "hw":"metalio-e-ink4-plus","fw":"1.2.0","fw_code":1200,
  "manifest_hash":"9c1e…","frame_v":2,
  "member":"m_ba","authed":true,
  "net":"wifi","caps_rev":17,"members_rev":4}}
```

回答：

```json
{"session":"s_01","proto":2,"now_ms":1793777018000,
 "caps_rev":18,"members_rev":4,"outbox":2,
 "pins":["sha256/…"],"urls":["wss://…:8443/v1/device"],"next_token":"…"}
```

- `now_ms`：校时，系统服务 §5.1 已经这样设计。
- `pins`、`urls`、`next_token` 都可以没有。`pins` 有两个时表示换密钥的过渡期。
- `outbox`：外发队列里有几条，中枢随后按普通 `tools/call` 发（概要设计 §6.8）。
- 错误码见 §4。`revoked` 时设备停止重连。
- 中枢保存 `fw`、数据格式版本（底座设计 §10.3）。

**兼容**：`proto:1` 只在开发模式（配置 `dev_allow_proto1`）接受，映射到一个开发家庭，给现有模拟器场景用。公网部署默认拒绝。

### 3.2 配对

只在没有令牌时用；设备用同一个地址，不发 `hello`。

**`pair/begin`（设备 → 中枢，请求）**

```json
{"proto":2,"hw":"metalio-e-ink4-plus","fw":"1.2.0","nonce":"f1a9…"}
```

回答：

```json
{"code":"K7QF2M9X","url":"https://…/claim/K7QF2M9X","expires_s":600}
```

设备把 `url` 加上 `#s=<观察到的公钥 SHA-256 前 8 字节，base64url>` 编成二维码，同时显示 `code`（给电脑输入）。

用 TF 卡配对文件时，`pair/begin` 多带 `"preauth":"<一次性码>"`；中枢核对后直接发 `pair/complete`，不用手机认领。一次性码只能用一次，24 小时过期。

**`pair/complete`（中枢 → 设备，请求，整数 id）**：用户在手机上确认后发。

```json
{"device":"dv_8f3a","token":"<令牌>","family":"张家","name":"爸爸的设备",
 "urls":["wss://…:8443/v1/device"],"pins":["sha256/…"],
 "members_rev":1,"now_ms":1793777018000}
```

设备存进 NVS 后回 `{"ok":true}`，然后断开，用 `hello` 重连，再用 `members/list` 拉成员表。中枢收到确认才把设备标为有效，否则令牌作废，下一次 `pair/begin` 重新来。

**`pair/expired`（中枢 → 设备，通知）**：认领码过期，设备重新 `pair/begin`。

限流：同一 IP 每分钟 10 次；全局未完成的配对不超过 32 个。

### 3.3 成员

**`ink/member`（设备 → 中枢，通知）**：`{"member":"m_kid","authed":false}`。设备切成员、解锁、闲置退回时发。中枢结束当前 AI 会话，策略和记忆换人。

**`members/list`（设备 → 中枢，请求）**：`{"cursor"?}` → `{"members":[{"id","name","role","mark"}],"nextCursor"?,"rev"}`。受 128 词法单元限制，一页不超过 8 人。

设备上加、删成员走已有的 `member.add`、`member.remove`（系统服务 §11.2），中枢经事件知道。

忘记 PIN：家长在控制台发起，经外发队列调设备的 `member.reset_pin`（新能力，只给中枢，系统级，设备上要有另一位成人或恢复流程确认，细节在实现时定）。PIN 本身永远不经过中枢。

### 3.4 `caps/list`

```json
{"tools":[{"name":"voice.chat","form":"session"},
          {"name":"tts.speak","offline":"local","timeout_ms":8000}],
 "nextCursor":"…","rev":18}
```

- 新字段都可省；省了就是默认（调用形态、离线失败、按服务质量默认超时）。
- 设备不认识的字段不用管。现在的设备读第一页就够（≤ 17 项时只有一页）。
- 参数 `{"cursor":"…"}`。
- 中枢按成员过滤，成员变了发 `caps/changed`。

### 3.5 打断时带上听到哪

```json
{"sid":7,"reason":"barge_in","heard_frames":42}
```

`reason`：`barge_in`（用户开口打断）、`dismiss`（关了面板）、`done`（正常结束）。没有这些字段时，中枢用已发帧数估计。

### 3.6 事件

**`ink/event`（设备 → 中枢，通知）**：`{"seq":120,"name":"message.received","member":"m_ba","t_ms":…,"data":{…}}`。
**`ink/event_ack`（中枢 → 设备，通知）**：`{"upto":120}`，设备才清。断网时设备留最近 8 条（系统服务 §1.2）。

`confirm.result` 也走这里：`{"name":"confirm.result","data":{"request_id":"r7","accepted":true,"receipt":{…}}}`。

### 3.7 中枢请求确认

设备能力 `confirm.request`，`expose` 只给远端，**中枢的模型工具表里没有**。

```json
{"name":"confirm.request","arguments":{
   "title":"AI 想提交这张表单","body":"example.com · 姓名、电话",
   "source":"hub","expires_ms":60000},
 "_meta":{"request_id":"r7"}}
```

设备弹系统确认卡，立即回 `{"status":"pending_confirmation"}`；用户操作后发 `confirm.result` 事件。设备和 AI 都不能代按（系统服务 §7）。

### 3.8 块传输

一条连接解决书、音频、字形、词库、固件、备份文件，不开第二条 TLS（D-89）。**设备拉取**：一次要一块，收到再要下一块，天然控流，可续传。

**下载**

```text
设备  blob/fetch {"id":"sha256:…","offset":0,"len":16384}          请求
中枢  result {"size":1048576,"sha256":"…","xfer":3}
中枢  [kind=6 sid=0 seq=3 flags=LAST] 负载 = [xfer u16][offset u32] + 数据
```

- 块 ≤ 16 KiB。设备下一次 `blob/fetch` 就是确认。
- 帧走后台优先级；有实时会话时设备先不要下一块（设备已有 `ink_jobs_hold`）。
- `id` 来自内容对象、`update/check` 或 `sync/snapshot`。按家庭和成员校验权限。

**上传**

```text
设备  blob/put.begin {"name":"…","size":…,"sha256":"…","member":"m_ba","kind":"photo"}  → {"upload":"u1","offset":0}
设备  [kind=7 sid=0] 负载 = [upload u16][offset u32] + 数据
中枢  blob/put.ack {"upload":"u1","offset":65536}       通知，每 64 KiB 一次
设备  blob/put.end {"upload":"u1"}                      → {"id":"sha256:…"}
```

断线后 `blob/put.begin` 同样的 `sha256` 回答已收到的 `offset`，接着传。

**设备改动**：`rt.c` 的 `on_binary` 现在要求帧有会话，`sid = 0` 要路由到块传输客户端；`INK_RT_KINDS` 加两个。

### 3.9 同步

**`sync/push`（设备 → 中枢，请求）**

```json
{"collection":"memo","member":"m_ba","schema":1,"from_ver":118,"to_ver":126,
 "records":[{"id":5,"sort":1793000000,"ver":120,"updated_s":1793000000,"src":"human","schema":1,"body":"…"},
            {"id":7,"ver":126,"del":true}]}
```

回答 `{"acked_ver":126}`，设备 `ink_db_ack(126)`。同一批重发不重复写入（按 `collection + member + id + ver`）。单条消息 ≤ 32 KiB，放不下的分批。`body` 是 JSON 集合就直接嵌，二进制集合 base64。

**`sync/status`（设备 → 中枢）**：`{"collection","member"}` → `{"acked_ver"}`。设备开机对账，或换机后知道从哪继续。

**`sync/snapshot`（设备 → 中枢，恢复时用）**：`{"collection","member"}` → `{"blob":"sha256:…","size":…,"ver":…}`，然后用块传输下载。

字段对应系统服务 §3.2 的记录模型。

### 3.10 升级

系统服务 §12.3 定义了 `update/check` 和 `update/fetch`。建议：`update/check` 不变，回答里加 `blob` 字段；下载用 §3.8 的 `blob/fetch`。这样固件和书共用一套可续传的下载代码。如果系统服务那边已经按 `update/fetch` 实现了，中枢两个都支持，`update/fetch {offset,len}` 等价于对最新包的 `blob/fetch`。

### 3.11 听写 `asr.stream`

语音输入（R-056、`claude/system-layer` 上的 D-65）。一路实时会话，不经过 AI。**不加帧类型，设备的 `rt.c` 不用改**：上行用现有语音帧，结果用 `rt.` 开头的通知，走现有的 `on_control`。行为规则见[概要设计 §5.3.1](hub-architecture.md)。这份约定已发给设备底层会话对齐。

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

**出错**：先发 `rt.asr.error {sid, code, message}`，再发 `rt.close {sid}`。`code` 取 `budget`、`vendor`、`not_allowed`、`timeout`、`too_long`。额度不够时也先确认 `rt.open` 再发这个（`open_reply` 不把错误码交给会话）。设备收到后提示，保留已定稿的文字，退回键盘。

**结束和取消**：`rt.asr.done` 之后的 `rt.close` 是正常结束；之前的 `rt.close` 就是取消（用户点“用键盘”），中枢丢掉没发完的结果。可以带 `reason`（§3.5），不是必需。中枢兜底：5 秒没有新文字就自己结束；一路最长 120 秒。

**断线**：不续传。中枢断开厂商连接；设备发现断线就关这个会话并退回键盘。要注意 `on_online` 会把还开着的会话重新登记一遍，听写不该这样，所以设备要先关。

**目录**：`asr.stream` 进 `caps/list`（`form: session`），不进模型工具表。离线或这位成员不允许时设备查不到它，话筒键可以据此变灰或隐藏。

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
| -32020 | `not_allowed` 成员或儿童规则不允许 | 会话被拒 |
| -32021 | `unavailable` 这台中枢没装（比如没开浏览器） | 会话被拒 |
| -32022 | `busy` 路数满了 | 会话被拒，稍后重试 |
| -32030 | `pair_code_invalid` | 重新 `pair/begin` |

业务结果不用错误码，用回执状态（底座设计 §6.4）。

## 5. 版本

- 协议版本 `proto`，中枢支持当前和上一版（N/N-1，底座设计 §10.3）。第 1 版只在开发模式下算。
- 帧版本 `frame_v`：现在是 2（10 字节帧头）。
- 新字段一律可省，旧设备不认识就忽略。
- 对外 MCP 走官方规范，和这里无关（D-12）。

## 6. 还没定、开发时再定的

- `ink/context`、`ink/intent`、`resources/read`、`ink/ui`：底座设计 §6.5 列了，现在都不是必需（页面快照放在 `ai.listen` 里）。做选区“解释”和声明式页面时加。
- `rt.turn`：详细设计写了，语音对话的代码没用，打断用的是 `rt.close` 加新会话，建议改设计里的说法；听写用它的“说完”（§3.11）。
- 语音帧的 UDP 通道（D-54）：4G 实测后再定。
- 设备探针上传的格式。

## 修订记录

- v0.1（2026-11-04）：初稿，对照提交 `9b3c24a` 的实现。同日补听写 `asr.stream`（§3.11）。
