# lock-faces-2 执行报告

基线：`42e3b1f051650827bacb7638101d14bbf6b4bdc4`。日期：2026-11-12。实现、全部检查及本地提交已完成。**锁屏自动走时仍受文件范围限制，完整功能验收未通过。**

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a | 通过【实测】 | 选择页 289 条，六种表盘均由真实点按和 AI `settings.set` 选中，两个小组件开关可用 | `INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/lock_faces.py --build build` |
| 1b | 通过【实测】 | 分钟事件及像素 35 条：只改时钟矩形，同一分钟像素及版本不变 | 同上，`INK_LOCK_FACES_CASE=tick` |
| 1c | 未完成【实测／实现】 | 自然分钟、自然跨日：时间服务在锁屏时取消调度；独立探针明确失败，非超时 | 同上，`INK_LOCK_FACES_CASE=auto`；见“没做到的” |
| 1d | 通过【实测】 | 跨日、相同分钟换日及闰二月正向过；跨日反向到达预期断言 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/lock_faces_reverse.py --build build --case day` |
| 1e | 通过【实测】 | 换成员重建，孩子及关闭的成人空间无成人待办／阅读内容；本人验证后继续第 5 页 | 正向命令；反向 `--case member`、`--case reading` |
| 1f | 通过【实测】 | 原 343 条覆盖保留并扩展为矩阵 487 条；7 组反向均触发预期断言 | 正向与完整反向命令 |
| 2 | 通过【实测】 | 普通／ASan 全量各 414 项、0 失败；默认跳过的三个中枢测试均已补跑过；GCC 9 个文件无发现；S31 留给合并主会话 | `taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2`；内存检查换 `build-asan`；补跑见下文 |
| 3 | 通过【实测】 | [20 张截图](SCREENSHOTS.md)，含选择页、六种表盘的 100%／120%、成员隔离及英文 | 正向命令；英文加 `INK_LOCK_FACES_CASE=english` |
| 4 | 通过【实测／实现】 | 新增 1901～2100 年节气日期查询，完整 MIT 许可与来源哈希；2026 年 24 个日期独立对照过 | 正向命令中的 `solar_terms` 检查；反向 `--case solar` |

## 没做到的

- 自动分钟／跨日调度仍有范围外依赖：`system/services/time/time_service.c:197–210` 在锁屏时停止并取消唯一的分钟调度；没有公开恢复接口。本任务不允许改该文件，也不允许另加定时器，不能声称自动走时已完成。
- S31 固件编译和真机验证按任务书留给合并主会话；不接触设备。

## 和设计不一致的

【实现】范围内已接 `time.minute` 和 `time.changed`，后者覆盖相同分钟跨日及手动校时。上面的锁屏调度缺口须由合并主会话处理。

## 改过的已有测试

- 原测试用样式替身并期望新枚举被拒绝；现已移除替身，改验真实 `settings.set` 成功。
- `tests/settings_b.c` 只改锁屏选择段：原“三次点按按三档循环”改为“点按打开选择页且不改值，再逐一选六种表盘”。原壁纸解码、主页像素不变、旧三档显示及重启持久化断言全部保留。首轮全量在发现此旧合同冲突后停止，重新构建并验证该测试后重跑全量。
- 同文件锁屏段两处 `%u` 的图片编号显式转为 `unsigned`，兼容 GCC 核查工具中 `uint32_t` 为 `unsigned long` 的目标；没有改测试预期。
- `tests/integration/ai_surface.c` 只把新锁屏入口 `shell.lock_faces` 加入 `FOR_AI` 覆盖清单，保留原“每个能力必须有覆盖归类、能在工具目录发现且参数一致”的全部断言；未登记时全量确实报错。
- 保留原矩阵、成员隔离、继续阅读、23:59 到 00:00 的跨日判断；跨日改为时间服务事件处理后直接断言。测试驱动复用既有 `INK_NEED_AWAKE` 租约，避免睡眠使 `on_ui` 投递的检查等到其 20 秒上限后仍未执行。没有改产品的电源或睡眠行为。
- 身份验证切换会按约定废弃读取中的快照；检查在 5 秒虚拟时间内等一次稳定快照，再保留原来的容量与隔离断言，缓冲未扩大。
- 新增选择页、分钟像素和原矩阵分为三次独立启动，沿用每次 900 秒模拟上限与原资源预算。没有提高预算、删断言或依赖 Git 历史。

## 要用户定的

需将允许修改的文件范围扩展至 `time_service.c`，才能满足已明确的自动走时要求；建议合并主会话沿用原分钟调度补齐。本任务保持原文件边界。

## 顺手发现

- 阅读器自带 `apps/reader/en.tsv`，构建会合并到生成表；不能重复补进基础表。直接以仓库根目录运行字面检查会报缺键，使用 CTest 配好的合并检查根目录。
- 交互决策板链接读取失败；任务不涉及产品决策，按任务书继续，不改共享决策文档。

## 新增的机制

- 常驻任务、定时循环、文件格式、配置项：均无新增。
- 页面入口能力 `shell.lock_faces`：任务书要求有独立表盘选择页且 AI 能进入；具体选择及开关仍共用既有 `settings.set`。
- 节气只读日期表与查询：任务书第 4 项要求，来源、固定提交、哈希和 MIT 许可见 `system/ui/lock_faces/SOLAR-TERMS.md`。

## 要改的文档

按任务书只重新生成能力文档，其他共享文档不改。合并主会话需将节气查询声明移到公共时间头文件（该文件不在本任务范围），同步界面规范与依赖文档中的六种表盘及锁屏分钟调度契约。

## 针对性验证

- 【实测】`lock-faces[picker]: exit=0, pass=289, fail=0, finished=1`；每种样式均经真实点按和 AI `settings.set` 后锁屏核对。
- 【实测】`lock-faces[tick]: exit=0, pass=35, fail=0, finished=1`；前后 PNG 像素差只落在实际时钟标签矩形内，同一分钟像素完全一致。
- 【实测】`lock-faces: exit=0, pass=487, fail=0, finished=1`。原来的 343 条所覆盖的场景全部保留，另加真实设置和 2026 年 24 个节气的独立天文台日期对照、普通日及边界检查。
- 【实测】`enum`、`solar`、`events`、`minute`、`day`、`member`、`reading` 七组反向均为 `reverse failed at the expected assertion`。跨日对照明确触发 `cross-day rebuild`，不再把模拟上限超时算成功。
- 【实测】英文 `exit=0, pass=42, fail=0, finished=1`；月历农历、节气、选择页和小组件均走翻译表。英文农历行按实际字高布局，避免与星期栏重叠。
- 【实测】更新单选页合同后的 `settings-b`：`write: PASS checks=431`、`restart: PASS checks=20`，原主页像素比较及保存后重启检查仍过。
- 【实测】ASan/UBSan 的选择页、分钟像素、矩阵同样为 289／35／487 条通过，无内存诊断；矩阵的内部 RAM（含 DMA 池）最小剩余 50,088 字节。
- 【实测】范围外依赖探针：`INK_LOCK_FACES_CASE=auto INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/lock_faces.py --build build-asan` 明确在 `locked minute advances automatically through the time service` 失败（27 通过、1 失败、已到结束标记）。该探针单独运行，保留缺口证据，不算正向测试通过。
- 【实现】节气来源为 MIT 项目 cnlunar 的香港天文台 1901～2100 年日期表，按 UTC+08:00 民用日期查询，不做天文瞬时／其他时区换算；[固定提交、哈希和许可](../../../system/ui/lock_faces/SOLAR-TERMS.md)。

## 规模与架构

- 产品代码新增 149 行、删除 33 行、净增 116 行（含节气表，不含测试、翻译数据和文档），小于 300 行预算。超过 100 行的原因：独立选择页与快照、时间事件接线、31 处翻译接入，以及任务要求的 200 年节气表是独立验收内容。看过更小做法：保留三档循环不符合单选页合同；只依赖分钟数不能发现相同分钟跨日；节气近似公式不符合来源合同。
- 【实现】选择页只有呈现和导航；按钮与 AI 均调用 `settings.set`，设置服务仍负责权限、成员值和保存；时间来自时间服务事件，农历与节气来自同一时间模块，未新增界面计时器。页面与锁屏均有 AI 快照，成员组件仍由原数据集合和阅读能力供给。

## 检查结果

- 【实测】普通工程按总规则第 5 节配置（Werror、给定字体／Python／LVGL），`taskpolicy -c utility cmake --build build -j2` 成功；最终普通全量原话：`100% tests passed out of 414`，`Total Test time (real) = 608.85 sec`。所要求的 `ui-scale`、各 i18n 检查、`capability-table-doc` 均过。
- 【实测】ASan/UBSan 工程使用同一配置，仅换目录并加 `-DINK_SANITIZE=ON`，完整构建成功；`UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2` 原话：`100% tests passed out of 414`，`Total Test time (real) = 963.73 sec`。完整日志未检出 ASan、LSan 或 UBSan 诊断。
- 【实测】默认配置的普通／ASan 全量各跳过三个中枢测试：`hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e`，因为配置的 Python 没有选中其依赖。随后核实 `$INK_DEPS/hub/.venv` 的 Python 3.11.15 和四个依赖版本与本分支锁文件一致，只读复用，显式加载本分支 `hub` 源码；普通／ASan 补跑各为 `100% tests passed out of 4`（含能力导出准备测试），分别 57.16／57.45 秒，无跳过。未安装或升级依赖。
- 【实测】`ink-codex/tools/gcccheck.py` 检查 6 个修改的产品 C 文件和 3 个相关测试 C 文件，原话：`gcccheck: 9 compiled, 0 with findings`。未运行 Linux CI；S31 固件按本任务要求由合并主会话编译。
- 【实测】指定隐私正则扫描基线至当前提交：0 命中，grep 返回 1；`git diff --check` 无输出；任务文档链接、文件范围及单文件大小检查过。

中枢补跑命令（在仓库根目录执行；内存检查只换 `--test-dir build-asan`）：

```sh
PYTHONPATH="$PWD/hub:$INK_DEPS/hub/.venv/lib/python3.11/site-packages" \
PYTHONDONTWRITEBYTECODE=1 UBSAN_OPTIONS=halt_on_error=1 \
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" \
  --test-dir build --output-on-failure -j2 \
  -R '^(hub-contract|hub-json-crosscheck|hub-simulator-e2e)$'
```

## 上机核对清单

- 合并时间服务的锁屏调度修复后，核对睡眠唤醒及分钟、午夜刷新区域和耗电。
- 核对 120% 字号、壁纸对比度、真实面板残影，以及换成员后不残留成人文字。
