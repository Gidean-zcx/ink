# backup-restore 修复报告

2026-11-06。接续复查基线 `1d0d04f0`；[复查](REVIEW.md)当前列出的 0 条严重、2 条高均已处理，没有另列中级问题。修复提交：`b3b70065`、`d12c1156`、`38ee3057`。上一轮报告保存在提交 `a83bef39` 的本文件。证据限 macOS/clang、合成成员、模拟器和本机模拟中枢。

## 验收表

| 编号 | 结论 | 证据 | 命令 |
| --- | --- | --- | --- |
| BR-R4 高：恢复覆盖并发原子写 | 已修复【实现、实测】 | 非阻塞稳定采样及条件安装；缺失文件、分块哈希、采集、成员清理、清空和 USB 归还同类路径已检查。不同大小、同大小和原本不存在三组真实恢复交错均保留完整新正文并拒绝恢复；修改锁忙时及时返回 BUSY，借卡意图后取得的版本在归还时失效 | `run.py`：atomic、atomic-same、atomic-absent、api；`review_reverse.py` |
| BR-R3 高：网络等待占用后台执行器 | 已修复【实现、实测】 | RPC、64 KiB ACK、下载正文均由回执/超时续接；每步最多发送 16 KiB。状态查询返回缓存并异步刷新；预览先排队查 latest。0.2 Mbit/s、固定 RTT 600 ms、零丢包下，原 98,304 B 文件的备份、恢复、回执查询、迟到 RPC/ACK、换人和暂停均完成，进度/解锁的 timeouts、late 均为 0 | `run.py`：slow、slow-preview、slow-switch、slow-pause；`review_reverse.py` |
| BR-R1 原高：目录递归栈 | 保持通过【实测】 | 八层目录完成，无栈警告；普通专项后台栈最高 4,976 B，小于原 8,192 B 配置；撤回迭代遍历仍触发栈警告 | `run.py`：deep；`review_reverse.py` |
| BR-R2 原严重：成员 ID 复用 | 保持通过【实测】 | 有本地资料、清空资料两种环境下，新孩子均取不到旧成人备份、清单、回执或正文；新身份自己的备份可跨重启恢复 | `run.py`：reused-id、identity-reboot；`review_reverse.py` |
| B1 清单与上传回执 | 通过【实测】 | 集合、位置、个人文件、空文件、重启回执；received 和 durable 分开；错误身份回执拒绝，旧无身份日志要求重新备份 | `run.py`：seed、receipt-reboot、wrong-receipt、legacy-journal、slow-status |
| B2 预览、确认与未恢复项目 | 通过【实测】 | 人工确认、家长 PIN、错误 PIN、换人及预览过期校验保留；恶意路径、重复条目、坏哈希及错误身份拒绝；未安装集合有说明 | `run.py`：pin、stale、member-confirm、corrupt、malicious |
| B3 备份→清空→恢复 | 通过，限重新绑定后的成员资料【实测】 | 真正清空并重启；先拒绝新身份认领旧备份，再通过独立合成身份夹具模拟授权重新绑定；两成员资料与阅读器第 4 页跨重启恢复。夹具不是产品重绑定功能 | `run.py`：wipe、rejoin、restore-cut/resume、verify-reboot |
| B4 断电、断网后继续 | 通过【实测】 | 采集断电重建，上传/下载按持久偏移继续；采集换人/暂停、丢失预览检查点和六组实际 Flash 故障恢复均保留 | `run.py`：collect-*、upload-*、network、restore-network、prepare-recover、flash-* |
| B5 成员隔离 | 通过【实现、实测】 | 延续持久身份和运行时代次校验；在途 RPC 使用不可复用 cookie，回执/正文使用固定成员、身份和代次。换人、暂停、错误身份回执均不继承旧传输 | `run.py`：reused-id、member-confirm、member-resume、slow-preview-switch、slow-switch、slow-pause |
| V1 修复反向对照 | 两条高及同类修复均通过【实测】 | 撤掉稳定采样/非阻塞安装：api 4 条、并发替换和并发新建各 2 条断言失败；只撤掉非阻塞安装、USB 归还换代各 1 条失败。网络改回同步等待：快链路通过，原 98,304 B 慢链路 3 条失败、1 次能力超时。正式源码及归档未变 | `review_reverse.py --build build --evidence build/backup-restore-q88swena`；证据 `build/backup-review-reverse-alpz_srt/` |
| V0 原六组反向对照 | 通过【实测】 | 版本 2 条、日志代 1 条、Flash 条件写 2 条、卡文件条件写 2 条、目录成员绑定 2 条失败；空闲任务变体内部 RAM 最低 32,408 B，低于原 32,768 B 预算；未排队清理的正向保留对照通过 | `reverse.py --build build`；证据 `build/backup-reverse-kpsw3oqo/` |
| G1 全量测试 | 通过【实测】 | 最终代码普通 105/105，66.24 s，退出 0，无跳过；其中备份专项 63 次启动、零断言失败、零警告 | 下方普通构建/CTest 命令；`build/repair-ctest.log` |
| G2 内存检查与预算 | 通过【实测】 | 最终 ASan/UBSan 105/105，112.75 s，退出 0，无跳过、无内存/未定义行为诊断。两套专项各 63 次启动，零失败、零警告、timeouts/late 均为 0。普通专项内部 RAM 最低 41,144 B，后台栈最高 4,976 B，存储栈最高 3,472 B；原预算未改，ASan 插桩栈不作设备栈依据 | 下方内存检查构建/CTest 命令；`build-asan/repair-ctest.log` |
| G3 能力与 AI | 通过【实现、实测】 | 原五调用、两事件继续走统一能力、确认、PIN 和成员绑定，无新增页面或常驻任务；传输仍属于备份服务，文件稳定采样/条件替换仍属于文件服务，不绕过平台接口 | 全量中的 capability-table、integration-ai-surface、determinism 系列；相关差异核查 |
| P1 差异、文件、链接、隐私 | 通过【实测】 | 本轮 7 个相关文件，无超过 5 MiB 的文件；链接、报告结构及差异检查通过。按任务规则扫描原任务基线和本轮基线到最终 HEAD：均 0 匹配（grep 退出 1）；无构建产物或凭据 | `git diff --check a51a804c..HEAD`；`git diff --stat 1d0d04f0..HEAD`；任务规则的隐私扫描 |

构建最多 `-j4`，ctest 最多 `-j3`，构建和全量测试串行。依赖只读复用 `$INK_DEPS`；不启动带窗口的模拟器、不连接设备、不访问真实服务器、不 push。

两套按[总规则 §5](../codex-rules.md)配置，`INK_WERROR=ON`；内存检查只增加 `INK_SANITIZE=ON`，保留已有 `INK_HUB_PYTHON=$PWD/hub/.venv/bin/python`。未升级依赖。实际执行：

```sh
cmake --build build -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build --output-on-failure -j3
cmake --build build-asan -j4
"$INK_DEPS/build-deps/pyenv/bin/ctest" --test-dir build-asan --output-on-failure -j3
"$INK_DEPS/build-deps/pyenv/bin/python" tests/backup_restore/reverse.py --build build
"$INK_DEPS/build-deps/pyenv/bin/python" tests/backup_restore/review_reverse.py --build build --evidence build/backup-restore-q88swena
```

专项入口：[run.py](../../../tests/backup_restore/run.py)、[原反向对照](../../../tests/backup_restore/reverse.py)、[复查反向对照](../../../tests/backup_restore/review_reverse.py)。普通、内存检查最终专项证据分别为 `build/backup-restore-of3r4eu9/`、`build-asan/backup-restore-jhn_wrxm/`；全量日志为各构建目录的 `repair-ctest.log` 和 `Testing/Temporary/LastTest.log`。复查反向对照共 12 个源码变体、15 个负向场景，编译、运行和断言原文均留在上表证据目录。

## 没做到的

- Linux/gcc 和 S31 实机本轮未运行，没有对应通过证据。新增/修改的文件读写均检查返回值，未以 `(void)` 忽略 read/write/fgets。
- 延续原任务范围：真实中枢身份重绑定、跨设备身份恢复、旧无身份备份迁移、设备设置和成员凭据备份仍未实现，未取消需求。
- 仍依赖存储卡暂存；原上限保持：128 项、8 层目录、单个 Flash 文件/集合快照 1 MiB，模拟中枢单对象 64 MiB；恢复逐项原子提交，完成后重启加载缓存。

## 和设计不一致的

- 延续上一轮：不可变整份清单与 `backup/commit`、`backup/latest` 协调；设备协议 §3.9 的增量对账和 dataset/restore_token 换代尚未接入。
- 与当前身份不符的旧日志只在内存中按空记录处理，不继承旧回执或授权；本轮未实现身份迁移。未修改设计、需求或依赖表迁就实现，依赖表继续保持“部分”。

## 改过的已有测试

- `device.c` 新增真实恢复与并发原子写、开放写句柄回归；`run.py` 新增不同大小、同大小、原本不存在三组场景；`review_reverse.py` 新增稳定采样反向对照。原断言、预算和能力期限不变。
- 三条已有状态期望改为异步结果：清空后的身份及复用 ID 的孩子请求预览，先检查 queued，再等待 failed；旧预览拒绝安装后检查 failed 和恢复成功计数为 0（原来检查 none）。仍逐条验证没有旧成人清单或正文，未放宽授权条件。
- 整文件验证由栈上 128 B 改为 PSRAM 4 KiB 读取，仍检查每一个字节及完整长度。8 MiB 恢复曾在原 500 s 场景结束前完成，但逐 128 B 验证未结束；调整验证批次后全场景完成，未修改 500 s 上限、业务截止时间或预算。
- 新增慢链路备份、恢复、回执刷新、迟到 RPC、迟到 ACK 换人及暂停；网络反向对照先验证快链路通过，再要求慢链路检出。未改设备档案，只在忽略的证据目录派生网络参数。
- 新增真实原子写持锁期间的条件安装检查，以及 USB 借卡意图、持卡延期、归还后的版本检查；各有独立反向对照。新增限制比原来严格，未改变原测试期望。

## 要用户定的

本轮无阻塞项。按既有授权修复可复现缺陷；未来身份重绑定等产品事项仍沿用保守拒绝自动归属。

## 顺手发现

通用测试 `CHECK` 的长中文失败诊断可能截断 UTF-8，使 `metrics.json` 解析报错；本轮调试时实际遇到，测试仍以失败退出。未改范围外的通用测试框架，最终两套通过的专项均没有此类警告。
