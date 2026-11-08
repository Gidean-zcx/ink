# speech-cache 执行报告

基线：`291da6760931601e4783ebaddabc08e5bcab22eb`。范围：DE-R6，仅音频服务及其测试；产品代码预算 60 行。开始时工作区干净。

【实测＋实现】DE-R6 在当前基线不可复现；基线已含 `39b7dcf4` 的修复和对应回归。按任务书的不可复现分支交付，本次不修改产品代码或测试。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 不可复现；无需新增修复 | 【实测】已有 private 回归 33 通过、0 失败、旧语音新增 0 ms；仅在临时音频副本关闭身份校验，同一回归 32 通过、1 失败，检出 810 ms。两次均 `test.finished=1`、模拟器退出 0。原审计语音探针 case/control 各 11 通过、0 ms。 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^device-events$' --output-on-failure -j2`；负面对照命令见下次更新 |
| 2 | 全量验证进行中 | 【实测】首轮 CTest 202 通过、3 跳过、0 失败（109.11 s）；正补齐只读 Python 环境后重跑。产品代码增删 0 行。内存与隐私检查待执行。 | 待补最终命令与结果 |

【实现】`git merge-base --is-ancestor 39b7dcf4 291da676` 退出 0。修复已在基线内：[音频身份校验](../../../system/services/audio/audio_service.c)的 `playback_owner_matches`、`stage_next`、`staged_wanted`、`service_playback` 分别覆盖待播流、PCM 与播放出口；不只依赖成员状态通知。[既有回归](../../../tests/device_events/private.c)第 94–113 行先断言回答已结束且仍有超过 20 帧待播，再同时暂停 UI 与成员通知，等待 200 ms 后观察 800 ms 的扬声器增量。

## 没做到的

- 仍待内存检查；没有设备、真实厂商或 Linux gcc 的实测结论。

## 和设计不一致的

- 未发现本任务范围内的不一致。音频隔离属于服务内部机制，仍经平台 HAL 输出；没有新增用户动作、能力、页面或旁路。未改 `system/policy/`。

## 改过的已有测试

- 无。

## 要用户定的

- 【实测】原审计语音探针没有暂停 UI，关闭音频身份校验后也未检出泄漏；因此不以它单独证明修复有效。已有 private 回归同时暂停 UI 和状态通知，负面对照能检出上述 810 ms。
- 【实测】题设构建用的 Python 缺少中枢测试依赖，导致首轮 3 项明确跳过。现有 `$INK_DEPS/hub/.venv/bin/python` 为 Python 3.11.15，四项依赖均与 `hub/pyproject.toml` 固定版本一致；通过 `INK_HUB_PYTHON` 选用，不安装或修改依赖。

## 顺手发现

- 暂无。

## 新增的机制

- 无产品机制；本次仅验证已有实现。

## 要改的文档

- 主会话收拢时更新 `docs/tasks/backlog.md` 的 speech-cache 状态：基线已有修复，本次验证不可复现；不记为本次新增修复。其余共享文档无需因本任务改变。
