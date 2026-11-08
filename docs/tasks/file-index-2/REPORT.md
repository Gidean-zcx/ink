# file-index-2 执行报告

基线：`291da6760931601e4783ebaddabc08e5bcab22eb`。状态：实现与反向对照完成，内存检查进行中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 通过 | 【实测】彩色/调色板/RGBA PNG、彩色和灰度基线 JPEG；坏签名、CRC、截断、缺结束标记、坏块长度及超限输入报错，失败输出清零 | `ctest --test-dir build -R '^image-decode$' --output-on-failure -j2` |
| 2 | 通过 | 【实测】渐变只含 16 个工厂灰阶，全部档位出现；平均亮度误差 `0.0003515518`，小于最小相邻间隔 `0.01`；256 种纯灰的 4×4 块也全部小于该间隔，重复计算一致 | `ctest --test-dir build -R '^image-dither$' --output-on-failure -j2` |
| 3 | 通过 | 【实测】最长边 128，保比例且不放大小图；单图 ≤16384 B，4 槽载荷 ≤65536 B；第 5 张淘汰最早插入者，命中不改顺序；持续插入不增长，不占内部堆，释放恢复 PSRAM | `ctest --test-dir build -R '^image-cache$' --output-on-failure -j2` |
| 4 | 通过 | 【实测】同编号黑图 v1 换白图 v2 不复用；失败的 v3 也移除 v2；不同缓存上下文互不串图 | `ctest --test-dir build -R '^image-version$' --output-on-failure -j2` |
| 5 | 部分验证 | 【实测】普通全量 206 项通过，3 项中枢测试因缺 Python 依赖跳过；ASan/UBSan 进行中。产品代码低于 250 行，精确计数待补 | 下方全量命令 |

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
  -DFETCHCONTENT_SOURCE_DIR_LVGL="$INK_DEPS/build/_deps/lvgl-src"
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
```

ASan/UBSan 使用同样设置，目录换成 `build-asan`，增加 `-DINK_SANITIZE=ON`；另将 SDL2、ImGui 的 `FETCHCONTENT_SOURCE_DIR_*` 指向依赖目录中相同固定版本，避免重复下载。

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
- 最终差异范围、隐私扫描及内存检查结果待补。

## 新增的机制

- 图片解码及统一完整性检查：验收 1 要求。
- 按面板实测亮度的 4×4 有序抖动：验收 2 要求。
- 调用方独占的 4 槽 FIFO 缩略图缓存、对象编号加版本键：验收 3、4 要求。
- 无新增任务、线程、持久格式、后台重试或配置项；测试复用已有媒体执行器。
- 架构审视：上述均是内部机制，只依赖 OSAL、HAL 和固定解码库，没有新增用户动作或页面。后续按钮和 AI 的文件能力共用此接口；本次不伪造能力表接线或页面快照。

## 要改的文档

合并时由主会话更新 `docs/tasks/backlog.md` 的 file-index-2 状态，以及 `docs/system/ui-dependencies.md` 的解码、抖动与有界缩略图进度，标明文件服务接线待合入。未修改共享文档或生成的能力表。
