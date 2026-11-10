# port-storage-1 执行报告

基线：`1450591caecfffdff023b19dc057fad5174a050f`。只编译和宿主机验证，不连接设备，不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | S31 构建退出 0，警告 0；端口普通版与 ASan 均 13/13；反向对照 8/8 检出，另有 GCC 符号转换反向对照 | `bash ports/esp32s31/build.sh`；`ctest --test-dir build-s31-host -j2`；`ctest --test-dir build-s31-host-asan -j2`；`python3 ports/esp32s31/tests/storage_negative.py build-s31-host` |
| 2 | 【实测】通过 | 普通版全量 339（初次 3 跳过，随后补齐）；ASan/UBSan 全量 339/339，无跳过；GCC 3 文件 0 finding；隐私 0 命中 | `ctest --test-dir build --output-on-failure -j2`；中枢补跑 4/4；`ctest --test-dir build-asan --output-on-failure -j2`；`gcccheck.py build-s31-host`；任务规定的基线扫描 |
| 3 | 【实测／实现】通过宿主机核对，未上机 | 外部包 4,718,142 B；内嵌包 549,358 B；空卷可用 401,408 B；原厂与脚本说明见下 | `python3 tools/ui_assets_check.py build/assets`；`build-s31-host/test-s31-storage-rw` |

## 没做到的

- 未做实机验证和真实刷写/恢复；任务明确禁止设备操作。未在 Linux 主机上执行，使用本机 clang、S31 GCC 和指定 gcccheck 核查。

## 和设计不一致的

- 卡端直接使用 IDF/FAT 的 `rename`：已有同名目标时返回失败，不能提供 POSIX 的原子覆盖；没有采用先删目标的有损替代。这是卡端 HAL 合同尚未满足的一项，需复核者明确处理。
- 首挂只在 LittleFS 返回 `LFS_ERR_CORRUPT` 时格式化（覆盖出厂空分区）；底层 IO 错误保留分区，不将读故障变成擦除。
- 原厂没有插卡检测脚，挂载按需触发；为满足本任务的插拔通知，启用 IDF 访问状态检查，并增加每秒一次检测，由默认平台事件任务执行。插拔通知有检测延迟；没有添加专用存储任务。

## 改过的已有测试

- `tools/ui_assets_check.py`：将字体+图标合计不超过 6 MiB，改为实际分包后分别不超过 4.5 MiB、1.5 MiB，包含包头/目录开销；任务第 1 项明确要求，总预算未增加。
- 其余既有断言、预算、跳过条件均未改。另新增端口测试和根工程两个打包/刷写计划测试注册。

## 要用户定的

- 卡上已有目标的原子覆盖约束见“和设计不一致的”；需要在后续 FAT 设计中定，不在这里引入事务或文件格式。

## 顺手发现

- 工作区起始干净，HEAD 与指定基线一致。
- 模拟器的既有链接方式仍产生重复静态库提示；S31 最终编译 warning 为 0，上游 Kconfig 仍有 NOTE。没有扩大到链接结构或上游 SDK 修复。

## 新增的机制

- `INKA` 两包资源格式：任务第 1 项要求；固定 12 B 头和 104 B 条目（96 B 名字、偏移、长度），按文件名排序，顺序装入。
- `CONFIG_INK_SD_READONLY`／构建变量 `INK_SD_READONLY=1`：任务第 3 项要求；默认可写，同时阻止 HAL 修改和 SD CMD24/CMD25/CMD38。
- 卡检测定时器和默认平台事件任务：任务第 3 项的通知要求；定时器只投递事件，事件中检测/挂载，回调在锁外运行。
- LittleFS 使用既有格式和模拟器相同参数；没有增加 Flash 写任务、缓存、重试队列或新的持久格式。

## 要改的文档

- `docs/system/platform-and-simulator.md` 的 Plus `/user` 1408 KiB 改为 400 KiB，资源预算说明补两包限制。
- `ports/esp32s31/README.md` 更新资源、文件已接入和卡只读开关；跨任务入口合并后统一更新。
- 本次未改需求、想法、决策、能力生成表或研究/审计历史。

## 里程碑

- 已接入真实资源和存储 HAL，仅删除对应桩。资源查找不复制进堆；使用同一份 LittleFS；卡挂载、容量和通知由 SDMMC/FAT 返回。
- 普通端口与 ASan 的原话均为 `100% tests passed out of 13`。8 项反向对照分别撤掉几何约束、只读打开保护、SD 写命令保护、通知、两个包的查找、包预算和备份哈希核对，全部失败；两个未改动的读写模式基线成功。
- 模拟器普通版全量退出 0：原话 `100% tests passed out of 339`，818.33 秒；其中中枢 3 项因所选 Python 缺依赖跳过。切换到只读的现成 `$INK_DEPS/hub/.venv`，并显式指定本检出的 `hub/src` 后，3 项和 fixture 补跑 `100% tests passed out of 4`，63.91 秒；没有删改跳过条件或安装/升级依赖。
- 最终端口普通版/ASan 均重新验证 13/13；资源和存储 HAL 的宿主 GCC 核查 3 文件 0 finding。另用实际 S31 编译命令加只读宏，编译退出 0；撤掉有符号大小转换后 GCC 恢复 `changes signedness` 错误，反向编译退出 1。
- 全量 ASan/UBSan 构建退出 0；原话 `100% tests passed out of 339`，915.35 秒，没有跳过。`build-asan/Testing/Temporary/LastTest.log` 中 sanitizer 错误、警告和 UBSan runtime error 均为 0。
- 实际两个包逐条回读：889 个资源、5,175,020 B 原始内容与 `build/assets` 逐字节一致。
- 基线至本地提交的任务规定隐私扫描为 0 命中；`git diff --check` 通过，差异仅任务文件。未推送，未创 PR，未执行设备脚本输出的命令。

## 容量与依据

| 项目 | 本次大小 | 上限／余量 |
| --- | --- | --- |
| 外部包（222 条） | 4,718,142 B | 4,718,592 B／450 B |
| 内嵌包（667 条） | 549,358 B | 1,572,864 B／1,023,506 B |
| 完整固件，含内嵌包 | 2,217,504 B | 5,242,880 B／3,025,376 B |
| `/user` 空卷 | 总量 409,600 B；可用 401,408 B（392 KiB） | 100 × 4096 B；2 个元数据块，写入后按实际占用减少 |

- 【实现】资源两区上限为 4.5 MiB、1.5 MiB，保留原有 6 MiB 总资源预算；固件另受整个 `ota_1` 的 5 MiB 限制。表头/目录开销已计入包预算，溢出输出超出字节数并失败。
- 【实现】IDF v6.1，提交 `fff9895c82d744c7237be8847347bdd1b07c6643`：`components/soc/esp32s31/include/soc/ext_mem_defs.h:34–44,85` 定义 Flash 和 PSRAM 的独立窗口，1024 个映射页；`soc_caps.h:321` 为每种外存独立 MMU。生成配置为 64 KiB 页，即每窗 64 MiB；16 MiB PSRAM 在独立窗口。即按完整程序 5 MiB 加资源 4.5 MiB 和页对齐计算，也远低于 Flash 窗口，故整体 `esp_partition_mmap`，不逐资源重复映射。
- 【实现】原厂 `CloudZao/Metalio-E-INK4-Plus@690a023`，查阅 2026-11-10：`main/boards/metalio-e-ink4-plus/config.h:125–139` 为 GPIO24/25/20–23；`main/boards/common/storage/SdCardManager.cc:83–101,125–140` 为 SDMMC Slot0、4 线、默认时钟、不格式化卡；`:27–29,42–60` 为内部 4 KiB DMA 缓冲及 SDK 对齐查询。`main/boards/common/power_policy/README.md:61,99–100,150` 说明卸载和引脚保持；完整休眠整合由并行电源任务处理，本任务不承诺整板休眠。
- 【实现】本机 IDF 的 `components/fatfs/vfs/esp_vfs_fat.h:98–139` 没有新版文档的 `read_only` 字段，故在 HAL 与 SD 命令入口保护，未升级 IDF。[官方 FAT 文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/fatfs.html)描述的新版字段不能直接用于本机版本。
- 【实现】Flash 读写仍由原存储服务调度和显示守卫处理（`storage_service.c:495–514` 的音频判断和写入）；首挂/空卷格式化在系统和音频启动前完成。卡事件不操作 `/user`。R-061：现有文件能力和 AI 路径继续共用 HAL，没有界面直写存储的新入口。

## 构建与脚本用法

先按总规则生成 `build/assets`；`INK_DEPS` 指向只读依赖检出，`INK_UPSTREAM` 可指向准许读取的原厂副本。普通编译：`taskpolicy -c utility bash ports/esp32s31/build.sh`；只读卡编译加 `INK_SD_READONLY=1`。CI 补生成同一套资源的步骤。编译用分区 CSV 与原厂名称、类型、偏移、大小一致，仅用于 5 MiB 尺寸检查；脚本不写分区表。

两个设备脚本都只生成命令，**不会执行 esptool 或打开设备**：

```bash
export INK_FLASH_BACKUP='<整片备份文件>'
export INK_FLASH_BACKUP_SHA256='<此前独立核对过的 SHA-256>'
bash tools/device/flash_trial.sh > build-s31/flash-trial.commands.sh
bash tools/device/restore_factory.sh > build-s31/restore-factory.commands.sh
```

备份必须是 16 MiB，哈希与分区表必须符合固定布局。试用命令只写并逐段比对 `ota_1`、`font_data` 前 4.5 MiB 和最后的 `otadata`（序号 2、合法 CRC，选 `ota_1`）；恢复命令从备份切出完整 `model`、`font_data`、最后的 `otadata`，各段写后比对。命令输出留在构建目录，包含运行时路径，不提交。上机复核者检查命令、设置 `INK_FLASH_PORT` 后自行执行；命令不自动重启。

## 规模

- 产品代码净增 350 行（含设备脚本、构建配置和 CI 资源构建步骤；排除测试及其注册、文档，以及任务明确另计的 70 行打包工具）。没有提高原有资源预算。
- 为什么不能更小：任务同时要求两个资源包、LittleFS、SDMMC/FAT 的完整 HAL 与离线刷写/恢复校验。仓库只有 LittleFS 内核，没有可复用的 IDF VFS 适配器；直接用现有内核和少量 HAL 包装，比引入第三方适配层更小。没有重构存储服务，也没有复制文件到 PSRAM 或逐调用方打补丁。

## 上机先核对清单

- 实际 16 MiB Flash／16 MiB PSRAM 与资源整体映射，界面字体/图标是否完整。
- 使用只读卡编译先检查真实 FAT 的读、容量、拔插通知与 SDMMC 时序；再由复核者决定写卡验证。
- 真实 Flash 擦写/掉电、音频播放期间的守卫，以及电源任务接入后的 SD 停放与恢复。以上均未在本任务中执行。

## 复核入口

所有构建最多 `-j2`、ctest 最多 `-j2`，本机命令均以 `taskpolicy -c utility` 调度。上表 `ctest` 指 `$INK_DEPS/build-deps/pyenv/bin/ctest`。普通版和全量内存版均按总规则配置字体和 LVGL；内存版增加 `INK_SANITIZE=ON`。两边的 `INK_HUB_PYTHON` 使用 `$INK_DEPS/hub/.venv/bin/python`，测试前设置 `PYTHONPATH="$PWD/hub/src"`，确保测试本检出的中枢源码。

本地证据在 `work/port-storage-1/`：`s31-final.log`、`host-test.log`、`host-asan-test.log`、`full-test.log`、`hub-retest.log`、`asan-test.log`、`negative.log`、`gcccheck.log`、`compiler-controls.log`、`real-packs.log`、`privacy.log`；构建产物和日志均未提交。S31 报告另在 `build-s31/size.txt`、`size-components.txt`、`stack.txt`。反向对照使用临时源码副本，不依赖 Git 历史。
