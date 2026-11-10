# small-gaps-2 执行报告

基线：`e65bfc261b68e9fb063fc60da507122119ae4fc6`。仅在任务分支本地提交，不推送；未接触设备。执行中，下面的待验证不表示完成。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1.1 继续读 | 【实测】定向测试通过 | 标题、按正文当前页起点字节比例显示百分比；停在今日屏插拔卡自动刷新；跨重启、成员隔离保留。`work/small-gaps-2/reader-after.log` | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/reader_continue.py --build build` |
| 1.2 全部页图标 | 【实测】通过 | 给三份注册声明补 `.icon`；复用现有 SVG、许可与构建检查。改后 19 条检查通过；截图 `work/small-gaps-2/before/ui/icons/`、`build/test-out/small-gaps-2/icons/`；最终反向对照待补 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/small_gaps_2/run.py --build build --case icons` |
| 1.3 拨号页 | 【实测】通过 | 提示移到拨号按钮左侧，100%/120% 下均与 12 个原尺寸数字键不重叠；改前失败 6 条，改后 94 条通过 | 同上，`--case phone`；`work/small-gaps-2/phone-after.log` |
| 1.4 飞行模式 | 【实测】通过 | 控制面板关闭飞行模式后收起；AI 的“关闭飞行模式”错误面板也收起；仅操作成功时关闭，失败走原拒绝提示。原测试与新增测试 2/2 通过 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^(airplane-mode|small-gaps-2-airplane)$' --output-on-failure -j2` |
| 1.5 日期时间选择 | 待实现、验证 | — | — |
| 1.6 计算器 120% | 待检查、验证 | — | — |
| 2 全量、内存、S31、gcccheck | 待验证 | — | — |
| 3 改前改后截图 | 待生成 | — | — |

## 没做到的

- 执行中。

## 和设计不一致的

- 待核对。

## 改过的已有测试

- `tests/reader_continue.c`：增加百分比、标题、插拔卡不离开今日屏的断言与截图；原页数、重启和成员隔离期望保留。`read_to` 改为格式化页码，增加第 20 页的非零百分比用例。
- `tests/airplane_mode.c`：增加成功关闭后不再停留 `AiOffline` 的断言，未修改原断言。

## 要用户定的

- 暂无。

## 顺手发现

- 暂无。

## 新增的机制

- 任务 1：复用原 `reader.last`，增加可选 `offset` 字段以保留百分比；旧记录仍能读取。没有新文件、新定时器或任务。
- 任务 1：阅读卡片提供者订阅现有 `storage.card` 状态，复用原卡片版本事件和刷新节流。

## 要改的文档

- 暂无；遵守共享文档不改的规则。
