# file-index-2 执行报告

基线：`291da6760931601e4783ebaddabc08e5bcab22eb`。状态：完成；仅本地提交，未推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过 | 【实测】彩色/调色板/RGBA PNG、彩色和灰度基线 JPEG；坏签名、CRC、截断、缺结束标记、坏块长度及超限输入报错，失败输出清零 | `ctest --test-dir build -R '^image-decode$' --output-on-failure -j2` |
| 2 | 通过 | 【实测】渐变只含 16 个工厂灰阶，全部档位出现；平均亮度误差 `0.0003515518`，小于最小相邻间隔 `0.01`；256 种纯灰的 4×4 块也全部小于该间隔，重复计算一致 | `ctest --test-dir build -R '^image-dither$' --output-on-failure -j2` |
| 3 | 通过 | 【实测】最长边 128，保比例且不放大小图；单图 ≤16384 B，4 槽载荷 ≤65536 B；第 5 张淘汰最早插入者，命中不改顺序；持续插入不增长，不占内部堆，释放恢复 PSRAM | `ctest --test-dir build -R '^image-cache$' --output-on-failure -j2` |
| 4 | 通过 | 【实测】同编号黑图 v1 换白图 v2 不复用；失败的 v3 也移除 v2；不同缓存上下文互不串图 | `ctest --test-dir build -R '^image-version$' --output-on-failure -j2` |
| 5 | 通过 | 【实测】普通和 ASan/UBSan 全量各 209 项通过，零失败、零跳过；两次构建及测试均返回 0。【实现】产品代码 182 行，连同第三方局部补丁 204 行，低于 250 行 | 下方全量命令，分别使用 `build`、`build-asan` |

上表 `ctest` 实际使用 `$INK_DEPS/build-deps/pyenv/bin/ctest`，所有命令前置 `taskpolicy -c utility`。

【实现】接口在 `system/include/ink/sys/image.h`，实现位于 `system/services/media/`。输入上限：4 MiB、边长 4096、1048576 像素。透明度按白底合成，缩略图先缩放再抖动。缓存不接触文件路径，只接受调用方提供的对象编号和内容版本。

【实现】目标亮度是 `PANEL_LEVEL[0] + gray/255 × (PANEL_LEVEL[15] - PANEL_LEVEL[0])`，源图黑白映射到面板可达范围；输出为工厂级别编号乘 17。HAL 查询直接读既有 `PANEL_LEVEL`，没有另造灰度表。此处是数值验收，不是新实机光度测量。

【实测】测试使用既有 `INK_EXEC_MEDIA` 的 8192 B PSRAM 栈，普通构建最坏栈估计 7056 B（宿主机悲观估计，不是 RV32 实测）。PNG 解压状态、Huffman 工作区、图片及缓存均经 PSRAM 分配。

反向对照：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/media/negative.py --build build
```

【实测】六项均按预期失败，`ctest` 返回 8，恢复后四项全部通过。脚本只暂改并恢复实现，不改断言。

| 撤掉的实现 | 失败证据 |
| --- | --- |
| 文件完整性检查 | `truncated JPEG` |
| 像素数限制 | `oversized PNG area` |
| 实测非均匀亮度，换为等距档位 | `mean luminance error` |
| 满缓存淘汰 | `over-capacity insertion succeeds` |
| 版本匹配 | `expected decoded/dithered pixel value` |
| PSRAM 工作区，换回依赖中的原始头文件 | `BUDGET FAIL warnings.count`，超过原有解码栈预算 |

构建与全量命令：

```sh
taskpolicy -c utility cmake -S . -B build -DINK_WERROR=ON \
  -DINK_CJK_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJK-Regular.ttc" -DINK_CJK_FACE=2 \
  -DINK_CJK_MEDIUM_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Medium.otf" \
  -DINK_CJK_BOLD_FONT="$INK_DEPS/build-deps/fonts/NotoSansCJKsc-Bold.otf" \
  -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python" \
  -DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python" \
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src"
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility env PYTHONPATH="$PWD/hub/src" PYTHONDONTWRITEBYTECODE=1 \
  "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
```

ASan/UBSan 使用同样设置，目录换成 `build-asan`，增加 `-DINK_SANITIZE=ON`；另将 SDL2、ImGui 的 `FETCHCONTENT_SOURCE_DIR_*` 指向依赖目录中相同固定版本，避免重复下载。

【实测】普通全量耗时 110.14 s；ASan/UBSan 全量耗时 217.25 s。确定性、分层和 SDK 边界检查包含在全量测试中。GUI 冒烟沿用既有 SDL dummy 无头脚本，未打开窗口。内存检查日志没有 AddressSanitizer 错误或 UBSan `runtime error`。

【实测】中枢测试复用依赖目录已有虚拟环境，aiohttp/PyYAML/pytest/pytest-asyncio 分别为 3.12.15/6.0.3/8.4.2/1.2.0，与项目锁定版本一致；未安装或升级依赖。`PYTHONPATH` 保证加载本检出的中枢源码。

【实现】行数含空行、注释与接口：新 C/H 共 172 行、CMake 产品接入 7 行、HAL 查询 3 行，共 182 行；第三方局部补丁另有 22 行新增/替换。未将整个第三方头文件冒充自写代码。

第三方：复用 LVGL **v9.5.0** 附带的 **stb_image v2.30**，选 MIT 许可，原许可证完整保留在头文件尾部；无新增下载或升级依赖。原文件 303477 B，SHA-256 `d81d95287ae188e472fe66ddcad8635041e8f29d51a799cc77c65c6e94400f1c`。原样复用触发解码栈超限，故保存本地副本，仅把 PNG 解压状态和 Huffman 工作区移到分配钩子；标记了修改。最终文件 **303860 B**，SHA-256 `fa53291210010dd3c8e1bafbd2abafb583b2971ecb3a98684f0c216d28748e28`。来源、许可及修改见[第三方说明](../../../third_party/stb/README.md)。

为什么不能更小：任务同时要求真实格式解码、亮度查询、抖动、缓存和版本失效。已核查更小方案：现有 JPEG HAL 只认模拟 SJ 数据；LVGL LodePNG 依赖全局界面绘图缓冲；均匀档位不满足实测亮度。现实现只保留一个解码器、一个完整性入口和一个定长 FIFO，没有另加调度或持久化层。

## 没做到的

- 文件服务接线按任务书留待 file-index-1 合入；成员/卡变化时的缓存销毁由接线方负责。接口已说明调用方独占与串行使用约束，不宣称产品接线已完成。
- 未接设备；实机画质、耗时、功耗及 Linux 运行未做（当前只有 macOS 环境）。
- 非验收范围的 EXIF 旋转、色彩管理和模糊测试未做。

## 和设计不一致的

无已知偏离。边界值是本次保守实现常量，不升级为用户需求。未放宽资源预算。

## 改过的已有测试

无；只新增 `tests/media/` 的合成夹具、四组测试、零警告预算及反向对照脚本。

## 要用户定的

无。

## 顺手发现

- 既有 SDL2 弃用警告和重复静态库链接提示仍存在，未扩大范围修复。
- 【实测】基线至最终内容的规定隐私扫描：零命中，`grep` 返回 1。报告相对链接和 `git diff --check` 通过。
- 【实测】`git diff --stat` 核查共 13 个相关文件：媒体帮助接口/实现、HAL 查询、CMake 接入、新测试、第三方头文件/来源说明及本报告；未夹带产物，文件服务与共享文档未改。

## 新增的机制

- 图片解码及统一完整性检查：验收 1 要求。
- 按面板实测亮度的 4×4 有序抖动：验收 2 要求。
- 调用方独占的 4 槽 FIFO 缩略图缓存、对象编号加版本键：验收 3、4 要求。
- 无新增任务、线程、持久格式、后台重试或配置项；测试复用已有媒体执行器。
- 架构审视：上述均是内部机制，只依赖 OSAL、HAL 和固定解码库，没有新增用户动作或页面。后续按钮和 AI 的文件能力共用此接口；本次不伪造能力表接线或页面快照。

## 要改的文档

合并时由主会话更新 `docs/tasks/backlog.md` 的 file-index-2 状态，以及 `docs/system/ui-dependencies.md` 的解码、抖动与有界缩略图进度，标明文件服务接线待合入。未修改共享文档或生成的能力表。

## 复查后的修复

2026-11-09，接续基线 `e750154a`。前文保留首次执行证据，本次结果以本节为准。复查只有三项中级问题，严重、高级、低级均为零；三项均已修复，定向及反向验证通过，全量及内存检查待完成。

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| FI2-R1-001 | 定向通过 | 【实测】彩色、灰度及两种带重启标记的基线 JPEG 正常解码；四张图逐字节截断并补 EOI，均报错且输出清零。四类解码容错分别撤回后均失败 | `ctest --test-dir build -R '^image-decode$' --output-on-failure -j2`；下方反向命令 |
| FI2-R1-002 | 定向通过 | 【实测】最大像素 PNG 解码和逐像素转换、1024×1024 抖动均消耗模拟 CPU 时间；分别撤掉解码、转换、抖动计费均失败 | 同上及 `ctest --test-dir build -R '^image-dither$' --output-on-failure -j2` |
| FI2-R1-003 | 定向通过 | 【实测】坏长度用例先恢复有效 CRC；仅撤掉长度保护时程序越界失败，恢复后通过 | 同上；反向用例 `chunk_length` |
| 验收 1～4 | 定向通过 | 【实测】四组图片测试全部通过，未放宽原断言或资源预算 | `ctest --test-dir build -R '^image-' --output-on-failure -j2` |
| 验收 5 | 待完成 | 普通、ASan/UBSan 全量待完成；【实现】自写产品代码 191 行，加第三方新增/替换 34 行，共 225 行，低于 250 行 | 沿用前文全量命令 |

【实现】JPEG 只改现有解码器：停止用零补齐缺失的熵数据，校验 DC/AC 位数、分量扫描完成和过早缺失的重启标记，错误标记返回失败；最后一个 MCU 允许没有尾随重启标记。没有另加解析器。媒体模块仅在既有解码边界、灰度转换、抖动和缩放循环调用既有 OSAL 成本接口，保留抖动让出点。

【估算】计费假设为编码字节 80 周期、解码像素 200 周期、灰度合成像素 64 周期、抖动像素 128 周期、缩放像素 32 周期，再由原有模拟器应用档案中的 CPU 频率及指令缓存惩罚。这些数值未经实机或 ISS 校准，只补齐随工作量增长的模拟成本，不作为设备速度结论。

反向命令（日志 `build/fix-negative.log` 和 `work/image-negative/`）：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/media/negative.py --build build
```

【实测】14 项反向对照全部按预期失败（`ctest` 退出码 8）；脚本恢复后四组全部通过。包含原六项与 JPEG 四种容错、三种成本计费、PNG 长度保护。坏长度变体的越界是故意移除保护的对照结果，正常实现无此失败。

改过的已有测试：

- `tests/media/image.c`：增加 JPEG 各截断点及模拟 CPU 时间回归；PNG 坏长度前重新复制夹具。所有原期望、堆断言及预算保持不变。
- 真实计费使测试跨过平台音频启动任务退出时刻，原全局堆快照会把该任务释放误判为媒体变化；测试前通过现有音频启动接口等待启动锁并停止输出，再取快照，没有加 sleep。
- `tests/media/fixtures.py`：新增两张合成的带重启标记 JPEG，复用已安装 Pillow，未引入新库。
- `tests/media/negative.py`：完整性检查反向预期由 `truncated JPEG` 改为 `PNG CRC error`，因为 JPEG 截断现在由解码器自身拒绝；原有 CRC 检查仍需独立验证。新增反向变体；每次构建前移除两份生成对象，避免 Make 同秒时间戳复用上一个变体造成假结果。完整重跑通过。

【实现】本次产品差异为媒体实现新增/替换 15 行、第三方新增/替换 14 行；没有新机制，没有改接口、文件服务或共享文档。第三方仍是原版本和 MIT 许可；现文件 304260 B，SHA-256 `97d51a9fb2bd50f9a7f3ecdabb9c3603d6c7ff972c4bc5aa556fac8178b99580`，来源说明已补充 JPEG 修改。架构边界及未接文件服务、Linux 与实机未验证的限制沿用前文；本次没有新增用户动作或页面。
