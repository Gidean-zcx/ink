# hub-pair-1 执行报告

基线：`36473cc4933e6fea2f2a78a4fbec16186d3be3f4`。仅本地分支工作，不连接设备、不读取真实密钥、不请求真实模型、不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】TLS、固定公钥、权限、路由隔离及远端协议判定通过 | `test_pairing.py`：4 passed；公钥独立经 openssl 算法核对 | `PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" -m pytest -q hub/tests/contract/test_pairing.py` |
| 2 | 【实测】认领、nonce 恢复、过期、32 路上限、五次核对失败、预授权、txid 换令牌、7 天待生效和重启后认证通过 | `test_pairing.py`：8 passed；令牌原文未落入数据目录；去掉核对码校验后测试失败（exit 1） | 同第 1 条 |
| 3 | 待验证：设备消息契约 | — | — |
| 4 | 【实测】假设备描述与参数原样传给模型，合法参数送达设备，缺参／越界不调用；真实设置能力另有范围外阻断，见顺手发现 | 同句前后对照已运行；恢复只传名字后 3 项失败（exit 1） | `PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" hub/tests/pair_compare.py 36473cc4933e6fea2f2a78a4fbec16186d3be3f4`；反向 `hub/tests/pair_reverse.py schema` |
| 5 | 【实测】统一产品提示、称呼默认不外发、开关开启才出现、无称呼不编名字、孩子口吻通过 | `test_hub_pair_model.py` | 下方专项命令 |
| 6 | 【实测】三段间隔 300 ms；首段文字／语音早于流结束；UTF-8 分段；断流只补一次失败提示并正常 done | `test_hub_pair_model.py`；原有长答和朗读节奏待全量结果 | 下方专项命令 |
| 7 | 【实测】默认配置、强制假模型、临时假文件检查模式、权限提醒和日志不含假密钥通过 | 没有真实服务请求；未读取真实环境文件 | 下方专项命令 |
| 8 | 待验证：全量、内存、S31 与固件构建 | — | — |
| 9 | 待完成：真机联调步骤 | — | — |

## 没做到的

- 网页认领页按任务书本次不做。

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

- `docs/hub/device-protocol.md`：补充 `ai.ask`、`ai.listen` 的 `member: {name, role}` 参数及称呼默认不发给外部模型的约定。

## 新增的机制

- TLS 证书文件与 `--lan`、`--lan-port`：任务书里程碑 1 要求。
- `pin` 子命令：任务书里程碑 1 要求。

- 配对令牌表 `hub.db/pairing_tokens`：任务书里程碑 2 要求；令牌及预授权码只存 SHA-256。
- `claim`、`pair-file` 子命令、本机认领接口和配对过期计时：任务书里程碑 2 要求。

- `INK_HUB_SHARE_MEMBER_NAME`：任务书里程碑 4 要求，默认关。
- `INK_HUB_ENV_FILE`、`INK_HUB_PYTHON`、脚本 `--check`：任务书里程碑 6 要求安全读取和检查模式；沿用 `INK_HUB_DATA`。
- 没有新增常驻任务、线程、后台重试、代理循环或 Python 依赖。

## 专项验证与反向对照

```sh
taskpolicy -c utility env PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" -m pytest -q hub/tests/contract/test_pairing.py hub/tests/contract/test_hub_pair_model.py
taskpolicy -c utility env PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" hub/tests/pair_reverse.py claim
taskpolicy -c utility env PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" hub/tests/pair_reverse.py schema
```

【实测】专项 22 passed；单独扩展 TLS 正式配对、启动进程和 CLI 后，配对专项 10 passed。反向命令刻意返回 1：前者把错误核对码放行，被 403 断言拦住；后者恢复只传名字，3 个参数用例都失败。仅在内存中改被测函数，不修改工作区文件，不注册为依赖 Git 历史的 CTest。

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

任务同时要求独立 TLS 监听、稳定证书、完整配对及令牌恢复、真实工具 Schema、文字／语音流式和启动脚本，不能只放开一个地址或改协议版本号。复用 aiohttp、系统 openssl、现有 hub.db、ModelResult、发送队列和语音节奏；未选反向代理、新厂商 SDK、新表格式之外的持久文件或多轮代理。产品代码统计待最终核对。
