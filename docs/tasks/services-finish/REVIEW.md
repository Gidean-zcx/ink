# 系统服务收尾复查（第 5 轮）

复查进行中，尚未给出最终验收结论。2026-11-05；基线 `fdff779`，复查对象 `86d3ac6609a9d42ff2436c6cafe0c122ba0ab092`。按任务书逐条核对，优先验证上一轮 R4-01 及同类修复；只深挖严重、高问题。本轮只修改本报告，不提交。

已读取任务书、总规则、开发报告和 Git 中的上一轮复查报告。普通与 ASan/UBSan 构建、全量 CTest 均退出 0，各 86/86，分别 25.78 秒和 69.03 秒。所有构建最多 `-j4`、CTest 最多 `-j3`，同一时间只跑一轮；不启动带窗口的模拟器。现有全绿测试之外，已确认下列隔离遗漏。

## 已确认问题

### R5-01（高）：跨成员关闭文件把原成员的文件路径发给下一位的 AI

- **位置**：`system/services/file/file_service.c:240`、`:253`、`:332`、`:343`，`system/policy/cap.c:930`；真实录音链路为 `system/services/audio/media_service.c:493` → `system/services/audio/ogg.c:390`。
- **【实测】复现命令**：`$INK_DEPS/build-deps/pyenv/bin/python build/services-review5-events.py`。脚本位于忽略的构建目录，完整可重建内容待本报告补入。
- **结果**：普通构建真实 `audio.record` 在成人空间开始，切到 `kid` 后正常收尾，无故障注入。模拟中枢实际收到 `file.changed`，`member` 为 `kid`，`path` 为成人的 `/me/SYNTH_OWNER_PRIVATE.opus`；本地订阅者同样收到错误归属。普通文件句柄打开、写入后换人再关闭也复现。同成员关闭/停止录音的两个对照通过；两条换人分支各 2 项隔离断言失败，模拟器均退出 0、`test.finished=1`，脚本退出 1。
- **【实现】原因**：文件句柄已有 `member`，但 `ink_fs_close` 调用 `changed(path, "written")`，丢掉原归属；`changed_for(NULL, ...)` 使总线按发送当刻的当前成员补齐身份。后续总线及传输层的隔离检查因此放行。`audio.record_done` 正确绑定原成员，不能补救同次收尾附带的 `file.changed`。
- **修法**：在释放文件槽前复制文件事件所需的原成员及代次，并通过带归属的事件入口投递；同时核对原子写、移动、建目录等可跨换人的短操作。增加真实录音换人收尾，以及持有文件句柄跨换人的端到端事件断言。
- **范围**：已证实文件路径泄漏和成员事件归属错误，没有声称录音内容或整个成人空间可读。按跨成员异步收尾的现实操作计高。普通证据 `build/services-review5-events-lo8fouwm/`、内存检查证据 `build-asan/services-review5-events-nd59_evb/`；两套结果相同，无 sanitizer 诊断。

### R5-02（高）：移动/拷贝的两条路径分别读取当前成员，换人会把成人文件送进孩子空间

- **位置**：`system/services/file/file_service.c:156`、`:475`、`:477`、`:877`、`:879`、`:896`；调用入口 `system/services/file/file_caps.c:200`、`:239`。
- **【实测】复现命令**：`$INK_DEPS/build-deps/pyenv/bin/python build/services-review5-paths.py`。通过真实 `file.move` / `file.copy`，把 `/me/source.txt` 改名/复制为 `/me/renamed.txt`；只在源物理路径解析完成后、目标解析前暂停任务，再经真实成员服务切到 `kid` 后放行。
- **结果**：孩子随后通过真实 AI `file.read /me/renamed.txt` 读出成人原文 `SYNTH_ADULT_CONTENT`。移动分支成人原路径与成人目标均不存在，文件实际到了孩子目录；拷贝分支成人原文件仍在，孩子目录多出完整副本。同成员移动/拷贝两个对照均通过。换人移动 2 项失败、换人拷贝 1 项失败，模拟器均退出 0、`test.finished=1`，脚本退出 1。
- **【实现】原因**：`FS.edits` 只互斥文件修改与删除清理，不互斥成员切换；两次 `fs_resolve` 各用当时的 `ink_member_current()`，因此同一操作可组合成人源路径和孩子目标路径。拷贝登记的 `j->who/generation` 又取换人后的身份，使后续作业检查认可这份错误组合。调用回执即使回 `stale`，也不能回滚已经发生的跨空间写入。
- **修法**：在操作接受时固定原成员、代次和授权上下文，所有源/目标解析、作业归属与事件均使用同一上下文；在产生副作用前原子确认该上下文仍有效，否则拒绝。补充源已解析、目标未解析时换人的回归，不能只测进入 provider 前换人或作业开始后换人。
- **范围**：这是确定性并发窗口探针，包装只暂停/恢复真实 HAL 边界，不篡改路径或文件结果；未测真机自然触发概率。按现实换人时序下的成员数据泄漏计高。普通证据 `build/services-review5-paths-6jkb9e7h/`；内存检查复跑待补。

SEVERE=0 HIGH=2
