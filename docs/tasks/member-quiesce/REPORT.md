# member-quiesce 执行报告

基线：`ed3d122665cf704bb77a962f1fe38d66f6febe01`。分支：`claude/codex-member-quiesce`。

结论：**未通过完整验收，不能按可合并结果交付**。实现提交 `6d675b4d`；无远端写入。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| 1 三条复现与反向对照 | 新收尾回归通过；原漏洞基线条件已变化 | 【实测】普通和 ASan 各 10 场景、131 检查通过。普通证据 `build/member-quiesce-nxdx0bds/`，内存版 `build-asan/member-quiesce-7jpppx8_/`。只替换 cap/members/hub 三处源码为基线后，10 场景均正常退出并完成：9 个收尾场景失败、25 项断言失败，空闲通过；证据 `build/member-quiesce-ge8c_iis/` | `tests/member_quiesce/run.py`（0）；反向加 `--baseline ed3d122665cf704bb77a962f1fe38d66f6febe01`（1） |
| 2 切换额外时延 | 通过，仅模拟器 | 【实测】空闲基线 0.701 ms、修后 0.814 ms，增加 0.113 ms；在途数据 0.655→178.797 ms，增加 178.142 ms；发送中上传 0.673→100.237 ms，增加 99.564 ms。其他测量见下文 | 同上；查看各场景 `metrics.json` 的 `test.switch.*` |
| 3 可删的逐服务检查 | 已核查：没有可安全删除的现存代次检查 | 【实现】编号不复用的基线已删过逐服务 generation；现存 epoch 检查覆盖页面、语音等独立生命周期，见下表，全部保留 | `rg -n 'epoch|generation|turn_allowed|owner_matches' system/services system/ui/ui_task.c` |
| 4 全量、内存检查、150 行预算 | 全量不通过；未检出内存检查器错误；预算通过 | 【实测】普通、ASan+UBSan 均 200/206 通过，同样 6 项失败，无跳过。构建各退出 0，测试各退出 8；未检出 ASan/UBSan/LSan 错误，未完成的场景不能算覆盖通过。产品新增 131 行、删除 18 行，保守合计 149 行。日志 `build/ctest-final.log`、`build-asan/ctest-full.log` | 两套 `cmake --build` / `ctest`，见下方完整命令 |

## 没做到的

未接触设备、真实密钥或付费接口，未作远端写入。Linux gcc 和真机未执行。

【实现】基线已用原成员打开数据、按指定成员发通知，页面快照已有归属检查，且成员编号已禁止复用。services-finish 的原脚本未经适配会因通知钩子不再被使用而编译失败（`build/original-services.log`），不能拿它充当漏洞复现。本次测试保留实际服务边界和隔离断言，增加集中收尾的顺序、停收和回执要求；基线失败不等于原文的泄露仍然成立。

【实测】以下失败两套一致，均未放宽断言、预算或跳过。集中门、离线 EV 取消、上传发送边界三轮处理后仍不通过，按总规则 §7 停止扩改：

| 未通过的 CTest | 具体证据 |
| --- | --- |
| `ui-privacy-regressions-metalio-plus`、`ui-privacy-regressions-desktop-lcd` | `tests/ui_shell_regressions.c:299–327` 固定 BG 阻塞后换人，孩子未进入，随后页面断言失败；各 5 项失败 |
| `device-events` | `tests/device_events/isolation.c:70` 仍要求旧上传返回正 id 并以回调拒绝；当前门直接返回负值。发布被测试暂停时，它又等待新轮成功，最终场景未完成、9 项失败 |
| `integration-member-wake-stream` | `tests/integration/member_wake_stream.c:169` 中枢被暂停且有排队帧，切换取消；后续流与口述链路共 9 项失败 |
| `integration-races` | `tests/integration/races.c` 未产生 `test.finished`，没有完成；执行器重入排空与原并发夹具是否互等尚未解决 |
| `integration-confirm` | `tests/integration/confirm.c:246` 没收到 `member_changed`，仍是先前的 `withdrawn`。这是缺失回执，不能当成功 |

## 和设计不一致的

【实现＋实测】正在执行的 C provider 没有通用中断接口；不能在期限后强行换人而让其副作用继续。本实现取消切换并保留旧身份（`ink_member_switch` 返回 -3，能力回执为 cancelled），已排队尚未执行的调用则用原 QoS 取消。80 ms QoS 的排队场景 95.575 ms 完成切换；执行中阻塞场景约 3010.029 ms 取消切换，原 provider 释放后可以再切换。未实现任务书所说的所有超时工作都取消后继续换人。

【实现】离线、尚未进入发送队列的 EV 事件在收尾时直接取消并计入 `cap.events_cancelled`，避免没有网络时空等；已有队列发送仍计入收尾。没有新增确认、重试或超时配置。

【实现＋实测】发布期间的 sealed 门也挡住了 `confirm.c:426–446` 直接状态回调发出的 `confirm.result`（入口 `cap.c:1022`），这是未解决的回执缺失。门尚不能代替所有异步生命周期检查，也未实现阻塞传输队列的完整超时取消。PIN 能力按名称豁免等待自身，多个排队 PIN 请求未验证。

## 改过的已有测试

既有测试及预算一行未改。新增回归中的儿童取消预期曾从 rejected 改为 failed：核查 `time_caps.c:249` 和 `time_service.c:581`，原 provider 对无权限返回 -1，现有映射本就是 failed；不是放宽旧断言。上传断言改为等待接收端独立观察，同时精确要求 owner=1、kid=0；HAL 发送返回与接收端观察不是同一时刻。

## 要用户定的

若要求超时后必定完成换人，需要明确 provider 的合作取消边界；保守推荐本实现的“切换取消、身份保留”，但这不是用户已确认需求。现有回归冲突未修，不建议直接合并。

## 顺手发现

【实现】上传结束应以已排队请求完成发送为界，不能等待 AI 回答，否则 AI 发起 `system.switch_member` 时会互相等待。已在中枢发送出口收尾计数；不变更各服务的身份传递。

## 新增的机制

任务书要求：集中停收/收尾门及暂态计数（调用执行者、本地事件、排队/发送中的上传）；复用 `CB.lock`、中枢现有锁、已有执行器和能力 QoS。无新任务、线程、持久格式、后台重试、缓存、队列或配置项。

为什么不能更小：只在 members.c 等锁，观察不到 CB.pool、本地订阅和直接 AI 上传；只等 CB.pool 又会漏掉 EV 和上传（基线 upload 场景有独立失败证据）。本次只为这些已有生命周期补计数和停收，未逐服务打补丁。

## 要改的文档

主会话按最终验收结果再更新 backlog、ui-dependencies 的状态；能力清单未变化，无需重新生成。共享文档未修改。

## 现存检查的删除核查

| 文件和行号 | 结论与理由 |
| --- | --- |
| `system/services/ai/ai_service.c:98–105` 的 `turn_allowed` | 不能删：AI 回答、UI 回调、录音缓冲比上传活得久，不全部在 CB.pool 内 |
| `system/ui/ui_task.c:349–372` 的页面/浮层归属及 epoch 复核 | 不能删：发布状态和 UI 实际销毁页面之间仍有异步窗口；中央门不能替代页面所有权 |
| `system/services/audio/audio_service.c:157–162`、`:511–550` 的播放归属检查 | 不能删：任务要求不改音频，已完成回答的声音缓存也独立于上传和能力回执 |
| `system/services/data/data_caps.c:22–29`、`system/services/time/time_service.c:591–626` | 不是可删的代次检查：原成员绑定及 io_begin 还保护删除/清理；不能借换人收尾删除 |
| 文件、下载的成员绑定 | 不在本次取消或清理范围，未修改其服务或检查 |

## 实测时延与架构审视

固定合成场景，使用 OSAL/模拟器虚拟时钟；100 ms 左右的阻塞由测试边界控制，不代表真实设备自然负载分布。普通构建：闹钟/提醒 100.980 ms、儿童取消 84.182 ms、通知含排队订阅 115.906 ms；数据写入还包含真实持久化。只测每场景一次，不将其当统计分位数。

【实现】入口仍是原子能力；按钮和 AI 共用成员策略。收尾是策略内部机制，不新增给 AI 的配置或能力。数据、事件和上传的服务归属不变；长作业不改。现存页面和音频的异步归属检查仍必需，不能用此次收尾宣称隔离已被完全证明。

## 命令与最终核查

按总规则配置两套构建，ASan 仅额外开 `INK_SANITIZE=ON`。为使用已安装的中枢测试依赖，两套均配置 `-DINK_HUB_PYTHON=$INK_DEPS/hub/.venv/bin/python`；`PYTHONPATH` 指向当前仓库代码，未安装或升级依赖。以下构建和测试按序执行，没有并行两轮，也未启动窗口 GUI。

```sh
taskpolicy -c utility cmake --build build -j3
taskpolicy -c utility env SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 PYTHONPATH="$PWD/hub/src" "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j2
taskpolicy -c utility cmake --build build-asan -j3
taskpolicy -c utility "$INK_DEPS/build-deps/pyenv/bin/python" tests/member_quiesce/run.py --baseline ed3d122665cf704bb77a962f1fe38d66f6febe01
taskpolicy -c utility env SDL_VIDEODRIVER=dummy UBSAN_OPTIONS=halt_on_error=1 PYTHONPATH="$PWD/hub/src" "$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j2
```

【实测】普通全量 116.45 s，内存版 222.11 s（宿主耗时，与上方模拟器时延分开）。`git diff --check` 为 0；相对基线的规定隐私正则为 0 命中；差异仅 12 个相关文件，未触及音频、文件服务、system/ui 和共享文档。构建产物及探针副本均在忽略目录，不提交。
