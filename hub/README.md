# 家庭服务器：第 1 步

这是本机的示例演示：设备网关、能力总线、合成身份和权限、默认假模型与语音、持久任务队列、安装骨架，以及默认关闭的模拟器桥。可选择接入真实对话模型（见下文），账号、配对和音频仍是示例，不能导入真实家庭资料。

## 从零运行

Python 固定为 **3.11.15**，uv 固定为 **0.9.5**。`uv.lock` 固定全部运行和测试依赖及下载哈希；镜像使用带哈希的 `requirements.lock`，不在启动时解析新版本。Python/uv 放在自己的开发环境中；下面从仓库根目录运行：

```sh
python3.11 -m venv work/hub-tools
work/hub-tools/bin/python -m pip install uv==0.9.5
UV_CACHE_DIR=$PWD/work/uv-cache work/hub-tools/bin/uv sync --project hub --frozen --python python3.11
hub/.venv/bin/python -m ink_hub doctor --data work/hub-data
hub/.venv/bin/python -m ink_hub serve --data work/hub-data --dev-allow-proto1
```

打开终端打印的本机 `/setup`，输入本次设置码。向导只保存示例进度；设置码成功使用后更换。CLI 等价命令：`hub/.venv/bin/python -m ink_hub setup --data work/hub-data --config example.yaml`，配置仅含 `step: 1` 和 `mode: example`。`status --data work/hub-data` 可读进度。凭据不进配置或日志，真实密钥仅从环境变量读；示例 `.env.example` 没有密钥。

服务固定监听 `127.0.0.1:8443`。远程开发机通过 `ssh -L 8443:127.0.0.1:8443 user@example.com`，再访问本机地址。第 1 步不开放公网设置，不信任转发来的 HTTPS 头，不自动申请证书，也不把自签证书当成可信网页入口。开发握手默认拒绝，必须显式打开；它只映射到合成家庭的儿童身份，不能验证成人密码。

## 测试

先按 [任务总规则](../docs/tasks/codex-rules.md#5-构建和测试) 配好 C 构建依赖（`INK_DEPS` 是只读依赖目录），再运行：

```sh
cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" \
  -DINK_HUB_PYTHON="$PWD/hub/.venv/bin/python"
cmake --build build -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^hub-' --output-on-failure
cd hub
INK_HUB_CAPS=../build/test-out/hub/caps.json .venv/bin/python -m pytest -q
```

`hub-export-caps` 启动当前产品的原始初始化代码，调用生产能力清单函数导出能力表；不手抄能力名。`hub-contract` 的假设备逐条检查中枢出站 JSON，生成 `build/test-out/hub/messages.jsonl`；`hub-json-crosscheck` 用链接原始 `ink_json.c` 的 C 程序解析这些消息。两种词法计数独立实现。

`hub-simulator-e2e` 在同一个 Python 主进程启动中枢，等待监听就绪后启动原始产品模拟器，检查一轮语音和一次设备工具调用，并写内存、事件循环数据到 `build/test-out/hub-e2e/measurement.json`。缺少 Python 依赖、字体或测试内容时 CTest 明确显示 `Skipped`（退出码 77）。运行五次：

```sh
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^hub-simulator-e2e$' --repeat until-fail:5 --output-on-failure
```

所有流程只用假厂商、替身 Opus 包，不代表真实语音可用。模拟器桥只接受 `--hub-url ws://127.0.0.1:端口/v1/device`，强制 1× 实时；省略参数就用原来的确定性模拟中枢。模拟任务只排队；专用宿主网络线程经 `vtk_inject_external` 回到模拟世界，关闭或代次过期的回调丢弃。继续交互可使用原来的 `ink-sim` / `ink-sim-gui` 加同一个参数。

## 怎么连设备

本机 HTTP 保持 `127.0.0.1:8443`，设置页和认领命令只走这个入口；模拟器桥仍加 `--dev-allow-proto1`。真实设备加 `--lan`，另开 `0.0.0.0:8444` 的 TLS 入口，那里只提供 `/v1/device`，只收正式协议 2：

```sh
PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" -m ink_hub serve --lan --data work/hub-data
PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" -m ink_hub pin --data work/hub-data
```

启动和 `pin` 都打印设备地址、公钥 SHA-256、12 位核对码。首次用系统 openssl 在 `work/hub-data/tls/` 生成证书和私钥，以后复用；私钥权限 600，备份数据目录时保留它。端口可用 `--lan-port` 改；自动选出的地址需要主会话核对是否能从设备所在局域网访问。这里的 pin 是 SPKI DER 的 SHA-256，不是证书文件哈希。

推荐通过配对文件带外交付公钥和一次性预授权码：

```sh
PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" -m ink_hub pair-file --data work/hub-data --out work/ink-pair.json
```

由主会话将文件放到 TF 根目录（设备上 `/content/ink-pair.json`），调用 `pairing.import`。文件权限 600，一次有效，24 小时过期，不含 Wi-Fi；用后删除，不提交或打印其内容。`console` 是设备配置要求的 HTTPS 地址占位，本次没有网页认领页；预授权配对不访问它。

也可在设备上调用 `pairing.configure`，填写打印的 `url`、`pin` 和同主机的 HTTPS `console`，设备显示 8 位认领码后，在中枢本机运行：

```sh
PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" -m ink_hub claim <认领码> <核对码> --name "我的设备"
```

核对码必须从设备上核对，不能只照抄服务器终端。认领后在设备确认家庭和设备名。设备收到令牌后重新 `hello`，`hub.online=true` 才算完成；TLS connected 只说明加密连接已建立。令牌和一次性码在 `hub.db` 只存哈希。网页 `/claim/<码>` 本次没有实现。停止中枢即可退回；去掉 `--lan` 可只保留开发入口，保留数据目录可继续使用原公钥和令牌。

## 怎么换成真模型

不显式设置接法、但环境里有 `DEEPSEEK_API_KEY` 时，默认使用 OpenAI 兼容接口 `https://api.deepseek.com` 和 `deepseek-chat`（D-130）。没有密钥时仍用假模型；`INK_HUB_MODEL_PROVIDER=fake` 始终强制假模型。显式 `openai`、`anthropic` 沿用原接法，缺少对应密钥时用假模型。

| 接法 | 必填环境变量 | 可选环境变量 |
| --- | --- | --- |
| DeepSeek 默认 | `DEEPSEEK_API_KEY` | 不需要设置 provider |
| OpenAI 兼容 | `INK_HUB_MODEL_PROVIDER=openai`、`INK_HUB_OPENAI_API_KEY`、`INK_HUB_OPENAI_MODEL` | `INK_HUB_OPENAI_BASE_URL`，默认 `https://api.openai.com/v1` |
| Anthropic | `INK_HUB_MODEL_PROVIDER=anthropic`、`INK_HUB_ANTHROPIC_API_KEY`、`INK_HUB_ANTHROPIC_MODEL` | `INK_HUB_ANTHROPIC_BASE_URL`，默认 `https://api.anthropic.com/v1` |

Mac 启动脚本由主会话使用；只读环境文件的 `DEEPSEEK_API_KEY=` 行，不执行文件里的其他内容，不回显密钥，权限比 600 宽会提醒：

```sh
INK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python" INK_HUB_DATA=work/hub-data bash hub/deploy/start-mac.sh
```

环境文件默认在 `$HOME/Documents/Workspace/ink-hw/private/hub.env`，可用 `INK_HUB_ENV_FILE` 指定。`--check` 只显示接法、地址和模型，不请求服务、不显示密钥。开发测试只用临时假文件；执行本任务时没有读取真实环境文件或调用真实模型。

网关把设备 `tools/list` 的 `title`、`inputSchema` 原样交给模型；超过 128 个时页面能力优先。文字逐段按整字切到 511 字节以内，语音整句到达即合成，工具回执接在已发文字后。每轮仍只调用一个工具，仍经 Schema 校验和授权，不加自动重试。统一中文提示限定产品身份、简短回答和真实回执。默认只发送“大人／孩子”；`INK_HUB_SHARE_MEMBER_NAME=1` 才允许把设备给的称呼写进外部模型提示，不从成员编号编名字。

语音接法由 `ink_hub.speech.from_environment()`（若存在）选择，返回 `(asr, tts)`；未提供时使用假语音。本任务没有修改 speech 模块。

```sh
PYTHONPATH=hub "$INK_DEPS/hub/.venv/bin/python" -m pytest -q hub/tests/contract/test_models.py hub/tests/contract/test_hub_pair_model.py hub/tests/contract/test_pairing.py
```

## 安装骨架

```sh
sh hub/deploy/install.sh --dry-run
sh hub/deploy/install.sh --prefix work/inkhub-example --apply
```

第一条没有安装副作用。第二条要求 Docker Compose，会预检系统、端口和资源，再构建本地镜像并初始化专用数据目录权限，最后启动；不覆盖已有安装。资源读不到时明确报未知。`--prepare-only` 只准备安装包和检查 Compose，供 CI 使用。仅在管理员显式加 `--install-docker` 时才尝试 Debian/Ubuntu 的系统安装。Compose 使用 Linux 的 host 网络，仅暴露服务自己监听的本机地址；Mac 上先用上面的 Python 模式。容器默认拒绝开发握手，明确设置 `INK_HUB_DEV_ALLOW_PROTO1=1` 才打开。本次开发机没有 Docker，实际容器运行结果见 [报告](../docs/tasks/hub-step1/REPORT.md)。

`worker` 是无网络、非 root、只读根文件系统、限内存的独立容器，只能读取本任务输入和写输出，拿不到家庭数据库和环境密钥。本步只实现有大小上限的合成文件哈希探针，书籍转换在第 5 步。任务队列使用 SQLite，主进程调度器会恢复、执行合成哈希任务；安全任务租约过期后可恢复，外部请求结果不明标 `unknown`，不自动重试；幂等键按 Scope 隔离并核对参数摘要。任务执行中权限变化则禁止发布结果。参数 Schema 本步支持常见对象、类型、必填、枚举和大小限制，其他关键字明确拒绝。

目录按 [设计 §9](../docs/hub/hub-architecture.md#9-代码结构) 保留。后续阶段的空包只标边界，不列为可用能力。第一步两端约定见 [contracts-step1.md](../docs/hub/contracts-step1.md)，测试及限制见 [执行报告](../docs/tasks/hub-step1/REPORT.md)。依赖来自 [PyPI](https://pypi.org/)，版本、源码和所有分发哈希在 `uv.lock`，许可证副本在 `licenses/`；没有引入 GPL 依赖。使用的服务接口依据 [aiohttp 3.12.15](https://docs.aiohttp.org/en/v3.12.15/web_reference.html)，锁文件用法依据 [uv 官方文档](https://docs.astral.sh/uv/concepts/projects/sync/)（查阅：2026-11-05）。
