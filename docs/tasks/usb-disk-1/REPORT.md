# usb-disk-1 执行报告

基线：`2adfe5b52f06d93e119919d24fe71027cacfc2ca`。执行日期：2026-11-12。当前阶段：主工程、S31、内存和 GCC 检查均完成；最终提交的固件复编待收尾。未接触设备。来源和许可见 [SOURCE.md](SOURCE.md)。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a S31 宿主机测试、新测试与反向对照 | 【实测】最终 79/79 通过，114.73 秒 | `100% tests passed out of 79`；USB 未改版本退出 0，11 个撤销版本均编译成功、运行断言退出 -6 | `ctest --test-dir build-s31-host --output-on-failure -j2` |
| 1b 主工程普通全量测试 | 【实测】434 通过、3 跳过、0 失败，1455.67 秒 | 原话：`100% tests passed out of 437`；`hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` 按既有环境条件跳过。首轮检出本次 CMake 边界问题，修复后 `autotest-build-switch` 在全量中通过（3.17 秒），未改原断言 | `ctest --test-dir build --output-on-failure -j2` |
| 1c S31 固件编译、警告与大小 | 【实测】编译通过，退出 0；无新增警告 | `ink-s31.bin` 为 4,320,096 字节；IDF image size 4,319,680 字节；应用槽余 18%；编译器警告 0；隔离导出基线重新配置，6 条配置警告逐条相同、新增 0；ELF 中 5 个 USB HAL 与弹出回调均为实际代码符号 | `taskpolicy -c utility bash ports/esp32s31/build.sh`；基线 `idf.py --preview … reconfigure` |
| 1d 内存与 GCC 检查 | 【实测】ASan/UBSan 3/3，0.48 秒；GCC 无发现 | USB HAL、读写存储、只读存储；`gcccheck: 1 compiled, 0 with findings`，未跳过文件；真实 USB HAL 另已由 S31 GCC 编过 | `cmake -S ports/esp32s31/tests -B build-s31-asan -DINK_SANITIZE=ON`；`ctest --test-dir build-s31-asan -R '^unit-s31-(usb|storage-(rw|ro))$' --output-on-failure -j2`；`python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build-s31-host ports/esp32s31/tests/usb.c` |
| 2 上机核对清单 | 已列；实机项待主会话执行 | 本任务未插设备、烧录、读串口 | 见报告末尾 |

## 没做到的

- 电脑枚举、真实卡读写和拔线恢复均未上机验证，不能写成“真机已支持”。ASan/UBSan 检查的是宿主机 HAL 和 LittleFS，未运行真实 USB 控制器或真实 TinyUSB 任务；按 vendor-common 第 7 条没有跑主工程全量 ASan。
- Linux 全量运行环境未提供；已有测试在 macOS 运行，GCC 辅检和 S31 固件 GCC 构建通过。
- 三个中枢测试跳过的原因：缺少按锁定依赖安装的 `hub/.venv` 环境，JSON 交叉检查所需的中枢样本也未生成。

## 和设计不一致的

- 系统服务、公开 HAL 和原子能力未修改。原厂先把 MSC 挂到 APP 再交 USB；本适配在原有普通 FAT 卸载后直接创建 USB 归属，在退出时恢复原存储挂载。没有移植原厂 UI、请求队列和工作状态机；这是接入既有借卡服务的差异，只有宿主机和编译证据，实机待核。
- MSC 事件只置标记，复用每秒 SD 事件在平台事件任务中通知系统已有的归还流程。原厂 GPIO36 和启停等待时序保留。具体适配见 SOURCE.md。

## 改过的已有测试

- 没有修改已有测试断言；只增加两个 ctest 项。新 USB 测试首次把连接回调固定当作第一条通知，实跑发现还有未连接时的初始通知，改为检查连接前后的回调次数差；反向对照增加未修改版本先通过的前置条件，防止误报。

## 要用户定的

- 暂无。

## 顺手发现

- `build.sh` 的文件模式是 100644，直接执行被系统拒绝；使用 `bash` 运行，没有改脚本或权限。
- 六条既有 IDF 配置警告来自 Wi-Fi/WPA 私有头文件（5 条）和 iot_eth/uart-eth-modem 私有头文件（1 条），与隔离基线完全一致；不在本任务修复。

## 新增的机制

- USB MSC 驱动及其原生 TinyUSB 服务任务（4096 字节内部栈、优先级 5、CPU0）：任务书第 1 项明确要求，参数沿用原厂。没有新增产品工作任务、队列、系统状态机、持久格式或后台重试。
- TinyUSB 的 MSC 开关与 8192 字节缓冲：原厂参数，任务书要求只开 MSC 必需项。
- 构建时获取两份固定哈希组件：任务书要求接入 TinyUSB；只写构建目录，原组件源码不改。

## 要改的文档

- 合并主会话统一更新功能清单 `usb_disk` / 移植队列 T02：增加“实现及宿主机验证完成，待上机核对”，暂不写“真机已支持”。本分支不修改共享文档。

## 规模与架构审视

- 【实现】产品文件物理新增 259 行、删除 1 行：USB HAL 209 行、main CMake 新增 46 行、USB 配置 4 行；扣除原厂逐字拷贝的 GPIO36 整函数 9 行，胶水 250 行，达到预算上限。其余原厂流程适配全部计入；测试和文档不计入。
- 为什么不能更小：单独填 USB 占位函数拿不到私有 SDMMC 卡句柄和互斥锁；另开一个 SDMMC 实例会和已挂载 FAT 冲突；修改存储文件或公开接口超出任务范围。同编译单元接入避免另建存储接口或状态机。
- 【实现】按钮和 AI 继续共用既有 `usb.mount` / `usb.unmount` 和 `usb.changed`；成员限制、确认规则、借卡释放和电源持有仍由原系统服务执行。没有新页面或独立操作入口。

## 复现与证据

以下在仓库根目录执行，环境沿用用户提供的 `INK_DEPS`；主工程按[总规则第 5 节](../codex-rules.md#5-构建和测试)配置，构建和测试并发均为 2，命令前统一加 `taskpolicy -c utility`。未启动模拟器窗口。

```sh
export PATH="$INK_DEPS/build-deps/pyenv/bin:$PATH"
export INK_UPSTREAM="$INK_DEPS/../ink-hw/upstream/Metalio-E-INK4-Plus"
taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-host -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python"
taskpolicy -c utility cmake --build build-s31-host -j2
taskpolicy -c utility ctest --test-dir build-s31-host --output-on-failure -j2
taskpolicy -c utility cmake -S ports/esp32s31/tests -B build-s31-asan -DINK_SANITIZE=ON -DPython3_EXECUTABLE="$INK_DEPS/build-deps/pyenv/bin/python"
taskpolicy -c utility cmake --build build-s31-asan --target test-s31-usb test-s31-storage-rw test-s31-storage-ro -j2
taskpolicy -c utility ctest --test-dir build-s31-asan -R '^unit-s31-(usb|storage-(rw|ro))$' --output-on-failure -j2
taskpolicy -c utility python3 "$INK_DEPS/../ink-codex/tools/gcccheck.py" build-s31-host ports/esp32s31/tests/usb.c
taskpolicy -c utility ctest --test-dir build --output-on-failure -j2
taskpolicy -c utility bash ports/esp32s31/build.sh
```

配置警告对照：`git archive 2adfe5b52f06d93e119919d24fe71027cacfc2ca` 导出到 `work/usb-baseline`，为该副本的 `build` 建指向已生成主工程资源的符号链接；复制其 `sdkconfig.defaults` 到独立的 `build-s31-baseline/sdkconfig`。加载 IDF v6.1 的 `export.sh`，从副本的 `ports/esp32s31` 运行：

```sh
taskpolicy -c utility python "$IDF_PATH/tools/idf.py" --preview -B "$usb_baseline_build" -DIDF_TARGET=esp32s31 -DSDKCONFIG="$usb_baseline_build/sdkconfig" reconfigure
```

其中 `usb_baseline_build` 是仓库内 `build-s31-baseline` 的完整路径；只配置，不编译或操作设备。归一化仓库路径后逐条比较配置警告，基线 6、当前 6，新增和删除均为 0。原厂只读克隆的 HEAD 未移动。

命令日志和构建产物只在忽略的 `work/`、`build*/` 下，未提交。本报告保留退出状态、测试总数和关键输出；源码、反向对照脚本、来源哈希和许可证均已提交。

## 上机核对清单

1. 主会话使用测试卡，插 Mac，从既有成人入口借卡；Mac 出现存储卡 U 盘，设备本地写卡被拒；系统区和 `/user` 不作为 USB 磁盘出现。
2. Mac 把内容明显为合成数据的 `usb-check.txt` 拷入卡的 `content/`（对应设备共享目录 `/family`），比较拷贝后的内容或哈希。成员自己的 `/me` 对应 `content/.m/<成员编号>/`。
3. Mac 正常弹出，设备恢复读写并能看到、读出新文件；USB 串口/JTAG 恢复枚举。
4. 再次借卡，在拷贝完成后直接拔线，设备自动归还并恢复读写；若 FAT 挂载失败，应报卡不可用，不能格式化。验证真实拔线事件而非仅注入回调。
5. 反复借出、归还，确认没有遗留 TinyUSB 任务或卡句柄；留意 USB/SDMMC 的错误和栈报告。上述步骤由主会话操作，本任务没有执行。
