# pairing 执行报告

基线：`103382b1`。范围：仅 backlog 的 pairing 服务任务；使用模拟器与合成身份，不接触设备或真实凭据。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| P1 配对状态机及配网、令牌隔离 | 阶段通过 | 【实测】Wi-Fi 与家庭身份独立持久化 | T1 |
| P2 模拟器热点、小网页和存储卡配对文件 | 阶段通过 | 【实测】本地页面提交、启动自动导入、删除一次性文件 | T1 |
| P3 掉线、确认丢失、重启恢复 | 正向通过；反向待运行 | 【实测】begin、accept、complete、hello 的中断和重启 | T1 |
| P4 吊销与数据处理 | 正向通过；反向待运行 | 【实测】普通移除保留私有 Flash 和卡资料，wipe 清除两者 | T1 |
| G1 全量与内存检查 | 执行中 | 首轮全量 92/94；资源影响已修正，受影响的 3 项针对复测通过；最终全量与内存检查待完成 | T1 |
| G2 原子能力与成员隔离 | 已实现，最终全量待完成 | 【实现】9 条能力/事件；人确认加入家庭，AI 读脱敏状态；复用 wifi.connect | `tests/integration/ai_surface.c` |
| G3 依赖状态 | 已更新 | 【实现】仅改配网和家庭绑定一行的状态格 | `git diff 103382b1 -- docs/system/ui-dependencies.md` |
| G4 页面验收 | 不适用 | 服务任务，不新增页面 | — |
| G5 服务范围 | 阶段核查通过 | 服务、HAL 模型、启动装配、测试；不新增页面 | `git diff --stat 103382b1` |
| 隐私与提交范围 | 待检查 | 完工前检查基线差异 | 待记录 |

## 没做到的

- 尚在执行。

## 和设计不一致的

- 待核查。

## 改过的已有测试

- `tests/integration/ai_surface.c`：新增 pairing.status、7 项仅供人的能力、pairing.state 和 pairing.changed 的期望；没有改旧期望或预算。

## 要用户定的

- 暂无；按现有设计推进可逆事项。

## 顺手发现

- 已有状态监听名额接近上限；本任务没有增加监听名额或预算。卡文件在启动时自动读取，也可显式调用 pairing.import。

阶段记录：服务实现与正向故障矩阵完成。热点网页为模拟器内部模型，不启动宿主 HTTP 监听或窗口；TLS 公钥也仅为合成模型。已有 proto:1 开发场景保留；配置家庭服务器后走 proto:2 配对。

T1：`ctest --test-dir build -R 'pairing-recovery|integration-update|scenario-ai_ask-metalio-plus-pqbn' --output-on-failure -j3`，3/3 通过。最终全量、ASan/UBSan 和反向对照待补录。
