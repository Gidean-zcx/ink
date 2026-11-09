# boot-flow 执行报告

基线：`fa7c18803f8f0eefc44db39acc2da053c7aa9386`。2026-11-09 中断后接续，保留原实现与测试，未重新开发。只在本地分支提交，不 push。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 场景通过【实测】；有设计缺项 | 从网络设置的“设置设备”进入 FirstBoot，选网、本人输入、加入、认领、拒绝后再确认至 BootHello；fresh 173 项、restore 181 项检查通过，每种场景均留下 11 屏截图。自动首启与示例正文见下文 | T1；`tests/boot_flow_check.py` |
| 2 | 通过【实测】 | 错密码后重输、换网；无服务器后先用；拒绝邀请后重新加入；无备份当新设备用、有备份经本人确认实际恢复；吊销后重新进入选网 | T1；`tests/boot_flow.c` |
| 3 | 通过【实测】 | 逐屏及输入后的页面、键盘下层 AI 上下文和 text.state 不含密码；AI 插入、提交密码和确认加入均被拒；遍历输出日志检查密码、热点载荷和令牌 | T1、T3 |
| 4 | 通过【实测】 | 普通、ASan/UBSan 均为 216/216，分别 143.64、220.34 秒；新增产品及构建代码 321 行、删除 6 行，按增删合计也是 327/400；内部 RAM 最低余量 45,784 B，高于原 32,768 B 门槛 | T1、T2、T5 |
| G1 | 通过【实测】 | 原有测试、预算不变；含确定性、分层、能力目录、恢复与配对检查 | T1、T2 |
| G2 | 通过【实测／实现】 | 11 页使用 F1 语义快照；按钮与 AI 共用 shell.launch、wifi、pairing、restore 既有能力；密码与加入确认保留本人限制 | T1、T3；`system/ui/shell/boot_flow.c` |
| G3 | 按总规则 §7 留给主会话 | 共享文档未改，拟更新内容在“要改的文档” | T5 |
| G4 | 已对照，11 屏均超目标【实测】 | 已看原画板与实际截图；按现行主题、真实服务实现，无动画；逐屏差分和缺项见下文，不宣称视觉验收通过 | T4 |
| G5 | 符合边界【实现】 | 主要修改 UI、F2 与测试；服务只增补一行，允许系统 UI 获取热点二维码；未改配对状态机 | T3、T5 |

命令用已有构建配置：总规则 §5 的字体、Python、LVGL 参数，另复用 `$INK_DEPS/build/_deps/` 中的 SDL2、ImGui 和 `$INK_DEPS/hub/.venv/bin/python`。两套均为 `INK_WERROR=ON`，内存目录另设 `INK_SANITIZE=ON`。构建、全量测试逐轮串行，最多 `-j3` / `-j2`，使用 utility 优先级；GUI 冒烟使用 SDL dummy。

- T1：`taskpolicy -c utility cmake --build build -j3`；`SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`。均退出 0；日志 `work/boot-flow/resume-build.log`、`resume-full.log`。单独复现新场景可在 CTest 命令末尾加 `-R '^boot-flow$'`。
- T2：T1 换为 `build-asan`，其已有配置包含 `-DINK_SANITIZE=ON`。构建、全量测试均退出 0，216/216，220.34 秒；没有跳过测试或非预期 sanitizer 诊断。日志 `work/boot-flow/resume-build-asan.log`、`resume-full-asan.log`。
- T3：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/boot_flow_reverse.py --build build --asan-build build-asan`。退出 0；两项正向探针退出 0；撤掉重试文案更新、F2 密码支持、SYSTEM 热点二维码回执、pairing.changed 订阅后分别出现 1、4、1、1 项失败，退出 1；撤掉动作 id 的持有副本触发 ASan `stack-use-after-scope`，退出 -6。只在构建目录编译替换对象，源码哈希及正常二进制保持不变。证据 `build/boot-reverse/summary.json`、`commands.json`，日志 `work/boot-flow/resume-reverse.log`。
- T4：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/boot-compare --reuse --scenes FirstBoot BootWifi BootWifiPass BootJoining BootJoinFail BootClaim BootNoHub BootConfirm BootRestore BootHello BootRevoked`。退出 0；分数见 `build/boot-compare/scores.json`，对照图 `build/boot-compare/<页面>.png`。原始截图在 `build/test-out/boot-flow-{fresh,restore}/snap-<页面>.png`；内存检查有同名目录。
- T5：`git diff --stat <基线>..HEAD`、`git diff --check <基线>..HEAD`；总规则 §6 隐私扫描、文件大小与报告本地链接检查，待最终提交后核对。

**为什么不能更小【实现】**：11 页共用一个 create、动作声明、快照和回执处理，页面文件 299 行；其余产品及构建变更 22 行。看过直接沿用普通输入框的更小方案，不能满足任务指定的 F2 与密码保护；只监听 pairing.state 的方案会漏掉同一状态中的邀请变化（T3 可复现）。没有拆出 11 套布局或另建配对机制。

## 没做到的

- 未接“启动即自动进入 FirstBoot”及跨重启的向导位置；当前入口是网络设置“设置设备”，或 `shell.launch(id=boot)`。测试从此入口走完，不能算整机首次启动自动接管。
- BootHello 到达与书架入口已接；D-99 的离线示例正文、就地问 AI 教程、已有家庭回到上次阅读位置未做。当前页显示尚未打开阅读内容的提示，没有把测试书塞进产品。
- 未实测 Linux/gcc、真机、真实手机网页或真实家庭服务器。所有身份、网络和备份均为本机模拟器合成夹具。

## 和设计不一致的

11 屏均超过 12/255 的目标。共同差异：画板仍为旧字号、旧灰度和示例状态，产品使用现行放大主题与真实状态栏；除此之外还有下列布局与内容差异，未修改设计稿或比较阈值迁就实现。

| 页面 | 1/4 灰度平均差 | 本次可见差异 |
| --- | ---: | --- |
| FirstBoot | 40.338/255 | 实际热点二维码与画板载荷不同；缺稿中的二维码圆角外框，“在设备上设置”用整行胶囊 |
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

- 没有修改已有测试、期望或预算。新增 `boot-flow` 场景与复现脚本；既有测试全量照跑。

## 要用户定的

- 暂无新增产品决定。D-99 仍按原记录，未用当前替代页改变它。

## 顺手发现

- 暂无本次确认的范围外问题。

## 新增的机制

- 11 个页面及已有服务事件到页面的映射：任务书明确要求，只有界面状态，业务配对仍由 pairing 服务负责。
- LVGL 内置二维码的编译开关：FirstBoot、BootClaim 显示实际码所需，未引入外部资源或新二维码框架。
- F2 短输入宿主接受 `PASSWORD + secret + ai_hidden` 的组合：任务书指定，编辑与提交沿用输入服务的本人检查。
- 新常驻任务、线程、持久格式、后台重试、缓存、队列和配置项：无。

## 要改的文档

- `docs/system/ui-dependencies.md` §4.3、§7 的开机／配对行：更新为“页面与既有配对、热点、恢复服务已在模拟器连接；首启自动进入及 D-99 内容仍缺”。D-99 示例内容行不能标成已完成。
- `docs/tasks/backlog.md` boot-flow：记入本报告的功能结果、视觉超标和缺项；由主会话统一改，本分支不动共享文档。
- 复用现有能力，没有新增能力 id；生成的能力总表无需新增条目。

## 里程碑

- 开始：已核对分支与基线，工作区干净；读取任务规则和任务书，准备检查画板及现有接口。
- 实现：11 屏共用布局、F2 密码支持、服务事件连接及合成场景已完成；中断前留下的全量结果与截图保留。
- 接续核验：重新阅读代码与画板，五项反向对照符合预期；普通与内存全量均 216/216 再次通过。最终隐私与工作区检查待补。
