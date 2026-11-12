# hub-pair-1 执行报告

基线：`36473cc4933e6fea2f2a78a4fbec16186d3be3f4`。仅本地分支工作，不连接设备、不读取真实密钥、不请求真实模型、不推送。日期：2026-11-12。实现和本机验证已完成，普通轮的原始失败及补验结果如下保留。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】TLS、固定公钥、权限、路由隔离及远端协议判定通过 | 配对 11 项已纳入最终 hub-contract；公钥经 openssl 独立核对，核对码经标准 base32 独立核对 | `PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" -m pytest -q hub/tests/contract/test_pairing.py` |
| 2 | 【实测】认领、nonce 恢复、过期、32 路上限、五次核对失败、预授权、txid 换令牌、7 天待生效和重启后认证通过 | 完整配对、CLI、TLS 和超过 10 秒等待均通过；令牌原文未落入数据目录；去掉核对码校验后测试失败（exit 1） | 同第 1 条 |
| 3 | 【实测】通过；654 条消息最多 110 词法单元 | 记录含 pair/offer 2 条、pair/complete 5 条、协议 2 hello 回答 3 条；生产 ink_json.c 解析通过 | `ctest --test-dir build -R hub-json-crosscheck --output-on-failure`（由中枢复跑产生样本） |
| 4 | 【实测】假设备描述与参数原样传给模型，合法参数送达设备，缺参／越界不调用；真实设置能力另有范围外阻断，见顺手发现 | 同句前后对照已运行；恢复只传名字后 3 项失败（exit 1） | `PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" hub/tests/pair_compare.py 36473cc4933e6fea2f2a78a4fbec16186d3be3f4`；反向 `hub/tests/pair_reverse.py schema` |
| 5 | 【实测】统一产品提示、称呼默认不外发、开关开启才出现、无称呼不编名字、孩子口吻通过 | `test_hub_pair_model.py` | 下方专项命令 |
| 6 | 【实测】三段间隔 300 ms；首段文字／语音早于流结束；UTF-8 分段；断流只补一次失败提示并正常 done | `test_hub_pair_model.py`；原有长答和朗读节奏待全量结果 | 下方专项命令 |
| 7 | 【实测】默认配置、强制假模型、临时假文件检查模式、权限提醒和日志不含假密钥通过 | 没有真实服务请求；未读取真实环境文件 | 下方专项命令 |
| 8 | 【实测】普通全量已跑并补验；ASan/UBSan 437 项全部通过；pytest 190 项；S31 普通／内存各 77 项；固件通过、增加 48 字节；GCC 无发现 | 普通原始失败和端到端复跑波动见“构建与内存检查”；未伪报单轮普通全绿 | 该节完整命令及结果 |
| 9 | 【实现】启动、配对文件／人工认领、公钥核对、成功日志、退回步骤齐全 | 本报告“真机联调步骤”；称呼外发仍默认关 | 见该节命令；本轮未上机 |

## 没做到的

- 网页认领页按任务书本次不做。
- 没有真机、真实模型、真实语音或 Linux CI 验收；只执行本机编译和假服务测试。
- 协议里的同 IP 每分钟 10 次限流未加；本次按任务明确的最小范围实现 32 个未完成配对和核对码五次作废。

## 和设计不一致的

- 设备配置要求 `console` 为 HTTPS，但本次不提供 HTTPS 网页；配对文件填同主机 HTTPS 地址仅满足设备配置，预授权不访问该地址，人工认领使用本机 HTTP 命令。网页入口按任务书延期。

## 改过的已有测试

- `test_protocol.py`：协议 2 缺令牌由“不支持协议”改为“令牌不对”（`-32010`）；现已支持协议 2。

- `test_models.py`：两种接法的函数说明从“能力名后缀”改为严格等于设备 `title`。
- `test_protocol.py`：语音工具轮从 5 帧改为 10 帧，因为模型前文和设备回执分别朗读、都保留。
- `test_delivery.py`：排队工具失效测试将合成前文设为空，继续让工具请求本身排队；新增流式前文会先阻塞发送。断言未放宽。
- `tests/i18n_lang.c`：增加 AI 请求成员称呼和角色断言，原有语言断言保留。

## 要用户定的

- 是否允许把称呼发给外部模型；默认关闭，仅发送大人／孩子。

## 顺手发现

- 【实测】固件导出的 `settings.set` 将 `value.type` 定义为数组（integer／boolean／string），既有 `hub/ink_hub/bus/schema.py:20` 用作字典键会抛 `TypeError: unhashable type: list`。对真实 `{id:"display.light",value:30}` 已复现；任务范围不含 bus，未修改。因此本次参数透传验证使用明确的合成亮度能力，不声称真机调亮度已完成。

## 要改的文档

- `hub/README.md` 开头和“从零运行”仍有“配对为示例／只监听本机”的旧描述，任务只允许改连接和模型两节，留给主会话同步。

- `docs/hub/device-protocol.md`：补充 `ai.ask`、`ai.listen` 的 `member: {name, role}` 参数及称呼默认不发给外部模型的约定。

## 新增的机制

- `<data>/tls/cert.pem`、`key.pem`：里程碑 1 要求稳定 TLS 身份，私钥 600。
- `--lan`：里程碑 1 要求另开 TLS 监听。
- `--lan-port`：里程碑 1 要求 TLS 端口可指定，默认 8444。
- `pin`：里程碑 1 要求只显示设备地址、公钥和核对码。
- `hub.db/pairing_tokens`：里程碑 2 要求，存令牌／预授权哈希、待生效与有效状态。
- 内存配对记录及 600 秒过期回调：里程碑 2 的认领、重连与 32 路限制要求；不是常驻任务。
- `claim` 和本机 POST `/claim`：里程碑 2 要求人工核对。
- `pair-file`：里程碑 2 要求导出现有设备格式的一次性配对文件。
- `INK_HUB_SHARE_MEMBER_NAME`：里程碑 4 要求，默认关。
- `DEEPSEEK_API_KEY`：里程碑 6 要求默认接法的凭据，只从环境读取。
- `INK_HUB_ENV_FILE`：里程碑 6 要求指定启动密钥文件。
- `INK_HUB_PYTHON`：启动脚本和假文件检查选择已有 Python 环境。
- 脚本 `--check`：里程碑 6 允许的无请求检查模式；数据目录沿用 `INK_HUB_DATA`。
- 没有新增常驻任务、线程、后台重试、代理循环或 Python 依赖。

## 专项验证与反向对照

```sh
taskpolicy -c utility env PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" -m pytest -q hub/tests/contract/test_pairing.py hub/tests/contract/test_hub_pair_model.py
taskpolicy -c utility env PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" hub/tests/pair_reverse.py claim
taskpolicy -c utility env PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" hub/tests/pair_reverse.py schema
```

【实测】专项 22 passed；随后增加 TLS 正式配对、启动进程、CLI、10 秒等待及语音工厂接口验证，独立完整 pytest 190 passed。反向命令刻意返回 1：前者把错误核对码放行，被 403 断言拦住；后者恢复只传名字，并保留旧适配器的空对象参数行为，3 个用例均在收到真实 HTTP 请求后因函数描述不匹配而失败；不是靠参数类型崩溃获得反向结果。仅在内存中改被测函数，不修改工作区文件，不注册为依赖 Git 历史的 CTest。

## 同句前后对照

【实测】手动 `pair_compare.py` 分别加载指定基线的网关轮次／模型适配和当前实现，请求同一个本机假模型，假设备提供同一个合成亮度 Schema。问题都是“把亮度调到 30”。

| 项目 | 改前 | 改后 |
| --- | --- | --- |
| 系统提示 | 只有“请用 zh 回答；需要操作设备时调用提供的能力，每轮最多调用一次” | 完整产品提示，包含设备身份、三句以内、不编参数、等设备回执、孩子口吻和 lang |
| 函数说明 | `设备能力：display.set_brightness` | `设置亮度（0 到 100）` |
| 函数参数 | 空 properties，允许任意参数 | value 为 integer、0～100、必填、禁止额外参数 |
| 假设备 tools/call | 没有（模型看不到字段，回空参数，被校验拦住） | `display.set_brightness {"value":30}` |

假模型按收到的 Schema 确定返回参数，用于证明参数透传，不代表 DeepSeek 的理解效果。原始输出留在忽略目录 `work/hub-pair-1/model-comparison.jsonl`。

## 为什么不能更小

任务同时要求独立 TLS 监听、稳定证书、完整配对及令牌恢复、真实工具 Schema、文字／语音流式和启动脚本，不能只放开一个地址或改协议版本号。复用 aiohttp、系统 openssl、现有 hub.db、ModelResult、发送队列和语音节奏；未选反向代理、新厂商 SDK、新表格式之外的持久文件或多轮代理。产品新增行数按基线 diff 统计（含空行／注释，不含测试／文档）：中枢 471／700，设备 6／20，脚本 28／40。

## 真机联调步骤（交主会话）

1. 用户自行准备真实环境文件并设权限 600。仓库根目录运行：

   ```sh
   export INK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"
   export INK_HUB_DATA="$PWD/work/hub-data"
   bash hub/deploy/start-mac.sh
   ```

   脚本默认读 `$HOME/Documents/Workspace/ink-hw/private/hub.env`，也可设 `INK_HUB_ENV_FILE`。只读取 DeepSeek 那一行。先核对打印地址属于设备可达的网卡，设备和 Mac 同网段；本机设置仍在 `127.0.0.1:8443`，设备 TLS 默认 8444。

2. 另一个终端用同一数据目录查看公钥和核对码：

   ```sh
   PYTHONPATH=hub "$INK_HUB_PYTHON" -m ink_hub pin --data "$INK_HUB_DATA"
   openssl x509 -in "$INK_HUB_DATA/tls/cert.pem" -pubkey -noout | openssl pkey -pubin -outform DER | openssl dgst -sha256
   ```

   两个公钥结果应一致。核对码来自公钥前 60 位，不是证书哈希。不要每次删除数据目录或替换 TLS 文件。

3. 推荐预授权文件路径：

   ```sh
   PYTHONPATH=hub "$INK_HUB_PYTHON" -m ink_hub pair-file --data "$INK_HUB_DATA" --out work/ink-pair.json
   ```

   主会话将文件放入 TF 根目录，在设备调用 `pairing.import`；也可以重启由现有自动导入读取。文件只含 `url/console/pin/preauth`，不要打印，24 小时内只用一次，用后删除。不会由本任务替用户添加 Wi-Fi 凭据。设备要求 `console` 为 HTTPS，但本次没有网页认领服务，不能把它当成可打开的设置页。

4. 或者通过设备 `pairing.configure` 设置可信的 `wss://<Mac地址>:8444/v1/device`、同主机的 HTTPS `console` 和打印的 64 位 pin。设备显示认领码后，在 Mac 本机运行：

   ```sh
   PYTHONPATH=hub "$INK_HUB_PYTHON" -m ink_hub claim <设备认领码> <设备核对码> --name "我的设备"
   ```

   在设备端确认家庭和设备名。必须核对设备显示的核对码；仅知道服务器终端的码不足以排除中间人。

5. 设备端 `ink_ws: TLS and WebSocket connected` 只证明加密连接；还要看到状态 `hub.online=true`。中枢终端出现 `设备已连接：协议 2` 才证明 hello 已认证。先用一个无参数、当前成员允许的能力（例如翻页）做一轮文字问答，设备状态或页面确实变化，中枢有 `设备操作回执：完成`，并可在本机 `/metrics` 核对 `tool_calls` 增加。测试中这些消息均由假设备产生；没有真机、真实 DeepSeek 或真实语音的验收结论。真实调亮度前需主会话修复上文类型数组校验问题。

6. 退回：Ctrl-C 停中枢。去掉 `--lan`、显式 `INK_HUB_MODEL_PROVIDER=fake` 并加 `--dev-allow-proto1` 可恢复原本机开发入口；保留原数据目录中的证书和 hub.db，之后原设备令牌仍可使用。不自动清设备身份、不动设备固件。

## 架构审视

【实现】设备参数仅在现有 AI 请求构造处增加成员说明；按钮和 AI 仍使用同一设备能力及策略闸门。TLS／配对归网关、哈希凭据归数据层、外部模型协议归 models、流式调度归现有 voice 轮次。称呼和令牌不开放给模型作身份授权；本次没有新页面，G-002 页面快照继续沿用原接口。

【实测】核对码输成中文时，字符串 compare_digest 原先抛 TypeError（专项失败、exit 1）；改用 UTF-8 字节比较后，同一测试通过，仍累计五次作废。中枢只记录“协议 2 已连接”和固定的操作回执状态，不记录令牌、称呼、问题或工具参数。

## 构建与内存检查

- 【实测】主工程普通首次全量原话：`99% tests passed, 2 tests failed out of 437`，1569.83 秒。失败为旧排队测试前文阻塞导致的 `hub-contract`，以及依赖它未运行的 `hub-json-crosscheck`；修正测试场景后这两项复跑通过。
- 【实测】中枢专项复跑时，既有模拟器端到端测试出现 `turns=0`；清理本次生成的 `build/test-out/hub-e2e` 后单项复跑：`100% tests passed out of 1`，18.66 秒。首次全量中该项通过。这里记录复跑稳定性问题，不把清理后一次通过当作已证明根因。
- 【实测】独立中枢 pytest：`190 passed in 81.31s`；含固件能力导出，无跳过。
- 【实测】`ink-codex/tools/gcccheck.py build system/services/ai/ai_service.c tests/i18n_lang.c`：`gcccheck: 2 compiled, 0 with findings`，exit 0。
- 【实测】S31 宿主机普通：`100% tests passed out of 77`，121.84 秒；ASan/UBSan：同样 77 项全部通过，131.05 秒。包含 WS、驱动、编解码、任务配置和原有反向对照，未报告 sanitizer 错误。
- 【实测】S31 固件：同一检出、依赖、构建目录，临时仅还原设备 AI 文件到基线编译，再恢复实现重编；镜像 `4,268,048 → 4,268,096` 字节，增加 **48 字节**。内部 DIRAM 静态占用均为 `165,350` 字节。基线／最终编译器警告均 0；各 6 条 SDK CMake 组件告警，正文完全相同。两次 `ports/esp32s31/build.sh` 均 exit 0，未连接设备。
- 【实测】主工程 ASan/UBSan 全量原话：`100% tests passed out of 437`，2059.22 秒，exit 0；无跳过，未报告 sanitizer 错误。该轮 hub-contract、hub-json-crosscheck、hub-simulator-e2e 均通过，包含最终实现和完整 190 项中枢 pytest。

实际命令均带 `taskpolicy -c utility`；构建和 ctest 并发均为 2，串行执行不同构建与全量测试。主工程配置照总规则 §5，额外指定 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`。

```sh
taskpolicy -c utility cmake --build build -j2
taskpolicy -c utility env INK_KEEP_EVIDENCE=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility env INK_KEEP_EVIDENCE=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^hub-' --output-on-failure -j2
taskpolicy -c utility env PYTHONPATH=hub INK_HUB_CAPS="$PWD/build/test-out/hub/caps.json" "$INK_DEPS/hub/.venv/bin/python" -m pytest -q hub/tests
taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-tests -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python"
taskpolicy -c utility cmake --build build-s31-tests -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-tests --output-on-failure -j2
taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-tests-asan -DINK_SANITIZE=ON -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python"
taskpolicy -c utility cmake --build build-s31-tests-asan -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-s31-tests-asan --output-on-failure -j2
taskpolicy -c utility env INK_DEPS="$INK_DEPS" INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" bash ports/esp32s31/build.sh
```

反复运行测试时先清本次生成的测试状态，保留原始失败记录；没有删除测试、放宽预算、跳过测试或延长等待来掩盖问题。本轮未运行 Linux CI；GCC 检查是本机交叉编译器检查，不能代替 Linux 运行。

主工程内存构建在相同配置下换成 `build-asan` 并加 `-DINK_SANITIZE=ON`，随后：

```sh
taskpolicy -c utility cmake --build build-asan -j2
taskpolicy -c utility env INK_KEEP_EVIDENCE=1 "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

## 最终差异与隐私检查

- 【实测】对指定基线检查差异，23 个文件均在任务允许范围；没有修改 speech、设备配对／WebSocket、需求、决策或共享能力表。
- 【实测】`git diff --check` 通过。按总规则 §6 扫描基线到最终提交的差异，无命中，grep exit 1（没有匹配）。未提交真实密钥、配对文件、设备标识、本机绝对路径、设计稿或构建产物。
- 产品新增行数（含空行和注释）：中枢 **471／700**；设备 **6／20**；启动脚本 **28／40**。未增加依赖。
- 本机日志与原始对照保留在忽略目录 `work/hub-pair-1/`；契约样本、固件和测试生成物保留在忽略的构建目录。只完成本地提交，没有 push 或 PR。

## 本地里程碑

- `d8e0d009`：稳定 TLS 入口与报告骨架。
- `887bc0d0`：协议 2、认领和哈希令牌。
- `52c2202c`：真实工具 Schema、口吻、流式、DeepSeek 脚本与设备成员字段。
- `99088be5`：CLI、连接日志与核对码边界验证。
- `61b97b9d`：固件、S31 内存检查与独立核对证据。
- 最后提交包含本报告最终全量结果和加强的参数反向对照。
