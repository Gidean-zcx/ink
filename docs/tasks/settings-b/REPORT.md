# settings-b 执行报告

基线：`558c7ba8604fc321073b943716f49512a7dc3690`。分支：`claude/codex-settings-b`。仅本地提交，未推送、未碰设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 部分完成 | 【实测】五页可打开；成员添加、改名、确认删除、密码、儿童音量和壁纸连接真实后端。跨进程重启验证成员名、删除状态、密码、音量、壁纸及活动记录。应用/时段、运行记录和记忆缺口见下文 | C1 |
| 2 | 通过 | 【实测】儿童通过 AI 和直接打开路径均进不了五页；去掉页面检查，5 条断言失败，测试仍运行结束。反例未修改产品源码 | C1、C2 |
| 3 | 通过 | 【实测】F9 选择真实编号的合成 PNG；【实现】经现有解码和抖动接口在 MEDIA 执行；【实测】主页、锁屏显示选中图片，重启后可重新加载 | C1 |
| 4 | 通过已实现部分 | 【实测】五页快照可放入真实 AI 上下文；输入、成员操作、设置、选图复用能力。密码遮蔽，AI 无权输入或提交；设置密码后仍可读快照。恢复旧认证代次检查，5 条断言失败 | C1、C2 |
| 5 | 未全部通过 | 【实测】普通和 ASan/UBSan 全量各 233 项：228 通过、2 项文档检查失败、3 项缺依赖而跳过，未检测到内存错误。最后的密码页修正另跑普通和 ASan 专项，各 195+20 条检查通过。新增产品代码 **334 行**（含 CMake/头文件），低于 350 行 | C1、C3 |

构建采用任务书的完整 CMake 参数；ASan 仅增加 `-DINK_SANITIZE=ON`。构建和测试均以 `taskpolicy -c utility` 启动，构建 `-j2`、CTest `-j2`。下面命令在仓库根目录执行：

```sh
# C1：最终专项，分别验证写入和独立进程重启
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -R '^settings-b$' -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -R '^settings-b$' -j2
# C2：撤掉儿童检查，以及恢复旧认证代次判断
 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/settings_b_reverse.py --build build
# C3：全量，当前均因两项文档检查退出 8
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
# C4：生产页面截图，再调用 tools/ui_compare.py --reuse
 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/settings_b_compare.py --build build --design "$INK_DESIGN"
```

【实测】全量证据：`build/ctest.log`、`build-asan/ctest.log`；最终专项：两目录的 `settings-b-test.log`。专项最低内部 RAM 余量 **47,360 字节**，预算仍为 32,768。反例的命令、失败和源码哈希在 `build/settings-b-reverse/`，源码哈希核对一致。壁纸截图在 `build/test-out/settings-b-sim-settings-b/write/snap-{WallpaperSelected,HomeWallpaper,LockWallpaper}.png`。

## 没做到的

- 【实现】基线儿童策略只有 `members.c:set_allow` 固定白名单、应用 `child_ok` 和 `audio.child_max`，没有可修改的应用白名单、每日时长或禁用时段接口。`SetKid` 读取已有策略并修改音量；应用与时段编辑未完成，未代做并行 `kid-1` 的服务。
- 【实现】设备和 `hub/` 没有记忆列举/删除接口，`AiMemory` 如实显示不可用，未伪造条目或删除。
- 【实现】`Records` 显示 `activity.list` 最近 20 条并在页内翻页；更早记录、运行记录、跨成员查看和删除尚未实现。后端也未提供稳定活动记录编号。
- 【实测】全量失败：`capability-table-doc`（新增能力/设置未同步生成文档）、`services-mid-capability-doc`（服务文档缺 `member.rename`）。按任务规则留给主会话同步，没有改测试放行。三个 `hub-*` 测试缺 Python 依赖而跳过。本机为 Apple clang，未实测 Linux/gcc 或真机。
- 【实现】壁纸保存稳定文件编号；重启依赖原图仍可读取，没有新增壁纸副本格式。

## 和设计不一致的

【实测】C4 的五页 1/4 灰度平均差全部超过 12/255；证据在 `build/settings-b-compare/`：

| 页 | 分数 | 差别 |
| --- | ---: | --- |
| SetMembers | 49.042 | 独立分页行，缺头像和连续分组；添加/改名使用现有输入框，密码入口明确是自己的密码 |
| SetKid | 50.290 | 实际策略和音量，缺画板的应用开关、时长和时段选择 |
| Wallpaper | 61.845 | 选图、锁屏样式、自动锁屏已接入，缺三种预览、壁纸缩略图和通知内容开关 |
| Records | 42.545 | 显示真实 AI 活动条目，缺成员分段、分类计数和全部删除 |
| AiMemory | 28.311 | 不可用状态，没有画板的记忆条目和忘掉按钮 |

字号、控件和灰度采用当前主题，未为分数回退规格。`ui-scale`：35 个文件，0 违规。未修改 `settings_pages.c`；共用文件增加启动和导航接线，锁屏另修改一行以启用壁纸全屏容器。

## 改过的已有测试

`tests/integration/ai_surface.c` 只增加能力合同，没有删原条目或放宽断言：

- AI 可调用：`shell.family`、`shell.kid_settings`、`shell.wallpaper`、`shell.records`、`shell.ai_memory`、`family.edit`、`family.focus`、`family.save`、`family.role`、`family.pin`、`family.prev`、`family.next`、`wallpaper.choose`、`member.rename`。`family.edit` 的 `id` 可选，`member.rename` 必须给 `id,name`，其他无参数。
- 仅人可调用：`family.old_pin`、`family.pin_save`，原因是密码只能在设备上由本人输入和提交。

## 要用户定的

无新增决策。`SetBluetooth` 本次不实现，既有蓝牙能力保留。

## 顺手发现

无任务外修复。新页曾在设置密码后因认证代次变化丢失快照，现用已有页面归属检查，并在密码保存成功后重建成员页；已补正向和反向验证。

【实测】结束检查：`git diff --check` 无错误；基线差异没有构建产物、设计稿或无关文件；按任务书第 6 节的隐私正则扫描基线到 HEAD 的差异，未命中（退出码 1）。

## 新增的机制

- `display.wallpaper_file`：由任务书的壁纸持久化要求引入，沿用 settings 整型项和文件编号，没有新持久格式。
- 小增量服务能力 `member.rename`：由改名要求引入，沿用成员存储及 `member.changed`，界面不写策略内部状态。
- 复用 UI、MEDIA 执行器和 F1/F2/F4/F9，没有新增线程、重试、缓存框架或队列。

超过 100 行的原因：五页、成员输入/密码、异步活动查询和跨 MEDIA/UI 的图片生命周期都要接线。已共用 F4 呈现和 F2 输入；更小方案是继续塞进 `settings_pages.c`，但任务禁止修改它；只画静态页面则不能验证后端与重启。未另建通用框架。

## 要改的文档

- `docs/system/capabilities.md`：主会话重新生成，增加 16 项能力及 1 项设置；参考输出为 `build/settings-b-capabilities.md`，未提交共享文件。
- `docs/system/system-services-design.md` §1.3/成员节：补 `member.rename`，普通改名无需确认；添加/删除保留原确认规则。
- `docs/system/ui-dependencies.md`：更新五页实际状态，保留儿童应用/时段、记忆、运行记录和设计对齐缺口，不能标成全部完成。
