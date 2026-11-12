# almanac-2 执行报告

基线：`ed4645db28e1339bca7338ca69e42990cf5a6f30`。分支：`claude/codex-almanac-2`。状态：进行中。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 原文与逐日对照 | 【实测】通过 | `--check` 通过；36,525 天全部字段一致 | `ctest --test-dir build -R almanac-oracle --output-on-failure` |
| 2 固定 50 天与反向对照 | 【实测】通过 | 50 天通过；仅篡改 2026-02-17 的宜，原测试退出 1 | `ctest --test-dir build -R 'almanac-(fixtures\|reverse)' --output-on-failure` |
| 3 cnlunar 对照 | 【实测】已列差异 | 50/50 天有差异，完整表见 [CNLUNAR-DIFF.md](CNLUNAR-DIFF.md) | `python -B tests/almanac/run.py --binary build/almanac-model --mode compare --cnlunar "$INK_DEPS/../ink-hw/upstream/oss/cnlunar" --out docs/tasks/almanac-2` |
| 4 现有农历逐日对照 | 【实测】已列差异 | 36,495 天一致，30 天不一致，全部日期见 [LUNAR-DIFF.md](LUNAR-DIFF.md)；没有修改旧实现 | `python -B tests/almanac/run.py --binary build/almanac-model --mode oracle --out docs/tasks/almanac-2` |
| 5 AI、孩子、范围与时区 | 待验证 | — | — |
| 6 构建、全量、ASan、S31、资源 | 待验证 | — | — |

## 没做到的

尚在执行。

## 和设计不一致的

待核查。

## 改过的已有测试

无。

## 要用户定的

完整原文静态表的交叉编译对象约 170 KB，只读数据已超过 60 KB 的增量预算；待实际固件前后对比。若确认超限，按任务书停止，不删内容、不引入压缩。

## 顺手发现

无。

## 新增的机制

任务书要求：离线静态历法表、只读 `time.almanac` 能力。没有新增任务、线程、状态机、持久格式或配置项。

## 要改的文档

任务书明确允许生成并提交 `docs/system/capabilities.md`；其它共享文档不修改。

## 接口与证据边界

【实现】`ink_almanac_date` 与 `time.almanac` 已接入，主工程构建和能力测试待完成。不操作设备，不推送远端。

【实测】宿主机 1,000,000 次不同日期查询，平均 531.0 ns/次；这是 Mac 宿主测试，不代表 S31 真机耗时。
