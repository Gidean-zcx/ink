# i18n-2b 执行报告

基线：`d6dbdce0a060429a7f4d4a58bee1057084d239e3`。仅本地编译和宿主机测试，不操作设备、不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 未通过，保留失败 | 【实测】补充测试 469 条、33 帧通过，缺词 0；原截图测试第三轮剩 3 项失败，按总规则停止继续修该测试 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility python3 tests/i18n_shell.py --build build --binary build/sim-i18n-shell-remaining --out build/test-out/i18n-shell-remaining` |
| 2a 普通全量 | 未通过 | 【实测】原话：`98% tests passed, 6 tests failed out of 349`，657.16 秒；3 项 hub 测试跳过。失败清单见下文；其中 `ui-scale`、`emergency-call`、`mobile-network` 修正后定向复验通过 | 见末尾全量命令 |
| 2b ASan | 待运行 | 配置通过，待构建、全量内存检查 | 见末尾命令 |
| 2c S31 | 待运行 | 待仅编译 | 见末尾命令 |
| 2d gcccheck / 隐私 | 进行中 | 首轮 18 文件、0 处；提交差异隐私扫描无匹配，最终复核待记录 | 见末尾命令 |
| 3 | 已列出；截图覆盖不完整 | 【实现】逐文件迁移数见下表；【实测】新增 33 帧和归档驱动 35 帧，未覆盖全部状态 | 见截图清单 |

## 没做到的

- `small-gaps-1` 的英文定位已修正，但英文标签被产品省略号截断，仍有两条按钮可见性断言失败。没有改断言或把省略号当作正确文案；问题与原截图测试的低电量页失败相同，依停止规则保留。
- 原 `i18n-shell` 第三轮仍有 3 项失败：`KidBlocked-en` 的输入密码按钮截断；`BatteryLow-en` 的省电控件重叠及截断；`FactoryReset-zh` 一次截图时显示仍忙。遵守总规则第 7 节，停止继续修该测试，不能报告验收通过。
- 新补测试覆盖 11 个文件的主要入口、AI 状态和全部更新页面，但备份恢复各阶段、传输成功/失败、成员密码等待等全部变体尚未逐页截图。服务提供的 ask 模板、动态业务正文按 i18n-2 的边界保留，待 i18n-4。

## 和设计不一致的

- 英文锁屏日期下移，避开紧急呼叫按钮；日期与时钟的中文位置也随之变化。未声明中文像素完全相同。
- 电源菜单的首排按标签实际高度安排下方按钮；AI 面板对话按钮按文字宽度加内边距与边框计算；字号不变。

## 改过的已有测试

- `small-gaps-1`：仅英文按钮定位词改为 `Battery Saver: lock after 1 min`；中文定位词和两条权限断言不变。
- `emergency-call`：英文阶段的动作标签匹配 `Open the emergency dialler`，中文仍匹配原文；动作 ID、权限及快照隔离断言不变。
- `mobile-network`：英文 APN 页的密码存在标记改为 `Filled`；仍断言只显示存在性。以上两处由本次全量复现旧中文期望失败后修改，编译数据库问题没有复现。
- 归档自带的 `i18n-shell`：点击发送 `PRESSED` / `CLICKED`，等待卡片关闭；键盘名称使用真实的 `Keyboard`；关闭电源菜单后再切成员；检查顶层实际显示的浮层，避免把下层被遮住的文字算作重叠。排版、期望文字、缺词和显示稳定断言保留。

## 要用户定的

- 暂无。

## 顺手发现

- 暂无。

## 新增的机制

- 无新增产品任务、状态机、文件格式、配置项或抽象层。
- 测试任务 `i18n-remaining`：任务书要求剩余页面英文截图与排版验证；只在测试程序启动，更新安装等状态仅为界面预览，未声称跑过更新业务流程。

## 要改的文档

- 合并时由主会话运行 `python3 tools/capabilities.py` 更新共享能力表。`search.c` 现在明确引用墙纸设置，生成表的 s39 设置引用从空变为 `display.wallpaper_file`；本任务按总规则第 7 节不改 `docs/system/capabilities.md`，因此 `capability-table-doc` 会报告一行过期。
- 任务总表应记为“迁移完成主要入口，验收未通过”，保留原截图测试和未覆盖的状态清单。

## 过程记录

- 已读取任务总规则和任务书；工作树初始干净，当前提交与指定基线一致，归档分支可在本地读取。

## 迁移清单

数量为相对本任务基线新增的不同 `TR` / `TR_NOOP` 字面量键，同文件去重；不包含宏展开的共享密码提示。前一任务已迁的文件仍列出，避免把归档工作重复算作本次新增。

| 文件（`system/ui/` 下） | 合计迁移键 | 本次新增键 | 英文截图 |
| --- | ---: | ---: | --- |
| `shell/activity_card.c` | 19 | 19 | `Tasks-en` |
| `shell/ai_chat.c` | 22 | 22 | `AiHome-en`、`AiAsk-en` |
| `shell/ai_pages.c` | 41 | 41 | `AiOffline-en`、`AiTimeout-en`、`AiBudget-en`、`AiLost-en`、`AiLog-en`、`AiReceipts-en` |
| `shell/ai_panel.c` | 12 | 12 | `AiThink-en`、`AiListen-en`、`AiRecording-en`、`AiAnswer-en` |
| `shell/backup_pages.c` | 44 | 44 | `Backup-en` |
| `shell/bluetooth_page.c` | 25 | 25 | `SetBluetooth-en` |
| `shell/confirm_card.c` | 38 | 38 | `ConfirmSetting-en` |
| `shell/member_pages.c` | 32 | 32 | `MemberSwitch-en`、`PinSet-en`、`PinForgot-en` |
| `shell/transfer_pages.c` | 36 | 36 | `Transfer-en`、`TransferUsbBusy-en` |
| `shell/update_pages.c` | 44 | 44 | `Update-en`、`Update-state-en`、`UpdateFail-state-en`、`UpdateReady-state-en`、`UpdateInstall-state-en`、`UpdateBlocked-en`、`UpdateDone-en`、`BootRecovery-en` |
| `shell/wallpaper.c` | 1 | 1 | `WallpaperSearch-en` |
| `search.c` | 16 | 16 | `SearchEmpty-en`、`WallpaperSearch-en` |
| `shell/airplane_card.c` | 11 | 0 | `AirplaneOn-en`、`Refused-en` |
| `shell/factory_pages.c` | 30 | 0 | `FactoryReset-en`、`FactoryDone-en` |
| `shell/interrupt_card.c` | 9 | 0 | 来电、闹钟卡未补截图 |
| `shell/keyboard.c` | 22 | 0 | `keyboard-en` |
| `shell/kid_pages.c` | 14 | 0 | `KidBlocked-en`（未通过） |
| `shell/member_gate.c` | 12 | 0 | `member_gate-en` |
| `shell/off_picture.c` | 5 | 0 | `off-en` |
| `shell/pinpad.c` | 3 | 0 | `pinpad-en` |
| `shell/power_menu.c` | 9 | 0 | `power_menu-en` |
| `shell/power_pages.c` | 23 | 0 | `BatteryLow-en`（未通过）、`Charging-en` |
| `shell/settings_pages.c` | 71 | 0 | `SetVoice-en-*`、`SetStorage-en-*`、`SetAbout-en-*`、`SetPrivacy-en-*`、`DeviceDetails-en-*`、`Licenses-en-*`、`LicenseText-en` |
| `file_picker.c` | 12 | 0 | `FilePicker-en-*` |
| `lock_mark.c` | 11 | 0 | `LockPage-en` |
| `textfield.c` | 8 | 0 | `textfield-menu-en` |
| `ui_list.c` | 1 | 0 | 列表截图内 |
| `ui_semantics.c` | 2 | 0 | 各页同名 `.json` |
| `app_registry.c` | 0 | 0 | 继承既有翻译，无新增固定界面文案 |

新增截图在 `build/test-out/i18n-shell-remaining/snap-<名称>.png`，同名 `.json` 为 AI 快照；后半表截图在 `build/test-out/i18n-shell/`。中文回切另有 `AiHome-zh`、`Tasks-zh`、`SetPrivacy-zh`、`FactoryReset-zh`。均未提交图片。

## 浮层证据

- 【实测】归档原始驱动：671 条通过、15 条失败；记录在 `build/test-out/i18n-shell-original/metrics.json`。拒绝卡遮住后续页面。
- 【实现】`theme.c` 的按钮注册 `LV_EVENT_CLICKED`，原驱动发送 `LV_EVENT_SHORT_CLICKED`，未触发关闭。修正事件并校准页面等待后，三轮末次降为 3 条失败，缺词增量为 0。
- 【实测】`LockPage-en` 原截图中紧急呼叫盖住日期；下移后该指定排版断言不再失败。电源菜单英文两行标签的高度修正后，指定排版断言也不再失败。
- 【实现】键盘宽度计算原本把两个方向符号也送进翻译表，产生 2 次缺词；与显示路径一致地直接测量符号后，缺词归零。

## 规模与架构审视

【实现】相对指定基线的产品代码（C 与两处 CMake，不含表、测试、文档）暂计 `+652/-535`，净增约 117 行，低于 400 行净增预算。归档导入与本次新增分开见表；最终准确数待收尾复核。超过 100 行是因为逐项迁移 28 个显示文件并注册构建检查；更小方案已采用同一源键表和已有布局测量，没有统一翻译任意字符串，以免误翻用户姓名、文件名、正文及服务模板。

能力 ID、权限、对象和调用路径不变，界面与 AI 仍走同一能力。只翻译固定文字和快照的人类标签；内部 AI 状态比较保留源语言，录音和播放状态判断不依赖译文。密码和一次性数字仍隐藏。

## 核查命令

普通和 ASan 配置使用任务总规则第 5 节的依赖参数；ASan 额外 `-DINK_SANITIZE=ON`，保留 `-DINK_WERROR=ON`。

```sh
taskpolicy -c utility cmake --build build -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" taskpolicy -c utility bash ports/esp32s31/build.sh
taskpolicy -c utility python3 ../../tools/gcccheck.py build <本次与导入的变更 C 文件>
```

## 普通全量结果

【实测】首次全量失败为 `ui-scale`、`small-gaps-1`、`i18n-shell`、`capability-table-doc`、`emergency-call`、`mobile-network`。前三类排版和文档差异见前文；紧急呼叫和手机网络均已找到编译数据库，实际失败来自英文阶段仍匹配旧中文标签。首次完整输出在 `build/verification/ctest-first.log`，后续 `ui-scale`、`emergency-call`、`mobile-network`、`i18n-shell-remaining` 四项复验原话：`100% tests passed out of 4`，8.97 秒，记录在 `build/verification/focused.log`。
