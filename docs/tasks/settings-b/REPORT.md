# settings-b 执行报告

基线：`558c7ba8604fc321073b943716f49512a7dc3690`。分支：`claude/codex-settings-b`。仅本地提交，未推送、未碰设备。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 部分完成 | 【实测】五页可打开；成员添加、改名、确认删除、密码、儿童音量和壁纸连接真实后端。跨进程重启验证成员名、删除状态、密码、音量、壁纸及活动记录。应用/时段、运行记录和记忆缺口见下文 | C1 |
| 2 | 通过 | 【实测】儿童通过 AI 和直接打开路径均进不了五页；去掉页面检查，5 条断言失败，测试仍运行结束。反例未修改产品源码 | C1、C2 |
| 3 | 通过复查规格 | 【实测】F9 选择合成 PNG，经已有解码、抖动后用于第三种锁屏；这一页、时钟、壁纸三种截图独立，壁纸无时钟、比例不变、主页全图不变，重启后可重新加载 | C1 |
| 4 | 通过已实现部分 | 【实测】五页快照可放入真实 AI 上下文；按钮和 AI 复用能力。密码沿用共用 `PinSet` 的遮蔽及仅人可提交路径；设置密码后成员页仍可读。恢复旧认证代次检查，3 条断言失败 | C1、C2 |
| 5 | 普通完成，内存检查进行中 | 【实测】普通全量 239 项：237 通过，只有指定的两项共享文档失败，0 跳过；专项 404+20 条通过。ASan/UBSan 正在重新配置和编译。本轮产品代码净减 18 行 | C1、C3 |

## 复查后的修复

本轮基线 `f9ca3924`，按主会话复查 SB-M1～M4、SB-L1 执行。开工先按任务参数重新配置、编译。普通全量最终只有指定的两项共享文档失败；ASan/UBSan 进行中，结果完成后补入。

| 编号 | 修复 | 测试与反向对照 |
| --- | --- | --- |
| SB-M1 | 两个密码入口都走 `shell.pin_set`，删除重复表单及三项 `family.*pin*` 能力 | 【实测】点击成员列表和编辑表单进入共用 `PinSet`；旧能力不存在。恢复旧入口，两条检查失败。改密码本身由已有 `kid-1` 验证 |
| SB-M2 | 家人卡片进入 `SetMembers`；锁屏行显示自动锁屏时间并打开 `Wallpaper`；删除页尾重复入口；在 `SetVoice` 接通记忆和记录两行 | 【实测】从真实控件依次进入四页。恢复设置入口，七条检查失败；撤回语音页接线，两条失败。旧总览测试改验 600 秒对应“10 分钟” |
| SB-M3 | 三种锁屏独立选择；壁纸整屏按比例覆盖，不画时钟；读不到图片退回当前页标记；主页不铺图 | 【实测】三种锁屏各留截图；检查壁纸对象比例、覆盖范围和无时钟标签；选择前后主页截图逐字节一致。分别撤回锁屏逻辑、覆盖方式和主页删除，检查均失败 |
| SB-M4 | 每次取六条，沿用 `activity.list` 的 `cursor` 和 F4 翻页读取更早记录，不改 128-token 上限 | 【实测】通过真实能力生成 25 条可辨认记录，按钮和 AI 翻页遍历全部 25 条，不出现读取错误；恢复一次 20 条，两条检查失败 |
| SB-L1 | 声明移回头文件保护内；改名直接注册进 `MEMBER_CAPS`；孩子页只列已有 `allow` 中的阅读、AI、主页 | 【实测】重复包含头文件编译；成员初始化后改名可用；页面内容对应策略。撤回保护导致编译错误，撤回注册或策略文字分别导致三条、一条检查失败 |

【实测】12 组反向对照全部得到预期失败，只编译构建目录中的替换对象或头文件，不改产品源码；保留儿童隔离和认证代次的原有反例。证据、命令和源码哈希在 `build/settings-b-reverse/`。主页反例由 Python 的全图一致断言拒绝，C 检查仍正常结束；头文件反例在编译时被拒绝，其余反例均记录 C 检查失败并运行结束。

本轮产品代码 **+72 / -90，净减 18 行**，符合净增 150 行预算；以前次 334 行为口径，累计为 316 行，仍低于原任务 350 行。复用现有能力、游标和整屏刷新，没有新增任务、线程、状态机、格式、配置项或队列。按本次明确复查意见，仅改 `settings_pages.c` 中的两行入口，未扩展其他页面或后端。

原来三项中枢测试跳过的根因已查明：任务指定 Python 缺 `pytest`、`pytest_asyncio`、`aiohttp`、`yaml`（包名 PyYAML）。本分支 `work/settings-b-hub-venv/` 沿用 Python 3.11.15，按 `hub/uv.lock` 固定版本和分发哈希安装依赖，用本地路径加载本分支 `hub`。没有修改只读依赖目录、锁文件或依赖版本。普通和 ASan 配置共同追加 `-DINK_HUB_PYTHON="$PWD/work/settings-b-hub-venv/bin/python"`；该临时环境未提交。

构建采用任务书的完整 CMake 参数；ASan 仅增加 `-DINK_SANITIZE=ON`。构建和测试均以 `taskpolicy -c utility` 启动，构建 `-j2`、CTest `-j2`。下面命令在仓库根目录执行：

```sh
# C1：最终专项，分别验证写入和独立进程重启
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -R '^settings-b$' -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -R '^settings-b$' -j2
# C2：五项复查修复及儿童隔离、认证代次的反向对照
 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/settings_b_reverse.py --build build
# C3：全量，当前均因两项文档检查退出 8
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
# C4：生产页面截图，再调用 tools/ui_compare.py --reuse
 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/settings_b_compare.py --build build --design "$INK_DESIGN"
```

【实测】普通全量证据：`build/settings-b-review-full-final.log`，两项文档失败使 CTest 退出 8；内存检查证据写入 `build-asan/settings-b-review-full.log`，待运行结束核对。普通专项写入 404 条、独立重启 20 条通过，最低内部 RAM 余量 **47,000 字节**，预算仍为 32,768。反例源码哈希核对一致。截图在 `build/test-out/settings-b-sim-settings-b/write/`：`snap-{LockPage,LockClock,LockImage,LockFallback,HomeBefore,HomeAfter,WallpaperSelected}.png`；25 条记录最后一页为 `Records25.json`。

## 没做到的

- 【实现】基线儿童策略只有 `members.c:set_allow` 固定白名单、应用 `child_ok` 和 `audio.child_max`，没有可修改的应用白名单、每日时长或禁用时段接口。`SetKid` 读取已有策略并修改音量；应用与时段编辑未完成，本轮未扩展儿童策略后端。
- 【实现】设备和 `hub/` 没有记忆列举/删除接口，`AiMemory` 如实显示不可用，未伪造条目或删除。
- 【实现】`Records` 每批读取六条并沿游标查看更早记录；运行记录、跨成员查看和删除尚未实现。后端也未提供稳定活动记录编号，跨批只向更早方向翻，重新打开页面回到最新记录。
- 【实测】普通全量保留失败：`capability-table-doc`（能力/设置文档未同步）、`services-mid-capability-doc`（服务文档缺 `member.rename`），按用户明确指示留给主会话同步，没有改测试放行。原来缺依赖的三个 `hub-*` 已通过，0 跳过。本机为 Apple clang，未实测 Linux/gcc 或真机。
- 【实现】壁纸保存稳定文件编号；重启依赖原图仍可读取，没有新增壁纸副本格式。

## 和设计不一致的

【实测】C4 的五页 1/4 灰度平均差全部超过 12/255；证据在 `build/settings-b-compare/`：

| 页 | 分数 | 差别 |
| --- | ---: | --- |
| SetMembers | 49.042 | 独立分页行，缺头像和连续分组；添加/改名使用现有输入框，密码入口明确是自己的密码 |
| SetKid | 49.966 | 实际策略和音量，缺画板的应用开关、时长和时段选择 |
| Wallpaper | 61.845 | 选图、锁屏样式、自动锁屏已接入，缺三种预览、壁纸缩略图和通知内容开关 |
| Records | 42.545 | 显示真实 AI 活动条目，缺成员分段、分类计数和全部删除 |
| AiMemory | 28.311 | 不可用状态，没有画板的记忆条目和忘掉按钮 |

本轮重新运行 C4；字号、控件和灰度采用当前主题，未为分数回退规格。共用文件按复查意见修改入口、注册和三种锁屏，未扩展页面版式或调整画板。

## 改过的已有测试

本轮按复查要求更新以下已有期望，没有放宽预算或跳过测试：

- `tests/settings_b.c`：删除重复密码表单的输入、保存、密码重启断言，替换为两处真实点击都进入 `PinSet`，保留 `kid-1` 的设置、修改、隔离、遮蔽和独立重启验证；成员名、删除、音量、壁纸与活动记录的原重启断言保留。
- `tests/settings_b.py`：撤回“主页也显示壁纸”的旧期望，改为选择前后主页全图逐字节一致；一张壁纸锁屏改成三种锁屏的独立截图和壁纸可见性检查。
- `tests/integration/ai_surface.c`：仅移除已经删除的 `family.pin`、`family.old_pin`、`family.pin_save` 三项合同，继续验证共用 `shell.pin_set` 和仅人可提交的 `shell.pin_submit`。
- `tests/ui_shell_regressions.c`：两个设备档案的旧断言原来要求总览显示锁屏样式，按 SB-M2 改为写入 600 秒后显示“10 分钟”；没有删掉该检查。
- `tests/settings_b_reverse.py`：保留两个原反例，扩充复查修复的撤回检查和头文件编译反例。

保留的能力合同：

- AI 可调用：`shell.family`、`shell.kid_settings`、`shell.wallpaper`、`shell.records`、`shell.ai_memory`、`family.edit`、`family.focus`、`family.save`、`family.role`、`family.prev`、`family.next`、`wallpaper.choose`、`member.rename`。`family.edit` 的 `id` 可选，`member.rename` 必须给 `id,name`，其他无参数。
- 密码入口复用 `shell.pin_set`，凭据输入和提交沿用 `kid-1` 的仅人可操作路径。

## 要用户定的

无新增决策。`SetBluetooth` 本次不实现，既有蓝牙能力保留。

## 顺手发现

无任务外产品修复。前次的新页曾在设置密码后因认证代次变化丢失快照，现仍用已有页面归属检查；重复密码页已按 SB-M1 删除，密码流程统一归 `PinSet`。

【实测】结束检查：`git diff --check` 无错误；基线差异没有构建产物、设计稿或无关文件；按任务书第 6 节的隐私正则扫描基线到 HEAD 的差异，未命中（退出码 1）。

## 新增的机制

本轮没有新增机制，以下两项均为前次实现，本轮保留：

- `display.wallpaper_file`：由任务书的壁纸持久化要求引入，沿用 settings 整型项和文件编号，没有新持久格式。
- 小增量服务能力 `member.rename`：由改名要求引入，沿用成员存储及 `member.changed`，界面不写策略内部状态。
- 复用 UI、MEDIA 执行器和 F1/F2/F4/F9，没有新增线程、重试、缓存框架或队列。

超过 100 行的原因：五页、成员输入/密码、异步活动查询和跨 MEDIA/UI 的图片生命周期都要接线。已共用 F4 呈现和 F2 输入；更小方案是继续塞进 `settings_pages.c`，但任务禁止修改它；只画静态页面则不能验证后端与重启。未另建通用框架。

## 要改的文档

- `docs/system/capabilities.md`：主会话重新生成能力表，保留本任务 13 项能力和壁纸设置，移除三项重复密码能力，并更新锁屏选项；未提交共享文件。
- `docs/system/system-services-design.md` §1.3/成员节：补 `member.rename`，普通改名无需确认；添加/删除保留原确认规则。
- `docs/system/ui-dependencies.md`：更新五页实际状态，保留儿童应用/时段、记忆、运行记录和设计对齐缺口，不能标成全部完成。
