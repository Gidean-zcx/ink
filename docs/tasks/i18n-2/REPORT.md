# i18n-2 执行报告

基线：`e65bfc261b68e9fb063fc60da507122119ae4fc6`。当前状态：按预算和三轮停止规则收尾，验收未通过。实现提交：`ecd6a57b`。预算内处理 16 个源文件；其余文件仍有未迁文案，未达到全覆盖。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 未通过 | 【实测】新测试生成 35 帧，671 条检查通过、15 条失败；末轮 `i18n.missing=2`，不能报告为 0。前 25 帧（设置至拒绝卡）无排版断言失败；键盘及后续浮层失败 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^i18n-shell$' --output-on-failure -j2` |
| 2a 原有及全量测试 | 未通过 | 【实测】原话：`99% tests passed, 4 tests failed out of 348`；580.79 秒；失败为 i18n-shell、small-gaps-1、emergency-call、mobile-network；另有 3 项 hub 测试标为 Skipped | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2` |
| 2b 内存检查 | 未通过 | 【实测】全量构建退出 0；原话：`99% tests passed, 4 tests failed out of 348`，670.49 秒，同 2a 四项失败、3 项跳过；完整输出未检出 ASan/UBSan 报错。已核对 17 个相关编译单元及测试链接的 sanitizer 参数 | `taskpolicy -c utility cmake --build build-asan -j2`；`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2` |
| 2c S31 | 通过 | 【实测】退出 0，镜像代码/数据 3,217,774 字节；`.rodata.I18N_EN` 位于 `0x4024d384`（Flash），长度 `0xfb8`；尺寸及栈报告在 `build-s31/` | `INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 2d gcccheck | 通过 | 【实测】`gcccheck: 17 compiled, 0 with findings`，退出 0 | 见核查命令 |
| 3 | 部分完成 | 【实现】下表逐文件列数与未迁范围；【实测】截图保留在 `build/test-out/i18n-shell/` | 同验收 1 |

## 迁移清单

计数为本次新增 TR/TR_NOOP 的不同中文键，同文件去重；不等于新增译文行数。

| 文件 | 新增键 | 状态 |
| --- | ---: | --- |
| `system/ui/shell/activity_card.c` | 0 | 预算不足，未迁 |
| `system/ui/shell/ai_chat.c` | 0 | 预算不足，未迁 |
| `system/ui/shell/ai_pages.c` | 0 | 预算不足，未迁 |
| `system/ui/shell/ai_panel.c` | 0 | 预算不足，未迁 |
| `system/ui/shell/airplane_card.c` | 11 | 已加标记；截图覆盖见下节 |
| `system/ui/shell/backup_pages.c` | 0 | 预算不足，未迁 |
| `system/ui/shell/bluetooth_page.c` | 0 | 预算不足，未迁 |
| `system/ui/shell/confirm_card.c` | 0 | 预算不足，未迁 |
| `system/ui/shell/factory_pages.c` | 30 | 已加标记；截图覆盖见下节 |
| `system/ui/shell/interrupt_card.c` | 9 | 已加标记；截图覆盖见下节 |
| `system/ui/shell/keyboard.c` | 22 | 候选和拼音按任务书保留；截图覆盖见下节 |
| `system/ui/shell/kid_pages.c` | 14 | 已加标记；截图覆盖见下节 |
| `system/ui/shell/member_gate.c` | 12 | 已加标记；截图覆盖见下节 |
| `system/ui/shell/member_pages.c` | 0 | 预算不足，未迁 |
| `system/ui/shell/off_picture.c` | 5 | 已加标记；截图覆盖见下节 |
| `system/ui/shell/pinpad.c` | 3 | 已加标记；截图覆盖见下节 |
| `system/ui/shell/power_menu.c` | 9 | 已加标记；截图覆盖见下节 |
| `system/ui/shell/power_pages.c` | 23 | 已加标记；截图覆盖见下节 |
| `system/ui/shell/settings_pages.c` | 71 | 已加标记；截图覆盖见下节 |
| `system/ui/shell/transfer_pages.c` | 0 | 预算不足，未迁 |
| `system/ui/shell/update_pages.c` | 0 | 预算不足，未迁 |
| `system/ui/shell/wallpaper.c` | 0 | 能力 title 按规则保留；注册的设置标题仍中文，未覆盖搜索中的显示 |
| `system/ui/app_registry.c` | 0 | 能力元数据/既有翻译，无新增 |
| `system/ui/file_picker.c` | 12 | 已加标记；截图覆盖见下节 |
| `system/ui/lock_mark.c` | 11 | 已加标记；截图覆盖见下节 |
| `system/ui/search.c` | 0 | 预算不足，未迁 |
| `system/ui/textfield.c` | 8 | 已加标记；截图覆盖见下节 |
| `system/ui/ui_list.c` | 1 | 已加标记；截图覆盖见下节 |
| `system/ui/ui_semantics.c` | 2 | 已加标记；截图覆盖见下节 |

## 英文截图

路径均相对当前检出，不提交图片。`build/test-out/i18n-shell/snap-<名称>.png`，同名 `.json` 为实际 AI 快照。

- 标签检查通过：`SetVoice-en-{1,2}`、`SetStorage-en-{1,2}`、`SetAbout-en-{1,2}`、`SetPrivacy-en-{1,2,3}`、`DeviceDetails-en-1`、`Licenses-en-{1,2,3}`、`LicenseText-en`、`FactoryReset-en`、`FactoryDone-en`、`FilePicker-en-{-1,0,1,2,3}`、`member_gate-en`、`pinpad-en`、`AirplaneOn-en`、`Refused-en`、`off-en`。
- 已生成但检查失败，不能作为验收通过证据：`keyboard-en`、`textfield-menu-en`、`power_menu-en`、`KidBlocked-en`、`BatteryLow-en`、`Charging-en`、`LockPage-en`；中文回切的 `SetPrivacy-zh`、`FactoryReset-zh` 也失败。
- 未截图：`interrupt_card` 的来电/闹钟卡、儿童超时卡、`BatterySaving` 和锁屏全部样式/状态、文字长编辑的保存状态，以及 11 个未迁页面文件和 wallpaper 设置标题。英文大字号、浮层已打开时原地切换语言未完成验证。

## 反向对照

【实测】`INK_KEEP_EVIDENCE=1 taskpolicy -c utility python3 tests/i18n_shell_reverse.py --build build` 退出 0：撤掉设置页标题翻译，新增 `SetVoice-en-1 has expected text: Voice & AI` 失败；撤掉许可版本行的高度定位，新增 `Licenses-en-3 layout` 失败。两项指定断言在正向记录中均未失败；整体截图测试仍有已知失败。源文件哈希未变，临时替换对象和逐条命令在 `build/i18n-shell-reverse/`。

## 没做到的

英文截图测试仍失败：键盘及后续浮层切换连续三轮修正后仍有问题，按规则停止继续调该测试；完整末轮记录在 `build/test-out/i18n-shell/metrics.json`，不能作为排版通过证据。受 300 行产品代码预算限制，尚未迁移 activity_card、ai_chat、ai_pages、ai_panel、backup_pages、bluetooth_page、confirm_card、member_pages、transfer_pages、update_pages、search；没有自行取消这些要求。本次运行平台为 macOS；Linux CI 未执行，GCC 验证使用本地 S31 工具链。

## 和设计不一致的

英文排版：语音分段用短译文 In use；清空设备页缩短行文；许可行按名称实际高度安排版本行，不缩字号。许可列表的中文界面也采用该行高计算，可能改变分页；未声明像素完全一致。清空设备页“继续”复用既有英文键 `Resume`，语境更合适的 `Continue` 尚未拆分。

## 改过的已有测试

无。全量发现 `small-gaps-1` 的英文分支仍在 `tests/small_gaps/pages.c:98` 用中文 `省电：1 分钟锁屏` 定位按钮；本次按钮正确翻成英文后，该成员权限断言失败 2 次。该既有测试未修改。

另两处相同的英文期望冲突：`tests/emergency_call/emergency_call.c:70` 仍要求“打开紧急呼叫页面”，`tests/mobile_network/mobile_network.c:121` 仍要求秘密字段值“有值”。实际英文分别是 `Open the emergency dialler`、`Filled`；能力 ID 与秘密隔离未变。这两处也未修改。

## 要用户定的

后续需要为尚未迁移的 11 个页面文件及 wallpaper 设置标题分配预算，并确定是否允许更新上节三处既有测试的英文期望（断言逻辑和权限要求不变）。

## 顺手发现

【实测】新截图驱动在键盘前仍被拒绝卡遮住，后续浮层检查也有叠层问题，尚未确认是驱动清理顺序还是产品原有行为；停止该路线，不报告为产品缺陷已定位。全量中的 `small-gaps-1` 英文定位词问题见上节。

## 规模与实现边界

【实现】当前产品代码 +290/-230 行（含两处 CMake，不含译文、测试和文档），未超过 300 行。超过 100 行的原因是 16 个文件逐项包裹固定文案；更小的方案已用现有显示时翻译及局部 action 副本，未采用全局翻译任意 label/value 的方案，以免把用户文件名、姓名、正文和服务模板当成译文键。英文表新增 181 键；既有 `使用时` 的英文缩为 `In use`。

## 新增的机制

无新增产品机制；仅复用 TR/TR_NOOP 和已有布局计算。测试任务 `i18n-shell-test`：任务书验收 1 要求的英文截图、标签尺寸检查及语言回切；只在测试程序中创建，产品不新增常驻任务。

## 要改的文档

主会话合并时在任务总表记明本项“部分完成，验收未通过”，保留尚未迁移的 11 个页面文件及 wallpaper 设置标题；未修改共享需求、决策、能力表或底层依赖文档。

## 范围和隐私检查

【实测】相对基线的 `git diff --stat` 为 23 个文件，均属任务范围；`git diff --check` 通过。按任务书第 6 节的隐私表达式扫描基线至提交差异：无匹配，grep 退出 1。构建、截图、原始日志和设计稿均未提交；工作仅在本地提交。

普通及内存检查的完整 ctest 输出分别在 `build/Testing/Temporary/LastTest.log`、`build-asan/Testing/Temporary/LastTest.log`。未检出 sanitizer 报错的扫描也覆盖内存检查的截图驱动 stderr。

## 核查命令

```sh
taskpolicy -c utility python3 ../../tools/gcccheck.py build \
  system/ui/{file_picker,lock_mark,textfield,ui_list,ui_semantics}.c \
  system/ui/shell/{airplane_card,factory_pages,interrupt_card,keyboard,kid_pages,member_gate,off_picture,pinpad,power_menu,power_pages,settings_pages}.c tests/i18n_shell.c
```

## 架构审视

【实现】能力 ID、参数、对象引用、权限及密码隔离保持原路径；按钮和 AI 仍调用同一能力。只翻译固定显示文案与快照的人类标签；用户姓名、文件名、正文、第三方许可名称/原文、服务提供的 ask/打断内容保留，不把它们送入译文查表。`ui_semantics` 的秘密字段只翻译“有值/无值”，不泄露输入内容。
