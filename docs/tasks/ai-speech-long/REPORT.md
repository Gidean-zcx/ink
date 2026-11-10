# ai-speech-long 执行报告

基线：`5ef1b94dd947936c42a0eb4532b0fdf0bf55df1e`。在任务分支本地执行，不推送、不碰设备、不调用真实厂商。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1a 约 6 KB 中文按正常语速朗读超过 60 秒，全文、页面、缓冲正常 | 通过 | 【实测】6002 字节，正常每秒 4.5 字，806 项检查；模拟扬声器播放 485760 ms，一轮结束约 488 秒；全文逐页相等，零溢出 | `ctest --test-dir build -R '^ai-answer-long$' --output-on-failure -j2` |
| 1b 中途停止、打断后新问 | 通过 | 【实测】听到 220 秒后停止、AI 实体键新问、换成员、模拟来电均通过；停止保留文字，新问只留下新回答，旧帧不重启 | `python tests/ai_speech_long/run.py --build build` |
| 1c 无帧超时 | 通过 | 【实测】首帧未到约 45.55 秒结束；收过帧后无帧约 14.01 秒（含前置处理及短句）结束；连续帧也在整轮约 600 秒停止 | 同上（`silent`、`gap`、`limit`） |
| 1d 中枢超过 450 帧与超长整字截断契约 | 通过 | 【实测】新契约 5 项通过：751 帧跨过旧 42 秒时限、显示和朗读逐字相等（含 40 KB 输入的截断提示）、逐句合成顺序、慢合成后实时发帧 | `cd hub && python -m pytest -q tests/contract/test_long_speech.py` |
| 2a 原有长回答、聊天、分页、语音测试 | 通过 | 【实测】上述原测试及所有语音相关场景通过，原断言未改 | 同下全量命令 |
| 2b 全量测试 | 通过 | 【实测】原话 `100% tests passed out of 319`，无跳过，662.21 秒 | `ctest --test-dir build --output-on-failure -j2` |
| 2c 内存检查 | 通过 | 【实测】完整构建 ASan+UBSan，筛选的 66 项全部通过，原话 `100% tests passed out of 66`，96.69 秒；日志无 sanitizer 诊断。长朗读及七类场景内部内存最低均为 38000 字节，预算仍为 32768 | `ctest --test-dir build-asan -R 'ai\|voice\|audio\|scenario\|integration-member\|selftest\|determinism' --output-on-failure -j2` |
| 2d S31 编译 | 通过 | 【实测】脚本退出 0，两个修改过的系统服务均编译进 S31 组件；仅编译，未连接设备 | `taskpolicy -c utility bash ports/esp32s31/build.sh` |
| 2e gcccheck | 通过 | 【实测】最终 `5 compiled, 0 with findings`；gccscan、rscan 无发现 | `python ../../tools/gcccheck.py build system/services/ai/ai_service.c system/services/hub/hub_link.c simulator/environment/sim_hub.c tests/ai_answer_long/ai_answer_long.c tests/ai_speech_long/ai_speech_long.c` |
| 2f 中枢测试 | 通过 | 【实测】`hub-contract` 调用锁定环境运行 pytest：`147 passed in 56.78s`；生产 C JSON 解析交叉检查及模拟器—中枢端到端测试均通过 | 同全量命令中的 `hub-contract`、`hub-json-crosscheck`、`hub-simulator-e2e` |

## 没做到的

未碰设备、未使用真实 TTS 或真实音频；当前播放证据来自原产品和确定性模拟器的替身音频。

本次为 macOS 本地验证；Linux CI 未执行（任务禁止推送，本机无 Docker），已补 GCC 编译与静态扫描。

【实现】整轮上限为 10 分钟，不保证 32 KB 或任意慢语速都能念完。约 6 KB 中文按正常语速可在该上限内完成，持续发送也不能无限占用设备。

## 和设计不一致的

AiSpeak 的按钮、分页和音频焦点规则沿用原实现；朗读时没有增加翻页入口。仅按本任务更新设备协议中的朗读时限和发送节奏。其它设计里的旧时限待主会话同步。

【实现】没有新增页面或能力；停止与 AI 键仍走现有原子能力，沿用同一份 AI 页面快照，传输内部的续期接口不直接暴露为用户动作。

## 改过的已有测试

只改 `ai-answer-long` 的语速 `60 → 4.5` 字/秒，以及对应等待时长 `200 → 600` 秒、场景终点 `300 → 700` 秒；为正常语速留出运行时间，原有断言和内存预算不改。其它已有测试未改。

## 要用户定的

暂无。

## 顺手发现

首次全量原话为 `98% tests passed, 7 tests failed out of 319`。旧语音冒烟场景在 15～17 秒后结束或关面板，默认长示例在 1× 下来不及念完。将模拟中枢默认语音示例改为完整的短句（显示与朗读相同，语速仍是每秒 4.5 字），旧场景和断言均不改；完整长回答仍由 6 KB 专用数据验证。结束回调在收拢本地状态后、释放锁前排入 UI，保留原有新旧轮回调顺序；原 `integration-ai-race` 原样通过。七项复查 `100% tests passed out of 7`。

## 新增的机制

逐帧续期及 10 分钟总上限：任务第 1 项要求；复用现有请求截止时间，没有新定时器或任务。

逐句合成及实时节奏：任务第 2、4 项要求；复用现有异步发送和模拟事件，没有新协议消息。

没有新增线程、状态机、持久格式、配置项、队列或常驻缓存。设备仅增加一个放在 PSRAM 的 64 位截止时间；512 帧缓冲和内部内存预算不变。模拟中枢合并、切句的临时内存属于宿主机。

## 要改的文档

已改任务允许的 `docs/hub/device-protocol.md`。主会话需同步 `docs/hub/contracts-step1.md`、`docs/hub/hub-architecture.md` 等仍写 45 秒或 1.8 KB 的旧说明，以及 backlog 中的长朗读限制；本任务不改共享文档和历史报告。

## 规模和检查

按基线 `git diff --numstat` 保守计（含空行和注释）：设备与模拟器产品代码新增 82 行、删除 19 行，共 101 行；另加 CMake 4 行。中枢产品代码新增 31 行、删除 22 行。设备预算 200 行未超；测试和文档另计。

为什么不能更小：只放宽 45 秒仍会有断流挂起及下行溢出；只加大缓冲违反预算。设备复用中枢请求表的截止时间，因此仅增加一个续期入口；模拟中枢需跨原文字块分句，并与真实中枢一样处理 UTF-8 和截断提示，这部分使用临时宿主内存，不侵占设备 RAM。没有逐服务补丁或新后台机制。

【实测】`git diff --check` 通过；已检查 `git diff --stat`，仅任务所需的 16 个文件。按总规则 §6 的隐私正则检查基线到本分支的差异：无匹配，grep 退出 1；没有提交本机路径、设备标识、密钥、构建产物或设计稿。

构建与测试均以 `taskpolicy -c utility` 执行；构建 `-j2`、CTest `-j2`，串行进行各轮完整检查。配置使用总规则给定的字体、Python 和 LVGL，并加 `-DINK_HUB_PYTHON=$INK_DEPS/hub/.venv/bin/python`，中枢依赖目录只读。证据命令设 `INK_KEEP_EVIDENCE=1`，日志在 `work/ai-speech-long/`，场景数据在相应构建目录；这些产物不提交。

关键日志：`ctest-final.log`（319 项）、`ctest-asan.log`（66 项）、`s31.log`、`gcccheck-final.log`、`reverse.log`、`hub-reverse.log`。构建配置和构建日志也在同一目录。ASan 构建仅在普通配置上增加 `-DINK_SANITIZE=ON`。

本次新增用例发现：超时回调原先把缓存音频放完，才真正静音。现在只在成功时播放余下缓存，失败/超时直接丢弃；先结束本地会话状态并停止失败音频，再向 UI 发布结果。10 分钟用例中的观察任务显式持有 `INK_NEED_AWAKE`，以便在产品释放电源需求后还能记录结束证据。

【实测】手动反向对照：基线 AI 服务使 `fixed-wait` 失败；恢复模拟中枢默认 2×，`double-pace` 失败。命令：`python tests/ai_speech_long/reverse.py --build build --baseline 5ef1b94dd947936c42a0eb4532b0fdf0bf55df1e`。正常 CI 不依赖提交历史。

【实测】在隔离 Python 进程加载基线的 `voice/flow.py` 和 `gateway/wire.py`，运行新契约的帧数和同文朗读检查：`3 failed, 2 deselected in 0.32s`。分别检出了 450 帧拒绝、1800 字节朗读截断；没有修改工作树中的中枢模块。

实现提交：`3d2bdbca`；原场景兼容与回调顺序：`00344767`。验证报告单独提交，全部为本地提交，未推送。
