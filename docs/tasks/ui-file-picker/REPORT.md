# ui-file-picker 执行报告

日期：2026-11-09。基线：`c6dede1945807b2cd688e6967c7299154813827e`。本轮修复提交：`736d7ac6`。状态：两项复查问题均已修复；普通、ASan/UBSan 全量各 218/219，唯一失败为待合并生成的共享能力总表。仅本地提交，未推送。

前轮实现提交为 `02326868`、`7c6f4691`、`3ab4830d`，原始复查和前轮证据见 [REVIEW.md](REVIEW.md)。复查列出严重 0、高 1、低 1，无中级；本轮两项都修。

代码：[选择器](../../../system/ui/file_picker.c)、[接口](../../../system/include/ink/sys/file_picker.h)、[回归测试](../../../tests/ui_file_picker.c)。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| R1-01 高 | 【实测】已修复 | 真实 AI `system.home` 销毁调用页，选择器同时关闭；后续取消不回调；单独撤回生命周期修复后 6 条断言失败，包含销毁后回调及后续计数失败 | T1、N1 |
| R1-02 低 | 【实测】已修复 | 真实最大字号、孩子字号各检查 4 行文字框，均不重叠；单独撤回行高修复后仅这 8 条断言失败；已打开两张截图核对 | T1、N2 |
| 1 图片过滤、分页、编号和 lookup | 【实测】通过 | 7 张图片按 4/3 分页，音频和文字不出现；触摸、AI 返回稳定编号，调用方 lookup 成功 | T1 |
| 2 取消恢复原页原位置 | 【实测】通过 | 原活动对象、页位置和快照不变，取消前后 PNG 全像素相同；调用页销毁时按 R1-01 丢弃回调 | T1、N1 |
| 3 孩子隔离及反例 | 【实测】正向通过；反例沿用前轮证据 | 孩子看不到、不能选择或 lookup 大人私有图片，能选自己的图片；换人关闭旧浮层。前轮独立复查撤掉文件服务权限检查后准确失败 2 条，文件服务本轮未改 | T1；REVIEW.md 的 N1 |
| 4 全量、内存检查、250 行预算 | 【实测】两套全量各 218/219；【实现】208 行，预算通过 | 唯一失败为待合并生成的能力总表；两套均零跳过，未见 sanitizer 错误；本轮产品增删合计 15 行，未放宽预算或断言 | T2、T3；git diff --numstat |

## 没做到的

- 【实测】两套全量的 `capability-table-doc` 均未通过：提交的总表是 197 项，代码已有 201 项，缺 `file_picker.*` 四项，后续分组编号也随之变化。总规则第 7 节禁止本任务修改共享文档，留给合并者统一生成；本轮保留原表运行全量。
- 未运行 Linux/gcc 或真机；证据限 macOS/clang 模拟器。没有碰设备、运行可见窗口、调用付费接口或对外发布。
- 按 F9 任务范围只做系统选择器和测试调用方，文件应用、缩略图、解码或转换未实现。类型资格沿用文件服务分类及权限，不代表文件内容已验证可解码。

## 和设计不一致的

【实测】默认字号 `FilePicker` 画板差值仍为 **28.414/255**，超过 12/255。设计是灰底分组、分隔线和圆形图标底，实现沿用 F4 的四个独立描边行；页码条跟随行池，约比画板高 300 像素，顶部还有额外外框。设计展示书籍准备状态，测试使用合成图片和“可以选择”；现行字号和灰度也与旧画板有差异。本轮按复查范围修生命周期和行距，未更改这些既有样式。对照图：`work/ui-file-picker/r1-compare/FilePicker.png`。

## 改过的已有测试

【实现】`tests/ui_file_picker.c` 追加调用页销毁标记、真实 `system.home` 后关闭和禁止回调检查；通过真实设置切到最大字号、切换到孩子，检查每页四行名称和状态的边界框并截图。保留全部原断言和预算，没有修改既有期望、跳过测试或增加掩盖时序错误的等待。前轮对能力和 SDK 白名单的追加仍保留，本轮未再修改。

## 要用户定的

无；未新增产品选择。

## 顺手发现

【实测】反向夹具初次只撤掉事件绑定，仍保留解绑代码，取消时访问已销毁的页面，临时程序退出信号 11。这不是完整撤回修复，未作为验收证据；改为在编译器输入中完整撤回生命周期修复后得到 N1。未改产品源文件、原库或原测试可执行程序。

## 新增的机制

【实现】本轮没有新增任务、线程、状态机、文件格式、配置项、缓存或框架。选择器复用调用页现有 `LV_EVENT_DELETE`，正常结束时解除绑定；成员变化、后台读取及 UI 收尾沿用原实现。

## 要改的文档

- 按总规则第 7 节，共享文档本轮保留原样。合并者统一生成并提交 `docs/system/capabilities.md`，纳入 `file_picker.prev/next/cancel/select`，再验证 `capability-table-doc`。
- 合并者更新 `docs/tasks/backlog.md`、`docs/system/ui-dependencies.md` 中 F9 状态，保留本报告的视觉差异和验证范围。
- 后续应用开发说明可收录 `file_picker.h`：在 UI 打开，以回调获取编号，BG 上 lookup；调用页销毁或成员/认证变化会关闭选择器并丢弃回调。

## 命令与证据

所有构建和测试均加 `taskpolicy -c utility`，构建最多 `-j3`、CTest 最多 `-j2`，两套全量串行；依赖和设计稿只读。以下路径都相对仓库根目录，构建产物和临时夹具未提交。

配置使用总规则全部参数，并指定已有中枢 Python：

```bash
export PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src" \
  -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"
```

T3 配置只把目录改为 `build-asan`，再加 `-DINK_SANITIZE=ON`，保留 `INK_WERROR=ON`。

- T1：`taskpolicy -c utility cmake --build build --target sim-ui-file-picker -j3` 后执行 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^ui-file-picker' --output-on-failure -j2`。构建和测试退出 0，2/2、121 条断言通过，零失败和告警。日志 `work/ui-file-picker/r1-after-build.log`、`r1-after-test.log`；指标 `r1-after-metrics.json`。内部 RAM 最低余量 47,472 字节，高于原门槛 32,768；内外部分配失败均为 0。
- N0：新增测试、产品尚未修复时构建退出 0，运行退出 8、`finished=1`、14 条失败；日志 `r1-before-build.log`、`r1-before-test.log`，指标 `r1-before-metrics.json`。修复后同一测试得到 T1。
- N1/N2：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" work/ui-file-picker/r1-negative.py`。按正常 CMake 参数在编译器标准输入中分别撤回完整生命周期修复、行高修复，链接本轮未修改的测试对象；不写产品源码或原库。两次编译和链接退出 0，运行均退出 1、`finished=1`，分别 6、8 条失败。日志、临时程序和指标在 `work/ui-file-picker/r1-negative-{home,text}*`。
- 视觉：将 T1 的真实静止截图及通过指标复制到 `work/ui-file-picker/r1-compare/FilePicker/`，执行 `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out work/ui-file-picker/r1-compare --reuse --scenes FilePicker`，退出 0，28.414/255。已打开 `build/test-out/ui-file-picker/snap-large-text.png` 和 `snap-child-text.png`，未编辑截图。

```bash
# T2
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
# T3
taskpolicy -c utility cmake --build build-asan -j3
UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

| 检查 | 配置/构建退出码 | CTest 结果 | 耗时 | 日志（work/ui-file-picker/） |
| --- | --- | --- | --- | --- |
| T2 普通全量 | 0 / 0 | 218/219，零跳过，退出 8；仅能力总表失败 | 190.64 秒 | r1-configure.log、r1-full-build.log、r1-full-test.log |
| T3 ASan/UBSan 全量 | 0 / 0 | 218/219，零跳过，退出 8；仅能力总表失败 | 296.00 秒 | r1-asan-configure.log、r1-asan-build.log、r1-asan-test.log |

【实测】两套选择器均 `test.pass=121`、`test.fail=0`、`finished=1`、零告警，取消前后逐像素检查通过。内部 RAM 最低余量均为 47,472 字节，内外部分配失败均为 0。ASan 构建参数含 `-Werror -fsanitize=address,undefined -fno-omit-frame-pointer`，运行设置 `UBSAN_OPTIONS=halt_on_error=1`；完整日志未见 AddressSanitizer/UBSan 错误。分层、SDK、确定性、能力运行时表和中枢契约均通过。

## 规模与架构审视

【实现】相对指定基线，产品累计增加 207、删除 1，共 208 行（含 9 行 CMake）；本轮增加 11、删除 4，共 15 行。为什么累计不能更小：F9 须提供选择会话、真实索引的后台读取、F4 列表、系统浮层、编号回传、成员归属、共用能力和 AI 快照。更小的裸路径返回或 UI 上直接读取索引分别违反编号要求和不阻塞 UI 的约束。

【实现】文件身份及权限仍归文件服务；选择器只保存临时编号和名称，模型在 PSRAM。选择前再次 lookup；按钮、实体键和 AI 共用原子能力，快照来自同一 F1/F4 声明。调用页生命周期集中在选择器内处理，未逐调用方加补丁，也未改变服务内部状态。

【实测】报告链接均存在，`git diff --check` 通过；差异仅含选择器、接口注释、回归测试和本报告，本轮没有修改共享文档或调研、简报、审计资料。基线至 HEAD 的补丁按总规则指定正则扫描，隐私项零命中（grep 退出 1）；构建产物、截图、设计稿和临时夹具均未提交。
