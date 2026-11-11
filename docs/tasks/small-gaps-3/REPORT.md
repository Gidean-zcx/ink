# small-gaps-3 执行报告

日期：2026-11-11。基线：`a108101cbb81bd4a0b4785d66f0ab3a4fcb32003`。仅宿主机测试、模拟数据和 S31 离线编译；不碰设备，不对外发布。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| G5 | 正向与反向通过 | 【实测】头像走 `shell.member_select`；AI 拒绝、过期点按拒绝 | `ctest --test-dir build -R '^semantics-shell-member$' --output-on-failure -j2` |
| G9 备份 | 正向与反向通过 | 【实测】每天／每周到期；未到期、未充电、LTE、活跃时均不跑；成功结果显示 | `ctest --test-dir build -R '^small-gaps-3-backup$' --output-on-failure -j2` |
| G9 更新说明 | 正向与反向通过 | 【实测】44 段合成说明分 12 页，拼回后和原文逐字相同；无说明不显示 | `ctest --test-dir build -R '^small-gaps-3-(notes|empty-notes)$' --output-on-failure -j2` |
| G10 | 正向与反向通过 | 【实测】离线点开原创 CC0 示例、跳过、真实模拟 AI 请求带这一页正文 | `ctest --test-dir build -R '^small-gaps-3-sample$' --output-on-failure -j2` |
| 综合验证 | 未运行 | 全量、ASan、能力表、S31、gcccheck、ui-scale、隐私 | 待运行 |

## 没做到的

综合验证尚在运行。

## 和设计不一致的

【实现】按任务书在 BootHello 保留引导页，用“看看示例”打开已有分页阅读器；没有直接把引导页替换成正文。未修改设计合同。

## 改过的已有测试

`tests/semantics_shell/member.c` 加头像能力与 AI 权限断言；`tests/integration/ai_surface.c` 为新增能力登记开放／禁止原因；`ports/esp32s31/tests/asset_pack_test.py` 加示例打包断言。没有放宽或删除已有断言。

`tests/i18n_lang.c:185`：英文首启页的按钮期望由 `Bookshelf` 改为 `Read a sample`，因为任务书 G10 明确将该入口改成“看看示例”；仍同时检查英文标题和跳过按钮。

## 要用户定的

暂无。按任务书选择最小、可逆实现。

## 顺手发现

【实测】初版自动调度错误地要求中枢已经在线；现有中枢空闲会断开，导致到点也不启动。已移除此先决条件，由既有备份作业按需连接。新测试的长说明解析器容量不足曾越界，已在宿主调用中用完整 JSON 解析器读取；不是产品崩溃。

## 新增的机制

配置项 `backup.auto`（关／每天／每周，默认关）：任务书 G9 明确要求。复用现有设置持久化、`B.retry` 定时节点、`B.job` 和 `backup.now`；无运行期新任务、线程、状态机、文件格式或框架。既有应用框架增加 `ink_app_asset_read`，供 G10 读取资源包并保持 HAL 边界。

## 要改的文档

共享能力表由合并会话同步：新增 `shell.member_select`（仅人）、`reader.open_sample`、`shell.update_notes_page` 和 `backup.auto`。本任务在 `build/small-gaps-3-capabilities.md` 生成副本并 `--check`，【实测】534 definitions。共享界面依赖的 G5／G9／G10 状态，以及中枢 `update/check` 的可选 `notes` 字段也需同步；本任务不改共享文档。

## 实现边界与截图

【实现】自动备份按设备设置周期，只处理当前仍可访问的成员空间；关闭或过期的成人空间不绕过验证。有历史备份时沿用上次成功时间；无历史备份时从本次首次检查计时（此起点不另写持久文件，重启会重算）。定时节点空闲时每分钟检查，条件不满足先等待。

【实现】更新说明读取模拟协议的可选 `notes` 字段，全长保留在 PSRAM，页面按实际文字高度和 UTF-8 边界分页；缺字段没有说明区。没有新增跨重启说明存储。原创短文 `assets/sample/welcome.txt` 随既有资源包离线提供，不导入成员文件；复用阅读器的正文快照和阅读位置。

【实测】截图已查看：`build/test-out/small-gaps-3-notes/device/snap-notes-first.png`、`snap-notes-last.png`；`build/test-out/small-gaps-3-sample/device/snap-boot-hello.png`、`snap-sample-first.png`；备份截图 `build/test-out/small-gaps-3-backup/device/snap-automatic-backup.png`。

## 里程碑一：功能和反向对照

【实测】`tests/small_gaps_3/reverse.py --build build --baseline a108101cbb81bd4a0b4785d66f0ab3a4fcb32003` 四组旧源码均编译和链接成功，测试均退出 1：成员 2 条、备份 18 条、更新说明 4 条、示例 5 条失败断言。仅替换对应产品编译单元，不改工作区，也不让注册进 CTest 的用例依赖历史。证据 `build/small-gaps-3-reverse/summary.json`、`work/small-gaps-3/reverse.log`。无说明本来就应无说明区，和有说明场景成对验证。

【实测】定向正向用例分别通过；`ui-scale: 51 files, 0 violations`。首次 gcccheck 为 `10 compiled, 0 with findings`，最终综合检查仍需重跑。

## 规模与架构审视

【实现】产品源码（`apps/`、`system/`、`simulator/`、`tools/`）新增 211 行、删除 38 行，净增 173 行；另有 CMake 12 行、英文词条 8 行、原创资源 10 行，合计仍低于 400 行预算。三项缺口合计超过 100 行的原因是调度条件、UTF-8 分页和资源加载都要落到真实链路。更小方案已看过：头像继续直接调服务不满足 G5；定时另起线程违反任务书；说明截到小缓冲不满足“完整”；把测试书当产品示例不满足资源许可与打包。最终复用既有机制。

【实现】成员服务仍拥有验证和身份，成员门只通过人用能力发起选择；设置服务拥有周期，中枢和备份服务拥有业务执行；更新服务保留说明，界面负责分页；资源包和阅读器提供示例。按钮、AI 共用设置、说明翻页、读示例能力，唯一新增人用能力不向 AI 暴露；PIN 和本人确认规则不变。页面快照包含当前可见说明或示例正文，未开放私有成员空间。通过现有应用框架增加一个只读资源读取接口，阅读器不直接引用 HAL；没有引入模拟器头文件到产品层。

原创示例为 1080 字节，SHA-256 `c076341606cce24765c7a3fc946776c75147a5207bb1ef2f196c4a6c57f92e90`，正文内明确 CC0 1.0。原资源预算未放宽，打包为 font_data 4718252 字节、embedded 576672 字节，样本在包内。

## 里程碑二：全量发现的接入问题

【实测】首轮普通全量原话 `99% tests passed, 4 tests failed out of 368`，843.41 秒。除禁止改写的共享能力表外，发现英文旧按钮期望、阅读器直连 HAL、备份首页快照超过既有小解析器容量三项。已更新明确变更的英文期望、通过既有应用框架读取资源、去掉备份首页未显示的明细字段（保留成员、状态、日期和周期）；没有删测试或放宽阈值。第二轮这三项已分别通过，其余仍在执行。
