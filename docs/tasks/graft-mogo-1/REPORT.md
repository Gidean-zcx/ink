# graft-mogo-1 执行报告

基线：`6171c2bf8d59238c95f2d548663f8b2ab3c6f43a`。日期：2026-11-12。状态：已完成承载接口及模块测试里程碑，完整任务未完成；其余构建检查执行中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 阅读链、编码、EPUB、截图 | 未通过完整验收 | 【实测】Mogo 模块及 UTF-16 测试通过；未接原应用页面，无阅读截图 | `ctest --test-dir build -R '^mogo-' --output-on-failure -j2` |
| 2 浮层恢复、成员隔离 | 仅承载层通过；原应用未验收 | 【实测】合成客户端覆盖浮层恢复、旧成员文件/偏好拒绝、儿童空集合；不是 Mogo 阅读会话 | `ctest --test-dir build -R '^graft-carrier$' --output-on-failure -j2` |
| 3 能力与按钮等价 | 仅承载层通过；原应用未验收 | 【实测】合成按键和 AI 调同一测试能力；未登记 Mogo 用户动作 | 同上 |
| 4 全量、内存检查、S31、资源 | 尚未完整验收 | 【实测】普通全量 434 项、ASan/UBSan 全量 436 项均零失败，各 3 项跳过；普通/ASan 收尾专项各 13/13；S31 宿主 75/75；固件编译待补，阅读资源未测 | `taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/ctest --test-dir build --output-on-failure -j2`；同命令换 `build-asan`、`build-s31-host` |

## 没做到的

- 未实现 `apps/` 下的“新阅读”及桌面入口；未接通 Mogo 原书架→正文→目录→书签→设置。没有放空壳入口或改用自写阅读页面冒充整块移植。
- 未完成 Mogo 的生产文件/NVS 适配、全局状态清理、原页面动作登记、快照、系统字号/键盘/弹窗接线；接口的存在不代表这些接线完成。
- 没有三本书各前后 50 页、默认/最大字号截图、真实来电卡和 AI 面板覆盖 Mogo 的验收，也没有 Mogo 翻页延迟、内部 RAM/PSRAM 峰值。
- 字体现有直接路径不成立：【实现】上游 `main/font/ttf_font.c:1323–1333` 只允许内存中的 CFF，拒绝文件中的 CFF；当前 Noto TTC 为 19,484,784 字节，face 2 的轮廓签名为 `OTTO`。不能直接把整文件塞进仅约 15.5 MiB 的模拟 PSRAM，也不能绕过资源计量。还未实现兼容字体路径；这不是取消字体要求。

## 和设计不一致的

只完成可单独验证的承载层和原模块测试，没有交付任务书要求的“整块阅读应用”。旧阅读未改。未用主机测试替身顶替设备文件、NVS 或字体实现。

## 改过的已有测试

无。

新增测试未放宽任何上游断言。UTF-16 手动反向对照换回固定上游的 `book_txt.c`，在章节/原字节位置校验处按预期退出 4；当前版本正向通过。命令：`taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python tests/graft_mogo/reverse_utf16.py build $INK_DEPS/../ink-hw/upstream/oss/mogo-e4p/main/book/book_txt.c`。

## 要用户定的

无须用户重新决定产品范围。保守方案是字体放存储卡，不改分区、不削减字形或质量；现有 Noto TTC 文件本身需 18.58 MiB，兼容读取方式及实际运行占用尚未完成验证。不能把本轮未完成的接线转成取消需求或要求用户重新授权。

## 顺手发现

开工时工作区干净、HEAD 与指定基线一致；远端指向本机依赖检出，未推送。

普通全量原话：`100% tests passed out of 434`，耗时 `806.12 sec`；`hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` 是 Skipped，不能算实测通过。全量之后新增了设置绘制和来源检查，并补测承载层关闭中切成员；最终注册 436 项，收尾专项 13/13。

内存检查：`build-asan` 使用 `INK_SANITIZE=ON`（AddressSanitizer + UBSan），主工程全量原话 `100% tests passed out of 436`，耗时 `1480.70 sec`，同样 3 个 Hub 测试跳过。C ABI 补齐后，普通和 ASan 的 13 项专项又各跑一遍，均通过；C++ 调用合同编译通过，撤掉 C 链接声明的反向编译按预期失败。单独 S31 宿主工程为普通构建，`100% tests passed out of 75`，耗时 `107.66 sec`。

来源：109 个原文文件已逐个对到上游固定提交的 Git 对象。复核命令：`taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python tests/graft_mogo/verify_import.py --upstream $INK_DEPS/../ink-hw/upstream/oss/mogo-e4p`。C ABI 反向命令：`taskpolicy -c utility $INK_DEPS/build-deps/pyenv/bin/python tests/graft_mogo/reverse_abi.py build`。

`gcccheck: 10 compiled, 0 with findings`；其中 `zip_reader.c` 因交叉工具链缺少主机 zlib 头被跳过，实际不能算 10 个文件全部通过 Linux 编译。未运行 Linux CI。

过程偏差：两项短的手动反向/设置测试编译曾与普通全量 ctest 重叠；后续构建与整轮测试按顺序执行。未并行运行两个全量测试，没有启动窗口模拟器。

## 新增的机制

- 灰度承载接口：任务书第 1 项要求；复用现有活动、LVGL 合成和显示服务，无新产品任务或线程。
- UTF-16 解码：任务书第 2 项明确要求；沿用 Mogo 读取/索引机制，原生索引缓存版本升为 3。
- 新增测试客户端与宿主测试目标：仅测试，不是产品阅读功能。

产品改动按含注释/空行的保守口径计约 302 行：嫁接目录 238（上限 600），Mogo 原文新增 32、删除 28（共 60，上限 1500），两处产品构建接线共 4 行；原样副本、测试和文档另计。超过 100 行的原因是任务明确要求画面、输入、生命周期及成员文件/偏好边界。已检查复用远程页的做法，其状态属于远程会话，不能直接作为其它应用的公开接口；本次承载层继续使用其 L8 图像合成方式。

## 要改的文档

待复核后由主会话统一更新能力目录与接续记录；本任务不改共享文档。
