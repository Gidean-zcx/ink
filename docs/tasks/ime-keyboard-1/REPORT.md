# ime-keyboard-1 执行报告

基线：`b9ca75d74ab5c1839ebfae38fd1c664c93c4cac9`。仅本地开发和编译，不接触设备、不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1.1 英文直输与空格 | 【实测】通过 | 按真实控件得到 `niab c`，没有组合串 | C1 |
| 1.2 切回中文 | 【实测】通过 | `ni` 重新进入组合串并出现真实候选 | C1 |
| 1.3 未提交拼音 | 【实测】通过 | 点“中”后 `ni` 原样上屏且组合串清空 | C1 |
| 1.4 中英文标点 | 【实测】通过 | 中文 `，。？`，英文 `,.?`；符号页键面与输入一致 | C1 |
| 1.5 特殊输入框 | 【实测】通过 | 密码、网址、数字、电话没有模式键，保留原有输入过滤；密码拒绝 AI 操作并遮蔽内容 | C1 |
| 1.6 模式保持与默认值 | 【实测】通过 | 换普通框及经过特殊框后保持英文；每次新进程默认中文 | C1 |
| 1.7 AI 切换与快照 | 【实测】通过 | `keyboard.key(code=3)` 返回完成，快照同步显示 `输入=中文/英文` | C1 |
| 1.8 反向对照 | 【实测】通过 | 三处独立单点变异均触发指定失败，每次模拟器退出 1，对照脚本退出 0 | C2 |
| 2.1 普通版全量 | 【实测】部分通过 | 原话 `99% tests passed, 3 tests failed out of 375`，1013.41 秒；3 项跳过。更新旧期望和夹具后，最终文字输入重跑原话 `100% tests passed, 0 tests failed out of 3`，7.03 秒；余下两项基线失败见下文 | C3、C4；`work/ime-keyboard-1/ctest.log`、`text-tests-final.log` |
| 2.2 ASan + UBSan 全量 | 【实测】内存检查未见错误，全量部分通过 | 完整构建退出 0；原话 `99% tests passed, 2 tests failed out of 375`，1182.57 秒，退出 8；失败仍为清单文档和备份，3 项依赖跳过。本任务 531 次检查全部通过；完整 CTest 日志未出现 ASan/UBSan 错误标记 | C3，目录改为 `build-asan`；`work/ime-keyboard-1/asan-ctest.log`、`sanitizer-findings.log`、`build-asan/Testing/Temporary/LastTest.log` |
| 2.3 S31 | 【实测】通过 | 完整构建、资源打包、size/stack 报告退出 0；两份修改的产品 C 均编入；镜像 3302304 B；产品 C 代码 0 警告，另有 5 条 SDK CMake 配置警告及 1 条 shell 补全警告 | C5；`work/ime-keyboard-1/s31.log`、`build-s31/export.log` |
| 2.4 gcccheck | 【实测】通过 | `gcccheck: 4 compiled, 0 with findings`，无跳过 | C6 |
| 2.5 ui-scale | 【实测】通过 | `ui-scale: 51 files, 0 violations` | C7 |
| 2.6 能力清单 | 【实测】运行时通过，文档未通过 | 生成文件检查通过：`Runtime matches: 452 capabilities, 34 settings`；运行时及策略 ctest 通过；提交的共享文档在基线已过期 | C8 |
| 3 截图与图标 | 【实测】通过 | 中/英文界面各三张 684×1216 图已逐张目视核对；键盘文字、图标无裁切、遮挡，几何断言通过；图标保留读屏标签 | C1；`build/test-out/ime-keyboard1-current/` |

`ime-keyboard1` 共 531 次检查，0 失败；既有 `semantics-shell2-keyboard` 同样通过。配置参数与总规则 §5 一致，所有构建/测试限制为 `-j2`，ASan 只更换目录并增加 `-DINK_SANITIZE=ON`。

## 没做到的

普通版与 ASan 全量没有全部通过，剩余问题均已核查并记录：

- `capability-table-doc`：基线文档仍为 448 项能力/事件、31 项设置，代码已有 452、34。本次生成清单与基线逐字相同（两份生成结果 SHA-256 均为 `27d55cdd74fbdba3712fbe9f3d936eda91f37a7c00aad73aa827cc0419b3b6f7`）；规则禁止修改这份共享文档。复核：`taskpolicy -c utility "$PY" -B work/ime-keyboard-1/baseline_catalogue.py`；证据 `work/ime-keyboard-1/capability-baseline.txt`。
- `small-gaps-3-backup`：在 `tests/small_gaps_3/main.c:57` 首先失败，共 16 个失败。用基线的 `keyboard.c`、`text_service.c` 重新编译并链接原测试后，同样 16 个失败、首个失败及其虚拟时刻一致；不修改任务外备份代码。命令和证据：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" -B work/ime-keyboard-1/baseline_backup.py`、`work/ime-keyboard-1/baseline-backup.log`。
- `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` 因给定 Python 环境缺中枢测试依赖按原脚本跳过，未算作通过。未连接设备，未做实机或泄漏检查；内存检查指任务书规定的 ASan + UBSan 全量。

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

同文件 706 行：二进制夹具的 `.magic = "IKD1"` 和 `.text = "dede"` 改为逐字节初始化。原来两个数组都只有 4 字节，GCC 15 报 `unterminated-string-initialization`，挡住必需的 gcccheck；改后字节、布局和校验内容不变。修前 gcccheck 退出 1、报告两处错误，修后四个文件全部通过；未禁用告警。证据：`work/ime-keyboard-1/gcccheck-before-fixture.log`、`gcccheck-final.log`。

## 要用户定的

无。

## 顺手发现

备份基线失败、能力文档过期和旧二进制测试夹具告警均见上文；除阻挡 gcccheck 的夹具写法外未扩大修复范围。

## 新增的机制

中英模式：任务书第 1 条要求，一个内存布尔值，换框保留、进程启动默认中文。切换使用现有 `keyboard.key` 的 `code=3`；标点映射由文字服务集中提供。无新增产品任务、线程、状态机、文件格式、持久配置、重试、缓存或队列。

## 要改的文档

合并主会话需重新生成 `docs/system/capabilities.md`，修复基线已有过期内容；本任务不新增能力或设置，`code=3` 在已有能力输入 schema 和语义快照中说明。

## 阶段记录

- 实现与定向验收完成：仅修改文字服务、公共输入头和键盘；未改词库、资源打包器、硬件、共享需求文档。
- 反向对照（临时副本，每次仅改一处）：英文仍走拼音、切换丢拼音、标点忽略模式，各产生 2 个断言失败，模拟器退出码均为 1；对照脚本均确认命中指定断言，退出码为 0。恢复正常实现后 531 次检查通过。
- 六张截图：`snap-ImePinyin-zh.png`、`snap-ImeEnglish-zh.png`、`snap-ImeSymbols-zh.png`、`snap-ImePinyin-en.png`、`snap-ImeEnglish-en.png`、`snap-ImeSymbols-en.png`。中文/英文输入均使用同一模式键和标点入口；图标按 `INK_ICON_MEDIUM` 与主题间距缩放，并保留读屏文字和语义标签。
- 架构审视：【实现】模式与标点归文字服务，界面只显示和调能力；按钮和 AI 均走 `keyboard.key`；模式、图标文字和切换键值在快照可读；密码内容继续遮蔽且 AI 不能操作。
- 规模：【实测】产品代码新增 52、删除 12，净增 40 行；另有 5 行 CMake 注册，不超过 200 行预算。
- 收尾：【实测】`git diff --check` 通过；相对基线的 diff 仅 8 个相关文件，未夹带产物、设计稿或共享文档修改；按总规则 §6 正则查隐私，0 匹配。全部修改仅本地提交，未推送。

## 复核命令

以下从仓库根目录运行，`PY="$INK_DEPS/build-deps/pyenv/bin/python"`、`CTEST="$INK_DEPS/build-deps/pyenv/bin/ctest"`，构建配置按任务书。

```sh
# C1：保留本任务截图及断言证据
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$CTEST" --test-dir build --output-on-failure -j2 -R 'ime-keyboard1|semantics-shell2-keyboard'
# C2：三处反向对照，只在构建目录生成副本，不依赖 Git 历史
taskpolicy -c utility "$PY" -B tests/ime_keyboard1.py --build build --reverse
# C3：全量，ASan 对应 build-asan，ASan 运行时未设置 INK_KEEP_EVIDENCE
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$CTEST" --test-dir build --output-on-failure -j2
taskpolicy -c utility "$CTEST" --test-dir build-asan --output-on-failure -j2
# C4：中文标点旧期望更新后的既有回归
taskpolicy -c utility "$CTEST" --test-dir build --output-on-failure -j2 -R '^integration-text-(main|nocard|baddict)$'
# C5：只编译
INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" INK_S31_LIGHT_SLEEP=OFF taskpolicy -c utility bash ports/esp32s31/build.sh
# C6
taskpolicy -c utility "$PY" -B ../../tools/gcccheck.py build system/services/text/text_service.c system/ui/shell/keyboard.c tests/ime_keyboard1.c tests/integration/textinput.c
# C7
taskpolicy -c utility "$PY" -B tools/ui_scale_check.py
# C8：不改共享文档；另用 ctest 的 capability-table 系列检查运行时和策略
taskpolicy -c utility "$PY" -B tools/capabilities.py --output work/ime-keyboard-1/capabilities.generated.md
taskpolicy -c utility "$PY" -B tools/capabilities.py --output work/ime-keyboard-1/capabilities.generated.md --check --runtime build/test-out/capability-table/capability-inventory.json
```
