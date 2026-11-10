# audit11-fix 报告

基线 `4099f390`，分支 `claude/system-layer`，改动未提交。只修 B11-01、B11-02。

（Codex 额度用完后由 Claude 子代理完成；运行环境不让子代理写报告文件，本文由主会话按它的回报原样保存。）

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| B11-01 | 已修 | 【实测】`audit11-b11-01`：清理时 `/content/SYNTH_0010.txt` 的一次状态查询失败，本次清理停下、不写完成标记、保留脏标记，编号 30 保留；再插一次卡后清理做完，40 条、编号 30；重启后仍 40 条、编号 30。撤掉修复后同一测试失败 3 条（编号被删，重插卡后拿到新编号）。 | `ctest -R audit11-b11-01` |
| B11-02 | 已修 | 【实测】`audit11-b11-02`：清理批次之间经 AI 能力 `file.index` 查空目录 `/family/SYNTH_EMPTY`，返回空列表，状态不变成“建好”，之后清理做完，40 条；重启后仍 40 条。撤掉修复后失败 3 条：查询后立刻显示建好，清理没做完，最后 57 条（与审计一致）。 | `ctest -R audit11-b11-02` |
| 1 既有回归 | 通过 | 【实测】index-prune、index-compact、index-durable、file-index-3、file-index-regressions、storage-usage 原样通过 | `ctest -R '^(audit11\|index-prune\|index-compact\|index-durable\|file-index-3\|file-index-regressions\|storage-usage)'` |
| 2 全量 | 通过 | 【实测】`100% tests passed out of 303` | `ctest --test-dir build -j3` |
| 2 S31 | 通过 | 【实测】`ports/esp32s31/build.sh` 退出 0 | 同左 |
| 2 GCC | 新增行无发现 | 【实测】只有 `hal_storage_sim.c:254` 一条，基线就有（换回基线文件同样报），只在 RISC-V 的 `uint32_t` 宽度下出现，和本次无关 | `gcccheck.py build <文件>` |
| 3 预算 | 符合 | 【实测】产品代码净增 23 行（文件服务 +8/-3，存储 HAL 头 +2/-1，模拟器 +16，S31 空实现 +1），上限 80 | `git diff --numstat -- platform ports system` |

## 修法

- B11-01：存储 HAL 加 `hal_fs_exists`（1 在、0 不存在、-1 出错），模拟器用 `stat`/LittleFS 的错误码区分，S31 空实现返回 -1。清理只删“确认不存在”的记录；查询出错时把清理游标归零、返回读写错误，这次清理停下，不写完成标记，下次（插卡、编辑、刷新、重启等原有触发）从头重查。
- B11-02：集中改 `index_done`：按目录的扫描在所有根都扫过、且清理还没做完时，不再清 `saving`、不取消后台定时器，并补一次后台调度。只有后台清理走到尾、落盘后才算建好。没有改任何调用方。

## 没做到的

- 内存检查（ASan/UBSan 全量）没跑：本机没有已配置的 `build-asan`。
- 状态查询出错后，同一次开机里不会自动重试，要等下一个原有触发（插卡、文件改动、刷新、重启）。任务书和审计要求不加后台重试。
- 没在实机上验证；S31 的 `hal_fs_exists` 是空实现（和其它文件接口一样）。

## 改过的已有测试

无。新增 `tests/audit11_fix/`（`boundary.c`、`device.c`、`run.py`）和 CMake 里的 `sim-audit11-fix`、`audit11-b11-01`、`audit11-b11-02`，不依赖 git 历史。

## 新增的机制

- `hal_fs_exists`：存储 HAL 新增一个查询函数，是审计和任务书给的修法之一。没有新任务、状态机、文件格式或配置项。

## 顺手发现

- `ports/sim/hal/hal_storage_sim.c:254` 用 `%x` 打印 `uint32_t`，RISC-V GCC 报格式不匹配；Linux x86 上不报。基线就有，没修。

## 要改的文档

- 索引清理的验收状态可补上“状态查询出错不删、下次重查”和“局部查询不结束整卡清理”两条。
