# hub-pair-1：中枢开局域网加密入口、支持设备正式配对（协议 2）；把能力的真实参数和产品口吻交给模型，回答流式逐段发给设备；默认用 DeepSeek

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）。

## 为什么

`hub-link-1` 已合入：设备有了带公钥校验的 TLS WebSocket，中枢有了 OpenAI 兼容和 Anthropic 两种模型接法。它的报告（`docs/tasks/hub-link-1/REPORT.md`“真机联调步骤”第 2 条）写明真机还连不上，阻断有三处：

- 中枢只开本机 HTTP：`hub/ink_hub/__main__.py:22` 不许监听 127.0.0.1 以外的地址；
- 网关拒绝远端地址：`hub/ink_hub/gateway/server.py:461`；
- 只认开发协议 1：`server.py:360-361`；设备配对用的是协议 2（`system/services/hub/pairing.c:262-290`，`hello` 带令牌，没有令牌时发 `pair/begin`）。

主会话用 DeepSeek 实测了一轮，又发现两件事：

- 模型选对了“调亮度”，但参数是空的。原因是网关只把能力名交给模型（`hub/ink_hub/voice/flow.py:69` 传的是 `list(available)`），模型适配给每个能力的都是同一个空对象参数（`hub/ink_hub/models/remote.py:58-61`）；设备 `tools/list` 本来就给了每个能力的说明和参数（`system/policy/cap.c:1115-1140`：`title`、`inputSchema`）。
- 模型自称“Z-Alpha”。系统提示只有一句语言要求（`remote.py:62`）。

另外，网关是等模型整段答完才发 `ai.delta`（`flow.py:69-92`，`remote.py:127-133` 先汇总），设备要等很久才看到第一个字。

用户的决定：中枢主力模型用 DeepSeek（D-130，OpenAI 兼容接口 `https://api.deepseek.com`，模型 `deepseek-chat`；密钥用户自己放在 `ink-hw/private/hub.env`，不进仓库、不进日志）；“中枢有必要启动配合开发的也可以搞”（2026-11-12）。

## 依据（照它写，不自己设计协议）

- 协议：`docs/hub/device-protocol.md` §3.1 `hello` 第 2 版（120-147 行）、§3.2 配对（149-180 行）、§3.3 成员（182-200 行）、§4 错误码（455-475 行）、§2 第 2、10 条（单条 `ai.delta` 不超过 511 字节、未认证连接只收 `hello` 和 `pair/*`）。
- 设备端实现（以它为准，字段名照它）：`system/services/hub/pairing.c`（`hello` 参数 262-276 行：`proto`、`device`、`token`、`member`、`authed`、`epoch`；`pair/begin` 277-290 行；配对文件 `/content/ink-pair.json` 的读法 852-887 行、字段校验 392-440 行）。
- 现成的协议替身：`simulator/environment/sim_hub.c:345-440`（模拟器里的配对对端，`pair/begin`、`pair/accept`、`pair/complete`、`pair/expired`、预授权码、错误码怎么回）。中枢这边照它的行为写真的实现。
- 设备的 `pairing.configure` 能力要 `url`（必须 `wss://`）、`console`、`pin`（64 位十六进制的公钥 SHA-256，`pairing.c:399-401`）。

## 要做的

按里程碑做，每个做完就提交；前两个是今晚真机联调要用的。

1. **局域网加密入口**
   - `serve` 加一个开关（名字你定，例如 `--lan`）：另开一个只走 TLS 的监听，地址 `0.0.0.0`，端口默认 8444；原来的本机 HTTP（127.0.0.1:8443，开发协议 1、`/setup`、测试和模拟器桥都用它）照旧不动。
   - 第一次启动在数据目录里生成自签证书和私钥（`<data>/tls/`，私钥权限 600），以后一直用同一份，公钥不能每次启动都变。用系统自带的 `openssl` 命令生成（macOS 有 LibreSSL），**不新增 Python 依赖**；没有 `openssl` 就明确报错退出。
   - 启动时打印三样给配对用：设备地址 `wss://<本机局域网地址>:<端口>/v1/device`、公钥 SHA-256（SubjectPublicKeyInfo 的 DER 的 SHA-256，64 位十六进制，和 hub-link-1 报告第 3 步的 `openssl` 算法一致）、核对码（协议 §3.2：公钥 SHA-256 的前 60 位，base32，12 个字符）。再加一个子命令只打印这三样（例如 `pin`）。
   - TLS 监听上只开 `/v1/device`；`/setup`、认领页只在本机 HTTP 上。远端地址只能走协议 2；开发协议 1 只许本机地址。
2. **协议 2 和配对**
   - `hello` 第 2 版：校验设备编号和令牌；回答照协议 §3.1（`session`、`proto:2`、`now_ms`、`hub_instance`，`pins`、`urls` 可带）。令牌不对回 `-32010`，待生效令牌过期回 `-32013`；协议版本不支持回 `-32012`。
   - `pair/begin`：发 8 位认领码、`expires_s:600`；同一个 `nonce` 重连拿回同一个码；带 `txid` 时重发 `pair/complete`（生成新的待生效令牌，旧的作废）；带 `preauth` 时核对一次性预授权码，对了直接走到 `pair/complete`，用过或不对回 `-32030`。认领码过期发 `pair/expired`。`pair/begin` 之后不受“10 秒内要握手”的限制，最多等 10 分钟。
   - 认领（中枢这边的人确认）：先做命令行，例如 `python -m ink_hub claim <认领码> <核对码> --name "<设备名>"`，核对码不对就拒绝，同一个码错 5 次作废；对了中枢给设备发 `pair/offer`（`txid`、`family`、`by`、`name`），设备上点确认后收 `pair/accept`，回 `{}` 再发 `pair/complete`（`txid`、`device`、`token`、`urls`、`pins`、`members_rev`、`now_ms`）。命令行只连本机 HTTP。网页认领页（设备二维码里的 `/claim/<码>`）这次不做，写进“没做到的”。
   - 配对文件：子命令（例如 `pair-file --out <路径>`）写出设备能导入的 `ink-pair.json`：`url`、`console`、`pin`、`preauth`（一次性，24 小时过期）。不写 Wi-Fi 名称和密码（要的话主会话手工加）。这个文件和预授权码不进仓库、不进日志。
   - 令牌：随机生成；中枢只存哈希（放进数据目录已有的 `hub.db`，新加一张表），“待生效”第一次 `hello` 成功才生效，待生效留 7 天（协议 §3.2“令牌丢不了”）。数据目录里查不到令牌原文。
   - 家人：协议 2 下按设备在 `hello` 和 `ink/member` 里给的 `member`、`authed`、`epoch` 建作用域；中枢不自己提升权限（没解锁的一律按孩子的限制过滤能力，`flow.py:50-51` 现有的过滤照用）；设备的策略闸门仍然是最终判断。
   - 限流照协议写最小的：未完成的配对最多 32 个；认领码错误次数如上。
3. **把能力的真实说明和参数交给模型**
   - `run_turn` 把可用能力的完整描述（设备 `tools/list` 给的 `title`、`inputSchema`）交给模型，不再只给名字；`remote.py` 用它们生成函数说明和参数。函数名照现在的做法映射成模型接受的字符，调用回来再映射回能力名。
   - 模型给的参数照旧先用 `inputSchema` 校验（`flow.py:77`），不合格就不调设备，告诉人一句短话；仍然每轮最多一个工具，不加自动重试或多轮代理。
   - DeepSeek 一次最多 128 个函数：超了按页面上的能力优先，截断的个数写日志（不写能力参数）。
4. **产品口吻的系统提示**
   - 一处常量（中文），内容至少：你是这家人用的墨水屏设备上的助手；不要给自己起名字、不要说自己是哪个模型或代号，被问“你是谁”就说是这台设备上的助手；回答简短（屏幕小、还要朗读），默认三句以内；要操作设备就调一个能力，参数照说明填全，拿不准就先问一句，不要编参数；设备回执回来之前不要说“已经做好了”；不知道的事不要编；孩子在用时用孩子听得懂的话；按设备给的 `lang` 回答。
   - 当前家人：设备在 `ai.ask`、`ai.listen` 参数里加 `"member":{"name":"<称呼>","role":"adult|child"}`（`system/services/ai/ai_service.c` 组参数的地方，579-608 行附近，不超过 20 行）。**默认只把“大人/孩子”放进给模型的提示，称呼不发给外部模型**（家人的名字算家人数据，用户还没同意发给 DeepSeek）；中枢加一个开关（环境变量，默认关），打开后才在提示里用称呼。不论开关，**绝不从成员编号编名字**。协议文档要加的这一个字段写进报告“要改的文档”。
5. **流式**
   - `run_turn` 改用模型的流式输出（`RemoteModel.stream()` 已有；假模型补一个按段吐字的 `stream()`）：文字一到就发 `ai.delta`，单条按整字切到 511 字节以内，整轮总长照旧用 `wire.answer` 截断并附同一句提示。
   - 语音轮：整句一到就合成这一句（照用现在的分句 `sentences()`），60 ms 节奏、最多预发 5 帧的规矩不变（`flow.py:94-111`）。
   - 中途调了工具：已经发出的文字保留，设备回执的 `say` 接在后面；流中途断了：已发的保留，再补一句现有的“这次没有完成，请重试。”，不重发前面的字。
6. **默认用 DeepSeek，启动脚本读密钥**
   - `ink_hub/models/__init__.py` 的 `from_environment()`：没设 `INK_HUB_MODEL_PROVIDER`、但有 `DEEPSEEK_API_KEY` 时，用 OpenAI 兼容接法，地址 `https://api.deepseek.com`，模型 `deepseek-chat`，密钥取 `DEEPSEEK_API_KEY`；显式设 `INK_HUB_MODEL_PROVIDER=fake` 仍用假模型。
   - 启动脚本（例如 `hub/deploy/start-mac.sh`）：从 `${INK_HUB_ENV_FILE:-$HOME/Documents/Workspace/ink-hw/private/hub.env}` 里**只取** `DEEPSEEK_API_KEY=` 那一行（不 `source` 整个文件，不开 `set -x`，不回显），作为环境变量交给中枢进程，然后 `exec` 中枢 `serve`（带局域网开关和数据目录）。文件权限比 600 宽就提示一句（不打印内容）。
   - **你不许打开、读取或用真实的 `hub.env` 跑这个脚本**（总规则第 1 节：`ink-hw` 只许读 `upstream/`）。测试用临时目录里的假文件（假密钥如 `sk-test-0000`），并检查输出和日志里查不到它。
   - 语音的接口留给 `hub-real-1`：`Hub` 和 `__main__` 里用 `getattr(ink_hub.speech, "from_environment", None)` 取识别和朗读（返回 `(asr, tts)`，`None` 表示用假的）；去掉 `__main__.py:24` 对 `INK_HUB_ASR`、`INK_HUB_TTS` 的硬拒绝，交给它校验。**不改 `hub/ink_hub/speech/` 下的文件**（归 `hub-real-1`）。

## 范围和预算

- 只改：`hub/ink_hub/__main__.py`、`hub/ink_hub/gateway/`、`hub/ink_hub/voice/flow.py`、`hub/ink_hub/models/`、`hub/ink_hub/data/`（令牌表需要的那几处）、新的启动脚本、`hub/tests/`、`hub/README.md`（只改“怎么连设备”和“怎么换成真模型”两节）、`system/services/ai/ai_service.c` 里组参数的那几行、受影响的设备协议测试（`tests/` 里 ai 请求参数相关的断言）、`docs/tasks/hub-pair-1/`。
- 不碰：`hub/ink_hub/speech/`（`hub-real-1`）、`ports/esp32s31/hal_ws_s31.c`、`system/services/hub/pairing.c`、`hub_link.c`（设备配对逻辑以现有实现为准，发现设备侧问题写进报告，不改）。
- 中枢产品代码不超过 700 行，设备不超过 20 行，脚本不超过 40 行（不算测试）。新增的机制（TLS 证书文件、令牌表、两个子命令）都是本任务书要求的，照样列进报告“新增的机制”。
- 一切测试用本机假服务器和假设备，**不发任何真实模型请求**；测试进程开头清掉 `DEEPSEEK_API_KEY` 和所有 `INK_HUB_*`，防止误连外网。

## 验收

1. **局域网入口**：临时数据目录起 `--lan`；测试客户端用公钥固定的 TLS 连上 `/v1/device`；打印的公钥哈希和 `openssl x509 -pubkey | openssl pkey -pubin -outform DER | openssl dgst -sha256` 一致；重启后公钥不变；远端地址走协议 1 被拒、走 HTTP 被拒；`/setup` 在 TLS 监听上取不到。远端地址的判断写成可单测的函数，给它造假的远端地址测。
2. **照协议走一遍配对**（假设备 `hub/tests/fake_device.py`）：`pair/begin` 拿到码 → 用错的核对码认领被拒 → 用对的认领 → 收到 `pair/offer` → `pair/accept` → 收到 `pair/complete`（带令牌、地址、公钥）→ 用令牌 `hello` 成功 → 中枢重启后同一令牌还能 `hello` → 数据目录里搜不到令牌原文。另测：同一 `nonce` 拿回同一个码；码过期收到 `pair/expired`；带 `txid` 重连拿到新令牌、旧的失效；预授权码第一次直接完成、第二次 `-32030`；错令牌 `-32010`；待生效令牌过期 `-32013`（注入时钟）。反向对照：去掉核对码校验，认领测试必须失败。
3. **中枢发出的新消息都过现有的契约检查**：`pair/offer`、`pair/complete`、`hello` 回答进 `hub-contract` 的消息记录，被 `hub-json-crosscheck` 用设备的 `ink_json.c` 解析通过，词法单元不超过协议 §5 的 128 个。
4. **模型拿到真参数**：本机假 OpenAI 服务器收到的请求里，每个函数的说明和参数就是设备 `tools/list` 里那个能力的 `title` 和 `inputSchema`；假服务器回一个带参数的工具调用，假设备收到同样参数的 `tools/call`；回不合格参数时设备收不到调用。反向对照：改回只传名字，测试必须失败。这是中枢任务，没有页面：“改前改后对照”用同一句“把亮度调到 30”在改前和改后假服务器收到的请求（系统提示、亮度那个能力的函数说明）和假设备收到的调用代替截图，摘要贴进报告。
5. **系统提示**：请求里的系统提示没有任何代号或自称的名字；开关默认关时，即使设备给了称呼，发给模型的请求里也查不到这个称呼；打开开关后才出现；没给称呼时没有任何名字占位；孩子轮次里有“孩子”的要求。
6. **流式**：假服务器分三段、每段隔 300 ms 吐字，假设备在流结束前就收到第一条 `ai.delta`；拼起来的文字等于原文；一段超过 511 字节时在整字处切开；流中途断开时已发的保留、末尾补一次失败提示、最后仍有 `{"done":true}`。语音轮：第一帧朗读在整段回答结束前发出（用假的朗读）。
7. **DeepSeek 默认和脚本**：只设 `DEEPSEEK_API_KEY=sk-test-0000` 时，选中的接法是 OpenAI 兼容、地址 `https://api.deepseek.com`、模型 `deepseek-chat`（只检查配置，不发请求）；脚本对临时假文件能起中枢（或带一个只打印所选接法、不打印密钥的检查模式），输出和日志里查不到假密钥；`INK_HUB_MODEL_PROVIDER=fake` 时用假模型。
8. 主工程普通全量 ctest 过（含 `hub-*`）；中枢 pytest 过；S31 宿主机测试过；`ports/esp32s31/build.sh` 编译通过、无新警告，报告写固件大小变化（应接近 0）。
9. 报告写给主会话的真机联调步骤：怎么用启动脚本起中枢、去哪看公钥和核对码、怎么做 `ink-pair.json`（或在设备上用 `pairing.configure` 填地址和公钥后走认领命令）、设备和中枢各看哪几行日志算连上（`hub.online=true` 加一轮带工具调用的问答）、怎么退回。另写“要用户定”：要不要打开“称呼发给模型”的开关（默认关，只发“大人/孩子”）。
