# C. AI 原生设备架构与能力协议调研（服务 G-002）

查阅日期：2026-11-01（UTC）。范围：设备与家庭中枢之间的能力协议、页面上下文、声明式界面、语音链路和儿童模式。

依据标注：〔文档〕表示官方或上游文档；〔代码〕表示本次在固定提交上读过的代码；〔报道〕表示媒体或第三方资料；〔推断〕〔估算〕〔建议〕表示本文的判断。本文没有任何实机测试。来源编号见文末 §8。

## 0. 结论摘要

1. **上游固件对 AI 几乎没有开放 app 能力。**〔代码〕目标板的上游固件（CloudZao Plus v2.0.71）由 xiaozhi-esp32 派生：支持 WebSocket 与 MQTT+UDP 两种传输，用 `hello` 握手，在 `type:"mcp"` 信封里传 JSON-RPC，设备充当 MCP server。但 AI 只能调用 3 个工具（设备状态、音量、亮度），另有 4 个隐藏工具。书库、进度、听书都没有开放给 AI，G-002 在上游的覆盖接近于零。
2. **MCP 最新稳定版是 2026-07-28，设备链路不宜照搬。**〔文档〕新版取消了 `initialize` 握手和会话，改为每个请求在 `_meta` 中携带版本和能力，并新增 `server/discover`、`subscriptions/listen` 和 MRTR；Sampling、Roots、Logging 已弃用，Tasks 移入扩展。小智系设备仍停留在 2024-11-05 的子集；项目旧文档引用的 2025-11-25 版也已被取代。〔建议〕设备链路采用沿用 MCP 形状的私有 profile；中枢对外再提供标准 MCP 2026-07-28。
3. **能力模型由四部分组成，策略由设备执行。**〔建议〕四部分是：带 URI 与版本号的实体；带 JSON Schema 和风险、确认、儿童策略的动作；按需生成、带 `rev` 的页面语义快照（≤8 KB）；带序号与确认的事件日志。策略由设备执行，模型只负责提议动作。
4. **声明式界面采用 A2UI 线格式加自定义组件目录。**〔文档〕MCP Apps、OpenAI Apps SDK、MCP-UI 都依赖沙箱 iframe 中的 HTML，MCU 无法渲染。A2UI 的“客户端目录 + 扁平组件表 + 数据模型绑定”最适合 LVGL。另外，LVGL 从 9.5 起，设备端运行时加载 XML 需要付费许可，因此 XML 不宜作为下发格式。〔建议〕采用 A2UI v0.9 线格式，配自定义目录 `ink.eink/v0`（14 个组件）。
5. **语音默认用流式管线，按键说话（PTT）为主。**〔建议〕默认链路是“流式 ASR → LLM → 流式 TTS”，原因有三：回答本来就要以文字上屏；儿童内容可以在 TTS 之前过滤；模型可以自由选择。端到端语音模型（S2S）留作可选模式。〔估算〕松键到首音，Wi-Fi 约 1.0–1.8 s，4G 约 1.2–2.2 s；每多一次设备工具回合，再加 0.5–1.3 s。回执中带一句可直接播报的 `say`，就能省掉第二次 LLM 调用。
6. **现成服务端不适合共享设备。**〔代码〕xinnan 服务端按 `device_id` 归档记忆，声纹识别只用来把说话人姓名写进提示词，并不用于权限判断。本项目需要按家庭成员归档记忆，成员授权用家长 PIN，而不是声纹。

## 1. xiaozhi-esp32 协议族

### 1.1 传输与消息（〔文档〕S1；〔代码〕S2）

| 项目 | 内容 |
|---|---|
| WebSocket | 握手头：`Authorization: Bearer`、`Protocol-Version`、`Device-Id`（MAC）、`Client-Id`（UUID）。设备先发 `hello`，10 s 内须收到 `transport:"websocket"` 的服务端 hello。文本帧传 JSON，二进制帧传 Opus |
| 二进制帧版本 | v1 为裸 Opus；v2 带 timestamp，供服务端 AEC 使用；v3 为 4 字节轻量头 |
| MQTT+UDP | MQTT 传控制消息，keepalive 默认 240 s；服务端 hello 下发 UDP 地址与 AES key/nonce；UDP 音频用 AES-CTR 加密，并用序号防重放 |
| 音频参数 | 上行 Opus 16 kHz 单声道、60 ms 帧；下行可用 24 kHz；设备处于 listening 状态时丢弃下行音频 |
| 设备→服务端消息 | `hello`；`listen`（state 为 start/stop/detect，mode 为 auto/manual/realtime）；`abort`；`mcp`；`goodbye`（仅 MQTT） |
| 服务端→设备消息 | `hello`、`stt`、`llm`（emotion）、`tts`（start/stop/sentence_start+text）、`mcp`、`system`（reboot）、`alert`、`custom`（可选）、`goodbye`。新版另有两种：`notify`，设备空闲时单向播放 Ogg Opus URL 并显示字幕，无回执；`glyph_push`，随 stt/tts 补发设备字体缺失的字形 |
| 旧协议 | `type:"iot"` 已弃用，统一改走 MCP |

### 1.2 设备作为 MCP server

- **方向。**〔文档〕后端是 MCP client，设备是 server，但连接由设备主动向外拨出，对 4G/NAT 环境友好。设备在 `hello` 中用 `features.mcp:true` 声明支持 MCP。
- **只实现了最小子集。**〔代码〕`initialize` 返回 `protocolVersion:"2024-11-05"`、`capabilities:{tools:{}}`。设备只实现 initialize、tools/list、tools/call 三个方法，收到 `notifications/*` 直接忽略。
- **分页与隐藏工具。**〔代码〕`tools/list` 每页上限 8000 字节，cursor 就是工具名。只有带上非标准参数 `withUserTools` 时，才会列出“user-only”工具；标记方式是在工具上加 `annotations.audience:["user"]`，而 MCP 的 ToolAnnotations 中并没有 audience 字段。
- **安全缺陷：隐藏不等于禁止。**〔代码〕最新版和上游 Plus 的 `tools/call` 都不检查 user-only 标记。服务端只要知道工具名，就能直接调用重启、升级固件等隐藏工具。
- **参数与返回。**〔文档/代码〕参数只支持 bool、int、string 三种类型（可带 min/max/default）。返回 `content:[{type:"text"}]` 加 `isError:false`，没有 outputSchema 和 structuredContent。工具在主循环中串行执行；源码注释说明，常用工具排在列表前部，以便命中 prompt cache。
- **工具命名与描述。**工具名采用 `self.模块.动作` 形式，描述是写给模型看的自然语言，例如“不知道当前音量时，先调用 `self.get_device_status`”。内置工具有 `get_device_status`、`audio_speaker.set_volume`、`screen.set_brightness/set_theme`、`camera.take_photo`。user-only 工具有 `get_system_info`、`reboot`、`upgrade_firmware`、`screen.snapshot/preview_image`、`assets.set_download_url`。

### 1.3 服务端如何让 LLM 调用设备工具（xinnan-tech，〔代码〕S3）

- **每次连接都重新拉取工具。**收到带 mcp 能力的 hello 后，服务端立即发出 `initialize`（id=1）和 `tools/list`（id=2），按 nextCursor 续取完毕后，把设备工具并入 LLM 的函数列表。
- **工具名改写。**为兼容 OpenAI 的函数命名规则，工具名中 `[^a-zA-Z0-9_\-一-鿿]` 范围外的字符都被替换为 `_`，例如 `self.audio_speaker.set_volume` 变成 `self_audio_speaker_set_volume`；描述中出现的工具名同步替换。
- **调用与结果回传。**调用默认超时 30 s。服务端取返回的 `content[0].text` 交给 LLM 再生成一轮（REQLLM）；如果结果是含 `action` 字段的 JSON，则直接回复，跳过第二次 LLM。
- **五类工具来源。**服务端插件、服务端 MCP、设备 IoT、设备 MCP，以及 MCP 接入点（云端 MCP 通过 WebSocket 接入点挂到某个智能体）。另有“上下文源”机制：唤醒时以 GET 请求外部 HTTP 接口（带 device-id 头），把结果注入 `{{dynamic_context}}`。
- **打断。**收到 `abort` 后清空 LLM 与 TTS 队列，并回发 `tts stop`。
- **成熟度与许可。**README 自述“未通过网络安全测评，请勿在生产环境中使用”；许可证为 MIT。

### 1.4 上游 Plus 现状（〔代码〕S2，提交 690a023，2026-10-30）

- **云服务。**可在三家中选一家：百问（默认，CloudZao 自有 HTTP 接口）、小智、云信。走小智协议时默认手动（PTT）拾音；WebSocket 只在需要音频通道时才连接；120 s 无数据判定超时。
- **开放给 AI 的能力。**只注册了上述 3 个 AI 工具和 4 个 user-only 工具。截图、主题、图片预览三项在 S31 上被 `#if 0` 关闭。`main/boards`、`main/reader` 目录下没有任何 `AddTool` 调用。
- **新协议特性。**`notify` 和 `glyph_push` 均未实现。
- **界面框架。**LVGL 锁定 9.5.0，`CONFIG_LV_USE_OBSERVER=y`，可以用来做数据绑定。
- **上游支持情况。**〔文档〕小智上游 README 已列出 ESP32-S31 变体，需要 IDF 6.1 及以上。

### 1.5 延迟数据（〔文档〕S4；作者自述是不严谨的内部测试，测试地点为广州联通）

| 环节 | 实测值 |
|---|---|
| 流式 ASR 首词等待 | 讯飞 0.795 s；豆包 0.852–0.864 s；阿里 0.984 s（2026-04） |
| LLM 首 token（超长角色提示词） | qwen3-8b 0.587 s；qwen3.6-flash 0.600 s；qwen-flash 0.670 s；doubao-seed-2.0-mini 0.700 s；deepseek-v4-flash 0.901 s（2026-08）。报告结论表中 qwen-flash 写作 0.434 s，与明细不一致 |
| 流式 TTS 首音 | 讯飞 0.253 s；火山双流 0.317 s；阿里 0.712 s（2025-09） |
| 视觉模型 | 2.6–13.9 s |

README 称 0.5.2 版起启用流式配置后，响应比早期快约 2.5 s。报告没有端到端（按键到出声）数据，也没有 4G 数据。

### 1.6 多设备与多用户

- **xinnan 服务端。**〔代码/文档〕每个连接对应一个 handler，按 `device-id` 头读取私有配置。全模块部署支持多用户、多智能体和设备绑定。声纹识别与 ASR 并行，结果以 `{"speaker":..,"content":..}` 的形式写进 LLM 输入；记忆使用 `role_id=device_id`。
- **xiaozhi.me 官方服务。**闭源。README 只说明固件默认接入它、个人账号可以免费使用 Qwen 实时模型、可在控制台配置。官方服务端如何处理家庭多成员，无法核实。
- **乐鑫 esp_xiaozhi 组件。**同时支持两种传输，两者都配置时优先使用 MQTT（S23）。

### 1.7 对本项目的含义（〔推断〕）

- **可以复用：**传输层；Opus 参数；hello、abort、tts 事件；McpServer 的分页与参数校验骨架。xinnan 服务端可以直接作为第 0 期实验台，用来测延迟。
- **必须修改：**
  - 调用工具时强制检查可见性和策略，而不只是在列表中隐藏。
  - 按清单哈希缓存工具目录，不再每次连接全量拉取。
  - 返回结构化回执。
  - 会话与记忆按成员区分。
  - 如果启用 UDP 音频，改用带认证的 AEAD；目前用的 CTR 模式没有 MAC，无法发现篡改。

## 2. MCP 2026-07-28

### 2.1 版本与主要变化（〔文档〕S5）

2026-07-28 是最终稳定版，GitHub 发布日期同为 2026-07-28。主要变化：

- **无状态化：**去掉 `initialize/initialized` 握手和 `Mcp-Session-Id`。每个请求在 `_meta` 中携带 `io.modelcontextprotocol/protocolVersion` 和 `clientCapabilities`；`server/discover` 成为必须实现的方法。
- **订阅：**`subscriptions/listen` 取代 `resources/subscribe` 和 GET 流，客户端按类型选择要接收的通知。
- **MRTR（多轮往返请求）：**服务端不再主动发起 sampling、elicitation、roots 请求，而是返回 `resultType:"input_required"`，由客户端带上 `inputResponses` 重试。所有结果都必须带 `resultType` 字段。
- **Tasks 移入官方扩展：**用 `tasks/get` 轮询，用 `tasks/update` 补充输入；状态有 working、input_required、completed、failed、cancelled。
- **缓存与排序：**列表类结果必须带 `ttlMs` 和 `cacheScope`。`tools/list` 应按确定顺序返回（便于 prompt cache），不得随连接变化，但可以随授权变化。
- **断流处理：**响应流中断后，进行中的请求即告丢失，客户端须用新 id 重新发起。因此业务层需要自带幂等键。
- **弃用：**Roots、Sampling、Logging，以及 HTTP+SSE 传输。

### 2.2 与本项目相关的要点

- **工具（tools）。**
  - 定义字段：name（建议 1–128 个字符，限 `A–Z a–z 0–9 _ - .`）、title、description、icons、inputSchema 和 outputSchema（JSON Schema 2020-12）。
  - 注解及默认值：readOnlyHint（false）、destructiveHint（true）、idempotentHint（false）、openWorldHint（true）。规范明确：来自不受信服务器的注解只是提示，不可作为决策依据。
  - 结构化结果：structuredContent 须符合 outputSchema，并建议同时附上序列化文本。业务失败用 `isError:true` 返回，让模型自行纠正；协议错误走 JSON-RPC error。
  - 规范建议保持人在回路中，在执行前给出确认提示。跨调用的状态用显式 handle 作为工具参数传递，而不依赖会话。
- **资源（resources）。**以 URI 标识；`resources/templates/list` 提供 uriTemplate；注解有 audience（user/assistant）、priority、lastModified。
- **进度与取消。**用 `_meta.progressToken` 配合 `notifications/progress` 报进度；用 `notifications/cancelled` 取消；客户端断开视为取消。
- **Elicitation（向用户索取输入）。**有表单和 URL 两种模式；凭证类敏感信息必须走 URL 模式。
- **自定义传输。**允许，但须保留 JSON-RPC 格式、消息模式和每请求元数据。
- **扩展。**MCP Apps（`io.modelcontextprotocol/ui`，Stable）；Tasks；Skills（SEP-2640 Final，按需加载工作流说明，S6）。

### 2.3 哪些功能放在 MCU，哪些放在中枢（〔建议〕）

| 特性 | 设备（MCU） | 中枢 |
|---|---|---|
| tools/list 分页、tools/call、isError | 实现，沿用 8 KB 分页 | 缓存、检索、编排 |
| outputSchema、structuredContent | 实现：返回回执 JSON，并附一句文本 | 校验 |
| annotations（工具注解） | 静态输出；设备本身按策略执行 | 映射给模型和界面 |
| resources/read | 只做小集合：`page://current`、`book://`、`word://`、`memo://` | 资源模板、索引、订阅 |
| list_changed | 安装或更新 app 时发一条通知 | 对外提供 `subscriptions/listen` |
| progress、cancel | 报告长任务进度，尽力取消 | 实现 Tasks 语义 |
| prompts、completion、Skills | 不做 | 实现 |
| sampling、roots、logging | 不做（已弃用） | 不做，或改用 OpenTelemetry |
| elicitation、MRTR | 不做，改为设备本地确认卡 | 对外实现 |
| MCP Apps | 不可能（设备没有浏览器） | 可选，用于家长控制台 |
| 无状态 `_meta`、OAuth | 设备链路不采用，用设备令牌 + WSS | 对外完整实现 |

## 3. 生成式与声明式界面

| 方案 | 机制 | 是否适合 1216×684 墨水屏 + LVGL |
|---|---|---|
| A2UI v0.9.1（v1.0 处于候选阶段，S9） | 声明式 JSON，消息有 createSurface、updateComponents、updateDataModel、deleteSurface。组件以扁平邻接表组织（按 id 引用）；数据模型用 JSON Pointer 绑定，List 模板用相对路径。渲染端声明 `supportedCatalogIds`，agent 只能使用目录内的组件。动作分两类：发事件给 agent，或执行本地函数。v1.0 新增双向函数调用，以及 `allowedCallers` 和 `requiresUserActivation`（例如 openUrl 只能由用户手势触发）。Apache-2.0 | 最适合。basic 目录有 18 个组件，minimal 目录有 5 个。需要自己用 C 写渲染器并裁剪组件 |
| MCP Apps、OpenAI Apps SDK、MCP-UI（S6–S8） | 工具通过 `_meta.ui.resourceUri` 指向 `ui://` 资源，类型为 `text/html;profile=mcp-app`，在沙箱 iframe 中渲染，经 postMessage 走 JSON-RPC。Apps SDK 已对齐此标准，其 `_meta` 字段只发给组件，不给模型；MCP-UI 现在是这个标准的一个实现 | 设备不可行；可用于家长端 |
| Vercel AI SDK（S10） | 工具调用以 `tool-${name}` 类型的消息片段映射到 React 组件，状态有 input-available、output-available、output-error 等。RSC/streamUI 仍处于实验阶段 | 不可直接使用；“工具输出类型对应组件”的思路可以借鉴 |
| Adaptive Cards（S11） | 宿主用原生控件渲染 JSON 卡片。Copilot Studio 支持到 1.6；Teams 只到 1.5；Web Chat 不支持 Action.Execute；Refresh 自 1.4 起提供 | 可行，但各宿主版本不一致，且偏向聊天卡片场景 |
| LVGL XML（S12） | 从 v9.5 起，`lv_xml_*` 移出开源仓库；设备端运行时加载 XML 需要付费许可 | 不宜作为下发格式 |

〔建议〕具体方案见 §6.7。

## 4. 各平台的 OS 级 AI 集成方式

| 平台 | (a) 屏幕内容 | (b) app 动作 | (c) 数据与历史 | (d) 发现与渐进披露 | (e) 副作用确认 | (f) 用户与隐私 |
|---|---|---|---|---|---|---|
| Apple App Intents（S13） | 把视图或 NSUserActivity 关联到 AppEntity（`appEntityIdentifier`，iOS 18.2+）；自绘视图用 AppEntityUIElement 提供位置与状态信息（18.4+）。用户可以说“这张照片”来指代屏幕内容 | AppIntent 加 schema（按领域定义的契约） | IndexedEntity 进入 Spotlight 语义索引；app 通过 Donation（向系统报告用户行为）提供行为线索；Transferable 支持跨 app 传递内容 | schema 和索引让系统按需匹配；EntityCollection 只存 ID，用到时再解析（2026-06） | `requestConfirmation(conditions:)`；2026-06 起，对共享实体的破坏性操作可通过 OwnershipProvidingEntity 触发确认；另有 UndoableIntent、CancellableIntent、LongRunningIntent | 〔报道〕只在用户提问时读取屏幕；发给第三方（如 ChatGPT）之前先征得用户同意 |
| Android（S14） | Assist API：AssistStructure（基于无障碍的视图树）；onProvideAssistContent（JSON-LD 或 URI）；可用 FLAG_SECURE 逐窗口排除敏感内容 | AppFunctions（Android 16+，实验阶段）：用 @AppFunction 标注，KDoc 即描述，构建时生成 XML schema 供系统索引，在本地执行。没有接入的 app 由 Gemini 通过 UI 自动化操作 | 文档未说明 | 系统索引 + app 级元数据，用来指导编排 | 文档未说明。〔报道〕“屏幕自动化”是一项特殊权限，默认“每次询问”，用户可选择停止任务并打开 app | 调用方须持有 EXECUTE_APP_FUNCTIONS 权限；Gemini 接入截至 2026-05 仍在私测 |
| Windows（S15） | agent 在独立工作区（agent workspace，单独的桌面）中操作界面，对用户桌面的可见范围受限 | MCP server 经 On-device Agent Registry 注册（本地或远程皆可，工具为 odr.exe）；内置 File Explorer 和 Settings 两个连接器 | 预览期默认只能访问几个已知文件夹 | 由注册表统一发现 | 敏感操作可能需要用户额外批准 | agent 使用独立账户，最小权限，默认隔离，交互可审计；企业可用 Intune 按 agent 管控 |
| Rabbit r1（S16） | — | 宣称 LAM 能代用户操作网站；teach mode 先由用户演示网页任务再复现，官方说明是实验功能、结果不可预测 | — | — | — | 2024 年出现硬编码 API key 泄露、聊天日志无法删除等问题 |
| Humane CosmOS（S17） | 演示中能理解电视和手机屏幕，未见技术细节 | 定位为编排模型、数据与服务的“intelligent conductor” | — | — | — | AI Pin 于 2025-02-28 停服，设备随即失去云端功能 |

〔推断〕可以借鉴的做法：

- 让 AI 理解语义而不是像素：用实体绑定，并以默认视图树兜底。
- 动作具备确认、撤销、取消和长任务语义。
- 用行为捐赠和语义索引来支持历史查询。
- 敏感页面整体排除在快照之外。
- 给 agent 独立身份，按最小权限授权。

需要避免的教训：设备上不放密钥；用户数据可删除、可导出；核心功能离线可用。

## 5. 中枢 agent 设计

### 5.1 流式管线与端到端语音模型（S2S）

| 对比项 | 流式管线 | S2S |
|---|---|---|
| 价格〔文档〕 | 按各家 ASR、LLM、TTS 分别计费 | OpenAI gpt-realtime-2.1：音频输入 $32、输出 $64 / 百万 token；文本输入 $4、输出 $24；上下文 128K（gpt-realtime 已弃用，S18）。Gemini Live（如 3.1-flash-live-preview）：音频输入 $3（约 $0.005/分钟），输出 $12（约 $0.018/分钟）/ 百万 token（S19） |
| 限制〔文档〕 | — | Gemini Live 纯音频会话约 15 分钟，单个连接约 10 分钟（结束前约 60 s 发送 GoAway）；上下文 128K，可开滑窗压缩；3.1 Flash Live 只支持同步函数调用。豆包实时语音于 2025-01-20 发布，支持随时打断和联网，API 细节未能核实（S20） |
| 对本项目的影响 | 文字可同步上屏；可在 TTS 之前过滤儿童内容；模型可自选（符合 D-07 质量优先） | 延迟低、语气自然；但难以同步上屏文字和做内容过滤，工具调用也受限 |

〔建议〕默认采用流式管线。设备协议对两种方式保持透明，与 xiaozhi 一样只传 Opus 和事件。

### 5.2 延迟预算（〔估算〕，从 PTT 松键到听到第一个音）

假设：

- 流式 ASR 收尾 0.1–0.3 s。
- LLM 首 token 0.4–0.9 s，TTS 首音 0.25–0.35 s（S4）。
- 播放缓冲 1–2 个 60 ms 帧。
- Wi-Fi 到 VPS 单程数十毫秒；商用 LTE 往返 50–200 ms（S21，CMU 2022）。

| 场景 | Wi-Fi | 4G |
|---|---|---|
| 不调用工具 | 1.0–1.8 s | 1.2–2.2 s |
| 调用 1 次设备工具（含第二次 LLM） | +0.5–1.1 s | +0.6–1.3 s |
| 用回执中的 `say` 跳过第二次 LLM | +0.1–0.2 s | +0.15–0.4 s |

参照：语音 agent 的常见目标约为 800 ms；各环节串行相加约 1.18 s，只有让各环节流水重叠才能接近目标（S21）。

〔建议〕

- 按键后 150 ms 内，本地局部刷新显示“正在听”，与网络无关。
- 按下瞬间预连 WSS 并发送页面快照，让上下文传输与用户说话重叠进行。
- PTT 本身省去了 VAD 判断说话结束的等待时间（约 200–400 ms）。

### 5.3 打断

〔文档〕xiaozhi 的做法：用户再按键或说唤醒词时，设备发出 `abort`，服务端清空队列并回发 `tts stop`。

〔建议〕

- 设备本地立即停止播放。
- `abort` 中附带最后一个 `sentence_start` 的序号，中枢据此把对话历史截断到用户实际听到的位置。
- 已经执行的工具不自动回滚，改为提供撤销入口。
- 在验证设备具备 AEC 之前，不做全双工对话。

### 5.4 上下文预算（8 KB 页面快照）

〔估算〕8 KB 的 UTF-8 中文约 2,700 字，约合 2–3k token。快照分四层：

- T0 头部，≤0.5 KB。
- T1 可见语义块，≤5 KB；每块截断，选区前后各保留约 80 字。
- T2 当前可用的动作，≤1 KB。
- T3 展开指针，≤0.5 KB；用 URI 指向前后文、目录和历史。
- 另留余量约 1 KB。

每轮提示词由以下几部分组成：系统提示和工具目录（作为稳定前缀，可缓存）；成员摘要（≤500 token）；页面快照；近期对话（超出部分改为摘要）。

### 5.5 成员记忆

按 member_id（而不是 device_id）分别存储：

- 档案：年龄、阅读水平、偏好。
- 对话摘要。
- 事实，并标注来源：观察所得、用户陈述或模型推断。
- 学习状态：以设备数据为准，同步到中枢。
- 活动日志：由设备事件生成。

声纹只作为辅助线索，不作为授权依据。

### 5.6 儿童安全

〔文档〕相关规范：

- 英国 ICO《适龄设计规范》要求：默认高隐私；用户画像默认关闭；不得用助推手段诱导儿童提供数据；家长监控须让孩子知情；联网玩具和设备须提供合规工具（S22）。
- 《生成式人工智能服务管理暂行办法》（2023-08-15 起施行）第十条要求防范未成年人过度依赖或沉迷；第十一条规定不得非法留存能识别身份的输入信息和使用记录。

〔建议〕中枢侧措施：

- 儿童专用系统提示：面向 5–6 岁，回答简短，可朗读。
- TTS 之前先审核输出；主题用白名单，遇到不宜话题温和转开。
- 开放给模型的工具 = 儿童白名单 ∩ 当前页面的可用动作。
- 禁止从任意网址导入内容。
- 限制单次会话和每日时长。
- 原始转写只短期保留。
- 给家长发日报，家长可删除和导出记录。

设备侧措施见 §6.6。

### 5.7 成本控制

- 模型路由：意图分类和摘要交给便宜模型。
- 稳定前缀加确定性工具顺序，提高 prompt cache 命中率。
- 翻页、音量、播放等本地快捷指令不经过 LLM。
- 用回执中的 `say` 省去第二次调用。
- 页面快照分层，按需展开。
- 按成员和按日设置预算，每轮工具调用设上限（如 ≤8 次）。
- 故事和报纸的 TTS 在离峰时段预先生成并缓存。
- 只在 PTT 期间上传音频。

## 6. 综合建议

### 6.1 能力模型

- **实体。**`book`、`locator`（书内位置）、`audio_item` 及播放位置、`issue/article`（报纸的期与文章）、`memo`、`word`（含间隔复习状态）、`member`、`setting`；后续再加 `sms_thread`、`call_log`、`contact`。每个实体都有 URI（如 `word://{member}/{id}`）和版本号 rev。
- **动作。**
  - 命名为 `app.verb`，参数用 JSON Schema 描述，限于设备能校验的子集：string、int、bool、enum、短数组。
  - 每个动作带 hints（映射到 MCP 注解）和 policy 两组属性。
  - policy 包括：`effect`（read/nav/local_write/destructive/external/system）、`confirm`（none/device/parent）、`kid`（allow/deny/parent）、`undo`、`rate`。
- **页面上下文。**
  - 与界面同源：快照和屏幕显示出自同一个状态对象。
  - 没有实现自定义快照的页面，用 LVGL 控件树兜底（类似 Android 的 AssistStructure）。
  - 阅读排版、游戏画面等自绘页面必须自行实现快照。
  - 标记为 `private:true` 的页面（PIN 输入，日后的短信草稿）只提供头部信息（类似 FLAG_SECURE）。
  - 快照只在用户唤起 AI，或会话中中枢请求时才生成。
- **事件。**`page.enter/leave`、`action.done`（无论用户还是 AI 发起，都附回执）、`media.state`、`member.switch`、`task.progress/done`、`confirm.result`。事件带序号 seq，中枢确认（ack）后设备才清理，以保证历史完整。

### 6.2 协议选择

| 选项 | 评价 |
|---|---|
| 设备直接实现完整的 MCP 2026-07-28 | 不推荐。2024-11-05 以来规范已有多次破坏性修订，固件升级成本高；无状态 `_meta`、MRTR 等机制对单连接设备没有好处；音频、页面上下文、确认仍要另行自定义 |
| **沿用 MCP 形状的私有 JSON-RPC profile（推荐）** | 在 xiaozhi 的 WSS 信封 `type:"mcp"` 中沿用 tools/list、tools/call、resources/read 及其结果结构，可复用上游代码和 xinnan 实验台。新增 `type:"ink"` 信封承载厂商方法（`ink/context`、`ink/intent`、`ink/event`、`ink/ui`、`ink/confirm`）。版本号由本项目自管，由中枢负责转换为标准 MCP |
| 完全自定义的二进制格式（CBOR/protobuf） | 暂不需要。8 KB 量级的 JSON 在 4G 上约需 1 个往返；以后按实测再定 |

传输方式：v0.1 用 WSS（每次会话建立连接）加 HTTPS 同步。报纸就绪、提醒等常驻推送是否改用 MQTT，按待机功耗实测结果决定。提醒在设备本地排程，不依赖推送。

一次请求的消息顺序：

1. 用户按下 AI 键：设备局部刷新显示“正在听”，并预连 WSS；随即发送 `ink/context`（快照，`rev` 为按键时刻的值）。
2. 设备发送 `listen start(manual)` 和 Opus 音频，松键时发送 `stop`。如果请求来自界面（例如在选区菜单点“解释”），则发送 `ink/intent{verb,target}`，不传音频。
3. 中枢依次执行 ASR、组装提示词、调用 LLM。
4. LLM 发起 `tools/call` 后，设备检查策略、执行动作并返回回执。
5. 中枢用回执中的 `say` 直接回复，或再调用一轮 LLM；然后发送 TTS 音频流，文字随 `sentence_start` 上屏。
6. 设备发出 `ink/event action.done`，写入该成员的活动日志。

### 6.3 设备与中枢的分工

**设备（可信的执行者）：**

- 能力注册表与参数校验。
- 策略闸门：成员身份、儿童白名单、确认、限速、可见性。
- 回执与撤销日志。
- 快照提供者与事件日志。
- 本地快捷指令。
- Opus 采集与播放。
- ink-ui 渲染与确认界面。
- 离线数据（D-05）。

**中枢（编排者）：**

- 会话管理；ASR/LLM/TTS 管线或 S2S。
- 意图路由；工具目录缓存与渐进披露；提示词组装。
- 成员记忆；内容安全；成本控制。
- 长任务：生成报纸、转换书籍、预生成 TTS。
- 生成声明式页面。
- 对外 MCP 网关，按成员授权裁剪 tools/list。
- 审计与家长端。

### 6.4 发现与渐进披露

1. `hello` 中只带 `manifest_hash`。中枢按哈希缓存工具目录，缺失时才分页拉取；同一固件版本的设备共用一份缓存。
2. 每轮常驻上下文包括：L0 目录索引（app 名加一句话简介，≤1 KB），以及当前 app 和系统常用动作的完整定义（≤15 个，按固定顺序排列）。
3. 其余能力通过元工具获取：`find_capabilities(query)` 返回名称和简短描述；`load_capabilities(names)` 在下一轮加入完整 schema。题库、全文、历史等大块数据只返回 URI，需要时再调用 `resources/read`。
4. 快照中的 `affordances` 告诉模型此刻哪些动作有效，而工具集合本身保持不变，以利于缓存。

### 6.5 回执与确认

- **状态集合。**`done`；`queued`（附任务 handle，进度通过事件报告）；`pending_confirmation`；`rejected`（被策略拒绝）；`stale`（rev 不一致）；`invalid`；`failed`；`cancelled`；`undone`。工具不存在或参数错误走 JSON-RPC error；业务失败用 `isError:true` 返回。
- **幂等。**每次调用在 `_meta["ink/req"]` 中携带请求 ID，设备记住近期的请求 ID，断线重连后重发不会重复执行。
- **防止操作过期状态。**调用时附上模型看到的 `ink/rev`。“这个词”之类的指代，用快照中的 ref 或 locator 传递，设备核对无误后才执行；不一致时返回 `stale` 和新的摘要。成员不一致时直接拒绝。
- **确认由设备决定，不由模型决定。**
  - 需要确认时，设备返回 `pending_confirmation`，本次调用随即结束，从而避开 30 s 超时。
  - 设备屏幕弹出确认卡（局部刷新），确认结果通过 `confirm.result` 或 `action.done` 事件回报。
  - 发短信、打电话等对外有影响的操作，必须在屏幕上点按确认；语音确认只限低风险操作。
- **撤销。**可逆的本地操作返回撤销令牌（例如 10 分钟内有效），通过 `system.undo(token)` 撤销。
- **完成标准。**只有收到回执，才能对用户说“已完成”。

### 6.6 儿童模式（设备侧执行的部分）

- **成员切换。**从儿童切到成人须输入家长 PIN：孩子识字，只放一个按钮挡不住；声纹也不作为授权依据。
- **动作放行。**每个动作按 `kid` 策略放行、拒绝或要求家长确认。系统设置、OTA、删除、对外通信、从任意网址导入内容默认禁止。
- **数据范围。**实体查询按成员 ACL 过滤，儿童只能看到儿童书架。
- **时长控制。**每日 AI 使用时长和静默时段由设备本地计时。
- **透明提示。**麦克风开启状态和“家长可以查看记录”的提示在屏幕上常显。
- **唤醒方式。**〔建议〕儿童模式默认用 PTT，不开启常听唤醒词，兼顾隐私和功耗。

### 6.7 最小声明式界面：ink-ui v0

- **线格式。**沿用 A2UI v0.9 的四种消息，每条 A2UI 消息作为 `ink/ui` 通知的 params 下发。这样中枢可以直接使用 A2UI 的 SDK、校验器和示例；v1.0 稳定后再跟进。
- **组件目录 `ink.eink/v0`（14 个）。**
  - 取自 A2UI basic 的 13 个：Column、Row、Card、List、Text（h1/h2/body/caption）、Image、Icon、Divider、Button、ChoicePicker、CheckBox、AudioPlayer、TextField（v0.2 起提供，走平台输入法）。
  - 自定义 1 个：Progress（静态进度条）。
  - Image 由中枢预先抖动为 1bpp 或 4bpp，并缩放到目标尺寸。
- **舍弃的组件及原因。**
  - Video：墨水屏不适合播放视频。
  - Tabs：改用 ChoicePicker 加分页。
  - Slider：拖动操作在墨水屏上体验差，改用 ± 按钮。
  - DateTimeInput：改用 ChoicePicker 或输入法。
  - Modal：确认只能由系统确认卡完成，并使用页面无法模仿的专属外框，防止生成的界面冒充系统对话框。
- **墨水屏规则。**
  - 不滚动；内容溢出时自动分页并显示页码。
  - 没有动画。
  - 刷新方式由设备决定：新建页面用高质量刷新，数据更新用快速刷新。
  - 〔估算〕触控目标不小于约 100 px（屏幕约 297 PPI，约 8.5 mm），儿童模式不小于 120 px。
  - 只使用设备已安装的字体；缺字可参考 glyph_push 的思路补发。
- **动作类型。**
  - `event`：发送给中枢。
  - 目录函数 `invokeAction`：调用本地已注册的动作，仍然经过策略闸门。按 v1.0 语义，它设为 rendererOnly 加 requiresUserActivation，即只能由用户点按触发。
  - 目录函数 `openApp(uri)`：打开 app 页面。
- **限额。**每个界面 ≤120 个组件，嵌套深度 ≤6，单条消息 ≤24 KB，数据模型 ≤32 KB，图片 ≤3 张。遇到未知组件或属性时整体拒绝并报错。不支持表达式，只提供少量格式化函数。

### 6.8 示例

(a) 设备 hello（在 xiaozhi 格式上扩展）：

```json
{"type":"hello","version":3,"transport":"websocket","features":{"mcp":true,"ink":1},
 "audio_params":{"format":"opus","sample_rate":16000,"channels":1,"frame_duration":60},
 "ink":{"fw":"0.1.0","manifest_hash":"sha256:9c1e…","ui_catalog":"ink.eink/v0","member":"m_kid1","mode":"kid","net":"lte"}}
```

(b) 单词本 app 的能力清单（节选）。字段与 MCP 的对应关系：`desc` 对应 description，`input` 对应 inputSchema，`hints` 对应 annotations 中的 \*Hint，`policy` 放入 `_meta["ink/policy"]`。

```json
{"app":"words","ver":"0.1.0","title":"单词本","summary":"收集生词并按间隔复习；离线可用",
 "entities":{"word":{"uri":"word://{member}/{id}",
   "fields":{"term":"string","lang":"string","gloss":"string","box":"int","due":"date","source":"locator"}}},
 "actions":[
  {"name":"words.add","title":"加入单词本","desc":"把一个词加入当前成员的单词本，可附出处；已存在则只补充出处",
   "input":{"type":"object","additionalProperties":false,"required":["term"],"properties":{
     "term":{"type":"string","maxLength":32},"lang":{"type":"string","enum":["zh","en","ja"]},
     "gloss":{"type":"string","maxLength":120},"source":{"type":"string","description":"locator，如 book://b42#c3:p8:o16-o18"}}},
   "output":"receipt","hints":{"readOnly":false,"destructive":false,"idempotent":true,"openWorld":false},
   "policy":{"effect":"local_write","confirm":"none","kid":"allow","undo":true,"rate":"20/min"}},
  {"name":"words.list_due","desc":"列出今天到期需复习的词（分页）",
   "input":{"type":"object","properties":{"limit":{"type":"integer","maximum":20},"cursor":{"type":"string"}}},
   "hints":{"readOnly":true},"policy":{"effect":"read","kid":"allow"}},
  {"name":"words.delete","desc":"删除一个词及其复习记录",
   "input":{"type":"object","required":["id"],"properties":{"id":{"type":"string"}}},
   "hints":{"readOnly":false,"destructive":true,"idempotent":true,"openWorld":false},
   "policy":{"effect":"destructive","confirm":"device","kid":"parent","undo":true}}],
 "events":["words.added","words.reviewed","words.deleted"]}
```

(c) 阅读页快照（书名与正文为虚构内容，约 1.1 KB）：

```json
{"v":1,"rev":1842,"ts":"2026-11-01T11:15:03Z","member":{"id":"m_kid1","mode":"kid"},
 "app":"reader","page":"reader.book","entity":"book://b42","title":"小狐狸找朋友",
 "loc":{"chapter":3,"page":"18/64","at":"book://b42#c3:p7"},
 "visible":[{"ref":"p7","role":"para","text":"小狐狸每天都来看小鹿，可小鹿总是躲在树后面。"},
            {"ref":"p8","role":"para","text":"妈妈说：“别着急，慢慢地，它就会信任你了。”"}],
 "selection":{"ref":"p8","text":"信任","at":"book://b42#c3:p8:o16-o18","around":"慢慢地，它就会信任你了。"},
 "media":{"tts":{"state":"paused","at":"book://b42#c3:p7"}},
 "ui":{"modal":null,"focus":"selection_menu"},
 "affordances":["reader.next_page","reader.read_aloud","reader.explain","words.add","memo.create"],
 "more":{"before":"book://b42#c3:p1-p6","after":"book://b42#c3:p9-","toc":"book://b42/toc"}}
```

(d) 工具调用与回执：

```json
{"session_id":"s-7f3a","type":"mcp","payload":{"jsonrpc":"2.0","id":31,"method":"tools/call","params":{
 "name":"words.add","arguments":{"term":"信任","lang":"zh","gloss":"相信对方，愿意依靠他","source":"book://b42#c3:p8:o16-o18"},
 "_meta":{"ink/req":"r-0031","ink/rev":1842,"ink/member":"m_kid1","progressToken":31}}}}
```

```json
{"session_id":"s-7f3a","type":"mcp","payload":{"jsonrpc":"2.0","id":31,"result":{
 "content":[{"type":"text","text":"已加入单词本：信任（明天复习）"}],
 "structuredContent":{"status":"done","req":"r-0031","receipt":"rc-0931",
   "changed":[{"uri":"word://m_kid1/w588","op":"create","rev":1}],
   "say":"好啦，“信任”放进单词本了，明天我们一起复习。","undo":{"token":"u-0931","ttl_s":600},"rev":1843},
 "isError":false}}}
```

需要确认的动作：设备先返回下面的 structuredContent，确认完成后再发事件。

```json
{"status":"pending_confirmation","req":"r-0032","confirm":"c-12","prompt":"删除单词“信任”和它的复习记录？","needs":"parent","ttl_s":120}
```

```json
{"type":"ink","payload":{"jsonrpc":"2.0","method":"ink/event","params":{"seq":5521,"type":"confirm.result",
 "req":"r-0032","result":"approved","by":"parent_pin","status":"done","receipt":"rc-0932"}}}
```

(e) ink-ui 测验卡（A2UI v0.9 消息，各条作为 `ink/ui` 的 params 下发）：

```json
{"version":"v0.9","createSurface":{"surfaceId":"quiz-1","catalogId":"ink.eink/v0"}}
{"version":"v0.9","updateComponents":{"surfaceId":"quiz-1","components":[
 {"id":"root","component":"Column","children":["q","pick","ok"]},
 {"id":"q","component":"Text","variant":"h2","text":{"path":"/q"}},
 {"id":"pick","component":"ChoicePicker","variant":"mutuallyExclusive","value":{"path":"/ans"},
  "options":[{"label":"相信对方","value":"a"},{"label":"跑得很快","value":"b"},{"label":"藏起来","value":"c"}]},
 {"id":"ok_t","component":"Text","text":"确定"},
 {"id":"ok","component":"Button","child":"ok_t","variant":"primary",
  "action":{"event":{"name":"quiz.submit","context":{"ans":{"path":"/ans"}}}}}]}}
{"version":"v0.9","updateDataModel":{"surfaceId":"quiz-1","path":"/","value":{"q":"“信任”最接近哪个意思？","ans":[]}}}
```

### 6.9 下一步验证（硬件到货前就能做）

1. **第 0 期实验台。**在 VPS 上运行 xinnan 服务端，把上游固件的小智通道指向它（需要改编译常量或配置，尚未验证）。分段测量 Wi-Fi 和 4G 下从按键到首音的耗时，以及 tools/call 的往返时间。
2. **渲染与快照原型。**在 PC 上的 LVGL 9.5 模拟器里实现 ink-ui v0 渲染器和阅读页快照提供者，测量 JSON 生成耗时和内存占用。
3. **中枢原型。**实现目录缓存、两级渐进披露和回执 `say`，比较有无第二次 LLM 调用时的延迟和费用。
4. **儿童策略红队清单。**覆盖越权调用动作、诱导联网、索取个人信息等场景。

## 7. 未能核实

1. xiaozhi.me 官方服务端是闭源的，其 MCP 调度、多用户处理和延迟情况无从核实。
2. 豆包实时语音 API 的协议、价格和延迟：火山引擎文档访问被拒，只有 2025-01 的 Seed 博客和一篇非官方博文。“3.0 API 于 2026-06-18 上线”只看到新闻标题。
3. OpenAI Realtime 打断时如何截断对话（conversation.item.truncate）、音频 token 与分钟数如何换算，以及 gpt-realtime-2.1 的连接方式，官方页面均未写明。
4. Apple 的 “Providing contextual cues” 和 “App schema domains” 两篇正文抓取被拒；Android AppFunctions 的确认交互、Windows 的审计日志细节，文档均未提及。
5. Rabbit LAM 的实际实现方式、CosmOS 的技术细节，都只有宣传性描述。
6. A2UI v1.0 basic 目录的组件清单（只核对了 v0.9）；ChoicePicker 的 options 能否绑定数据路径；LVGL 9.5 运行时 XML 的文档正文（现有结论依据官方博客）。
7. Plus 的 PSRAM 容量、WebSocket 帧缓冲上限、ESP-IDF WebSocket 是否支持压缩，以及上游固件能否不重新编译就改连自建的小智服务器。
8. NT26 模组在实际运营商网络下的往返时延、墨水屏局部刷新的首帧耗时。本文中的延迟、token 数和触控尺寸都是估算。
9. xinnan 性能报告中 qwen-flash 的数值，结论表与明细表不一致；作者也自述测试方法不严谨。
10. 有三个页面抓取失败，已改用对应仓库中的同版本文件核对：MCP tools 规范页、Vercel AI SDK 文档页、xiaozhi-performance-research 仓库页。
11. Anthropic 关于渐进披露的文章，沿用项目旧文档的引用，本次没有重读。

## 8. 来源（查阅日期均为 2026-11-01）

- S1 xiaozhi-esp32 @a78ca19：https://github.com/78/xiaozhi-esp32/tree/a78ca19d5bc9a12ee0853b9f509530276d36aa03/docs （websocket.md、mqtt-udp.md、mcp-protocol.md、mcp-usage.md、notify.md、glyph-push.md；main/mcp_server.cc；README_zh.md）
- S2 CloudZao Plus @690a023：https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/690a023c66862af103a3187464373ee7c5efcd14/main （mcp_server.cc/.h、protocols/、application.cc、chat_cloud.h、idf_component.yml；../sdkconfig）
- S3 xinnan-tech/xiaozhi-esp32-server @9233788：https://github.com/xinnan-tech/xiaozhi-esp32-server/tree/92337881a7b6553614b47e137a774d02bf40512f （core/providers/tools/device_mcp/、core/handle/helloHandle.py、abortHandle.py、core/providers/asr/base.py、core/connection.py、docs/context-provider-integration.md、docs/mcp-endpoint-integration.md、README.md）
- S4 https://github.com/xinnan-tech/xiaozhi-performance-research @90c6601（README）
- S5 https://modelcontextprotocol.io/specification/2026-07-28/changelog ；https://github.com/modelcontextprotocol/specification/releases/tag/2026-07-28 ；规范仓库 https://github.com/modelcontextprotocol/modelcontextprotocol/tree/d0e246ea3c69eafaa2806bc4cd28025703ba2c2b/docs/specification/2026-07-28 （server/tools.mdx、resources.mdx、discover.mdx、basic/patterns/、basic/transports/index.mdx、client/elicitation.mdx；schema/2026-07-28/schema.ts）
- S6 同一仓库下的 docs/extensions（overview、apps、tasks、skills、client-matrix）
- S7 https://developers.openai.com/apps-sdk/mcp-apps-in-chatgpt ；https://developers.openai.com/apps-sdk/reference
- S8 https://github.com/MCP-UI-Org/mcp-ui
- S9 https://github.com/google/A2UI @825beac（specification/v0_9、v1_0）；https://developers.googleblog.com/en/a2ui-v0-9-generative-ui/
- S10 https://github.com/vercel/ai/blob/dffe4053c15709577329518044b1ec1d4c6bb0fe/content/docs/04-ai-sdk-ui/04-generative-user-interfaces.mdx ；同目录 05-ai-sdk-rsc/01-overview.mdx
- S11 https://learn.microsoft.com/en-us/microsoft-copilot-studio/guidance/adaptive-cards-overview ；https://adaptivecards.io/explorer/Refresh.html
- S12 https://lvgl.io/blog/announcement-lv_xml-removal-from-v9-5
- S13 https://developer.apple.com/documentation/updates/appintents.md ；https://developer.apple.com/documentation/appintents/apple-intelligence-and-siri-ai.md ；https://developer.apple.com/documentation/appintents/appentityuielement.md ；https://developer.apple.com/documentation/foundation/nsuseractivity/appentityidentifier.md ；https://heise.de/-10008604
- S14 https://developer.android.com/ai/appfunctions ；https://developer.android.com/training/articles/assistant ；https://9to5google.com/2026/02/25/android-appfunctions-gemini/ ；https://9to5google.com/2026/01/15/android-16-qpr3-screen-automation/
- S15 https://learn.microsoft.com/en-us/windows/ai/mcp/overview ；https://blogs.windows.com/windowsexperience/2025/10/16/securing-ai-agents-on-windows/
- S16 https://en.wikipedia.org/wiki/Rabbit_r1
- S17 https://techcrunch.com/2025/02/18/humanes-ai-pin-is-dead-as-hp-buys-startups-assets-for-116m ；https://techcrunch.com/2024/12/05/ai-pin-maker-humane-is-developing-ai-software-for-cars-phones-and-smart-speakers
- S18 https://developers.openai.com/api/docs/models/gpt-realtime.md ；https://developers.openai.com/api/docs/models/gpt-realtime-2.1.md ；https://developers.openai.com/api/docs/guides/realtime-vad
- S19 https://ai.google.dev/gemini-api/docs/pricing ；https://firebase.google.cn/docs/ai-logic/live-api/sessions ；https://ai.google.dev/gemini-api/docs/live-api/tools.md.txt
- S20 https://seed.bytedance.com/zh/blog/%E8%B1%86%E5%8C%85%E5%AE%9E%E6%97%B6%E8%AF%AD%E9%9F%B3%E5%A4%A7%E6%A8%A1%E5%9E%8B%E4%B8%8A%E7%BA%BF%E5%8D%B3%E5%BC%80%E6%94%BE-%E6%83%85%E5%95%86%E6%99%BA%E5%95%86%E5%8F%8C%E9%AB%98 ；非官方博文 https://deepseek.csdn.net/6a0ac0e8662f9a54cb7561b4.html ；官方文档入口（访问被拒）https://www.volcengine.com/docs/6561/1594356
- S21 https://soniox.com/wiki/voice-agent-latency-budget ；https://www.cmu.edu/scs/edgecomputing/articles/latency-segmentation.html
- S22 https://gov.uk/guidance/child-online-safety-data-protection-and-privacy ；https://www.cac.gov.cn/2023-07/13/c_1690898327029107.htm
- S23 https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/ai/xiaozhi.html
- S24 （项目旧文档引用，本次未重读）https://www.anthropic.com/research/code-execution-with-mcp
