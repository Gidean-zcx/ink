# 短信任务报告

基线：`f0b0c539f3f8a30bca03f663484287d65c76de33`。分支：`claude/codex-sms`。仅模拟器，不接设备、不发真实短信、不推送。

## 第 1 轮复查修复（2026-11-09，验证中）

- 接续 `086c5059` 的复查，工作区干净；原报告及复查结论保留为历史证据。下列最终修复结论以本节后续记录为准。
- 本轮按用户授权修 SMS-R1-01～06：通信入口统一归属成员会话库、长来信、正文分页、首次导航、可见回执、长文草稿恢复。允许修改这些问题涉及的通信服务既有实现；不修改其他并行任务或共享生成文档。
- 【实测】六项修复的 15 阶段短信回归通过；`sms`、`systest-services`、`integration-apps`、`integration-lifecycle` 定向 4/4 通过。消息缓冲扩到 355 字节；事件参数表扩到 416 字节，补足正文、号码及成员元数据，不改变 118 字符／354 字节的短信限制。
- 【实测】`tests/sms/reverse.py` 的 8 个变体、10 个预期失败阶段全部命中断言，退出 1 且 `test.finished=1`；草稿写入前置阶段退出 0。分别撤掉成员列表隔离、直接发送记账、收信容量、事件容量、分页、首次导航清理、提示条位置和草稿引用恢复。没有修改产品源码或预算来做反例。
- 【实现】持久收发移入通信服务；`sms.send` 仅保留为同一处理函数的兼容能力名，按钮与页面 AI 都用 `message.send`，删除应用中的嵌套发送实现及全局收信缓存。长文仅将收件人和草稿编号接到原应用保存／恢复接口；没有另存正文。
- 【实测】首轮全量 223/225：能力文档仍过期；通信收口后发现原注册状态可能过期，已改为发送前通过现有 HAL 核对实际注册状态，定向复测通过。原离线测试改为真实模拟卡掉线，断言未放宽。最终两套全量及隐私检查待跑。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】新会话路径通过；旧发送入口未收口 | 17 条收信及成功、失败、未知各自落盘；另起进程恢复 21 条记录 | `ctest --test-dir build -R '^sms$' --output-on-failure -j2`（实际均用 utility 优先级及指定解释器） |
| 2 | 【实测】118 汉字发送和显示通过；长收信未完成 | F2 → F3 → 返回 → 实际点发送；AT 模型逐字相等；真实标签完整落在卡片内且为白字 | 同上；`SmsThread` 阶段 |
| 3 | 【实测】新页面及数据能力通过；旧 `message.list` 不通过 | 去掉集合归属后两条断言失败；另测旧能力确实跨成员泄露 | 同上；`tests/sms/run.py … --mutant`；下文 `legacy` 探针 |
| 4 | 【实测】通过 | 三页快照进入真实 AI 上下文；AI 发送停在确认卡，人确认后只发一次；号码经 `contacts.select` | 同上，四阶段 |
| 5 | 【实测】能力、规模和内存检查通过；全量未全部通过 | 两套全量各 224/225，唯一失败为共享能力文档；8 项能力注册，产品 234 行；RAM 最低余量 47,840 字节 | 全量命令见下文 |

## 没做到的

- 【实现／实测】旧 `message.send` 仍可绕过新会话库直接发送，旧 `message.list` 仍读全局 16 条缓存；完整通信入口的持久化和成员隔离未完成。新页面、按钮和页面 AI 动作均使用新路径，不能据此宣称旧接口安全。
- 【实现】通信服务收信缓冲仍为 200 字节，超过约 66 个汉字的来信会读取失败；本任务验证的是 118 汉字的发出与显示，没有完成长来信。发信仍受既有 118 字符／354 字节参数限制，没有擅自抬高。
- 三页视觉分数未到 12/255。未运行 Linux／gcc 或实机。

## 和设计不一致的

- 【实测】已看三页画板，并查看实际截图。普通及 ASan 对照分数相同：`Sms` 32.312、`SmsThread` 100.430、`SmsCompose` 36.807（均为 1/4 灰度平均差 /255，全部超标）。产物：两套 `test-out/sms-compare/`。
- 使用现行主题的放大字号、101／125 控件和第 8 级灰；原稿仍为旧字号、浅灰。列表没有做搜索、头像、未读数和时间；会话按一条消息一页显示，未复现原稿多气泡、日期、拨号和日程建议。写短信复用 F2 短框，长文单开 F3，未复现原稿大输入卡与“帮我写”。示例号码和内容只在测试。

## 改过的已有测试

- `tests/integration/ai_surface.c`：只向严格清单末尾增加 8 项 `sms.*` 及必填参数，保留原断言。
- `tests/integration/apps.c`：注册应用数量从精确的 7 改为精确的 8，因为新增短信应用；其余断言不变。没有改预算或跳过测试。

## 要用户定的

- 推荐合并主会话允许改通信服务原有收发／列表入口，使旧 `message.*` 统一走成员会话库，并扩大收信缓冲。当前任务要求共用文件只追加，故未改其已有实现，也未用运行时替换能力或新路由机制绕开。

## 顺手发现

- 【实测】旧 `message.list` 的隔离缺口不是推测：合成来信后切孩子，`legacy message.list must not expose the adult inbox` 断言失败。证据在 `build/sms-legacy/metrics.json`；该诊断未混入新页面通过结论。
- 【实测】新增短信应用最初重复出现在“记和联系”和“工具”，导致两个已有分页回归失败；已在 `tools()` 末尾追加去重，原测试及断言保持不动，定向 3/3 恢复通过。

## 新增的机制

- 新数据集合 `sms.messages`：任务明确要求短信会话进入数据服务；按成员，512 条上限，满时拒绝写入，不自动删历史。
- 新页面和 8 项原子能力：任务明确要求三页及 AI 操作；额外 `ImeLong` 复用已合入 F3。
- 无新增常驻任务／线程、状态机、持久文件格式、后台重试、队列或用户配置项；后台执行器、数据日志、输入草稿和确认卡均为既有机制。

## 要改的文档

- `docs/system/capabilities.md`：合并时运行 `python3 tools/capabilities.py`；临时生成在 `work/sms/capabilities.md`，与运行时 221 项能力、16 项设置一致。
- `docs/system/ui-dependencies.md` 及任务 backlog：记录新三页和会话库的实际状态，同时保留旧入口隔离、长收信和视觉差异缺口。按总规则第 7 节，本任务不改这些共享文档。

## 为什么不能更小

产品 234 行（新文件 223、启动尾部 2、导航函数尾部 4、CMake 尾部 5；保守计入测试注册 CMake）。三个页面、成员存储、异步回执及能力声明需要这些接线。更小的方案已看过：继续使用 16 条内存摘要不能过重启验收；直接让界面写服务状态破坏能力路径；另做输入和分页会重复已有机制。未采用这些方案。

## 执行记录

- 【实测】开始时工作区干净，HEAD 等于指定基线，已在独立短信分支。
- 【实现】三页接到既有 `message` 启动入口；新增 `sms.messages` 成员集合，订阅既有收信事件，写入和发送都在原后台执行器上完成。发送前持久记录未知状态，避免重启后把在途消息误报失败。
- 【实现】按钮和 AI 共用 8 个 `sms.*` 能力；选号码经 `contacts.select`，复用 F2、F3 和 F4；未改通信、模组固件或配置。
- 【实测】两套完整构建已跑；首轮定位并修正 F3 页面名接线；真实截图发现黑底文字颜色不对，补了标签边界和白字断言。未用 sleep 隐藏问题。
- 【实测】反向对照只改变测试二进制的编译输入：定义 `INK_SMS_NO_MEMBER_CHECK`，把应用的缓存读取固定到 owner。编译／链接退出 0，模拟器及运行脚本退出 1，`test.finished=1`，恰好两条隔离断言失败。正常二进制四阶段通过；源码和预算未被修改。
- 【实测】初次全量有三项中枢测试自动跳过，原因是默认 Python 缺锁定依赖。已选用 `$INK_DEPS/hub/.venv/bin/python` 的现成环境重新配置，没有安装或升级依赖；最终全量单独记录。

## 复现命令

配置沿用任务书的字体、Python 和 LVGL 参数，两套均 `INK_WERROR=ON`；内存检查目录加 `INK_SANITIZE=ON`。另外两套均设 `-DINK_HUB_PYTHON="$INK_DEPS/hub/.venv/bin/python"`，SDL2／ImGui 复用固定版本源码，未换版本。所有构建最多 `-j2`，没有启动窗口。

```sh
taskpolicy -c utility cmake --build build -j2
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j2
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
INK_KEEP_EVIDENCE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/sms/run.py "$PWD/build/sim-sms" "$PWD" "$PWD/build/assets" "$PWD/build/test-content" "$PWD/build/test-out/sms-negative" --mutant
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/capabilities.py --output work/sms/capabilities.md --runtime build/test-out/capability-table/capability-inventory.json
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/test-out/sms-compare --scenes Sms SmsThread SmsCompose --reuse
INK_SMS_PHASE=legacy taskpolicy -c utility build/sim-sms --profile simulator/profiles/metalio-plus.json --script tests/sms/sms.script --budget tests/ui_scene.budget.json --assets build/assets --content build/test-content --flash-image build/sms-legacy.img --sd-dir build/sms-legacy-sd --out build/sms-legacy --quiet --no-random-outages
```

正常短信命令需 `INK_KEEP_EVIDENCE=1` 才保留四阶段完整记录；对照目录会留下经验证的截图和指标。反例和 legacy 命令预期退出 1；后者是尚未修复的问题，不计为通过。

## 最终验证

- 【实测】两套完整构建退出 0；普通及 ASan／UBSan 全量均为 224/225、退出 8，唯一失败 `capability-table-doc`；没有跳过测试。最终普通全量 172.00 秒，ASan／UBSan 全量 327.48 秒。完整日志分别为 `build/sms-full-last.log`、`build/sms-asan-full-final.log`。
- 【实测】最终 ASan 短信四阶段分别 117／33／21／27 项断言通过，零失败、零警告；内部 RAM 最低余量均 47,840 字节，门槛仍为 32,768，内外部内存分配失败均为 0。证据保存在 `build-asan/test-out/sms/`。
- 【实测】两套运行时注册表均与临时能力表一致：221 项能力、16 项设置，生成和 `--check` 退出 0；未改共享能力文档。确定性测试通过，预算未改。
- 【实测】全量 ASan／UBSan 日志检查未发现 AddressSanitizer／LeakSanitizer 错误或 UBSan 的 `runtime error`。最后一处测试等待使用注册状态条件，重编后的 ASan 短信测试再次通过，未用固定延迟掩盖时序。
- 【实测】基线至实现提交及包含最终报告的差异均按指定隐私正则扫描，零命中（搜索退出 1）；`git diff --check` 退出 0。差异仅 10 个相关文件，未夹带其他并行任务文件、预算、共享文档、设计稿或构建产物。
- 本地里程碑提交：`c621c93c`（开工报告）、`d1c2cc8b`（三页及会话库）、`b315e0b1`（回归、反例及缺口报告）；最终证据另作本地提交。没有推送或开 PR。
