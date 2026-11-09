# boot-flow 执行报告

基线：`fa7c18803f8f0eefc44db39acc2da053c7aa9386`。依据[任务书](../boot-flow.md)与[总规则](../codex-rules.md)。2026-11-09 从 `7472e8c7` 接续复查修复；`REVIEW.md` 尚无问题条目，按用户补充补齐原缺项第 1 条。只在本地分支提交，不 push。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 首次开机与接续通过【实测】；D-99 内容仍缺 | 从空闪存、无配对和无已存成员的真正首启自动进入 FirstBoot，走到 BootHello；两种场景各留下 11 屏截图。另以同一闪存连续启动 14 次验证各步接续、密码草稿不恢复、退出欢迎页后不再进入向导、吊销后重新设置 | T1；T6；`tests/boot_flow_check.py`、`tests/boot_flow_reboot.py` |
| 2 | 通过【实测】 | 错密码后重输、换网；无服务器后先用；拒绝邀请后重新加入；无备份当新设备用、有备份经本人确认实际恢复；吊销后重新进入选网 | T1；`tests/boot_flow.c` |
| 3 | 通过【实测】 | 逐屏及输入后的页面、键盘下层 AI 上下文和 text.state 不含密码；AI 插入、提交密码和确认加入均被拒；遍历输出日志检查密码、热点载荷和令牌 | T1、T3 |
| 4 | 通过【实测】 | 普通和 ASan/UBSan 均 217/217，无跳过；产品及产品构建新增 368 行、删除 6 行，合计 374/400。含重启场景的内部 RAM 最低余量 43,736 B，高于原 32,768 B 门槛；测试源码和 CMake 测试配置按总规则 §7 不计入产品预算 | T1、T2、T5 |
| G1 | 通过【实测】 | 原断言和预算保留；含确定性、分层、能力目录、恢复与配对检查 | T1、T2 |
| G2 | 通过【实测／实现】 | 11 页使用 F1 语义快照；按钮与 AI 共用 shell.launch、wifi、pairing、restore 既有能力；密码与加入确认保留本人限制 | T1、T3；`system/ui/shell/boot_flow.c` |
| G3 | 按总规则 §7 留给主会话 | 共享文档未改，拟更新内容在“要改的文档” | T5 |
| G4 | 已对照，11 屏均超目标【实测】 | 已看原画板与实际截图；按现行主题、真实服务实现，无动画；逐屏差分和缺项见下文，不宣称视觉验收通过 | T4 |
| G5 | 符合边界【实现】 | 主要修改 UI、F2 与测试；服务增补 SYSTEM 二维码回执、2 行认领通知，以及成员已有配置标志的只读查询；配对业务状态和转换保持不变 | T3、T5 |

命令用已有构建配置：总规则 §5 的字体、Python、LVGL 参数，另复用 `$INK_DEPS/build/_deps/` 中的 SDL2、ImGui 和 `$INK_DEPS/hub/.venv/bin/python`。两套均为 `INK_WERROR=ON`，内存目录另设 `INK_SANITIZE=ON`。两套最终构建与全量逐轮串行，最多 `-j3` / `-j2`，反向对照最终单独运行，均使用 utility 优先级；GUI 冒烟使用 SDL dummy。

- T1：`taskpolicy -c utility cmake --build build -j3`；`SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`。均退出 0，217/217，206.66 秒；无失败、跳过或未运行项，能力表文档同步检查也通过，未使用用户允许的豁免。日志 `work/boot-flow/recheck-build.log`、`recheck-full-final.log`。单独复现场景可加 `-R '^boot-flow(-reboot)?$'`。
- T2：T1 换为 `build-asan`，其已有配置包含 `-DINK_SANITIZE=ON`。构建、全量测试均退出 0，217/217，283.20 秒；没有跳过测试或非预期 sanitizer 诊断，`LastTest.log` 中无 sanitizer／UB 报错。日志 `work/boot-flow/recheck-build-asan.log`、`recheck-full-asan.log`。
- T3：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/boot_flow_reverse.py --build build --asan-build build-asan`。最终退出 0：两项正向探针通过；九项反向对照全部失败于预期原因。原五项分别撤掉重试文案、F2 密码支持、SYSTEM 二维码回执、pairing.changed 订阅、动作 id 持有副本，最后一项触发预期 ASan `stack-use-after-scope`（退出 -6）。新增四项撤掉自动入口、进度写入、同状态认领通知、确认信息就绪检查，重启测试分别在空设备入口、选网接续、认领页接续、本人确认后完成处失败（退出 1）。只在构建目录编译替换对象，源码哈希及正常二进制保持不变。证据 `build/boot-reverse/summary.json`、`commands.json`，日志 `work/boot-flow/recheck-reverse-final.log`。首次误用旧 ASan 库时缺新接口符号，未计为通过；重建后上述完整复跑通过。
- T4：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/boot-compare --reuse --scenes FirstBoot BootWifi BootWifiPass BootJoining BootJoinFail BootClaim BootNoHub BootConfirm BootRestore BootHello BootRevoked`。退出 0；分数见 `build/boot-compare/scores.json`，对照图 `build/boot-compare/<页面>.png`，本轮日志 `work/boot-flow/recheck-compare.log`。原始截图在 `build/test-out/boot-flow-{fresh,restore}/snap-<页面>.png`；内存检查有同名目录。
- T5：`git diff --stat <基线>..HEAD`、`git diff --check <基线>..HEAD`；相对任务基线共 27 个相关文件，均小于 5 MB，两个报告链接有效。按总规则 §6 的原始表达式扫描差异，0 命中。预算计数为产品源码增删 373 行，加 CMake 产品源文件列表 1 行；另 44 行 CMake 测试目标／场景配置属于测试。证据 `work/boot-flow/recheck-checks.json`、`recheck-final-checks.log`。
- T6：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/boot_flow_reboot.py --build build`；内存目录换 `build-asan`。两套均包含于对应全量；每套 14 次实际进程启动合计 181 项检查通过，共用同一模拟闪存，逐步保存至欢迎页、退出、重启、吊销后重启；包含连接过程中断后回密码页。日志和闪存留在各目录的 `test-out/boot-reboot-sim-boot-flow/`。

**为什么不能更小【实现】**：11 页共用一个 create、动作声明、快照和回执处理，页面文件现为 338 行，其余产品及产品构建变更合计 36 行（含删除）。本轮复用现有数据存储记录页面和网络，成员服务只公开已有“已配置／读失败”标志，避免 UI 重读成员私有表。看过普通输入框和只监听 pairing.state 的方案，分别不能满足 F2 密码保护、同一配对状态内的变化通知（T3 可复现）。没有拆出 11 套布局或另建配对机制。

## 没做到的

- BootHello 到达与书架入口已接；D-99 的离线示例正文、就地问 AI 教程、已有家庭回到上次阅读位置未做。当前页显示尚未打开阅读内容的提示，没有把测试书塞进产品。
- 未实测 Linux/gcc、真机、真实手机网页或真实家庭服务器。所有身份、网络和备份均为本机模拟器合成夹具。

## 和设计不一致的

11 屏均超过 12/255 的目标。共同差异：画板仍为旧字号、旧灰度和示例状态，产品使用现行放大主题与真实状态栏；除此之外还有下列布局与内容差异，未修改设计稿或比较阈值迁就实现。

| 页面 | 1/4 灰度平均差 | 本次可见差异 |
| --- | ---: | --- |
| FirstBoot | 40.547/255 | 实际热点二维码与画板载荷不同；缺稿中的二维码圆角外框，“在设备上设置”用整行胶囊 |
| BootWifi | 21.189/255 | 本次只扫到一个合成网络；用独立胶囊显示，没有稿中的分组行与信号图标 |
| BootWifiPass | 29.293/255 | 复用 F2 的真实密码键盘；未补“显示”按钮和键盘顶部说明行 |
| BootJoining | 14.998/255 | 共用居中标题、状态文案；未画稿中的等待点，取消按钮位置不同 |
| BootJoinFail | 20.874/255 | 标题为通用连接失败，密码错误写在真实回执说明；图标尺寸、灰度按主题 |
| BootClaim | 48.138/255 | 实际加入链接二维码、确认码；没有稿中的二维码外框及十分钟说明，多了返回选网按钮 |
| BootNoHub | 24.524/255 | 图标、字号、位置按共用布局；文案说明手机或存储卡可设置服务器 |
| BootConfirm | 24.132/255 | 标题为“加入这个家庭？”，邀请人、家庭名放说明行；未显示稿中虚构的内容数量 |
| BootRestore | 22.492/255 | 此分数取 fresh 的无备份空状态；restore 场景另有来自真实恢复预览的日期、项目数、覆盖说明和“放回来”按钮截图 |
| BootHello | 34.201/255 | 缺 D-99 示例正文，当前为提示和打开书架入口，不能算阅读教程完成 |
| BootRevoked | 31.483/255 | 不承诺本地资料一定保留，显示以服务器处理为准；图标、字号、布局按主题 |

## 改过的已有测试

- 本轮 `boot-flow` 改从真正首启开始，删去“网络设置 → 设置设备”的前置导航；新增 14 次启动的接续场景和 4 项反向对照。
- 既有主页／阅读场景使用 `tests/configured_boot.c` 提供“设置已完成”的合成进度记录；CMake 改接这个测试入口，原物理脚本、断言和性能预算保持原样。产品命令行与空闪存首启测试仍使用正常启动入口。
- 7 个既有全系统测试在启动后显式跳过设置、关闭热点并回主页；`whole_product.h` 的准备步骤记录与物理 HOME 键一致的活动。`ui-cards` 在计数期间通过既有电源占用保持活跃，排除闲置状态栏刷新，结束时释放；原零绘制／提交断言保留。
- GUI dummy 冒烟在原点击前增加一次 HOME 盖板键点击，以离开首次设置。没有删除、跳过测试或放宽已有断言、预算、超时；截图的空闲检查保持原样。

## 要用户定的

- 暂无新增产品决定。D-99 仍按原记录，未用当前替代页改变它。

## 顺手发现

- 暂无本次确认的范围外问题。

## 新增的机制

- 11 个页面及已有服务事件到页面的映射：任务书明确要求，只有界面状态，业务配对仍由 pairing 服务负责。
- LVGL 内置二维码的编译开关：FirstBoot、BootClaim 显示实际码所需，未引入外部资源或新二维码框架。
- F2 短输入宿主接受 `PASSWORD + secret + ai_hidden` 的组合：任务书指定，编辑与提交沿用输入服务的本人检查。
- 开机设置进度记录：用户本轮明确要求跨重启保留。沿用数据存储的集合／记录格式，以记录排序字段保存页面、JSON 保存所选网络，不保存密码草稿、认领链接、令牌或确认结果。
- 新常驻任务、线程、独立持久文件格式、后台重试、缓存、队列和产品配置项：无。

## 要改的文档

- `docs/system/ui-dependencies.md` §4.3、§7 的开机／配对行：更新为“页面、自动首启及跨重启接续已在模拟器连接；D-99 内容仍缺”。D-99 示例内容行不能标成已完成。
- `docs/tasks/backlog.md` boot-flow：记入本报告的功能结果、视觉超标和缺项；由主会话统一改，本分支不动共享文档。
- 复用现有能力，没有新增能力 id；生成的能力总表无需新增条目。

## 里程碑

- 开始：已核对分支与基线，工作区干净；读取任务规则和任务书，准备检查画板及现有接口。
- 实现：11 屏共用布局、F2 密码支持、服务事件连接及合成场景已完成；中断前留下的全量结果与截图保留。
- 接续核验：重新阅读代码与画板，五项反向对照符合预期；普通与内存全量均 216/216 再次通过。
- 收尾：实现与测试提交为 `f8e3c15d`；完成差异、预算、隐私及报告链接检查。功能、设计缺项与模拟器证据边界如实保留，报告另行本地提交。
- 复查修复：自动首启、进度接续和对应回归完成；最终普通与内存全量均 217/217，九项反向对照命中，D-99 内容缺项保留。

## 复查后的修复

- 【实现／实测】原“没做到的”第 1 条已补齐，第 2 条继续保留。空设备自动进入 FirstBoot；现有数据存储保存页面和所选网络，重复的进度写入会跳过。连接中断后回到所选网络的密码页，由本人重新输入；确认与恢复重新查询服务，退出欢迎页才记为设置结束。
- 【实现／实测】重启后，同一配对状态重新取得认领码时缺少通知，页面会停在 BootNoHub：在原状态已为 pairing 时补发已有 `pairing.changed`（服务增加 2 行），首次转入 pairing 不重复发；确认页等待当前有效的认领信息，再提供本人确认按钮。
- 【实现／实测】自动接管限定在系统主页的启动，不接管使用通用 UI 框架的其他根页面。限制前 `integration-lifecycle` 原有“所有调用槽位都归还”检查失败，限制后定向及两套全量通过，前后证据在 `recheck-full.log`、`recheck-final-targeted.log` 和最终全量日志。
- 【实测】曾发现旧场景依赖“启动即主页”；额外 HOME 动作会污染精确按键计数和延迟统计，最终改用已配置设备夹具，原脚本恢复。卡片测试中的额外绘制来自 5 秒闲置切换的状态栏，按上述测试准备处理，保留原断言。
- 【实测】最终普通全量 217/217（206.66 秒），ASan/UBSan 全量 217/217（283.20 秒）；两套均无失败、跳过、未运行项和缺依赖。九项反向对照全部命中，两套启动场景的内部 RAM 最低余量为 43,736 B。预算、文件大小、链接、空白和隐私核查通过；本轮只提交相关文件，开工前未跟踪的 `REVIEW.md` 保持原样。
