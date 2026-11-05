# member-wake 独立复查

复查日期：2026-11-06。开发基线：`103382b1`；复查提交：`6b4fb6c6`。范围：`backlog.md` 开头 G1～G5 与 `member-wake`。上一轮记录取自 `be7abb88`，优先复核 MW-R1、MW-R2。仅写本报告，不改产品代码、不提交。

**结论：本轮范围内通过，严重 0、高 0。** 上一轮两项高问题已修复；普通和 ASan/UBSan 全量各 96/96 通过。旧复现脚本、正式回归和撤销修复的对照均由本轮独立执行，没有直接采用开发报告的测试结论。证据限本机模拟器。

## 上一轮问题复核

### MW-R1【原高，已修复】过期成人成员门换人后的应用生命周期

- 【实现】`system/ui/app_registry.c:279` 在重建主页前同步电源和认证状态；`:285` 区分实际换成员与同成员重新认证；`:496` 在启动时也同步状态。
- 【实测】上一轮 `REVIEW_CASE=lifecycle` 原脚本在普通、ASan 构建均 **16/16、退出 0**。从过期成人门选择孩子后，应用重新启动、清除旧成员内存、按孩子保存状态，前台应用权限恢复。
- 【实测】`integration-apps` **164/164**，另覆盖无 PIN 成人、有 PIN 成人、旧成员停止及双方状态恢复。已有 PIN 和成员表不可读的连续启动场景分别通过，未认证时主页应用保持停止。
- 【反向实测】只移除 `member_job` 的状态同步，新测试失败 **15 条**；只移除启动同步，`table2`、`table3` 各失败 **1 条**。均完成测试，不是崩溃或超时造成的失败。

### MW-R2【原高，已修复】后台 Wi-Fi 扫描跨过截止后继续上传语音

- 【实现】`system/services/audio/audio_service.c:206`、`system/services/ai/ai_service.c:96`、`system/services/text/voice_input.c:129` 在采集和缓冲处理时检查有效授权；`system/services/hub/rt.c:211` 保留会话原成员和成员代际；`system/services/hub/hub_link.c:382`、`:722` 在入队及最终发送前复核授权，已排队帧还核对撤销轮次。
- 【实测】上一轮 `REVIEW_CASE=uplinkscan` 原脚本在普通、ASan 构建均 **13/13、退出 0**：默认 120 秒、真实模拟 Wi-Fi 扫描占用后台执行器，无需唤醒，到期后停止继续上传。
- 【实测】`integration-member-wake-stream` **87/87**，补查通用非本地采集、AI/听写等待 `rt.open` 的缓冲、到期前入队的语音、成员快速往返及旧会话在孩子身份下发送；同成员的本地文件录音继续。
- 【反向实测】分别替换为修复前的采集、AI、传输、听写、会话实现，新测试分别失败 **2/1/6/1/1 条**，均到达 `test.finished=1`。

## 逐条验收

以下测试均包含在本轮普通和 ASan 全量运行中。

| 项目 | 结论 | 独立证据 |
| --- | --- | --- |
| 阅读闲置不退出；自动锁屏后才起算 | 通过【实测】 | `integration-member-wake`：阅读 121 秒仍开放；自动锁屏后的第 119 秒直接返回 |
| 1 分 59 秒 / 2 分 01 秒；电源键 / AI 键 | 通过【实测】 | 两键 × 两时点均执行；119 秒免密，121 秒进成员门；唤醒键不额外启动 AI，短暂唤醒取消旧截止 |
| 密码错误 | 通过【实测】 | 成员门实际按钮回调输入错误密码后保持关闭并显示错误；正确密码才开放；`integration-people` 保留递增等待和成员隔离检查 |
| 回原页、原位置 | 通过【实测】 | 真实阅读器第 4 页的页面对象、文本、内容版本、能力回执页码保持；同成员认证恢复前台，实际换人重建主页 |
| 孩子不受影响 | 通过【实测】 | 孩子两键唤醒免密、不能进入带 PIN 成人；无 PIN 成人也不受影响；过期成人门换人路径见 MW-R1 |
| 时长一处可改、仍为暂定 | 通过【实测／实现】 | `members.c:37` 单处默认 120；`:714` 保留原设置 id，范围 30～3600；测试经人工确认改为 30 后生效 |
| 报告 MW-7：后台繁忙不能延长授权 | 通过【实测】 | `integration-member-wake` 核对访问及唤醒关口；`integration-member-wake-stream` 核对持续采集、缓冲及最终发送，见 MW-R2 |
| 服务设计 §11.4 | 已同步【实现】 | `docs/system/system-services-design.md:720` 记录锁屏起算、暂定时长、同页恢复、换成员和语音撤销；与本轮验证一致 |
| G1 全量、内存检查、预算 | 通过【实测】 | 普通与 ASan/UBSan 均 96/96；既有预算文件未修改，新预算要求测试完成且零失败、零警告 |
| G2 能力、AI 快照及凭据边界 | 通过【实测／实现】 | `integration-ai-surface`、`integration-people` 及新回归通过；复用成员、设置能力及脱敏成员门快照，PIN 仍仅人工输入；未新增用户操作或页面 |
| G3 依赖状态 | 通过【实现】 | `docs/system/ui-dependencies.md:159` 已更新实际状态，仍注明暂定时长、模拟器证据及占位外观，没有改变需求判断 |
| G4 页面规格与对照图 | 不适用 | 本节为服务策略任务；没有页面外观改动，未声称完成 `MembersBack` 设计稿。未把占位外观计作页面验收通过 |
| G5 范围 | 符合【实现】 | 基线以来共 25 个相关文件；应用管理器改动用于原页保留及生命周期恢复，传输接口用于原成员绑定；未开发具体应用或修改无关设计 |

## 独立命令与结果

沿用已有构建缓存：两种构建均 `INK_WERROR=ON`，内存检查另为 `INK_SANITIZE=ON`；依赖来自给定的 `$INK_DEPS`。逐轮串行执行。

| 命令 | 结果 |
| --- | --- |
| `cmake --build build -j4` | 退出 0 |
| `$INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j3` | 退出 0，96/96，41.53 秒 |
| `cmake --build build-asan -j4` | 退出 0 |
| `UBSAN_OPTIONS=halt_on_error=1 $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build-asan --output-on-failure -j3` | 退出 0，96/96，76.43 秒；无 ASan/UBSan 报错 |
| 两种构建的 `test-out/{member-wake,member-wake-stream,apps}/metrics.json` | 分别 174、87、164 条检查通过，均 `test.finished=1`、零警告、零预算违规 |
| `git diff --check 103382b1..HEAD` | 退出 0 |
| `git diff --stat 103382b1..HEAD`、隐私及新增 I/O 调用扫描 | 25 个相关文件；本机路径/MAC 零命中；未新增 `write/read/fgets` 调用 |

复跑旧复现的命令如下；本轮分别使用 `build`、`build-asan` 与 `lifecycle`、`uplinkscan` 四种组合。旧脚本读取当前源码，只把探针编译到被忽略的测试输出目录，不依赖本报告旧内容仍在工作区。

```sh
REVIEW_BUILD=build REVIEW_CASE=lifecycle REVIEW_VARIANT=current \
UBSAN_OPTIONS=halt_on_error=1 python3 - <<'PY'
import subprocess
doc = subprocess.check_output(
    ['git', 'show', 'be7abb88:docs/tasks/member-wake/REVIEW.md'], text=True)
code = doc.split('<!-- member-wake-probe -->\n```python\n', 1)[1].split('\n```', 1)[0]
exec(compile(code, '<old-member-wake-review>', 'exec'))
PY
```

旧探针证据在两种构建的 `test-out/review-member-{lifecycle,uplinkscan}-current-out/metrics.json`，编译、链接、运行均退出 0。

## 测试有效性：撤销修复的对照

本轮从标准输入编译替代对象，使用对应目标的 `flags.make`、`link.txt` 链接到独立可执行文件；没有修改工作区源码或正式测试。AI、听写替换对应测试包装对象；旧传输实现缺少新增内部接口时，仅加转发到旧发送函数的兼容入口，不增加授权检查。以下编译和链接全部退出 0；各失败运行均退出 1，且 `test.finished=1`。

| 临时撤销内容 | 使用的正式测试 | 失败检查数 | 证据目录，位于 `build/test-out/` |
| --- | --- | --- | --- |
| 成员策略换回 `103382b1` | member-wake | 25 | `review2-reverse-members-baseline-out` |
| 应用管理器换回 `103382b1` | member-wake | 32 | `review2-reverse-apps-baseline-out` |
| 仅去掉访问时的截止检查 | member-wake | 1 | `review2-reverse-access-deadline-out` |
| 仅去掉唤醒时的截止检查 | member-wake | 5 | `review2-reverse-wake-deadline-out` |
| 仅去掉成员门换人时的状态同步 | apps | 15 | `review2-reverse-member-lifecycle-out` |
| 仅去掉启动时的状态同步 | people，table2 / table3 | 各 1 | `review2-boot-reverse/table2`、`table3` |
| 音频采集换回 `be7abb88` | member-wake-stream | 2 | `review2-reverse-audio-out` |
| AI 缓冲换回 `be7abb88` | member-wake-stream | 1 | `review2-reverse-ai-out` |
| 传输队列换回 `be7abb88` | member-wake-stream | 6 | `review2-reverse-hub-out` |
| 听写缓冲换回 `be7abb88` | member-wake-stream | 1 | `review2-reverse-dictation-out` |
| 实时会话换回 `be7abb88` | member-wake-stream | 1 | `review2-reverse-session-out` |

启动对照使用各自独立的合成 Flash 镜像，依次执行 `INK_PEOPLE_PHASE=table1/table2/table3`；`table2` 另设 `INK_TEST_TABLE_FAILS=2`。当前实现三阶段分别 **2/11/5 条通过、退出 0**，证据在 `review2-boot-current/`。汇总另见 `review2-reverse-summary.json`。

既有 `people`、`listening`、`media` 测试的期望变更与新规则一致：原来“锁屏立即关闭”的检查改为先确认宽限期仍开放、再等待配置的截止。新增延时用于推进明确的模拟时间，信号量用于固定队列边界；未发现删除测试、放宽断言/既有预算或用等待掩盖失败。

## 总规则与架构审视

- **总规则 §1、§4**：差异未触及设备、真实凭据、受保护资料或第三方依赖。分层、确定性和能力检查均随全量通过。新增大块队列/缓冲仍在 PSRAM，没有新增常驻任务；生产代码未引用模拟器头文件，LVGL 操作仍在 UI 任务。
- **R-061**：成员策略负责时长和有效授权，应用管理器负责生命周期，采集/缓冲/传输分别在执行边界复核；依赖方向成立。新增 `ink_hub_send_frame_for` 是内部会话归属机制，不是遗漏的用户能力。设置仍走现有能力和确认策略，凭据不开放给 AI。
- **证据边界**：本机为 macOS clang，未运行 Linux/gcc，也未操作实机；没有将模拟器通过写成设备验证。成员门外观仍待独立页面任务完成。
- **资源与改动**：构建最多 `-j4`、ctest 最多 `-j3`；构建和全量测试不并行。未启动带窗口的模拟器，既有 `gui-smoke` 使用 `SDL_VIDEODRIVER=dummy`。除本报告外，仅生成被忽略的构建/测试产物；没有提交、推送或改开发报告。

SEVERE=0 HIGH=0
