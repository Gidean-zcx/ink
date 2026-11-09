# FW-F8 执行报告

基线：`50fd91821b47d6a9cacd42e7ba07fbd3b8084de4`。分支：`claude/codex-fw-f8`；实现提交：`ed86e983`。以下实测均为 macOS 模拟器，卡片与身份使用合成测试数据。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 四张实际内容按“合成卡乙、听、单词、合成卡甲”显示；两个进程共用 Flash 验证重启恢复，成人与孩子各自保存 | 下方 F8 命令；写入 129 项、重启 39 项断言 |
| 2 | 【实测】通过 | 真实长按打开 IconMenu；触摸“接着听”与 AI 都调用合成应用声明的同一能力；菜单开关确实加入卡片 | F8；`build/fw-f8-evidence/asan-final/` |
| 3 | 【实测】正反向通过 | 短按照旧打开应用，长按不触发短按；保留原图标绑定查询合同，两种设备回归通过；去掉短按绑定时恰好失败 1 项 | F8、反向命令 |
| 4 | 【实测】通过 | 两页结构化快照均装入 3072 字节 AI 缓冲；触摸与 AI 共用能力；孩子的卡片和导航选中位置均不受成人影响 | F8、反向命令；最终 ASan 定向 6/6 |
| 5 | 【实测】未完全通过 | 普通全量与 ASan 全量各 238 通过、1 项文档校验失败、3 项缺依赖跳过；无 ASan/UBSan 内存错误。最终修正后 ASan 定向 6/6。产品代码新增 294 行，截图已留存 | 全量、ASan、差异统计；见下方证据 |

## 没做到的

- `capability-table-doc` 未通过：新能力需要重新生成共享的 `docs/system/capabilities.md`，总规则第 7 节明确本任务不改它。候选文件为 `build/fw-f8-capabilities.md`，独立构建目录中对 369 项定义的生成校验通过。不能据此声称正式全量全绿。
- `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` 被既有脚本跳过：当前 Python 缺少 pytest、pytest_asyncio、aiohttp、yaml，返回码 77。未删测试或改跳过条件，也未安装或升级依赖。
- 没有 Linux 实测；本机 clang 开启 `INK_WERROR=ON`。最终参数组装使用局部缓冲，避免结构体字段互作 snprintf 来源与目标。

## 和设计不一致的

【实测】TodayEdit 为 **45.104/255**，IconMenu 为 **46.353/255**，均超过 12。对照图和分数在 `build/fw-f8-evidence/compare/`；最新 ASan 截图复测得到相同分数。

【实现】TodayEdit 复用分页列表，增加“第几张”和“上移”，行间距、描边、固定读书行与画板不同；卡片内容来自应用，不硬编码画板示例。IconMenu 按任务只从“全部”页打开，画板底图是主屏；控件采用现行规范尺寸和灰度，面板为完整容纳关闭按钮而更高。已通过断言和看图检查文字重叠与关闭按钮裁切。

【实现】编辑入口在今日页标题；按本任务只改今日和全部页的边界，没有接设置列表入口。

## 改过的已有测试

仅扩展 `tests/integration/ai_surface.c` 的五项精确期望，继续检查真实注册、AI 暴露和必填参数；未放宽断言或预算。

| 新增期望 | 必填参数 | 原因 |
| --- | --- | --- |
| `shell.today_edit` | 无 | 打开编辑页 |
| `shell.icon_menu` | `id` | 指定应用菜单 |
| `shell.today_card` | `card` | 选择或上移，`up` 可选 |
| `shell.today_prev` | 无 | 候选上一页 |
| `shell.today_next` | 无 | 候选下一页 |

## 要用户定的

无。

## 顺手发现

【实测】既有 JSON 解析上限为 128 token。给今日快照额外加动作时，原卡片测试发生越界；最终撤掉额外条目，保留编辑能力与触摸入口，原卡片测试通过。初次诊断日志留在 `build/fw-f8-evidence/initial-cards-sanitizer.log`。

## 新增的机制

- 成员集合 `home.today`：F8 任务明确要求保存选择和顺序；沿用现有数据服务和 JSON 记录，至多四个应用卡片标识。
- 没有新增任务、线程、后台重试、缓存、全局配置项或底层文件格式。

## 要改的文档

合并时更新 `docs/system/ui-dependencies.md` 的 TodayEdit、IconMenu 状态，并重新生成 `docs/system/capabilities.md`；本任务按总规则不改这两份共享文档。设置列表可接入已提供的 `shell.today_edit`。

## 为什么不能更小

【实测】产品代码新增 294 行、删除 8 行：navigation.c 新增 289 行，CMake 接线 5 行。需要覆盖成员读写、卡片匹配、选择排序、两页快照和短长按分流；复用已有数据服务、卡片提供者、分页及能力总线。考虑过只存四个应用 id（不能选择同应用的不同卡片）、只存内存（不能重启恢复）、只给 AI 排序（人不能排序），均不满足验收。新增函数追加在 navigation.c 末尾，已有函数仅作接线。

## 架构审视

【实现】选择属于成员数据，页面只维护视图；五项 shell 能力与应用原有卡片动作同时供按钮和 AI 调用，不直接改应用服务状态。导航位置在新建编辑页时清空，数据仍从当前成员集合读取。无新增服务或通用框架。

## 验证命令和证据

构建按总规则第 5 节指定依赖配置，使用 `taskpolicy -c utility` 和 `-j2`；内存目录为 `build-asan`，额外开启 `INK_SANITIZE=ON`，保留 `INK_WERROR=ON`。

```sh
F8_CTEST="$INK_DEPS/build-deps/pyenv/bin/ctest"
F8_PYTHON="$INK_DEPS/build-deps/pyenv/bin/python"
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$F8_CTEST" --test-dir build -j2 --output-on-failure -R '^(fw-f8|ui-cards|ui-scale|ui-snapshot-regressions-.*|integration-ai-surface)$'
taskpolicy -c utility "$F8_CTEST" --test-dir build -j2 --output-on-failure
taskpolicy -c utility "$F8_CTEST" --test-dir build-asan -j2 --output-on-failure
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$F8_CTEST" --test-dir build-asan -j2 --output-on-failure -R '^(fw-f8|ui-cards|ui-scale|ui-snapshot-regressions-.*|integration-ai-surface)$'
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$F8_PYTHON" tests/fw_f8/reverse.py --build build
taskpolicy -c utility "$F8_PYTHON" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/fw-f8-evidence/compare --scenes TodayEdit IconMenu --reuse
```

- 【实测】5/5 反向对照得到预期失败：忽略映射 12 项、共用成员集合 5 项、保留成人导航位置 1 项、吞短按 1 项、替换应用快捷动作 2 项。仅编译替换对象，产品源文件哈希不变；见 `build/fw-f8-reverse/summary.json`。
- 【实测】最终 F8 写入 129 项、重启 39 项断言通过；内部 RAM 最小余量 47736 字节，原预算 32768 字节未放宽。最终导航修正后的 ASan 定向为 6/6；ASan 全量完成于该修正前，其后按影响范围补验。
- 全量日志：`build/fw-f8-evidence/full.log`、`build/fw-f8-evidence/asan-full.log`；最终定向与内存指标：`build/fw-f8-evidence/asan-final/`；截图：`build/fw-f8-evidence/write/`、`build/fw-f8-evidence/compare/`。
- 【实测】结束差异检查仅含本任务 8 个文件，`git diff --check` 通过；共享文档、历史调研、设计稿与构建产物均未提交。按规则扫描基线到最终改动，隐私匹配为 0（grep 返回 1，表示无匹配），证据为 `build/fw-f8-evidence/privacy.txt`。

## 复查后的修复

复查依据：主会话 2026-11-09 的 `REVIEW.md`，仅有 FW-L1，无严重、高或中级未结项。

- 【实现】FW-L1：删除今日标题上的透明点击区域；“显示和亮度”新增可见的“今日屏放什么”列表行，按钮与 AI 复用 `shell.today_edit`，设置页快照列出同一能力。未新增机制。
- 【实现】扩展 F8 场景：从 Settings 点进显示设置，再点可见列表行进入 TodayEdit；核对能力绑定、行高和完整可见，遍历今日页确认无编辑入口绑定，并实点标题确认不跳页。反向对照分别恢复透明区域、断开设置行绑定。
- 【实测】普通定向 10/10 通过（F8、卡片、字号、两种设备快照、AI 暴露、中枢及其夹具）；反向对照 7/7 得到预期失败，恢复透明入口失败 25 项、断开设置行绑定失败 21 项。截图确认新行完整可见。日志：`build/fw-f8-evidence/review-focused.log`、`review-reverse.log`；截图与内存记录：`build/fw-f8-evidence/review/`。
- 【实测】上次三项中枢跳过是 Python 环境选错：只读依赖目录现有 `hub/.venv` 包含 Python 3.11.15、pytest 8.4.2、pytest-asyncio 1.2.0、aiohttp 3.12.15、PyYAML 6.0.3。改用 CMake 的 `INK_HUB_PYTHON` 并令 `PYTHONPATH="$PWD/hub"` 指向本分支，三项已全部通过；不安装或升级依赖。
- 验证进行中：普通全量、ASan/UBSan 全量与最终隐私检查待填写。当前对原任务基线统计产品代码新增 297 行、删除 8 行，未超过 300 行预算。
