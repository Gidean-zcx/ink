# 依赖来源与许可证

查阅日期：2026-11-05。运行和测试依赖均来自 PyPI 官方索引；精确版本、源码包/各平台 wheel 的 SHA-256 和下载地址在 [uv.lock](../uv.lock)。容器安装清单 [requirements.lock](../requirements.lock) 同样带哈希。这里保留本次安装包附带的许可证文字，仅清理 Pygments 名单中的一处行尾空格。

| 包 | 固定版本 | 许可 |
| --- | --- | --- |
| aiohttp | 3.12.15 | Apache-2.0 AND MIT |
| aiohappyeyeballs | 2.7.1 | PSF-2.0 |
| aiosignal | 1.4.0 | Apache-2.0 |
| attrs | 26.1.0 | MIT |
| frozenlist | 1.8.0 | Apache-2.0 |
| idna | 3.20 | BSD-3-Clause |
| multidict | 6.9.1 | Apache-2.0 |
| propcache | 0.5.4 | Apache-2.0 |
| PyYAML | 6.0.3 | MIT |
| typing_extensions | 4.16.0 | PSF-2.0 |
| yarl | 1.25.1 | Apache-2.0 |
| pytest | 8.4.2 | MIT |
| pytest-asyncio | 1.2.0 | Apache-2.0 |
| iniconfig | 2.3.1 | MIT |
| packaging | 26.3 | Apache-2.0 OR BSD-2-Clause |
| pluggy | 1.7.0 | MIT |
| Pygments | 2.21.0 | BSD-2-Clause |

开发工具 uv 0.9.5 和构建后端 hatchling 1.27.0 不随产品分发。没有把设计稿、依赖源码包、wheel、Python 环境或构建产物提交进仓库。

容器基底为官方 `python:3.11.15-slim-bookworm`，2026-11-05 从 [Docker Hub 标签元数据](https://hub.docker.com/v2/repositories/library/python/tags/3.11.15-slim-bookworm) 核对清单摘要，两个 Dockerfile 都固定到 `sha256:d29f48a31a8b408ed19272ca1e7b10ebae13b240a27e862d3d4217c528e2e0c3`；本机未构建镜像。
