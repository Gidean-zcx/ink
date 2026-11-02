# C. AI 原生设备架构与能力协议调研（服务 G-002）

查阅：2026-11-01（UTC）。标注：〔文档〕官方或上游文档；〔代码〕本次在固定提交上读过的代码；〔报道〕媒体或第三方资料；〔推断〕〔估算〕〔建议〕为本文判断。没有实机测试。来源编号见 §8。

## 0. 结论

1. 〔代码〕上游 Plus 固件（v2.0.71）由 xiaozhi-esp32 派生：WebSocket 或 MQTT+UDP 传输，`hello` 握手，在 `type:"mcp"` 信封里传 JSON-RPC，设备充当 MCP server。但开放给 AI 的只有 ≤3 个工具（状态、音量、亮度），书库、进度、听书都没开放，G-002 覆盖接近零。
2. 〔文档〕MCP 最新稳定版是 2026-07-28：取消握手和会话，新增 `server/discover`、`subscriptions/listen`、MRTR；Sampling、Roots、Logging 弃用。小智系设备仍停在 2024-11-05 子集；项目旧文档引用的 2025-11-25 已被取代。〔建议〕设备链路用 MCP 形状的私有 profile，中枢对外提供标准 MCP。
3. 〔建议〕能力模型分四部分：实体（URI + rev）、动作（JSON Schema + 风险/确认/儿童策略）、按需页面快照（≤8 KB）、事件日志。策略由设备执行，模型只负责提议。
4. 〔文档〕MCP Apps、OpenAI Apps SDK、MCP-UI 都是沙箱 iframe 里的 HTML，MCU 渲染不了；LVGL 9.5 起，运行时加载 XML 要付费许可。〔建议〕采用 A2UI v0.9 线格式，配 14 组件的自有目录。
5. 〔建议〕语音默认用“流式 ASR → LLM → 流式 TTS”加按键说话（PTT），因为文字能同步上屏、能在 TTS 前过滤儿童内容、模型可自选；端到端语音模型（S2S）作可选。〔估算〕松键到首音：Wi-Fi 1.0–2.0 s，4G 1.1–2.4 s。回执里带一句可直接播报的 `say`，可省掉二次 LLM，工具调用的增量从 0.5–1.5 s 降到 0.1–0.6 s。
6. 〔代码〕xinnan 服务端按 `device_id` 存记忆，声纹只把说话人姓名写进提示词。共享设备要按成员存记忆，用家长 PIN 授权。

## 1. xiaozhi-esp32 协议族

### 1.1 传输与消息（〔文档〕S1，〔代码〕S2）

| 项目 | 内容 |
|---|---|
| WebSocket | 握手头：`Authorization: Bearer`、`Protocol-Version`、`Device-Id`（MAC）、`Client-Id`（UUID）。设备先发 `hello`，10 s 内须收到服务端 hello。文本帧传 JSON，二进制帧传 Opus：v1 裸帧，v2 带 timestamp（供服务端 AEC），v3 为 4 字节头 |
| MQTT+UDP | MQTT 传控制（keepalive 240 s）；服务端 hello 下发 UDP 地址和 AES key/nonce；UDP 音频用 AES-CTR 加密并带序号 |
| 音频 | 上行 Opus 16 kHz 单声道、60 ms 帧；下行可 24 kHz；listening 状态丢弃下行音频 |
| 设备→服务端 | `hello`、`listen`（start/stop/detect；auto/manual/realtime）、`abort`、`mcp`、`goodbye` |
| 服务端→设备 | `hello`、`stt`、`llm`（emotion）、`tts`（start/stop/sentence_start+text）、`mcp`、`system`、`alert`、`custom`、`goodbye`。新版另有 `notify`（空闲时单向播放 Ogg Opus 加字幕，无回执）和 `glyph_push`（补发缺失字形）。`iot` 已弃用 |

### 1.2 设备作为 MCP server（〔文档〕S1，〔代码〕S1/S2）

- **范围。**连接由设备拨出，适合 4G/NAT。只实现 `initialize`（返回 `protocolVersion:"2024-11-05"`，只声明 tools）、`tools/list`、`tools/call`；收到 `notifications/*` 直接忽略。
- **隐藏工具。**`tools/list` 每页 ≤8000 字节，cursor 就是工具名。“user-only”工具只在请求带非标准参数 `withUserTools:true` 时列出，标记写作 `annotations.audience:["user"]`，MCP 的 ToolAnnotations 并无此字段。
- **安全缺陷：隐藏不等于禁止。**`tools/call` 不检查 user-only 标记，知道工具名就能调用 `reboot`、`upgrade_firmware`。
- **参数与返回。**参数只有 bool/int/string（可带 min/max/default）；返回 `content:[{type:"text"}]` 加 `isError`，没有 outputSchema 和 structuredContent。工具串行执行；常用工具排在列表前部，以命中 prompt cache（源码注释）。
- **命名。**`self.模块.动作`，描述写给模型看，例如“不知道当前音量时先调用 `self.get_device_status`”。

### 1.3 服务端（xinnan-tech，〔代码〕S3）

- **工具拉取。**每次连接收到 `features.mcp` 后，立刻发 `initialize` 和 `tools/list`，翻页取完后并入 LLM 函数列表。工具名中除 `[a-zA-Z0-9_-]` 与汉字外的字符替换为 `_`。
- **调用。**超时 30 s。取 `content[0].text` 交给 LLM 再生成一轮；结果是含 `action` 的 JSON 时直接回复，跳过二次 LLM。`abort` 会清空 LLM/TTS 队列并回发 `tts stop`。
- **其他。**工具来源有五类：服务端插件、服务端 MCP、设备 IoT、设备 MCP、MCP 接入点。“上下文源”机制在唤醒时把外部 HTTP 数据注入提示词。README 自述“未通过网络安全测评，请勿在生产环境中使用”。MIT 许可。

### 1.4 上游 Plus 现状（〔代码〕S2，提交 690a023，2026-10-30）

- **云服务。**三选一：百问（默认，CloudZao 自有接口）、小智、云信。小智协议默认手动拾音；WebSocket 只在需要音频时才连接；120 s 无数据判定超时。
- **开放给 AI 的能力。**3 个 AI 工具（亮度工具需板级提供 backlight 才注册）、≤4 个 user-only 工具。截图、主题、图片预览在 S31 上被 `#if 0` 关闭。全仓没有其他工具注册；`notify`、`glyph_push` 未实现。
- **界面框架。**LVGL 锁定 9.5.0，已开 `LV_USE_OBSERVER`，可用于数据绑定。
- **上游支持。**〔文档〕小智上游已有 ESP32-S31 变体（需 IDF ≥6.1）。

### 1.5 延迟与多用户

- 〔文档〕S4（作者自称测试不严谨，广州联通）：流式 ASR 首词 0.80–0.98 s；LLM 首 token 0.59–0.90 s（qwen3-8b、qwen-flash、doubao-seed-2.0-mini、deepseek-v4-flash，长提示词）；流式 TTS 首音 0.25–0.71 s。没有端到端数据，也没有 4G 数据。
- 〔代码〕xinnan 每连接一个 handler，按 `device-id` 读私有配置；记忆用 `role_id=device_id`；声纹以 `{"speaker","content"}` 写进 LLM 输入，不参与授权。xiaozhi.me 闭源。

### 1.6 对本项目的含义（〔推断〕）

- **可复用：**传输层、Opus 参数、hello/abort/tts 事件、McpServer 的分页与参数校验骨架；xinnan 服务端可作第 0 期延迟实验台。
- **必须修改：**
  - 调用时强制执行可见性与策略。
  - 按清单哈希缓存工具目录。
  - 返回结构化回执。
  - 会话和记忆按成员区分。
  - UDP 音频改用带认证的 AEAD；现在的 CTR 没有 MAC。

## 2. MCP 2026-07-28（〔文档〕S5、S6）

### 2.1 主要变化

- 去掉 `initialize` 和 `Mcp-Session-Id`；每个请求在 `_meta` 带 `protocolVersion` 和 `clientCapabilities`。
- `server/discover` 必须实现；`subscriptions/listen` 取代 `resources/subscribe`。
- MRTR：服务端返回 `resultType:"input_required"`，客户端带 `inputResponses` 重试。
- Tasks 移入扩展（`tasks/get`、`tasks/update`）。
- 列表结果须带 `ttlMs` 和 `cacheScope`；`tools/list` 顺序确定，不随连接变化，可随授权变化。
- 响应流中断即丢失请求，须换新 id 重发。

### 2.2 相关要点

- **Tool。**
  - 字段：name（建议 1–128 字符，限 `A–Z a–z 0–9 _ - .`）、title、description、icons、inputSchema/outputSchema（JSON Schema 2020-12）。
  - 注解默认值：readOnly=false、destructive=true、idempotent=false、openWorld=true；不受信服务器的注解不得作为决策依据。
  - structuredContent 须符合 outputSchema；业务失败用 `isError:true`，协议错误走 JSON-RPC error。
  - 规范建议人在回路中，执行前确认；跨调用状态用显式 handle 参数传递。
- **Resources。**以 URI 标识，有 uriTemplate；注解为 audience、priority、lastModified。
- **其他机制。**进度用 `progressToken` + `notifications/progress`；取消用 `notifications/cancelled`，断开视同取消。Elicitation 有表单/URL 两种模式，凭证类信息必须走 URL。允许自定义传输，但须保留 JSON-RPC 格式和每请求元数据。
- **扩展。**MCP Apps（`io.modelcontextprotocol/ui`，Stable）、Tasks、Skills（SEP-2640 Final）。

### 2.3 MCU 与中枢分工（〔建议〕）

| 特性 | 设备 | 中枢 |
|---|---|---|
| tools/list 分页、tools/call、isError | 实现 | 缓存、检索、编排 |
| outputSchema/structuredContent、注解 | 返回回执 JSON；注解静态输出，策略由设备执行 | 校验，映射给模型和界面 |
| resources/read | 小集合：`page://current`、`book://`、`word://`、`memo://` | 模板、索引、订阅 |
| list_changed、progress、cancel | 安装或更新时通知；长任务报进度，尽力取消 | 对外 `subscriptions/listen`、Tasks |
| prompts、Skills、elicitation/MRTR | 不做，改为本地确认卡 | 对外实现 |
| sampling、roots、logging | 不做（已弃用） | 不做，改用 OpenTelemetry |
| MCP Apps | 不可能 | 可选，用于家长控制台 |
| 无状态 `_meta`、OAuth | 不采用，用设备令牌 + WSS | 对外实现 |

## 3. 声明式界面

| 方案 | 机制 | 是否适合 1216×684 墨水屏 + LVGL |
|---|---|---|
| A2UI v0.9.1（v1.0 候选，S9） | JSON 消息：createSurface、updateComponents、updateDataModel、deleteSurface。组件为扁平表，按 id 引用；JSON Pointer 绑定数据模型。渲染端声明 `supportedCatalogIds`，agent 只能用目录内组件。动作可发事件或调本地函数。v1.0 增加 `allowedCallers` 和 `requiresUserActivation`（openUrl 只能由用户手势触发）。Apache-2.0 | 最合适：basic 18 个组件，minimal 5 个；需自写 C 渲染器并裁剪 |
| MCP Apps / OpenAI Apps SDK / MCP-UI（S6–S8） | `_meta.ui.resourceUri` 指向 `ui://` 资源（`text/html;profile=mcp-app`），沙箱 iframe 渲染，经 postMessage 通信；Apps SDK 已对齐此标准，`_meta` 只发给组件、不给模型 | 设备不可行；家长端可用 |
| Vercel AI SDK（S10） | `tool-${name}` 消息片段映射 React 组件（input-available、output-available、output-error）；RSC 仍为实验功能 | 借鉴“输出类型对应组件” |
| Adaptive Cards（S11） | 宿主用原生控件渲染；Copilot Studio 支持到 1.6，Teams 到 1.5，Web Chat 不支持 Action.Execute | 可行，但宿主差异大 |
| LVGL XML（S12） | 9.5 起 `lv_xml_*` 移出开源仓库，运行时加载需付费许可 | 不宜作为下发格式 |

## 4. 各平台的 OS 级 AI 集成

| 平台 | 屏幕内容 | 动作 / 数据 / 发现 | 确认 | 用户与隐私 |
|---|---|---|---|---|
| Apple（S13） | 视图或 NSUserActivity 关联 AppEntity（`appEntityIdentifier`，iOS 18.2+）；自绘视图用 AppEntityUIElement（18.4+），用户可说“这张照片” | AppIntent + 领域 schema；IndexedEntity 进 Spotlight 语义索引；Donation 提供行为线索；Transferable 跨 app；EntityCollection 只存 ID | `requestConfirmation(conditions:)`；2026-06 起共享实体的破坏性操作经 OwnershipProvidingEntity 确认；UndoableIntent、CancellableIntent | 〔报道〕只在用户提问时读屏，发给第三方前先征询 |
| Android（S14） | AssistStructure（无障碍视图树）、onProvideAssistContent（JSON-LD 或 URI）；FLAG_SECURE 逐窗口排除 | AppFunctions（Android 16+，实验）：@AppFunction 标注，KDoc 即描述，生成 XML schema 供系统索引，本地执行；〔报道〕未接入的 app 由 Gemini 做 UI 自动化 | 文档未提及；〔报道〕屏幕自动化权限默认每次询问，可停止任务 | 调用方须持有 EXECUTE_APP_FUNCTIONS；Gemini 接入截至 2026-05 仍在私测 |
| Windows（S15） | agent 在独立工作区（单独桌面）操作 | MCP server 注册到 On-device Agent Registry（odr.exe）；内置文件、设置连接器；预览期只能访问几个已知文件夹 | 敏感操作可能需要额外批准 | agent 独立账户、最小权限、默认隔离、可审计；Intune 按 agent 管控 |

**另外两个参考：**Rabbit r1 宣称 LAM 能代用户操作网站，teach mode 官方说结果不可预测；2024 年出现硬编码密钥泄露、聊天日志不能删除（S16）。Humane CosmOS 只公开了“intelligent conductor”的定位；AI Pin 于 2025-02-28 停服后，设备失去云端功能（S17）。

〔推断〕

- **可借鉴：**
  - 用实体绑定让 AI 理解屏幕，而不是靠像素；没有绑定的页面用视图树兜底。
  - 动作具备确认、撤销、取消语义。
  - 用行为捐赠和语义索引支持历史查询。
  - 敏感页面整体排除在快照之外。
  - 给 agent 独立身份，按最小权限授权。
- **应避免：**设备上放密钥；数据不能删除；核心功能依赖云端。

## 5. 中枢 agent

### 5.1 流式管线与 S2S

〔文档〕

- OpenAI gpt-realtime-2.1：音频输入/输出 $32/$64、文本 $4/$24（每百万 token），上下文 128K（S18）。
- Gemini Live（如 3.1-flash-live-preview）：音频 $3/$12 每百万 token（约 $0.005/$0.018 每分钟）。纯音频会话约 15 分钟，单连接约 10 分钟（结束前约 60 s 发 GoAway），可开滑窗压缩；3.1 Flash Live 只支持同步函数调用（S19）。
- 豆包实时语音：2025-01 发布，可打断、可联网，API 细节未能核实（S20）。

〔建议〕默认用流式管线，以满足上屏、儿童内容过滤和 D-07 质量优先；S2S 作可选。设备协议对两者透明，只传 Opus 和事件。

### 5.2 延迟预算（〔估算〕，从松键到首音）

假设：ASR 收尾 0.1–0.3 s；LLM 首 token 0.4–0.9 s；首句累积 0.1–0.3 s；TTS 首音 0.25–0.35 s（S4）；商用 LTE 往返 50–200 ms（S21，CMU 2022）。

| 场景 | Wi-Fi | 4G |
|---|---|---|
| 不调用工具 | 1.0–2.0 s | 1.1–2.4 s |
| 1 次设备工具 + 二次 LLM | +0.5–1.3 s | +0.6–1.5 s |
| 1 次设备工具 + 回执 `say` | +0.1–0.4 s | +0.15–0.6 s |

参照：常见目标约 800 ms；各环节串行相加约 1.18 s，要靠流水重叠才能接近（S21）。

〔建议〕

- 按键后 150 ms 内，本地局刷显示“正在听”。
- 按下即预连 WSS 并发送快照，与用户说话重叠。
- PTT 省掉了判断说话结束的等待（200–400 ms）。

### 5.3 打断、上下文与记忆（〔建议〕）

- **打断。**设备本地立即停播，再发 `abort`（xiaozhi 已有此消息），并附上最后一个 `sentence_start` 的序号，中枢据此把对话历史截断到用户实际听到的位置。已执行的工具不回滚，改为提供撤销。验证 AEC 之前不做全双工。
- **快照预算。**〔估算〕8 KB 中文约 2,700 字、2–3k token。分层：头部 ≤0.5 KB；可见语义块 ≤5 KB（选区前后各约 80 字）；可用动作 ≤1 KB；展开指针 ≤0.5 KB。
- **每轮提示词。**稳定前缀（系统提示 + 工具目录，可缓存）+ 成员摘要（≤500 token）+ 快照 + 近期对话（超出部分转为摘要）。
- **记忆。**按 member_id 存：档案、对话摘要、带来源的事实（观察、陈述或推断）、学习状态（以设备为准）、活动日志。声纹只作线索。

### 5.4 儿童安全与成本

〔文档〕

- 英国 ICO《适龄设计规范》：默认高隐私；画像默认关闭；不得诱导儿童提供数据；家长监控须让孩子知情；联网设备须提供合规工具（S22）。
- 《生成式人工智能服务管理暂行办法》第十条要求防范未成年人沉迷；第十一条禁止非法留存能识别身份的输入信息和使用记录。

〔建议〕儿童安全（中枢侧）：

- 儿童专用系统提示，面向 5–6 岁。
- TTS 前审核输出，话题用白名单。
- 开放给模型的工具 = 儿童白名单 ∩ 当前页面可用动作。
- 禁止从任意网址导入。
- 原始转写只短期保留。
- 给家长发日报，记录可删除、可导出。

〔建议〕成本控制：

- 意图分类和摘要交给便宜模型。
- 稳定前缀加确定性工具顺序，提高缓存命中。
- 翻页、音量、播放走本地快捷指令。
- 用 `say` 省去二次调用。
- 按成员、按日设预算；每轮工具调用 ≤8 次。
- 故事和报纸的 TTS 离峰预生成。
- 只在 PTT 期间上传音频。

## 6. 方案建议

### 6.1 能力模型

- **实体。**`book`、`locator`（书内位置）、`audio_item`、`issue/article`、`memo`、`word`、`member`、`setting`；以后加 `sms_thread`、`call_log`、`contact`。每个实体有 URI（如 `word://{member}/{id}`）和 rev。
- **动作。**命名为 `app.verb`，参数用设备可校验的 JSON Schema 子集（string/int/bool/enum/短数组）；带 hints（映射到 MCP 注解）和 policy：`effect`（read/nav/local_write/destructive/external/system）、`confirm`（none/device/parent）、`kid`（allow/deny/parent）、`undo`、`rate`。
- **页面上下文。**
  - 与界面同源，只在用户唤起 AI 或会话中被请求时生成。
  - 没实现快照的页面用 LVGL 控件树兜底；自绘页面（阅读排版、游戏）必须自行提供。
  - `private:true` 页面（PIN、短信草稿）只给头部。
- **事件。**`page.enter`、`action.done`（用户或 AI 发起都带回执）、`media.state`、`member.switch`、`task.progress`、`confirm.result`。带 seq，中枢 ack 后设备才清理。

### 6.2 协议

| 选项 | 评价 |
|---|---|
| 设备实现完整 MCP 2026-07-28 | 不推荐：规范仍有破坏性修订；无状态 `_meta`、MRTR 对单连接设备无益；音频、上下文、确认仍需自定义 |
| **MCP 形状的私有 JSON-RPC profile（推荐）** | 在 xiaozhi WSS 的 `type:"mcp"` 信封里沿用 tools/list、tools/call、resources/read 及结果结构，复用上游代码和 xinnan 实验台；`type:"ink"` 信封承载 `ink/context`、`ink/intent`、`ink/event`、`ink/ui`、`ink/confirm`；版本自管，由中枢转换为标准 MCP |
| 自定义二进制（CBOR/protobuf） | 暂不需要；8 KB JSON 在 4G 上约 1 个往返 |

v0.1 用 WSS（每次会话连接）加 HTTPS 同步；常驻推送是否用 MQTT，按待机功耗实测决定；提醒在本地排程。

一次请求的消息顺序：

1. 按 AI 键：局刷显示“正在听”，预连 WSS，发 `ink/context`（rev 取按键时刻）。
2. 发 `listen start(manual)` 和 Opus，松键发 `stop`；从界面发起则发 `ink/intent{verb,target}`。
3. 中枢 ASR → 组装提示词 → LLM。
4. LLM 发 `tools/call`，设备检查策略、执行、返回回执。
5. 中枢用 `say` 回复或再调一轮 LLM，下发 TTS，文字随 `sentence_start` 上屏。
6. 设备发 `ink/event action.done`，写入成员活动日志。

### 6.3 分工

- **设备（可信执行者）：**能力注册与校验；策略闸门（成员、儿童白名单、确认、限速、可见性）；回执与撤销日志；快照与事件日志；本地快捷指令；Opus；ink-ui 渲染与确认卡；离线数据（D-05）。
- **中枢（编排者）：**会话与 ASR/LLM/TTS（或 S2S）；意图路由；目录缓存与渐进披露；提示词组装；成员记忆；内容安全；成本控制；长任务（报纸、书籍转换、TTS 预生成）；声明式页面；对外 MCP 网关（按成员裁剪 tools/list）；审计与家长端。

### 6.4 发现与渐进披露

1. `hello` 只带 `manifest_hash`；中枢按哈希缓存目录，缺失时才分页拉取。
2. 每轮常驻：L0 目录索引（app 名 + 一句话，≤1 KB），以及当前 app 和系统常用动作的完整定义（≤15 个，固定顺序）。
3. 其余能力通过元工具获取：`find_capabilities(query)` 返回名称和简述，`load_capabilities(names)` 在下一轮加入完整 schema；大数据只给 URI，需要时再 `resources/read`。
4. 快照里的 `affordances` 标出此刻有效的动作，工具集合保持不变，以利缓存。

### 6.5 回执与确认

- **状态。**`done`、`queued`（带任务 handle）、`pending_confirmation`、`rejected`、`stale`、`invalid`、`failed`、`cancelled`、`undone`。未知工具或参数错误走 JSON-RPC error；业务失败用 `isError:true`。
- **幂等。**`_meta["ink/req"]` 携带请求 ID，断线重发不重复执行。
- **防止操作过期状态。**附上模型看到的 `ink/rev`；指代用快照里的 ref 或 locator，设备核对后才执行，不符时返回 `stale` 和新摘要；成员不符直接拒绝。
- **确认由设备决定，不由模型决定。**设备返回 `pending_confirmation` 后本次调用即结束（避开 30 s 超时），屏幕弹出确认卡，结果经 `confirm.result` 事件回报。对外操作（短信、电话）必须点按确认，语音确认只用于低风险操作。
- **撤销。**可逆操作返回撤销令牌（如 10 分钟有效），通过 `system.undo(token)` 撤销。
- **完成标准。**只有收到回执，才能对用户说“已完成”。

### 6.6 儿童模式（设备侧）

- **成员切换。**从儿童切到成人须输家长 PIN：孩子识字，只放按钮挡不住；声纹不作为授权依据。
- **动作放行。**按 `kid` 策略放行、拒绝或要求家长确认。系统设置、OTA、删除、对外通信、任意网址导入默认禁止。
- **数据范围。**实体查询按成员 ACL 过滤。
- **时长控制。**每日时长和静默时段由本地计时。
- **透明提示。**麦克风状态和“家长可查看记录”常显。
- **唤醒方式。**默认 PTT，不开常听唤醒词。

### 6.7 最小声明式界面 ink-ui v0

- **线格式。**沿用 A2UI v0.9 的四种消息，每条作为 `ink/ui` 通知的 params 下发，可直接用 A2UI 的 SDK 和校验器。
- **组件目录 `ink.eink/v0`（14 个）。**
  - 取自 basic 的 13 个：Column、Row、Card、List、Text（h1/h2/body/caption）、Image（中枢预抖动为 1bpp 或 4bpp）、Icon、Divider、Button、ChoicePicker、CheckBox、AudioPlayer、TextField（v0.2 起，走平台输入法）。
  - 自定义 1 个：Progress。
- **舍弃的组件。**
  - Video：墨水屏不适合。
  - Tabs：改用 ChoicePicker 加分页。
  - Slider：改用 ± 按钮。
  - DateTimeInput：改用选择或输入法。
  - Modal：确认只能用系统确认卡，并用页面无法模仿的外框，防止冒充系统对话框。
- **墨水屏规则。**
  - 不滚动，溢出自动分页；无动画。
  - 刷新方式由设备选择：新页面高质量刷新，数据更新快速刷新。
  - 〔估算〕触控目标不小于约 100 px（约 297 PPI，≈8.5 mm），儿童不小于 120 px。
  - 只用已装字体。
- **动作。**
  - `event`：发送给中枢。
  - 目录函数 `invokeAction`：调用本地动作，仍经过策略闸门；按 v1.0 语义设为 rendererOnly + requiresUserActivation。
  - 目录函数 `openApp(uri)`：打开 app 页面。
- **限额。**≤120 个组件、嵌套 ≤6 层、单条消息 ≤24 KB、数据模型 ≤32 KB、图片 ≤3 张。遇到未知组件整体拒绝；不支持表达式。

### 6.8 示例

(a) 设备 hello（在 xiaozhi 格式上扩展）：

```json
{"type":"hello","version":3,"transport":"websocket","features":{"mcp":true,"ink":1},
 "audio_params":{"format":"opus","sample_rate":16000,"channels":1,"frame_duration":60},
 "ink":{"fw":"0.1.0","manifest_hash":"sha256:9c1e…","ui_catalog":"ink.eink/v0","member":"m_kid1","mode":"kid","net":"lte"}}
```

(b) 单词本 app 的能力清单（节选）。字段与 MCP 的对应关系：`desc` 对应 description，`input` 对应 inputSchema，`hints` 对应 annotations，`policy` 放入 `_meta["ink/policy"]`。

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

(c) 阅读页快照（书名与正文为虚构，约 0.9 KB）：

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

### 6.9 下一步验证（硬件到货前）

1. **第 0 期实验台。**VPS 上跑 xinnan 服务端，把上游固件的小智通道指向它（需改 OTA 常量或 NVS，未验证）。分段测量 Wi-Fi/4G 下按键到首音的耗时，以及 tools/call 往返。
2. **渲染与快照原型。**在 PC 上的 LVGL 9.5 模拟器里做 ink-ui 渲染器和阅读页快照，测耗时与内存。
3. **中枢原型。**做目录缓存、两级披露、回执 `say`，比较有无二次 LLM 的延迟与费用。
4. **儿童策略红队清单。**覆盖越权调用、诱导联网、索取个人信息。

## 7. 未能核实

1. xiaozhi.me 闭源，其 MCP 调度、多用户和延迟无从核实。
2. 豆包实时语音 API 的协议、价格、延迟：官方文档访问被拒，只有 2025-01 的 Seed 博客和一篇非官方博文；“3.0 API 于 2026-06-18 上线”只看到新闻标题。
3. OpenAI Realtime 打断时的截断机制、音频 token 与分钟数换算、2.1 的连接方式，官方页未写明。
4. Apple 的“Providing contextual cues”“App schema domains”正文抓取被拒；AppFunctions 的确认交互、Windows 审计细节，文档未提及。
5. Rabbit LAM 实现方式、CosmOS 技术细节，只有宣传性描述。
6. A2UI v1.0 basic 目录（只核对了 v0.9）；ChoicePicker 的 options 能否绑定数据；LVGL 9.5 XML 文档正文（结论依据官方博客）。
7. Plus 的 PSRAM 容量、WebSocket 缓冲上限；上游能否不重编译就改连自建服务器；亮度工具是否在 Plus 上注册。
8. NT26 实网往返时延、局刷首帧耗时；文中延迟、token 数、触控尺寸均为估算。
9. xinnan 报告中 qwen-flash 的结论表（0.434 s）与明细（0.670 s）不一致。
10. MCP tools 规范页、Vercel 文档页、xiaozhi-performance-research 页面抓取失败，已改用对应仓库的同版本文件核对。
11. Anthropic 渐进披露一文沿用旧引用，本次未重读。

## 8. 来源（查阅日期均为 2026-11-01）

- S1 xiaozhi-esp32 @a78ca19：https://github.com/78/xiaozhi-esp32/tree/a78ca19d5bc9a12ee0853b9f509530276d36aa03/docs （websocket.md、mqtt-udp.md、mcp-protocol.md、mcp-usage.md、notify.md、glyph-push.md；main/mcp_server.cc；README_zh.md）
- S2 CloudZao Plus @690a023：https://github.com/CloudZao/Metalio-E-INK4-Plus/tree/690a023c66862af103a3187464373ee7c5efcd14/main （mcp_server.cc/.h、protocols/、application.cc、chat_cloud.h、idf_component.yml；../sdkconfig）
- S3 xinnan-tech/xiaozhi-esp32-server @9233788：https://github.com/xinnan-tech/xiaozhi-esp32-server/tree/92337881a7b6553614b47e137a774d02bf40512f （core/providers/tools/device_mcp/、core/handle/helloHandle.py、abortHandle.py、core/providers/asr/base.py、core/connection.py、docs/context-provider-integration.md、README.md）
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
- S23 https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/ai/xiaozhi.html （乐鑫 esp_xiaozhi 组件，两种传输都配置时优先 MQTT）
- S24 （项目旧文档引用，本次未重读）https://www.anthropic.com/research/code-execution-with-mcp
