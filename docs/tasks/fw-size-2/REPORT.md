# fw-size-2 执行报告

基线：`6171c2bf8d59238c95f2d548663f8b2ab3c6f43a`。完成；代码与最终构建对应 `eea79e63`，报告另作本地提交。未接设备、未推送。尺寸、哈希、像素和时延数据见 [measurements.json](measurements.json)。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 固件 4,831,328 → 4,100,416 字节，省 730,912 字节（713.8 KiB）；716 条资源解压后与基线逐字节一致；字库首包不变；六页像素一致；六个首次开页用例 p50 最大增加 10.670 ms、p95 最大增加 0.225 ms | `bash ports/esp32s31/build.sh`；`python tests/asset_pack/screens.py --build build --out work/final/screens --before work/baseline/screens`；`python tests/perf_latency/run.py --build build --runs 3 --out work/final/perf` |
| 2 | 【实测】通过 | 测试页使用原始资源确实存在、设备名单外的 `plus-line-106.inki`，模拟器退出 2、缺图警告计数非零；恢复旧加载器会掩盖缺图；撤回释放会触发 PSRAM 断言；S31 撤回解压与释放分别退出 -6 | `python tests/asset_pack/run.py --build build`；`python tests/asset_pack/reverse.py --build build --before <基线>`；`python ports/esp32s31/tests/storage_negative.py build-s31-tests` |
| 3 | 【实测】通过 | 普通全量 424/424，无跳过；S31 宿主机 75/75；ASan/UBSan 主工程相关 15/15、S31 资源 2/2；S31 编译成功，无新增警告；gcccheck 共 5 个文件、0 findings | 普通全量、内存和固件命令见下 |

## 实现与资源

【实现】仅嵌入包允许条目压缩：目录项长度最高位标记压缩，低 31 位为存储长度；载荷为原长 4 字节加 zlib 流，只有更小时才压缩。首包仍为原 INKA 格式，分包顺序、安装工具和分区不变。使用已有 miniz，检查完整长度、流结束和 Adler-32。输出和解压工作区都用 OSAL 外部内存；图标交给原有 256 项像素缓存后立即 unmap 临时条目。

【实现】模拟器只读同一脚本生成的两个包，宿主文件读取仍走 `vtk_host_call`，压缩输入的闪存缓存访问照常计费。宿主使用同一固定 miniz 源码的无内部分配接口，避免把应用分配器引入平台测试；固件直接链接已有 `ink_miniz`。原阅读器的分配方式保持原样。普通全量未发现缺失的大图标，原名单保持不变。

【实测】嵌入包 1,498,390 → 767,212 字节，省 731,178 字节；716 条均被压缩，解压后逐字节一致。字库首包仍为 4,718,377 字节，SHA-256 不变。宿主机和 S31 两份包的输出逐字节相同。固件应用分区余 1,142,464 字节。

【实测】主页、应用、设置、控制中心、听写、电源菜单六页的灰度像素哈希全部一致。六页资源标签峰值均为 10,264 字节、结束占用 0；全机 PSRAM 峰值相对基线的最大增加为 112 字节，分配器采样与合并会影响此差值。无卡词库回退保留 968,736 字节（文件 968,735 字节，含分配对齐，约 0.924 MiB），资源标签峰值 977,696 字节；服务没有另复制一份。S31 编译器栈报告：资源查找 `find` 80 字节、`hal_asset_map` 32 字节；解压工作区在外部内存。

【实测·模拟器】下表使用 `--cpu measured`、配置中的宿主到目标系数 40；改前改后各 3 个独立进程，每页首次打开。属于模拟器测量，不代表设备实测。基线和最终各有 42 条既有长计算警告，全部为未进入内核的长计算提示，没有资源缺失警告；常规功能测试的零警告预算未改。

| 页面 | 改前 p50（ms） | 改后 p50（ms） | p50 增量（ms） | p95 增量（ms） |
| --- | ---: | ---: | ---: | ---: |
| 设置 | 1071.949 | 1082.619 | +10.670 | -9.035 |
| 文件 | 963.048 | 961.954 | -1.094 | -10.186 |
| 相册 | 819.226 | 818.505 | -0.721 | -2.960 |
| 短信 | 952.055 | 951.957 | -0.098 | -1.983 |
| AI | 942.115 | 941.870 | -0.245 | +0.225 |
| 行为记录 | 992.006 | 991.896 | -0.110 | -0.130 |

## 测试与复现

沿[总规则](../codex-rules.md)配置字体、Python、LVGL；本次还复用 `$INK_DEPS/build/_deps/` 下固定的 SDL2、ImGui 源码。中枢使用 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`，`PYTHONPATH="$PWD/hub"` 指向本工作区代码；未安装或升级依赖。以下命令在 Mac 上均加 `taskpolicy -c utility`，构建和 CTest 最多 `-j2`。

- 普通全量原话：`100% tests passed out of 424`（918.43 秒，无跳过）。命令：`PYTHONPATH="$PWD/hub" taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`。
- S31 宿主机：`cmake -S ports/esp32s31/tests -B build-s31-tests`，构建后 CTest 全部通过，原话 `100% tests passed out of 75`（95.71 秒）。
- 内存检查：主工程换目录 `build-asan`、加 `-DINK_SANITIZE=ON`，其余配置保持一致，完整构建。CTest 原话 `100% tests passed out of 15`（11.84 秒），覆盖资源解压/释放/缺图、词库、输入法三种路径、六页、阅读器、两项确定性；不是全量 ASan CTest。
- S31 内存检查：`cmake -S ports/esp32s31/tests -B build-s31-asan -DINK_SANITIZE=ON`，构建 `test-s31-assets`，CTest `-R '^unit-s31-(assets|asset-pack)$'`，原话 `100% tests passed out of 2`（0.32 秒）。损坏流、两次分配失败、重复解压及回收均覆盖。
- 固件：`INK_DEPS="$INK_DEPS" taskpolicy -c utility bash ports/esp32s31/build.sh`，成功。基线与最终均为 0 条编译器警告，6 条既有 IDF CMake 警告逐条相同。
- gcccheck：`python3 ../../tools/gcccheck.py build ports/sim/hal/hal_assets_sim.c system/ui/icons.c tests/asset_pack/device.c` 输出 `gcccheck: 3 compiled, 0 with findings`；对 `build-s31-tests` 的 `hal_assets_s31.c`、`tests/assets.c` 输出 `gcccheck: 2 compiled, 0 with findings`。这是交叉 GCC 检查，未声称本机跑过 Linux。
- 反向对照：模拟器旧加载器能显示被排除图标，故缺图验收会拒绝旧行为；删除 unmap 后 PSRAM 回收断言失败。S31 手动反向编译输出 `reverse controls: 10/10 rejected; both storage baselines passed`，新加的 `inflate`、`asset-unmap` 均为 `compile=0 test=-6`。历史对照仅手动运行，不注册进 CTest。

主工程内存检查选择命令：

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2 \
  -R '^(unit-asset-pack|integration-asset-pack|ime-lexicon|integration-text-(main|nocard|baddict)|determinism|determinism-reader|ui-scene-(Main|Apps|Settings|Control|AiListen|PowerMenu)|scenario-reader_basic-metalio-plus)$'
```

基线截图、时延及固件在首次修改前采集，产品源码为指定基线（当时只有报告的文档提交）。原始截图在 `work/baseline/screens/` 和 `work/final/screens/`；时延在对应的 `perf/`；构建、测试和反向对照日志在 `work/`，不提交构建产物。

## 没做到的

- 未做真机延迟和内存验收；本任务禁止接设备。
- 【实现】`system/services/text/pinyin.c:244` 的旧回收分支把所有 HAL 映射视为闪存，当前不会主动回收压缩词库的约 0.924 MiB。HAL 的 `unmap` 已可释放；调用方不在允许改动范围，未改。图标临时映射已实测回收。

## 和设计不一致的

- `platform/include/ink/hal_assets.h` 的总述仍声称所有资源不拷入 RAM；本任务要求压缩按需解压，现有 map/unmap 接口兼容，但此注释需主会话更新（文件不在允许范围）。

## 改过的已有测试

- `asset_pack_test.py` 保留原有四项断言，新增逐条压缩与名单过滤两项。
- S31 资源 fixture 新增可压缩条目、损坏流；资源测试新增外部内存分配失败和回收断言；反向编译补链原有 miniz，新增撤回解压与撤回回收两项对照。
- `ime_lexicon/run.py` 的空词库夹具也生成实际资源包；保留服务标签占用 0 的断言，补充 HAL 标签实占词库大小（允许分配器对齐，不改预算）。
- `integration-text-nocard`、`integration-text-baddict` 的 CMake 空词库夹具也生成同格式资源包，原断言未改。首轮全量原话 `99% tests passed, 2 tests failed out of 424`（803.23 秒），均因旧夹具只覆盖原始文件；修复后两项定向复测通过，再清空生成的 `build/test-out`，重跑全量 424 项全部通过。

## 要用户定的

- 无。

## 顺手发现

- S31 基线实编为 4,831,328 字节，任务书旧值为 4,830,560 字节；减量采用本次同依赖复建的前后对照。

## 规模与边界

【实测】产品代码新增 108 行、删除 37 行，改动合计 145 行（CMake 中新增测试与夹具的 6 行另算测试），不超过 150 行。

为什么不能更小：只压图标实算只能省约 237 KB，达不到 400 KB；两端 HAL 都需处理条目长度和解压，直接用 miniz 的栈式便捷接口会把约 9 KB 工作区放到任务栈，故使用低层接口和外部内存。没有引入跨模块抽象。资源格式和读取属于 HAL 内部机制，既有用户与 AI 的调用路径共用这套读取。

## 新增的机制

- 嵌入包条目压缩标记与原长前缀：任务书第 1 项要求；首包不变。
- 无新增任务、线程、后台重试、配置项或缓存；既有 map/unmap 承担按需解压与释放。

## 要改的文档

- 合并时由主会话更新任务清单和 HAL 资源注释；本任务未改共享文档。词库主动回收需另行允许修改其调用方。

## 上机核对清单

- 确认冷开六页图标完整，反复开关页面不会累积临时解压内存。
- 无卡时调用拼音输入，确认约 0.924 MiB 词库常驻后仍满足设备 PSRAM 预算；核对真机首次解压耗时。

## 差异与隐私检查

【实测】差异仅含允许的打包器、两端资源 HAL、图标释放、资源构建接线、相关测试和本报告目录；未修改分区、安装工具或共享需求文档。`git diff --check`、报告相对链接检查通过。按总规则正则扫描基线至最终 HEAD 的提交差异，无绝对路径或设备标识匹配（退出 1）。
