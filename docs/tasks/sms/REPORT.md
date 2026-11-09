# 短信任务修复报告（第 1 轮）

日期：2026-11-09。基线：`f0b0c539f3f8a30bca03f663484287d65c76de33`。分支：`claude/codex-sms`。接续复查提交 `086c5059`，开工时工作区干净。复查前的问题和原有证据见 [REVIEW.md](REVIEW.md)；本报告记录修复后的状态。

【实测】严重 1 项、高 2 项、中 3 项均已修复，15 阶段短信回归及逐项反向对照通过。产品修复提交：`c1fecdbf`。普通及 ASan／UBSan 全量均为 224/225，唯一失败为按规则留待合并的共享能力文档。仅使用模拟器 AT 模型和合成数据，没有连接设备、发送真实短信、推送或开 PR。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| SMS-R1-01（严重） | 【实测】已修 | 旧列表读取调用成员的持久会话库，孩子看不到成人收信；直接调用旧发送能力只发送一次且持久记账。两个发送能力名的处理函数相同，按钮和页面 AI 都调用 `message.send` | 下文短信回归；`legacy`、`legacy-send`、`write`、`verify` |
| SMS-R1-02（高） | 【实测】已修 | 118 汉字来信完整进入会话，新进程读取逐字相等；收信正文缓冲 355 字节，参数表 416 字节，完整事件同时保留正文和成员信息 | 同上，`long-rx`、`long-rx-read` |
| SMS-R1-03（中） | 【实测】已修 | 80 汉字加 15 个换行分成两页；每页真实标签在卡片边界内，拼接后逐字等于原文，上一页／下一页均可用，AT 和落盘内容未变 | 同上，`multiline` |
| SMS-R1-04（中） | 【实测】已修 | 未打开短信页时，收到短信后直接 AI 调 `sms.thread`，保留目标号码和记录，真实 AI 快照含来信 | 同上，`cold-thread` |
| SMS-R1-05（中） | 【实测】已修 | 成功、明确未发送、结果未知三种回执均位于按钮上方；断言检查真实对象坐标，并逐屏查看截图 | 同上，三个 `receipt-*` 阶段 |
| SMS-R1-06（高） | 【实测】已修 | F3 已保存后结束进程；同镜像重启、选择同一收件人、再次打开 F3，恢复全部正文，复用原编号，草稿记录数保持 1 | 同上，`draft-write`、`draft-read` |
| 原验收 1 | 【实测】通过 | 17 条收信和成功／失败／未知发送记录共 21 条在新进程恢复；旧发送入口也记账，回执真实可见 | 短信回归的 `write`、`verify` 及复查阶段 |
| 原验收 2 | 【实测】通过 | 118 汉字发送、收信、显示、草稿恢复，以及普通换行正文分页都通过 | 短信回归 |
| 原验收 3 | 【实测】通过 | 页面、数据能力和旧公开列表均隔离；撤掉缓存归属失败 2 条，撤掉旧列表归属失败 1 条 | 短信回归、成员反例和逐项反向对照 |
| 原验收 4 | 【实测】通过 | 三页快照可进入实际 3072 字节 AI 上下文；确认前不发送，人确认后只发送一次；首次直接导航不依赖手工先打开列表 | 短信回归 |
| 原验收 5 | 【实测】部分 | 产品新增行数保守计 279／350；221 项能力／事件、16 项设置与临时生成表一致。共享能力文档按总规则留待合并；两套全量各 224/225；内存检查无报错，结果见末节 | 下文全量、能力检查 |

## 没做到的

- 共享 `docs/system/capabilities.md` 未更新：总规则第 7 节明确禁止本任务修改。原验收 5 不能写成全部通过；不删除、跳过或放宽该测试。
- 三页整体视觉差仍超过 12/255；结构缺项见下一节。本轮已解决正文裁切和回执遮挡，没有把页面其余设计缺项算作完成。
- 未在 Linux／gcc 或设备上运行；本机 macOS／clang 和模拟器证据不外推。
- 本轮六条复查问题没有未修项。既有 118 字符／354 字节上限保留，不扩展短信长度产品范围。

## 和设计不一致的

【实测】已看三张设计稿及实际截图，用同一 `tools/ui_compare.py --reuse` 复算：`Sms` 32.312、`SmsThread` 100.430、`SmsCompose` 36.807（1/4 灰度平均差 /255）。对照和分数在 `build/test-out/sms-compare/`。

列表仍无搜索、头像、时间和未读数；会话仍按一条消息的正文窗口分页，没有原稿多气泡、日期、拨号和日程建议；写短信仍复用 F2 短框并单开 F3，没有大输入卡、字数和“帮我写”。这些是复查已披露的页面补齐工作，超出本轮小修，不能改设计文档迁就实现。现行放大字号、主题间距和第 8 级灰没有改回旧稿。

## 改过的已有测试

- `tests/sms/sms.c`：保留原断言，新增复查场景；数据集合从服务注册表查找，页面发送快照的精确期望改为统一能力 `message.send`。离线夹具改为模拟 SIM 真正掉线并等待实际注册条件，未知结果仍由禁止短信业务触发；不再只改一个可能过期的状态值。没有加固定等待或降低预算。
- `tests/sms/run.py`：保留原四阶段，增加 11 阶段；长来信及草稿各自使用写入／重启共用镜像。
- `tests/integration/systest.c`：旧列表改读磁盘，运行于后台执行器，因此从同步 `ink_cap_invoke` 改为异步调用并等待回执；保留成功状态、收信数量精确为 1 和包含原号码三项断言。
- 原任务已有的严格能力清单增加 8 项、应用数量 7 改 8 保持不变。本轮没有删测试、跳过测试、改预算或放宽断言。

## 要用户定的

无新增产品决定需要等待。按本轮用户指令修改了 SMS-R1-01／02 涉及的通信服务既有实现，覆盖原任务“共用文件只追加”的限制；没有改其他并行任务或 4G 模组固件、配置路径。

## 顺手发现

- 【实测】只扩收信缓冲仍会丢成员元数据：354 字节正文、号码及字段名已接近原 384 字节参数表，事件再加成员时写不下。容量改为 416 字节；回归检查完整正文和成员，恢复旧容量的反例准确失败。没有放宽短信长度或内存预算。
- 【实测】首轮全量 223/225：除已知能力文档外，发送前读取缓存注册状态导致既有 `systest-services` 失败。统一发送实现改为在原 AT 锁内通过现有 HAL 核对实际注册状态；定向 4/4 复测通过。

## 新增的机制

- 无新增任务／线程、状态机、后台重试、队列、配置项或草稿格式。
- 沿用既有 `sms.messages` 集合和数据日志，归属从页面移入通信服务；删除旧全局 16 条缓存和页面嵌套发送实现。`sms.send` 保留为同一函数的兼容能力名，不再调用第二个发送能力，不重复确认或发送。
- 沿用既有应用保存／恢复载荷，仅保存 28 字节的收件人和草稿编号；正文仍归原 `text.drafts`。SMS-R1-06 要求恢复入口，因此给原应用保存函数补一个可在建草稿时调用的入口，不新建定时器或另一份正文。
- 正文分页沿用 F4 及 F3 的 UTF-8 字簇和文字测量方法；页数索引在 PSRAM，AI 快照只给当前可见正文，整条记录保留。

## 要改的文档

- 合并主会话运行 `python3 tools/capabilities.py` 更新共享能力文档，反映短信能力及 `sms.send` 的服务归属；本任务只生成 `work/sms/capabilities.md` 校验运行时。
- 合并时更新共享 `docs/system/ui-dependencies.md` 和 backlog：六条复查已修，保留上述视觉缺项。未改 requirements、ideas、decisions 或历史调研／审计资料。

## 为什么不能更小

【实现】从指定基线保守累计新增产品代码 279 行（含改动行和测试注册 CMake），未超 350：短信页 206、通信服务 54、应用保存入口和头文件 7、参数容量 1、启动 2、导航 4、CMake 5。没有用净删行掩盖预算。

整体超过 100 行，主要是原任务三页接线及本次将持久收发移到公共服务。更小方案已核对：只修新页面会留下旧公开能力绕过成员库；保留发送包装会继续出现两次能力调用；只扩正文缓冲仍无法附加事件成员；只在应用冻结时保存编号无法恢复仍在 F3 编辑时结束的进程。采用现有存储、保存接口和分页，删除重复路径，没有加拦截器或新框架。

## 反向对照

【实测】`tests/sms/reverse.py` 编译隔离变体并替换链接输入，不修改产品源文件。编译／链接都成功；脚本退出 0 表示已逐个检查下面的预期失败。每个失败阶段都退出 1 且 `test.finished=1`，不是超时或启动失败。

| 变体 | 撤掉的修复 | 失败断言数 |
| --- | --- | ---: |
| 01-list | 旧列表改回读取 owner | 1 |
| 01-send | 发送函数恢复复查前不记账实现 | 1 |
| 02-rx | 收信正文缓冲恢复 200 字节 | 2 |
| 02-metadata | 参数写入容量恢复 384 字节；保持 ABI 以便隔离链接 | 3 |
| 03-pages | 正文始终整条显示，不分可见页 | 3 |
| 04-cold | 移除导航入口的成员初始化 | 2 |
| 05-notice | 提示条恢复原纵坐标 | 成功／失败／未知各 1 |
| 06-draft | F3 总是传入 `draft_id=0` | 写入前置阶段成功，重启阶段 2 |

另跑原 `--mutant`：编译／链接成功，去掉页面缓存成员归属后退出 1，`test.finished=1`，恰好 2 条隔离断言失败。正常同场景通过。证据：`build/test-out/sms-reverse/`、`build/test-out/sms-member-negative/`。

## 复现命令

两套构建沿用任务指定字体、Python 和 LVGL；均 `INK_WERROR=ON`，ASan 另设 `INK_SANITIZE=ON`；中枢 Python 为 `$INK_DEPS/hub/.venv/bin/python`。没有安装或升级依赖。构建与测试依次执行，最多 `-j2`，均用 utility 优先级。

```sh
taskpolicy -c utility cmake --build build -j2
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^sms$' --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/sms/reverse.py build build/test-out/sms-reverse
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/sms/run.py build/sim-sms . build/assets build/test-content build/test-out/sms-member-negative --mutant
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output work/sms/capabilities.md --runtime build/test-out/capability-table/capability-inventory.json
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/test-out/sms-compare --scenes Sms SmsThread SmsCompose --reuse
git diff --check f0b0c539f3f8a30bca03f663484287d65c76de33 HEAD
```

`--mutant` 预期退出 1；全量检查的已知非零结果必须与下一节核对，不当作全绿。

## 最终验证

- 【实测】最终普通构建退出 0，全量 224/225、退出 8，无跳过；唯一失败 `capability-table-doc`，148.73 秒。日志：`build/sms-repair-full-final.log`。确定性、层级和预算测试通过。
- 【实测】ASan／UBSan 构建退出 0，全量 224/225、退出 8，无跳过；仍仅失败 `capability-table-doc`，263.60 秒。日志：`build-asan/sms-repair-full-final.log`；没有 AddressSanitizer／LeakSanitizer 错误或 UBSan `runtime error`。
- 【实测】两套短信回归各 15 阶段、442 条断言，零失败、零警告；内部 RAM 总最低余量均为 47,840 字节，原门槛 32,768 未变；内部和外部内存分配失败均为 0。证据：两套 `test-out/sms/`。
- 【实测】两套运行时均与各自临时生成表一致：221 项能力／事件、16 项设置（共 282 项能力、设置、状态定义）；生成和运行时校验退出 0。
- 【实测】指定基线至最终版本的 `git diff --check` 退出 0。指定隐私正则命中 1 处，是开工前已有的 `REVIEW.md:104` 扫描命令自身，没有真实本机路径或标识符；仅本轮修复差异（`086c5059` 至最终版本）扫描零命中，搜索退出 1。
- 【实测】基线差异仅 17 个相关文件；本轮修复及报告共 10 个文件，没有预算、共享生成文档、依赖、设计稿或构建产物。里程碑已本地提交：`ed9a5cd6`（开工记录）、`c1fecdbf`（六项修复及回归）；最终报告另作本地提交，没有推送。
