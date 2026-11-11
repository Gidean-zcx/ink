# reader-more-1 执行报告

基线：`d9ece7ad488d0a5ec74ed289c1c03e63719d538d`。功能、定向回归、内存检查及 S31 验证完成；普通全量 **417/419 通过，2 项未过**，原因列于下表。只在本地任务分支提交；未连接设备、未推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 收件箱书架、续读和成员隔离 | 通过【实测】 | reader-more 三阶段通过；EPUB/TXT、排除项及两个家人 | `ctest --test-dir build -R "^reader-more$" --output-on-failure` |
| 2 原厂书签模型、操作和重排定位 | 通过【实测】 | 256 条、768 字节、半开区间模型测试及整机重排跳转通过；来源和适配见 [SOURCE.md](../../../apps/reader/vendor/metalio_bookmarks/SOURCE.md) | `ctest --test-dir build -R "reader-(more|bookmark-model)" --output-on-failure` |
| 3 书签隔离、重启和 AI 一致性 | 通过【实测】 | 当前页标记、菜单增删、列表删除、按成员和重启保存 | 同上 |
| 4 反向对照和原有阅读回归 | 通过【实测】 | 定向回归：`100% tests passed out of 24`；本任务三个临时对象对照退出均为 1，分别失败 1、2、1 条断言 | 下列回归命令；包含原有 reader-settings*、reader-epub*，断言和预算未放宽 |
| 5.1 普通全量 | 未全绿【实测】 | 原话：`99% tests passed, 2 tests failed out of 419`，1298.73 秒，0 跳过。仅 capability-table-doc 与 settings-b 未过 | `PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2` |
| 5.2 内存检查 | 通过【实测】 | ASan + UBSan：`100% tests passed out of 24`，61.90 秒；包括临时对象对照和分段测试 | 相同阅读回归集合，目录换为 `build-asan`，加 `-DINK_SANITIZE=ON` |
| 5.3 GCC | 通过【实测】 | `gcccheck: 6 compiled, 0 with findings`，含 C++ 胶水 | `python "$INK_DEPS/../ink-codex/tools/gcccheck.py" build apps/reader/reader.c apps/reader/bookmarks.cc apps/reader/epub_source.cc tests/reader_more/device.c tests/reader_more/model.cc tests/integration/ai_surface.c` |
| 6.1 S31 宿主机 | 通过【实测】 | `100% tests passed out of 75`，153.17 秒 | `cmake -S ports/esp32s31/tests -B build-s31-tests`；构建和 ctest 均为 utility、`-j2` |
| 6.2 S31 固件 | 通过【实测】 | bin **4,907,616 字节**；5 MiB 分区余 **335,264 字节（6.39%）**。编译器告警 0；SDK CMake 提示 6 条，数量与首轮相同 | `INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus" taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 7 常规及 120% 截图 | 通过【实测】 | 六张 clean 帧；最大 50,243 字节；逐张检查，120% 删除按钮完整 | `INK_KEEP_EVIDENCE=1 ctest --test-dir build -R '^reader-more$' --output-on-failure` |

定向及内存检查使用同一集合；`fixture-aged-user-image` 由 ctest 自动加入，共 24 项：

```sh
READ_RE='^(reader-more.*|reader-bookmark-model|reader-settings.*|reader-epub.*|reader-continue|scenario-reader_.*|determinism-reader|integration-ai-surface|sdk-boundary|i18n-literals)$'
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2 -R "$READ_RE"
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2 -R "$READ_RE"
```

## 没做到的

- 没有设备实测，不能用模拟器替代残影、真实定位耗时和空闲内存核对。
- 没有在 Linux 主机运行全量；本次使用 macOS clang、交叉 GCC 严格检查和 S31 GCC 构建。
- 沿用基线 TXT 的 2 MiB 上限及 UTF-8 正文读取路径，没有扩展编码或 EPUB 图片解码。
- 普通全量未全绿：共享能力目录按总规则留给合并会话更新；`settings-b` 的临时能力注册超过系统上限，未修改该测试或抬高上限。两项处理材料见“要用户定的”和“要改的文档”。

## 和设计不一致的

原厂模型头保持原文；持久化按任务要求改为成员数据集合，移除原厂 BKM1 文件格式；ID 改由成员数据库分配，避免不同书籍编号相撞。其余范围与上限逻辑保留，具体差异见 SOURCE.md。

## 改过的已有测试

只在 `tests/integration/ai_surface.c` 登记四个新能力及参数要求；没有修改已有断言或预算。

## 要用户定的

【实测／实现】本次四个能力使注册数由 485 变为 489；`settings-b` 还注册 25 个临时能力，合计 **514 > 512**。出处：`system/policy/cap.c:30,45`、`tests/settings_b.c:113`。正式测试在注册处失败，后续产生 36 条失败断言。

主会话需决定是否调整全局能力容量，或另行处理测试夹具的容量占用。本任务保留 512 上限和原有测试。已提供 [capacity_control.py](capacity_control.py)：只在临时对象中移除这四个注册，原测试的 write/restart 阶段通过 **410/20** 条断言，证明容量是触发条件；该结果不替代正式测试结论。复现命令：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" docs/tasks/reader-more-1/capacity_control.py --build build
```

## 顺手发现

【实现】S31 原配置关闭全局 C++ 异常；ESP-IDF 将抛出异常接到终止桩。原厂 catch 保留并不代表设备上能从内存耗尽中恢复，本轮没有改 SDK 配置或测试此情形。出处：SDK `components/cxx/CMakeLists.txt:21`；本项目 `ports/esp32s31/sdkconfig.defaults` 未开启异常。

## 新增的机制

- 成员数据集合 `reader.bookmarks`：任务第 2 条要求，复用现有 JSON 记录和持久化服务，按书籍路径筛选。
- 书签列表页和四个能力：任务第 2、3 条要求，复用系统分页、能力调用、页面快照和已有跳转路径。
- 收件箱沿用现有进度文件判断“新”；没有新增配置项、常驻任务、状态机、缓存、后台重试或持久文件格式。

## 为什么不能更小

收件箱必须沿用已有打开和进度路径，书签必须接到成员数据库、原厂源位置、分页列表、菜单与四个 AI 能力。只加页码标签会在重排后定位错误；直接使用原厂存储会引入新的文件格式。产品代码新增 **314 行**（含 CMake、翻译、AI 登记和原文适配的 8 行；不以删除行抵扣），低于 400 行。

## 要改的文档

按[总规则第 7 条](../codex-rules.md)的“这些共享文档不改，合并时由主会话统一改”，没有直接改 `docs/system/capabilities.md`。完整生成差异在 [CAPABILITIES.patch](CAPABILITIES.patch)，已通过 `git apply --check`。补丁还补上基线缺少的排版三能力、五设置和状态关联。

本任务新增 `reader.bookmarks`（可选 open）、`reader.add_bookmark`、`reader.remove_bookmark`（可选 id，默认当前页）、`reader.goto_bookmark`（必选 id）。成员集合 `reader.bookmarks` 已在启动时注册，AI 可读，通过专用能力修改。

## 最终核查

- 【实测】路径范围、`git diff --check`、报告链接、原厂文件哈希、六张 PNG 和单文件大小检查通过；没有构建产物、设计稿、依赖升级或无关文件入库。
- 【实测】基线至提交的隐私正则检查无命中；补丁和手动对照脚本也已检查。生成文档补丁通过 `git apply --check`，未应用到共享文档。
- 【实测】GCC 检查 6 个 C/C++ 源码为 0 问题；另外对原厂书签实现去掉 `-w`、使用交叉 G++ 的 `-Wall -Wextra -Wshadow -Werror -fsyntax-only` 检查通过。
- 【实测】初轮内存对照因新源码链接旧对象失败，完整重建后 24/24；S31 首轮缺少原厂语言头，定位为源文件属性的 CMake 目录作用域，绑定应用目标后同一构建命令成功。没有跳过测试或放宽断言。
- 构建和 ctest 最大均为 `-j2`、utility；全量只运行既有无窗口 GUI smoke；没有开启窗口模拟器、访问设备或外发操作。

## 上机核对清单

1. 从手机或电脑分别传入 TXT、EPUB，当前家人书架显示“新”；非书、目录及传输中的 .part 不显示；回书架、续读和目录、阅读设置可用。
2. 在 EPUB 第二章中段及 TXT 第二个目录位置加书签，确认页角标记、时间、章名和摘录；点击列表返回原句，菜单和列表都能删。
3. 连续改变字号和行距，书签仍回原句；核对定位耗时、内部 RAM/PSRAM 及刷新残影。
4. 切换两个家人，收件箱和同一共享书的书签各自独立；重启后进度与书签保留。
5. 常规与 120% 系统字号核对三行条目和“删除”按钮，没有裁字或重叠。

截图：[书架 100%](shelf-100.png) / [120%](shelf-120.png)，[书签 100%](bookmarks-100.png) / [120%](bookmarks-120.png)，[正文标记 100%](marked-100.png) / [120%](marked-120.png)。模拟器 clean 帧不代表真实屏幕残影。
