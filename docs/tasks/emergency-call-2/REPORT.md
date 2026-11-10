# emergency-call-2 执行报告

基线：`951bdfc15f0997a77cd9ad960b056eee10fb3ef8`。仅本地编译与宿主机测试，不接触设备、不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 功能、原测试、新检查与反向对照 | 【实测】专项通过 | 六个原场景及禁用电话变体，共 825 条断言；5 组反向对照成立 | `tests/emergency_call/run.py --build build`；分别加 `--negative radio/page/ownership/entry/prefix` |
| 1 普通全量测试 | 首轮 341/346，最终轮待跑 | 首轮原话 `99% tests passed, 5 tests failed out of 346`；相机与文档问题已修正，中枢复测通过，另两项基线对象对照仍失败 | `work/emergency-call-2/full.log`、`verified.log`、`hub-recheck.log`、`baseline-check.log` |
| 1 ASan 全量测试 | 构建通过，测试待跑 | 同普通配置，仅换目录并加 `-DINK_SANITIZE=ON` | `work/emergency-call-2/build-asan-final.log` |
| 2 能力清单 | 【实测】通过 | 520 项定义；公开入口的锁屏描述按新标记生成 | `python3 tools/capabilities.py`、`--check` |
| 2 S31 | 编译中 | 仅编译、链接、尺寸与静态栈检查 | `taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 2 gcccheck | 【实测】通过 | `gcccheck: 11 compiled, 0 with findings`；无跳过 | `work/emergency-call-2/gcccheck.log` |
| 2 隐私 | 待最终扫描 | 待记录 | 待记录 |
| 3 与原实现的差别 | 【实现】完成 | 见下文 | `git diff 951bdfc1..HEAD` |

## 没做到的

- 尚在执行。

## 和设计不一致的

- 沿用归档的模型假设，NT26 紧急拨号、无 SIM 和临时射频还没有实机证据；没有修改规范迁就实现。

## 改过的已有测试

- `integration/ai_surface.c`：保留当前主线所有条目，登记紧急入口及四项只限本人操作的能力。
- 同文件 `sms.send` 的必填参数预期从 `to` 改为 `to,text`：指定基线的通信服务已要求两项，旧检查首轮稳定失败；保留“全部必填且无额外必填”的断言，没有放宽。
- `kid_apps.c`：原有“所有 phone.* 都有应用归属”检查对明确列出的五个紧急能力改为断言“系统归属、孩子可用”；其他能力仍执行原断言。这是任务第 4 项要求的归属变化。
- 归档紧急呼叫测试六场景和原断言保留，增加入口语义、AI 从锁屏／未认证成员门打开、标记与名称无关、禁用电话策略变体。
- 紧急测试驱动在场景期间持有 `INK_NEED_AWAKE`，仅使宿主驱动能在挂断后继续执行；不持有 `INK_NEED_NO_LOCK`，仍验证电源锁屏、成员认证过期和关闭后仍锁屏。首轮未持有时驱动随整机休眠，`test.finished` 未发生；未加 sleep 或延长时限。

## 要用户定的

- AI 是否可直接拨紧急号码：沿用原任务的保守选择，只能开页面，不能拨号、输号码或挂断。

## 顺手发现

- 【实现】当前 `members.c` 将电话列为必备应用，`kid.app_set` 存入禁用记录后仍被必备规则覆盖。专项测试只在构建目录源码副本中移除电话的这一项豁免，再经原有家长能力存入 mode=0；验证普通电话确实被拒、紧急能力仍可用。未改变产品的必备应用规则。
- 【实测／实现】布局变化触发 `lock-camera` 孩子场景的 TPS65185 告警；显示上电可插入休眠准备并重新打开 XOE。统一上电入口复用已有 `sim_power_lease`，新增两行调用，未改参数；原相机测试通过，临时撤掉两行后同一告警恢复。证据：`verified.log`、`negative-display.log`。原厂 `main/boards/common/power_policy/README.md:69` 明确冻结后续 UI 并等物理刷新完成；本次只补模拟器已有互斥的覆盖缺口。
- 【实测】将本任务修改的八个产品 C 文件全部替换为指定基线源码编译的临时对象（仍用当前头文件和依赖），`app-framework-gaps` 的 `legacy success remains zero` 和 `integration-races` 的 `the background is busy` 仍在相同虚拟时刻失败。未扩大范围修复这两项。
- 【实测】首轮中枢端到端 `tool_calls=0`，最终源码单项复测通过（17.66 秒）；最终全量仍需核对。
- 【实测】GCC 初查发现显示端口原有四处 `uint32_t` 配 `%u` 的类型诊断；补 `(unsigned)` 后 11 文件零问题。日志 `gcccheck-first.log` 保留未修状态的编译失败。

## 新增的机制

- `ink_cap_t.locked_entry`：任务第 2 项允许的标记；表示不读取成员资料的公开页面入口。锁屏策略、AI 清单和成功回执均按标记判断。
- 紧急浮层与临时呼叫／射频标记：迁入原任务第 1、2 项，复用现有任务、HAL 和事件。
- `phone.emergency_hangup`：任务第 4 项要求紧急能力独立于电话应用名单；复用既有挂断处理，仅接受本成员的紧急通话。普通 `call.hangup` 继续属于电话应用。
- 显示上电复用已有的休眠互斥，未新增机制；为保证任务要求的原相机回归通过而补齐覆盖。
- 无新任务、线程、状态机、持久格式、队列、缓存、重试或配置项。

## 和 emergency-call 原实现的差别

- 不迁入 `camera.*`／`phone.*`／`call.*` 锁屏前缀特判；紧急浮层声明 `locked_caps`，普通电话能力使用既有 `locked_owner_only`，公开入口使用新增标记。
- 保留 audit14-fix 的 `.app` 映射和当前主线电话功能；五个紧急能力明确 `.app = NULL`。
- 保留 semantics-shell-1 的版本、对象及动作描述，在锁屏和成员门补紧急动作；成员门动作容量从 4 调到 5 并加静态断言。
- 紧急页面的挂断从 `call.hangup` 改为系统能力，避免禁用电话时无法结束紧急通话。
- 保留主线已有翻译、能力契约和分钟通话时长，只补紧急功能所需内容。
- 生成器识别公开入口标记，避免将 AI 可用的紧急入口错写成“锁定时仅人工”。

## 要改的文档

- 已按任务明确要求重新生成能力清单；`app-framework.md` 仅校正两处已失效的源码行号链接，正文合同未改。其余共享文档不改。

## 里程碑

- 已核对工作区干净、当前 HEAD 与指定基线一致；在独立任务分支执行。
- 已迁入归档功能和六个原场景，完成当前闸门与语义模型的适配。
- 里程碑 1：【实测】普通构建退出 0；紧急专项七场景 825 条检查通过；四组功能反向对照均到达 `test.finished` 并命中预期失败，第五组验证重新加入前缀判断会使静态检查失败。证据在 `work/emergency-call-2/emergency-final.log`、`negative-*.log`。
- 里程碑 2：【实测】普通及 ASan 构建通过，原相机回归与反向对照完成；GCC 11 文件零问题。`integration-ai-surface`、`audit14-ownership`、`audit14-locked`、六项 `semantics-shell-*`、原电话、飞行模式、成员唤醒检查通过。S31 编译中。

## 为什么不能更小

- 产品 C／头文件新增 184 行、删除 41 行；连同 CMake、英文词条和生成器保守计新增 207、删除 43，低于原功能 250 行新增预算。需要专用不透明拨号浮层、号码校验、临时射频、能力和双语；复用普通电话页会暴露联系人／记录。
- 新主线适配复用已有页面白名单和应用归属机制；仅增加一个公开入口标记和复用现有处理的紧急挂断能力，没有另建权限表或调用路径。

## 上机先核对清单

- 查阅日期 2026-11-11。只读原厂 `upstream/Metalio-E-INK4-Plus/main/boards/common/board/uart_eth_modem.cc:1596–1611`：初始化先 `CFUN=1`，无 SIM 保留 AT 通道；对应板型及公共模组未搜到 `ATD`、`CFUN=4` 或紧急呼叫实现。
- 【估算】沿用本项目 `ATD<number>;` 和 `CFUN=4 → 1 → 4`，需核对 NT26 命令、无 SIM／受限服务拨号及结束回执。模型通过不能当实机通过。
- 本次只编译和宿主机测试。上机绝不拨真实紧急号码；只读查询，或经用户明确同意用指定普通号码替代。
