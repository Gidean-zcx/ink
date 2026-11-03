# CLAUDE.md

本仓库的协作约定在 [AGENTS.md](AGENTS.md)，照做：中文交流、证据分级、需求与建议分开记录等。AGENTS.md 末尾的“Codex and OpenCode hybrid routing”只适用于 Codex，Claude Code 忽略。

## 新会话先读

1. [docs/background.md](docs/background.md)：项目全貌、证据边界、构建测试、已知问题。
2. [docs/project.md](docs/project.md)：最近的接续记录。
3. 涉及实机时：[docs/hardware/README.md](docs/hardware/README.md)，包括规则、上游配置要点和实测记录。
4. 在用户的电脑上接手时：[docs/handoff/local-claude-code.md](docs/handoff/local-claude-code.md)。

## 硬性规则

- **写设备前**（烧录、擦除、写 NVS、OTA）必须有核对过的整片备份，并得到用户明确同意。
- **不可逆或高风险的操作不做**：不烧 eFuse，不开 Flash 加密或安全启动，不改 4G 模组的固件或配置。
- **仓库是公开的**：备份、原始日志、NVS、TF 卡数据不进仓库。提交前检查没有 MAC、IMEI、ICCID、Wi-Fi 信息、令牌。
- **等用户明确选择的事**：采购、花钱（包括短信和电话费）、拆机改硬件、删除历史资料、对外发布。

## 构建与测试

```bash
cmake -S . -B build && cmake --build build -j && ctest --test-dir build --output-on-failure
```

需要拉取 LVGL，还需要 Noto Sans CJK 字体，见 [docs/background.md §11](docs/background.md)。模拟器只在 Linux 上验证过，以 GitHub Actions 的 Linux CI 为准。
