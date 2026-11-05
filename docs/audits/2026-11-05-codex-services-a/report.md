# 第十七轮 A 路审计：存储与后台

> 这一路运行被服务方的内容分类器中途切断（`exit 1`），报告停在切断时的样子，里面的“进行中”就是这个意思。没有写完的不代表通过。

状态：进行中。审计日期：2026-11-05。对象提交：`4d23fc2a62f9ae39cc1e1bb0a1f1d98ce2cbb27f`（`docs: the next audit in three narrower parts`）。

## 一页结论

尚未完成；以下随核查和复现逐项补充，未记录为已验证的事项不代表通过。

本轮按 `TASK.md` 只读审计产品、测试与历史资料，仅在本目录写报告及补充实验，并使用允许的 `build-audit/`、`build-audit-asan/` 构建目录。由主代理独立完成，不调用子代理或外部模型，不访问设备与实机数据，不进行密码尝试次数或等待时间实验。

## 新问题

当前已确认 3 条，继续核查其他模块。

## 历史修复核查

待逐条核查 AUD17-001、AUD17-002、AUD16-004～006、AUD15-001～004 及相关项。

## 范围与进度

| 范围 | 当前进度 |
| --- | --- |
| 升级包、安装、状态持久化与下载 | 待核查 |
| 恢复出厂、启动依赖及清理范围 | 待核查 |
| 文件服务、持卡、U 盘及文件作业 | 待核查 |
| 数据存储、存储服务及 `data.status` | 待核查 |
| 任务列表、取消与完成的配对 | 待核查 |
| 软总线、原子能力及 AI 接口 | 待核查 |

## 验证记录与边界

- 开始时执行 `git log -1 --format=fuller`、`git status --short`；工作树已有删除项 `docs/audits/2026-11-04-codex-services-s2/experiments/pin_cut_probe.c` 及未跟踪的本目录 `codex-run.log`，均不修改、不还原。
- 本报告只评价本地对象提交；不联网同步决策板或 Issue/PR，不改现有决策与资料。
- 构建、测试命令、结果和未覆盖范围待补充。

## 阶段记录：文件、持卡与任务（已复现）

### AUD18-101｜高｜个人目录打开失败被当成不存在，清理返回成功并失去重试标记

【复现】见 experiments/file_probe.c + file_boundary.c 和 file-probe.log：failed_root_open wipe=0 remaining=0；同一探针深目录对照为 result=0 remaining=-3。

【代码核查】system/services/file/file_service.c:451；system/services/file/file_service.c:479；system/policy/system_actions.c:155。

影响：在真实个人文件存在时，仅令 /content/.m 的一次 hal_fs_opendir 返回 NULL，ink_fs_wipe_personal 仍返回 0，文件可完整访问。开机恢复据此删除 reset.card；原身份清除后默认 owner 可继承遗留文件。标记删除链为代码核查，完整重启链待补充。

建议：区分不存在与无法打开，不能确定目标不存在时保留清理标记；全部范围确实清空后再确认完成。

### AUD18-102｜高｜目录句柄和短文件操作未计入持卡，U 盘独占可在卡仍被使用时成功

【复现】experiments/file_probe.c：directory_vs_usb nopen=0 lend=0 active=1 next=1。

【代码核查】system/services/file/file_service.c:346；system/services/file/file_service.c:401；system/services/file/file_service.c:488；system/services/file/file_service.c:969。

影响：无故障注入：ink_fs_dir_open 打开 /me 后 FS.nopen=0，ink_usb_disk_start 成功，已有目录仍可继续读到条目。write_atomic、mkdir、move、remove 等同样没有覆盖整个 HAL 操作的持卡。实机 FAT 与 USB 同时访问的损坏风险为推断，本轮未接触设备。

建议：给所有卡访问统一获取/释放租约；目录关闭才释放，短操作持有至完成。检查借出状态与增加计数须在同一锁区。

### AUD18-103｜中｜先提交文件作业后登记任务，先完成的作业会留下永不结束的任务

【复现】experiments/file_boundary.c 仅在提交返回处让调用者停 500 ms，后台运行原产品：file_state=2 task=1，job.list 中 state=running。

【代码核查】system/services/file/file_service.c:874；system/services/file/file_service.c:885；system/services/file/file_service.c:782。

影响：另一执行器在 ink_job_submit 返回后完成小文件拷贝；job_finish 当时使用尚为 0 的 j->task，之后 ink_task_begin 创建的条目无人终结。用户和 AI 看见永久 running，job.running 不减。

建议：发布作业前完成任务登记和上下文初始化；提交失败时配对终结任务。完成回调使用不可变作业上下文，不能依赖以后才填的全局槽位。

基线：`cmake` 配置与 `cmake --build build-audit -j 4` 均退出 0；相关 CTest 8/8 通过，见 `experiments/ctest.log`。

## 阶段记录：升级、数据存储

### AUD18-104｜高｜升级状态写入失败仍切槽，自检后跳过资源并把升级判为完成

【复现】experiments/run_update_matrix.py 的 state-control 和 state-refused 两启动链；见 update-matrix.log、update-results.json。

【代码核查】system/services/update/update_service.c:132；system/services/update/update_service.c:149；system/services/update/update_service.c:333；system/services/update/update_service.c:373；system/services/update/update_service.c:868；system/services/update/update_service.c:952。

影响：升级状态提交返回 -1 后原产品仍重启；下次启动 code=200、marked_valid=1、resource_bytes=0、resource_failed=0，并写出空的完成状态。同包对照写入 12288 字节资源。新应用与旧资源组合及失去恢复入口已确认，实机表现未测。

建议：状态写入应提供成功回调并等待持久化，无法建立恢复记录时禁止切槽。新槽试运行但记录缺失或读失败时进入可恢复错误，不能当作没有资源。

### AUD18-105｜高｜资源未恢复时接受只含应用的新版本，覆盖唯一恢复记录

【复现】run_update_matrix.py resources-replaced：half-resources res=2 resource_failed=1；new-version-without-resources code=300 state= path= resource_bytes=0。

【代码核查】system/services/update/update_service.c:333；system/services/update/update_service.c:338；system/services/update/update_service.c:877；system/services/update/update_service.c:926。

影响：200 版资源写入 8192/12288 字节后失败，状态保留 resources；保留原包，随后安装独立文件中的 300 版 app-only 包，最终 state/path 为空、resource_bytes=0。没有补写剩余资源，却清除了恢复入口。

建议：未恢复资源作为独立持久事务保留；只有新包覆盖全部未完成资源才能替代旧事务，否则先恢复或拒绝新安装。

### AUD18-106｜中｜只记最近一个失败版本，第二次回滚后重新接受较早的失败版本

【复现】run_update_matrix.py blocked-history：rollback-200 prior_failed_package=9；rollback-300=0。

【代码核查】system/services/update/update_service.c:213；system/services/update/update_service.c:509；system/services/update/update_service.c:985。

影响：100 版先后试用 200、300 并自检失败回滚，200 包最初返回 PKG_BLOCKED(9)，第二次回滚后变为 PKG_OK(0)。扫描与在线检查可再次提示/允许下载已失败版本；人工安装确认仍存在。

建议：保存有界失败版本或包身份集合，并定义人工重试、修订包或升级成功后的清理规则。

### AUD18-107｜中｜段表缺少类型、唯一性、非空和重叠检查，结构非法的已签名包通过校验

【复现】run_update_matrix.py package-table：overlap/duplicate/unknown/zero=0；past_eof/truncated/overflow=7；unsigned_damage=3。

【代码核查】system/services/update/update_service.c:173；system/services/update/update_service.c:211；system/services/update/update_service.c:227；ports/sim/hal/hal_ota_sim.c:155。

影响：用模拟器公开测试签名重新签署的重叠段、重复 app、未知 kind=99、零长度 app 包均返回 PKG_OK；超出文件、截断、偏移溢出及未重新签名的改动被拒绝。这不是绕过生产签名，而是发布端结构校验不足；模拟 HAL 也接受任意资源 kind/offset。

建议：要求唯一非空 app、合法且不重复的资源段、头后偏移、分区大小上界、无溢出的长度检查和区间不重叠；模拟 HAL 应执行边界校验。

### AUD18-108｜中｜活动下载复用可变 offer，宣告新版本就绪但实际文件仍是旧包

【复现】run_update_matrix.py download-offer-changed：offer=300 dl_off=12288；结束 state=ready，header_code=200 size=196820。

【代码核查】system/services/update/update_service.c:528；system/services/update/update_service.c:699；system/services/update/update_service.c:705；system/services/update/update_service.c:734。

影响：下载 200 版已有 12288 字节时，新一次 update.check 把活动下载使用的 size/blob/version 换成 300 版。结果 ready、最终文件头仍 code=200，文件长却是新包的 196820 字节；不同内容也可能混包校验失败并丢弃续传文件，后者为代码推断。

建议：开始下载时固定 code/version/blob/size/path；后续 check 只更新候选。续传核对包身份，最终校验同时核对目标身份与长度。

### AUD18-109｜高｜删除成员集合时直接清零仍在定时链表中的节点，其他集合延迟保存不再执行

【复现】experiments/storage_probe.c：storage-probe.log sentinel=0、owner_durable=0；AUDIT_KEEP_MEMBER=1 的 storage-control.log sentinel=1、owner_durable=1。

【代码核查】system/services/data/datastore.c:999；system/services/data/datastore.c:1004；system/services/data/datastore.c:1006；system/services/res/svc.c:14；system/services/res/svc.c:51；system/policy/members.c:543。

影响：owner 和 kid 各有延迟写入，关闭 kid 集合后调用真实 ink_db_forget_member。它直接清零内嵌 flush_at 节点，截断共享截止时间链表；独立的一秒回调消失，超过 60 秒 owner 仍 last=1/durable=0。只省略 forget 的对照回调执行且 durable=1。之后断电会丢失原应落盘的数据，其他服务计时也可能受影响。

建议：释放或清零集合前取消其全部 svc 节点，处理 compact_wanted 对象生命周期，并等待在途工作退役。对照 evict_one 的取消流程统一实现。

