# member-wake 执行报告

原开发基线：`103382b1`；本轮修复基线：`be7abb88`。范围：[独立复查](REVIEW.md) 的 MW-R1、MW-R2 及同类路径。复查列出严重 0、高 2，没有中级项。状态：两项修复和定向回归完成，全量与内存检查待执行。本轮证据仅限本机模拟器。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| MW-R1 | 已修复【实测／实现】 | 重建主页前同步电源及授权状态；过期成人门 → 孩子、无 PIN 成人、有 PIN 成人，均检查应用启动、旧成员停止、内存隔离、保存恢复和前台权限；`integration-apps` 164 条通过 | `cmake --build build --target sim-apps -j4`；定向命令见下 |
| MW-R2 | 已修复【实测／实现】 | 截止检查独立于后台通知；非本地采集停止，AI 和听写丢弃缓冲；实时会话保留原成员与代际，传输队列在最终发送前检查成员、代际、撤销轮次及有效授权；本地录音继续 | `integration-member-wake-stream` 87 条通过；定向命令见下 |
| MW-1～MW-6 | 待本轮全量复验 | 保留原有阅读闲置、两键 × 119/121 秒、错误/正确 PIN、原书第 4 页、孩子及无 PIN 成人、设置时长回归 | `integration-member-wake`、`integration-people` |
| MW-7 | 已补齐持续流撤销【实测／实现】 | 正常 Wi-Fi 扫描跨过默认 120 秒截止，无需唤醒；通用采集及排队语音也受约束；状态通知仍可排队，授权执行不等它 | `integration-member-wake-stream` |
| MW-8 / G3 | 已同步【实现】 | 服务设计 §11.4 的合同未变；依赖表补记成员切换和语音撤销回归，仍注明暂定时长及占位成员门 | `git diff --check` |
| G1 | 待执行 | 普通全量、ASan/UBSan 全量及预算待本轮验证；未改预算 | 见下 |
| G2 / R-061 | 通过【实现】 | 成员策略负责授权；应用管理器负责生命周期；音频、语音和传输各在执行关口复核。新增接口仅传递原成员和代际，没有新增用户操作或页面；按钮和 AI 仍走原能力，PIN 仍仅人工输入 | 相关源码、现有 `integration-ai-surface` |
| G4 / G5 | 范围符合【实现】 | 没有页面外观任务，像素对照不适用；仅改服务、内部发送接口、恢复必需的应用管理器、构建测试及报告/状态说明 | `git diff --stat be7abb88..HEAD` |
| S1 | 部分完成 | R1 撤修复失败 15 条；R2 五层分别撤销后失败 2/1/6/1/1 条，均到达 `test.finished=1`，最终文件、链接、隐私检查待全量后记录 | 反向对照见下 |

定向命令：`ctest --test-dir build --output-on-failure -j3 -R '^integration-(member-wake-stream|apps|listening)$'`，3/3 通过。原复查脚本不作修改，`REVIEW_CASE=lifecycle`、`REVIEW_CASE=uplinkscan` 均编译/链接/运行退出 0，分别 16/16、13/13，零警告、零预算违规。

新增持续流回归使用真实麦克风与 `wifi.scan`，补查通用非本地采集、真实本地文件录音、等待 `rt.open` 的 AI/听写缓冲、已经入队的语音、成员快速往返和旧会话继续发送。队列边界用信号量固定发送前的时刻；延时只用于推进明确的模拟截止时刻，没有加等待掩盖失败。

构建沿用总规则 §5，两个目录均为 `INK_WERROR=ON`，内存检查仅增加 `INK_SANITIZE=ON`，保留原 `INK_HUB_PYTHON`。以下 `ctest` 均指 `$INK_DEPS/build-deps/pyenv/bin/ctest`：

```sh
cmake --build build -j4
ctest --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-asan --output-on-failure -j3
```

反向对照使用 `git show be7abb88:<源文件>` 从标准输入编译替代对象，在现有测试对象/静态库之前链接；AI 和听写替换对应测试包装对象。临时对象、可执行文件和指标均在被忽略的 `build/test-out/`，没有撤改工作区源码、覆盖正式测试或改已有断言。旧传输源缺少新内部接口时，仅补同签名转发到旧发送函数，保留旧行为供对照。

| 撤销的层 | 失败检查数 | 证据目录（均在 `build/test-out/`） |
| --- | --- | --- |
| MW-R1：成员切换时同步锁定状态 | 15 | `member-wake-r1-out` |
| MW-R2：通用音频采集截止检查 | 2 | `member-wake-r2-audio-out` |
| MW-R2：AI 发送及缓冲截止检查 | 1 | `member-wake-r2-ai-out` |
| MW-R2：传输队列检查 | 6 | `member-wake-r2-hub-out` |
| MW-R2：听写缓冲截止检查 | 1 | `member-wake-r2-dictation-out` |
| MW-R2：实时会话原成员绑定 | 1 | `member-wake-r2-session-out` |

## 没做到的

全量和内存检查尚未完成。没有执行 Linux/gcc 或实机测试；macOS clang 的结果不代替这些验证。

## 和设计不一致的

没有新增产品设计差异。成员门仍为占位外观；同一成员重新认证继续保留原页面，实际换成员才重建主页。本地录音不因同成员空间到期而停止。

## 改过的已有测试

本轮没有修改已有断言、期望、预算或脚本时限；`integration-apps` 追加三个成员门生命周期场景，另新增持续流回归及只读缓冲观察包装。原开发轮改过的 `people`、`listening`、`media` 期望未再次改动。

## 要用户定的

免密码返回时长仍按 D-95 暂定 120 秒，沿用单处定义和既有设置；本轮没有新增产品决定。

## 顺手发现

既有 CMake 字体参数提示不在本轮范围。没有新增 `write`、`read`、`fgets` 调用。最终差异与本地文档链接检查待补。
