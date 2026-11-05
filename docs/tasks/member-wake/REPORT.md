# member-wake 执行报告

原开发基线：`103382b1`；本轮修复基线：`be7abb88`（独立复查记录已提交）。范围：`REVIEW.md` 的 MW-R1、MW-R2 及同类路径。状态：修复中；原报告的通过结论须由本轮回归重新确认，证据仍仅限本机模拟器。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| MW-R1 | 已修复【实测／实现】 | 重建主页前同步电源与认证；孩子、无 PIN 成人、有 PIN 成人的启动、隔离、保存和权限均通过；撤销该行修复后 15 条失败且 `test.finished=1` | `cmake --build build --target sim-apps -j4`；`ctest --test-dir build --output-on-failure -j1 -R '^integration-apps$'`（1/1）；反向对照见 `build/test-out/member-wake-r1-out/metrics.json` |
| MW-R2 | 待修复 | 后台 Wi-Fi 扫描跨过授权截止；补查非本地采集、语音缓冲和最终发送，本地录音保持原规则 | 本轮待补 |
| MW-1 | 通过【实测】 | 阅读闲置 121 秒不退出；自动锁屏后才起算免密码时段 | 定向回归命令，`integration-member-wake` |
| MW-2 | 通过【实测】 | 电源键、AI 键分别在锁屏后 119 秒直接回原页，121 秒进成员门；唤醒键不启动 AI | 同上 |
| MW-3 | 通过【实测】 | 错误 PIN 显示错误并保持关门；正确 PIN 经现有按键回调认证 | 同上 |
| MW-4 | 通过【实测】 | 真实阅读器第 4 页的页面对象、文本、内容版本、能力回执页码保持一致 | 同上 |
| MW-5 | 通过【实测】 | 孩子两种唤醒键均免密码；仍不能进入成人空间；无 PIN 成人不受影响 | 同上 |
| MW-6 | 通过【实测／实现】 | 暂定 120 秒只在 `IDLE_RETURN_S` 定义；沿用 `member.idle_return_s`，设置成 30 秒即生效 | 同上；`system/policy/members.c` |
| MW-7 | 通过【实测／实现】 | 后台队列阻塞时，状态尚未更新也拒绝过期空间访问；唤醒仍显示成员门；共享的 64 位计时值采用原子读写 | 同上；`system/policy/members.c` |
| MW-8 | 通过【实现】 | 同步服务设计 §11.4、应用恢复状态行及 `ui-dependencies.md` 对应状态 | `git diff --check` |
| G1 | 通过【实测】 | 普通 95/95（51.16 秒）、ASan/UBSan 95/95（82.64 秒），无跳过项；已有预算未改 | 下列全量命令 |
| G2 | 通过【实测／实现】 | 无新增用户操作；复用设置能力和人工 PIN 接口；关门时 AI 不能调用能力，成员门快照不含凭据 | 定向回归；现有 `integration-people` |
| S1 | 通过【实测】 | 四组撤修复均失败，恢复后通过；13 个相关文件；25 个本地文档链接有效；已提交差异隐私扫描零命中 | 见下；`git diff --check`、`git diff --stat 103382b1..HEAD`、总规则 §6 的隐私命令 |

配置沿用总规则 §5，两个目录都保留 `INK_WERROR=ON`，相关配置的唯一区别是 `INK_SANITIZE=OFF/ON`。中枢环境按 `hub/README.md` 用 uv 0.9.5、`uv sync --project hub --frozen` 安装，两个目录都指定 `-DINK_HUB_PYTHON="$PWD/hub/.venv/bin/python"`；未改锁文件、未升级依赖。以下 `ctest` 均使用 `$INK_DEPS/build-deps/pyenv/bin/ctest`：

```sh
cmake --build build -j4
ctest --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-asan --output-on-failure -j3
```

全部构建和全量测试逐轮串行；`gui-smoke` 仅用 SDL dummy 无窗口模式。最终两个构建中的 `integration-member-wake` 都是 174 条检查、`test.finished=1`、零警告、零预算违规。

## 没做到的

本节验收无遗留。没有执行 Linux/gcc 或实机测试；本机结果不代表已在实机验证。

## 和设计不一致的

没有新增产品设计差异。`MembersBack` 复用现有占位成员门，本节只改服务策略与恢复所必需的应用管理逻辑，不重画页面；G4 的页面像素对照不适用。应用管理器原来在认证失效时清空页面栈，已最小调整为同成员保留、实际换成员（含代际改变）才清空。

## 改过的已有测试

- `integration-people`：原“闲置 60 秒关门”改为“阅读不关门、锁屏 60 秒关门”；原“锁屏立即关门”改为“锁屏 30 秒后关门”；异步状态发布用条件等待核对。PIN 错误次数、递增等待和隔离断言保留。
- `integration-listening` 两处关闭空间的前置条件改为先等 30 秒锁屏时段，另断言刚锁屏仍有认证；原有禁止上传、结束 AI 会话等断言保留，预算和脚本时限未改。
- `integration-media`：锁屏立即关闭空间改为先确认有免密时段，再等 30 秒后检查关闭；原“媒体继续播放、音量键有效、AI 不能越权”等断言保留，预算和脚本时限未改。

定向回归命令：`ctest --test-dir build --output-on-failure -j3 -R '^integration-(member-wake|people|listening|apps|media)$'`，5/5 通过；新增测试 174 条检查、`test.finished=1`、零警告。

反向对照：每次只改一处，`cmake --build build --target sim-member-wake -j4` 均退出 0，再执行 `ctest --test-dir build --output-on-failure -j1 -R '^integration-member-wake$'` 均退出 8。原文件逐次恢复，最终重建与复测均退出 0。

| 撤掉的修复 | 失败检查数 | 关键失败 |
| --- | --- | --- |
| 将 `members.c` 换成基线版本（`git show 103382b1:system/policy/members.c`） | 25 | 阅读 121 秒仍应开放、119 秒免密返回 |
| 将 `app_registry.c` 换成基线版本 | 32 | 成员门底下变成 `home`，没有保留原书页 |
| 仅移除访问入口的截止时间检查 | 1 | 后台队列阻塞时，状态更新前仍须拒绝过期空间 |
| 仅移除唤醒时的截止时间检查 | 5 | AI 键唤醒不得绕过排队中的过期处理 |

## 要用户定的

大人免密码返回时长仍按 D-95 暂定为 120 秒；保持单处可改，不将暂定值视为最终确认。

## 顺手发现

本机 CMake 提示 `INK_CJK_MEDIUM_FONT`、`INK_CJK_BOLD_FONT` 未被基线使用；未处理这条基线提示。没有新增 `write`、`read` 或 `fgets` 调用。
