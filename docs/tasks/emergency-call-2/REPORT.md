# emergency-call-2 执行报告

基线：`951bdfc15f0997a77cd9ad960b056eee10fb3ef8`。归档来源：`origin/archive/emergency-call`（`b7f04ba9`）。仅本地提交、编译和宿主机测试，没有连接设备或推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 迁入原功能与测试 | 【实测】通过 | 六个原场景及禁用电话策略变体，7 场景、826 条检查；锁屏拨号、普通号码拒绝、手动／远端挂断、射频恢复、无 SIM 模型、中英文与快照隔离均通过 | C1；`work/emergency-call-2/emergency-final.log` |
| 1b 新闸门、语义与应用隔离 | 【实测】通过 | 两个浮层快照导出紧急动作；无 phone 名称的合成入口按标记可用，带 phone 名称但无标记的入口拒绝；五个紧急能力均为系统归属。禁用变体中普通拨号和挂断被拒，紧急呼叫照常 | C1；`tests/emergency_call/policy.py` |
| 1c 反向对照 | 【实测】通过 | 六个专项模式均命中指定失败；另撤销显示互斥、恢复旧测试检查顺序／等待宏，也分别使原相机、框架和历史索引断言失败，均到达 `test.finished` | C2；`negative-*.log` |
| 1d 普通全量 | 【实测】通过 | 原话 `100% tests passed out of 346`；321.32 秒，0 跳过 | C3；`work/emergency-call-2/full-clean.log` |
| 1e ASan/UBSan 全量 | 最终复跑中 | 两套构建通过；首轮无消毒器诊断，修正测试同步后正在完整复跑 | C4；`work/emergency-call-2/asan-final.log` |
| 2a 能力清单 | 【实测】通过 | `PASS: 520 definitions, docs/system/capabilities.md`；生成及 `--check` 均通过 | C5 |
| 2b S31 | 【实测】通过 | 完整构建、链接、镜像、尺寸和静态栈报告退出 0；0 编译警告；镜像 3,201,200 字节 | C6；`work/emergency-call-2/s31.log`、`build-s31/size.txt`、`stack.txt` |
| 2c gcccheck | 【实测】通过 | `gcccheck: 12 compiled, 0 with findings`，无跳过 | C7；`work/emergency-call-2/gcccheck.log` |
| 2d 范围与隐私 | 【实测】通过，结束前再核对 | 24 个相关文件，均小于 5 MiB；diff 检查无错误；总规则隐私扫描 grep 退出 1、0 匹配 | C8；`work/emergency-call-2/privacy.log` |
| 3 原实现差别 | 【实现】完成 | 见下文；不迁入旧的名称特判 | `git diff 951bdfc1..HEAD` |

## 没做到的

- 最终内存全量尚在运行。没有运行 Linux 原生全量；本次为 macOS 宿主测试及 RISC-V GCC 检查。
- 未进行实机验证。当前 S31 蜂窝 HAL 仍是桩（`ports/esp32s31/hal_stubs.c:53`、`:57`）；模型通过不代表真机能拨号。

## 和设计不一致的

- 未通过修改设计合同迁就实现。继承原实现的证据边界：NT26 紧急命令、无 SIM、受限服务及临时射频行为仍是模型假设，待按下文核对。

## 改过的已有测试

- `ai_surface.c` 保留主线已有条目，登记紧急入口及四项仅限本人操作的能力。既有 `sms.send` 必填预期由 `to` 改成 `to,text`：指定基线的服务已要求这两项，保留完整参数集合断言。
- `kid_apps.c` 对明确列出的五个紧急能力断言系统归属且孩子可用；其他 phone/call 能力仍按原应用归属规则检查，没有按前缀跳过。
- `whole_product.h` 的等待宏返回成功的那次观测，消除退出循环后再次查询导致的假失败；上限和 20 ms 间隔不改。新增首次真、再次假条件的最小回归；恢复旧宏后该回归及原索引断言均失败。
- `fw_gaps/app.c` 将原两条 DIRECT 订阅检查移到已有队列屏障之后，等已退订的 UI 回调收尾再跨执行器复用槽。断言、100 次页面循环和容量检查不改；恢复旧位置后原断言失败。
- 归档紧急测试的六个场景和原断言保留，补入口语义、标记与名称无关、禁用电话及等待宏回归。测试驱动持有 `INK_NEED_AWAKE`，使其能在挂断后继续操作；不持有 `INK_NEED_NO_LOCK`，仍验证锁屏和认证过期。未加 sleep、改资源预算或延长超时。
- `index_durable/device.c` 最终保持原样；局部加强等待未解决问题，已撤回，统一修复等待宏。

## 要用户定的

- 无新增产品决定。任务已明确只有人能拨号、AI 只打开页面。本人点按紧急拨号是 D-119／D-105 的明确直接操作，不弹确认卡；不扩大普通 AI 通话的免确认例外。

## 顺手发现

- 【实现／实测】电话在主线是必备应用，设置接口拒绝 mode=0，读取也强制 mode=2。专项只在构建目录副本中移除这两处电话豁免，经原家长 PIN 确认保存 mode=0；验证普通电话确实拒绝、紧急能力仍可用。产品的必备应用规则未改。
- 【实测】新布局触发原相机测试的 TPS65185 告警。统一上电入口复用已有 `sim_power_lease`，新增两行调用，避免显示上电插入休眠准备；未改硬件参数。撤销两行后原告警恢复。原厂依据：`main/boards/common/power_policy/README.md:69` 要求冻结后续 UI 并等物理刷新完成。
- 【实测】首次普通全量 341/346，首轮 ASan 344/346；最终补齐源码链接、参数契约及测试同步后重新跑全量。索引失败起初误判为旧输出残留，全新 ASan 对照排除了该判断，实际是等待宏二次取样；当前索引单项原已通过，失败在历史索引对照阶段。
- 【实测】GCC 初查发现显示端口原有四处 `uint32_t` 配 `%u`，补 `(unsigned)` 后零问题；未修版本的失败保留在 `gcccheck-first.log`。主机链接器的重复库提示未改；S31 无编译警告。

## 新增的机制

- `ink_cap_t.locked_entry`：任务第 2 项允许的标记，表示不含成员资料的公开页面入口；策略、AI 清单和成功回执都按标记判断。
- 紧急浮层及短暂呼叫／射频标记：迁入原任务第 1、2 项，复用现有 UI、通信任务、HAL 和事件。
- `phone.emergency_hangup`：任务第 4 项要求紧急能力独立于电话应用名单；复用原挂断处理，仅接受本成员的紧急通话。普通挂断仍归电话应用。
- 未额外新增任务、线程、状态机、持久格式、队列、缓存、重试或配置项；显示互斥与测试同步均复用已有机制。

## 和 emergency-call 原实现的差别

- 不迁入 camera/phone/call 名称前缀特判；`opens_emergency()` 按标记判断。紧急浮层声明 `locked_caps`，普通电话使用已有 `locked_owner_only`。
- 保留 audit14-fix 的 `.app` 映射，五个紧急能力明确 `.app = NULL`；紧急页挂断改用独立系统能力，避免禁用电话时挂不了紧急通话。
- 保留 semantics-shell-1 的对象、版本和动作描述，在锁屏与成员门增加紧急动作；成员门动作容量 4 改 5，并加静态断言。
- 保留主线翻译、能力契约和分钟通话时长；生成器识别公开入口标记，避免把 AI 可用的入口写成“锁定时仅人工”。

架构审视：页面只建界面并调用能力，通信与射频归通信服务，权限统一由能力总线执行。按钮与 AI 共用同一入口；拨号、按键、关闭和挂断依任务限制为本人操作。两个公开入口有语义动作，紧急页有隔离快照。

## 为什么不能更小

- 产品 C／头文件新增 184 行、删除 41 行，增删合计 225；含 CMake、英文词条和生成器保守计新增 207、删除 43，合计 250，未超过原功能预算。
- 必须有不透明拨号浮层、号码校验、临时射频、权限与双语。看过复用普通电话页的更小方案，但它包含联系人和记录；现复用已有浮层、页面白名单、应用归属及通信处理，没有另建权限框架。

## 要改的文档

- 已按任务要求重新生成能力清单。`app-framework.md` 仅修正两处源码行号链接，正文合同不改。需求、想法、决策、backlog、ui-dependencies 及既有研究／审计资料未改。

## 上机先核对清单

- 查阅日期 2026-11-11。只读原厂 `upstream/Metalio-E-INK4-Plus/main/boards/common/board/uart_eth_modem.cc:1596–1611`：先 `CFUN=1`，无 SIM 保留 AT 通道；对应板型及公共模组未找到紧急拨号实现。
- 【估算】沿用本项目 `ATD<number>;`、`CFUN=4 → 1 → 4`；需核对 NT26 命令、无 SIM／未注册／受限服务、射频回执和通话结束上报。
- 本次不碰设备。上机绝不拨真实紧急号码；只读查询，或经用户明确同意用指定普通号码替代。

## 验证命令与证据

构建均为 `taskpolicy -c utility cmake --build <目录> -j2`；普通配置沿用总规则，另指定依赖目录里的 SDL2、ImGui、中枢 Python 和编译数据库。ASan 配置只换构建目录并加 `-DINK_SANITIZE=ON`，保留 WERROR。依赖只读，没有安装或升级。全量复跑前清理生成的 test-out 并重新配置恢复输入脚本，原指标另存 `work/emergency-call-2/`。

- C1：`INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/emergency_call/run.py --build build`。
- C2：C1 分别追加 `--negative radio`、`--negative page`、`--negative ownership`、`--negative entry`、`--negative prefix`、`--negative wait`。额外显示和测试同步反向对照在临时源码副本中撤销修复，按编译数据库与现有 link.txt 编译链接；脚本及完整日志位于 `work/emergency-call-2/`，产品文件未动。
- C3：`PYTHONDONTWRITEBYTECODE=1 PYTHONPATH="$PWD/hub" UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`。
- C4：C3 改为 `--test-dir build-asan`，另加 `ASAN_OPTIONS=halt_on_error=1`；没有缩减测试项目。
- C5：`taskpolicy -c utility python3 tools/capabilities.py`，随后同命令加 `--check`。
- C6：预设 `INK_DEPS` 后执行 `taskpolicy -c utility bash ports/esp32s31/build.sh`。
- C7：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B "$INK_DEPS/../ink-codex/tools/gcccheck.py" build $(git diff --name-only 951bdfc15f0997a77cd9ad960b056eee10fb3ef8 -- '*.c')`；12 文件列表另存 `gcccheck-files.txt`。
- C8：`git diff 951bdfc1..HEAD --check`、`git diff --stat 951bdfc1..HEAD`，以及[总规则第 6 节](../codex-rules.md)的隐私扫描命令。
