# i18n-4 执行报告

本报告的实测均为模拟器或编译检查。

基线：`4eabca66395ff1da6610526d79e1c3dfde9cc98b`。工作分支：`claude/codex-i18n-4`。实现提交：`550a1603`。

产品代码新增 292 行、删除 256 行（按基线 diff 统计，包含构建文件，不含测试、文档和译文表），预算 300 行。为什么不能更小：需要覆盖 31 个服务、策略文件的既有文字；已将确认模板、设置回执和无线传输集中翻译，未增加产品机制。农历改用英文数字日期，日历仅加一处短日期适配，字号不变。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 服务、策略英文及中文回切、排版、缺译计数 | 部分通过；未覆盖全部状态页面 | 【实测】代表性页面 9 张 English、1 张中文截图，布局检查通过，`95 pass / 0 fail / i18n.missing=0`；范围外 UI 仍有中文，见下文 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R i18n --output-on-failure -j2` |
| 2a 原有测试及全量 | 通过 | 【实测】`100% tests passed out of 348`，401.44 秒，无跳过；中枢契约包含 `147 passed` | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2` |
| 2b 内存检查 | 通过 | 【实测】独立 ASan/UBSan 全量 `100% tests passed out of 348`，953.53 秒，无跳过 | 同配置加 `-DINK_SANITIZE=ON`，在 `build-asan` 执行全量 ctest，`-j2` |
| 2c S31 | 通过 | 【实测】构建退出 0；镜像 3,219,954 字节；`.rodata.I18N_EN` 在 flash，只读表 5,096 字节 | `INK_S31_LIGHT_SLEEP=OFF taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 2d gcccheck | 通过 | 【实测】`gcccheck: 35 compiled, 0 with findings` | `taskpolicy -c utility python3 ../../tools/gcccheck.py build $(git diff --name-only 4eabca66..HEAD -- '*.c')` |
| 3 文件迁移计数、保留中文原因、英文截图 | 已记录，详见下文 | 【实现】31 个源文件；英文表由 322 条增至 637 条 | 译文生成器及下表 |

| 文件 | 新迁移键数（文件内去重） |
| --- | ---: |
| `system/policy/cap.c` | 2 |
| `system/policy/confirm.c` | 5 |
| `system/policy/members.c` | 10 |
| `system/policy/settings.c` | 4 |
| `system/policy/system_actions.c` | 14 |
| `system/services/ai/ai_service.c` | 20 |
| `system/services/audio/audio_service.c` | 15 |
| `system/services/audio/media_service.c` | 4 |
| `system/services/backup/backup_service.c` | 19 |
| `system/services/bt/bt_service.c` | 2 |
| `system/services/camera/camera_service.c` | 1 |
| `system/services/comm/comm_service.c` | 31 |
| `system/services/data/data_caps.c` | 1 |
| `system/services/display/light.c` | 2 |
| `system/services/file/file_caps.c` | 5 |
| `system/services/file/file_service.c` | 8 |
| `system/services/file/transfer.c` | 26 |
| `system/services/hub/pairing.c` | 13 |
| `system/services/log/log.c` | 4 |
| `system/services/net/net_service.c` | 7 |
| `system/services/net/wifi.c` | 4 |
| `system/services/notify/notify_service.c` | 25 |
| `system/services/power/power_service.c` | 8 |
| `system/services/res/task.c` | 1 |
| `system/services/text/text_caps.c` | 7 |
| `system/services/text/text_service.c` | 1 |
| `system/services/text/voice_input.c` | 11 |
| `system/services/time/lunar.c` | 36 |
| `system/services/time/time_caps.c` | 4 |
| `system/services/time/time_service.c` | 3 |
| `system/services/update/update_service.c` | 18 |

计数为新增的字面量键；无线传文件为经集中翻译入口处理的 26 个键。确认卡显示标题另在显示时翻译，能力表原文不改。`datastore.c`、`interrupt.c`、`notify_caps.c` 无需新迁移；`pinyin.c` 的汉字是输入内容，保留。其它服务文件没有待迁移中文。

故意保留：能力表、数据库和状态的 AI 元数据；短信的内部判断值“收到”；已有目录标识；可编辑的家人名字；拼音字典和离线识别的中文原话。它们不是系统显示文案，翻译会改变内容或判断。无线传输保留原结果供内部比较，HTTP 回复和状态输出各在原入口调用 `TR()`。已有英文错误详情保留原样。农历的中文月份、日期字样仅用于中文分支，英文走整句数字模板。

反向对照：【实测】`sim-i18n-services-negative` 仅关闭确认模块翻译，返回 1，`93 pass / 2 fail`，准确检出确认卡标题、说明和设置模板未翻译；正常版本 `95 pass / 0 fail / i18n.missing=0`。printf 与具名占位符集合均已校验。命令：`taskpolicy -c utility cmake --build build --target sim-i18n-services-negative -j2`，再运行 `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/i18n_services.py --build build --negative`。

构建使用总规则的字体、Python 和 LVGL 参数，普通与内存检查均启用 `INK_WERROR=ON`；另外选用已有的 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`。测试前设置 `PYTHONDONTWRITEBYTECODE=1`、`PYTHONPATH="$PWD/hub/src"`，使三个中枢测试实际运行；未安装或升级依赖。所有构建及全量测试顺序执行，均为 utility 优先级，最多 `-j2`。S31 环境按 `ports/esp32s31/README.md`，`INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus"`。

English 模式截图（生成并保留的实际帧；其中界面自身的遗留中文不属于本次迁移）：

```text
build/test-out/sim-i18n-services/snap-ConfirmCall-en.png
build/test-out/sim-i18n-services/snap-ConfirmSetting-en.png
build/test-out/sim-i18n-services/snap-Notifications-en.png
build/test-out/sim-i18n-services/snap-Channels-en.png
build/test-out/sim-i18n-services/snap-Channels2-en.png
build/test-out/sim-i18n-services/snap-Battery-en.png
build/test-out/sim-i18n-services/snap-IncomingCall-en.png
build/test-out/sim-i18n-services/snap-ActiveCall-en.png
build/test-out/sim-i18n-services/snap-Alarm-en.png
```

中文回切帧：`build/test-out/sim-i18n-services/snap-Notifications-zh.png`。英文低电量通知缩短为 `%d%%. Charge now`，在原字号和行宽内完整显示；其它验证帧未改字号或布局。

命令日志保留于 `work/ctest-final.log`、`work/asan-ctest.log`、`work/s31-build.log`、`work/gcccheck.log`、`work/negative.log` 和 `work/i18n-services-final.log`。构建产物及截图未提交。收尾已核对基线差异统计；隐私模式扫描无匹配（grep 退出 1）。

## 没做到的

验收 1 未全部满足：截图中确认卡标题、拒接、闹钟电源键说明、低电量专页和公历格式仍来自范围外的 UI 文件；更新、备份等全部状态页面尚无逐页英文截图。受任务限定的服务、策略范围及 300 行产品代码预算约束，保留这些界面缺口供主会话合并界面任务后补验；未据此删减产品功能。服务文案的代表性路径已验证，未逐条触发所有错误分支。

## 和设计不一致的

【实现】低电量弹层由 `system/ui/shell/power_pages.c` 重新生成标题和正文，覆盖服务提供的文字；该界面本身仍是中文，属于界面迁移范围。本任务验证服务通过 `interrupt.list` 提供英文，保留截图并明确此边界。

## 改过的已有测试

- `tests/log/log.c`：独立日志测试增加中文直通的 `ink_tr` 桩；未改断言或期望。
- `tests/unit/lunar_test.c`：独立农历算术测试增加中文语言和翻译桩，原有日期、生肖、干支期望不变。
- `tests/i18n_tables.py`：数组结束符按独占一行的 `};` 识别。无线传文件 HTML 内的 JavaScript 也含 `};`，原解析误截断为 15 条；未改数量、排序或坏表断言。

## 要用户定的

暂无。已存通知按生成时的语言保留，语言切换后新通知使用新语言。

## 顺手发现

【实测】首次全量测试的源码片段检查发现 `settings.c` 新增头文件使文档行号落到空行；将该头文件放进原有空行，保留原行号，检查恢复通过。

【实现】范围外的 `system/include/ink/sys/ai.h:19` 仍含长回答截断提示，须另迁移。

【实现】确认卡、来电、闹钟等周边 UI 还有自行生成的中文（如拒接、确认卡标题、闹钟电源键说明）；服务提供的文案已经迁移，完整界面英文须由界面任务合并。

## 新增的机制

无产品机制；沿用现有 `TR()` / `TR_NOOP()` 和语言设置。新增的反向对照编译开关只用于测试。模块边界、原子能力和权限规则保持原样，按钮和 AI 仍调用相同能力。

## 要改的文档

主会话合并时更新任务进度，并汇总界面迁移的剩余中文。共享文档未改。
