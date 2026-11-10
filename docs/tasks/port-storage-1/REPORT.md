# port-storage-1 执行报告

基线：`1450591caecfffdff023b19dc057fad5174a050f`。只编译和宿主机验证，不连接设备，不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | S31 构建退出 0，警告 0；端口普通版与 ASan 均 13/13；反向对照 8/8 检出 | `bash ports/esp32s31/build.sh`；`ctest --test-dir build-s31-host -j2`；`ctest --test-dir build-s31-host-asan -j2`；`python3 ports/esp32s31/tests/storage_negative.py build-s31-host` |
| 2 | 待验证 | 模拟器全量、gcccheck、隐私扫描 | 待补 |
| 3 | 【实测／实现】通过宿主机核对，未上机 | 外部包 4,718,142 B；内嵌包 549,358 B；空卷可用 401,408 B；原厂与脚本说明见下 | `python3 tools/ui_assets_check.py build/assets`；`build-s31-host/test-s31-storage-rw` |

## 没做到的

- 未做实机验证；任务明确禁止设备操作。

## 和设计不一致的

- 待核查。

## 改过的已有测试

- 暂无。

## 要用户定的

- 暂无；本任务按已确定闪存布局实施。

## 顺手发现

- 工作区起始干净，HEAD 与指定基线一致。

## 新增的机制

- 任务要求的两包资源格式、LittleFS 块设备和只读卡编译开关；实现后补充细节。

## 要改的文档

- 若涉及共享文档，列在此处交主会话处理。

## 里程碑

- 已接入真实资源和存储 HAL，仅删除对应桩。资源查找不复制进堆；使用同一份 LittleFS；卡挂载、容量和通知由 SDMMC/FAT 返回。
- 普通端口与 ASan 的原话均为 `100% tests passed out of 13`。8 项反向对照分别撤掉几何约束、只读打开保护、SD 写命令保护、通知、两个包的查找、包预算和备份哈希核对，全部失败；两个未改动的读写模式基线成功。
- 模拟器全量与全量内存检查进行中，最终结果待补。
