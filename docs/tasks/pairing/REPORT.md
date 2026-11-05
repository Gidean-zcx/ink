# pairing 执行报告

2026-11-06。接续 [独立复查](REVIEW.md)，本轮起点 `47c82d67`，合入 `FETCH_HEAD` 的 `1c43c28a`。门禁只有合并日志，唯一冲突是 `hub_link.c`；构建、测试、CI 和隐私门禁尚未执行。以下是本轮重新验证的结果，取代旧验收结论。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| I1 合并集成分支 | 已解决冲突【实现／实测】 | 共用发送队列同时保留配对阶段标志和成员 generation/epoch；更新全部调用点。21 个集成文件与目标提交完全一致，另 4 个共用文件逐项核对；索引无未合并条目 | T1、T5 |
| I2 配对与成员边界 | 通过【实测】 | 普通响应、错误、通知、语音及带成员的控制帧不能在未认证连接发送；配对响应仍能完成。成员过期、快速切换后返回、旧实时会话与 AI／听写缓冲沿用集成回归 | T1、T3 |
| I3 反向对照 | 20/20 通过【实测】 | 原 18 项及新增两项均触发预期保护断言。去掉 generation/epoch 的变体完整运行到 `test.finished=1`，两条成员隔离断言失败，证明旧帧不会因切回同一成员而复活 | T3 |
| P1 配网、身份与状态 | 通过（模拟器）【实测】 | 五状态、固定公钥、同公钥迁址、跨公钥授权清除、认证错误分类、AI 权限与凭据脱敏通过 | T1、T2 |
| P2 热点与配对文件 | 通过（模拟器）【实测】 | 热点网页模型、随机密码、成员隔离、启动导入、可选 Wi-Fi 恢复、人工确认与文件删除通过 | T1、T2 |
| P3 断线与持久化恢复 | 通过（模拟器）【实测】 | 原进程重启及写入故障矩阵通过；accept／complete 单次丢失、请求槽满、远端错误及过期均有回归 | T1、T2、T3 |
| P4 吊销与资料处理 | 通过（模拟器）【实测】 | 已绑定吊销及布尔 wipe 通过；未受信任 begin、公钥变更及字符串 wipe 不能授权清空 | T1、T2、T3 |
| G1 全量与内存检查 | 通过【实测】 | 普通 98/98，62.04 秒；ASan/UBSan 98/98，99.55 秒；均含 SDL dummy 桌面冒烟，无跳过，预算未放宽 | T1、T2 |
| G2 能力与分层 | 保持【实现／实测】 | 沿用原子能力和中枢任务；人工与 AI 仍走原能力，最终 accept 和凭据仍受原策略约束；没有新增用户操作、页面、任务或预算 | T1、T2；源码差异 |
| G3 依赖状态 | 保持【实现】 | 保留配对服务／网页模型与页面待接的边界，并保留集成分支的成员唤醒状态说明，没有改原判断 | T5 |
| G4 页面验收 | 不适用【实现】 | 未新增页面，桌面冒烟仅验证既有输入路径；没有产生新的 UI 对照分数 | — |
| G5 范围与隐私 | 核查进行中【实测】 | 本轮冲突修复及补测仅涉及 `hub_link.c`、两个配对测试文件和本报告；其余变更来自集成提交。合并前差异检查、文件大小及路径／MAC 模式扫描通过，提交后复核待完成 | T4、T5 |

所有构建最多 `-j4`，全量测试最多 `-j3`，构建与全量测试逐轮串行。两目录均为 `INK_WERROR=ON`、`INK_GUI=ON`，内存目录另加 `INK_SANITIZE=ON`；仅经 SDL dummy 运行桌面冒烟，未启动带窗口的模拟器。

配置使用总规则 §5 的字体、Python 和 LVGL 参数；另将 `FETCHCONTENT_SOURCE_DIR_SDL2`、`FETCHCONTENT_SOURCE_DIR_IMGUI` 指向只读 `$INK_DEPS/build/_deps/` 中对应的 `sdl2-src`、`imgui-src`，不下载或改依赖。既有 `INK_HUB_PYTHON` 保持不变。下面的 `ctest`、`python` 均使用 `$INK_DEPS/build-deps/pyenv/bin/` 中的可执行文件。

- T1：`cmake --build build -j4`；`SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build --output-on-failure -j3`，均退出 0，98/98。日志 `work/pairing-gate-configure.log`、`work/pairing-gate-build.log`、`work/pairing-gate-full.log`。补测前另跑配对、成员唤醒、应用及 AI 能力定向 6/6，见 `work/pairing-gate-targeted.log`。
- T2：同样配置换 `build-asan` 并加 `INK_SANITIZE=ON`；`cmake --build build-asan -j4`、`SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-asan --output-on-failure -j3` 均退出 0，98/98。日志 `work/pairing-gate-asan-configure.log`、`work/pairing-gate-asan-build.log`、`work/pairing-gate-asan-full.log`。
- T3：`python tests/pairing/reverse.py --build build`，最终整轮 20/20、退出 0；日志 `work/pairing-gate-reverse.log`，证据 `build/pairing-reverse-ll7hu1qt/`。仅在被忽略目录中生成替代对象，不撤改生产源码或断言。新增两项另用 `--only member-control-before-auth --only stale-member-frame` 复验也是 2/2、退出 0。
- T4：按 CMake 生成的各目标参数，对相对起点及集成提交有变化的 25 个 C 编译单元逐一加 `-Wsign-compare -Werror=unused-result -fsyntax-only`，均退出 0；日志 `work/pairing-gate-strict.log`。本轮及原配对分支新增的 POSIX `write`、`read`、`fgets` 调用均为 0；该检查仍是 clang，不代替 Linux/gcc。
- T5：`git diff --stat 47c82d67`、`git diff --check 47c82d67`，后者退出 0；28 个文件均存在且不超过 5 MB。分别扫描相对起点和 `FETCH_HEAD` 的差异，使用总规则 §6 的 `grep -nE` 模式，均 0 命中（grep 退出 1）。日志 `work/pairing-gate-checks.log`；提交后及报告链接核查待完成。

## 没做到的

- 本机是 macOS／Apple clang，没有 Linux/gcc 环境，未实测 Linux CI。没有执行真实设备、TLS/NVS、手机网页或家庭服务器验证；不把本机模拟器通过写成这些环境通过。
- 提交后核查待完成；构建、测试和反向对照均已完成。

## 和设计不一致的

- 沿用模拟器内部 Flash 原子存储保存配对配置和身份；真机 NVS 映射、真实手机网页与首次开机页面仍未实现。本轮没有改设计迁就代码。
- 集成分支已有的成员唤醒设计和状态说明原样保留；没有修改需求、想法或决策记录。

## 改过的已有测试

- 本轮在 `tests/pairing/device.c` 只追加四条未认证连接拒发断言；`reverse.py` 保留原 18 项，新增两项合并边界对照。没有删除测试、修改原有期望、预算或脚本时限。
- 合入的 `people` 原有期望由闲置／锁屏立即关闭改为锁屏宽限到期关闭；`listening` 两处、`media` 一处按既有 30 秒设置核对到期，保留关闭后的隔离断言。这些是 `FETCH_HEAD` 为 D-95 调整的既有内容，逐字保留，详见 [member-wake 报告](../member-wake/REPORT.md)；本轮没有额外调整等待。
- `apps`、`people` 的新增生命周期断言及两项 `member-wake` 测试均保留；桌面冒烟恢复到构建默认的启用状态。

## 要用户定的

本轮没有新决定。继续沿用此前暂定的配对文件 JSON 编码和 D-95 时长，不变更产品范围。

## 顺手发现

- 新增反向脚本初次从 stderr 找成员断言，实际警告存于 `metrics.json` 的 `warnings`。撤保护已经触发预期的两条失败；改为读取原始指标后复验通过，未放宽产品测试。
- SDL 和链接器有既有第三方／重复库警告；不在本轮修改范围。正式代码仍以 `INK_WERROR=ON` 构建。
