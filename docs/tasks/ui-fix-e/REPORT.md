# ui-fix-e 执行报告

基线：`4a4414da81ae6739f5cb174b8cb1e9b5aab2441a`。主要修复和截图已本地提交于 `bfd8b29a`。普通及 ASan/UBSan 全量均未全绿：唯一失败是范围外的 `autotest-build-switch`。未完成的审计余项在下文逐项列出。

全量记录对应 `bfd8b29a`；其后仅给文件错误提示加了 `F.field` 条件，避免编辑器尚未建立时误称文字保留。该收尾改动已分别重编普通和内存检查版本，并完成文件相关测试、120% 场景及两档专用 UI 检查。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| UX09（T32、T35～T39） | 【实测】主要交付通过；审计余项见下 | 所选家人名、只删除活动记录、连续时段输入、真实保存及家长确认、开关与通信例外 | `python tests/ui_fix_e/run.py --build build` |
| UX15（A11～A13） | 【实测】主要交付通过；A11 余项见下 | 空态无清空绑定；两行问题、时间和中文来源；底部语音和文字等高且不重叠 | 同上 |
| UX18（F01～F10） | 【实测】主要交付通过；审计余项见下 | 勾选和数量、无选择时不提供拷贝/移动、来源对象、空/错区分和重读、真实 gray4 缩略图、文件/草稿保存区分与失败保字 | 同上 |
| 常规及 120% 字号前后截图 | 【实测】82 张，每张 ≤40,288 字节 | [截图索引](screenshots/README.md)，覆盖 19 种基线状态及写入失败状态与保存后补图 | 同上，加 `--export docs/tasks/ui-fix-e/screenshots`；大字号加 `--large` |
| 针对性测试及反向对照 | 【实测】100% 和 120% 各 256 项，0 失败；恢复四个基线文件后 15 项预期失败；撤掉写入失败提示，两档各 2 项预期失败；撤掉编辑器保护条件，普通/ASan 各 1 项预期失败 | 测试输出分别明确含 UX09、UX15、UX18；基线仅手动模式读取，不注册进 ctest | 同上；反向加 `--baseline 4a4414da81ae6739f5cb174b8cb1e9b5aab2441a`；另跑 `tests/files_ops/run.py --reverse save-status/save-guard` |
| 普通全量测试（含 ui-scale） | 【实测】未全绿：`99% tests passed, 1 tests failed out of 409`，退出 8 | 1124.05 秒；405 通过、3 跳过；唯一失败为范围外 `autotest-build-switch`，相关 10 项和 `ui-scale` 均通过 | `taskpolicy -c utility ctest --test-dir build --output-on-failure -j2` |
| 内存检查 | 【实测】`99% tests passed, 1 tests failed out of 409`，退出 8；相关检查未见 ASan/UBSan 报错 | 1665.95 秒；405 通过、3 跳过；唯一失败同上；确认 `-fsanitize=address,undefined` 与 `INK_SANITIZE=ON` | `taskpolicy -c utility ctest --test-dir build-asan --output-on-failure -j2` |
| 收尾改动回归 | 【实测】普通和 ASan 的 `files-app/files-ops` 各 2/2 通过；ASan 专用 UI 两档各 256/0 | ASan 120% 文件操作：写入阶段 128 项、恢复阶段 333 项，0 失败；读取失败不误称保字 | `ctest --test-dir <build> -R '^files-(ops|app)$' --output-on-failure -j2`；专用脚本换 `--build build-asan` |
| gcccheck | 【实测】`gcccheck: 9 compiled, 0 with findings` | 四个产品文件、五个修改的已有测试 | `$INK_DEPS/../ink-codex/tools/gcccheck.py build <文件>` |
| S31 固件 | 没编固件；【实测】普通全量中的 S31 宿主测试通过 | 没有烧录或连接设备；固件链接与空间余量未验证 | 未运行 `ports/esp32s31/build.sh` |
| 范围、预算、隐私及截图链接 | 【实测】范围无夹带，269 行；规定隐私正则 0 匹配（grep 退出 1） | 4 个产品 C 文件 246 行变更，英文新增 23 行；83 个本地链接、82 张 684×1216 PNG 校验通过；成功专用测试临时目录已清理 | `git diff --stat`；按总规则第 6 节扫描基线到 HEAD |

## 没做到的

- T35/T36：沿用已有“正在用”和称呼外标签；没有重新分组管理操作或将角色轮换改成分段选择。
- T37：每日时长和音量仍是原有预设轮换，没有增建数值选择页。
- A11：输入框尺寸和候选条右侧语音键沿用已合入实现；没有补来源上下文标签。
- F01/F04：没有重新分组文件根页，也没有把原有改名输入另做编辑态。
- F02：加了易读大小；点文件仍先到现有操作页，没有改成直接打开。
- F05：补了移动/拷贝的来源文件名或数量；确认按钮仍沿用“放到这里/拷贝到这里”。
- F06：两排批量操作入口仍沿用原布局，没有增加“更多”菜单；部分失败逐项结果沿用既有实现，已由 `files-ops` 回归。
- F07：名称外标签和 `.txt` 后缀校验已由前置实现提供，本次保留。
- 不做后台记忆/日志服务、会话搜索、文件缓存、批处理引擎或持久格式变化；没有接设备。
- 全量测试的唯一失败和 3 项跳过如验收表所列；Hub 锁定的 Python 依赖未安装，对应测试按既有脚本返回 77，并未计为通过。

## 和设计不一致的

- T39 采用一个 `HH:MM-HH:MM` 的统一输入框连续编辑开始和结束，摘要用“次日”解释跨日；没有增加时间滚轮或日期选择组件。
- 多孩子记录选择仍沿用已有入口（默认首位孩子，能力可指定家人）；未新增选择层。所选家人的实际称呼用于页上和 AI 快照。

## 改过的已有测试

- `settings_b.c`：通信例外期望改成“始终可用”；两个既有 `%u` 参数显式转 `unsigned`，修复 gcccheck 暴露的格式告警。
- `privacy_records/device.c`：按实际家人称呼找分段按钮；删除按钮改用完整对象文案；仍校验 `kid` 目标参数和隔离。
- `kid_apps.c`：星期二按实际分页查找，保留密码确认和星期掩码断言；英文期望改为连续时段输入框的外标签。
- `ai_history_photo/test.c`：问题占两行后每页三条，UI 第二页期望从第 46 条改为第 47 条；服务的每页四条及顺序断言保留。
- `files_ops/files_ops.c`：选中态从下划线改为可见勾，仍检查唯一行边框、无额外选中框及真实数量；保存按钮改“更新文件”；增加失败回执注入，验证保字、键盘上方的错误提示和编辑器未建立时的真实表述。`run.py --reverse save-status/save-guard` 分别撤掉提示和保护条件。

## 要用户定的

- 没有阻断本次任务的新决定。上述审计余项和多孩子选择留给主会话安排，不当作已完成。

## 顺手发现

- 120% 长文编辑器的分页偏碎：合成 9 个字符显示 9 页，前后截图均如此（`file-edit`）。涉及共享输入组件，本任务没有改它。
- 首轮时段保存测试未先建立家长密码，服务拒绝落盘；补齐真实的合成密码前置条件后通过，没有放宽权限或断言。
- `autotest-build-switch` 的假 IDF 工程未创建 `__idf_ink_apps`，却调用 `apps/reader/reader_build.cmake:57` 向它加源文件；该测试和两个相关 CMake 文件与任务基线逐字相同。其 CMake 子进程另报权限拒绝。超出本任务范围，未改公共测试或阅读器构建。

## 新增的机制

- 无；不新增任务、状态机、格式、缓存或配置项。
- 缩略图仅保留当前可见行的 LVGL 像素缓冲；解码走既有 `file.thumb` 媒体执行器。文件选取器错误测试仅在临时测试对象里注入索引失败。

## 为什么不能更小

- 四个页面文件合计新增 187 行、删除 59 行（246 行变更）；连同 23 行新增英文词条共 269 行，未超过 300 行预算。只改文案无法满足连续编辑、开关、勾选、实际缩略图和失败重读；更大的新时间选择器、文件缓存与批处理方案均未采用。
- `family.kid_periods` 扩展 `edit=focus/save` 参数供按钮和 AI 共用，保存仍调用 `kid.period_set`；没有放宽既有 `family.focus/save` 的 SetMembers 页面限制。
- 保留应用的普通允许、禁用、禁用时段例外三种既有值，因此显示两个二态开关；行缓冲上限命名为 80，并静态断言容纳 40 个应用的两行。没有改服务或存储格式。

## 要改的文档

- 主会话可在任务清单登记本报告及审计余项；共享主题、生成的能力表、需求、想法、决策记录均未修改。

## 复核命令

以下 Python 使用 `$INK_DEPS/build-deps/pyenv/bin/python`，构建和测试均加 `taskpolicy -c utility`，并行数为 2。普通与 ASan 构建参数按任务总规则第 5 节，后者仅增加 `-DINK_SANITIZE=ON`。

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/ui_fix_e/run.py --build build
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/ui_fix_e/run.py --build build --large
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/ui_fix_e/run.py --build build --baseline 4a4414da81ae6739f5cb174b8cb1e9b5aab2441a
INK_KEEP_EVIDENCE=1 INK_FILES_OPS_LARGE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/files_ops/run.py --build build
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/files_ops/run.py --build build --reverse save-status
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/files_ops/run.py --build build --reverse save-guard
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

整体基线反向模式返回 0 表示三个 UX 均出现预期失败；`files-ops --reverse save-status` 返回 1，须核对只有上述两条失败；`--reverse save-guard` 应只有“编辑器未建立时不误称保字”一条失败。成功的 UI 专用测试默认清理临时目录，保留证据时加 `INK_KEEP_EVIDENCE=1`。
