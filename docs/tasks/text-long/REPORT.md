# text-long 执行报告

日期：2026-11-09。基线：`fa7c18803f8f0eefc44db39acc2da053c7aa9386`。仅本地开发、提交和模拟器验证，不碰设备、不推送。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 固定字节表校验双向换算及字节内偏移；18 个真实文字页连续覆盖正文，组合音标、ZWJ、旗帜不被切开；两类索引反例失败 | 定向命令、反向对照 |
| 2 | 【实测】通过 | 同一 Flash 镜像模拟断电、重启，恢复正文、组合串、光标及草稿引用；错误页面拒绝 | 定向命令 |
| 3 | 【实测】通过 | 切换当刻，旧会话不能写入或读取；孩子独立创建同页草稿；仅撤成员检查时 3 项断言失败 | 定向命令、反向对照 |
| 4 | 【实测】通过 | 真实长按菜单触摸与 AI 能力均操作选区；实查单词记录来源和假中枢收到的问题 | 定向命令 |
| 5 / G1 | 【实测】未全绿 | 最终普通及 ASan/UBSan 全量各 214/216，只有两项能力文档同步检查失败；无内存或未定义行为诊断。产品新增和删除合计 264 行，未放宽预算 | 完整验证命令 |
| G2 | 【实测／实现】通过 | 三项新能力；真实菜单触摸、AI、翻页实体键调用总线；快照带正文、草稿引用和实际文字页范围，失焦后不宣称可编辑 | 定向及 AI surface 测试 |
| G3 | 按总规则第 7 节留主会话 | UI 依赖表状态未改，列入“要改的文档” | 差异检查 |
| G4 | 【实测】未达视觉目标 | 已看原稿、只用主题规格、无动画、真实服务；最终对照 42.197/255，超过 12 | 画面对照命令 |
| G5 / F3 | 【实现／实测】完成本任务接口 | 复用现有字段、键盘、编辑会话和数据服务；只增文本服务接口；交付可分页测试编辑页和单一草稿合同 | 定向命令、源码差异 |

## 没做到的

- 两项文档检查未过，未跳过、删测试或改检查逻辑。Linux/gcc、实机未运行。
- 断电恢复保证已落盘的版本；“正在保存”阶段尚未落盘的最新输入，不能承诺瞬时断电不丢。
- 保留原来 4095 字节正文上限；未实现完整 Unicode UAX #29 字素边界表，覆盖常见组合音标、变体选择符、肤色、ZWJ 表情及双区域旗帜。
- “问 AI”沿用现有问题缓冲，选区达到 384 字节时明确拒绝，不静默截断。未验证任意 Unicode 字形、存储耗尽／写失败后的跨页面恢复或多字段表单流转；未开发备忘、单词应用。

## 和设计不一致的

- 【实测】最终 `ImeLong` 对照 42.197/255，超过 12。使用混排合成测试文而非画板例文，页数为 18；现有键盘的麦克风、第三行和底行与旧画板不同，现行规范已放大 1.2 倍；表情和部分组合字符缺字，正文可用行数较少。已查看原稿、最终实际截图及干净帧，未修改设计或为分数另做键盘。

## 改过的已有测试

- `tests/integration/textinput.c`：旧三项位置初始化补第四项 `NULL`，兼容新增的翻页回调；不改任何断言或预期。
- `tests/integration/ai_surface.c`：白名单新增 `text.page` 必填 `direction`、`text.ask_ai` 和 `text.add_word` 的可选会话参数；保留“所有运行时能力必须且只能归入一张表”的断言。
- `tests/integration/confirm.c`：新增能力后完整目录超出原 32 KiB 单缓冲，改用现有分页接口逐页检查；缓冲仍为 32 KiB，每页仍要求有效输出、禁止 AI 确认／取消，完整遍历后仍要求查询与撤回能力存在。没有放宽资源门槛或安全条件。

## 要用户定的

- 无新增产品决定；已知限制与文档同步项均列在对应节，未把暂定做法升级成需求。

## 顺手发现

- 首轮普通全量的 AI 白名单、单缓冲目录断言失败已按上节处理，定向重跑均通过。新增能力也使两份能力文档过期，留主会话同步。
- ASan 前两轮的 UI 源码与旧对象时间戳落在同一秒，Make 漏重编，失焦视口两项新断言失败。已刷新该源码时间戳并核对内容哈希不变；最终日志确认重编 `textfield.c`，对象比源码新，定向与全量均通过该测试。

## 新增的机制

- 任务要求的草稿：数据服务成员集合 `text.drafts`，schema 1；一个记录存页面命名空间、正文、组合串和字节位置，导航只持有页面与记录引用。
- 任务要求的加单词：数据服务成员集合 `text.words`，schema 1，存选区正文及可信来源；不实现单词应用。
- 草稿回执使用编辑控件内的 LVGL 计时器，完成、失败或失焦后暂停，编辑后恢复，控件删除时释放；没有新增产品 OS 任务、线程、队列、缓存、系统设置或后台重试机制。

## 要改的文档

- 合并时运行 `python3 tools/capabilities.py` 更新 `docs/system/capabilities.md`，新增 `text.page`、`text.ask_ai`、`text.add_word`；未更新前 `capability-table-doc` 必然报旧目录。
- 合并时在 `docs/system/ui-dependencies.md`、`docs/tasks/backlog.md` 标记 text-long/F3 的实际状态；任务规则第 7 节禁止本任务修改这些共享文件。
- `docs/system/system-services-design.md` §1.3 文本行补上述三项 AI 能力，否则 `services-mid-capability-doc` 报缺三项。总规则第 2 节禁止为实现修改设计合同，本任务只列待同步项，不改规范或检查器。

## 里程碑

- 已核对当前分支 `claude/codex-text-long`，HEAD 与给定基线一致，开工时工作区干净。
- 【实测】最终普通全量中长文测试退出 0：写入阶段 259 项、重启阶段 91 项断言通过；无告警、无预算违规，两阶段内部 RAM 最低余量均 47,840 字节，原门槛 32,768 未改。证据：`build/text-long-2xr6dj9e/`。
- 【实现】按相对基线的新增和删除保守计数，产品 264 行，包含头文件和 CMake，不计测试和报告。超过 100 行的原因：索引／分页、持久草稿、选区能力是三个明确交付。看过更小方案：只扩旧 30 字窗口不能保证分页边界；在 UI 另存草稿违反 F3 单一合同；保留直接调用的选区按钮则 AI 无同源能力。已复用键盘、撤销、存储队列和 UI 任务。
- 【实现】新增服务接口仅涉及字符／字节与组合边界、草稿引用／落盘状态、现有输入客户端的翻页回调；草稿保存在文本服务，界面负责测量分页。能力复用成员和会话版本检查，密码、隐藏字段不落草稿且不能执行选区动作；未改策略或其他业务服务。
- 【实现／实测】字符索引沿用现有服务和 LVGL 的 Unicode 码点口径：`e` 加组合音标占两个索引，分页时仍作为一个组合整体，不改变既有光标、选区 API 的口径。

定向命令：

```bash
taskpolicy -c utility cmake --build build --target sim-text-long -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build -R '^text-long$' --output-on-failure -j2
```

## 反向对照

【实测】`tests/text_long/run.py --mutant <名称>` 只在忽略的构建目录生成替代对象并链接，不改工作区源码。最终七组编译、链接成功，测试均退出 1、正常跑到 `test.finished=1`；前后所有产品 C/头文件 SHA-256 一致。

| 名称 | 临时撤掉什么 | 失败断言数 |
| --- | --- | ---: |
| mapping | 字符转字节，错误地把字数当字节数 | 11 |
| clusters | 组合边界，错误地逐字节分页 | 7 |
| member | 当前会话的成员检查 | 3 |
| draft | 草稿保存 | 1：重启无法打开草稿 |
| page | 草稿页面归属检查 | 1 |
| selection | 把选区改回整段正文 | 11 |
| viewport | 失焦时保留当前文字页 | 1 |

命令：`taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/text_long/run.py --build build --mutant <名称>`。日志：`build/text-long-final-reverse-<名称>.log`，汇总：`build/text-long-final-reverses.json`；均不提交。

## 完整验证命令

两套配置均为 `INK_WERROR=ON`，内存检查另加 `INK_SANITIZE=ON`。字体、Python、LVGL、SDL2、ImGui 使用用户指定的只读依赖；中枢测试指向现成 `$INK_DEPS/hub/.venv/bin/python`，配合本检出的 `PYTHONPATH`，未安装或升级依赖。

```bash
taskpolicy -c utility cmake --build build -j3
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j3
PYTHONPATH="$PWD/hub" PYTHONDONTWRITEBYTECODE=1 UBSAN_OPTIONS=halt_on_error=1 taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/text-long-compare --scenes ImeLong --reuse
```

- 【实测】普通构建退出 0；最终普通 CTest 退出 8，214/216、零跳过，148.91 秒；只有 `capability-table-doc`、`services-mid-capability-doc` 失败。日志：`build/text-long-final-{build,ctest}.log`。
- 【实测】最终 ASan/UBSan 构建退出 0；定向长文退出 0；全量退出 8，214/216、零跳过，244.53 秒，同样只有两项文档检查失败，无 ASan/UBSan 诊断。两阶段均为 259／91 项断言通过、零告警、零预算违规，内部 RAM 最低余量均 47,840 字节。日志：`build/text-long-final-asan-{build,focused,ctest}.log`；长文证据：`build-asan/text-long-ys2gxrm0/`。
- 【实测】差异仅包含文本服务、现有字段／键盘、相应头文件、CMake、测试和本报告；受保护文档与第三方资源未改，未夹带产物或设计稿。`git diff --check` 退出 0；按总规则第 6 节对基线至 HEAD 的隐私扫描零匹配（grep 退出 1），文件引用核对通过。所有里程碑本地提交，未推送。
