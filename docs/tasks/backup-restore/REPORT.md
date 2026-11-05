# backup-restore 开发报告

基线：`a51a804c`。只开发设备服务、策略接口和模拟测试；未连接真实设备、服务器或付费接口。接口和证据边界见[实现说明](../../system/backup-restore.md)。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| B1 备份清单与上传回执 | 通过，限成员资料【实测】 | 集合、阅读位置、个人文件、空文件；版本回执跨重启；中枢收到与云端持久性分开 | `python tests/backup_restore/run.py --build build` 的 seed、receipt-reboot |
| B2 预览、覆盖确认、未恢复项目 | 通过【实测】 | 人工确认、家长密码及错误密码；新编辑使旧预览失效；未安装集合明确列出；拒绝坏哈希、跨成员、越界路径和重复条目 | 同上，stale、member-confirm、pin、corrupt、malicious |
| B3 备份→清空设备→恢复 | 通过【实测，模拟器】 | 实际调用清空能力并重启；两位成员的记录编号/版本/删除/下个编号、文件和位置复原；再次重启后真实阅读器回到第 4 页 | 同上，wipe、restore-cut、restore-resume、verify-reboot |
| B4 断电、断网后继续 | 通过【实测】 | 上传丢 ACK、双向断网、传输时退出进程、6 个 Flash 写入撕裂点；重新进入原成员后继续 | 同上，upload-cut/resume、network、restore-network、member-resume、flash-cut/verify |
| B5 成员隔离 | 通过【实测】 | 备份/清单/进度/任务按成员；孩子不读成人；确认期间切成员作废；外来成员清单拒绝；卡暂存随成员和清空一同删除 | 同上，member-confirm、member-resume、malicious；串联场景 |
| G1 全量测试、内存检查与预算 | 普通构建 105/105；ASan/UBSan 待跑【实测】 | `INK_WERROR=ON`；原预算通过，未修改任何预算；无跳过 | `cmake --build build -j4`；`ctest --test-dir build --output-on-failure -j3` |
| G2 原子能力与 AI | 通过【实测】 | 5 个调用、2 个成员事件；统一能力门和密码/确认门；能力目录与运行时一致 | 全量中的 capability-table、integration-ai-surface |
| G3 依赖表状态 | 已更新【实现】 | 只改“备份和恢复”状态格为“部分”，保留原判断 | `git diff -- docs/system/ui-dependencies.md` |
| G4 页面 | 不适用【实现】 | 此任务无页面和动画 | — |
| G5 服务层范围 | 通过【实现】 | 仅服务、必要接口、启动装配、模拟中枢、构建和测试；无界面实现改动 | `git diff --stat` |
| P1 差异与隐私检查 | 待最终提交后检查 | — | — |

测试命令中的 `python`、`ctest` 使用 `$INK_DEPS/build-deps/pyenv/bin/`。中枢测试环境从 `hub/uv.lock` 按版本和 SHA-256 安装，未改依赖：本地 `hub/.venv` 用 `.pth` 指向当前 `hub/` 源码；配置加 `-DINK_HUB_PYTHON="$PWD/hub/.venv/bin/python"`。初轮全测发现空闲时提前建后台任务会多占 360 字节内部 RAM，以及状态监听槽被占用；改为仅有持久任务才恢复执行，不增加监听槽。存储条件写的哈希计算独立栈帧，普通写入不增加该栈占用。最终普通全测退出 0、105/105。

## 没做到的

- ASan/UBSan、反向对照和最终隐私检查正在继续，尚不算完成。
- 未实现设备级设置、成员身份/PIN、网络/配对凭据的备份恢复；当前恢复范围是已存在成员自己的资料，未取消这些需求。
- 未实现真实中枢的对象存储、历史版本选择、跨设备合并；任务仅连接 `sim_hub`。Linux/gcc 和实机未在本机运行，新增 `read`、`write`、`fgets` 类返回值已逐处检查。
- 需要存储卡暂存；无卡时明确失败，暂存不足或超限不静默漏项。上限：128 项、8 层目录、Flash 文件/集合快照 1 MiB；个人卡文件流式处理。恢复是逐项原子提交，非整台设备事务；完成后需要重启加载缓存。

## 和设计不一致的

- 采用不可变整份清单及 `backup/commit`、`backup/latest` 协调；块传输沿用设备协议 §3.8。协议 §3.9 的增量推送、全量对账、每集合 dataset/restore_token 换代尚未接入，不能声称完整同步已交付。
- 系统服务设计 §2.1 的非凭据设备数据备份未覆盖；当前仅成员范围。AI 活动日志有备份，但恢复保留本机现有审计记录，清单标 `kept_local_audit`；未安装集合标 `collection_unavailable`。共享内容按需下载，外部动作与过期提醒不重放；清单外本地文件保留。原设计文档未改。

## 改过的已有测试

仅 `tests/integration/ai_surface.c`：`FOR_AI` 增加 `backup.status`、`backup.manifest`、`backup.now`、`restore.preview`、`restore.apply`，后者必填 `preview`；`EVENTS` 增加两个备份事件。未删测试、放宽断言或更改旧能力预期。

## 要用户定的

设备级资料的选择性恢复、恢复其他成员身份/权限的界面及授权范围仍需确定。当前暂按“只恢复现有成员自己的资料、保留清单外文件和本机审计记录”推进，未把暂定方案升级为需求。

## 顺手发现

无另修范围外问题。未 push、未创建 PR、未启动窗口模拟器；CTest 的 GUI 项使用既有无窗口 smoke 模式。
