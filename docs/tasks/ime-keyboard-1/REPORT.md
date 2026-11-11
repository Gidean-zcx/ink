# ime-keyboard-1 执行报告

基线：`b9ca75d74ab5c1839ebfae38fd1c664c93c4cac9`。仅本地开发和编译，不接触设备、不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | `ime-keyboard1` 531 次检查，0 失败；现有 `semantics-shell2-keyboard` 通过；三处单点反向变异均检出预期失败 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R '^(ime-keyboard1\|semantics-shell2-keyboard)$'`；`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B tests/ime_keyboard1.py --build build --reverse` |
| 2 | 【实测】部分完成，继续验证 | 普通版首轮原话 `99% tests passed, 3 tests failed out of 375`，1013.41 秒；3 项中枢依赖测试跳过。中文标点旧期望已更新，文字输入三项重跑全部通过；清单过期及备份失败另列下文。gcccheck 为 `3 compiled, 0 with findings`，ui-scale 通过 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；`-R '^integration-text-(main\|nocard\|baddict)$'` |
| 3 | 【实测】通过 | 六张 684×1216 模拟屏截图已逐张目视核对；键盘文字、图标无裁切、遮挡，布局边界断言通过 | 同 `ime-keyboard1`，证据在 `build/test-out/ime-keyboard1-current/` |

## 没做到的

ASan、S31 尚在执行。能力清单文档检查已发现基线过期（452 项能力/事件、34 项设置，文档还是 448、31）；本次生成清单与基线逐字相同，不修改规则禁止改的共享文档。`small-gaps-3-backup` 在自动备份设置点按后失败，正在用基线源码独立核查。

## 和设计不一致的

【实现】保留现有符号页的符号集合和一页布局；规范 §7.5 的两页与设计稿的 `1/2` 键是原有缺口，本次未增加翻页。保留可读的“空格”文字（稿中为空白），其它布局沿用现有键盘。中文逗号、句号及 `?!:;()` 映射为全角，其余符号保持原样，不加引号配对机制。

## 改过的已有测试

仅更新 `tests/integration/textinput.c` 的 10 处中文模式字符串期望，原因都是任务书要求逗号、句号用全角。未改条件、预算、撤销步数、光标位置或提交次数。

| 行 | 原期望 → 新期望 |
| --- | --- |
| 106 | `西安中国abc,` → `西安中国abc，`；同时把提示改为中文逗号提交拼音 |
| 112 | `西安中国abc,xyz` → `西安中国abc，xyz` |
| 114 | `西安中国abc,xyz` → `西安中国abc，xyz` |
| 343 | `开头ab,` → `开头ab，` |
| 348 | `开头ab,` → `开头ab，` |
| 354 | `开头ab,` → `开头ab，` |
| 359 | `开头ab,.` → `开头ab，。` |
| 365 | `开头ab,.` → `开头ab，。` |
| 368 | `ab,.` → `ab，。` |
| 370 | `开头ab,.` → `开头ab，。` |

## 要用户定的

无。

## 顺手发现

无。

## 新增的机制

中英模式：任务书第 1 条要求，一个内存布尔值，换框保留、进程启动默认中文。切换使用现有 `keyboard.key` 的 `code=3`；标点映射由文字服务集中提供。无新增产品任务、线程、状态机、文件格式、持久配置、重试、缓存或队列。

## 要改的文档

合并主会话需重新生成 `docs/system/capabilities.md`，修复基线已有过期内容；本任务不新增能力或设置，`code=3` 在已有能力输入 schema 和语义快照中说明。

## 阶段记录

- 实现与定向验收完成：仅修改文字服务、公共输入头和键盘；未改词库、资源打包器、硬件、共享需求文档。
- 反向对照（临时副本，每次仅改一处）：英文仍走拼音、切换丢拼音、标点忽略模式，各产生 2 个断言失败，模拟器退出码均为 1；对照脚本均确认命中指定断言，退出码为 0。恢复正常实现后 531 次检查通过。
- 六张截图：`snap-ImePinyin-zh.png`、`snap-ImeEnglish-zh.png`、`snap-ImeSymbols-zh.png`、`snap-ImePinyin-en.png`、`snap-ImeEnglish-en.png`、`snap-ImeSymbols-en.png`。中文/英文输入均使用同一模式键和标点入口；图标按 `INK_ICON_MEDIUM` 与主题间距缩放，并保留读屏文字和语义标签。
- 架构审视：【实现】模式与标点归文字服务，界面只显示和调能力；按钮和 AI 均走 `keyboard.key`；模式、图标文字和切换键值在快照可读；密码内容继续遮蔽且 AI 不能操作。
