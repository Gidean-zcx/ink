# remote-app 执行报告

基线：`50fd91821b47d6a9cacd42e7ba07fbd3b8084de4`。在独立任务分支执行，不推送。
本报告的【实测】均指本机模拟器和合成中枢。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 | 【实测】通过 | 慢首屏先显示 Loading；6 秒断线自动恢复且画布哈希一致；301 秒后终止恢复，网络晚回来不重开 | `ctest --test-dir build -R ^remote-app$ --output-on-failure -j2` |
| 2 | 【实测】通过 | 地址进入会话参数；两次文字编辑无上行，完成后一次完整 UTF-8；密码快照遮蔽，AI 输入和提交均拒绝；边打边发的反例被检出 | 同上；`tests/remote_app/run.py --build build --mutant stream` |
| 3 | 【实测】通过 | 设备专项中枢收到 3 次翻页、1 次长按、2 次完成输入；中枢网关和适配器 10 项通过 | 同上；在 hub 运行 `$INK_DEPS/hub/.venv/bin/python -m pytest tests/contract/test_remote_input.py -q` |
| 4 | 【实测】通过 | 拦截关闭会话；儿童直接调用和应用入口拒绝、目录不可见；六页快照 JSON、动作与版本均核对 | `ctest --test-dir build -R ^remote-app$ --output-on-failure -j2` |
| 5 | 【实测】未完全通过 | 普通、ASan/UBSan 全量均 241/242；中枢 pytest 135 项通过。唯一失败均是按规则未更新的共享能力目录；无 sanitizer 报错。设备端保守计 298 行 | 见下方复验命令 |

## 没做到的

真实 Chromium 浏览器节点仍属中枢第 6 步；本次只接输入处理接口及合成替身，不宣称验证真实网页。G1 因冻结的共享目录未全绿，G4 六页视觉未达标。Linux/gcc 未在本机实跑；本机两套构建均为 Apple clang、`INK_WERROR=ON`。
当前地址栏最多 179 字节、网页输入最多 255 字节；更长输入未做。

## 和设计不一致的

【实测】六页均未达到 G4 的 12/255 目标。用 `tools/ui_compare.py --build build --design "$INK_DESIGN" --out build/remote-compare --scenes Remote RemoteLoading RemoteOffline RemoteAddress RemoteType RemoteBlocked --reuse` 对照已验证的静止截图：

| 页面 | 差值 /255 | 差异 |
| --- | ---: | --- |
| Remote | 79.606 | 内容是模拟中枢生成的图形替身；没有画板的星星正文和照片。导航在底部，未完成地址顶栏、刷新、页码和“问这一页”按钮。已给导航留出视口，不遮住网页。 |
| RemoteLoading | 18.754 | 静态地球图标代替三个点；通用顶栏、字号和灰度按现行规范。 |
| RemoteOffline | 33.684 | 使用公共断网图标，重连按钮占整行；关闭在顶栏。 |
| RemoteAddress | 40.928 | 使用现有系统键盘；没有历史网址服务，不伪造画板的站点列表。 |
| RemoteType | 39.386 | 通用输入说明替代网页表单标题；复用系统键盘。 |
| RemoteBlocked | 27.683 | 使用通用拦截说明；无法从当前控制信息区分内网等具体原因。 |

画板仍使用旧字号、浅灰和键盘，规范已改为统一放大及深灰；本次不改键盘等共用组件迁就旧图。中枢架构 §5.7 记着 D-94 开放儿童网页，与本任务明确的临时禁用相反；按本任务执行，待主会话统一核对。

## 改过的已有测试

`tests/integration/apps.c` 的精确应用数 15 → 16，因新增网页应用；保留等值断言。`tests/integration/ai_surface.c` 在清单末尾补 8 个新能力，保留全部旧检查，`remote.pointer` 必填参数核对为 `x,y`。

## 要用户定的

儿童网页按任务书暂不开放（D-94 待选）。

## 顺手发现

能力目录生成器把 `child_ok=false` 显示为“儿童须白名单”；本任务处理函数按任务要求拒绝全部儿童，目录合并时需说明这道额外限制。

## 新增的机制

任务书要求的六种页面状态及五分钟恢复截止；复用现有界面定时器和执行器，不加任务、线程、持久格式、配置项、缓存或队列。新增输入类型 2（翻页）、3（长按）、4（完成文字）；原点按 1、区域帧和额度格式保持不变。

## 要改的文档

主会话合并时更新 `docs/system/ui-dependencies.md` 的 remote 行，并重新运行 `tools/capabilities.py` 生成共享能力目录。本分支仅将生成候选放在忽略的 `build/remote-capabilities.md`。中枢协议文档还需补记上行类型 2/3/4 和 `rt.nav` 的 `blocked`、`input` 控制字段（定义在 remote.c 文件头）；不改区域帧和额度格式。

## 里程碑

- 已核对基线一致、工作区干净，读取任务书和总规则，开始核对现有实现与画板。

- 界面与能力、设备输入、模拟中枢和 Python 输入适配器已实现。新增同页恢复核查发现并修正了替身“无变化也扣额度”的问题；同一会话重连保留页号、重置白屏基准。

- 设备专项最终重跑 84 条断言通过，内部 RAM 最低余量 45104 字节；相关既有回归通过。7 种反向对照（边打边发、加载页、翻页、长按、拦截、超时、儿童入口）均被对应断言检出；不改工作树源码，只在构建目录编译变体。

- 最终普通全量 241/242（462.36 秒）、ASan/UBSan 全量 241/242（738.17 秒），两轮中枢契约及网页专项均通过。检查了 sanitizer 编译参数及完整测试日志，没有地址、泄漏或未定义行为诊断。

额度修复前，“翻页帧到达”和“重连同页哈希”两条断言失败；修复后同一测试通过。没有删除断言、加固定等待或提高资源预算。

## 复验命令与结束检查

按总规则配置 `build`，`build-asan` 仅增加 `INK_SANITIZE=ON`，复用同版依赖。两套构建均使用 `taskpolicy -c utility cmake --build <目录> -j2`。

```sh
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/remote_app/run.py --build build --mutant stream
```

最后一条依次将 `stream` 换成 `loading`、`page`、`long`、`blocked`、`timeout`、`child` 可复验全部反向对照。两套全量命令均已实跑，退出码均为 8（仅目录检查失败），没有跳过失败项。构建、ctest 的并行数均未超过 2。

【实测】`git diff --check` 无输出；已检查基线至最终提交的文件统计，只有任务实现、测试及本报告。按总规则第 6 节的表达式扫描基线至 HEAD：隐私匹配 0；没有提交构建产物或设计图。全部修改仅提交到本地任务分支。

## 为什么不能更小

六种页面、系统键盘和密码边界、能力入口、恢复截止、两端新增输入都在验收范围，无法按单一提示修补处理。已考虑只改原提示条（不能满足六页和文字输入）和拆成独立页面/服务（新增生命周期和接线更大）；保留原画布、解码、额度与实时会话，在原文件集中实现。设备端按 diff 保守计 298 行（remote.c 新增 254、删除 42，启动接线新增 2），低于 350；中枢 Python 产品代码新增 35 行，模拟替身另计。
