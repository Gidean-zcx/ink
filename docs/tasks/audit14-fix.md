# audit14-fix：batch14 复查的三个中等问题和两个低问题

总规则先读 docs/tasks/codex-rules.md，第 7、8 节优先。报告写到 docs/tasks/audit14-fix/REPORT.md。复查报告是 `docs/audits/batch14/report.md`，修第 1～5 条。

## 要做的

预算：产品代码净增不超过 250 行，能删的特判要删掉。

1. **锁屏受限页面改成页面自己声明**（第 1 条）：
   - 去掉 `cap.c` 里按 `camera.`、`photo.`、`file.`、`app.open` 前缀的特判，去掉 `on_its_page()`、`input_service.c`、`system_actions.c` 里对 `ink_camera_restricted()` 的直接调用；
   - 改成页面（activity）声明“锁屏时可以用”，并列出它允许的能力。策略层、输入服务和回主页的处理只查这个声明。
   相机的锁屏模式照新写法声明。行为不变，lock-camera 的测试照原样通过。
2. **能力自己声明属于哪个应用**（第 2 条）：
   - 去掉 `members.c` 的 `action_app()` 那套前缀和特例，改成能力表里能力自己声明归属的应用（加字段，或者由应用注册时登记它的能力域，选更小的一种，写清理由）；
   - 补上漏网的：相册的 `photo.open`，文件服务的 `file.*` 归文件应用；
   - 加一个测试，列出所有声明了归属应用的能力，并断言：把这个应用从孩子的名单里去掉以后，这些能力都被拒（AI 来源和触摸来源都要测）。电话和短信照 kid-apps 的规矩，始终保留。
3. **批量文件操作的回执**（第 3 条）：回执只给总数、成功数、失败数和失败的文件名（最多 20 个，超过的写“还有 N 个”）；缓冲照这个大小算。files-ops 的测试改成核对新的回执，改了哪些期望逐条列进报告。
4. **时间设置页只在相关变化时重画**（第 4 条）：只对 `time.auto`、`time.zone_auto`、`time.zone`、`member.idle_return_s` 重画；分钟变化照旧只更新文字。加一个测试：停在这页时改一个闹钟，页面不整页重画。
5. **`ink_hub_server_time` 加锁**（第 5 条）：`server_ms`、`hello_sent_us`、`hello_rtt_ms` 的读和写都放进 `id_lock`。

## 验收

1. 每一条都有测试，并做反向对照：撤掉修复，对应的测试失败。
2. lock-camera、kid-apps、files-ops、time-settings 原有的测试照原样通过；期望改了的（只有第 3 条）逐条列出。
3. 全量测试和内存检查通过；S31 能编过；gcccheck 通过。
4. 报告列出删掉的特判（文件和行），以及新增的声明字段。
