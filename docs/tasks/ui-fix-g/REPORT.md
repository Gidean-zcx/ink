# ui-fix-g 执行报告

基线：`943e8d9f44a3ecfb6cbafa6778388c6ef92920bf`。状态：页面修复、基线对照和普通全量收尾完成，ASan/UBSan检查进行中。以下 `PY="$INK_DEPS/build-deps/pyenv/bin/python"`。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| V06 | 【实测】失败保留“再扫一次”，重扫仍走原MEDIA执行器 | `after-scan-failed-{100,120}.png`；失败重扫断言 | `taskpolicy -c utility "$PY" -B tests/ui_fix_g/run.py --build build --screens` |
| V07 | 【实测】现有 `remote.open` 打开网址；完整复制、UTF-8静态分页、按键/AI同路 | `after-scan-url/long-first/long-last/limited-{100,120}.png`；含长文拼回校验 | 同上；`scan-100/120` |
| V08 | 【实测】显示网络/联系人/号码和具体动作，去掉统一问句，密码不进快照 | `after-scan-wifi/contact/tel-{100,120}.png` | 同上；原确认策略未改 |
| S27 | 【实测】保留三项电源操作及危险主次；诊断单列分组，按钮间19，底边38，显示真实回执 | `after-power-menu/recorded/exported-{100,120}.png` | 同上；`menu-100/120` |
| S28 | 【实测】关机图按实际文字高度容纳长书签，不裁字 | `after-power-off-{100,120}.png`；反向120%高度268而所需370，断言失败 | 同上；`off-100/120` |
| S29 | 【实测】统一“充上电再开机”；保留已存好/未存好区别 | `after-power-empty/unsaved-{100,120}.png` | 同上；`battery-empty/unsaved` |
| S30 | 【实测】低电状态图标去按钮圆框；保留电量和既有继续入口 | `after-battery-low-{100,120}.png` | 同上；`battery-low` |
| S31 | 【实测】保持保存中与已关机阶段区分 | `after-battery-saving-{100,120}.png`；既有power-pages回归 | 同上；`battery-saving` |
| S32 | 【实测】保留静态电量、充电和按键唤醒说明 | `after-charging-{100,120}.png`；未改刷新周期 | 同上；`battery-charging` |
| Z06 | 【实测】孩子卡同时显示受影响动作和请家长处理，仍无关飞行权限 | `after-airplane-adult/child-{100,120}.png` | 同上；`airplane-100/120` |
| Z07 | 【实测】去掉静态头像/闹钟按钮式圆底，补返回原页提示；未具体命名暂停任务 | `after-interrupt-call/alarm-{100,120}.png`；打断关闭后原活动不变 | 同上；`interrupt-100/120` |
| 常规与120%前后截图 | 【实测】完成 | [截图索引](SCREENSHOTS.md)：21种状态、84张图，最大24086字节，共1412858字节 | `run.py --before 943e8d9f44a3ecfb6cbafa6778388c6ef92920bf --screens`；正向同上 |
| 针对性测试及反向对照 | 【实测】正向20组586条断言；基线20组完整结束、35条预期失败 | [结果摘要](page-test-results.json)；每个被改产品文件至少一档反向失败 | `run.py --reverse 943e8d9f44a3ecfb6cbafa6778388c6ef92920bf` |
| 普通全量 ctest（含 ui-scale） | 【实测】未全绿；首轮5失败，3项复验已过，剩2项登记门禁 | 原话：`99% tests passed, 5 tests failed out of 413`；`ui-scale`通过；复验原话：`100% tests passed out of 3` | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；复验加 `-R '^(i18n-services\|integration-power-menu\|unit-s31-uhci-negative)$'` |
| 内存检查、S31、gcccheck | 待核查 | 运行范围和结果待补 | 待记录 |
| GCC检查 | 【实测】12个文件无发现 | 产品6个、专用C测试6个，各为 `gcccheck: 6 compiled, 0 with findings` | `taskpolicy -c utility "$PY" "$INK_DEPS/../ink-codex/tools/gcccheck.py" build <6个产品C文件>`；测试用对应编译参数 |
| S31 | 未编固件；【实测】已跑主工程S31宿主测试 | 首轮UHCI归还缓冲断言失败，单独复跑通过，未改该目标；没有设备验证 | 上述全量及复验命令 |
| 范围、行数和隐私检查 | 范围已核对；隐私待最终提交检查 | C文件新增98行、删除26行，共124行；英文表另增7行；合计131≤250行 | `git diff --numstat` |

## 没做到的

- 普通全量仍有能力生成目录、公共AI清单两项门禁未通过，见“要改的文档”。3项中枢测试由现有环境条件跳过：`hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`，本任务未加跳过逻辑。
- V06未在失败结果页新增取景框或持续识别；已按任务书保留重扫路径并沿用相机会话。Z07未显示被暂停活动的具体名字，未新增恢复队列。
- S11、S12、S13、S16由 `lock-faces-1` 处理，本次未碰 `lock_mark.c`；S14、S15、S17、S18涉及活动卡、控制中心和任务列表，超出本次文件范围，未改。S30继续按钮保留原“知道了”文案。
- 未编S31固件、未进行真机显示/唤醒验证。

## 和设计不一致的

- 审计V06建议失败保留取景框，Z07建议命名具体暂停活动；本任务仅完成上述结果重扫与返回原页提示。其余低优先项按上节列明，未改设计文档迁就实现。

## 改过的已有测试

- 暂无。

## 要用户定的

- 暂无。

## 顺手发现

- 【实现】翻译表在CMake配置时复制到构建目录，改译文后需重新配置。本任务未改构建机制。
- 【实测】新增英文提示的换行重叠、统一锁屏控件引起的旧触摸回归已修正，既有 `i18n-services`、`integration-power-menu` 复验均通过，没有改其断言。
- 【实测】未改的S31 UHCI负向测试首轮报 `StartReceive` 的 `!counts[N_HELD]` 断言，单独复跑通过；原因未查清，不能据一次复跑宣布消除。相关驱动和宿主测试均无本任务差异。

## 新增的机制

- 无；本任务不新增线程、状态机、文件格式或配置项。
- 分页沿用现有 UI 执行器和能力分派；新增 `camera.scan_next`、`camera.scan_prev` 两个页面原子动作，是任务明确要求的静态分页。按钮、实体键和AI走同一能力。

## 为什么不能更小

- 六个文件合计124行产品差异。扫码82行涵盖字体测量分页、UTF-8边界、按钮/按键/AI同路、网页现有能力及受限入口；其余42行用于五个电源/打断页面。看过只删“没有浏览器”提示、固定字数截断、按钮直接改页码的更小方案，分别不能提供网页入口、会裁掉中文/内容、绕过原子能力，未采用。两个文本缓冲使用既有 `INK_PSRAM_BSS` 标记。

## 要改的文档

- `docs/system/capabilities.md`：主会话统一运行 `python3 tools/capabilities.py`，收录 `camera.scan_next`、`camera.scan_prev`。按本基线，能力数438→440、总定义476→478。总规则第7节禁止本任务改这个共享生成文件，普通全量的 `capability-table-doc` 因而报目录过期；运行时目录、确认策略检查已通过。未修改或跳过该测试。
- `tests/integration/ai_surface.c` 的 `FOR_AI` 表：登记 `{"camera.scan_next", NULL}` 和 `{"camera.scan_prev", NULL}`，保持“全部能力必须且只能列在一张表里”的现有断言。该公共测试不在本任务写入范围，尚未修改；全量首轮仅这两个新增能力触发其两条断言。
