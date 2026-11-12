# almanac-1：老黄历宜忌（离线，照开源库移植）

总规则先读 `docs/tasks/codex-rules.md`（第 9、10 节优先）和 `docs/tasks/vendor-common.md`（第 1～3、6、7、11、12 条）。

用户（2026-11-12）：“可以做，你自己研究有没有公开的 api。”主会话的结论：在线黄历接口要账号或收费，而且设备只连中枢（D-04）；成熟的离线开源库能直接算宜忌，优先移植。已固定两份只读副本：

- `$INK_DEPS/../ink-hw/upstream/oss/cnlunar`：OPN48/cnlunar，提交 `3f674fe`，MIT；`lock-faces-2` 的节气表已取自它。
- `$INK_DEPS/../ink-hw/upstream/oss/lunar-javascript`：6tail/lunar-javascript v1.7.8，提交 `5c92e62`，MIT。

## 要做的

1. 比较两家的宜忌：依据、算法、表的大小、两家结果的一致率（抽 2026 年全年对照）。选一家，把宜忌部分逐段移植成 C，放在 `system/services/time/` 下的新文件里；`SOURCE.md` 写清每段对应上游哪段、提交、许可、哈希。只移植宜忌及其依赖（干支、建除十二神、神煞等），不搬整个库。
2. 时间服务加查询（某天的宜、忌列表）和能力 `time.almanac`（参数日期，默认今天），人和 AI 共用。
3. 日历应用的日视图显示“宜 …… / 忌 ……”，各最多一行，放不下就省略；锁屏月历表盘不改。
4. 数据放只读区，报告写固件增加多少（应用分区只剩约 8%）。

## 范围和预算

- 只改：`system/services/time/**`（新增文件为主）、`system/apps/calendar/**` 里日视图那一处、`tests/integration/ai_surface.c` 登记新能力的那几行、新增测试、`assets/i18n/en.tsv`（只加新文案；宜忌词条的英文用通行译名或拼音，报告说明）、生成的 `docs/system/capabilities.md`、`docs/tasks/almanac-1/`。不碰别的任务在改的文件（`timer-seconds` 也可能改 `system/services/time/` 里计时器那几处）。
- 胶水不超过 200 行（移植的算法和表不算）；不新增常驻任务、文件格式。

## 验收

1. 测试：抽 2026 年 30 个日期，宜忌和所选上游完全一致（对照数据在宿主机用上游自己的实现生成，写清命令）；日视图显示；AI 能力和页面一致；反向对照。
2. 主工程普通全量 ctest 过；`ports/esp32s31/build.sh` 编译通过、无新警告，报告写固件大小。
3. 截图：日历日视图，常规和 120%（`docs/tasks/almanac-1/`，单张不超过 300 KB）。
