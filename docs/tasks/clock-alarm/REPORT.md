# clock-alarm 任务报告

日期：2026-11-09。基线：`d0cacd1d4b348a8e885904026f3796a10996c646`。工作分支：`claude/codex-clock-alarm`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】部分完成 | 真实触摸新建、能力删除、同一模拟闪存重启后保留三条；编辑、开关缺服务接口，未做 | 定向命令 |
| 2 | 【实测】通过 | 每天 127、工作日 62、自选周日／周六 65；显示、原始记录、重启读取均断言 | 定向命令 |
| 3 | 【实测】通过 | 实际响铃，触摸稍后响保存十分钟，AI 调原能力关掉；页面与 AI 的 3072 字节快照一致 | 定向命令 |
| 4 | 【实测】未全通过 | 普通及 ASan／UBSan 全量各 217/218；唯一失败均为禁止本任务修改的共享能力目录；产品 228 行 | 完整验证命令 |

## 没做到的

- 编辑和开关闹钟未做。现有[时间接口](../../../system/include/ink/sys/time.h)仅有 add、cancel、next、dismiss、snooze，[时间能力表](../../../system/services/time/time_caps.c)也没有 update／enable 能力；新增接口会越过任务书“只做页面和接已有能力”的边界，因此没有用删除重建冒充编辑，也没有把停用做成删除。
- G1 尚未满足：共享 `docs/system/capabilities.md` 待主会话重新生成；未绕过、跳过或放宽其校验。
- 尚未运行 Linux／gcc 和实机；不操作真机，不推送或开 PR。

## 和设计不一致的

- Clock 对照 43.297/255，ClockRing 为 35.467/255，均超过 12。Clock 未放任务排除的“计时”入口；下一个闹钟只显示真实时间；增加 F4 页码条。已有闹钟详情明确提示暂不支持修改和开关；字号、比例、灰色均取当前主题，不为分数改规格。ClockRing presenter 本任务未改。
- 【实测】逐图核查：Clock 三张卡片随当前主题放大，上方没有画板中的“明早／还有多久”，三条测试闹钟均开启；ClockRing 原 presenter 没有画状态栏，图标更大、时间字号较小；它的旧场景实际为 22:47、11 月 4 日，画板是 7:30、11 月 5 日。未改真实时间或主题来压低分数。

## 改过的已有测试

- `tests/integration/apps.c`：精确应用数量 6 → 7，新增时钟应用；并追加其初始状态应为 stopped 的断言，原有状态检查保留。
- `tests/integration/ai_surface.c`：在 AI 能力白名单末尾追加七个时钟能力；open 必须有 id，draft 参数可选，其余无需参数。没有删除断言、放宽资源预算或跳过测试。

## 要用户定的

- 暂无；任务内可逆选择按最保守方案处理。

## 顺手发现

- 新增能力后，生成的能力目录需要主会话统一更新；依任务总规则第 7 节，本任务不修改该共享文件。

## 新增的机制

- Clock 列表、新建表单及详情页：任务书要求；复用 F4、F2、F1 和时间服务，新增七个页面导航／草稿／保存能力，底层写入仍走 `alarm.set`／`alarm.cancel`。
- 无新产品任务、线程、状态机、持久格式、配置项、缓存或队列；只保留三条可见记录和当前表单，位于 PSRAM。测试驱动任务仅在测试程序内。

## 要改的文档

- 完成后由主会话更新 backlog、ui-dependencies 和生成的能力目录；本任务不改共享文档。

## 阶段证据

- 【实测】`taskpolicy -c utility cmake --build build -j2` 退出 0；开启 `INK_WERROR=ON`，没有放宽任何预算。定向测试退出 0，包含两个独立进程的保存／重启读取。
- 【实现】页面不写服务内部状态；触摸和 AI 都经注册能力，响铃继续使用既有系统打断卡；`alarm.revision` 通过既有 UI 执行器刷新页面，LVGL 只在 UI 任务调用。
- 【实测】首轮定向检查发现键盘打开时快照超出 AI 的 3072 字节缓冲；输入期间的操作归键盘，表单快照在这时不重复列出不可执行的页面按钮，完整字段及真实输入会话仍可读。十分钟起点按实际点击前后虚拟时间界定，未新增 sleep。

定向命令（`INK_DEPS` 指用户给定的依赖检出）：

```bash
taskpolicy -c utility cmake --build build --target sim-clock-alarm -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^clock-alarm$' --output-on-failure -j2
```

日志：`build/clock-build.log`、`build/clock-targeted.log`；截图、模拟闪存及两轮结果在忽略目录 `build/test-out/clock-alarm-*/`。

## 反向对照

- 【实测】仅在独立编译对象中撤掉“输入期间不重复列出页面动作”的修复；编译、链接均为 0，原测试程序退出 1，162 项通过、4 项失败、finished=1。四次名称输入均触发 AI 快照缓冲断言，未改原源码、断言或正常可执行文件。日志及指标在 `build/clock-negative/`。
- 可复现命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/clock_alarm_negative.py --build build`。

## 规模与验证

- 【实测】产品源文件 220 行，boot 注册 3 行，CMake 共 5 行（含测试接线也计入），保守合计 228 行，不超过 300。超过 100 行是因为包含列表、表单、详情、F1／F2／F4 接线和七个能力声明；看过只接 `alarm.set` 的更小方案，但它不提供新建表单、分页和同源快照。未增加时间服务实现。
- 【实测】普通全量首轮 215/218；更新上面两份固定清单后为 217/218，CTest 退出 8，210.85 秒，无跳过。仅 `capability-table-doc` 因共享目录过期失败；单独生成到 `build/clock-capabilities.md` 并 `--check` 通过，265 条定义。
- 【实测】ASan／UBSan 的配置和完整构建退出 0；已核对产品及场景测试编译参数含 `-Werror -fsanitize=address,undefined -fno-omit-frame-pointer`，全量启用 `UBSAN_OPTIONS=halt_on_error=1`。217/218，退出 8，368.31 秒，无跳过；唯一失败同为 `capability-table-doc`，没有 Sanitizer 错误。日志：`build-asan/clock-build.log`、`build-asan/clock-ctest.log`。
- 【实测】新场景创建轮 166 项断言通过，重启轮 24 项通过；内部 RAM 最低余量分别为 48,192 和 47,840 字节，原门槛 32,768；内外部内存分配失败均为 0。`ui_scale_check.py system/apps/clock/clock.c` 通过。
- 【实测】对照图及分数在 `build/clock-compare/`。命令为 `tools/ui_compare.py --build build --out build/clock-compare --reuse --scenes Clock ClockRing`；Clock 截图和指标直接复制自成功验收场景，ClockRing 由该工具的正常场景入口生成。

完整验证命令：配置使用总规则第 5 节全部依赖参数；额外指定 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"` 使用已有、只读的服务器测试依赖。ASan 目录只另启用 `-DINK_SANITIZE=ON`，未更改其他设置；未安装或升级依赖。

```bash
export PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1
taskpolicy -c utility cmake --build build -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
export UBSAN_OPTIONS=halt_on_error=1
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```
