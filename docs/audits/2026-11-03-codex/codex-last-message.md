审计已完成，产出位于指定目录：

- [审计报告](docs/audits/2026-11-03-codex/report.md)：覆盖 A～I，含目标目录、路径映射和迁移建议。
- [问题清单](docs/audits/2026-11-03-codex/findings.json)：共 **24 项**，严重 5、高 10、中 8、低 1。
- [复现说明与日志](docs/audits/2026-11-03-codex/experiments/README.md)。

使用已记录的构建绕过后，普通及 ASan/UBSan 测试均 **35/35 通过**；新增探针仍确认多项正确性与硬件模型缺陷。

未修改产品文件，未提交、推送或接触设备。