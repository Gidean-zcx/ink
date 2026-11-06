# backup-restore 开发报告

基线：`a51a804c`。只开发设备服务、必要接口和模拟测试；未连接真实设备、服务器或付费接口。接口和证据边界见[实现说明](../../system/backup-restore.md)。

2026-11-06 复查修复从 `cf258b3b` 接续。BR-R2（严重）、BR-R1/BR-R3（高）正在处理；下文原验收结果仅代表上一轮，不代表复查问题已修复。本轮将补身份复用、八层目录、数 MiB 文件及后台查询回归和反向对照，完成后重跑普通与内存检查全量。复查没有另列中级问题。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| B1 备份清单与上传回执 | 通过，限成员资料【实测】 | 集合、阅读位置、个人文件、空文件；版本回执跨重启；中枢收到与云端持久性分开 | `python tests/backup_restore/run.py --build build` 的 seed、receipt-reboot |
| B2 预览、覆盖确认、未恢复项目 | 通过【实测】 | 人工确认、家长密码及错误密码；新编辑使旧预览失效；未安装集合明确列出；拒绝坏哈希、跨成员、越界路径和重复条目 | 同上，stale、member-confirm、pin、corrupt、malicious |
| B3 备份→清空设备→恢复 | 通过【实测，模拟器】 | 实际调用清空能力并重启；两位成员的编号/版本/删除/下个编号、文件和位置复原；再次重启后真实阅读器回到第 4 页 | 同上，wipe、restore-cut、restore-resume、verify-reboot |
| B4 断电、断网后继续 | 通过【实测】 | 上传丢 ACK、双向断网、传输时断电；6 种 Flash 故障配置中 5 次实际撕裂后恢复通过，另 1 次为完整执行对照；回到原成员后继续 | 同上，upload-cut/resume、network、restore-network、member-resume、flash-cut/verify |
| B5 成员隔离 | 通过【实测】 | 清单/进度/任务按成员；孩子不读成人；确认期间换人成为无效；恢复目录沿用原成员；卡暂存随成员和清空一同删除 | 同上，member-confirm、member-resume、malicious、api；串联场景 |
| G1 全量测试、内存检查与预算 | 通过，普通与 ASan/UBSan 各 105/105【实测】 | 两轮均无跳过；各含 32 次备份恢复启动，无警告；未改预算 | `cmake --build build -j4`；`ctest --test-dir build --output-on-failure -j3`；同样配置在 build-asan 加 `INK_SANITIZE=ON`，同样构建/测试 |
| G2 原子能力与 AI | 通过【实测】 | 5 个调用、2 个成员事件；统一能力门和密码/确认门；目录与运行时一致 | 全量中的 capability-table、integration-ai-surface |
| G3 依赖表状态 | 已更新【实现】 | 只改“备份和恢复”状态格为“部分”，保留原判断 | `git diff a51a804c -- docs/system/ui-dependencies.md` |
| G4 页面 | 不适用【实现】 | 此任务无页面和动画 | — |
| G5 服务层范围 | 通过【实现】 | 服务、启动装配、模拟中枢、构建和测试；无界面实现改动 | `git diff --stat a51a804c` |
| V1 反向对照 | 6 组均得到预期失败【实测】 | 隔离生成变体对象和归档，正式源码、构建归档不变；见下表 | `python tests/backup_restore/reverse.py --build build` |
| P1 差异、文件/链接与隐私 | 通过，隐私扫描 0 匹配【实测】 | 仅相关文件；无保护文档改动、无大于 5 MiB 文件；链接与 `diff --check` 通过；按任务规定扫描基线到最终 HEAD | `git diff --stat a51a804c`；`git diff --check a51a804c`；任务规则中的隐私扫描命令 |

反向对照各自撤掉一个保护；日志项另模拟允许发生的“旧日志清理未提交”。

| 撤掉的保护 | 实际失败证据 |
| --- | --- |
| 安装前核对版本 | api 2 条断言失败，新编辑被旧快照覆盖 |
| 快照使用新日志代 | api-reboot 1 条失败，旧的较高版本日志复活；保留换代、同样不提交清理时，两次启动均通过 |
| Flash 条件替换 | api 2 条失败，队列里的较新写入被覆盖 |
| 卡文件条件替换 | api 2 条失败，变过的原件被替换 |
| 目录绑定原成员 | api 2 条失败，目录出现在后来成员空间 |
| 空闲时不创建后台任务 | 原 AI 场景预算失败，内部 RAM 最低 32,408 B，小于既有 32,768 B；正式实现同场景 32,880 B |

【实测，模拟器】新增场景内部 RAM 最低余量 43,920 B；普通构建存储任务栈估计峰值 3,472 / 3,584 B。ASan/UBSan 是宿主检查，不是实机内存测量。普通全测退出 0（59.91 s），内存检查全测退出 0（101.66 s）。

增量接口：`ink_db_export/import`；`ink_storage_restore`；`ink_fs_restore`、`ink_fs_mkdir_member_generation`；`ink_jobs_held`；`ink_hub_on_blob`。SHA-256 仅复用为公共构建目标，未加依赖。能力总表自动生成，新分组使原关联链接编号随之调整。

命令中的 `python`、`ctest` 使用 `$INK_DEPS/build-deps/pyenv/bin/`。中枢测试环境从 `hub/uv.lock` 按版本和 SHA-256 安装，未改依赖；本地 `hub/.venv` 用 `.pth` 指向当前 `hub/` 源码，两个构建均加 `-DINK_HUB_PYTHON="$PWD/hub/.venv/bin/python"`。初轮全测发现空闲启动及监听槽影响原测试，已改为只有持久任务才恢复执行，不增加监听槽；存储条件写的哈希计算使用独立栈帧，普通写入不增加该栈占用。

## 没做到的

- 未实现设备级设置、成员身份/PIN、网络/配对凭据的备份恢复；当前恢复范围是已存在成员自己的资料，未取消这些需求。
- 未实现真实中枢的对象存储、历史版本选择、跨设备合并；只连接 `sim_hub`。Linux/gcc 和实机未在本机运行，新增读写及 `fgets` 类调用已检查返回值。
- 需要存储卡暂存；无卡时明确失败，暂存不足或超限不静默漏项。上限为 128 项、8 层目录、Flash 文件/集合快照 1 MiB；个人卡文件流式处理。恢复逐项原子提交，非整台设备事务；完成后需要重启加载缓存。

## 和设计不一致的

- 采用不可变整份清单及 `backup/commit`、`backup/latest` 协调；块传输沿用设备协议 §3.8。协议 §3.9 的增量推送、全量对账、每集合 dataset/restore_token 换代未接入，不能声称完整同步已交付。
- 系统服务设计 §2.1 的非凭据设备数据备份未覆盖；当前仅成员范围。AI 活动日志有备份，但恢复保留本机审计记录，清单标 `kept_local_audit`；未安装集合标 `collection_unavailable`。共享内容按需下载，外部动作与过期提醒不重放；清单外本地文件保留。原设计未改。

## 改过的已有测试

仅 `tests/integration/ai_surface.c`：`FOR_AI` 增加 `backup.status`、`backup.manifest`、`backup.now`、`restore.preview`、`restore.apply`，后者必填 `preview`；`EVENTS` 增加两个备份事件。未删测试、放宽断言或更改旧能力预期。

## 要用户定的

设备级资料选择性恢复、恢复其他成员身份/权限的界面及授权范围仍需确定。当前暂按“只恢复现有成员自己的资料、保留清单外文件和本机审计记录”推进，未把暂定方案升级为需求。

## 顺手发现

无另修范围外问题。未 push、未创建 PR、未启动窗口模拟器；CTest 的 GUI 项使用既有 SDL dummy 无窗口 smoke 模式。
