# 第十一轮审计补充实验

对象 `c95f48c5bc398178b512c3babc19e475217fbe18`，从仓库根目录运行。实验只链接本地构建库或原样 `#include` 产品源码；不修改产品、既有测试或构建配置，不访问真实设备、网络服务或短信通道。

**这些是问题的见证程序，不是“exit 0 即产品正确”的回归测试。** 请对照 `AUDIT` 输出理解业务状态。全部最终实验在普通和 ASan/UBSan 构建运行；退出码均为 0。ASan/UBSan 未报错不代表并发/协议问题不存在，亦未启用 LSan。

## 编译

先按 TASK.md / report.md 配置并构建 `build-audit`、`build-audit-asan`。构建助手从 `sim-systest` 的实际编译和链接参数生成审计专用可执行文件，替换其 `ink_main`；每个实验对象/可执行文件留在选定构建目录。

```sh
audit_exp=docs/audits/2026-11-04-codex-system-r2/experiments
audit_build=build-audit
for audit_source in service_probe.c ai_race_probe.c remote_batch_probe.c exec_start_probe.c ui_queue_probe.c; do
  python3 "$audit_exp/build_probe.py" "$audit_build" "$audit_source"
done
```

将 `audit_build` 换成 `build-audit-asan` 可生成内存检查版本。

## 运行及实际输出

### 网络、短信、会话内存失败

```sh
for audit_case in sms net rt; do
  AUDIT_CASE="$audit_case" ASAN_OPTIONS=detect_leaks=0 "$audit_build/service_probe" \
    --profile simulator/profiles/metalio-plus.json \
    --script "$audit_exp/services.script" \
    --assets "$audit_build/assets" --content "$audit_build/test-content" \
    --out "$audit_build/probe-$audit_case" --no-random-outages --trace
done
```

`services.script` 提供两张模拟卡和两种网络，18 s 时撤去蜂窝覆盖，40 s 结束。所有参数均是虚构实验数据。

- **sms → AUD12-002**：Schema 通过、参数完整，`requested_bytes=321 requested_chars=109`；驱动收到 `actual_bytes=319 ends_abc=0 ends_a=1 hal_return=0`；`final_receipt=done`；模拟模组读取结果 `modem_sent=1 stored_bytes=319 equals_requested=0`。hook 只观察传给真实模拟 HAL 的字符串，再调用原函数，没有伪造发送结果。
- **net → AUD12-001、005**：先设 Wi-Fi，再 LTE，再自动，等待覆盖消失后最后手动设 Wi-Fi。依次输出：

  ```text
  wifi prefer=1 type=1 up=1 hub=1 online_count=1
  wifi_to_lte prefer=2 type=2 up=1 hub=1 online_count=1
  auto prefer=0 type=2 up=1 hub=1 online_count=1
  auto_cell_down_wifi_available prefer=0 type=2 up=0 hub=0 online_count=1
  manual_wifi prefer=1 type=1 up=1 hub=1 online_count=2
  ```

  `online_count` 来自实际注册的 hub 上线回调，不是最终才填入的 `net.connects` 指标。最终普通构建 Trace 显示 5.200103 s 切 LTE 时没有连接失效/握手；18 s 丢覆盖、23 s 才出现 connection lost。
- **rt → AUD12-003**：仅在 `rt.c` 调用 `ink_hub_call` 的边界，对第一次调用返回真实接口定义的 `INK_HUB_ERR_NOMEM`，不调用回调（与 `hub_link.c:513-515` 一致）。得到 `sid=1 open=1 inflight=1 calls=1 closed=0`；12 s 后内存失败注入已撤销，执行已注册的重连回调，仍 `open=1 ready=0 inflight=1 calls=1 closed=0`。该实验验证确定的错误返回路径，未声称真实分配器随机耗尽。

### AI 回答与下一轮按键交错

```sh
ASAN_OPTIONS=detect_leaks=0 "$audit_build/ai_race_probe" \
  --profile simulator/profiles/metalio-plus.json --script "$audit_exp/short.script" \
  --assets "$audit_build/assets" --content "$audit_build/test-content" \
  --out "$audit_build/probe-ai-race" --no-random-outages
```

**AUD12-004**。启动真实产品服务，通过 UI 任务按下/释放 AI 键。真正的 hub 回复在 `AI.lock` 解锁、`close_session()` 之前被精确暂停；让 UI 任务调用下一次 `ink_ai_key(true)` 后继续旧回复。观察：

```text
inside_new_press old_sid=1 new_sid=2 seq=2 mic_failed=0 rt_count=1
after_old_reply hook=1 new_sid=2 new_seq=2 current_session=0 capturing=1 up_open=1 rt_count=0
```

新一轮成功开启且麦克风可用，旧回复随后误关新会话。没有直接伪造 AI 内部 sid/轮次，也没有把新轮放到非 UI 线程执行。

### 远程更新边界、UI 拥塞

```sh
for audit_binary in remote_batch_probe ui_queue_probe; do
  ASAN_OPTIONS=detect_leaks=0 "$audit_build/$audit_binary" \
    --profile simulator/profiles/metalio-plus.json --script "$audit_exp/short.script" \
    --assets "$audit_build/assets" --content "$audit_build/test-content" \
    --out "$audit_build/probe-$audit_binary" --no-random-outages
done
```

- **remote_batch_probe → AUD12-006**：用一个 4×4 白色画布隔离更新事务，解码和 show_update 均为原始产品实现，分别在媒体与 UI 任务执行。A 的一块带 LAST，B 的首块不带 LAST；两块都到达后放行 UI。输出 `shown_A=0 shown_partial_B=0 updates=1 owed=1`，未完成的 B 也变黑（本应保持 255）。该实验只观察实际显示源 `px`，未将简化画布当作整页硬件刷新测试。
- **ui_queue_probe → 通过**：阻塞 UI 后投递 80 个独立节点，得到 `wake_queue=24 busy=1 waiting=1`；放行后 `count=80 fifo=1 busy_during_jobs=1 waiting=0`。检查唤醒消息队列已满时任务仍全部按序执行，摘链后的运行区间也保持 busy。

### 执行器首次启动

```sh
ASAN_OPTIONS=detect_leaks=0 "$audit_build/exec_start_probe" \
  --profile simulator/profiles/metalio-plus.json --script "$audit_exp/exec.script" \
  --assets "$audit_build/assets" --content "$audit_build/test-content" \
  --out "$audit_build/probe-exec-start" --no-random-outages
```

**AUD12-007**。优先级 3 的调用者把 state 发布为 2 后，用信号量唤醒优先级 5/6 的两个并发调用者；初始化者只暂时延迟 1 ms，不等待其他任务放行。三者均 `core=-1`，沿用双核档案。200 ms 观察仍 `state=2 low_done=0 high_done=0`，1 s 结束时两个高优先级任务一直在 yield 循环。observer 优先级 10，用来读取状态。此为确定调度交错，不是对自然出现概率的估计。

## 内存账和结果文件

```sh
python3 "$audit_exp/static_inventory.py"
python3 "$audit_exp/stack_inventory.py"
```

前者读取主机对象 BSS 符号区间；后者以原编译选项加 `-fstack-usage` 重编译到 `build-audit/audit-stack`，无链接或产品修改。结果不能代替目标 RV32 map、完整调用栈或设备内存测量。

[results.json](results.json) 保留本轮 ASan 实验的简短输出及普通场景的关键指标、栈帧清单。原始构建/CTest/实验日志位于 `/tmp/ink-audit11-*.log`、`/tmp/ink-r2-*.log`，不随审计目录交付；Trace 和场景完整指标在相应构建目录。早期实验程序初始化不完整造成过一次测试自身失败，补齐后复跑完成，未将其当作产品发现。

`sync_findings.py` 仅将 `findings.json` 和 `verification.json` 同步到本轮报告的对应段落，不改产品或历史报告。
