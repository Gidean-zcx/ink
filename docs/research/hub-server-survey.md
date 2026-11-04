# 家庭中枢调研：现成方案、模型和语音费用

查阅日期：2026-11-04。服务于[家庭中枢概要设计](../hub/hub-architecture.md)（R-055）。

标注：〔文档〕对方官方文档或仓库说明；〔GitHub〕当天用 GitHub 接口查到的许可证、星数、最后推送时间；〔报道〕媒体或第三方文章；〔本仓库〕本仓库的代码或既有研究；〔估算〕〔建议〕是本文的判断。

**这里列的东西本次都没有部署或运行过。** 用户 2026-11-04 说服务器价格不用查，只要规格，所以本文不含服务器价格，规格见[概要设计 §4.3](../hub/hub-architecture.md)。

## 0. 结论

1. **小智服务端不能直接当中枢，但值得借。** 它许可证宽松（MIT）、很活跃，语音厂商的接法齐全；但没有家庭成员、设备策略关口、确认卡、费用上限、备份同步和内容管线，协议也不同，README 自己写着没过安全测评。沿用 D-14：自己写中枢，语音厂商适配从它借（保留版权声明），早期还能当时延实验台。
2. **模型网关不必引入大部件。** 国内几家都提供 OpenAI 兼容接口，我们真正要的是按角色路由、按成员记账和费用上限，这些现成网关也要二次开发。先写一层很薄的路由；想用 LiteLLM 或 new-api 时，把它当成一个兼容地址接进来就行。LiteLLM 2026 年 3 月的投毒事件也说明，把握有全部密钥的大依赖放进主进程有代价。
3. **记忆先自己做。** mem0 默认依赖 OpenAI 和 Qdrant，也没有“成员、儿童、观察和推断分开”这些概念；家庭规模的记忆量很小，SQLite 加全文检索和向量暴力比较就够。接口留好，以后可以换。
4. **渲染用 Playwright + Chromium。** Lightpanda 没有图形渲染，给不了设备要的像素；Browserless 是 SSPL。一路浏览器按约 1 GB 内存算。
5. **数据库用 SQLite，一家一个文件。** 隔离、备份、搬家、删除都最简单；用不上 PostgreSQL 的并发能力，以后要换也留了路。
6. **许可证有两个坑**：EbookLib 和 PyMuPDF 都是 AGPL-3.0，内容转换改用 pdfminer.six、pypdfium2 和自己解析 EPUB；TEN 框架和 xiaozhi-server-go 是加了限制条件的 Apache 许可证，不用。
7. **费用在 D-07 的预算内。** 全家一天 50 轮对话的假设下，模型加语音约每月 50～210 元（§10），主要看主模型选谁。

## 1. 小智服务端

| 项目 | 内容 | 依据 |
| --- | --- | --- |
| [xinnan-tech/xiaozhi-esp32-server](https://github.com/xinnan-tech/xiaozhi-esp32-server) | MIT，1.1 万星，当天还有推送 | 〔GitHub〕 |
| 语言 | Python（语音服务）+ Java（管理接口）+ Vue（控制台） | 〔文档〕README |
| 部署 | 最小安装：一个 Python 容器加配置文件，不要数据库。全模块：`docker-compose_all.yml` 四个容器（语音服务、Java 控制台、数据库、Redis） | 〔文档〕[Deployment.md](https://raw.githubusercontent.com/xinnan-tech/xiaozhi-esp32-server/main/docs/Deployment.md)、[Deployment_all.md](https://raw.githubusercontent.com/xinnan-tech/xiaozhi-esp32-server/main/docs/Deployment_all.md) |
| 镜像 | 用 `ghcr.nju.edu.cn` 镜像站；0.8.2 起 Docker 镜像只支持 x86，arm64 要自己编 | 〔文档〕同上 |
| 资源 | 文档没有给 CPU 和内存数字，只说本地跑 FunASR 费内存、全走 API 占用低 | 〔文档〕README |
| 厂商 | 语音识别：阿里、火山、讯飞、腾讯、百度、FunASR、Sherpa 等。朗读：阿里、火山、讯飞、腾讯、MiniMax、CosyVoice、EdgeTTS 等。模型：任意 OpenAI 兼容接口。语音检测：Silero | 〔文档〕README |
| 记忆 | mem0ai、PowerMem、本地短期记忆、不用记忆 | 〔文档〕README |
| 成员 | 全模块有多用户管理和声纹；按设备存记忆，声纹只把说话人姓名写进提示词，不用于授权 | 〔本仓库〕[协议调研 §1.3、§1.5](foundation/ai-native-protocols.md) |
| 安全 | README 写明功能未完善、没有通过网络安全测评，不要用于生产环境 | 〔文档〕README |

和我们要的对比（〔建议〕）：

| 我们要的 | 小智服务端 |
| --- | --- |
| 一台设备多个成员，数据和记忆按成员隔离（D-03、R-030） | 按设备 |
| 策略关口和确认卡，儿童规则（D-47） | 没有这一层 |
| 能力描述、回执、请求号去重、页面版本（D-12、F-6） | 用 MCP 的子集，结果是文本 |
| 自定帧头、多路流、优先级、拉取式控流（详细设计 §6） | 小智协议 |
| 远程画面（R-048） | 没有 |
| 费用上限（D-07）、备份同步（R-023）、内容管线（D-15）、固件签名分发 | 没有 |

**可以借的**：`main/xiaozhi-server/core/providers/asr/` 和 `tts/` 下各家流式接口的接法（如 `aliyunbl_stream.py`、`doubao_stream.py`、`alibl_stream.py`、`huoshan_double_stream.py`），都是 MIT，保留版权声明即可。也可以按底座设计 §9 的原意，先把它搭起来量各家厂商的时延。

其他实现：

| 项目 | 许可证 | 判断 |
| --- | --- | --- |
| [joey-zhou/xiaozhi-esp32-server-java](https://github.com/joey-zhou/xiaozhi-esp32-server-java) | MIT，1.4 千星，2026-09 还有推送 | Java 全家桶，和 D-14 不合 |
| [AnimeAIChat/xiaozhi-server-go](https://github.com/AnimeAIChat/xiaozhi-server-go) | 改过的 Apache-2.0：未经书面授权不能运营多租户服务 | 与“朋友家庭”（R-022）冲突，不用 |

## 2. 个人 AI 网关和代理框架

| 项目 | 许可证、活跃度 | 是什么 | 判断 |
| --- | --- | --- | --- |
| [OpenClaw](https://github.com/openclaw/openclaw) | MIT，39 万星，当天有推送〔GitHub〕 | 一个网关进程管会话、路由和连接，手机和电脑作为“节点”提供摄像头、定位等；记忆是 Markdown 文件；面向单个使用者〔报道〕 | 形状和我们相近（中枢 + 节点），但面向一个人、TypeScript、不做儿童策略和低延时语音。借两点：节点模型、记忆可以让人直接看懂。不采用 |
| [Hermes Agent](https://github.com/NousResearch/hermes-agent) | MIT，26 万星〔GitHub〕 | 本次只看了仓库信息，没有细读 | 没有判断 |
| [Pydantic AI](https://github.com/pydantic/pydantic-ai)、[LangGraph](https://github.com/langchain-ai/langgraph)、[OpenAI Agents SDK](https://github.com/openai/openai-agents-python) | 都是 MIT，都活跃〔GitHub〕 | 通用代理框架 | 〔建议〕不用作核心。我们的工具循环要处理“待确认”“已过期”、回执里的 `say`、按成员的工具集，都是自己的语义，循环本身不长。以后某个长任务适合时再局部引入 |

## 3. 语音框架

| 项目 | 许可证、活跃度 | 特点 | 判断 |
| --- | --- | --- | --- |
| [Pipecat](https://github.com/pipecat-ai/pipecat) | BSD-2，1.7 万星，当天有推送〔GitHub〕 | 按流水线组装；要自己带传输。`services` 目录有 `funasr`、`qwen`、`minimax`、`deepseek`，没有看到火山和讯飞〔GitHub，只看目录名〕 | 备选。〔建议〕我们的传输、语音检测和打断都已经定了，只需要“识别 → 模型 → 断句 → 朗读”一条可取消的链，先自己写薄的；写不动再换它 |
| [LiveKit Agents](https://github.com/livekit/agents) | Apache-2.0，1.5 万星〔GitHub〕 | 绑在 WebRTC 媒体服务器上〔报道〕 | D-54 定了先用 WSS，不合 |
| [TEN Framework](https://github.com/TEN-framework/ten-framework) | Apache-2.0 加附加条件：不能放在终端设备上，不能用于和声网竞争〔仓库许可证原文〕 | — | 不用 |
| [FunASR](https://github.com/modelscope/FunASR)、[SenseVoice](https://github.com/QwenAudio/SenseVoice)、[sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx)、[Silero VAD](https://github.com/snakers4/silero-vad) | MIT、MIT、Apache-2.0、MIT〔GitHub〕；模型权重的许可证要另外核 | 本地识别和语音检测 | 可选的本地语音识别（“宽裕”档），不进首版必需项 |

## 4. 模型网关

| 项目 | 许可证 | 活跃度〔GitHub〕 | 特点 |
| --- | --- | --- | --- |
| [LiteLLM](https://github.com/BerriAI/litellm) | MIT，`enterprise/` 目录除外〔仓库许可证原文〕 | 6.3 万星，v1.108.0（2026-11-04） | Python 库或代理服务，接口最全 |
| [new-api](https://github.com/QuantumNous/new-api) | AGPL-3.0 | 5.1 万星，当天前后有推送 | Go，单容器，国内厂商全，有渠道和额度管理 |
| [one-api](https://github.com/songquanpeng/one-api) | MIT | 3.7 万星，最后推送 2026-01 | new-api 的上游，已不太活跃 |
| [Portkey Gateway](https://github.com/Portkey-AI/gateway) | MIT | 1.3 万星，最后推送 2026-05 | TypeScript |
| [Bifrost](https://github.com/maximhq/bifrost) | Apache-2.0 | 9 千星，当天有推送 | Go |

**LiteLLM 投毒事件**〔报道〕：2026-03-24，PyPI 上的 1.82.7 和 1.82.8 被植入窃取凭据的代码，约几十分钟后下架；入口说法不一（Trivy 被攻破、维护者账号被盗），修复版本 1.83.0。来源：[Arthur](https://www.arthur.ai/column/litellm-supply-chain-attack-pypi-compromise-2026)、[NetSPI](https://www.netspi.com/?p=32614)、[pypistats](https://pypistats.com/blog/litellm-pypi-supply-chain-compromise-how-a-popular-llm-proxy-became-a-credential-stealing-backdoor-march-24-2026)。这不说明它现在不安全，说明网关握有全部密钥，依赖要锁版本、要算进风险。

〔建议〕自己写薄路由，理由：

- 小智的 README 列出的厂商都能走 OpenAI 兼容接口，几乎只有一种线路格式要处理（再加 Anthropic 格式，共两种）。
- 我们要的是“这一轮用哪个角色的模型”“这个成员今天还有多少额度”，现成网关的额度按密钥和团队算，还是要自己记账。
- 依赖少，内存小；换网关只是改一个地址，决定可逆。

## 5. 记忆

| 项目 | 许可证、活跃度〔GitHub〕 | 特点 | 判断 |
| --- | --- | --- | --- |
| [mem0](https://github.com/mem0ai/mem0) | Apache-2.0，6.8 万星，2026-10-30 有推送 | 开源版默认 OpenAI 模型和向量、本地 Qdrant、SQLite 历史；自托管服务默认 Postgres + pgvector〔[文档](https://docs.mem0.ai/open-source/overview)〕；据说能用 `OPENAI_BASE_URL` 指向兼容接口〔第三方部署指南，未核实〕 | 备选后端。对中文的效果没有查到数据，也不区分观察、陈述和推断 |
| [Letta](https://github.com/letta-ai/letta) | Apache-2.0，2.5 万星 | 是完整的代理运行环境，不只是记忆 | 太大 |
| [Graphiti](https://github.com/getzep/graphiti) | Apache-2.0，3.3 万星 | 时序知识图谱，要图数据库（本次未细读） | 家庭规模用不上 |
| [LangMem](https://github.com/langchain-ai/langmem) | MIT，1.7 千星 | 绑 LangChain 生态 | 不用 |
| [Memobase](https://github.com/memodb-io/memobase) | Apache-2.0，2.9 千星，最后推送 2026-01 | 用户画像式记忆 | 活跃度一般 |
| OpenClaw 的做法 | — | 长期记忆一个 Markdown 文件，每日追加一个〔报道〕 | 简单透明，但不好按成员、儿童规则、来源管理 |

〔建议〕自己做，见[概要设计 §5.5](../hub/hub-architecture.md)。接口留成可替换，真发现效果不好再接 mem0。

## 6. 无头浏览器

| 项目 | 许可证、活跃度〔GitHub〕 | 判断 |
| --- | --- | --- |
| [Playwright](https://github.com/microsoft/playwright) | Apache-2.0，9.9 万星，v1.64.1（2026-10-14） | 采用，带 Python 接口；浏览器是 Chromium |
| [Browserless](https://github.com/browserless/browserless) | SSPL-1.0 或商业许可 | 不用 |
| [Steel Browser](https://github.com/steel-dev/steel-browser) | Apache-2.0，7.9 千星 | 面向自动化的服务封装，我们不需要这层 |
| [Lightpanda](https://github.com/lightpanda-io/browser) | AGPL-3.0，4.1 万星；README 说没有图形渲染引擎〔文档〕 | 给不了设备要的像素；以后可以考虑只取文字的场景 |

**内存**：一篇 2025 年的测量里 Chromium 峰值约 1094 MB，无头模式约 706 MB，最小配置约 690 MB（[Playwright Browser Footprint](https://datawookie.dev/blog/2025-06-06-playwright-browser-footprint/)）。这是一个人的一次测量，和页面有关。〔估算〕按一路 1 GB 算，上限用容器内存限制卡住，开发时实测。

## 7. 数据库和备份

| 项目 | 许可证、活跃度〔GitHub〕 | 判断 |
| --- | --- | --- |
| SQLite | 公有领域；FTS5 全文检索自带 | 采用，一家一个文件 |
| PostgreSQL + [pgvector](https://github.com/pgvector/pgvector) | PostgreSQL 许可证，2.4 万星 | 备选。多一个常驻服务、多一份备份流程；几十个家庭以内用不上 |
| [sqlite-vec](https://github.com/asg017/sqlite-vec) | Apache-2.0，8 千星，v0.1.9（2026-03）还在 1.0 之前 | 暂不用。一家的记忆只有几千到几万条，向量取出来暴力比较就够快〔估算〕 |
| [jieba](https://github.com/fxsjy/jieba) | MIT，最后推送 2024-08 | 中文分词后写入 FTS5；词典够用，不活跃但稳定 |
| [restic](https://github.com/restic/restic) | BSD-2，3.7 万星 | 采用，异地备份，客户端加密，支持 S3 兼容存储 |
| [Litestream](https://github.com/benbjohnson/litestream) | Apache-2.0，1.5 万星，v0.5.18 | 以后可选（持续复制，丢得更少）；首版设备上还有一份数据，每天一次够用 |
| [rclone](https://github.com/rclone/rclone) | MIT，6.1 万星 | restic 接不上的存储可以经它转 |

任务队列：[Huey](https://github.com/coleifer/huey)（MIT，支持 SQLite）可用；但底座设计 §5.6 的作业有阶段、幂等键、回执，这些要自己的表，所以〔建议〕直接写一个小的。

## 8. 内容转换

| 项目 | 许可证〔GitHub〕 | 判断 |
| --- | --- | --- |
| [EbookLib](https://github.com/aerkalov/ebooklib) | **AGPL-3.0** | 避开。EPUB 是压缩包加 XHTML，自己解析 |
| [PyMuPDF](https://github.com/pymupdf/PyMuPDF) | **AGPL-3.0** | 避开 |
| [pdfminer.six](https://github.com/pdfminer/pdfminer.six) | MIT | 文字型 PDF |
| [pypdfium2](https://github.com/pypdfium2-team/pypdfium2) | 仓库带 `LICENSES` 目录（REUSE 规范），GitHub 接口没有识别，用之前要核 | PDF 渲染和取文字 |
| [Pandoc](https://github.com/jgm/pandoc) | GPL-2.0 | 作为独立命令调用，不链接 |
| [Calibre](https://github.com/kovidgoyal/calibre) | GPL-3.0 | 作为可选组件调用（MOBI、AZW3），镜像很大 |
| [trafilatura](https://github.com/adbar/trafilatura) | Apache-2.0 | 从网页提取正文，报纸和 `web.fetch` 用 |
| [feedparser](https://github.com/kurtmckee/feedparser) | GitHub 接口未识别（官方说明是 BSD 系），用之前要核 | RSS |

本仓库目前没有许可证文件。AGPL 的影响要看以后怎么分发，用户没有定，所以先绕开。

## 9. 部署相关的事实

| 事情 | 内容 | 依据 |
| --- | --- | --- |
| IP 证书 | Let's Encrypt 从 2026-01-15 起正式签发 IP 地址证书，必须用 160 小时的短期证书，只能用 HTTP-01 或 TLS-ALPN-01 验证 | 〔文档〕[Let's Encrypt](https://letsencrypt.org/2026/01/15/6day-and-ip-general-availability)；验证方式来自[第三方](https://www.stackscale.com/blog/lets-encrypt-opens-the-door-to-https-by-ip/) |
| Caddy 用 IP 证书 | 有博客给了配置（`profile shortlived`、建议 v2.10 以上），官方文档没有核实 | 〔报道〕[博客](https://cream-blog.pages.dev/2026/03/31/%E4%BD%BF%E7%94%A8-acme-sh-%E7%94%B3%E8%AF%B7-IP-%E8%AF%81%E4%B9%A6%E5%B9%B6%E9%83%A8%E7%BD%B2%E5%88%B0-Caddy/) |
| 备案 | 阿里云的官方口径：备案要求看域名是否解析到中国内地服务器。未备案域名 80、443 被拦截的说法来自社区，没有查到官方文字；只用 IP、不用域名是否完全不涉及，**信息不足，买服务器时问服务商** | 〔文档〕[阿里云备案流程](https://help.aliyun.com/zh/icp-filing/access-to-the-registration-process)；〔报道〕[腾讯云开发者社区](https://cloud.tencent.com/developer/article/2498643)、[V2EX](https://v2ex.com/t/839643) |
| Docker Hub | 国内直接拉取不稳定，公开加速地址经常变；建议用自己的镜像仓库（阿里云 ACR 等）。ACR 个人版现在是否免费没有查到 | 〔报道〕[镜像源整理](https://gitcode.csdn.net/6aa0e4011a35784c8420ac85.html)、[博客](https://eastondev.com/blog/zh/posts/dev/20251217-docker-mirror-guide-2025/) |
| MCP | 现行版本 2026-07-28，无状态，去掉握手和会话；Python SDK 最新 v2.3.0（2026-10-02） | 〔报道〕[AAIF](https://aaif.io/blog/mcp-2026-07-28-whats-changing-and-how-to-migrate)；〔GitHub〕；本仓库更早的整理见[协议调研 §2](foundation/ai-native-protocols.md) |
| 海外模型 | Anthropic 的支持地区列表里没有中国内地（官方页面），也没有香港（第三方整理）；OpenAI 2024-07 起停止向中国内地和香港提供 API（报道）。所以默认候选只列国内厂商 | 〔文档〕[Anthropic Supported Regions](https://www.anthropic.com/legal/supported-regions)；〔报道〕[Medianama](https://www.medianama.com/2025/09/223-anthropic-service-policy-update-bans-ai-access-chinese-firms/)、[整理文章](https://blog.eimoon.com/p/anthropic-supported-countries-regions-2026-04/) |
| 配网 | 乐鑫推荐 `network_provisioning` 组件（SoftAP 或蓝牙，官方配手机 App）；手机扫 `WIFI:` 二维码加入热点并弹出页面的行为，这次没有查到权威资料，要实机试 | 〔文档〕[乐鑫组件](https://components.espressif.com/components/espressif/network_provisioning/versions/1.2.4/examples/wifi_prov) |
| 穿透 | [frp](https://github.com/fatedier/frp)，Apache-2.0，11 万星 | 〔GitHub〕 |

## 10. 模型和语音的价格

都是 2026-11-04 查到的官方标价，单位元。各家常有折扣和活动，以控制台为准。Claude、GPT 不在默认候选里（见 §9）。

### 10.1 对话模型（元/百万 token）

| 模型 | 输入 | 缓存命中 | 输出 | 来源 |
| --- | --- | --- | --- | --- |
| qwen3.8-max、qwen3.7-max | 12 | — | 36 | [百炼](https://help.aliyun.com/zh/model-studio/model-pricing) |
| qwen3.7-plus（256K 以内） | 2 | — | 8 | 百炼 |
| qwen3.8-flash | 0.8 | — | 2.7 | 百炼 |
| qwen3.7-flash（32K 以内） | 0.2 | — | 0.8 | 百炼 |
| qwen-flash（128K 以内） | 0.15 | — | 1.5 | 百炼 |
| doubao-seed-2.1-pro | 6 | 1.2 | 30 | [火山方舟](https://docs.volcengine.com/docs/82379/1544106) |
| doubao-seed-2.1-lite | 0.8 | 0.16 | 2.7 | 火山方舟 |
| doubao-seed-2.0-pro（32K 以内） | 3.2 | 0.64 | 16 | 火山方舟 |
| doubao-seed-2.0-mini（32K 以内） | 0.2 | 0.04 | 2 | 火山方舟 |
| deepseek-v4-pro | 闲时 4.5，忙时 9 | 闲时 0.15，忙时 0.30 | 闲时 13.5，忙时 27 | [DeepSeek](https://api-docs.deepseek.com/zh-cn/quick_start/pricing) |
| deepseek-flash（V4.1） | 闲时 1，忙时 2 | 闲时 0.02，忙时 0.04 | 闲时 4，忙时 8 | DeepSeek |
| glm-5.3 | 8 | — | 28 | 百炼（代售） |
| ZHIPU/GLM-5.3-Flash | 0.8 | — | 2.8 | 百炼（代售） |
| kimi-k3 | 20 | — | 100 | 百炼（代售） |
| kimi-k3.1-pro | 13 | — | 68 | 百炼（代售） |

DeepSeek 的忙时是北京时间工作日 9:00–12:00 和 14:00–18:00。“—”是这次没有查到缓存价，不是没有缓存。百炼写明不含限时优惠。Kimi 官网的价格表这次没抓到，用的是百炼上的代售价。

### 10.2 语音和向量

| 服务 | 价格 | 来源 |
| --- | --- | --- |
| 百炼实时识别 fun-asr-realtime | 0.00033 元/秒（约 1.19 元/小时） | [百炼](https://help.aliyun.com/zh/model-studio/model-pricing) |
| 百炼实时识别 paraformer-realtime-v2 | 0.00024 元/秒（约 0.86 元/小时） | 百炼 |
| 百炼朗读 qwen3-tts-flash-realtime | 1 元/万字符 | [百炼](https://docs.bailian.console.aliyun.com/zh/model-studio/qwen3-tts-flash-realtime) |
| 百炼朗读 cosyvoice-v3.5-flash / plus | 0.8 / 1.5 元/万字符 | 百炼 |
| 豆包流式识别 2.0 | 1 元/小时（按量）；另有资源包 | [火山引擎，页面更新于 2026-09-29](https://docs.volcengine.com/docs/6561/1359370) |
| 豆包语音合成 2.0 | 3 元/万字符（按量） | 同上 |
| 百炼向量 text-embedding-v4 | 0.5 元/百万 token | [百炼](https://help.aliyun.com/zh/model-studio/text-embedding-v4) |
| 识别词表（热词） | 百炼：`vocabulary_id`，先建表再引用；Fun-ASR-Realtime 最多 2000 词，paraformer 最多 500 词。标点默认开；VAD 断句静音阈值默认 800 ms（200～6000 ms）。豆包：`boosting_table_id`，每表 5000 词，也能在请求里直传少量词 | [百炼 提升识别准确率](https://help.aliyun.com/zh/model-studio/improve-asr-accuracy)、[Paraformer SDK](https://help.aliyun.com/zh/model-studio/paraformer-real-time-speech-recognition-python-sdk)、[豆包 热词与上下文](https://docs.volcengine.com/docs/DoubaoVoice/hot-words-and-context?lang=zh) |
| 联网搜索 | DeepSeek 的 Search 5 元/千次；国内搜索接口约从 12 元/千次起 | [DeepSeek](https://api-docs.deepseek.com/zh-cn/quick_start/pricing)；[第三方对比](https://select.keenable.ai/r/web-search-apis-china-market-comparison/3G5FI8ZzfPffaploN_Zmqw) |

一把密钥：百炼一个账号能调 Qwen、DeepSeek、Kimi、GLM 的模型，也有语音和向量（同一张价格表），配置向导最省事。〔建议〕以此起步，见 D-78。

### 10.3 每月费用估算

**这是估算，不是测量。** 假设（都可以改）：

- 全家合计每天 50 轮对话，一个月 1500 轮；每轮说 8 秒，朗读 120 字；
- 每轮 1.3 次主模型调用，每次输入 5000 token（其中 3000 是稳定前缀，有缓存价的按缓存价算），输出 200 token；
- 每轮一次便宜模型做分类（1000 进 30 出）；每天 10 次记忆整理（3000 进 300 出）；
- 每天两份报纸（成人、孩子各一份）：40 篇用便宜模型摘要（3000 进 300 出），一次主模型编排（10000 进 2000 出）；
- 每天 10 次联网搜索；便宜模型用 qwen3.7-flash，语音用百炼。

| 项目 | 元/月 |
| --- | --- |
| 语音识别（200 分钟） | 4 |
| 朗读（18 万字） | 18 |
| 便宜模型（分类、记忆、报纸摘要、向量） | 3 |
| 联网搜索 | 4 |
| **以上合计** | **约 28** |

主模型（对话 + 报纸编排）另算：

| 主模型 | 主模型部分 | 总计 | 加可选项后 |
| --- | --- | --- | --- |
| qwen3.7-plus | 25 | **约 53** | 约 91 |
| doubao-seed-2.0-pro | 26 | **约 55** | 约 93 |
| deepseek-v4-pro（闲时 / 忙时） | 28 / 56 | **约 56 / 84** | 约 94 / 122 |
| doubao-seed-2.1-pro | 49 | **约 78** | 约 116 |
| glm-5.3 | 97 | **约 125** | 约 163 |
| qwen3.8-max | 143 | **约 171** | 约 209 |
| kimi-k3 | 258 | **约 286** | 约 324 |

可选项：报纸朗读预生成（18 万字，18 元）和一本书的朗读（20 万字，20 元）。用豆包语音时朗读是 3 倍价。对话量翻倍，主模型和语音部分也大约翻倍。语音输入（听写）没算在内：每天 5 分钟约每月多 3 元（150 分钟 × 0.00033 元/秒）。

判断：D-07 的“每月一两百元”够用；只有 kimi-k3 这一档（在这个用量下）超出。质量差多少要实测，模型选择见 D-78。

**流量**〔估算〕：Opus 语音约 3.5 KB/秒，一轮（上行 8 秒、下行 30 秒，加页面快照）约 140 KB，全走 4G 一个月约 200 MB。一整页远程画面约 50 KB（[详细设计 §13](../system/system-detailed-design.md)，模拟器）。

## 修订记录

- 2026-11-04：初稿。按用户要求去掉服务器价格。
